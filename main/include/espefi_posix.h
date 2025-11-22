#pragma once
#include "espefi_api.h"

#include <stdio.h>
#include <dirent.h>

//API module to patch some posix inconsistencyes in esp-idf
typedef struct{
    FILE* (*fopen)(const char* path, const char* mode);
    int (*open)(const char* path, int mode);
    DIR* (*opendir)(const char* path);

    char* (*getcwd)(char* buf, int path_len);
    int (*chdir)(const char* path);
    int (*remove)(const char* path);
    int (*mkdir)(const char* path, int mode);
    int (*rename)(const char* old, const char* new);
}espefi_posix_api;