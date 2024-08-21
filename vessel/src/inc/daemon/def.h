#pragma once
#include <base/stddef.h>

#define DAEMON_VALUE_MAX_LENGTH 2048

enum daemon_t {
  daemon_string,
  daemon_string_vec,
};

struct daemon_entry {
  enum daemon_t type;
  size_t length;
};
typedef struct daemon_entry daemon_entry_t;

#define VESSEL_CONTROL_INTER (999)
#define VESSEL_USER_ID (999)

static inline int create_chars_copy(char **dst, int fd, size_t len) {
  char *cpos;
  int index, ret;
  *dst = (char*) malloc(len);
  ret = read(fd, *dst, len);
  if (ret != len) return ret;
  for (index=0, cpos=*dst; *cpos!='\0' && index<len; ++cpos, ++index);
  if (unlikely(index!=len-1))
    free(*dst);
  return index + 1;
}

static inline int get_string(char **dst, int fd) {
  int ret;
  daemon_entry_t e;
  ret = read(fd, &e, sizeof(daemon_entry_t));
  if (ret != sizeof(daemon_entry_t)) {
    log_err("Can not read entry header");
    return -EINVAL;
  }
  ret = create_chars_copy(dst, fd, e.length);
  if (ret != e.length) {
    log_err("Read char miss mismatched.");
    return -EINVAL;
  }
  return ret;
}

static inline int put_string(char *src, int fd) {
  int ret;
  daemon_entry_t e;
  e.length = strlen(src) + 1;
  e.type = daemon_string;
  ret = write(fd, &e, sizeof(daemon_entry_t));
  if (ret != sizeof(daemon_entry_t)) {
    log_err("Can not write entry header");
    return -EINVAL;
  }
  ret = write(fd, src, e.length);
  if (ret != e.length) {
    log_err("Write char mismatched.");
    return -EINVAL;
  }
  return ret;
}

static inline int get_int(int *dst, int fd) {
  int ret;
  ret = read(fd, (void*)dst, sizeof(int));
  if (ret != sizeof(int)) {
    log_err("Can not read int");
    return -EINVAL;
  }
  return ret;
}

static inline int put_int(int src, int fd) {
  int ret;
  ret = write(fd, &src, sizeof(int));
  if (ret != sizeof(int)) {
    log_err("Can not write int");
    return -EINVAL;
  }
  return ret;
}

static inline int get_string_vec(char ***dst, int fd) {
  int ret, building_i;
  daemon_entry_t e;
  ret = read(fd, &e, sizeof(daemon_entry_t));
  if(ret!=sizeof(daemon_entry_t) || e.type!=daemon_string_vec) return -EINVAL;
  *dst = (char**) malloc(sizeof(char*) * (e.length + 1));
  for (building_i=0; building_i<e.length; ++building_i) {
    ret = get_string((*dst)+building_i, fd);
    printf("%s\n", *((*dst)+building_i));
    if (ret < 0) {
        building_i--;
        goto fail;
    }
  }
  ret = e.length;
  goto out;
fail:
    for (; building_i >= 0; building_i--) {
        free(*((*dst)+building_i));
    }
    free(*dst);
out:
  return ret;
}

static inline int put_string_vec(char **src, int fd) {
  int ret, building_i;
  daemon_entry_t e = {
    .length = 0,
    .type = daemon_string_vec
  };  
  for (char** pos=src;*pos!=NULL;++pos)
    e.length++;
  ret = write(fd, &e, sizeof(daemon_entry_t));
  if(ret!=sizeof(daemon_entry_t)) return -EINVAL;
  for (building_i=0; building_i<e.length; ++building_i) {
    ret = put_string(*(src+building_i), fd);
    if (ret < 0) {
        goto fail;
    }
  }
  ret = e.length;
  goto out;
fail:
    log_err("Failed to put string vec");
out:
  return ret;
}