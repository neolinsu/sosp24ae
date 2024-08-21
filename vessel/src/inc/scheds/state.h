#pragma once

#include <sys/ipc.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/types.h>
#include <base/stddef.h>
#include <base/limits.h>
#include <base/list.h>
#include <base/mem.h>

#define UIPI_VECTOR_CEDE    1
#define UIPI_VECTOR_YIELD   2

#define VESSEL_CORE_STATE_ID_SERVER 31085
#define VESSEL_CORE_STATE_ID_CLIENT 31086
#define VESSEL_CORE_STATE_BASE      0x9000000000llu

struct core_conn {
    pid_t    next_tid;
    volatile uint32_t gen;
    volatile uint32_t last_gen;
    volatile int yield_fd;
    volatile int cede_fd;
    int op;
    atomic_t idling;
    bool to_steal;
    uint8_t pad[CACHE_LINE_SIZE
                - sizeof(pid_t)
                - sizeof(uint32_t)*6
                - sizeof(bool)];

    uint64_t pmc_val;
    uint64_t pmc_tsc;
    bool     pmc_done;
    uint8_t pad1[CACHE_LINE_SIZE
                - sizeof(uint64_t) *2
                - sizeof(bool)];
};
BUILD_ASSERT(sizeof(struct core_conn)==2*CACHE_LINE_SIZE);
struct core_conn_map {
    struct core_conn map[NCPU];
};

#define VESSEL_CORE_STATE_SIZE align_up(sizeof(struct core_conn_map), PGSIZE_2MB)

struct uipi_fd {
    struct list_node link;
    int core_id;
    int fd;
};


#define ctrl_size (sizeof (struct cmsghdr) + sizeof (int))

static inline ssize_t sendmsg_with_sock (int fd, int s, void **data, size_t *lenv, size_t n) {
  struct iovec iov[n];
  char ctrl[ctrl_size];
  struct msghdr msg = {
    .msg_iov        = iov,
    .msg_iovlen     = n,
    .msg_control    = ctrl,
    .msg_controllen = 0,
  };

  for (size_t i = 0; i < n; i++) {
    iov[i].iov_base = data[i];
    iov[i].iov_len  = lenv[i];
  }

  if (s >= 0) {
    msg.msg_controllen = ctrl_size;

    struct cmsghdr *cmsg = (struct cmsghdr *) ctrl;
    cmsg->cmsg_len   = ctrl_size;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    int *cdata = (int *)CMSG_DATA(cmsg);
    *cdata = s;
  }

  return (sendmsg (fd, &msg, 0));
}

static inline int recvmsg_with_sock (int fd, int *s, void **data, size_t *lenv, size_t n) {

  struct iovec iov[n];
  char ctrl[ctrl_size];
  struct msghdr msg = {
    .msg_iov        = iov,
    .msg_iovlen     = n,
    .msg_control    = ctrl,
    .msg_controllen = ctrl_size,
  };

  struct cmsghdr *cmsg = (struct cmsghdr *) ctrl;
  cmsg->cmsg_len   = ctrl_size;
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type  = SCM_RIGHTS;

  for (size_t i = 0; i < n; i++) {
    iov[i].iov_base = data[i];
    iov[i].iov_len  = lenv[i];
  }

  *s = -1;
  int res = recvmsg (fd, &msg, 0);
  if (res >= 0 && msg.msg_controllen == ctrl_size) {
    int *cdata = (int *)CMSG_DATA(cmsg);
    *s = *cdata;
  } else {
    log_err("recvmsg_with_sock !!!!!!!!!");
    abort();
  }

  return res;
}

static inline void send_my_fd(int sfd, int fd) {
    size_t len[1];
	 void* data[1];
    int magic = 12138;

    data[0] = &magic;
    len [0] = sizeof(magic);
    sendmsg_with_sock(sfd, fd, data, len, 1); 
}

static inline void read_my_fd(int sfd, int *fd) {
    size_t len[1];
	  void* data[1];
    data[0] = malloc(sizeof(int));
    len[0] = sizeof(int);
    recvmsg_with_sock(sfd, fd, data, len, 1);
}
