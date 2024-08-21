#pragma once
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <runtime/runtime.h>
#include <runtime/tcp.h>
#include <runtime/cxl/cxltp.h>

#if __BIG_ENDIAN__
static inline uint64_t htonll(uint64_t x) {
    return x;
}
static inline uint64_t ntohll(uint64_t x) {
    return x;
}
#else
static inline uint64_t htonll(uint64_t x) {
    return ((uint64_t)htonl((x) & 0xFFFFFFFFll) << 32) | (uint64_t) htonl((x) >> 32);
}
static inline uint64_t ntohll(uint64_t x) {
    return ((uint64_t)ntohl((x) & 0xFFFFFFFFll) << 32) | (uint64_t) ntohl((x) >> 32);
}
#endif

typedef ssize_t (*try_write_func_t) (void* buf, size_t len);
typedef ssize_t (*try_read_func_t) (void* buf, size_t len);
typedef ssize_t (*conn_op_func_t) (void* conn, void* buf, size_t len);

enum ConnectType {
    CXLTP,
    TCP
};

struct cxltp_args {
    void*    base;
    uint64_t port;
};

static inline void* utils_get_try_write(enum ConnectType type) {
    switch(type) {
        case CXLTP:
            return cxltp_try_write;
        break;
        case TCP:
            return tcp_try_write;
        break;
        default:
        return NULL;
    }
}

static inline void* utils_get_try_read(enum ConnectType type) {
    switch(type) {
        case CXLTP:
            return cxltp_try_read;
        break;
        case TCP:
            return tcp_try_read;
        break;
        default:
        return NULL;
    }
}
static inline int utils_connect(enum ConnectType type, void* a, void** conn) {
    int ret;
    switch(type) {
        case CXLTP:{
            struct cxltp_args *args = a;
            struct cxltp_addr addr = {.base=args->base, .port=args->port};
            ret = cxltp_dial(addr, (cxltpconn_t **) conn);
            if (ret) {
                printf("Fail to dial for %d", ret);
                return ret;
            } else {
                printf("Get ont conn\n");
            }
        }
        break;
        case TCP: {
            struct netaddr *raddr = a;
            ret = tcp_dial_affinity(get_current_affinity(), *raddr, (tcpconn_t **)conn);
            if (ret) {
                printf("Fail to dial for %d", ret);
                return ret;
            }
        }
        break;
        default:
        return -EINVAL;
    }
    return 0;
}

static inline int utils_close_connect(enum ConnectType type, void* conn) {
    switch(type) {
        case CXLTP:
            cxltp_close((cxltpconn_t *)conn);
        break;
        case TCP:
            tcp_close((tcpconn_t *) conn);
        break;
        default:
        return -EINVAL;
    }
    return 0;
}