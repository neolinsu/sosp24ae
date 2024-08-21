#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
//TODO
#include <stdio.h>

#include "net_utils.h"


enum memcached_opcode
{
    Get = 0x00,
    Set = 0x01,
    Add = 0x02,
    Replace = 0x03,
    Delete = 0x04,
    Increment = 0x05,
    Decrement = 0x06,
    Flush = 0x08,
    Noop = 0x0a,
    Version = 0x0b,
    GetKQ = 0x0d,
    Append = 0x0e,
    Prepend = 0x0f,
    Touch = 0x1c,
};

enum magic {
    Request = 0x80,
    Response = 0x81,
};

enum response_status {
    NoError = 0x00,
    KeyNotFound = 0x01,
    KeyExists = 0x02,
    ValueTooLarge = 0x03,
    InvalidArguments = 0x04,
};

struct packet_header {
    uint8_t  magic;
    uint8_t  opcode;
    uint16_t key_length;
    uint8_t  extras_length;
    uint8_t  data_type;
    uint16_t vbucket_id_or_status;
    uint32_t total_body_length;
    uint32_t opaque;
    uint64_t cas;
}__attribute__((packed));

_Static_assert(sizeof(struct packet_header)==24,
    "Sizeof struct packet_header should be 24 B");


struct memcached_meta {
    uint64_t value_size; // Value size
    uint64_t key_size;   // Key size
    uint64_t nvalues;
    struct packet_header ph;
    uint64_t set_req_total_length;
    uint64_t get_req_total_length;
};


int memcached_meta_init(struct memcached_meta* meta,
    uint64_t value_size,
    uint64_t key_size,
    uint64_t nvalues);

static inline size_t packet_size(struct memcached_meta* m) {
    return sizeof(struct packet_header) + 8 + m->value_size + m->key_size;
}

static inline void* create_packets(
    struct memcached_meta* m,
    uint64_t req_num)
{
    size_t ele_size = sizeof(struct packet_header) + 8 + m->value_size + m->key_size;
    return malloc(ele_size * req_num);
}
/**
 * Write key to buf with pad, only if legnth of key as Oct Str is less than key_size.
 */
static inline void* write_key(void *buf, uint64_t key, uint64_t key_size) {
    char * index = buf;
    uint64_t k = key;
    size_t done = 0;
    while(true) {
        *index = 48 + (uint8_t) (k%10); index++;
        k /= 10;
        done++;
        if (k==0) break;
    }
    for (; done < key_size; done++) {
        *index = 'A'; index++;
    }
    return index;
}

static inline void* write_value(void *buf, uint64_t key, uint64_t value_size) {
    char * index = buf;
    for (uint64_t i=0; i < value_size; i++) {
        *index = (char) (( (key*i) >> (i%4) ) &0xff);
        index++;
    }
    return index;
}

static inline void* write_ph(struct memcached_meta *m,
    uint32_t opaque,
    uint8_t opcode,
    uint16_t key_size,
    uint8_t extras_length,
    uint32_t total_body_length,
    void *buf)
{
    memcpy(buf, &(m->ph), sizeof(m->ph));
    struct packet_header* ph = buf;
    ph->opcode = opcode;
    ph->opaque = htonl(opaque);
    ph->key_length = htons(key_size);
    ph->extras_length = extras_length;
    ph->total_body_length = htonl(total_body_length);
    return buf + sizeof(m->ph);
}

static inline void* write_set_req(struct memcached_meta *m, uint32_t opaque, uint64_t key, void *buf) {
    buf = write_ph(m, opaque, Set, m->key_size, 8, m->set_req_total_length, buf);
    *((uint64_t*)buf) = 0;
    buf += sizeof(uint64_t);
    buf = write_key(buf, key, m->key_size);
    buf = write_value(buf, key, m->value_size);
    return buf;
}
static inline void* write_get_req(struct memcached_meta *m, uint32_t opaque, uint64_t key, void* buf) {
    buf = write_ph(m, opaque, Get, m->key_size, 0, m->get_req_total_length, buf);
    buf = write_key(buf, key, m->key_size);
    return buf;
}

static inline uint32_t read_opaque(void* buf) {
    struct packet_header *p = buf;
    return ntohl(p->opaque);
}