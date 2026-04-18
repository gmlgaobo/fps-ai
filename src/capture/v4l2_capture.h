#ifndef V4L2_CAPTURE_H
#define V4L2_CAPTURE_H

#include <stdint.h>
#include <vector>
#include <string>
#include <functional>
#include <linux/videodev2.h>

// Image buffer holding captured frame
struct FrameBuffer {
    void*   data;          // User space address
    int     fd;            // DMA buffer fd (if used)
    size_t  size;          // Total size in bytes
    uint32_t width;        // Frame width
    uint32_t height;       // Frame height
    uint32_t stride;       // Bytes per line
    uint64_t timestamp;    // Capture timestamp (ns)
    int     index;         // Buffer index for V4L2
};

// Callback type for frame delivery
typedef std::function<void(FrameBuffer* frame)> FrameCallback;

class V4L2Capture {
public:
    V4L2Capture();
    ~V4L2Capture();

    // Open capture device
    bool open(const std::string& device = "/dev/video0");
    
    // Configure format: 1080p BGR24
    bool configure(uint32_t width = 1920, uint32_t height = 1080, 
                  uint32_t fps = 60);
    
    // Start streaming using MMAP buffers
    bool start(int numBuffers = 4);
    
    // Stop streaming
    void stop();
    
    // Close device
    void close();
    
    // Get next frame (blocking)
    bool getFrame(FrameBuffer* outFrame);
    
    // Requeue buffer after processing
    void releaseFrame(FrameBuffer* frame);
    
    // Start async capture with callback
    bool startAsync(FrameCallback callback);
    
    // Get current parameters
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    uint32_t getStride() const { return m_stride; }
    uint32_t getPixelFormat() const { return m_pixelFormat; }
    
    // Is running
    bool isRunning() const { return m_running; }

    // Get actual bytes per line (from driver)
    uint32_t getBytesPerLine() const { return m_bytesPerLine; }

    // Get actual buffer size
    size_t getBufferSize() const { return m_bufferSize; }

private:
    bool queryCapabilities();
    bool setFormat();
    bool setFps(uint32_t fps);
    bool requestBuffers();
    bool queueAllBuffers();
    bool xioctl(int request, void* arg);

private:
    std::string m_devicePath;
    int m_fd = -1;
    
    uint32_t m_width = 1920;
    uint32_t m_height = 1080;
    uint32_t m_pixelFormat = V4L2_PIX_FMT_BGR24;
    uint32_t m_stride = 0;
    uint32_t m_bytesPerLine = 0;
    size_t m_bufferSize = 0;
    
    struct Buffer {
        void*   start;
        size_t  length;
        int     index;
    };
    std::vector<Buffer> m_buffers;
    
    bool m_running = false;
    FrameCallback m_callback;
};

#endif // V4L2_CAPTURE_H
