#include "yolov8_pose.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <fstream>

using namespace std;

Yolov8Pose::Yolov8Pose() {
}

Yolov8Pose::~Yolov8Pose() {
    release();
}

void Yolov8Pose::release() {
    if (m_modelLoaded) {
        rknn_destroy(m_ctx);
        m_modelLoaded = false;
    }
    if (m_preprocessBuffer) {
        delete[] m_preprocessBuffer;
        m_preprocessBuffer = nullptr;
    }
}

bool Yolov8Pose::loadModel(const string& modelPath) {
    release();
    
    // Read model file
    FILE* fp = fopen(modelPath.c_str(), "rb");
    if (!fp) {
        cerr << "Failed to open model file: " << modelPath << endl;
        return false;
    }
    
    fseek(fp, 0, SEEK_END);
    size_t modelSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    void* modelData = malloc(modelSize);
    if (!modelData) {
        cerr << "Failed to allocate memory for model" << endl;
        fclose(fp);
        return false;
    }
    
    fread(modelData, 1, modelSize, fp);
    fclose(fp);
    
    // Init RKNN
    int ret = rknn_init(&m_ctx, modelData, modelSize, 0, nullptr);
    free(modelData);
    
    if (ret < 0) {
        cerr << "rknn_init failed, error code: " << ret << endl;
        return false;
    }
    
    // Query input size
    rknn_input_output_num ioNum;
    ret = rknn_query(m_ctx, RKNN_QUERY_IN_OUT_NUM, &ioNum, sizeof(ioNum));
    if (ret != RKNN_SUCC) {
        cerr << "rknn_query failed" << endl;
        return false;
    }
    
    // Allocate preprocess buffer (RGB 640x640)
    m_preprocessBuffer = new uint8_t[m_inputWidth * m_inputHeight * 3];
    
    m_modelLoaded = true;
    return true;
}

void Yolov8Pose::preprocess(const uint8_t* src, int srcW, int srcH) {
    // Bilinear resize from srcW x srcH BGR to 640x640 RGB
    // We keep it simple for RK3588, use naive resize implementation
    // In production you could use OpenCV or RGA hardware resize
    
    float scaleX = (float)srcW / m_inputWidth;
    float scaleY = (float)srcH / m_inputHeight;
    
    int srcStride = srcW * 3;  // BGR 3 bytes per pixel
    
    for (int y = 0; y < m_inputHeight; y++) {
        int srcY = (int)(y * scaleY);
        for (int x = 0; x < m_inputWidth; x++) {
            int srcX = (int)(x * scaleX);
            
            // Original BGR -> output RGB
            const uint8_t* srcPixel = src + srcY * srcStride + srcX * 3;
            uint8_t* dstPixel = m_preprocessBuffer + y * m_inputWidth * 3 + x * 3;
            
            dstPixel[0] = srcPixel[2];  // R
            dstPixel[1] = srcPixel[1];  // G
            dstPixel[2] = srcPixel[0];  // B
        }
    }
}

bool Yolov8Pose::detect(const uint8_t* bgrData, int srcWidth, int srcHeight, 
                        vector<Detection>& results, const NMSParams& params) {
    if (!m_modelLoaded) return false;
    
    auto startTime = chrono::high_resolution_clock::now();
    
    // Preprocess
    preprocess(bgrData, srcWidth, srcHeight);
    
    // Set input
    rknn_input input;
    memset(&input, 0, sizeof(input));
    input.index = 0;
    input.type = RKNN_TENSOR_UINT8;
    input.size = m_inputWidth * m_inputHeight * 3;
    input.buf = m_preprocessBuffer;
    input.pass_through = 0;
    
    int ret = rknn_inputs_set(m_ctx, 1, &input);
    if (ret != RKNN_SUCC) {
        cerr << "rknn_inputs_set failed" << endl;
        return false;
    }
    
    // Run inference
    ret = rknn_run(m_ctx, nullptr);
    if (ret != RKNN_SUCC) {
        cerr << "rknn_run failed" << endl;
        return false;
    }
    
    // Get output
    rknn_output output;
    memset(&output, 0, sizeof(output));
    output.index = 0;
    output.type = RKNN_TENSOR_FLOAT32;
    output.want_float = 1;
    
    ret = rknn_outputs_get(m_ctx, 1, &output, nullptr);
    if (ret != RKNN_SUCC) {
        cerr << "rknn_outputs_get failed" << endl;
        return false;
    }
    
    // Postprocess
    float* outBuf = (float*)output.buf;
    results.clear();
    
    // Output shape: 1x56x8400 where 56 = 4box + 1conf + 17*3kp = 5+51=56
    // Each candidate is in column: [x, y, w, h, conf, kp1x, kp1y, kp1s, ...]
    int numCandidates = OUTPUT_CANDIDATES;
    for (int i = 0; i < numCandidates; i++) {
        float conf = outBuf[4 + i * 56];
        
        if (conf < params.confThreshold) continue;
        
        // Extract box
        float cx = outBuf[0 + i * 56];
        float cy = outBuf[1 + i * 56];
        float w = outBuf[2 + i * 56];
        float h = outBuf[3 + i * 56];
        
        // Convert to xyxy
        float x1 = cx - w * 0.5f;
        float y1 = cy - h * 0.5f;
        float x2 = cx + w * 0.5f;
        float y2 = cy + h * 0.5f;
        
        // Scale to input size
        x1 *= (float)srcWidth / m_inputWidth;
        y1 *= (float)srcHeight / m_inputHeight;
        x2 *= (float)srcWidth / m_inputWidth;
        y2 *= (float)srcHeight / m_inputHeight;
        
        Detection det;
        det.box.x = x1;
        det.box.y = y1;
        det.box.w = x2 - x1;
        det.box.h = y2 - y1;
        det.box.confidence = conf;
        det.box.classId = 0;
        
        // Extract keypoints (17 keypoints, each 3 values: x, y, score)
        det.keypoints.reserve(NUM_KEYPOINTS);
        for (int kp = 0; kp < NUM_KEYPOINTS; kp++) {
            float kx = outBuf[5 + kp * 3 + 0 + i * 56];
            float ky = outBuf[5 + kp * 3 + 1 + i * 56];
            float ks = outBuf[5 + kp * 3 + 2 + i * 56];
            
            kx *= (float)srcWidth / m_inputWidth;
            ky *= (float)srcHeight / m_inputHeight;
            
            det.keypoints.push_back({kx, ky, ks});
        }
        
        results.push_back(det);
    }
    
    // NMS
    vector<Detection> nmsResult;
    nms(results, nmsResult, params.iouThreshold);
    
    results.swap(nmsResult);
    
    // Release output
    rknn_outputs_release(m_ctx, 1, &output);
    
    auto endTime = chrono::high_resolution_clock::now();
    m_lastInferenceTime = chrono::duration_cast<chrono::microseconds>(endTime - startTime).count();
    
    return true;
}

float Yolov8Pose::calculateIoU(const Box& a, const Box& b) {
    float ax1 = a.x;
    float ay1 = a.y;
    float ax2 = a.x + a.w;
    float ay2 = a.y + a.h;
    
    float bx1 = b.x;
    float by1 = b.y;
    float bx2 = b.x + b.w;
    float by2 = b.y + b.h;
    
    float interX1 = max(ax1, bx1);
    float interY1 = max(ay1, by1);
    float interX2 = min(ax2, bx2);
    float interY2 = min(ay2, by2);
    
    float interW = max(0.0f, interX2 - interX1);
    float interH = max(0.0f, interY2 - interY1);
    float interArea = interW * interH;
    
    float areaA = a.w * a.h;
    float areaB = b.w * b.h;
    
    return interArea / (areaA + areaB - interArea);
}

void Yolov8Pose::nms(vector<Detection>& input, vector<Detection>& output, 
                      float iouThreshold) {
    // Sort by confidence descending
    sort(input.begin(), input.end(), 
         [](const Detection& a, const Detection& b) {
             return a.box.confidence > b.box.confidence;
         });
    
    while (!input.empty()) {
        Detection best = input[0];
        output.push_back(best);
        
        input.erase(input.begin());
        
        // Remove overlapped boxes
        vector<Detection> remaining;
        for (auto& det : input) {
            float iou = calculateIoU(best.box, det.box);
            if (iou < iouThreshold) {
                remaining.push_back(det);
            }
        }
        
        input.swap(remaining);
    }
}

void Yolov8Pose::postprocess(vector<Detection>& results, 
                              int srcW, int srcH, const NMSParams& params) {
    // Postprocessing handled in detect()
}
