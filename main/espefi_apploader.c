#include "espefi_apploader.h"
#include "esp_heap_caps.h"
#include "espefi_api_internal.h"
#include "freertos/freertos.h"
#include "include/espefi_apploader.h"
#include "list.h"

#include "freertos/idf_additions.h"
#include "multi_heap.h"
#include "portmacro.h"

#include <fcntl.h>
#include <stdio.h>
#include <esp_log.h>
#include <string.h>

typedef struct espefi_loaded_apps{
    StaticSemaphore_t rmutexbuf;
    SemaphoreHandle_t rmutex;
    
    struct list_head apps_list;

    /*
    void* app_mem;
    size_t app_mem_size;
    multi_heap_handle_t app_heap;
    */
}espefi_loaded_apps_t;

static espefi_loaded_apps_t loaded_apps = {0};
__thread espefi_app_t* current_app = NULL;

extern int espefi_event_del_ownedby(espefi_app_t* owner);

static espefi_app_t* get_app(espefi_app_id_t id){
    espefi_app_t* app = NULL;
    xSemaphoreTakeRecursive(loaded_apps.rmutex, portMAX_DELAY);

    espefi_app_t* cur = NULL;
    list_for_each_entry(cur, &loaded_apps.apps_list, list){
        if(cur->id == id){
            app = cur;
            break;
        }
    }

    xSemaphoreGiveRecursive(loaded_apps.rmutex);
    return app;
}

static espefi_app_t* new_app(const char* path){
    xSemaphoreTakeRecursive(loaded_apps.rmutex, portMAX_DELAY);
    espefi_app_t* app = heap_caps_calloc(1,sizeof(*app) + strlen(path) + 2,MALLOC_CAP_SPIRAM);
    if(app){
        INIT_LIST_HEAD(&app->list);
        app->rmutex = xSemaphoreCreateRecursiveMutexStatic(&app->rmutexbuf);

        esp_elf_init(&app->elf);

        app->path = (char*)app + sizeof(*app) + 1;
        arc4random_buf(&app->id,sizeof(app->id));

        memcpy(app->path,path,strlen(path));

        list_add_tail(&app->list,&loaded_apps.apps_list);
    }
    xSemaphoreGiveRecursive(loaded_apps.rmutex);
    return app;
}

static void app_free(espefi_app_t* app){
    xSemaphoreTakeRecursive(loaded_apps.rmutex, portMAX_DELAY);

    xSemaphoreTakeRecursive(app->rmutex, portMAX_DELAY);

    list_del(&app->list);
    esp_elf_deinit(&app->elf);

    for(unsigned i = 0; i < ENVIRON_MAX; i++){
        free(app->app_environ[i]);
    }

    xSemaphoreGiveRecursive(app->rmutex);
    free(app);

    xSemaphoreGiveRecursive(loaded_apps.rmutex);
}

static void app_task(void* params_p){

    void** params = params_p;
    espefi_app_t* app = params[0];
    SemaphoreHandle_t proceed = params[3];
    xSemaphoreTakeRecursive(app->rmutex, portMAX_DELAY);
    xSemaphoreGive(proceed);
    
    int argc = (int)params[1];
    char** argv = params[2];
    char* argv_new[argc + 1];


    current_app = app;
    for(unsigned i = 0; i < argc; i++){
        argv_new[i + 1] = argv[i];
    }
    if(setjmp(app->exit) == 0)
        esp_elf_request(&app->elf,0,argc + 1, argv_new,app->app_environ);

    
    espefi_event_del_ownedby(app);
    app_free(app);

    xSemaphoreGiveRecursive(app->rmutex);
    current_app = NULL;

    vTaskDelete(NULL);
}

extern espefi_app_id_t espefi_apploader_load(const char* path, int argc, char* argv[]){
    espefi_app_id_t id = 0;

    xSemaphoreTakeRecursive(loaded_apps.rmutex, portMAX_DELAY);

    int elf_fd = open(path,O_RDONLY);
    uint8_t* elf_raw = NULL;
    if(elf_fd){
        int file_size = lseek(elf_fd,0,SEEK_END);
        lseek(elf_fd,-file_size,SEEK_END);

        elf_raw = heap_caps_malloc(file_size,MALLOC_CAP_SPIRAM);
        if(elf_raw){
            if(read(elf_fd,elf_raw,file_size) == file_size){
                espefi_app_t* app = new_app(path);
                if(app){
                    if(esp_elf_relocate(&app->elf, elf_raw) == ESP_OK){
                        free(elf_raw);
                        elf_raw = NULL; //prevent double free

                        StaticSemaphore_t sembuf;
                        SemaphoreHandle_t proceed = xSemaphoreCreateBinaryStatic(&sembuf);

                        char* path_end = strrchr(app->path, '/') + 1;
                        char* environ_path = heap_caps_malloc(strlen("PATH=") + path_end - app->path + 1, MALLOC_CAP_SPIRAM);

                        int path_size = path_end - app->path;
                        char path_cpy[path_size + 1];
                        memset(path_cpy,0,path_size + 1);

                        memcpy(path_cpy,app->path,path_size);
                        sprintf(environ_path,"PATH=%s",path_cpy);
                        
                        app->app_environ[0] = environ_path;
                        app->app_environ[ENVIRON_MAX] = NULL;
                        memcpy(app->cwd,path_cpy,path_size);

                        void* params[] = {app,(void*)argc,argv,proceed};
                        if(xTaskCreate(app_task,"app_task",4 * 1024 * sizeof(void*),params,10,&app->app_task) != pdPASS){
                            app_free(app);
                        } else xSemaphoreTake(proceed,portMAX_DELAY);

                        id = app->id;

                    } else app_free(app);
                }
            }
        }
    }

//exit:
    close(elf_fd);
    free(elf_raw);
    xSemaphoreGiveRecursive(loaded_apps.rmutex);
    return id;
}

extern int espefi_apploade_wait_app(espefi_app_id_t id){
    int err = 1;
    espefi_app_t* app = get_app(id);
    if(app){
        xSemaphoreTakeRecursive(app->rmutex, portMAX_DELAY);
        xSemaphoreGiveRecursive(app->rmutex);
        err = 0;
    }
    return err;
}

extern void espefi_apploader_init(){
    INIT_LIST_HEAD(&loaded_apps.apps_list);
    loaded_apps.rmutex = xSemaphoreCreateRecursiveMutexStatic(&loaded_apps.rmutexbuf);
    espefi_mutable_api.apploader.app_load = espefi_apploader_load;
    espefi_mutable_api.apploader.app_wait = espefi_apploade_wait_app;
}