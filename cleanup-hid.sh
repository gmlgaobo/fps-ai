#!/bin/bash
# Cleanup USB HID gadget

GADGET_DIR=/sys/kernel/config/usb_gadget
GADGET_NAME=fps_aim

cd $GADGET_DIR/$GADGET_NAME

# Unbind UDC
echo "" > UDC

# Remove symlinks
rm -f configs/c.1/hid.usb0
rm -f configs/c.1/hid.usb1

# Remove directories
rm -rf functions/hid.usb0
rm -rf functions/hid.usb1
rm -rf configs/c.1/strings/0x409
rm -rf configs/c.1
rm -rf strings/0x409

cd ..
rm -rf $GADGET_NAME

echo "Cleanup complete"
