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


const struct esp_elfsym g_customer_elfsyms[] = {
    {"abort",elf_abort},
    {"exit",elf_exit},
    ESP_ELFSYM_EXPORT(espefi_api),

    ESP_ELFSYM_END
};
