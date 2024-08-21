#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "core/config.h"

#define EARLY_PRINT_ERROR "\e[5;40;31;m"
#define EARLY_PRINT_END   "\e[0m\n"

#define early_error(fmt, ...) printf(EARLY_PRINT_ERROR fmt EARLY_PRINT_END, ##__VA_ARGS__)

extern int force_init_memory_meta;
char* vessel_name;
extern int force_init_vessel_meta; // TODO: del
extern char *task_startup_settings;
extern char *ld_path;
extern int task_startup_settings_num;
extern int max_loglevel;

config_handler_t CONFIG_HANDLERS[] = {
    register_param(max_loglevel, VENV(LOGLEVEL), INT, "6"), // base/log.h
    register_param(force_init_memory_meta, VENV(FORCE_INIT_MEMORY_META), INT, "0"),
    register_param(vessel_name, VENV(NAME), STR, NULL),
    register_param(force_init_vessel_meta, VENV(FORCE_INIT_VESSEL_META), INT, "0"),
    register_param(task_startup_settings, VENV(TASK_STARTUP_SETTINGS), STR, "LD_PRELOAD=libvjemalloc.so.2\n"), 
    register_param(task_startup_settings_num, VENV(TASK_STARTUP_SETTINGS_NUM), INT, "1"),
    register_param(ld_path, VENV(LD_PATH), STR, "ld-linux-x86-64-vessel.so.2"),
};

int config_init(void) {
    int ret, cnt;
    int handler_num = sizeof(CONFIG_HANDLERS)/sizeof(config_handler_t);
    config_handler_t *t;
    for (t=CONFIG_HANDLERS, cnt=0; cnt<handler_num; t++, cnt ++) {
        char *value = getenv(t->env_name);
        if(value==NULL || strlen(value)<=0)
            value = t->def;
        switch (t->type)
        {
        case INT: {
                int arg;
                ret = sscanf(value,"%d", &arg);
                if (ret==1) 
                    *((int*)t->p) =  arg;
                else
                    early_error("Can not read int config:%s, %s", t->env_name, value);
            }
            break;
        case STR:
            *((char**)t->p) = value;
            break;
        case UINT: {
            uint32_t arg;
            ret = sscanf(value,"%u", &arg);
            if (ret==1)
                *((uint32_t*)t->p) =  arg;
            else
                early_error("Can not read int config:%s, %s", t->env_name, value);
            }
            break;
        default:
            early_error("Unknown config type: %d", t->type);
            break;
        }
    }
    if (vessel_name==NULL) {
        early_error("VESSEL_NAME is not set");
        return -1;
    }
    if( geteuid() != 0) {
        log_err("Vessel daemon should be run as root.");
        return EACCES;
	}
    return 0;
}