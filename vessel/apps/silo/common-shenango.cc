
extern "C" {
#include <base/log.h>
#include <runtime/runtime.h>
#include <runtime/smalloc.h>
#include <runtime/storage.h>
#include <runtime/preempt.h>
#include "common.h"
}

#include "net.h"
#include "sync.h"
#include "thread.h"

#include <iostream>
#include <memory>
#include <new>

uint16_t server_port;
__thread int thread_no;

struct Payload {
  uint64_t work_iterations;
  uint64_t index;
  uint64_t randomness;
};

class SharedTcpStream {
 public:
  SharedTcpStream(std::shared_ptr<rt::TcpConn> c) : c_(c) {}
  ssize_t WriteFull(const void *buf, size_t len) {
    rt::ScopedLock<rt::Mutex> lock(&sendMutex_);
    return c_->WriteFull(buf, len);
  }

 private:
  std::shared_ptr<rt::TcpConn> c_;
  rt::Mutex sendMutex_;
};

class RequestContext {
 public:
  RequestContext(std::shared_ptr<SharedTcpStream> c) : conn(c) {}
  Payload p;
  std::shared_ptr<SharedTcpStream> conn;
  void *operator new(size_t size) {
    void *p = smalloc(size);
    if (unlikely(p == nullptr)) throw std::bad_alloc();
    return p;
  }
  void operator delete(void *p) { sfree(p); }
};

void HandleRequest(RequestContext *ctx) {
  preempt_disable();
  process_request();
  preempt_enable();
  ssize_t ret = ctx->conn->WriteFull(&ctx->p, sizeof(ctx->p));
  if (ret != static_cast<ssize_t>(sizeof(ctx->p))) {
    if (ret != -EPIPE && ret != -ECONNRESET) log_err("tcp_write failed");
  }
}

void ServerWorker(std::shared_ptr<rt::TcpConn> c) {
  auto resp = std::make_shared<SharedTcpStream>(c);

 /* allocate context */
  auto ctx = new RequestContext(resp);
  while (true) {
    Payload *p = &ctx->p;

    /* Receive a work request. */
    ssize_t ret = c->ReadFull(p, sizeof(*p));
    if (ret != static_cast<ssize_t>(sizeof(*p))) {
      if (ret != 0 && ret != -ECONNRESET)
        log_err("read failed, ret = %ld", ret);
      delete ctx;
      return;
    }
#define OUT_OF_ORDER_CONN 1
#ifdef OUT_OF_ORDER_CONN
    rt::Thread([=] {
      HandleRequest(ctx);
      delete ctx;
    })
        .Detach();
    ctx = new RequestContext(resp);
#else
    HandleRequest(ctx);
#endif
  }
}

void MainHandler(void *arg) {
  std::unique_ptr<rt::TcpQueue> q(rt::TcpQueue::Listen({0, server_port}, 4096));
  if (q == nullptr) panic("couldn't listen for connections");

  log_err("listening for connections port %d", server_port);

  while (true) {
    rt::TcpConn *c = q->Accept();
    if (c == nullptr) panic("couldn't accept a connection");
    rt::Thread([=] { ServerWorker(std::shared_ptr<rt::TcpConn>(c)); }).Detach();
  }
}

int init_thread_wrap(void) {
  extern __thread unsigned int thread_id;
  thread_no = thread_id;
  init_thread();
  return 0;
}

int init_shenango(const char *cfgpath, int port) {
  int ret;
  runtime_set_initializers(init_global, init_thread_wrap, NULL);

  server_port = port;

  ret = runtime_init(cfgpath, MainHandler, NULL);
  if (ret) fprintf(stderr, "failed to start runtime\n");

  return ret;
}
