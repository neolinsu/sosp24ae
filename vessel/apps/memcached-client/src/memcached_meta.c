#include <limits.h>
#include <memcached_meta.h>

#include <base/assert.h>

int memcached_meta_init (
    struct memcached_meta* meta,
    uint64_t value_size,
    uint64_t key_size,
    uint64_t nvalues)
{
    if (!meta) return EINVAL;
    if (key_size>UINT16_MAX) return ERANGE;
    meta->value_size = value_size;
    meta->key_size = key_size;
    assert(key_size < UINT16_MAX);
    meta->nvalues = nvalues;
    meta->get_req_total_length = key_size;
    meta->set_req_total_length = 8 + key_size + value_size;
    // toN Ready
    meta->ph.magic = Request;
    meta->ph.key_length = htons((uint16_t)key_size);
    meta->ph.data_type = 0;
    meta->ph.vbucket_id_or_status = 0;
    meta->ph.cas = 0;
    return 0;
}
