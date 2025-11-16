#pragma once

#include "espefi_vga.h"
#include "espefi_events.h"
#include "espefi_apploader.h"
#include "espefi_usbhid.h"
#include "espefi_console.h"


typedef struct{
    espefi_display_api_t display;
    espefi_events_api_t event;
    espefi_apploader_api_t apploader;
    espefi_console_api_t console;
}espefi_api_t;

extern const espefi_api_t* const espefi_api;