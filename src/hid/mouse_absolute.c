/*
 * Absolute Position HID Mouse for RK3588
 * Used for AI aiming - jump directly to specific screen coordinates
 */

#include "hid_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

// This uses a different report descriptor for absolute positioning
// The gadget must be configured with an absolute mouse report

// Absolute mouse report (6 bytes)
// Byte 0: buttons
// Byte 1-2: X (16 bits, 0 to 32767)
// Byte 3-4: Y (16 bits, 0 to 32767)
// Byte 5: wheel
typedef struct __attribute__((packed)) {
    uint8_t buttons;
    uint16_t x;
    uint16_t y;
    int8_t wheel;
} hid_absolute_mouse_report_t;

#define MAX_ABS_VALUE 32767

int main(int argc, char **argv) {
    int x = -1;
    int y = -1;
    int dx = 0;
    int dy = 0;
    int buttons = 0;
    int screen_width = 1920;
    int screen_height = 1080;
    char *device_path = "/dev/hidg0";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--x") == 0 && i + 1 < argc) {
            x = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--y") == 0 && i + 1 < argc) {
            y = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--dx") == 0 && i + 1 < argc) {
            dx = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--dy") == 0 && i + 1 < argc) {
            dy = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            screen_width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            screen_height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--left") == 0) {
            buttons |= MOUSE_BUTTON_LEFT;
        } else if (strcmp(argv[i], "--right") == 0) {
            buttons |= MOUSE_BUTTON_RIGHT;
        } else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            device_path = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s --x <pixel_x> --y <pixel_y> [options]\n", argv[0]);
            printf("Absolute position mouse for AI targeting\n");
            printf("\nOptions:\n");
            printf("  --x <pixels>      Target X coordinate (required)\n");
            printf("  --y <pixels>      Target Y coordinate (required)\n");
            printf("  --dx <pixels>     Add delta X after moving\n");
            printf("  --dy <pixels>     Add delta Y after moving\n");
            printf("  --width <pixels>  Screen width (default: 1920)\n");
            printf("  --height <pixels> Screen height (default: 1080)\n");
            printf("  --left            Press left button\n");
            printf("  --right           Press right button\n");
            printf("  --device <path>   HID device path\n");
            printf("  -h, --help        Show this help\n");
            printf("\nExample:\n");
            printf("  %s --x 960 --y 540  Move mouse to screen center\n", argv[0]);
            return 0;
        }
    }

    if (x < 0 || y < 0) {
        fprintf(stderr, "Error: --x and --y are required\n");
        exit(1);
    }

    // Open device
    int fd = hid_open(device_path);
    if (fd < 0) {
        fprintf(stderr, "Make sure you have configured the HID gadget\n");
        return 1;
    }

    // Convert screen pixels to HID absolute coordinates
    uint16_t hid_x = (uint16_t)round((double)x / screen_width * MAX_ABS_VALUE);
    uint16_t hid_y = (uint16_t)round((double)y / screen_height * MAX_ABS_VALUE);

    // Clamp
    if (hid_x > MAX_ABS_VALUE) hid_x = MAX_ABS_VALUE;
    if (hid_y > MAX_ABS_VALUE) hid_y = MAX_ABS_VALUE;

    // Create absolute mouse report
    hid_absolute_mouse_report_t report;
    report.buttons = buttons;
    report.x = hid_x;
    report.y = hid_y;
    report.wheel = 0;

    int ret = hid_send_report(fd, &report, sizeof(report));
    if (ret != 0) {
        hid_close(fd);
        return 1;
    }

    printf("Sent absolute mouse: pixel=(%d,%d) hid=(%d,%d) buttons=0x%02X\n",
           x, y, hid_x, hid_y, buttons);

    // If we have delta to add, send a relative report after
    // This allows small adjustments after a big jump
    if (dx != 0 || dy != 0) {
        usleep(1000);
        // Need to send on a different device if using mixed mode
        // For now we just note it
        printf("Note: Delta movement requires a separate relative HID device\n");
    }

    hid_close(fd);
    return 0;
}
