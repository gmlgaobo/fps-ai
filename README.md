# FPS AI自动瞄准 - RK3588 MVP实现

## 项目说明
本项目基于RK3588开发板Ubuntu系统，实现FPS AI自动瞄准完整MVP。包含：
1. USB HID虚拟鼠标键盘（用于控制游戏）
2. HDMI-IN视频采集（1080p@60fps BGR原生格式）
3. YOLOv8n-pose人体关键点检测（RKNN NPU硬件加速）
4. HDMI输出预览（DRM/KMS直接渲染）

## 硬件要求
- RK3588开发板（定昌电子等）
- HDMI-IN采集模块
- USB OTG接口支持
- HDMI输出接口
- Ubuntu系统（推荐Ubuntu 22.04/24.04）
- RKNN-Toolkit2 runtime 已安装

## 功能特性

### 第一阶段 - HID鼠标键盘
- ✅ 虚拟HID鼠标支持（相对移动）
- ✅ 绝对坐标鼠标（用于AI瞄准直接跳转）
- ✅ 虚拟HID键盘支持
- ✅ 通过ConfigFS配置USB gadget

### 第二阶段 - 视频采集+AI推理+预览
- ✅ V4L2多平面接口采集 HDMI-IN
- ✅ 1080P@60fps BGR原生格式零拷贝
- ✅ YOLOv8n-pose人体关键点检测
- ✅ NPU硬件加速，目标推理延迟 < 30ms
- ✅ DRM/KMS HDMI直接预览，带检测框和关键点绘制

## 快速开始

### 安装依赖
```bash
sudo apt update
sudo apt install -y build-essential cmake libv4l-dev libdrm-dev librknnrt-dev
```

### 第一阶段：HID设置
```bash
# 配置USB HID gadget
sudo modprobe libcomposite
sudo modprobe usb_f_hid
sudo ./setup-hid.sh

# 编译HID工具
cd src/hid
make
sudo make install
```

### 第二阶段：采集+推理+预览
```bash
# Build main pipeline
mkdir -p build && cd build
cmake ..
make -j4

# Run pipeline (point to your RKNN model)
./fps-aiming --model yolov8n-pose.rknn
```

## 使用说明

### HID鼠标控制
```bash
# 相对移动
./src/hid/mouse --dx 10 --dy 5

# 绝对坐标定位到屏幕中心
./src/hid/mouse_absolute --x 960 --y 540 --width 1920 --height 1080

# 发送按键
./src/hid/keyboard h e l l o
```

### AI推理管线
```bash
./fps-aiming [options]
  --model <path>    Path to YOLOv8n-pose RKNN model (required)
  --capture <dev>   V4L2 capture device (default: /dev/video0)
  --drm <dev>       DRM device for preview (default: /dev/dri/card0)
  --conf <0.0-1.0>  Confidence threshold (default: 0.45)
  --iou <0.0-1.0>   NMS IoU threshold (default: 0.45)
```

## 文件结构
```
.
├── README.md
├── setup-hid.sh           # USB HID配置脚本
├── cleanup-hid.sh         # 清理脚本
├── test-demo.sh           # HID演示脚本
├── phase2-plan.md         # 第二阶段计划
├── project-plan.md        # 第一阶段计划
├── docs/
│   └── rk3588-config.md   # RK3588配置说明
└── src/
    ├── hid/               # First stage - HID mouse/keyboard
    │   ├── mouse.c
    │   ├── keyboard.c
    │   ├── mouse_absolute.c
    │   ├── hid_common.h
    │   └── Makefile
    ├── capture/           # Second stage - V4L2 capture
    │   ├── v4l2_capture.h
    │   └── v4l2_capture.cpp
    ├── rknn/              # RKNN YOLOv8-pose inference
    │   ├── yolov8_pose.h
    │   └── yolov8_pose.cpp
    ├── preview/           # DRM preview
    │   ├── drm_preview.h
    │   └── drm_preview.cpp
    ├── main_pipeline.cpp  # Main program
    └── CMakeLists.txt
```

## 性能目标
- 视频采集：1920x1080 @ 60fps → 已实现（V4L2多平面MMAP零拷贝）
- AI推理：YOLOv8n-pose单帧推理 < 30ms → 使用RK3588 NPU可达成
- 管线端到端：稳定30fps以上

## 准备RKNN模型
你需要将YOLOv8n-pose转换为RKNN格式：
1. PyTorch导出ONNX：`yolov8n-pose.onnx`
2. 使用RKNN-Toolkit2量化转换：`rknn.converter` → `yolov8n-pose.rknn`
3. 放在项目目录，运行时通过`--model`指定路径

## 工作原理
1. **V4L2采集** 使用multi-planar API和MMAP零拷贝，直接获取HDMI-IN帧到内存，支持1080p@60fps BGR24原生输出
2. **RKNN推理** 使用NPU硬件加速，YOLOv8n-pose人体关键点检测输出box + 17个人体关键点
3. **DRM预览** 直接通过KMS显示在HDMI输出，叠加绘制检测框和绿色关键点，不需要X11/Wayland
4. **HID控制** 通过USB OTG作为HID设备直接发送鼠标移动，延迟极低

## 故障排除
详见 [docs/rk3588-config.md](./docs/rk3588-config.md)
