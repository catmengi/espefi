#include "espefi_console.h"
#include "espefi_api.h"

#include "espefi_api_internal.h"
#include "espefi_usbhid.h"
#include "freertos/idf_additions.h"
#include "include/espefi_usbhid.h"
#include "portmacro.h"
#include "usb/hid_usage_keyboard.h"

#include <stdint.h>
#include <string.h>
#include <stdatomic.h>

#define KEYBOARD_MAX_STRING 123
struct keyboard_input{
    char string[KEYBOARD_MAX_STRING + 1];
    uint32_t size;
};

static QueueHandle_t keyboard_queue = NULL; 
static atomic_ullong console_mode = portMAX_DELAY;
static atomic_bool console_syncimmediate = 0;
static char numpad[] = {
    ':','*','-','+','\n',
    '1','2','3','4','5','6','7',
    '8','9','0','\b','\\',

};

static int cmp(const void* a, const void* b){
    return *(uint8_t*)b - *(uint8_t*)a;
}
static bool exist_in(uint8_t* arr, uint8_t el){
    for(unsigned i = 0; i < HID_KEYBOARD_KEY_MAX; i++){
        if(el == arr[i])
            return true;
    }
    return false;
}

static void keyboard_listener(char* const event_name, void* userctx, void* eventctx){
    static espefi_keyboard_report_t previous_report = {0};
    static struct keyboard_input stdin_output = {0};
    espefi_keyboard_report_t new_report = *(espefi_keyboard_report_t*)eventctx;

    //sort keys
    qsort(new_report.update,new_report.update_len,sizeof(new_report.update[0]),cmp);

    //now filter out new keys

    uint8_t new_scancodes[HID_KEYBOARD_KEY_MAX] = {0};
    int new_scancodes_len = 0;

    for(unsigned i = 0; i < new_report.update_len; i++){
        if(new_report.update[i]){
            if(exist_in(previous_report.update, new_report.update[i]) == false)
                new_scancodes[new_scancodes_len++] = new_report.update[i];
        }
    }

    espefi_api->event.trigger(KEYBOARD_PRESSED_KEYS_EVENT,&new_report);


    previous_report = new_report;

    for(unsigned i = 0; i < new_scancodes_len; i++){
        if(stdin_output.size == KEYBOARD_MAX_STRING - 1){
            stdin_output.string[stdin_output.size++] = '\n';
            xQueueSend(keyboard_queue,&stdin_output,0);
            memset(&stdin_output,0,sizeof(stdin_output));
        }
        bool shift = new_report.modifier.left_shift || new_report.modifier.right_shift;
        bool ctrl = new_report.modifier.left_ctr || new_report.modifier.rigth_ctr;
        bool alt = new_report.modifier.left_alt || new_report.modifier.right_alt;

        if(new_scancodes[i] >= HID_KEY_A && new_scancodes[i] <= HID_KEY_SLASH){
            char sym = keycode2ascii[new_scancodes[i]][shift];
            switch(sym){
                case '\n':
                    stdin_output.string[stdin_output.size++] = '\n';
                    xQueueSend(keyboard_queue,&stdin_output,0);
                    memset(&stdin_output,0,sizeof(stdin_output));
                    break;
                case '\b':
                    if(stdin_output.size > 0){
                        stdin_output.string[--stdin_output.size] = '\0';
                    }
                    break;
                default:
                    stdin_output.string[stdin_output.size++] = sym;
                    break;
            }
        }
    }
}

int keyboard_stdin_read(void* userctx, char* wrbuf, int length){
    struct keyboard_input input = {0};

    if(xQueueReceive(keyboard_queue,&input,console_mode) == pdPASS){
        int copy_length = length > input.size ? input.size : length;
        if(input.size > length){
            int new_len = length - input.size;
            char* start = input.string + length;

            struct keyboard_input new_input = {0};
            new_input.size = new_len;
            memcpy(new_input.string,start,new_len);
            xQueueSendToFront(keyboard_queue, &new_input, 0);
        }

        memcpy(wrbuf,input.string,copy_length);


        return copy_length;
    }
    return 0;
}

extern void set_sync(){
    console_mode = portMAX_DELAY;
}
extern void set_async(){
    console_mode = 0;
}
extern void set_sync_immediate(){
    console_syncimmediate = true;
}
extern void set_sync_buffered(){
    console_syncimmediate = false;
}

extern void espefi_console_init(){
    setvbuf(stdout, NULL, _IONBF, 0);

    keyboard_queue = xQueueCreate(32,sizeof(struct keyboard_input));
    assert(keyboard_queue);

    _GLOBAL_REENT->_stdin = fropen(NULL,keyboard_stdin_read);
    stdin = _GLOBAL_REENT->_stdin;

    assert(espefi_api->event.create(KEYBOARD_PRESSED_KEYS_EVENT) != 0);
    assert(espefi_api->event.add_listener(USB_HID_KEYBOARD_EVENT,keyboard_listener,NULL) != 0);

    espefi_mutable_api.console.set_async = set_async;
    espefi_mutable_api.console.set_sync = set_sync;
    espefi_mutable_api.console.set_immediate = set_sync_immediate;
    espefi_mutable_api.console.set_buffered = set_sync_buffered;
}