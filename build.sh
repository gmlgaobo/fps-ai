#!/bin/bash
# FPS AI自动瞄准 - 完整编译脚本
# 整合所有模块：HID + 采集 + 推理 + 预览

set -e

# 项目根目录
PROJECT_ROOT=$(dirname $(readlink -f $0))
echo "项目根目录: $PROJECT_ROOT"

# 第一阶段：编译HID工具
echo ""
echo "=== [1/3] 编译HID鼠标键盘工具 ==="
cd $PROJECT_ROOT/src/hid
make clean
make -j4
echo "✅ HID工具编译完成"

# 第二阶段：编译C++管线（采集+推理+预览）
echo ""
echo "=== [2/3] 编译C++主管线 ==="
mkdir -p $PROJECT_ROOT/build
cd $PROJECT_ROOT/build
cmake ..
make -j4
echo "✅ C++主管线编译完成"

# 第三阶段：复制游侠的游戏控制Python模块
echo ""
echo "=== [3/3] 整合游侠的游戏控制算法 ==="
WORKER2_DIR=/root/.openclaw/.arkclaw-team/projects/p-mo45dwexf4vo5b/output/p-mo45dwexf4vo5b-worker2
if [ -d "$WORKER2_DIR" ]; then
    cp $WORKER2_DIR/game_control.py $PROJECT_ROOT/integration/
    cp $WORKER2_DIR/README.md $PROJECT_ROOT/integration/README-game-control.md
    echo "✅ 游戏控制算法整合完成"
else
    echo "⚠️  未找到游侠的输出目录，跳过整合"
fi

echo ""
echo "=================================="
echo "🎉 完整项目编译完成！"
echo ""
echo "输出文件:"
echo "  src/hid/mouse           - 相对鼠标控制"
echo "  src/hid/keyboard       - 键盘控制"
echo "  src/hid/mouse_absolute - 绝对坐标鼠标"
echo "  build/fps-aiming       - C++采集+推理+预览主程序"
echo "  integration/full_pipeline.py - Python完整整合管线"
echo "  integration/game_control.py - 游戏控制算法 (by 游侠)"
echo ""
echo "运行:"
echo "  ./build/fps-aiming --model yolov8n-pose.rknn"
echo "  or"
echo "  python3 integration/full_pipeline.py --model yolov8n-pose.rknn"
echo "=================================="
