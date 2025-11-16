#include "espefi_api.h"
#include <string.h>

extern void shell_loop(){
    char* argv[17] = {0};
    int argc_max = sizeof(argv) / sizeof(argv[0]);

    while(1){
        espefi_api->console.set_immediate();
        int argc_cur = 0;
        char inbuffer[512] = {0};
        int pos = 0;

        while(pos < sizeof(inbuffer)){
            int input = 0;
            fread(&input,sizeof(char),1,stdin);

            putchar(input);
            fflush(stdout);

            if(input == '\n') break;
            if(input == '\t') continue;
            if(input == '\b'){
                if(pos > 0){
                    inbuffer[--pos] = '\0';
                    putchar(' ');
                    fflush(stdout);
                } else if(pos == 0){
                    inbuffer[0] = '\0';
                    putchar(' ');
                    fflush(stdout);
                }
            }

            if(input != EOF){
                inbuffer[pos++] = input;
            }
        }

        char* strtok_r_tmp = NULL;
        char* cur = NULL;

        argc_cur = 0;
        argv[argc_cur++] = strtok_r(inbuffer," ",&strtok_r_tmp);
        while((cur = strtok_r(NULL," ",&strtok_r_tmp)) && argc_cur < argc_max){
            argv[argc_cur++] = cur;
        }
        if(argv[0]){
            espefi_api->console.set_buffered();
            printf("starting app %s\n",argv[0]);
            printf("app exited with: %d\n",espefi_api->apploader.app_wait(espefi_api->apploader.app_load(argv[0],argc_cur - 1, &argv[1])));
            espefi_api->console.set_sync();
        }
    }
}