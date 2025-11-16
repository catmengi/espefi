/*
 * rgb_disp.h
 *
 *  Created on: 25 авг. 2025 г.
 *      Author: catmengi
 */

#ifndef MAIN_VGA_DISP_H_
#define MAIN_VGA_DISP_H_

#include "esp_lcd_types.h"

#include <freertos/freertos.h>
#include <freertos/semphr.h>

#include <stdatomic.h>

#define VGA_MAX_RGBLINES 16

typedef struct{	
	unsigned disp_w,disp_h;
	unsigned num_data_lines;
	int data_lines[VGA_MAX_RGBLINES];
	int hsync,vsync;
}vga_disp_conf;

typedef struct{
	esp_lcd_panel_handle_t panel;
	vga_disp_conf copied_conf;
	
	StaticSemaphore_t vsync_buf;
	SemaphoreHandle_t vsync;

	uint16_t* fb;
	unsigned bypp; //bytes per pixel
		
}vga_disp;

int vga_disp_init(vga_disp_conf* conf, vga_disp* disp);
void vga_disp_destroy(vga_disp* display);
void vga_disp_wait_sync(vga_disp* display);

#endif /* MAIN_VGA_DISP_H_ */