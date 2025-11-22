/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <esp_log.h>

#include "esp_attr.h"
#include "private/elf_symbol.h"
#include "espefi_apploader.h"
#include "espefi_api.h"

#include "ffi.h"


/* Extern declarations from ELF symbol table */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#pragma GCC diagnostic pop

/* Available ELF symbols table: g_customer_elfsyms */

static void elf_abort(void){
    ESP_LOGE(__PRETTY_FUNCTION__,"Abort caused by app '%s' (%zu)\n",current_app->path,(size_t)current_app->id);
    longjmp(current_app->exit,1);
}

static void elf_exit(int unused){
    elf_abort();
}

static FILE* fopen_intercept(const char* path, const char* mode){
    return espefi_api->posix.fopen(path,mode);
}
static int open_intercept(const char* path, int mode){
    return espefi_api->posix.open(path,mode);
}
static DIR* opendir_intercept(const char* path){
    return espefi_api->posix.opendir(path);
}
static char* getcwd_intercept(char* buf, int buflen){
    return espefi_api->posix.getcwd(buf,buflen);
}
static int chdir_intercept(const char* path){
    return espefi_api->posix.chdir(path);
}
static int mkdir_intercept(const char* path,int mode){
    return espefi_api->posix.mkdir(path,mode);
}
static int rename_intercept(const char* old, const char* new){
    return espefi_api->posix.rename(old,new);
}
static int remove_intercept(const char* path){
    return espefi_api->posix.remove(path);
}

const struct esp_elfsym g_customer_elfsyms[] = {
    {"abort",elf_abort},
    {"exit",elf_exit},
    {"fopen",fopen_intercept},
    {"open",open_intercept},
    {"opendir",opendir_intercept},
    {"getcwd",getcwd_intercept},
    {"chdir",chdir_intercept},
    {"rename",rename_intercept},
    {"remove",remove_intercept},
    {"mkdir",mkdir_intercept},
    ESP_ELFSYM_EXPORT(espefi_api),

    ESP_ELFSYM_END
};
