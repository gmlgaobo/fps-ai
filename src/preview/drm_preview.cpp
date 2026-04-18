#include "drm_preview.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <cstring>
#include <algorithm>

using namespace std;

DRMPreview::DRMPreview() {
}

DRMPreview::~DRMPreview() {
    close();
}

void DRMPreview::close() {
    if (m_ready) {
        if (m_fb.map != MAP_FAILED && m_fb.map != nullptr) {
            munmap(m_fb.map, m_fb.size);
        }
        if (m_fd >= 0) {
            drmModeRmFB(m_fd, m_fb.fb_id);
        }
        m_ready = false;
    }
    if (m_connector) {
        drmModeFreeConnector(m_connector);
        m_connector = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool DRMPreview::open(const string& device) {
    m_fd = ::open(device.c_str(), O_RDWR | O_CLOEXEC);
    if (m_fd < 0) {
        perror(("Failed to open " + device).c_str());
        return false;
    }
    
    return true;
}

bool DRMPreview::findConnector() {
    drmModeRes* resources = drmModeGetResources(m_fd);
    if (!resources) {
        cerr << "drmModeGetResources failed" << endl;
        return false;
    }
    
    // Find a connected connector
    for (int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector* conn = drmModeGetConnector(m_fd, resources->connectors[i]);
        if (!conn) continue;
        
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            // Pick the preferred mode
            m_connector = conn;
            
            // Find preferred mode (usually the highest resolution
            drmModeModeInfo* bestMode = nullptr;
            for (int j = 0; j < conn->count_modes; j++) {
                if (conn->modes[j].type & DRM_MODE_TYPE_PREFERRED) {
                    bestMode = &conn->modes[j];
                    break;
                }
            }
            
            if (!bestMode) {
                bestMode = &conn->modes[0];  // Use first if no preferred
            }
            
            m_mode = *bestMode;
            m_width = m_mode.hdisplay;
            m_height = m_mode.vdisplay;
            
            cout << "Found display: " << m_width << "x" << m_height << endl;
            
            drmModeFreeResources(resources);
            return true;
        }
        
        drmModeFreeConnector(conn);
    }
    
    drmModeFreeResources(resources);
    cerr << "No connected display found" << endl;
    return false;
}

bool DRMPreview::setup(uint32_t reqWidth, uint32_t reqHeight) {
    if (!findConnector()) {
        return false;
    }
    
    return setupBuffers();
}

bool DRMPreview::setupBuffers() {
    // Create dumb buffer
    drm_mode_create_dumb create;
    memset(&create, 0, sizeof(create));
    
    create.width = m_width;
    create.height = m_height;
    create.bpp = 32;  // ARGB8888
    
    int ret = drmIoctl(m_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
    if (ret != 0) {
        perror("DRM_IOCTL_MODE_CREATE_DUMB failed");
        return false;
    }
    
    m_fb.width = m_width;
    m_fb.height = m_height;
    m_fb.pitch = create.pitch;
    m_fb.size = create.size;
    m_fb.pitch = create.pitch;
    
    // Map buffer
    drm_mode_map_dumb map;
    memset(&map, 0, sizeof(map));
    map.handle = create.handle;
    ret = drmIoctl(m_fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
    if (ret != 0) {
        perror("DRM_IOCTL_MODE_MAP_DUMB failed");
        return false;
    }
    
    m_fb.map = mmap(nullptr, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, map.offset);
    if (m_fb.map == MAP_FAILED) {
        perror("mmap failed");
        return false;
    }
    
    m_fb.pixels = (uint32_t*)m_fb.map;
    
    // Add framebuffer
    ret = drmModeAddFB(m_fd, m_width, m_height, 24, 32, m_fb.pitch, create.handle, &m_fb.fb_id);
    if (ret != 0) {
        perror("drmModeAddFB failed");
        return false;
    }
    
    // Set mode
    ret = drmModeSetCrtc(m_fd, m_connector->encoder_ids[0], m_fb.fb_id, 0, 0, m_connector, &m_mode);
    if (ret != 0) {
        perror("drmModeSetCrtc failed");
        return false;
    }
    
    cout << "DRM preview ready: " << m_width << "x" << m_height << endl;
    m_ready = true;
    return true;
}

void DRMPreview::convertBGRtoARGB(const uint8_t* bgr, uint32_t* argb, int width, int height) {
    // Convert BGR 24-bit to ARGB 32-bit
    int dstPitch = m_fb.pitch / 4;
    int srcPitch = width * 3;
    
    for (int y = 0; y < height && y < (int)m_height; y++) {
        for (int x = 0; x < width && x < (int)m_width; x++) {
            const uint8_t* srcPixel = bgr + y * srcPitch + x * 3;
            uint8_t b = srcPixel[0];
            uint8_t g = srcPixel[1];
            uint8_t r = srcPixel[2];
            argb[y * dstPitch + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
    
    // Clear the rest if frame with black
    for (int y = 0; y < m_height; y++) {
        for (int x = width; x < (int)m_width; x++) {
            argb[y * dstPitch + x] = 0xFF000000;
        }
    }
    for (int y = height; y < (int)m_height; y++) {
        for (int x = 0; x < (int)m_width; x++) {
            argb[y * dstPitch + x] = 0xFF000000;
        }
    }
}

bool DRMPreview::showFrame(const uint8_t* bgrData, int width, int height) {
    if (!m_ready) return false;
    
    convertBGRtoARGB(bgrData, m_fb.pixels, width, height);
    
    // Page flip
    int ret = drmModePageFlip(m_fd, m_connector->encoder_ids[0], m_fb.fb_id, 
                              DRM_MODE_PAGE_FLIP_EVENT, nullptr);
    // Ignore error if event not needed
    return ret == 0;
}

void DRMPreview::drawDetection(int x, int y, int w, int h, float confidence) {
    if (!m_ready) return;
    
    uint32_t color = 0xFFFF0000;  // Red box in ARGB
    int dstPitch = m_fb.pitch / 4;
    int x2 = x + w;
    int y2 = y + h;
    
    // Clamp to screen
    x = max(0, x);
    y = max(0, y);
    x2 = min((int)m_width - 1, x2);
    y2 = min((int)m_height - 1, y2);
    
    // Draw top/bottom edge
    for (int px = x; px <= x2; px++) {
        if (y >= 0 && y < (int)m_height)
            m_fb.pixels[y * dstPitch + px] = color;
        if (y2 >= 0 && y2 < (int)m_height)
            m_fb.pixels[y2 * dstPitch + px] = color;
    }
    
    // Draw left/right edge
    for (int py = y; py <= y2; py++) {
        if (x >= 0 && x < (int)m_width)
            m_fb.pixels[py * dstPitch + x] = color;
        if (x2 >= 0 && x2 < (int)m_width)
            m_fb.pixels[py * dstPitch + x2] = color;
    }
}

void DRMPreview::drawKeypoint(float fx, float fy, float score) {
    if (score < 0.3f) return;
    
    int x = (int)round(fx);
    int y = (int)round(fy);
    
    if (x < 2 || x >= (int)m_width - 2 || y < 2 || y >= (int)m_height - 2) return;
    
    uint32_t color = 0xFF00FF00;  // Green
    int dstPitch = m_fb.pitch / 4;
    
    // Draw 5x5 circle
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            if (dx*dx + dy*dy <= 4) {
                m_fb.pixels[(y + dy) * dstPitch + (x + dx)] = color;
            }
        }
    }
}
