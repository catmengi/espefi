/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <reent.h>
#include <pthread.h>
#include <setjmp.h>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <math.h>

#include "esp_timer.h"

#include "rom/ets_sys.h"

#include "private/elf_symbol.h"

extern int __ltdf2(double a, double b);
extern unsigned int __fixunsdfsi(double a);
extern int __gtdf2(double a, double b);
extern double __floatunsidf(unsigned int i);
extern double __divdf3(double a, double b);
extern long long __divdi3(long long a, long long b);
extern float __divsf3(float a, float b);
extern double __extendsfdf2 (float a);

/** @brief Libc public functions symbols look-up table */

static const struct esp_elfsym g_esp_libc_elfsyms[] = {

    /* string.h */

    ESP_ELFSYM_EXPORT(strerror),
    ESP_ELFSYM_EXPORT(memset),
    ESP_ELFSYM_EXPORT(memcpy),
    ESP_ELFSYM_EXPORT(strlen),
    ESP_ELFSYM_EXPORT(strtod),
    ESP_ELFSYM_EXPORT(strrchr),
    ESP_ELFSYM_EXPORT(strchr),
    ESP_ELFSYM_EXPORT(strcmp),
    ESP_ELFSYM_EXPORT(strcasecmp),
    ESP_ELFSYM_EXPORT(strncasecmp),
    ESP_ELFSYM_EXPORT(atoi),
    ESP_ELFSYM_EXPORT(itoa),
    ESP_ELFSYM_EXPORT(atol),
    ESP_ELFSYM_EXPORT(strtol),
    ESP_ELFSYM_EXPORT(strcspn),
    ESP_ELFSYM_EXPORT(strncat),
    ESP_ELFSYM_EXPORT(strdup),
    ESP_ELFSYM_EXPORT(strncpy),
    ESP_ELFSYM_EXPORT(strncmp),
    ESP_ELFSYM_EXPORT(memmove),
    ESP_ELFSYM_EXPORT(memcmp),

    /* stdio.h */

    ESP_ELFSYM_EXPORT(puts),
    ESP_ELFSYM_EXPORT(putchar),
    ESP_ELFSYM_EXPORT(getc),
    ESP_ELFSYM_EXPORT(getchar),
    ESP_ELFSYM_EXPORT(fputc),
    ESP_ELFSYM_EXPORT(fputs),
    ESP_ELFSYM_EXPORT(printf),
    ESP_ELFSYM_EXPORT(vfprintf),
    ESP_ELFSYM_EXPORT(vsnprintf),
    ESP_ELFSYM_EXPORT(sprintf),
    ESP_ELFSYM_EXPORT(snprintf),
    ESP_ELFSYM_EXPORT(fprintf),
    ESP_ELFSYM_EXPORT(fread),
    ESP_ELFSYM_EXPORT(fwrite),
    ESP_ELFSYM_EXPORT(fopen),
    ESP_ELFSYM_EXPORT(fclose),
    ESP_ELFSYM_EXPORT(ftell),
    ESP_ELFSYM_EXPORT(fflush),
    ESP_ELFSYM_EXPORT(fseek),
    ESP_ELFSYM_EXPORT(remove),
    ESP_ELFSYM_EXPORT(rename),
    ESP_ELFSYM_EXPORT(mkdir),
    ESP_ELFSYM_EXPORT(sscanf),

    /* unistd.h */

    ESP_ELFSYM_EXPORT(usleep),
    ESP_ELFSYM_EXPORT(sleep),
    ESP_ELFSYM_EXPORT(close),
    ESP_ELFSYM_EXPORT(open),
    ESP_ELFSYM_EXPORT(read),
    ESP_ELFSYM_EXPORT(write),
    ESP_ELFSYM_EXPORT(fsync),
    ESP_ELFSYM_EXPORT(lseek),
    ESP_ELFSYM_EXPORT(close),
    ESP_ELFSYM_EXPORT(system),

    /* stdlib.h */

    ESP_ELFSYM_EXPORT(malloc),
    ESP_ELFSYM_EXPORT(calloc),
    ESP_ELFSYM_EXPORT(realloc),
    ESP_ELFSYM_EXPORT(free),

    /* time.h */

    ESP_ELFSYM_EXPORT(clock_gettime),
    ESP_ELFSYM_EXPORT(strftime),

    /* newlib */

    ESP_ELFSYM_EXPORT(__errno),
    ESP_ELFSYM_EXPORT(__getreent),
#ifdef __HAVE_LOCALE_INFO__
    ESP_ELFSYM_EXPORT(__locale_ctype_ptr),
#else
    ESP_ELFSYM_EXPORT(_ctype_),
#endif

    /* math */

    ESP_ELFSYM_EXPORT(__ltdf2),
    ESP_ELFSYM_EXPORT(__fixunsdfsi),
    ESP_ELFSYM_EXPORT(__gtdf2),
    ESP_ELFSYM_EXPORT(__floatunsidf),
    ESP_ELFSYM_EXPORT(__divdf3),
    ESP_ELFSYM_EXPORT(__divdi3),
    ESP_ELFSYM_EXPORT(__divsf3),
    ESP_ELFSYM_EXPORT(__extendsfdf2),

    /* getopt.h */

    ESP_ELFSYM_EXPORT(getopt_long),
    ESP_ELFSYM_EXPORT(optind),
    ESP_ELFSYM_EXPORT(opterr),
    ESP_ELFSYM_EXPORT(optarg),
    ESP_ELFSYM_EXPORT(optopt),

    /* setjmp.h */

    ESP_ELFSYM_EXPORT(longjmp),
    ESP_ELFSYM_EXPORT(setjmp),

    ESP_ELFSYM_EXPORT(__assert_func),

    ESP_ELFSYM_END
};

/** @brief ESP-IDF public functions symbols look-up table */

static const struct esp_elfsym g_esp_espidf_elfsyms[] = {

    /* sys/socket.h */

    ESP_ELFSYM_EXPORT(lwip_bind),
    ESP_ELFSYM_EXPORT(lwip_setsockopt),
    ESP_ELFSYM_EXPORT(lwip_socket),
    ESP_ELFSYM_EXPORT(lwip_listen),
    ESP_ELFSYM_EXPORT(lwip_accept),
    ESP_ELFSYM_EXPORT(lwip_recv),
    ESP_ELFSYM_EXPORT(lwip_recvfrom),
    ESP_ELFSYM_EXPORT(lwip_send),
    ESP_ELFSYM_EXPORT(lwip_sendto),
    ESP_ELFSYM_EXPORT(lwip_connect),

    /* arpa/inet.h */

    ESP_ELFSYM_EXPORT(ipaddr_addr),
    ESP_ELFSYM_EXPORT(lwip_htons),
    ESP_ELFSYM_EXPORT(lwip_htonl),
    ESP_ELFSYM_EXPORT(ip4addr_ntoa),

    /* esp_timer */
    ESP_ELFSYM_EXPORT(esp_timer_get_time),

    /* ROM functions */

    ESP_ELFSYM_EXPORT(ets_printf),

    ESP_ELFSYM_END
};

/**
 * @brief Find symbol address by name.
 *
 * @param sym_name - Symbol name
 *
 * @return Symbol address if success or 0 if failed.
 */
uintptr_t elf_find_sym(const char *sym_name)
{
    const struct esp_elfsym *syms;

#ifdef CONFIG_ELF_LOADER_LIBC_SYMBOLS
    syms = g_esp_libc_elfsyms;
    while (syms->name) {
        if (!strcmp(syms->name, sym_name)) {
            return (uintptr_t)syms->sym;
        }

        syms++;
    }
#else
    syms = g_esp_libc_elfsyms;
    (void)syms;
#endif

#ifdef CONFIG_ELF_LOADER_ESPIDF_SYMBOLS
    syms = g_esp_espidf_elfsyms;
    while (syms->name) {
        if (!strcmp(syms->name, sym_name)) {
            return (uintptr_t)syms->sym;
        }

        syms++;
    }
#else
    syms = g_esp_espidf_elfsyms;
    (void)syms;
#endif

#ifdef CONFIG_ELF_LOADER_CUSTOMER_SYMBOLS
    extern const struct esp_elfsym g_customer_elfsyms[];

    syms = g_customer_elfsyms;
    while (syms->name) {
        if (!strcmp(syms->name, sym_name)) {
            return (uintptr_t)syms->sym;
        }

        syms++;
    }
#endif

    return 0;
}
