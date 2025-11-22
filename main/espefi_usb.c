#include "usb/usb_host.h"
#include "freertos/freertos.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

extern void espefi_usbhid_init();
extern void espefi_usbmsc_init();

static void usb_task(void* params_p){
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LOWMED,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xSemaphoreGive(params_p);

    while(1){
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
}

extern void espefi_usb_init(){
    StaticSemaphore_t sembuf;
    SemaphoreHandle_t sem = xSemaphoreCreateBinaryStatic(&sembuf);

    xTaskCreatePinnedToCore(usb_task,
                                           "usb_events",
                                           4096,
                                           sem,
                                           2, NULL, 1);

    xSemaphoreTake(sem,portMAX_DELAY);

    espefi_usbhid_init();
    espefi_usbmsc_init();
}