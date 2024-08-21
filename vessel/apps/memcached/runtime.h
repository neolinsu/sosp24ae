#pragma once

#include <base/lock.h>
#include <base/log.h>
#include <runtime/tls/tcache.h>

#include <net/ip.h>

#include <runtime/simple_sync.h>
#ifdef VESSEL_CXLTP
#include <runtime/cxl/cxltp.h>
#define tcpqueue_t cxltpqueue_t
#define tcpconn_t cxltpconn_t
#define tcp_dial cxltp_dial
#define tcp_listen cxltp_listen
#define tcp_accept cxltp_accept
#define tcp_read cxltp_read
#define tcp_write cxltp_write
#define tcp_writev cxltp_writev
#define tcp_abort cxltp_abort
#define tcp_close cxltp_close
#else
#include <runtime/tcp.h>
#endif
#include <runtime/thread.h>
#include <runtime/timer.h>
#include <runtime/udp.h>
#include <runtime/runtime.h>
#include <runtime/rculist.h>

#undef assert
#define assert(x) BUG_ON(!(x))
