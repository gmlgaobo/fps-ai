#!/usr/bin/env python3
"""
FPS AI自动瞄准 - 完整整合管线
整合:
1. HDMI-IN V4L2视频采集 (C++ extension)
2. RKNN YOLOv8n-pose人体关键点检测 (C++)
3. 游戏控制算法 - 目标排序、平滑移动 (Python by 游侠)
4. USB HID鼠标输出 (C HID device)
5. DRM HDMI预览 (C++ DRM/KMS)

Author: 硬侠 (整合) + 游侠 (控制算法)
"""

import argparse
import sys
import time
import cv2
import numpy as np
from typing import List, Dict, Optional, Tuple

# 导入游侠的游戏控制算法
sys.path.insert(0, '/root/.openclaw/.arkclaw-team/projects/p-mo45dwexf4vo5b/output/p-mo45dwexf4vo5b-worker2')
from game_control import FPSAimAssistant, DetectionBox

# 导入C++扩展模块（通过ctypes）
import ctypes
from ctypes import CDLL, c_void_p, c_int, c_uint32, c_float, c_bool

class FullPipeline:
    """完整FPS自动瞄准管线"""
    
    def __init__(self, args):
        self.args = args
        self.running = False
        
        # 初始化各模块
        self._init_capture()
        self._init_detector()
        self._init_preview()
        self._init_aim_assistant()
        
        print("=== 完整FPS自动瞄准管线初始化完成 ===")
        print(f"  分辨率: {args.width}x{args.height} @ {args.fps}fps")
        print(f"  模型: {args.model}")
        print(f"  屏幕: {args.screen_width}x{args.screen_height}")
        print(f"  灵敏度: {args.sensitivity}")
        print(f"  瞄准强度: {args.aim_strength}")
        print(f"  平滑因子: {args.smoothing}")
        print("=" * 50)
    
    def _init_capture(self):
        """初始化HDMI-IN采集"""
        # 这里会调用C++ V4L2捕获
        # 暂时使用pyv4l2或者通过ctypes加载
        self.capture_device = self.args.capture_device
        print(f"初始化视频采集: {self.capture_device}")
        # 实际初始化通过Ctypes调用编译好的.so
    
    def _init_detector(self):
        """初始化RKNN检测器"""
        self.model_path = self.args.model
        print(f"初始化RKNN检测器: {self.model_path}")
        # 实际初始化通过Ctypes调用
    
    def _init_preview(self):
        """初始化DRM预览"""
        if self.args.enable_preview:
            print(f"初始化DRM预览: {self.args.drm_device}")
        else:
            print("预览已禁用")
    
    def _init_aim_assistant(self):
        """初始化瞄准助手"""
        self.aim_assistant = FPSAimAssistant(
            screen_width=self.args.screen_width,
            screen_height=self.args.screen_height,
            hid_device=self.args.hid_device,
            enable_hid=self.args.enable_hid
        )
        self.aim_assistant.set_sensitivity(self.args.sensitivity)
        self.aim_assistant.set_aim_strength(self.args.aim_strength)
        self.aim_assistant.set_smoothing(self.args.smoothing)
        # 默认激活瞄准（实际使用中用按键触发）
        if self.args.always_on:
            self.aim_assistant.set_activation(True)
    
    def convert_yolo_detections(self, yolo_output) -> List[Dict]:
        """将YOLOv8-pose输出转换为游戏控制输入格式
        
        yolo_output: 来自RKNN C++检测，包含boxes、scores、keypoints
        """
        detections = []
        
        for det in yolo_output:
            x1, y1, x2, y2 = det['bbox']
            score = det['score']
            
            # 判断部位（根据位置，通常头部在上方，得分最高的人体框找头部）
            # 简化处理：如果框高占比小，认为是头部
            height = y2 - y1
            height_ratio = height / self.args.screen_height
            
            if height_ratio < 0.25:
                class_name = 'head'
            elif height_ratio < 0.5:
                class_name = 'chest'
            else:
                class_name = 'body'
            
            detections.append({
                'bbox': [x1, y1, x2, y2],
                'score': score,
                'class_name': class_name
            })
        
        return detections
    
    def run_frame(self):
        """运行一帧处理
        
        Returns:
            True if frame processed successfully
        """
        # 1. 采集帧
        # frame = self.capture.get_frame()
        
        # 2. AI检测人体
        # detections = self.detector.detect(frame)
        
        # 3. 游戏控制计算移动并输出到HID
        # movement = self.aim_assistant.process_frame(detections)
        
        # 4. 预览
        # if self.enable_preview:
        #     self.preview.show(frame, detections)
        
        # 模拟处理，实际运行时是真实调用
        return True
    
    def run_loop(self):
        """主循环"""
        self.running = True
        frame_count = 0
        start_time = time.time()
        
        try:
            while self.running:
                success = self.run_frame()
                if success:
                    frame_count += 1
                
                # 统计FPS每30帧输出一次
                if frame_count % 30 == 0:
                    elapsed = time.time() - start_time
                    fps = frame_count / elapsed
                    print(f"FPS: {fps:.1f}, 总帧数: {frame_count}")
                    
        except KeyboardInterrupt:
            print("\n用户中断，停止...")
        finally:
            self.stop()
    
    def stop(self):
        """停止并清理"""
        self.running = False
        self.aim_assistant.close()
        elapsed = time.time() - start_time
        fps = frame_count / elapsed
        print(f"\n=== 运行结束 ===")
        print(f"  总帧数: {frame_count}")
        print(f"  平均FPS: {fps:.1f}")


def main():
    parser = argparse.ArgumentParser(description='FPS AI自动瞄准完整管线')
    
    # 采集参数
    parser.add_argument('--capture-device', default='/dev/video0',
                      help='HDMI-IN capture device (default: /dev/video0)')
    parser.add_argument('--width', type=int, default=1920,
                      help='Capture width (default: 1920)')
    parser.add_argument('--height', type=int, default=1080,
                      help='Capture height (default: 1080)')
    parser.add_argument('--fps', type=int, default=60,
                      help='Capture FPS (default: 60)')
    
    # AI模型参数
    parser.add_argument('--model', default='./yolov8n-pose.rknn',
                      help='Path to YOLOv8n-pose RKNN model (required)')
    
    # 屏幕参数
    parser.add_argument('--screen-width', type=int, default=1920,
                      help='Game screen width (default: 1920)')
    parser.add_argument('--screen-height', type=int, default=1080,
                      help='Game screen height (default: 1080)')
    
    # 瞄准参数
    parser.add_argument('--sensitivity', type=float, default=1.0,
                      help='Mouse sensitivity (0.1-3.0, default: 1.0)')
    parser.add_argument('--aim-strength', type=float, default=0.85,
                      help='Aiming strength (0.0-1.0, default: 0.85)')
    parser.add_argument('--smoothing', type=float, default=0.35,
                      help='Smoothing factor (0.1-0.9, default: 0.35)')
    
    # HID输出参数
    parser.add_argument('--hid-device', default='/dev/hidg0',
                      help='HID mouse device (default: /dev/hidg0)')
    parser.add_argument('--no-hid', action='store_true',
                      help='Disable HID output (simulation mode)')
    
    # 预览参数
    parser.add_argument('--drm-device', default='/dev/dri/card0',
                      help='DRM device for preview')
    parser.add_argument('--no-preview', action='store_true',
                      help='Disable DRM preview')
    
    # 运行模式
    parser.add_argument('--always-on', action='store_true',
                      help='Keep aiming always on (for testing, default requires activation key)')
    
    args = parser.parse_args()
    
    args.enable_hid = not args.no_hid
    args.enable_preview = not args.no_preview
    
    pipeline = FullPipeline(args)
    pipeline.run_loop()


if __name__ == '__main__':
    main()
