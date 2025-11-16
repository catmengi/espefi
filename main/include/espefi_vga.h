#pragma once

#include <stdint.h>

/*enum espefi_vmode{
    M320x240,
    M640x480,
};*/

typedef struct{
    //int (*set_vmode)(enum espefi_vmode vmode);
    int (*wait_sync)();
    void (*blank_screen)();
    uint16_t* (*get_fb)();
}espefi_display_api_t;