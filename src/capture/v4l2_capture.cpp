#include "v4l2_capture.h"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <chrono>
#include <thread>

V4L2Capture::V4L2Capture() {
}

V4L2Capture::~V4L2Capture() {
    close();
}

bool V4L2Capture::open(const std::string& device) {
    m_devicePath = device;
    m_fd = ::open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (m_fd < 0) {
        perror(("Failed to open " + device).c_str());
        return false;
    }
    
    return queryCapabilities();
}

void V4L2Capture::close() {
    if (m_running) {
        stop();
    }
    
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool V4L2Capture::queryCapabilities() {
    struct v4l2_capability cap;
    if (!xioctl(VIDIOC_QUERYCAP, &cap)) {
        perror("VIDIOC_QUERYCAP failed");
        return false;
    }
    
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        std::cerr << "Device does not support multi-planar capture" << std::endl;
        return false;
    }
    
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        std::cerr << "Device does not support streaming" << std::endl;
        return false;
    }
    
    return true;
}

bool V4L2Capture::configure(uint32_t width, uint32_t height, uint32_t fps) {
    m_width = width;
    m_height = height;
    
    // Try to set format first
    if (!setFormat()) {
        return false;
    }
    
    // Set frame rate
    if (!setFps(fps)) {
        std::cerr << "Warning: Failed to set " << fps << " fps, using device default" << std::endl;
    }
    
    // Calculate stride for BGR24
    m_bytesPerLine = m_width * 3;  // 3 bytes per pixel for BGR24
    m_bufferSize = m_bytesPerLine * m_height;
    m_stride = m_bytesPerLine;
    
    return true;
}

bool V4L2Capture::setFormat() {
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = m_width;
    fmt.fmt.pix_mp.height = m_height;
    fmt.fmt.pix_mp.pixelformat = m_pixelFormat;  // V4L2_PIX_FMT_BGR24
    fmt.fmt.pix_mp.num_planes = 1;
    
    if (!xioctl(VIDIOC_S_FMT, &fmt)) {
        perror("VIDIOC_S_FMT failed");
        return false;
    }
    
    // Check if format was accepted
    if (fmt.fmt.pix_mp.width != m_width || fmt.fmt.pix_mp.height != m_height) {
        std::cerr << "Device doesn't support requested resolution " 
                  << m_width << "x" << m_height << ", got " 
                  << fmt.fmt.pix_mp.width << "x" << fmt.fmt.pix_mp.height << std::endl;
        m_width = fmt.fmt.pix_mp.width;
        m_height = fmt.fmt.pix_mp.height;
    }
    
    if (fmt.fmt.pix_mp.pixelformat != m_pixelFormat) {
        std::cerr << "Device doesn't support BGR24 format" << std::endl;
        return false;
    }
    
    m_bytesPerLine = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
    m_bufferSize = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
    
    return true;
}

bool V4L2Capture::setFps(uint32_t fps) {
    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (!xioctl(VIDIOC_G_PARM, &parm)) {
        perror("VIDIOC_G_PARM failed");
        return false;
    }
    
    if (!(parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
        std::cerr << "Device doesn't support timeperframe setting" << std::endl;
        return false;
    }
    
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = fps;
    
    if (!xioctl(VIDIOC_S_PARM, &parm)) {
        perror("VIDIOC_S_PARM failed");
        return false;
    }
    
    return true;
}

bool V4L2Capture::requestBuffers() {
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (!xioctl(VIDIOC_REQBUFS, &req)) {
        perror("VIDIOC_REQBUFS failed");
        return false;
    }
    
    if (req.count < 2) {
        std::cerr << "Insufficient buffer memory" << std::endl;
        return false;
    }
    
    m_buffers.resize(req.count);
    
    for (int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[1];
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.length = 1;
        buf.m.planes = planes;
        
        if (!xioctl(VIDIOC_QUERYBUF, &buf)) {
            perror("VIDIOC_QUERYBUF failed");
            return false;
        }
        
        m_buffers[i].length = planes[0].length;
        m_buffers[i].start = mmap(NULL, planes[0].length,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED,
                                  m_fd, planes[0].m.mem_offset);
        
        if (m_buffers[i].start == MAP_FAILED) {
            perror("mmap failed");
            return false;
        }
        
        m_buffers[i].index = i;
    }
    
    return true;
}

bool V4L2Capture::queueAllBuffers() {
    for (size_t i = 0; i < m_buffers.size(); i++) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[1];
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.length = 1;
        buf.m.planes = planes;
        
        if (!xioctl(VIDIOC_QBUF, &buf)) {
            perror("VIDIOC_QBUF failed");
            return false;
        }
    }
    
    return true;
}

bool V4L2Capture::start(int numBuffers) {
    if (!requestBuffers()) {
        return false;
    }
    
    if (!queueAllBuffers()) {
        return false;
    }
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (!xioctl(VIDIOC_STREAMON, &type)) {
        perror("VIDIOC_STREAMON failed");
        return false;
    }
    
    m_running = true;
    return true;
}

void V4L2Capture::stop() {
    if (!m_running) return;
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    xioctl(VIDIOC_STREAMOFF, &type);
    
    // Unmap buffers
    for (auto& buf : m_buffers) {
        if (buf.start != MAP_FAILED && buf.start != nullptr) {
            munmap(buf.start, buf.length);
        }
    }
    m_buffers.clear();
    
    m_running = false;
}

bool V4L2Capture::getFrame(FrameBuffer* outFrame) {
    struct v4l2_buffer buf;
    struct v4l2_plane planes[1];
    memset(&buf, 0, sizeof(buf));
    memset(planes, 0, sizeof(planes));
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.length = 1;
    buf.m.planes = planes;
    
    // Wait for buffer
    pollfd pfd;
    pfd.fd = m_fd;
    pfd.events = POLLIN;
    int ret = poll(&pfd, 1, 1000);  // 1s timeout
    
    if (ret < 0) {
        perror("poll failed");
        return false;
    }
    
    if (ret == 0) {
        // Timeout
        return false;
    }
    
    if (!xioctl(VIDIOC_DQBUF, &buf)) {
        perror("VIDIOC_DQBUF failed");
        return false;
    }
    
    Buffer* mapped = &m_buffers[buf.index];
    
    outFrame->data = mapped->start;
    outFrame->size = mapped->length;
    outFrame->width = m_width;
    outFrame->height = m_height;
    outFrame->stride = m_stride;
    outFrame->timestamp = buf.timestamp.tv_sec * 1000000000ULL + buf.timestamp.tv_usec * 1000ULL;
    outFrame->index = buf.index;
    outFrame->fd = -1;
    
    return true;
}

void V4L2Capture::releaseFrame(FrameBuffer* frame) {
    // Requeue the buffer directly using stored index
    struct v4l2_buffer vbuf;
    struct v4l2_plane planes[1];
    memset(&vbuf, 0, sizeof(vbuf));
    memset(planes, 0, sizeof(planes));
    
    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    vbuf.memory = V4L2_MEMORY_MMAP;
    vbuf.index = frame->index;
    vbuf.length = 1;
    vbuf.m.planes = planes;
    
    xioctl(VIDIOC_QBUF, &vbuf);
}

bool V4L2Capture::xioctl(int request, void* arg) {
    int ret;
    do {
        ret = ioctl(m_fd, request, arg);
    } while (ret == -1 && errno == EINTR);
    return ret != -1;
}

bool V4L2Capture::startAsync(FrameCallback callback) {
    m_callback = callback;
    
    if (!start()) {
        return false;
    }
    
    // Start capture thread
    std::thread([this]() {
        while (m_running) {
            FrameBuffer frame;
            if (getFrame(&frame)) {
                if (m_callback) {
                    m_callback(&frame);
                }
                releaseFrame(&frame);
            }
        }
    }).detach();
    
    return true;
}
