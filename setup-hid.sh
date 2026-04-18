#!/bin/bash
# RK3588 USB HID Gadget Setup Script
# Creates a composite HID device with mouse + keyboard

set -e

# Configuration
GADGET_DIR=/sys/kernel/config/usb_gadget
GADGET_NAME=fps_aim
HOST_ADDR=0x0100
DEV_ADDR=0x0100
MANUFACTURER="Rockchip"
PRODUCT="FPS HID Mouse/Keyboard"

# Create gadget directory
mkdir -p $GADGET_DIR/$GADGET_NAME
cd $GADGET_DIR/$GADGET_NAME

# Set Vendor and Product ID
echo $HOST_ADDR > idVendor
echo $DEV_ADDR > idProduct

# Set strings
mkdir -p strings/0x409
echo "0123456789abcdef" > strings/0x409/serialnumber
echo "$MANUFACTURER" > strings/0x409/manufacturer
echo "$PRODUCT" > strings/0x409/product

# Create HID function for mouse
mkdir -p functions/hid.usb0
echo 1 > functions/hid.usb0/protocol
echo 1 > functions/hid.usb0/subclass
echo 4 > functions/hid.usb0/report_length
# Mouse Report Descriptor (Boot Protocol compatible)
echo -n -e '\x05\x01\x09\x02\xa1\x01\x09\x01\x15\x00\x25\x01\x35\x00\x45\x01\x95\x03\x75\x01\x81\x02\x95\x01\x75\x05\x81\x03\x05\x09\x19\x01\x29\x03\x15\x00\x25\x01\x35\x00\x45\x01\x95\x03\x75\x01\x81\x02\x95\x01\x75\x02\x81\x03\x05\x01\x09\x30\x09\x31\x15\x81\x25\x7f\x35\x00\x45\xff\x75\x08\x95\x02\x81\x06\xc0' | xxd -r -p > functions/hid.usb0/report_desc

# Create HID function for keyboard (optional)
mkdir -p functions/hid.usb1
echo 1 > functions/hid.usb1/protocol
echo 1 > functions/hid.usb1/subclass
echo 8 > functions/hid.usb1/report_length
# Keyboard Report Descriptor (Boot Protocol compatible)
echo -n -e '\x05\x01\x09\x06\xa1\x01\x05\x07\x19\xe0\x29\xe7\x15\x00\x25\x01\x75\x01\x95\x08\x81\x02\x95\x01\x75\x08\x81\x03\x95\x05\x05\x07\x19\x00\x29\x65\x15\x00\x25\x65\x75\x08\x95\x05\x81\x00\xc0' | xxd -r -p > functions/hid.usb1/report_desc

# Create configuration
mkdir -p configs/c.1/strings/0x409
echo 100 > configs/c.1/bmAttributes
echo 200 > configs/c.1/MaxPower
echo "HID Mouse/Keyboard" > configs/c.1/strings/0x409/configuration

# Link functions to config
ln -s functions/hid.usb0 configs/c.1/
ln -s functions/hid.usb1 configs/c.1/

# Find UDC driver
UDC_DEVICE=$(ls /sys/class/udc/ | head -1)
if [ -z "$UDC_DEVICE" ]; then
    echo "Error: No UDC device found. Check your USB OTG configuration."
    exit 1
fi

echo $UDC_DEVICE > UDC
echo "USB HID gadget configured successfully!"
echo "Using UDC: $UDC_DEVICE"
echo "Mouse device: /dev/hidg0"
echo "Keyboard device: /dev/hidg1"
