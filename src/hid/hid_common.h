#ifndef HID_COMMON_H
#define HID_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

// Default HID device paths
#define MOUSE_DEVICE "/dev/hidg0"
#define KEYBOARD_DEVICE "/dev/hidg1"

// Mouse button definitions
#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04

// HID mouse report structure (4 bytes)
// Byte 0: buttons bitmask
// Byte 1: X movement (-127 to +127)
// Byte 2: Y movement (-127 to +127)
// Byte 3: Wheel movement
typedef struct __attribute__((packed)) {
    uint8_t buttons;
    int8_t dx;
    int8_t dy;
    int8_t wheel;
} hid_mouse_report_t;

// HID keyboard report structure (8 bytes)
// Byte 0: modifier bits
// Byte 1: reserved
// Byte 2-7: key codes (6 maximum)
typedef struct __attribute__((packed)) {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
} hid_keyboard_report_t;

// Open HID device
static inline int hid_open(const char *device_path) {
    int fd = open(device_path, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open HID device");
        return -1;
    }
    return fd;
}

// Send HID report
static inline int hid_send_report(int fd, void *report, size_t len) {
    ssize_t written = write(fd, report, len);
    if ((size_t)written != len) {
        perror("Failed to write HID report");
        return -1;
    }
    return 0;
}

// Close HID device
static inline void hid_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

#endif // HID_COMMON_H
