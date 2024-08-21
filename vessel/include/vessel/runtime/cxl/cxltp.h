#pragma once
#include <sys/uio.h>

#include <base/stddef.h>
#include <base/bitmap.h>

struct link_pool_status {
    atomic64_t *heads;
    atomic64_t *tails;
    size_t size;
    bool *shutdown_ptr;
};

struct cxltp_addr {
    void    *base;
    uint64_t port;
};
typedef struct cxltp_addr cxltp_addr_t;

struct cxltpqueue;
typedef struct cxltpqueue cxltpqueue_t;
struct cxltpconn;
typedef struct cxltpconn cxltpconn_t;
extern struct list_head cxltp_conns;



extern int cxltp_dial(cxltp_addr_t cxltp_addr,
		    cxltpconn_t **c_out);
// extern int cxltp_dial_affinity(uint32_t affinity, cxltp_addr_t cxltp_addr,
// 		    cxltpconn_t **c_out);
// extern int cxltp_dial_conn_affinity(cxltpconn_t *in, cxltp_addr_t cxltp_addr,
// 		    cxltpconn_t **c_out);
extern int cxltp_listen(cxltp_addr_t *cxltp_addr, size_t backlog, cxltpqueue_t **q_out);
extern int cxltp_accept(cxltpqueue_t *q, cxltpconn_t **c_out);
extern void cxltp_qshutdown(cxltpqueue_t *q);
extern void cxltp_qclose(cxltpqueue_t *q);
extern cxltp_addr_t cxltp_addr(cxltpconn_t *c);
extern ssize_t cxltp_read(cxltpconn_t *c, void *buf, size_t len);
extern ssize_t cxltp_try_read(cxltpconn_t *c, void *buf, size_t len);
extern ssize_t cxltp_write(cxltpconn_t *c, const void *buf, size_t len);
extern ssize_t cxltp_try_write(cxltpconn_t *c, const void *buf, size_t len);
// extern ssize_t cxltp_readv(cxltpconn_t *c, const struct iovec *iov, int iovcnt);
extern ssize_t cxltp_writev(cxltpconn_t *c, const struct iovec *iov, int iovcnt);
extern int cxltp_shutdown(cxltpconn_t *c);
extern void cxltp_abort(cxltpconn_t *c);
extern void cxltp_close(cxltpconn_t *c);
