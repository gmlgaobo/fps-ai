#ifndef DRM_PREVIEW_H
#define DRM_PREVIEW_H

#include <stdint.h>
#include <string>
#include <xf86drm.h>
#include <xf86drmMode.h>

class DRMPreview {
public:
    DRMPreview();
    ~DRMPreview();

    // Open DRM device (usually /dev/dri/card0)
    bool open(const std::string& device = "/dev/dri/card0");
    
    // Setup display mode
    bool setup(uint32_t width = 1920, uint32_t height = 1080);
    
    // Show frame with overlays (draw bounding boxes and keypoints)
    bool showFrame(const uint8_t* bgrData, int width, int height);
    
    // Draw detection overlays on top of frame
    void drawDetection(int x, int y, int w, int h, float confidence);
    void drawKeypoint(float x, float y, float score);
    
    // Close device
    void close();
    
    // Is initialized
    bool isReady() const { return m_ready; }
    
    // Get actual display resolution
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }

private:
    bool findConnector();
    bool setupBuffers();
    
    void convertBGRtoARGB(const uint8_t* bgr, uint32_t* argb, int width, int height);

private:
    std::string m_devicePath;
    int m_fd = -1;
    
    drmModeModeInfo m_mode;
    drmModeConnector* m_connector = nullptr;
    drmModeEncoder* m_encoder = nullptr;
    uint32_t m_crtcId = 0;
    
    struct Framebuffer {
        uint32_t  fb_id;
        void*     map;
        uint32_t* pixels;
        size_t    size;
        uint32_t  width;
        uint32_t  height;
        uint32_t  pitch;
    };
    
    Framebuffer m_fb;
    uint32_t m_width = 1920;
    uint32_t m_height = 1080;
    
    bool m_ready = false;
};

#endif // DRM_PREVIEW_H
