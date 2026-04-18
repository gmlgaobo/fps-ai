# RK3588 USB OTG HID配置说明

## 内核配置检查

RK3588 Ubuntu内核默认已启用USB OTG和USB HID gadget支持。检查以下配置：

```bash
zcat /proc/config.gz | grep -E "USB_GADGET|USB_CONFIGFS|USB_F_HID"
```

应该看到：
```
CONFIG_USB_GADGET=y
CONFIG_USB_CONFIGFS=y
CONFIG_USB_CONFIGFS_F_HID=y
CONFIG_USB_F_HID=y
```

如果没有，请重新配置内核并启用以上选项。

## Device Tree配置

确保RK3588的USB OTG节点配置正确。对于定昌开发板，通常在`/boot/dts/rockchip/`下：

检查是否有类似配置：
```dts
&usb_drd0 {
    status = "okay";
    dr_mode = "otg";
};
```

## 启用OTG模式

在RK3588开发板上，可能需要配置OTG模式：

```bash
# 检查当前模式
cat /sys/devices/platform/fe8a0000.usb2/mode

# 设置为设备模式
echo peripheral > /sys/devices/platform/fe8a0000.usb2/mode
```

## 开机自启（可选）

如果希望开机自动配置HID gadget，可以创建systemd服务：

```
# /etc/systemd/system/hid-gadget.service
[Unit]
Description=USB HID Gadget
After=local-fs.target

[Service]
Type=oneshot
ExecStart=/path/to/setup-hid.sh
ExecStop=/path/to/cleanup-hid.sh
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

启用服务：
```bash
sudo systemctl daemon-reload
sudo systemctl enable hid-gadget.service
```

## 权限配置

默认情况下，`/dev/hidg*`设备只有root可读写。添加udev规则让普通用户也能使用：

```
# /etc/udev/rules.d/99-hidg.rules
KERNEL=="hidg*", GROUP="users", MODE="0666"
```

然后重新加载udev：
```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## 测试步骤

1. 连接：将RK3588的USB OTG端口用USB线连接到你的电脑
2. 加载模块：`sudo modprobe libcomposite usb_f_hid`
3. 配置：`sudo ./setup-hid.sh`
4. 在电脑上检查：
   ```bash
   # Linux
   lsusb | grep "Rockchip"
   cat /proc/bus/input/devices | grep "HID Mouse"

   # Windows/macOS
   # 应该看到新的"USB Human Interface Device"设备
   # 鼠标指针应该能够通过我们的程序控制
   ```
5. 测试移动：
   ```bash
   cd src
   make
   ./mouse --dx 10 --dy 10
   # 你应该看到鼠标在电脑屏幕上向右下方移动
   ```

## 常见问题

**Q: 找不到UDC设备**
A: 检查USB ODT是否配置为设备模式，确认device tree enabled usb drd。

**Q: 权限 denied**
A: 需要root权限运行setup-hid.sh，普通用户访问/dev/hidg0需要udev规则。

**Q: 配置后电脑看不到设备**
A: 确认USB线是OTG数据线，连接到正确的OTG端口。

**Q: 移动范围有限制**
A: 单次相对移动范围是-127到+127，需要大距离移动可以多次发送报告。
