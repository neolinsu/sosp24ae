#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef void*(*env_malloc_func_t)(size_t);
typedef void(*env_free_func_t)(void *);

static inline bool match_key(char* item, char* key_item) {
    int i;
    for (i=0; key_item[i] != '=' && item[i]!= '\0'; ++i) {
        
        if (key_item[i] != item[i]) {
            goto not_matched;
        }
    } 
    if (key_item[i]=='=' && item[i]=='=') {
        return true;
    }
not_matched:
    return false;
}

static inline char** in_keys(char* item, char** keys) {
    char **pos;
    for (pos=keys; *pos!=NULL; ++pos) {
        if (match_key(item, *pos)) {
            goto is_in;
        }
    }
is_in:
    return pos;
}

static inline char** gen_new_env(char **env, char* new_settings, size_t keys_num,
                        env_malloc_func_t malloc_func, env_free_func_t free_func) {
    char *cpos;
    int i=0;
    size_t total_size = 0; 
    size_t new_num = 0;
    char **keys, **pos, **new_envs;
    int * key_sizes;
    bool * key_matcheds;
    char *new_env;
    keys = (char**) malloc_func((keys_num + 1) * sizeof(char*));
    key_sizes = (int*) malloc_func((keys_num + 1) * sizeof(int));
    key_matcheds = (bool*) malloc_func((keys_num + 1) * sizeof(bool));
    if(new_settings!=NULL) {
        i = 0; cpos=new_settings;
        *(keys) = new_settings;
        char *prepos = new_settings;
        for (; i < keys_num ; cpos++) {
            if (*cpos=='\n' || *cpos=='\0') {
                i++;
                *(keys+i) = cpos + 1;
                *(key_matcheds + i - 1) = false;
                *(key_sizes + i - 1) = cpos - prepos;
                log_info("key_sizes %d: %d", i-1, *(key_sizes + i - 1));
                prepos = cpos + 1;
                if (*cpos=='\0') break;
            }
        }
    }
    *(keys + keys_num) = NULL;

    for (pos=env; (*pos)!=NULL; ++pos) {
        char **matched = in_keys(*pos, keys);
        if (*matched != NULL) {
            total_size += *(key_sizes + (matched-keys)) + 1;
            *(key_matcheds + (matched-keys)) = true;
        } else {
            total_size += strlen(*pos) + 1;
        }
        new_num ++;
    }
    for (i=0; i<keys_num; ++i) {
        if (!*(key_matcheds + i)) {
            total_size += *(key_sizes + i) + 1;
            new_num++;
        }
    }
    new_env = (char*) malloc_func(total_size);
    memset(new_env, 0, total_size);
    new_envs = (char**) malloc_func((new_num + 1) * sizeof(char**));
    memset(new_envs, 0, (new_num + 1) * sizeof(char**));
    for (pos=env, i=0, cpos=new_env; (*pos)!=NULL; ++pos, ++i) {
        char **matched = in_keys(*pos, keys);
        size_t tmp_size = 0;
        if (*matched==NULL) {
            tmp_size = strlen(*pos);
            memcpy(cpos, *pos, tmp_size);
        } else {
            tmp_size = *(key_sizes + (matched-keys));
            memcpy(cpos, *matched, tmp_size);
        }
        *(cpos + tmp_size) = '\0';
        *(new_envs + i) = cpos;
        cpos += tmp_size + 1;
    }
    for (int j=0; j < keys_num; ++j) {
        if(*(key_matcheds+j)==false) {
            size_t tmp_size = *(key_sizes+j);
            memcpy(cpos, *(keys+j), tmp_size);
            *(cpos + tmp_size) = '\0';
            *(new_envs + i) = cpos;
            i++; cpos += tmp_size + 1;
        }
    }
    *(new_envs + new_num) = NULL;
    fflush(stdout);
    free_func(keys);
    free_func(key_sizes);
    free_func(key_matcheds);
    return new_envs;
}

static inline char** gen_new_argv(int argc, char **argv,
                                env_malloc_func_t malloc_func, env_free_func_t free_func) {
    char **pos, *cpos;
    int i=0;
    char **new_argvs = (char **) malloc_func((argc + 1) * sizeof(char*));
    size_t total_size = 0;
    i = 0;
    for (pos=argv;i<argc;i++) {
        total_size += strlen(*pos) + 1;
    }
    char *new_argv = (char*) malloc_func(total_size);
    i = 0; cpos=new_argv; pos=argv;
    for (; i<argc; ++i, ++pos) {
        size_t tmp_size = strlen(*pos);
        memcpy(cpos, *pos, tmp_size);
        *(cpos + tmp_size) = '\0';
        *(new_argvs + i) = cpos;
        cpos = cpos + tmp_size + 1;
    }
    *(new_argvs + i)=NULL;
    return new_argvs;
}

static inline void free_envs(char** envs,
                            env_free_func_t free_func) {
    char **pos;
    for(pos=envs; pos!=NULL; pos++) {
        free_func(*pos);
    }
    free_func(pos);
}

static inline void free_argv(int argc, char** argv,
                            env_free_func_t free_func) {
    char **pos;
    int i=0;
    for(pos=argv; i<argc; i++,pos++) {
        free_func(*pos);
    }
    free_func(pos);
}