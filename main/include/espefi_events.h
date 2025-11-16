#pragma once

#include <stdint.h>

typedef uint32_t espefi_event_listener_id_t;
typedef uint32_t espefi_event_id_t;

typedef struct{
    espefi_event_id_t (*create)(char* const event_name);
    int (*delete)(espefi_event_id_t id);
    int (*trigger)(char* const event_name, void* event_ctx);

    espefi_event_listener_id_t (*add_listener)(char* const event_name, 
                               void (*listener)(char* const ev_name, void* userctx, void* event_ctx), void* userctx);
    int (*del_listener)(espefi_event_id_t id, espefi_event_listener_id_t listener_id);
}espefi_events_api_t;