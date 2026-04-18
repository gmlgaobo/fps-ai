"""
FPS游戏自动瞄准控制算法
基于RK3588开发板

功能：
1. 人体目标检测与优先级排序（头部 > 胸口 > 其他部位）
2. 自动瞄准逻辑，计算鼠标移动目标位置
3. 鼠标平滑移动算法，避免跳帧，模拟人为轨迹
4. 可配置瞄准灵敏度参数
5. 整合HDMI采集+AI推理输出，输出鼠标控制指令到HID设备

Author: 游侠 (游戏控制算法专家)
"""

import math
import time
from typing import List, Tuple, Optional, Dict
from dataclasses import dataclass
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

@dataclass
class DetectionBox:
    """检测框数据结构"""
    x1: float  # 左上角x
    y1: float  # 左上角y
    x2: float  # 右下角x
    y2: float  # 右下角y
    score: float  # 置信度
    class_id: int  # 类别id
    class_name: str  # 类别名称
    
    @property
    def center_x(self) -> float:
        return (self.x1 + self.x2) / 2
    
    @property
    def center_y(self) -> float:
        return (self.y1 + self.y2) / 2
    
    @property
    def width(self) -> float:
        return self.x2 - self.x1
    
    @property
    def height(self) -> float:
        return self.y2 - self.y1
    
    def get_aim_point(self, priority: str = "head") -> Tuple[float, float]:
        """获取瞄准点"""
        if priority == "head":
            # 头部区域在检测框上方1/3处
            return (self.center_x, self.y1 + self.height * 0.25)
        elif priority == "chest":
            # 胸口区域在检测框中心偏上
            return (self.center_x, self.y1 + self.height * 0.45)
        else:
            # 默认中心点
            return (self.center_x, self.center_y)


@dataclass
class Target:
    """目标数据结构"""
    detection: DetectionBox
    keypoints: Optional[List[Dict]] = None
    priority_score: float = 0.0
    aim_point: Tuple[float, float] = (0, 0)


class TargetSorter:
    """目标优先级排序器
    
    优先级规则：
    1. 部位优先级：头部 > 胸口 > 其他部位
    2. 置信度优先级：置信度越高优先级越高
    3. 距离优先级：距离屏幕中心越近优先级越高
    4. 大小优先级：目标越大优先级越高
    """
    
    def __init__(self, screen_width: int = 1920, screen_height: int = 1080):
        self.screen_width = screen_width
        self.screen_height = screen_height
        self.center_x = screen_width / 2
        self.center_y = screen_height / 2
        
        # 部位权重
        self.part_weights = {
            "head": 100,
            "chest": 80,
            "body": 50,
            "limb": 30
        }
    
    def calculate_priority(self, detection: DetectionBox, screen_center_weight: float = 0.15) -> float:
        """计算目标优先级分数"""
        
        # 1. 部位基础分数
        class_name = detection.class_name.lower()
        base_score = self.part_weights.get(class_name, self.part_weights["body"])
        
        # 2. 置信度分数 (0-10)
        score_bonus = detection.score * 10
        
        # 3. 距离惩罚：距离屏幕中心越远，分数越低
        cx = detection.center_x
        cy = detection.center_y
        distance = math.sqrt((cx - self.center_x) ** 2 + (cy - self.center_y) ** 2)
        max_distance = math.sqrt(self.center_x ** 2 + self.center_y ** 2)
        distance_penalty = (1 - distance / max_distance) * 10 * screen_center_weight
        
        # 4. 大小奖励：目标越大分数越高
        size_bonus = (detection.width * detection.height) / (self.screen_width * self.screen_height) * 20
        
        total_score = base_score + score_bonus + distance_penalty + size_bonus
        
        return total_score
    
    def sort_targets(self, detections: List[DetectionBox]) -> List[Target]:
        """对检测结果按优先级排序"""
        targets = []
        
        for det in detections:
            priority = self.calculate_priority(det)
            aim_point = det.get_aim_point("head" if "head" in det.class_name.lower() else "chest")
            target = Target(
                detection=det,
                priority_score=priority,
                aim_point=aim_point
            )
            targets.append(target)
        
        # 按优先级降序排序
        targets.sort(key=lambda t: t.priority_score, reverse=True)
        
        return targets


class SmoothMouse:
    """鼠标平滑移动算法
    
    使用指数移动平均 + 贝塞尔曲线 实现平滑移动，模拟人类操作
    """
    
    def __init__(self, 
                 smoothing_factor: float = 0.35,
                 min_step: int = 1,
                 max_step: int = 50,
                 steps_per_frame: int = 5):
        """
        Args:
            smoothing_factor: 平滑因子 (0-1)，越小越平滑但响应越慢
            min_step: 最小移动步长
            max_step: 最大移动步长
            steps_per_frame: 每帧分割步数
        """
        self.smoothing_factor = smoothing_factor
        self.min_step = min_step
        self.max_step = max_step
        self.steps_per_frame = steps_per_frame
        
        # 上一次的目标位置
        self.last_dx: float = 0
        self.last_dy: float = 0
        
        # 是否启用平滑
        self.enabled = True
    
    def smooth_movement(self, dx: float, dy: float) -> Tuple[float, float]:
        """对鼠标移动进行平滑处理
        
        使用指数移动平均：EMA_smooth = alpha * new + (1 - alpha) * old
        """
        if not self.enabled:
            return (dx, dy)
        
        # 指数移动平均平滑
        smoothed_dx = self.smoothing_factor * dx + (1 - self.smoothing_factor) * self.last_dx
        smoothed_dy = self.smoothing_factor * dy + (1 - self.smoothing_factor) * self.last_dy
        
        # 保存上一次结果
        self.last_dx = smoothed_dx
        self.last_dy = smoothed_dy
        
        return (smoothed_dx, smoothed_dy)
    
    def generate_steps(self, 
                       start_x: int, 
                       start_y: int, 
                       target_x: int, 
                       target_y: int, 
                       speed_factor: float = 1.0) -> List[Tuple[int, int]]:
        """生成分步移动轨迹，模拟人类缓动效果
        
        使用贝塞尔曲线实现自然加速减速效果
        修复累积误差：跟踪绝对位置而非增量累加
        """
        steps = []
        distance = math.sqrt((target_x - start_x) ** 2 + (target_y - start_y) ** 2)
        
        if distance < self.min_step:
            steps.append((int(target_x - start_x), int(target_y - start_y)))
            return steps
        
        # 根据距离计算步数，距离越长步数越多
        num_steps = max(1, min(int(distance / (5 / speed_factor)), 20))
        
        # 二次贝塞尔曲线，起点 -> 控制点 -> 终点
        # 控制点选在靠近起点，实现先快后慢
        ctrl_x = start_x + (target_x - start_x) * 0.3
        ctrl_y = start_y + (target_y - start_y) * 0.3
        
        # 跟踪绝对位置，避免累积误差
        current_abs_x = float(start_x)
        current_abs_y = float(start_y)
        
        for t in range(1, num_steps + 1):
            t_norm = t / num_steps
            
            # 贝塞尔曲线公式
            bt = 1 - t_norm
            x = bt**2 * start_x + 2 * bt * t_norm * ctrl_x + t_norm**2 * target_x
            y = bt**2 * start_y + 2 * bt * t_norm * ctrl_y + t_norm**2 * target_y
            
            if t == num_steps:
                # 最后一步确保精确到达终点
                x = target_x
                y = target_y
            
            # 基于绝对位置计算增量，避免累积误差
            step_x = int(round(x - current_abs_x))
            step_y = int(round(y - current_abs_y))
            
            if step_x != 0 or step_y != 0:
                steps.append((step_x, step_y))
                current_abs_x += step_x
                current_abs_y += step_y
        
        # 最后检查是否到达终点，如果还有剩余位移一并加上
        remaining_x = target_x - current_abs_x
        remaining_y = target_y - current_abs_y
        if remaining_x != 0 or remaining_y != 0:
            steps.append((int(remaining_x), int(remaining_y)))
        
        return steps
    
    def reset(self):
        """重置平滑状态"""
        self.last_dx = 0
        self.last_dy = 0


class AimingConfig:
    """瞄准配置参数，支持动态调整"""
    
    def __init__(self):
        # 基础灵敏度 (0.1 - 3.0)
        self.sensitivity: float = 1.0
        
        # 瞄准强度 (0.0 - 1.0)
        # 1.0 = 直接移动到目标，0.5 = 只移动一半
        self.aim_strength: float = 0.85
        
        # 最大移动距离（像素），防止瞬间大跳
        self.max_move_delta: int = 100
        
        # 是否只在按键按下时瞄准
        self.activation_on_key: bool = True
        
        # 平滑参数
        self.smoothing_factor: float = 0.35
        
        # 目标选择
        self.max_target_distance: float = 500  # 最大目标距离（像素）
        self.confidence_threshold: float = 0.5  # 最小置信度
        
        # 屏幕参数
        self.screen_width: int = 1920
        self.screen_height: int = 1080
    
    def set_sensitivity(self, value: float):
        """设置灵敏度"""
        self.sensitivity = max(0.1, min(3.0, value))
    
    def set_aim_strength(self, value: float):
        """设置瞄准强度"""
        self.aim_strength = max(0.0, min(1.0, value))
    
    def set_smoothing(self, value: float):
        """设置平滑因子"""
        self.smoothing_factor = max(0.1, min(0.9, value))


class AimingController:
    """主瞄准控制器
    
    整合目标排序、平滑算法、灵敏度配置
    """
    
    def __init__(self, config: Optional[AimingConfig] = None):
        self.config = config if config else AimingConfig()
        self.target_sorter = TargetSorter(
            screen_width=self.config.screen_width,
            screen_height=self.config.screen_height
        )
        self.smooth_mouse = SmoothMouse(
            smoothing_factor=self.config.smoothing_factor
        )
        self.center_x = self.config.screen_width / 2
        self.center_y = self.config.screen_height / 2
        self.last_update_time = time.time()
        
    def process_detections(self, detections: List[Dict]) -> List[Target]:
        """处理AI检测输出，转换为内部格式并排序
        
        Args:
            detections: AI输出的检测列表，每个元素包含:
                - bbox: [x1, y1, x2, y2]
                - score: 置信度
                - class_id: 类别ID
                - class_name: 类别名称
        """
        processed = []
        
        for det in detections:
            if det.get('score', 0) < self.config.confidence_threshold:
                continue
                
            bbox = det['bbox']
            detection = DetectionBox(
                x1=bbox[0], y1=bbox[1], x2=bbox[2], y2=bbox[3],
                score=det['score'],
                class_id=det.get('class_id', 0),
                class_name=det.get('class_name', 'body')
            )
            processed.append(detection)
        
        # 排序
        sorted_targets = self.target_sorter.sort_targets(processed)
        
        return sorted_targets
    
    def calculate_mouse_movement(self, 
                                target: Target, 
                                current_cursor_x: Optional[float] = None,
                                current_cursor_y: Optional[float] = None) -> Tuple[float, float]:
        """计算需要移动的鼠标位移
        
        Returns:
            (dx, dy): 需要移动的像素位移
        """
        # 使用屏幕中心作为当前准心位置（假设准心始终在屏幕中心）
        # 在FPS游戏中，准心默认就在屏幕中心
        cx = current_cursor_x if current_cursor_x is not None else self.center_x
        cy = current_cursor_y if current_cursor_y is not None else self.center_y
        
        # 获取目标瞄准点
        tx, ty = target.aim_point
        
        # 计算原始位移
        dx = (tx - cx) * self.config.sensitivity * self.config.aim_strength
        dy = (ty - cy) * self.config.sensitivity * self.config.aim_strength
        
        # 限制最大移动，防止跳帧
        distance = math.sqrt(dx ** 2 + dy ** 2)
        if distance > self.config.max_move_delta:
            scale = self.config.max_move_delta / distance
            dx *= scale
            dy *= scale
        
        # 平滑处理
        dx, dy = self.smooth_mouse.smooth_movement(dx, dy)
        
        return (dx, dy)
    
    def get_best_target(self, detections: List[Dict]) -> Optional[Target]:
        """获取最佳瞄准目标"""
        sorted_targets = self.process_detections(detections)
        
        if not sorted_targets:
            return None
        
        # 检查最近目标是否在最大距离内
        best = sorted_targets[0]
        bx, by = best.aim_point
        distance = math.sqrt((bx - self.center_x) ** 2 + (by - self.center_y) ** 2)
        
        if distance > self.config.max_target_distance:
            return None
        
        return best
    
    def update_config_smoothing(self):
        """更新平滑参数"""
        self.smooth_mouse.smoothing_factor = self.config.smoothing_factor
    
    def reset_smoothing(self):
        """重置平滑状态"""
        self.smooth_mouse.reset()


class HIDMouseOutput:
    """HID鼠标输出接口
    
    针对RK3588开发板，通过/dev/hidraw或者usb_gadget输出鼠标指令
    """
    
    def __init__(self, device_path: str = "/dev/hidraw0", enabled: bool = True):
        self.enabled = enabled
        self.device_path = device_path
        self.connected = False
        self.device = None
        
        if enabled:
            self.connect()
    
    def connect(self) -> bool:
        """连接HID设备"""
        if not self.enabled:
            return False
        
        try:
            self.device = open(self.device_path, 'wb')
            self.connected = True
            logger.info(f"HID鼠标设备已连接: {self.device_path}")
            return True
        except Exception as e:
            logger.warning(f"无法连接HID设备 {self.device_path}: {e}")
            logger.info("将使用模拟输出模式")
            self.connected = False
            return False
    
    def send_move(self, dx: int, dy: int, buttons: int = 0) -> bool:
        """发送鼠标移动指令
        
        HID鼠标报告格式 (8字节):
        Byte 0: buttons (bit0:左键, bit1:右键, bit2:中键)
        Byte 1: X displacement (signed char)
        Byte 2: Y displacement (signed char)
        Byte 3: Wheel
        对于大于127的位移，需要拆分多次发送
        """
        if not self.enabled:
            logger.debug(f"模拟输出: dx={dx}, dy={dy}, buttons={buttons}")
            return True
        
        if not self.connected:
            logger.debug(f"HID未连接，模拟输出: dx={dx}, dy={dy}")
            return False
        
        # 拆分大位移
        remaining_x = dx
        remaining_y = dy
        
        while remaining_x != 0 or remaining_y != 0:
            chunk_x = max(-127, min(127, remaining_x))
            chunk_y = max(-127, min(127, remaining_y))
            
            # 构造HID报告
            report = bytearray(8)
            report[0] = buttons & 0x07
            report[1] = chunk_x & 0xFF
            report[2] = chunk_y & 0xFF
            report[3] = 0  # wheel
            
            try:
                self.device.write(report)
                self.device.flush()
            except Exception as e:
                logger.error(f"发送HID报告失败: {e}")
                return False
            
            remaining_x -= chunk_x
            remaining_y -= chunk_y
        
        return True
    
    def close(self):
        """关闭设备"""
        if self.device:
            self.device.close()
            self.connected = False


class FPSAimAssistant:
    """FPS自动瞄准助手 - 主入口类
    
    整合所有模块，提供简易API
    """
    
    def __init__(self, 
                 screen_width: int = 1920, 
                 screen_height: int = 1080,
                 hid_device: str = "/dev/hidraw0",
                 enable_hid: bool = True):
        
        # 初始化配置
        self.config = AimingConfig()
        self.config.screen_width = screen_width
        self.config.screen_height = screen_height
        
        # 初始化控制器
        self.controller = AimingController(self.config)
        
        # 初始化HID输出
        self.hid_output = HIDMouseOutput(hid_device, enabled=enable_hid)
        
        # 状态
        self.aim_active = False
        self.last_frame_time = time.time()
        
        logger.info(f"FPS自动瞄准助手初始化完成: {screen_width}x{screen_height}")
    
    def set_sensitivity(self, value: float):
        """设置灵敏度"""
        self.config.set_sensitivity(value)
        logger.info(f"灵敏度已设置: {value}")
    
    def set_aim_strength(self, value: float):
        """设置瞄准强度"""
        self.config.set_aim_strength(value)
        logger.info(f"瞄准强度已设置: {value}")
    
    def set_smoothing(self, value: float):
        """设置平滑因子"""
        self.config.set_smoothing(value)
        self.controller.update_config_smoothing()
        logger.info(f"平滑因子已设置: {value}")
    
    def set_activation(self, active: bool):
        """设置瞄准激活状态"""
        if active and not self.aim_active:
            # 重新激活时重置平滑
            self.controller.reset_smoothing()
        self.aim_active = active
    
    def process_frame(self, detections: List[Dict]) -> Optional[Tuple[float, float]]:
        """处理一帧检测结果，输出鼠标移动
        
        Args:
            detections: AI检测结果列表
        
        Returns:
            (dx, dy) 如果需要移动鼠标，否则None
        """
        current_time = time.time()
        dt = current_time - self.last_frame_time
        self.last_frame_time = current_time
        
        if not self.aim_active:
            return None
        
        # 获取最佳目标
        best_target = self.controller.get_best_target(detections)
        
        if best_target is None:
            return None
        
        # 计算鼠标移动
        dx, dy = self.controller.calculate_mouse_movement(best_target)
        
        if abs(dx) < 0.5 and abs(dy) < 0.5:
            return None
        
        # 输出到HID设备
        dx_int = int(round(dx))
        dy_int = int(round(dy))
        
        success = self.hid_output.send_move(dx_int, dy_int)
        
        if success:
            logger.debug(f"瞄准目标 移动: dx={dx_int}, dy={dy_int}, 置信度={best_target.detection.score:.2f}")
        
        return (dx, dy)
    
    def close(self):
        """关闭资源"""
        self.hid_output.close()
        logger.info("FPS自动瞄准助手已关闭")


# 导出
__all__ = [
    'DetectionBox',
    'Target',
    'TargetSorter',
    'SmoothMouse',
    'AimingConfig',
    'AimingController',
    'HIDMouseOutput',
    'FPSAimAssistant',
]
