#include <stdio.h>

#include <client.h>
#include <rand.h>
#include <config.h>
#include <memcached_meta.h>

#include <base/cpu.h>
#include <base/log.h>
#include <base/assert.h>
#include <runtime/simple_sync.h>
#include <runtime/runtime.h>
#include <runtime/timer.h>
#include <runtime/tls/thread.h>

void* my_u64_gen (void*last) {
    static void* buf;
    if (__glibc_unlikely(buf==NULL)) {
        buf = malloc(sizeof(uint64_t));
    }
    struct client_tls *tls = (void*) get_uthread_specific();
    *((uint64_t*)buf) = tls->_u64_gen_func(last);
    return buf;
}
static inline void init_u64_gen_func(void* func) {
    struct client_tls *tls = (void*) get_uthread_specific();
    tls->_u64_gen_func = func;
}


void* my_u8_gen (void*last) {
    static void* buf;
    if (__glibc_unlikely(buf==NULL)) {
        buf = malloc(sizeof(uint8_t));
    }
    struct client_tls *tls = (void*) get_uthread_specific();

    *((uint8_t*)buf) = tls->_u8_gen_func(last);
    return buf;
}
static inline void init_u8_gen_func(void* func) {
    struct client_tls *tls = (void*) get_uthread_specific();
    tls->_u8_gen_func = func;
}

#define DEFINE_RSEQ(name, ele_size, req_num) \
    uint##ele_size##_t *name = malloc(sizeof(uint##ele_size##_t) * req_num); 

#define GEN_RSEQ(name, ele_size, req_num, func) \
    do { \
    init_u##ele_size##_gen_func(func); \
    gen_rand_seq(req_num, sizeof(uint##ele_size##_t), my_u##ele_size##_gen, name); \
    } while(0)


struct recv_worker_args {
    uint64_t req_num;
    try_read_func_t do_read;
    struct req* list;
    struct memcached_meta* m;
    barrier_t * barrier;
    int conn_type;
    void * conn;
    struct signal *sig;
};

struct req** create_reqs(
    uint64_t req_num,
    uint64_t client_num,
    target_start_gen_func_t target_start_gen_func,
    key_gen_func_t key_gen_func,
    opcode_gen_func_t op_gen_func,
    struct memcached_meta* m)
{
    struct req** out = (struct req**) malloc(sizeof(struct req*) * (client_num+1));    
    struct req** head = out;
    log_info("req_num: %lu, client_num: %lu", req_num, client_num);
    if (req_num % client_num != 0) {
        puts("req_num % client_num != 0");
        abort();
    }
    uint64_t req_num_per_client = req_num / client_num;
    // Gen Interval Seq
    DEFINE_RSEQ(target_start_list, 64, req_num);
    GEN_RSEQ(target_start_list, 64, req_num, target_start_gen_func);
    // Gen Key List
    DEFINE_RSEQ(key_list, 64, req_num);
    GEN_RSEQ(key_list, 64, req_num, key_gen_func);
    // Gen Op List
    DEFINE_RSEQ(op_list, 8, req_num);
    GEN_RSEQ(op_list, 8, req_num, op_gen_func);

    for (uint64_t c=0; c<client_num; c++) {
        void* packet_buf = create_packets(m, req_num_per_client);
        struct req* c_req_list = (struct req*) malloc(sizeof(struct req) * req_num_per_client);
        struct req* req = c_req_list;
        void* buf = packet_buf, *endbuf=0;
        assert(req_num_per_client<UINT32_MAX);
        for (size_t i=0; i<req_num_per_client; ++i) {
            size_t index = i*client_num+c;
            uint64_t key = key_list[index] % m->nvalues;
            uint8_t op   = op_list[index];

            req->target_start_tsc = target_start_list[index];
            req->packet = buf;
            switch(op) {
                case Set: endbuf = write_set_req(m, i, key, buf); break;
                case Get: endbuf = write_get_req(m, i, key, buf); break;
                default: abort();
            }
            req->len = (uint64_t)endbuf - (uint64_t)buf;
            buf = buf + packet_size(m);
            req ++;
        }
        *out = c_req_list;
        out++;
    }

    free(target_start_list);
    free(key_list);
    free(op_list);
    *out = NULL;
    return head;
}

static inline uint64_t u64_delta(uint64_t a, uint64_t b) {
    if(a>b) return a-b;
    else return b-a;
}

enum work_send_states {
    WAITING,
    SENDING
};

void client_work(
    uint64_t req_num,
    try_write_func_t try_write,
    try_read_func_t try_read,
    spinlock_t *l,
    struct req* list,
    struct memcached_meta* m,
    struct signal *sig,
    bool* is_end)
{
    struct client_tls *tls = (void*)get_uthread_specific();
    size_t sent = 0, recved = 0;
    struct req *q = list;
    int send_state=0, read_state=0;
    size_t to_write=0, write_idx=0;
    size_t to_read=0, read_idx=0;
    send_state = WAITING;
    log_info("%lx work_start", get_cur_fs());
    size_t my_packet_size = packet_size(m);
    void* read_buf = malloc(my_packet_size);
    uint32_t read_total_body_length;
    to_read = sizeof(struct packet_header);

    uint64_t update_window = clock * 1000 * 10;
    uint64_t cur_time, update_last_time = rdtsc();
    uint64_t start_time = rdtsc();

    while (recved < req_num) {
        cur_time = rdtsc();
        softirq_do_directpath();
        if (cur_time - update_last_time > update_window) {
            do_tcp_handle_timeouts(tls->thread_conn, cur_time / clock);
            // log_info_ratelimited("do_tcp_handle_timeouts");
            cur_time = rdtsc();
            update_last_time = cur_time;
        }
        // SEND
        switch(send_state) {
        case WAITING:
            if (sent==req_num) break;
            uint64_t target_start = q->target_start_tsc + start_time;
            if (target_start > cur_time)
                break;
            else if (u64_delta(target_start, cur_time) > 2*clock){
                q++;
                q->real_end_tsc = 0;
                recved++;
                break;
            } else {
                to_write = q->len; write_idx = 0;
                send_state = SENDING;
            }// GOTO SENDING
        case SENDING:
            ssize_t write_size = try_write(q->packet + write_idx, to_write);
            if (write_size < 0 && write_size != -105) {
                log_err("Write Error: %ld", write_size);
                abort();
            } else if (write_size > 0) {
                to_write  -= write_size;
                write_idx += write_size;
            }
            if (to_write == 0) {
                q->real_start_tsc = cur_time;
                sent++; q++;
                q->real_end_tsc = 0;
                send_state = WAITING;
            }
        break;
        default:
            log_err("UNKNOWN send_state");
            abort();
        }
        // RECV
        if ( to_read > 0 ) {
            ssize_t read_size = try_read(read_buf+read_idx, to_read);
            if (read_size > 0) {
                to_read  -= read_size;
                read_idx += read_size;
            } else if (read_size < 0) {
                log_err("try_read fail for %ld", read_size);
                abort();
            }
        }
        if (to_read == 0) {
            if (read_state == 0) {
                struct packet_header*ph = read_buf;
                read_total_body_length = ntohl(ph->total_body_length);
                if (ph->magic!=Response || ph->vbucket_id_or_status != htons(NoError)) {
                    log_info("Error respone! magic=%x status=%x", ph->magic, ntohs(ph->vbucket_id_or_status));
                    log_info("Recived %lu", recved);
                    abort();
                }
                if(read_total_body_length!=0) {
                    if (read_total_body_length>50) {
                        log_err("read_total_body_length>50");
                        abort();
                    }
                    to_read = read_total_body_length;
                    read_state = 1;
                } else {
                    uint32_t idx = read_opaque(read_buf);
                    list[idx].real_end_tsc = rdtsc();
                    to_read = sizeof(struct packet_header);
                    read_idx = 0;
                    recved++;
                }
            } else {
                uint32_t idx = read_opaque(read_buf);
                list[idx].real_end_tsc = rdtsc();
                to_read = sizeof(struct packet_header);
                read_idx = 0;
                recved++;
                read_state = 0;
            }
        }
    }
    int ret = atomic_fetch_and_add(&sig->cnt, 1);
    if (ret == sig->len - 1)  *is_end = true;
    else *is_end = false;
    log_info("%lx Done: %d", get_cur_fs(), ret);
    while (atomic_read(&sig->cnt) < sig->len) {
        softirq_do_directpath();
    }
}

void client_work_old(
    uint64_t req_per_client,
    try_write_func_t try_write,
    try_read_func_t try_read,
    spinlock_t *l,
    struct req* list,
    struct memcached_meta* m,
    bool* is_end)
{
    size_t sent = 0, recved = 0;
    struct req *q = list;

    uint64_t start_tcs = rdtsc();
    uint64_t real_target_start_tsc = start_tcs + q->target_start_tsc;
    uint64_t fail_cnt = 0;
    size_t to_write=0, write_idx=0;
    bool to_next = false;

    int read_state = 0;
    size_t my_packet_size = packet_size(m);
    void* read_buf = malloc(my_packet_size);
    uint32_t read_total_body_length;
    uint32_t read_idx=0, to_read = sizeof(struct packet_header); 

    q->real_end_tsc = 0;

    uint64_t state_cxl_full = 0;
    uint64_t state_time_miss = 0;
    uint64_t state_time_interv = 0;
    uint64_t state_time_total = rdtsc();
    // uint64_t state_write_0 = 0;
    uint64_t state_write_1 = 0;
    uint64_t state_write_1_max = 0;
    uint64_t state_recv_1 = 0;
    uint64_t state_recv_2 = 0;
    uint64_t state_recv_1_max = 0;
    uint64_t state_recv_2_max = 0;
    uint64_t state_wr_sum = 0;
    uint64_t state_last_tsc = 0;
    uint64_t state_last_acc_tsc = 0;

    while(recved < req_per_client) {
        // Send
        uint64_t cur_tsc = rdtsc();
        softirq_do_directpath();

        state_last_acc_tsc = cur_tsc - state_last_tsc;
        state_recv_1 -= state_last_tsc;
        state_recv_2 -= state_last_tsc;
        state_write_1 -= state_last_tsc;
        state_last_tsc = cur_tsc;

        if (sent < req_per_client) {
            if (to_write) {
                ssize_t write_size = try_write(q->packet+write_idx, to_write);
                if (write_size <= 0) {
                    if (write_size < 0) {
                        log_info("try_write fail for %ld", write_size);
                        return;
                    }
                    goto wait_write_again;
                }
                to_write  -= write_size;
                write_idx += write_size;
                if (to_write==0) to_next=true;
            }
            if(!to_write && to_next==true) {
                sent++;
                q++;
                if (sent<req_per_client) {
                    real_target_start_tsc = start_tcs + q->target_start_tsc;
                    q->real_end_tsc = 0;
                } else {
                    real_target_start_tsc = 0;
                }
                to_next = false;
            }
            if (!to_write && cur_tsc >= real_target_start_tsc && real_target_start_tsc != 0) {
                if (u64_delta(real_target_start_tsc, cur_tsc) <= 2*clock) { 
                    to_write = q->len;
                    write_idx = 0;
                    q->real_start_tsc = cur_tsc;
                } else {
                    if (real_target_start_tsc != 0) {
                        recved++;
                        fail_cnt++;
                        state_time_miss += cur_tsc - real_target_start_tsc;
                        state_time_interv += (q)->target_start_tsc - (q-1)->target_start_tsc;
                        state_recv_1_max = state_recv_1 > state_recv_1_max ? state_recv_1 : state_recv_1_max;
                        state_recv_2_max = state_recv_2 > state_recv_2_max ? state_recv_2 : state_recv_2_max;
                        state_write_1_max = state_write_1 > state_write_1_max ? state_write_1 : state_write_1_max;
                        state_wr_sum += (state_last_acc_tsc);
                        to_next = true;
                    }
                }
            }
        }

wait_write_again:
        state_write_1 = rdtsc();
        // Rev
        ssize_t read_size = try_read(read_buf+read_idx, to_read);
        state_recv_2 = rdtsc();
        if (read_size>0) {
            if (read_state == 0) {
                if (unlikely(read_size != sizeof(struct packet_header))) {
                    printf("client_work -> read_size(%ld) != sizeof(struct packet_header)\n", read_size);
                    abort();
                } else {
                    struct packet_header*ph = read_buf;
                    read_total_body_length = ntohl(ph->total_body_length);
                    if(read_total_body_length!=0) {
                        read_idx = sizeof(struct packet_header);
                        to_read = read_total_body_length;
                        read_state = 1;
                    } else {
                        uint32_t idx = read_opaque(read_buf);
                        list[idx].real_end_tsc = rdtsc();
                        recved++;
                    }
                    if (unlikely(ph->magic!=Response || ph->vbucket_id_or_status != htons(NoError))) {
                        printf("Error respone! magic=%01x status=%02x\n", ph->magic, ntohs(ph->vbucket_id_or_status));
                        abort();
                    }
                }
            } else if(read_state==1) {
                if (unlikely(read_size==-11)) goto recv_end;
                if (unlikely(read_size != read_total_body_length)) {
                    printf("read_size != read_total_body_length\n");
                    abort();
                }
                uint32_t idx = read_opaque(read_buf);
                list[idx].real_end_tsc = rdtsc();
                recved++;
                to_read = sizeof(struct packet_header);
                read_idx = 0;
                read_state = 0;
            } else {abort();}
        } else if (unlikely(read_size == -103)) {
            printf("conn closed.\n");
            abort();
        }
recv_end:
        state_recv_1 = rdtsc();
    }
    state_time_total = rdtsc() - state_time_total;
    fail_cnt = (fail_cnt==0?1:fail_cnt);
    printf("fail_cnt: %lu/%lu state_cxl_full: %lu state_time_miss: %lu state_time_interv: %lu state_time_avg: %lu"
            " state_recv_1_max: %lu state_recv_2_max: %lu state_write_1_max: %lu  state_wr_sum %lu\n",
        fail_cnt, req_per_client,
        state_cxl_full,
        state_time_miss/fail_cnt,
        state_time_interv/fail_cnt,
        state_time_total/req_per_client,
        state_recv_1_max,
        state_recv_2_max,
        state_write_1_max,
        state_wr_sum/fail_cnt);
}

void work_send(
    uint64_t req_num,
    try_write_func_t do_write,
    struct req* list,
    struct memcached_meta* m)
{
    size_t sent = 0;
    struct req *q = list;

    uint64_t start_tcs = rdtsc();
    uint64_t real_target_start_tsc = start_tcs + q->target_start_tsc;
    size_t to_write=0, write_idx=0;
    while (sent < req_num) {
        uint64_t cur_tsc = rdtsc();
        if (cur_tsc >= real_target_start_tsc && real_target_start_tsc != 0) {
            if (u64_delta(real_target_start_tsc, cur_tsc) <= 2*clock) {
                q->real_start_tsc = cur_tsc;
                to_write = q->len;
                write_idx = 0;
                while (to_write) {
                    ssize_t write_size = do_write(q->packet+write_idx, to_write);
                    if (write_size<0) {
                        printf("Can not write\n");
                        abort();
                    }
                    to_write -= write_size;
                    write_idx += write_size;
                }
            } else {
                q->real_end_tsc = 0;
            }
            q++; sent++;
            if (sent%100==0) {
                log_info("sent: %ld", sent);
            }
            if (sent==req_num) {
                real_target_start_tsc = 0;
            } else {
                real_target_start_tsc = start_tcs + q->target_start_tsc;
            }
        }
    }
}

void work_recv(
    uint64_t req_num,
    try_read_func_t do_read,
    struct req* list,
    struct memcached_meta* m)
{
    size_t recv = 0;
    size_t my_packet_size = packet_size(m);
    void* read_buf = malloc(my_packet_size);

    while (recv < req_num) {
        ssize_t to_read = sizeof(struct packet_header), read_idx = 0;
        while(to_read) {
            ssize_t read_size = do_read(read_buf+read_idx, to_read);
            if(read_size < 0 &&
                    read_size != -104 && read_size != -103) {
                log_err("read_fail");
                return;
            } else if (read_size > 0) {
                to_read  -= read_size;
                read_idx += read_size;
            } else {
                return;
            }
        }
        struct packet_header*ph = read_buf;
        if (ph->magic!=Response || ph->vbucket_id_or_status != htons(NoError)) {
            printf("Error respone! magic=%x status=%x\n", ph->magic, ntohs(ph->vbucket_id_or_status));
            printf("Recived %lu\n", recv);
            abort();
        }
        uint32_t read_total_body_length = ntohl(ph->total_body_length);
        if (read_total_body_length) {
            to_read = read_total_body_length, read_idx = 0;
            while(to_read) {
                ssize_t read_size = do_read(read_buf+read_idx, to_read);
                if(read_size < 0 &&
                    read_size != -104 && read_size != -103) {
                    log_err("read_fail");
                    return;
                } else if (read_size > 0) {
                    to_read  -= read_size;
                    read_idx += read_size;
                } else {
                    return;
                }
            }
        }
        uint32_t idx = read_opaque(read_buf);
        list[idx].real_end_tsc = rdtsc();
        recv++;
        if (recv%100==0) {
            log_info("sent: %ld", recv);
        }
    }
}
void work_recv_thread(void *a) {
    struct recv_worker_args *args = a;
    struct client_tls *tls = malloc(sizeof(*tls));
    set_uthread_specific((uint64_t) tls);
    tls->thread_conn_try_write_op = utils_get_try_write(args->conn_type);
    tls->thread_conn_try_read_op = utils_get_try_read(args->conn_type);
    tls->thread_conn = args->conn;

    work_recv(
        args->req_num,
        args->do_read,
        args->list,
        args->m
    );
    barrier_wait(args->barrier);
}


void preload(
    uint64_t req_num,
    try_write_func_t try_write,
    try_read_func_t try_read,
    struct req* list,
    struct memcached_meta* m,
    struct signal *sig,
    bool *is_end)
{
    size_t sent = 0, recved = 0;
    struct req *q = list;
    size_t my_packet_size = packet_size(m);
    void* read_buf = malloc(my_packet_size);
    int send_recv_state=0;
    int read_state=0;
    uint32_t read_total_body_length;
    uint32_t read_buf_idx=0, to_read = sizeof(struct packet_header);
    size_t to_write=q->len, write_idx=0;
    size_t update_window = clock * 1000 * 10;
    struct client_tls *tls = (void*)get_uthread_specific();

    uint64_t cur_time, update_last_time = rdtsc();
    cur_time = update_last_time;

    while(recved < req_num) {
        cur_time = rdtsc();
        softirq_do_directpath();
        if (cur_time - update_last_time > update_window) {
            do_tcp_handle_timeouts(tls->thread_conn, cur_time / clock);
            // log_info_ratelimited("do_tcp_handle_timeouts");
            cur_time = rdtsc();
            update_last_time = cur_time;
        }
        // Send
        if (send_recv_state == 0) {
            if (sent < req_num) {
                if (to_write) {
                ssize_t write_size = try_write(q->packet+write_idx, to_write);
                if (write_size <= 0) {
                        if (write_size < 0 && write_size != -105) {
                            log_err("try_write fail for %ld", write_size);
                            abort();
                        }
                        continue;
                    }
                    to_write -=write_size;
                    write_idx+=write_size;
                }
                if (!to_write) {
                    struct packet_header*ph = q->packet;
                    if (ph->opcode!=Set) {
                        printf("ph->opcode!=Set\n");
                        abort();
                    }
                    sent++;
                    q++;
                    if (sent<req_num) {
                        to_write = q->len;
                        write_idx = 0;
                    }
                    send_recv_state = 1;
                }
            }
        } else {
            if ( to_read > 0 ) {
                ssize_t read_size = try_read(read_buf+read_buf_idx, to_read);
                if (read_size>0) {
                    to_read -= read_size;
                    read_buf_idx += read_size;
                } else if (read_size < 0) {
                    log_err("try_read fail for %d", to_read);
                    abort();
                }
            }
            if ( to_read == 0 ) {
                if (read_state==0) {
                    struct packet_header*ph = read_buf;
                    read_total_body_length = ntohl(ph->total_body_length);
                    if (ph->magic!=Response || ph->vbucket_id_or_status != htons(NoError)) {
                        log_info("Error respone! magic=%x status=%x", ph->magic, ntohs(ph->vbucket_id_or_status));
                        log_info("Recived %lu", recved);
                        abort();
                    }
                    if(read_total_body_length!=0) {
                        to_read = read_total_body_length;
                        read_state = 1;
                    } else {
                        to_read = sizeof(struct packet_header);
                        read_buf_idx = 0;
                        recved++;
                        send_recv_state = 0;
                    }
                } else {
                    to_read = sizeof(struct packet_header);
                    read_buf_idx = 0;
                    recved++;
                    read_state = 0;
                    send_recv_state = 0;
                }
            }
        }
    }

    int ret = atomic_fetch_and_add(&sig->cnt, 1);
    if (ret == sig->len - 1)  *is_end = true;
    else *is_end = false;
    log_info("%lx Done: %d", get_cur_fs(), ret);
    while (atomic_read(&sig->cnt) < sig->len) {
        softirq_do_directpath();
    }
}

void preload_send(
    uint64_t req_num,
    try_write_func_t do_write,
    struct req* list,
    struct memcached_meta* m)
{
    size_t sent = 0;
    struct req *q = list;

    while (sent < req_num) {
        size_t to_write = q->len, write_idx = 0;
        while (to_write) {
            ssize_t write_size = do_write(q->packet + write_idx, to_write);
            if(write_size < 0 &&
                    write_size != -104 && write_size != -103) {
                log_err("write_fail %lu@%p", q->len, q->packet);
                return;
            } else if (write_size > 0) {
                to_write  -= write_size;
                write_idx += write_size;
            } else {
                log_err("104 103");
                return;
            }
        }
        sent++; q++;
        if (sent%100000==0) {
            log_info("sent: %ld", sent);
        }
    }
    return;
}

void preload_recv(
    uint64_t req_num,
    try_read_func_t do_read,
    struct req* list,
    struct memcached_meta* m)
{
    size_t recv = 0;
    size_t my_packet_size = packet_size(m);
    void* read_buf = malloc(my_packet_size);
    while (recv < req_num) {
        size_t to_read = sizeof(struct packet_header), read_idx = 0;
        while (to_read) {
            ssize_t read_size = do_read(read_buf + read_idx, to_read);
            if(read_size < 0 &&
                    read_size != -104 && read_size != -103) {
                log_err("read_fail");
                return;
            } else if(read_size > 0) {
                to_read  -= read_size;
                read_idx += read_size;
            } else {
                log_err("104 103");
                return;
            }
        }

        struct packet_header*ph = read_buf;
        if (ph->magic!=Response || ph->vbucket_id_or_status != htons(NoError)) {
            log_err("Error respone! magic=%x status=%x\n", ph->magic, ntohs(ph->vbucket_id_or_status));
            log_err("Recived %lu\n", recv);
            return;
        }
        to_read = ntohl(ph->total_body_length), read_idx = 0;
        while (to_read) {
            ssize_t read_size = do_read(read_buf+sizeof(struct packet_header), to_read);
            if(read_size < 0 &&
                    read_size != -104 && read_size != -103) {
                log_err("read_fail");
                return;
            } else if(read_size > 0) {
                to_read  -= read_size;
                read_idx += read_size;
            } else {
                log_err("104 103");
                return;
            }
        }
        recv++;
    }
}

void preload_recv_thread(void *a) {
    struct recv_worker_args *args = a;
    struct client_tls *tls = malloc(sizeof(*tls));
    set_uthread_specific((uint64_t) tls);
    tls->thread_conn_try_write_op = utils_get_try_write(args->conn_type);
    tls->thread_conn_try_read_op = utils_get_try_read(args->conn_type);
    tls->thread_conn = args->conn;
    log_info("preload_recv_thread");
    preload_recv(
        args->req_num,
        args->do_read,
        args->list,
        args->m
    );
    static atomic_t cnt;
    atomic_inc(&cnt);
    log_info("recv: %d", atomic_read(&cnt));
    barrier_wait(args->barrier);
}

static ssize_t client_try_write(void* buf, size_t len) {
    struct client_tls *tls = (void*) get_uthread_specific();
    return tls->thread_conn_try_write_op(tls->thread_conn, buf, len);
}
static ssize_t client_try_read(void* buf, size_t len) {
    struct client_tls *tls = (void*) get_uthread_specific();
    return tls->thread_conn_try_read_op(tls->thread_conn, buf, len);
}



void client_thread(void* args) {
    int ret;
    struct client *myargs = args;
    struct client_tls *tls = malloc(sizeof(*tls));
    struct signal *start_sig, *end_sig;
    bool is_start, is_end = false;
    start_sig = myargs->start_sig;
    end_sig = myargs->end_sig;
    set_uthread_specific((uint64_t) tls);
    spin_lock_init(&myargs->l);
//Preload-Connect
    ret = utils_connect(myargs->conn_type, myargs->conn_args, &tls->thread_conn);
    if (ret) {
        printf("Failed to utils_connect\n");
        abort();
    }
    tls->thread_conn_try_write_op = utils_get_try_write(myargs->conn_type);
    tls->thread_conn_try_read_op = utils_get_try_read(myargs->conn_type);
    if (tls->thread_conn_try_write_op == NULL ||
        tls->thread_conn_try_read_op == NULL)
    {
        printf("Fail to get write or read op.\n");
        return;
    }
    barrier_wait(myargs->start_barrier);

    unlock_directpath();
    static atomic_t cnt;
    static atomic_t start_cnt;
    atomic_inc(&start_cnt);
    is_start = signal_wait(start_sig);

//Preload-Do-Preload
    if (myargs->async) {
        barrier_t preload_barrier;
        barrier_init(&preload_barrier, 2);
        struct recv_worker_args preload_args = {
            .req_num =  myargs->preload_req_num,
            .do_read =  client_try_read,
            .list    =  myargs->preload_list,
            .m       =  myargs->meta,
            .barrier = &preload_barrier,
            .conn_type = myargs->conn_type,
            .conn = tls->thread_conn
        };
        thread_spawn(preload_recv_thread, &preload_args);
        preload_send(myargs->preload_req_num, client_try_write, myargs->preload_list, myargs->meta);
        atomic_inc(&cnt);
        log_info("before preload_barrier: %d", atomic_read(&cnt));
        barrier_wait(&preload_barrier);
    } else {
        preload(myargs->preload_req_num, client_try_write, client_try_read, myargs->preload_list, myargs->meta,
                end_sig, &is_end);
    }
    utils_close_connect(myargs->conn_type, tls->thread_conn);

    if (is_start) signal_init(start_sig, myargs->client_num);
    ret = barrier_wait(myargs->preload_barrier);

    if (ret==true) {
        log_info("Preload done\n");
        signal_init(end_sig, myargs->client_num);
    }

//Do work
    ret = utils_connect(myargs->conn_type, myargs->conn_args, &tls->thread_conn);
    if (ret) {
        log_err("Failed to utils_connect\n");
        abort();
    }

    is_start = signal_wait(start_sig);
//Warm up
    if (myargs->async) {
        barrier_t warmup_barrier;
        barrier_init(&warmup_barrier, 2);
        struct recv_worker_args warmup_args = {
            .req_num =  myargs->warm_up_req_num,
            .do_read =  client_try_read,
            .list    =  myargs->warm_up_list,
            .m       =  myargs->meta,
            .barrier = &warmup_barrier,
            .conn_type = myargs->conn_type,
            .conn = tls->thread_conn
        };
        thread_spawn(work_recv_thread, &warmup_args);
        work_send(myargs->warm_up_req_num, client_try_write, myargs->warm_up_list, myargs->meta);
        barrier_wait(&warmup_barrier);
    } else {
        client_work(
            myargs->warm_up_req_num,
            client_try_write,
            client_try_read,
            &myargs->l,
            myargs->warm_up_list,
            myargs->meta,
            end_sig,
            &is_end);
    }
    if (is_start) signal_init(start_sig, myargs->client_num);
    ret = barrier_wait(myargs->warm_up_barrier);
    if (ret==true) {
        printf("Warm up done\n");
        signal_init(end_sig, myargs->client_num);
    }

// Work
    ret = barrier_wait(myargs->begin_barrier);
    if (ret) *(myargs->work_start_p) = rdtsc();

    is_start = signal_wait(start_sig);

    if (myargs->async) {
        barrier_t work_barrier;
        barrier_init(&work_barrier, 2);
        struct recv_worker_args work_args = {
            .req_num =  myargs->req_num,
            .do_read =  client_try_read,
            .list    =  myargs->warm_up_list,
            .m       =  myargs->meta,
            .barrier = &work_barrier,
            .conn_type = myargs->conn_type,
            .conn = tls->thread_conn
        };
        thread_spawn(work_recv_thread, &work_args);
        work_send(myargs->preload_req_num, client_try_write, myargs->warm_up_list, myargs->meta);
        barrier_wait(&work_barrier);
    } else {
        client_work(
            myargs->req_num,
            client_try_write,
            client_try_read,
            &myargs->l,
            myargs->list,
            myargs->meta,
            end_sig,
            &is_end);
    }
    utils_close_connect(myargs->conn_type, tls->thread_conn);
    barrier_wait(myargs->end_barrier);
    return;
}
