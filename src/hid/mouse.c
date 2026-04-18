/*
 * HID Mouse Control for RK3588
 * Supports relative mouse movement and button control
 */

#include "hid_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int dx = 0;
    int dy = 0;
    int buttons = 0;
    int wheel = 0;
    char *device_path = MOUSE_DEVICE;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dx") == 0 && i + 1 < argc) {
            dx = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--dy") == 0 && i + 1 < argc) {
            dy = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--left") == 0) {
            buttons |= MOUSE_BUTTON_LEFT;
        } else if (strcmp(argv[i], "--right") == 0) {
            buttons |= MOUSE_BUTTON_RIGHT;
        } else if (strcmp(argv[i], "--middle") == 0) {
            buttons |= MOUSE_BUTTON_MIDDLE;
        } else if (strcmp(argv[i], "--wheel") == 0 && i + 1 < argc) {
            wheel = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            device_path = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --dx <value>    X movement (-127 to 127)\n");
            printf("  --dy <value>    Y movement (-127 to 127)\n");
            printf("  --left          Press left button\n");
            printf("  --right         Press right button\n");
            printf("  --middle        Press middle button\n");
            printf("  --wheel <steps> Wheel movement\n");
            printf("  --device <path> HID device path (default: /dev/hidg0)\n");
            printf("  -h, --help      Show this help\n");
            printf("\nExample:\n");
            printf("  %s --dx 10 --dy 5   Move mouse right 10, down 5\n", argv[0]);
            printf("  %s --dx 0 --dy 0 --left  Click left button\n", argv[0]);
            return 0;
        }
    }

    // Clamp values to valid range
    if (dx < -127) dx = -127;
    if (dx > 127) dx = 127;
    if (dy < -127) dy = -127;
    if (dy > 127) dy = 127;

    // Open device
    int fd = hid_open(device_path);
    if (fd < 0) {
        fprintf(stderr, "Make sure you have run setup-hid.sh and have the correct permissions\n");
        return 1;
    }

    // Create mouse report
    hid_mouse_report_t report;
    report.buttons = buttons;
    report.dx = (int8_t)dx;
    report.dy = (int8_t)dy;
    report.wheel = (int8_t)wheel;

    // Send report
    int ret = hid_send_report(fd, &report, sizeof(report));
    if (ret != 0) {
        hid_close(fd);
        return 1;
    }

    printf("Sent mouse report: buttons=0x%02X dx=%d dy=%d wheel=%d\n",
           buttons, dx, dy, wheel);

    hid_close(fd);
    return 0;
}
