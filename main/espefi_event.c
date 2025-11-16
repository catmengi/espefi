#include "esp_heap_caps.h"
#include "espefi_api_internal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "espefi_events.h"
#include "espefi_apploader.h"
#include "espefi_events.h"
#include "list.h"
#include "portmacro.h"

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <esp_log.h>

/*
#define STATIC_HT_SIZE 16
#define STATIC_HT_KEYSIZE 32

struct ht_entry{
    void* ptr;
    char name[STATIC_HT_KEYSIZE];       
};
typedef struct{
    int load;    
    struct ht_entry ht_entries[STATIC_HT_SIZE];
}static_ht_t;

uint32_t djb2_hash(const char *str) {
    uint32_t hash = 5381; // Initial prime
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }

    return hash;
}

#define hashfunc djb2_hash

static void* static_ht_get(char name[STATIC_HT_KEYSIZE], static_ht_t* ht){
    void* ret = NULL;

    int index = hashfunc(name) % STATIC_HT_SIZE;
    int limit = STATIC_HT_SIZE;
    for(unsigned i = index; i < limit; i++){
        if(strcmp(ht->ht_entries[i].name,name) == 0){ //found!
            ret = ht->ht_entries[i].ptr;
            break;
        }

        if(i == STATIC_HT_SIZE - 1){
            i = 0;
            limit = index;
        }
    }

    return ret;
}

static struct ht_entry* static_ht_get_entry(char name[STATIC_HT_KEYSIZE], static_ht_t* ht){
    struct ht_entry* ret = NULL;

    int index = hashfunc(name) % STATIC_HT_SIZE;
    int limit = STATIC_HT_SIZE;
    for(unsigned i = index; i < limit; i++){
        if(strcmp(ht->ht_entries[i].name,name) == 0){ //found!
            ret = &ht->ht_entries[i];
            break;
        }

        if(i == STATIC_HT_SIZE - 1){
            i = 0;
            limit = index;
        }
    }

    return ret;
}

static int static_ht_set(char name[STATIC_HT_KEYSIZE],void* ptr, static_ht_t* ht){
    int err = 1;

    int index = hashfunc(name) % STATIC_HT_SIZE;
    int limit = STATIC_HT_SIZE;
    for(unsigned i = index; i < limit; i++){
        if(ht->ht_entries[i].ptr == NULL){
            strcpy(ht->ht_entries[i].name,name);
            ht->ht_entries[i].ptr = ptr;
            err = 0;
            break;
        }

        if(strcmp(ht->ht_entries[i].name,name) == 0){ //item already exist!
            break;
        }

        if(i == STATIC_HT_SIZE - 1){
            i = 0;
            limit = index;
        }
    }

    return err;
}
*/

typedef struct espefi_event_listener{
    struct list_head list;
    espefi_app_t* owner;

    espefi_event_listener_id_t id;
    void (*handler)(char* const ev_name, void* userctx, void* event_ctx);
    void* ctx;
}espefi_event_listener_t;

typedef struct espefi_event{
    struct list_head list;
    espefi_app_t* owner;

    char name[25];
    struct list_head listeners;

    espefi_event_id_t id;
}espefi_event_t;

typedef struct espefi_event_storage{
    struct list_head events;

    StaticSemaphore_t rmutexbuf;
    SemaphoreHandle_t rmutex;
}espefi_event_storage_t;

static espefi_event_storage_t event_storage = {0};

static espefi_event_t* find_event(char* const name){
    xSemaphoreTakeRecursive(event_storage.rmutex, portMAX_DELAY);
    if(strlen(name) < 24){
        espefi_event_t* cur = NULL;
        list_for_each_entry(cur, &event_storage.events,list){
            if(strcmp(cur->name, name) == 0){
                xSemaphoreGiveRecursive(event_storage.rmutex);
                return cur;
            }
        }
    } 
    xSemaphoreGiveRecursive(event_storage.rmutex);
    return NULL;
}

static espefi_event_t* find_event_byid(espefi_event_id_t id){
    xSemaphoreTakeRecursive(event_storage.rmutex, portMAX_DELAY);
    espefi_event_t* cur = NULL;
    list_for_each_entry(cur, &event_storage.events,list){
        if(cur->id == id){
            xSemaphoreGiveRecursive(event_storage.rmutex);
            return cur;
        }
    }
    xSemaphoreGiveRecursive(event_storage.rmutex);
    return NULL;
}

extern espefi_event_id_t espefi_event_create(char* const event_name){
    espefi_event_id_t id = 0;
    xSemaphoreTakeRecursive(event_storage.rmutex, portMAX_DELAY);

    if(strlen(event_name) < 24){
        if(find_event(event_name) == NULL){
            espefi_event_t* new = heap_caps_malloc(sizeof(*new),MALLOC_CAP_SPIRAM);
            if(new){
                INIT_LIST_HEAD(&new->list);
                INIT_LIST_HEAD(&new->listeners);
                strcpy(new->name,event_name);
                new->owner = current_app;

                do{
                    arc4random_buf(&new->id, sizeof(new->id));
                }while(new->id == 0);

                id = new->id;
                
                list_add_tail(&new->list, &event_storage.events);
                
            }
        }
    }

    xSemaphoreGiveRecursive(event_storage.rmutex);
    return id;
}

extern espefi_event_listener_id_t espefi_event_add_listener(char* const event_name, void (*listener)(char* const ev_name, void* userctx, void* event_ctx), void* userctx){
    espefi_event_listener_id_t id = 0;
    xSemaphoreTakeRecursive(event_storage.rmutex, portMAX_DELAY);

    if(listener){
        espefi_event_t* event = find_event(event_name);
        if(event){
            espefi_event_listener_t* cur = NULL;
            list_for_each_entry(cur, &event->listeners, list){
                if(cur->handler == listener && cur->ctx == userctx){ //adding same listener multiple times, why?
                    id = cur->id; //just return previously added ID, and log error
                    ESP_LOGI(__PRETTY_FUNCTION__,"Added same listener %d (%p %p) to event '%s'\n",id,listener,userctx,event_name);
                    goto exit;
                }
            }

            espefi_event_listener_t* new = heap_caps_malloc(sizeof(*new), MALLOC_CAP_SPIRAM);
            if(new){
                new->ctx = userctx;
                new->handler = listener;
                new->owner = current_app;
                INIT_LIST_HEAD(&new->list);

                list_add(&new->list, &event->listeners);

                do{
                    arc4random_buf(&new->id,sizeof(new->id));
                }while(new->id == 0);

                id = new->id;
            }
        }
    }

exit:
    xSemaphoreGiveRecursive(event_storage.rmutex);
    return id;
}

extern int espefi_event_del_listener(espefi_event_id_t id, espefi_event_listener_id_t listener_id){
    int err = 1;
    xSemaphoreTakeRecursive(event_storage.rmutex, portMAX_DELAY);

    if(listener_id){
        espefi_event_t* event = find_event_byid(id);
        if(event){
            espefi_event_listener_t *listener = NULL, *tmp = NULL;
            list_for_each_entry_safe_reverse(listener, tmp, &event->listeners, list){
                if(listener->id == listener_id){
                    list_del(&listener->list);
                    free(listener);
                    err = 0;
                    break;
                }
            }
        }
    }

    xSemaphoreGiveRecursive(event_storage.rmutex);
    return err;
}

extern int espefi_event_del_raw(espefi_event_t* event){
    int err = 1;
    xSemaphoreTakeRecursive(event_storage.rmutex, portMAX_DELAY);
    if(event){
        espefi_event_listener_t *listener = NULL, *tmp = NULL;
        list_for_each_entry_safe_reverse(listener, tmp, &event->listeners, list){ //destroy listener structs first to freeup memory
            list_del(&listener->list);
            free(listener);
        }
        list_del(&event->list);
        free(event);
        err = 0;
    }

    xSemaphoreGiveRecursive(event_storage.rmutex);  
    return err;  
}

extern int espefi_event_del(espefi_event_id_t id){
    return espefi_event_del_raw(find_event_byid(id));
}

extern int espefi_event_del_ownedby(espefi_app_t* owner){

    ESP_LOGI("espefi_event","deleting all events owned by %p\n",owner);

    int err = 0;
    xSemaphoreTakeRecursive(event_storage.rmutex, portMAX_DELAY);
    espefi_event_t* cur = NULL, *tmp = NULL;
    list_for_each_entry_safe_reverse(cur,tmp,&event_storage.events,list){
        if(cur->owner == owner){
            ESP_LOGI("espefi_event","\t deleting event %s\n",cur->name);
            if(espefi_event_del_raw(cur) != 0){
                err = 1;
                break;
            }
        }
    }
    cur = NULL; tmp = NULL;
    list_for_each_entry_safe_reverse(cur,tmp,&event_storage.events,list){
        espefi_event_listener_t *listener = NULL, *tmpl = NULL;
        list_for_each_entry_safe_reverse(listener, tmpl, &cur->listeners, list){ //destroy listener structs first to freeup memory
            if(listener->owner == current_app){
                ESP_LOGI("espefi_event","\t deleting listener %ld from event %s\n",listener->id, cur->name);
                list_del(&listener->list);
                free(listener);    
            }
        }
    }

    xSemaphoreGiveRecursive(event_storage.rmutex);
    return err;    
}

extern int espefi_event_trigger(char* const event_name, void* event_ctx){
    int err = 1;
    xSemaphoreTakeRecursive(event_storage.rmutex, portMAX_DELAY);

    espefi_event_t* event = find_event(event_name);
    if(event){
        espefi_event_listener_t* listener = NULL;
        list_for_each_entry(listener, &event->listeners, list){
            listener->handler(event->name,listener->ctx,event_ctx);
        }
        err = 0;
    }
    xSemaphoreGiveRecursive(event_storage.rmutex);
    return err;    
}

extern void espefi_events_init(){
    INIT_LIST_HEAD(&event_storage.events);
    event_storage.rmutex = xSemaphoreCreateRecursiveMutexStatic(&event_storage.rmutexbuf);

    espefi_mutable_api.event.create = espefi_event_create;
    espefi_mutable_api.event.delete = espefi_event_del;
    espefi_mutable_api.event.trigger = espefi_event_trigger;
    espefi_mutable_api.event.add_listener = espefi_event_add_listener;
    espefi_mutable_api.event.del_listener = espefi_event_del_listener;
}