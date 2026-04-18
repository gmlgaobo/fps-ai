# FPS AI自动瞄准 - 完整整合说明

## 项目整体结构

```
fps-aiming-rk3588/
├── build.sh                    # 一键编译脚本
├── setup-hid.sh                # USB HID配置脚本
├── cleanup-hid.sh              # 清理HID
├── INTEGRATION.md              # 本文件
├── README.md                   # 项目说明
├── docs/
│   ├── rk3588-config.md        # RK3588基础配置
│   └── hdmi-in-config.md       # HDMI-IN配置
├── src/
│   ├── CMakeLists.txt
│   ├── hid/                    # 第一阶段 - USB HID鼠标键盘
│   │   ├── mouse.c
│   │   ├── keyboard.c
│   │   ├── mouse_absolute.c
│   │   ├── hid_common.h
│   │   └── Makefile
│   ├── capture/                # 第二阶段 - HDMI-IN V4L2采集
│   │   ├── v4l2_capture.h
│   │   └── v4l2_capture.cpp
│   ├── rknn/                   # 第二阶段 - RKNN YOLOv8n-pose
│   │   ├── yolov8_pose.h
│   │   └── yolov8_pose.cpp
│   ├── preview/                # 第二阶段 - DRM预览
│   │   ├── drm_preview.h
│   │   └── drm_preview.cpp
│   └── main_pipeline.cpp      # C++主程序
└── integration/               # 第三阶段 - 完整整合
    ├── full_pipeline.py       # Python完整管线整合
    ├── game_control.py        # 游戏控制算法 (by 游侠)
    └── README-game-control.md # 游戏控制算法说明
```

## 模块职责

| 模块 | 职责 | 作者 |
|------|------|------|
| USB HID鼠标键盘 | USB OTG虚拟HID设备，输出鼠标移动指令 | 硬侠 |
| HDMI-IN采集 | V4L2多平面接口采集1080p@60fps BGR格式 | 硬侠 |
| RKNN YOLOv8-pose | NPU加速人体关键点检测，输出人体检测框 | 硬侠 |
| 游戏控制算法 | 目标优先级排序、平滑移动算法、计算鼠标位移 | 游侠 |
| DRM预览 | HDMI输出带检测框和关键点的预览画面 | 硬侠 |
| 整合管线 | 串联所有模块，主入口程序 | 硬侠 |

## 快速编译

```bash
# 授予执行权限
chmod +x build.sh

# 一键编译所有模块
./build.sh
```

编译结果在 `build/` 目录：
- `build/fps-aiming` - C++版本管线（仅采集+推理+预览）
- `src/hid/mouse` - HID相对鼠标控制
- `src/hid/mouse_absolute` - HID绝对坐标控制

## 完整运行

### 方法一：C++版本（原生高性能）

```bash
# 1. 配置USB HID
sudo modprobe libcomposite
sudo modprobe usb_f_hid
sudo ./setup-hid.sh

# 2. 运行管线
cd build
./fps-aiming --model ../yolov8n-pose.rknn
```

### 方法二：Python版本（整合游侠控制算法，推荐）

```bash
# 1. 配置USB HID（同上）
sudo modprobe libcomposite
sudo modprobe usb_f_hid
sudo ./setup-hid.sh

# 2. 运行完整整合管线
python3 integration/full_pipeline.py \
    --model yolov8n-pose.rknn \
    --sensitivity 1.0 \
    --aim-strength 0.85 \
    --smoothing 0.35 \
    --screen-width 1920 \
    --screen-height 1080
```

## 推荐参数配置

根据不同场景调整参数：

### 默认配置（大多数FPS游戏）
```
--sensitivity 1.0 --aim-strength 0.85 --smoothing 0.35
```

### 跟枪场景（移动目标）
```
--sensitivity 1.0 --aim-strength 0.6 --smoothing 0.25
```

### 定点狙击（静止目标）
```
--sensitivity 0.8 --aim-strength 0.95 --smoothing 0.45
```

## 接口对接说明

### YOLOv8-pose检测输出 → 游戏控制输入

YOLOv8-pose输出每个人体检测框：
```
[
  {
    "bbox": [x1, y1, x2, y2],
    "score": confidence,
  },
  ...
]
```

游戏控制会自动：
1. 根据检测框高度判断部位（头部/胸口/身体）
2. 按优先级排序（头部 > 胸口 > 身体）
3. 计算最佳瞄准点坐标
4. 计算需要移动的鼠标位移
5. 平滑处理后输出到HID鼠标

### 部位判断规则
- 框高度 < 屏幕高度 25% → 头部
- 框高度 < 屏幕高度 50% → 胸口
- 其他 → 身体

### 瞄准点选择
- 头部: 检测框上方 25% 位置（爆头优先）
- 胸口: 检测框上方 45% 位置（躯干中心）
- 其他: 检测框中心点

## 依赖项安装

在RK3588 Ubuntu上需要安装：

```bash
# 编译依赖
sudo apt install -y build-essential cmake libv4l-dev libdrm-dev

# Python依赖
sudo apt install -y python3-pip
pip3 install numpy opencv-python

# RKNN runtime 需要从瑞芯微下载安装
# https://github.com/rockchip-linux/rknn-toolkit2
```

## 准备YOLOv8n-pose RKNN模型

1. 从Ultralytics获取yolov8n-pose.pt
2. 使用RKNN-Toolkit2转换为RKNN量化模型:
   ```bash
   # 步骤示例
   python -m rknn.converter \
     --onnx yolov8n-pose.onnx \
     --output yolov8n-pose.rknn \
     --target_platform rk3588 \
     --quantization
   ```
3. 将 `yolov8n-pose.rknn` 放在项目根目录

## 性能预期

在RK3588 NPU上:
- HDMI采集 1080p@60fps: ✓ 零拷贝满足
- YOLOv8n-pose推理: 单帧 **~15-20ms** ✓ 满足 < 30ms要求
- 控制算法处理: < 1ms
- 端到端FPS: 30-40fps 稳定运行

## 故障排除

1. **V4L2 open失败**: 检查HDMI-IN驱动是否加载，设备节点是否存在 `/dev/video0`
2. **RKNN init失败**: 检查rknnrt库是否正确安装，模型路径是否正确
3. **DRM预览失败**: 检查HDMI输出是否连接，DRM设备 `/dev/dri/card0` 是否存在
4. **HID没有反应**: 检查USB OTG是否配置为device模式，`setup-hid.sh` 是否成功运行

## 作者
- 硬件驱动/采集/推理/预览整合: **硬侠** (嵌入式系统专家)
- 游戏控制算法/平滑移动/瞄准逻辑: **游侠** (游戏控制算法专家)
