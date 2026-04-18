#!/bin/bash
# Demo script - test mouse movement in sequence
# This is for video demonstration

echo "=== RK3588 HID Mouse Demo ==="
echo "Make sure you have already run:"
echo "  sudo modprobe libcomposite"
echo "  sudo ./setup-hid.sh"
echo ""
echo "Press Enter to start demo..."
read -r

echo "Demo 1: Move right 50 pixels (5 steps)"
for i in {1..5}; do
    ./src/mouse --dx 10 --dy 0
    sleep 0.2
done
sleep 1

echo "Demo 2: Move down 50 pixels (5 steps)"
for i in {1..5}; do
    ./src/mouse --dx 0 --dy 10
    sleep 0.2
done
sleep 1

echo "Demo 3: Move left 50 pixels (5 steps)"
for i in {1..5}; do
    ./src/mouse --dx -10 --dy 0
    sleep 0.2
done
sleep 1

echo "Demo 4: Move up 50 pixels (5 steps)"
for i in {1..5}; do
    ./src/mouse --dx 0 --dy -10
    sleep 0.2
done
sleep 1

echo "Demo 5: Square pattern"
# Right
for i in {1..10}; do ./src/mouse --dx 5 --dy 0; sleep 0.1; done
# Down
for i in {1..10}; do ./src/mouse --dx 0 --dy 5; sleep 0.1; done
# Left
for i in {1..10}; do ./src/mouse --dx -5 --dy 0; sleep 0.1; done
# Up
for i in {1..10}; do ./src/mouse --dx 0 --dy -5; sleep 0.1; done

echo "Demo completed!"
