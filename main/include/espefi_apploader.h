#pragma once

#include <stdint.h>
#include <setjmp.h>

#include "freertos/freertos.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "multi_heap.h"

#include "list.h"
#include "esp_elf.h"

#define ENVIRON_MAX 1
#define MAX_PATH 255 + 1

typedef uint64_t espefi_app_id_t;
typedef struct espefi_app{
    struct list_head list;
    
    StaticSemaphore_t rmutexbuf;
    SemaphoreHandle_t rmutex;
    
    esp_elf_t elf;
    jmp_buf exit;

    char* path;
    char* app_environ[ENVIRON_MAX + 1];
    espefi_app_id_t id;
    
    TaskHandle_t app_task;
    char cwd[MAX_PATH]; 

    //multi_heap_handle_t app_heap;
}espefi_app_t;

typedef struct espefi_apploader_api{
   espefi_app_id_t (*app_load)(const char* path, int argc, char* argv[]);
   int (*app_wait)(espefi_app_id_t id);
}espefi_apploader_api_t;

extern __thread espefi_app_t* current_app;