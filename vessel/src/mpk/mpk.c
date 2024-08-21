#include "mpk/mpk.h"
#include "base/log.h"


int mpk_init(void) {
    int ret = 0;
    for (int i=1; i<16; i++) {
        ret = pkey_alloc(0, 0);
        if(ret == -1) {
            log_err("mpk_init: key error when ret = %d and i = %d.", ret, i);
        }
    }
    ret = 0;
    return ret;
}