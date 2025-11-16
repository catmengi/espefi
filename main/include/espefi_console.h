#pragma once

#define KEYBOARD_PRESSED_KEYS_EVENT "kbscancodes_arrived"

typedef struct{
    void (*set_async)();
    void (*set_sync)();
    void (*set_immediate)();
    void (*set_buffered)();
}espefi_console_api_t;