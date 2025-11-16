#include <assert.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "espefi_api.h"
#include "include/espefi_vga.h"
#include "portmacro.h"
#include "sd_protocol_types.h"
#include "sdmmc_cmd.h"
 
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"

#include "espefi_api_internal.h"

#define SSFN_CONSOLEBITMAP_HICOLOR
#define SSFN_CONSOLEBITMAP_CONTROL
#define SDCARD_MOUNT_POINT "/sd"

extern void shell_loop();

void sdspi_init(){
	spi_bus_config_t spi_bus = {
		.mosi_io_num = 47,
		.miso_io_num = 48,
		.sclk_io_num = 45,
		.quadhd_io_num = -1,
		.quadwp_io_num = -1,
		.max_transfer_sz = 4000,
	};
	sdmmc_host_t mmc = SDSPI_HOST_DEFAULT();
	if(spi_bus_initialize(mmc.slot, &spi_bus, SDSPI_DEFAULT_DMA) == ESP_OK){
		sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
	    slot_config.gpio_cs = 40;
	    slot_config.host_id = mmc.slot;
	    
	    
	    sdmmc_card_t *card;
	    const char mount_point[] = SDCARD_MOUNT_POINT;
		
		esp_vfs_fat_sdmmc_mount_config_t mount_config = {
	        .format_if_mount_failed = false,
	        .max_files = 64,
	        .allocation_unit_size = 64 * 1024
	    };
		if(esp_vfs_fat_sdspi_mount(mount_point, &mmc, &slot_config, &mount_config, &card) == ESP_OK){
			sdmmc_card_print_info(stdout, card);
		}else spi_bus_free(mmc.slot);
	} else spi_bus_free(mmc.slot);
}

void app_main(void){
	sdspi_init();
	espefi_init();
	shell_loop();

	//assert(espefi_api->apploader.app_wait(espefi_api->apploader.app_load("/sd/elf.elf",2,(char*[]){"-iwad","/sd/doom2.wad"})) == 0);

}
