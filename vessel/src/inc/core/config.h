#pragma once
#include <base/log.h>


#define VENV(NAME) "VESSEL_"#NAME
#define register_param(NAME, ENV_NAME, TYPE, DEFAULT) {.p = (void*)&NAME, .env_name=ENV_NAME, .type=TYPE, .def=DEFAULT}

extern char* vessel_name;

enum CONFIG_TYPE {
    INT=1, STR, UINT
};
typedef enum CONFIG_TYPE config_type_t;

struct config_handler {
    void * p;
    char * env_name;
    config_type_t type;
    char * def;
};
typedef struct config_handler config_handler_t;

extern int config_init(void);