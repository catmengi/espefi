/*
 * rgb_disp.c
 *
 *  Created on: 25 авг. 2025 г.
 *      Author: catmengi
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "vga_disp.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_types.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "soc/clk_tree_defs.h"

#include <esp_log.h>

//static uint16_t fb320x240[320 * 240] = {0};
static esp_lcd_rgb_timing_t timings[] = {
	[0] = { //640x480@60hz
		.h_res = 640,
		.v_res = 480,
		.pclk_hz = 25 * 1000 * 1000,
		.hsync_front_porch = 16,
		.hsync_back_porch = 48,
		.hsync_pulse_width = 96,
		.vsync_front_porch = 10,
		.vsync_back_porch = 33,
		.vsync_pulse_width = 2,
		.flags.pclk_active_neg = true,
	},
	[1] = { //320x480@60hz
		.h_res = 320,
		.v_res = 480,
		.pclk_hz = 12.5 * 1000 * 1000,
		.hsync_front_porch = 16,
		.hsync_back_porch = 48,
		.hsync_pulse_width = 96,
		.vsync_front_porch = 10,
		.vsync_back_porch = 33,
		.vsync_pulse_width = 2,
		.flags.pclk_active_neg = true,
	},
};

static esp_lcd_rgb_timing_t* find_timings(unsigned w,unsigned h){
	esp_lcd_rgb_timing_t* ret = NULL;
	
	for(unsigned i = 0; i < sizeof(timings) / sizeof(timings[0]); i++){
		if(timings[i].h_res == w && timings[i].v_res == h){
			ret = &timings[i];
			break;
		}
	}
	
	return ret;	
}

IRAM_ATTR static bool vga_sync(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t* edata, void* user_ctx){
	vga_disp* display = user_ctx;

	BaseType_t woke = false;
	xSemaphoreGiveFromISR(display->vsync,&woke);

	return woke;
}

IRAM_ATTR static bool vga_bbuf_fill(esp_lcd_panel_handle_t panel, void *bounce_buf, int pos_px, int len_bytes, void *user_ctx){
	vga_disp* display = user_ctx;

	unsigned display_width = display->copied_conf.disp_w;
	unsigned bbuf_width = display_width << 1; //manualy swapping multitpy to bitwise, before: * 2
    unsigned y_max = len_bytes / (bbuf_width << 1); //before: * sizeof(uint16_t)

	unsigned y_start = pos_px == 0 ? 0 : pos_px / bbuf_width;
	uint16_t* input_fb = display->fb + ((y_start >> 1) * display_width);

	uint16_t* bbuf = bounce_buf;

	for(unsigned y = 0; y < y_max >> 1; y++,input_fb += display_width){
		for(unsigned oy = 0; oy < 2; oy++){
			for(unsigned x = 0; x < display_width; x++,bbuf+=2){
				*(bbuf) = *(input_fb + x);
				*(bbuf + 1) = *(input_fb + x);
			}
		}
	}

	return false;
}
int vga_disp_init(vga_disp_conf* conf, vga_disp* disp){
	int err = 1;
	
	if(conf){
		esp_lcd_rgb_timing_t* timings = NULL;
		unsigned timings_w = conf->disp_w;
		unsigned timings_h = conf->disp_h;
		unsigned fb_in_psram = 1;
		unsigned no_fb = 0;
		if(conf->disp_h == 240 && conf->disp_w == 320){
			timings_h = 480;
			timings_w = 640;
			fb_in_psram = 0;
			no_fb = 1;
		}
		
		if((timings = find_timings(timings_w, timings_h))){
			esp_lcd_rgb_panel_config_t rgb_conf = {
				.clk_src = LCD_CLK_SRC_PLL240M,
				.timings = *timings,
				.data_width = conf->num_data_lines,
				.bounce_buffer_size_px = 16 * rgb_conf.timings.h_res,
				.dma_burst_size = 64,
				.hsync_gpio_num = conf->hsync,
				.vsync_gpio_num = conf->vsync,
				.pclk_gpio_num = -1,
				.disp_gpio_num = -1,
				.num_fbs = no_fb == 0 ? 1 : 0,
				.flags.fb_in_psram = fb_in_psram,
				.flags.no_fb = no_fb,
				.flags.refresh_on_demand = 0,
			};
			
			for(unsigned i = 0; i < rgb_conf.data_width; i++){
				rgb_conf.data_gpio_nums[i] = conf->data_lines[i];
			}
			
			esp_lcd_panel_handle_t panel = NULL;
			if((err = esp_lcd_new_rgb_panel(&rgb_conf,&panel)) == ESP_OK){
				disp->panel = panel;
				disp->copied_conf = *conf;
				disp->bypp = conf->num_data_lines / 8;
				
				esp_lcd_rgb_panel_event_callbacks_t events = {
					.on_vsync = vga_sync,
				};

				disp->vsync = xSemaphoreCreateBinaryStatic(&disp->vsync_buf);

				if(fb_in_psram == 0){
					disp->fb = heap_caps_malloc(disp->copied_conf.disp_w * disp->copied_conf.disp_h * disp->bypp,MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
					if(disp->fb == 0){
						esp_lcd_panel_del(panel);
						ESP_LOGE("VGA","Cannot allocate 320x240 framebuffer!\n");
						return 1;
					}
					
					memset(disp->fb,0,disp->copied_conf.disp_w * disp->copied_conf.disp_h * disp->bypp);
				
					events.on_bounce_empty = vga_bbuf_fill;

				} else esp_lcd_rgb_panel_get_frame_buffer(panel, 1,(void**)&disp->fb);

				ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
				ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
				ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel, &events,disp));
				
			}
		}
	}
	return err;
}

void vga_disp_destroy(vga_disp* display){
	if(display && display->panel){
		esp_lcd_panel_del(display->panel);
		
		if(display->copied_conf.disp_w == 320 && display->copied_conf.disp_h == 240 && display->fb){
			free(display->fb);
		}
		memset(display,0,sizeof(*display)); //invalidate
	}
}

void vga_disp_wait_sync(vga_disp* display){
	xSemaphoreTake(display->vsync,portMAX_DELAY);
}