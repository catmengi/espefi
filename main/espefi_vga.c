#include "espefi_vga.h"
#include "espefi_api.h"
#include "espefi_api_internal.h"
#include "vga_disp.h"

#include <string.h>

static vga_disp display = {0};
static void display_init(int w, int h){
    vga_disp_conf conf = {
		.vsync = 1,
		.hsync = 2,
        .disp_w = w,
        .disp_h = h,
		.num_data_lines = 16, //BGR 565
		.data_lines = {11, 12, 13, 14, 41, 16, 17, 18, 46, 9, 10, 4, 5, 6, 7, 15},
    };
    ESP_ERROR_CHECK(vga_disp_init(&conf, &display));
}

/*static int display_set_vmode(enum espefi_vmode vmode){
    int err = 1;
    vga_disp_conf conf = display.copied_conf;
    if((vmode == M320x240 && conf.disp_w != 320 && conf.disp_h != 240) || (vmode == M640x480 && conf.disp_w != 640 && conf.disp_h != 480)){
        vga_disp_destroy(&display);
        usleep(100 * 1000); //sleep to be sure that display has stopped working

        switch(vmode){
            case M320x240:
                conf.disp_w = 320;
                conf.disp_h = 240;
                break;
            case M640x480:
                conf.disp_w = 640;
                conf.disp_h = 480;
                break;
        }
        err = vga_disp_init(&conf,&display);
    }
    return err;
}*/

static int display_vsync(){
    if(display.panel){
        vga_disp_wait_sync(&display);
        return 0;
    }
    return 1;
}

static uint16_t* display_get_fb(){
    return display.fb;
}

static void display_blankscr(){
    memset(display.fb,0,320*240*2);
}

extern void espefi_vga_init(){
    display_init(320, 240); //vmode M320x240

    //espefi_mutable_api.display.set_vmode = display_set_vmode;
    espefi_mutable_api.display.wait_sync = display_vsync;
    espefi_mutable_api.display.get_fb = display_get_fb;
    espefi_mutable_api.display.blank_screen = display_blankscr;
}