#include "espefi_api.h"
#include "espefi_api_internal.h"
#include "espefi_apploader.h"
#include "portmacro.h"

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <esp_log.h>


enum path_converter_state{
    FIRST_DOT,
    POST_DOTS,
};
static char* convert_to_absolute_path(const char* path, char* buf, int buflen){
    char* ret = NULL;
    if(strlen(path) < buflen){
        int dot_count = 0;
        const char* tmp_path = path;
        while(*tmp_path){
            if(*tmp_path == '.' && *(tmp_path + 1) != '.')
                dot_count++;
            tmp_path++;
        }

        if(strlen(path) + (current_app == NULL ? 1 : strlen(current_app->cwd)) < buflen){
            enum path_converter_state state = FIRST_DOT;
            char* dots[dot_count];
            
            const char* dot_finder = path;
            char* current_dot = NULL;
            int dot_pos = 0;
            while((current_dot = strchr(dot_finder,'.'))){
                if(*(current_dot+1) == '/'){
                    dots[dot_pos++] = current_dot;
                }
                dot_finder = current_dot+1;
            }
            if(dot_pos == 0){
                memcpy(buf,path,strlen(path) + 1);
            }

            char* outbuf_buf = buf;
            for(unsigned i = 0; i < dot_pos; i++){
                char* dot = dots[i];

                switch(state){
                    case FIRST_DOT:
                        if((dot + 2) - path  < strlen(path))
                            outbuf_buf += sprintf(outbuf_buf,"%s%s",(current_app == NULL ? "/" : current_app->cwd),dot + 2); //insert path and pos dot text
                        state = POST_DOTS;
                        break;
                    case POST_DOTS:
                        char* inpath_dot = strchr(buf,'.');
                        if(*(inpath_dot + 1) == '/')
                            memmove(inpath_dot - 1,inpath_dot + 1, (outbuf_buf - buf) - (inpath_dot - buf));
                        break;
                }
            }
            ret = buf;
        }
    }
    return ret;
}

static FILE* patched_fopen(const char* path, const char* mode){
    int path_buflen = strlen(path) + (current_app == NULL ? 1 + strlen(path) : strlen(current_app->cwd) + strlen(path));
    char path_buf[path_buflen + 1];
    memset(path_buf,0,path_buflen + 1);

    return fopen(convert_to_absolute_path(path, path_buf, path_buflen),mode);
}

static int patched_open(const char* path, int mode){
    int path_buflen = strlen(path) + (current_app == NULL ? 1 + strlen(path) : strlen(current_app->cwd) + strlen(path));
    char path_buf[path_buflen + 1];
    memset(path_buf,0,path_buflen);

    return open(convert_to_absolute_path(path, path_buf, path_buflen),mode);

}

static DIR* patched_opendir(const char* path){
    int path_buflen = strlen(path) + (current_app == NULL ? 1 + strlen(path) : strlen(current_app->cwd) + strlen(path));
    char path_buf[path_buflen + 1];
    memset(path_buf,0,path_buflen + 1);

    return opendir(convert_to_absolute_path(path, path_buf, path_buflen));
}

static int patched_mkdir(const char* path, int mode){
    int path_buflen = strlen(path) + (current_app == NULL ? 1 + strlen(path) : strlen(current_app->cwd) + strlen(path));
    char path_buf[path_buflen + 1];
    memset(path_buf,0,path_buflen + 1);

    return mkdir(convert_to_absolute_path(path, path_buf, path_buflen),mode);
}

static int patched_remove(const char* path){
    int path_buflen = strlen(path) + (current_app == NULL ? 1 + strlen(path) : strlen(current_app->cwd) + strlen(path));
    char path_buf[path_buflen + 1];
    memset(path_buf,0,path_buflen + 1);

    return remove(convert_to_absolute_path(path, path_buf, path_buflen));
}

static int patched_rename(const char* old, const char* new){
    int old_buflen = strlen(old) + (current_app == NULL ? 1 + strlen(old) : strlen(current_app->cwd) + strlen(old));
    int new_buflen = strlen(new) + (current_app == NULL ? 1 + strlen(new) : strlen(current_app->cwd) + strlen(new));

    char new_fixed[new_buflen];
    char old_fixed[old_buflen];

    return rename(convert_to_absolute_path(old, old_fixed, old_buflen),convert_to_absolute_path(new, new_fixed, new_buflen));
}

static char* patched_getcwd(char* buf, int path_len){
    if(buf == NULL){
        buf = malloc((current_app == NULL ? sizeof("/") : strlen(current_app->cwd) + 1));
        if(buf == NULL){
            ESP_LOGE("espefi_posix", "cannot allocate %d bytes for getcwd()\n",(current_app == NULL ? sizeof("/") : strlen(current_app->cwd) + 1));
            return NULL;
        }
        path_len = strlen(current_app->cwd) + 1;
    }
    memcpy(buf,(current_app == NULL ? "/" : current_app->cwd), (current_app == NULL ? sizeof("/") : (path_len > sizeof(current_app->cwd) ? sizeof(current_app->cwd) + 1 : path_len)));
    return buf;
}

static int patched_chdir(const char* path){
    if(current_app == NULL){
        return -ENOSYS;
    }
    DIR* cddir = patched_opendir(path);
    if(cddir){
        closedir(cddir);
        memcpy(current_app->cwd,path,strlen(path) > sizeof(current_app->cwd) ? sizeof(current_app->cwd) : strlen(path));
        return 0;
    }
    return -ENOENT;
}

//This module exist to patch some posix-incompabilitys within espidf
extern void espefi_posix_init(){
    espefi_mutable_api.posix.chdir = patched_chdir;
    espefi_mutable_api.posix.getcwd = patched_getcwd;
    espefi_mutable_api.posix.fopen = patched_fopen;
    espefi_mutable_api.posix.open = patched_open;
    espefi_mutable_api.posix.opendir = patched_opendir;
    espefi_mutable_api.posix.mkdir = patched_mkdir;
    espefi_mutable_api.posix.rename = patched_rename;
    espefi_mutable_api.posix.remove  = patched_remove;
}