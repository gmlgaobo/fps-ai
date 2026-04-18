#ifndef YOLOV8_POSE_H
#define YOLOV8_POSE_H

#include <stdint.h>
#include <vector>
#include <string>
#include "rknn_api.h"


// Constants for YOLOv8n-pose
#define YOLOV8_INPUT_SIZE 640
#define NUM_KEYPOINTS 17
#define NUM_BOX_ATTRIB 4  // x, y, w, h
#define NUM_CONF 1        // object confidence
#define NUM_CLASSES 1     // person only
#define OUTPUT_CANDIDATES 8400  // 80*80 + 40*40 + 20*20 = 6400+1600+400=8400

// Bounding box
struct Box {
    float x, y, w, h;
    float confidence;
    int   classId;
};

// Keypoint
struct Keypoint {
    float x, y;
    float score;
};

// Detection result
struct Detection {
    Box box;
    std::vector<Keypoint> keypoints;  // 17 keypoints for COCO
};

// NMS parameters
struct NMSParams {
    float confThreshold = 0.45f;
    float iouThreshold = 0.45f;
    int topK = 100;
};

class Yolov8Pose {
public:
    Yolov8Pose();
    ~Yolov8Pose();

    // Load RKNN model from file
    bool loadModel(const std::string& modelPath);

    // Release model
    void release();

    // Run inference on BGR image (1920x1080 or other)
    // Result is filled with detected persons and their keypoints
    bool detect(const uint8_t* bgrData, int srcWidth, int srcHeight, 
                std::vector<Detection>& results, const NMSParams& params = NMSParams());

    // Get inference time in microseconds
    int64_t getLastInferenceTimeUs() const { return m_lastInferenceTime; }

    // Is model loaded
    bool isLoaded() const { return m_modelLoaded; }

private:
    // Preprocess: convert BGR to RKNN input (resize to 640x640, to RGB)
    void preprocess(const uint8_t* src, int srcW, int srcH);

    // Postprocess: extract boxes and keypoints from output
    void postprocess(std::vector<Detection>& results, 
                     int srcW, int srcH, const NMSParams& params);

    // Non-maximum suppression
    void nms(std::vector<Detection>& input, std::vector<Detection>& output, 
             float iouThreshold);

    // Calculate IoU between two boxes
    float calculateIoU(const Box& a, const Box& b);

private:
    rknn_context m_ctx = 0;
    bool m_modelLoaded = false;
    
    // Model info
    int m_inputWidth = YOLOV8_INPUT_SIZE;
    int m_inputHeight = YOLOV8_INPUT_SIZE;
    int m_inputChannels = 3;
    
    // Preprocessed buffer in RKNN input format (RGB)
    uint8_t* m_preprocessBuffer = nullptr;
    
    // Quantization parameters (from model)
    float m_inputScale = 1.0f;
    int32_t m_inputZeroPoint = 0;
    
    int64_t m_lastInferenceTime = 0;
};

#endif // YOLOV8_POSE_H
