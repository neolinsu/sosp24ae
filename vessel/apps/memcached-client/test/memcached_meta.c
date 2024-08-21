#include <stdio.h>
#include <stdlib.h>
#include <net_utils.h>
#include <memcached_meta.h>
#include <mt19937_64.h>


#include <base/assert.h>

int main () {
    // Get
    int ret;
    struct memcached_meta *m;
    struct packet_header *ph;
    void * buf;
    buf = malloc(sizeof(struct memcached_meta));
    memset(buf, -1, sizeof(struct memcached_meta));
    m = buf;
    ret = memcached_meta_init(buf,
        2,
        20,
        10000);
    assert(ret==0);
    assert(m->ph.magic=Request);
    assert(m->ph.key_length=htons(20));
    buf = malloc(sizeof(struct memcached_meta) + 20);
    ph = buf;
    buf = write_get_req(m, 0, 1, buf);
    assert(buf==((void*)ph) + sizeof(*ph) + 20);
    assert(ph->key_length==htons(20));
    assert(ph->extras_length==0);
    assert(ph->total_body_length==0);
    buf--;
    for (;buf>(void*)ph+sizeof(*ph); buf--) {
        assert(*((char*)buf)=='A');
    }
    assert(*((char*)buf)=='1');
    free(ph);
    //Set
    buf = malloc(sizeof(struct memcached_meta) + 22);
    ph = buf;
    buf = write_set_req(m, 0, 121212121, buf);
    assert(buf==((void*)ph) + sizeof(*ph) + 30);
    assert(ph->key_length==htons(20));
    assert(ph->extras_length==8);
    assert(ph->total_body_length==0);
    buf--;
    assert(*((char*)buf)=='l'); buf--;
    assert(*((char*)buf)==0); buf--;
    for (;buf>(void*)ph+sizeof(*ph); buf--) {
    }
    assert(*((char*)buf)==0);
    puts("SUCCESS");
    return 0;
}
