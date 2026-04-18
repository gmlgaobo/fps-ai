/*
 * HID Keyboard Control for RK3588
 * Supports keyboard input sending
 */

#include "hid_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

// Common HID keyboard usage codes
#define KEY_NONE 0x00
#define KEY_A 0x04
#define KEY_B 0x05
#define KEY_C 0x06
#define KEY_D 0x07
#define KEY_E 0x08
#define KEY_F 0x09
#define KEY_G 0x0A
#define KEY_H 0x0B
#define KEY_I 0x0C
#define KEY_J 0x0D
#define KEY_K 0x0E
#define KEY_L 0x0F
#define KEY_M 0x10
#define KEY_N 0x11
#define KEY_O 0x12
#define KEY_P 0x13
#define KEY_Q 0x14
#define KEY_R 0x15
#define KEY_S 0x16
#define KEY_T 0x17
#define KEY_U 0x18
#define KEY_V 0x19
#define KEY_W 0x1A
#define KEY_X 0x1B
#define KEY_Y 0x1C
#define KEY_Z 0x1D
#define KEY_1 0x1E
#define KEY_2 0x1F
#define KEY_3 0x20
#define KEY_4 0x21
#define KEY_5 0x22
#define KEY_6 0x23
#define KEY_7 0x24
#define KEY_8 0x25
#define KEY_9 0x26
#define KEY_0 0x27
#define KEY_ENTER 0x28
#define KEY_ESC 0x29
#define KEY_BACKSPACE 0x2A
#define KEY_TAB 0x2B
#define KEY_SPACE 0x2C
#define KEY_MINUS 0x2D
#define KEY_EQUAL 0x2E
#define KEY_LEFT_BRACKET 0x2F
#define KEY_RIGHT_BRACKET 0x30
#define KEY_BACKSLASH 0x31
#define KEY_SEMICOLON 0x33
#define KEY_APOSTROPHE 0x34
#define KEY_GRAVE 0x35
#define KEY_COMMA 0x36
#define KEY_PERIOD 0x37
#define KEY_SLASH 0x38
#define KEY_CAPS_LOCK 0x39
#define KEY_F1 0x3A
#define KEY_F2 0x3B
#define KEY_F3 0x3C
#define KEY_F4 0x3D
#define KEY_F5 0x3E
#define KEY_F6 0x3F
#define KEY_F7 0x40
#define KEY_F8 0x41
#define KEY_F9 0x42
#define KEY_F10 0x43
#define KEY_F11 0x44
#define KEY_F12 0x45
#define KEY_PRINT_SCREEN 0x46
#define KEY_SCROLL_LOCK 0x47
#define KEY_PAUSE 0x48
#define KEY_INSERT 0x49
#define KEY_HOME 0x4A
#define KEY_PAGE_UP 0x4B
#define KEY_DELETE 0x4C
#define KEY_END 0x4D
#define KEY_PAGE_DOWN 0x4E
#define KEY_RIGHT_ARROW 0x4F
#define KEY_LEFT_ARROW 0x50
#define KEY_DOWN_ARROW 0x51
#define KEY_UP_ARROW 0x52

// Modifier bits
#define MOD_CTRL_LEFT   (1 << 0)
#define MOD_SHIFT_LEFT  (1 << 1)
#define MOD_ALT_LEFT    (1 << 2)
#define MOD_GUI_LEFT    (1 << 3)
#define MOD_CTRL_RIGHT  (1 << 4)
#define MOD_SHIFT_RIGHT (1 << 5)
#define MOD_ALT_RIGHT   (1 << 6)
#define MOD_GUI_RIGHT   (1 << 7)

void print_help(char *progname) {
    printf("Usage: %s [options] <key1> [key2] ...\n", progname);
    printf("Sends HID keyboard events\n");
    printf("\nCommon key names:\n");
    printf("  a-z, 0-1, enter, esc, backspace, tab, space\n");
    printf("  f1-f12, up, down, left, right, home, end\n");
    printf("  insert, delete, pageup, pagedown\n");
    printf("\nModifiers:\n");
    printf("  ctrl, shift, alt, gui\n");
    printf("\nExample:\n");
    printf("  %s ctrl alt delete  Send Ctrl+Alt+Del\n", progname);
    printf("  %s h e l l o space w o r l d enter  Type \"hello world\"\n", progname);
}

uint8_t parse_key(char *name) {
    if (strlen(name) == 1) {
        char c = tolower(name[0]);
        if (c >= 'a' && c <= 'z') {
            return KEY_A + (c - 'a');
        }
        if (c >= '0' && c <= '9') {
            if (c == '0') return KEY_0;
            return KEY_1 + (c - '1');
        }
    }

    if (strcmp(name, "1") == 0) return KEY_1;
    if (strcmp(name, "2") == 0) return KEY_2;
    if (strcmp(name, "3") == 0) return KEY_3;
    if (strcmp(name, "4") == 0) return KEY_4;
    if (strcmp(name, "5") == 0) return KEY_5;
    if (strcmp(name, "6") == 0) return KEY_6;
    if (strcmp(name, "7") == 0) return KEY_7;
    if (strcmp(name, "8") == 0) return KEY_8;
    if (strcmp(name, "9") == 0) return KEY_9;
    if (strcmp(name, "0") == 0) return KEY_0;

    if (strcmp(name, "enter") == 0) return KEY_ENTER;
    if (strcmp(name, "return") == 0) return KEY_ENTER;
    if (strcmp(name, "esc") == 0) return KEY_ESC;
    if (strcmp(name, "backspace") == 0) return KEY_BACKSPACE;
    if (strcmp(name, "tab") == 0) return KEY_TAB;
    if (strcmp(name, "space") == 0) return KEY_SPACE;
    if (strcmp(name, "f1") == 0) return KEY_F1;
    if (strcmp(name, "f2") == 0) return KEY_F2;
    if (strcmp(name, "f3") == 0) return KEY_F3;
    if (strcmp(name, "f4") == 0) return KEY_F4;
    if (strcmp(name, "f5") == 0) return KEY_F5;
    if (strcmp(name, "f6") == 0) return KEY_F6;
    if (strcmp(name, "f7") == 0) return KEY_F7;
    if (strcmp(name, "f8") == 0) return KEY_F8;
    if (strcmp(name, "f9") == 0) return KEY_F9;
    if (strcmp(name, "f10") == 0) return KEY_F10;
    if (strcmp(name, "f11") == 0) return KEY_F11;
    if (strcmp(name, "f12") == 0) return KEY_F12;
    if (strcmp(name, "up") == 0) return KEY_UP_ARROW;
    if (strcmp(name, "down") == 0) return KEY_DOWN_ARROW;
    if (strcmp(name, "left") == 0) return KEY_LEFT_ARROW;
    if (strcmp(name, "right") == 0) return KEY_RIGHT_ARROW;
    if (strcmp(name, "home") == 0) return KEY_HOME;
    if (strcmp(name, "end") == 0) return KEY_END;
    if (strcmp(name, "insert") == 0) return KEY_INSERT;
    if (strcmp(name, "delete") == 0) return KEY_DELETE;
    if (strcmp(name, "pageup") == 0) return KEY_PAGE_UP;
    if (strcmp(name, "pagedown") == 0) return KEY_PAGE_DOWN;

    return KEY_NONE;
}

uint8_t parse_modifier(char *name) {
    if (strcmp(name, "ctrl") == 0) return MOD_CTRL_LEFT;
    if (strcmp(name, "shift") == 0) return MOD_SHIFT_LEFT;
    if (strcmp(name, "alt") == 0) return MOD_ALT_LEFT;
    if (strcmp(name, "gui") == 0) return MOD_GUI_LEFT;
    if (strcmp(name, "win") == 0) return MOD_GUI_LEFT;
    return 0;
}

int main(int argc, char **argv) {
    char *device_path = KEYBOARD_DEVICE;
    uint8_t modifiers = 0;
    uint8_t keys[6] = {0, 0, 0, 0, 0, 0};
    int key_count = 0;

    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            device_path = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else {
            // Check if it's a modifier
            uint8_t mod = parse_modifier(argv[i]);
            if (mod != 0) {
                modifiers |= mod;
                continue;
            }
            // Check if it's a key
            uint8_t key = parse_key(argv[i]);
            if (key != KEY_NONE && key_count < 6) {
                keys[key_count++] = key;
            }
        }
    }

    // Open device
    int fd = hid_open(device_path);
    if (fd < 0) {
        fprintf(stderr, "Make sure you have run setup-hid.sh and have the correct permissions\n");
        return 1;
    }

    // Create and send keyboard report
    hid_keyboard_report_t report;
    report.modifiers = modifiers;
    report.reserved = 0;
    memcpy(report.keys, keys, sizeof(keys));

    int ret = hid_send_report(fd, &report, sizeof(report));
    if (ret != 0) {
        hid_close(fd);
        return 1;
    }

    printf("Sent keyboard report: modifiers=0x%02X keys=[", modifiers);
    for (int i = 0; i < key_count; i++) {
        printf("%02X", keys[i]);
        if (i < key_count - 1) printf(", ");
    }
    printf("]\n");

    // Send empty report to release keys
    usleep(10000);
    hid_keyboard_report_t release;
    memset(&release, 0, sizeof(release));
    hid_send_report(fd, &release, sizeof(release));

    hid_close(fd);
    return 0;
}
