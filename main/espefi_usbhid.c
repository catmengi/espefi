#include "esp_err.h"
#include "esp_intr_alloc.h"

#include "freertos/freertos.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "usb/hid.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"

#include "espefi_api.h"
#include "espefi_usbhid.h"

#include <stdbool.h>
#include <esp_log.h>

struct usbhid_event{
    hid_host_device_handle_t hid_device_handle;
    hid_host_driver_event_t event;
    void *arg;
};

static uint8_t queue_storage[64 * sizeof(hid_keyboard_input_report_boot_t)] = {0};
static StaticQueue_t queue_buf = {0};
static QueueHandle_t usbhid_queue = NULL;
static QueueHandle_t usbhid_event_queue = NULL;

static const char *hid_proto_name_str[] = {
    "NONE",
    "KEYBOARD",
    "MOUSE"
};

static void espefi_events_task(void* params_p){
    hid_keyboard_input_report_boot_t report = {0};
    while(xQueueReceive(usbhid_queue, &report, portMAX_DELAY)){
        espefi_keyboard_report_t new_report = {.modifier.val = report.modifier.val};
        
        for(unsigned i = 0; i < HID_KEYBOARD_KEY_MAX; i++){
            if(report.key[i] != HID_KEY_NO_PRESS){
                new_report.update[new_report.update_len++] = report.key[i];
            }
        }

        espefi_api->event.trigger(USB_HID_KEYBOARD_EVENT,&new_report);
    }
}

static void hid_host_keyboard_report_callback(const uint8_t *const data, const int length){
    if(length < sizeof(hid_keyboard_input_report_boot_t))
        return;

    xQueueSend(usbhid_queue,data,0);
}
static void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg){
    uint8_t data[64] = { 0 };
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;

    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch(event){
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
            ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle,
                                                              data,
                                                              64,
                                                              &data_length));

            if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
                if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                    hid_host_keyboard_report_callback(data, data_length);
                }
            }
        break;
        
        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGI("espefi_usbhid", "HID Device, protocol '%s' DISCONNECTED",hid_proto_name_str[dev_params.proto]);
            ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
        break;

        case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
            ESP_LOGI("espefi_usbhid", "HID Device, protocol '%s' TRANSFER_ERROR",
                    hid_proto_name_str[dev_params.proto]);
        break;

        default: break;
    }
}

static void hid_host_event_task(void* unused){
    struct usbhid_event ev;
    while(xQueueReceive(usbhid_event_queue,&ev,portMAX_DELAY)){
        
        hid_host_dev_params_t dev_params;
        ESP_ERROR_CHECK(hid_host_device_get_params(ev.hid_device_handle, &dev_params));

        switch(ev.event){
            case HID_HOST_DRIVER_EVENT_CONNECTED:
                ESP_LOGI("espefi_usbhid", "HID Device, protocol '%s' CONNECTED\n",
                hid_proto_name_str[dev_params.proto]);

                const hid_host_device_config_t dev_config = {
                    .callback = hid_host_interface_callback,
                    .callback_arg = NULL
                };
                
                ESP_ERROR_CHECK(hid_host_device_open(ev.hid_device_handle, &dev_config));
                if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class){
                    ESP_ERROR_CHECK(hid_class_request_set_protocol(ev.hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
                    if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                        ESP_ERROR_CHECK(hid_class_request_set_idle(ev.hid_device_handle, 0, 0));
                    }
                }
                ESP_ERROR_CHECK(hid_host_device_start(ev.hid_device_handle));
                break;

            default: break;
        }
    }
}
static void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event,
                              void *arg){

    struct usbhid_event hid_event = {hid_device_handle,event,arg};
    xQueueSend(usbhid_event_queue, &hid_event, 0);
}

extern void espefi_usbhid_init(){

    usbhid_queue = xQueueCreateStatic(64, sizeof(hid_keyboard_input_report_boot_t), queue_storage, &queue_buf);
    usbhid_event_queue = xQueueCreate(128,sizeof(struct usbhid_event));

    xTaskCreatePinnedToCore(hid_host_event_task,"hid_events",4096,NULL,10,NULL,1);
    
    xTaskCreatePinnedToCore(espefi_events_task,
                                           "event_task",
                                           4096,
                                           NULL,
                                           10, NULL, 1);
    

    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 1,
        .callback = hid_host_device_callback,
        .callback_arg = NULL
    };

    ESP_ERROR_CHECK(!(espefi_api->event.create(USB_HID_KEYBOARD_EVENT) != 0));

    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));
}