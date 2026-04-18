# RK3588 HDMI-IN 配置说明

## 内核配置

RK3588支持HDMI-IN输入，需要确保内核配置启用：

```bash
zcat /proc/config.gz | grep -E "HDMI|VIDEODEV"
```

需要有：
```
CONFIG_VIDEO_V4L2=y
CONFIG_MEDIA_SUPPORT=y
CONFIG_RK3588_HDMI_RX=y
```

## 设备树配置

对于定昌RK3588开发板，HDMI-IN设备树已经配置好。确认有类似：

```dts
&hdmirx {
	status = "okay";
	compatible = "rockchip, rk3588-hdmirx";
	pinctrl-names = "default";
	pinctrl-0 = <&hdmirx_scl &hdmirx_sda>;
	rockchip,phy-reset-gpio = <&gpio4 RK_PD4 GPIO_ACTIVE_LOW>;
};

&csi2_dcphy0 {
	status = "okay";
	ports {
		port@0 {
			csi2_dcphy0_input: endpoint {
				remote-endpoint = <&hdmirx_out>;
			};
		};
	};
};

&hdmirx_out {
	remote-endpoint = <&csi2_dcphy0_input>;
};
```

## 检查设备节点

加载驱动后，应该出现 `/dev/video0`：

```bash
ls -l /dev/video*
v4l2-ctl --list-devices
```

查看格式支持：
```bash
v4l2-ctl --list-formats-ext -d /dev/video0
```

应该看到 BGR24 格式支持（V4L2_PIX_FMT_BGR24）。

## HDMI-IN 分辨率和帧率

本项目默认配置 `1920x1080@60fps`，这是大多数HDMI输入源的标准输出。

如果你的输入源不同，可以在代码中修改：
```cpp
capture.configure(width, height, fps);
```

## 权限设置

添加udev规则允许用户访问：
```
# /etc/udev/rules.d/99-video.rules
SUBSYSTEM=="video4linux", GROUP="users", MODE="0666"
```

## 测试采集

可以使用ffmpeg测试采集：
```bash
ffmpeg -f v4l2 -framerate 60 -video_size 1920x1080 -i /dev/video0 -f null -
```

查看帧率输出，确认能达到60fps。
