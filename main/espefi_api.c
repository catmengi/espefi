#include "espefi_api.h"
#include "espefi_api_internal.h"

#include "esp_err.h"
#include "espefi_vga.h"
#include "freertos/freertos.h"
#include "freertos/idf_additions.h"
#include "include/espefi_usbhid.h"
#include "portmacro.h"
#include "usb/hid_usage_keyboard.h"

#include <stdatomic.h>
#include <sys/reent.h>
#include <esp_log.h>


/*INTERNAL IMPORTS*/
extern void espefi_vga_init();
extern void espefi_events_init();
extern void espefi_usb_init();
extern void espefi_console_init();
extern void espefi_posix_init();
extern void espefi_apploader_init();
/*================*/

/*GLOBAL VARIABLES*/
espefi_api_t espefi_mutable_api = {0};
const espefi_api_t* const espefi_api = &espefi_mutable_api;
/*================*/

void espefi_init(){
    espefi_events_init();
    espefi_vga_init();
    espefi_usb_init();
    espefi_console_init();
    espefi_posix_init();
    espefi_apploader_init();
}