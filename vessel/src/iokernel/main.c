/*
 * main.c - initialization and main dataplane loop for the iokernel
 */
#include <sys/syscall.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <base/log.h>
#include <base/bitmap.h>
#include <base/time.h>
#include <base/stddef.h>

#include "utils/cpu_utils.h"
#include "utils/pthread_utils.h"
#include "defs.h"
#include "sched.h"

#define LOG_INTERVAL_US		(5000 * 1000)
struct iokernel_cfg cfg;
struct dataplane dp;
pid_t main_pid;

bool allowed_cores_supplied;
DEFINE_BITMAP(input_allowed_cores, NCPU);
void *global_cpu_state_map=NULL;
void __weak init_shutdown(int status)
{
	log_info("init: shutting down -> %s",
		 status == EXIT_SUCCESS ? "SUCCESS" : "FAILURE");
	exit(status);
}

struct init_entry {
	const char *name;
	int (*init)(void);
};

#define IOK_INITIALIZER(name) \
	{__cstr(name), &name ## _init}

/* iokernel subsystem initialization */
static const struct init_entry iok_init_handlers[] = {
	/* base */
	IOK_INITIALIZER(cxltp_probe),
	IOK_INITIALIZER(cpu_utils),
	IOK_INITIALIZER(time),

	/* general iokernel */
	IOK_INITIALIZER(ksched),
	IOK_INITIALIZER(sched),
	IOK_INITIALIZER(simple),
	IOK_INITIALIZER(ias),

	/* control plane */
	IOK_INITIALIZER(control),
	IOK_INITIALIZER(ctl_shm),

	/* data plane */
	IOK_INITIALIZER(dpdk),
	IOK_INITIALIZER(rx),
	IOK_INITIALIZER(tx),
	IOK_INITIALIZER(dp_clients),
	IOK_INITIALIZER(dpdk_late),
	IOK_INITIALIZER(hw_timestamp)
};

static int run_init_handlers(const char *phase, const struct init_entry *h,
		int nr)
{
	int i, ret;

	log_debug("entering '%s' init phase", phase);
	for (i = 0; i < nr; i++) {
		log_debug("init -> %s", h[i].name);
		ret = h[i].init();
		if (ret) {
			log_debug("failed, ret = %d", ret);
			return ret;
		}
	}
	log_init_done=true;
	return 0;
}
unsigned long dataplane_loop_times[10];
static uint64_t prev=0;
static uint64_t now=0;
static unsigned long ticks=0;
// static pthread_barrier_t dp_barrier;

/*
 * The main dataplane thread.
 */
void *dataplane_loop(void * arg)
{
	bool work_done;
#ifdef STATS
	uint64_t next_log_time = microtime();
#endif
//	pthread_barrier_wait(&dp_barrier);
//	dp_pin_thread(main_pid, syscall(SYS_gettid), sched_dp_core, sched_ctrl_core);
	log_info("main: lcore %u running dataplane. [Ctrl+C to quit]",
			sched_dp_core);
	prev=rdtsc();
	ticks=0;
	/* run until quit or killed */
	for (;;) {
		work_done = false;
		ticks++;
		/* adjust core assignments */
		if (ticks % 10000000 == 0) {
			now = rdtsc();
			prev = now;
			ticks = 0;
		}

		sched_poll();

		/* drain overflow completion queues */
		//work_done |= tx_drain_completions();

		/* send a burst of egress packets */
		//work_done |= tx_burst();

		/* process a batch of commands from runtimes */
		work_done |= commands_rx();
	
		/* handle control messages */
		if (!work_done)
			dp_clients_rx_control_lrpcs();

#ifdef STATS
		if (microtime() > next_log_time) {
			print_stats();
			next_log_time += LOG_INTERVAL_US;
		}
#endif
	}
}

void mainloop_dump(){
}

static void print_usage(void)
{
	printf("usage: POLICY [noht/core_list/nobw/mutualpair]\n");
	printf("\tsimple: the standard, basic scheduler policy\n");
	printf("\tias: a policy aware of CPU interference\n");
	printf("\tnuma: a policy aware of NUMA architectures\n");
}

int main(int argc, char *argv[])
{
	int i, ret;
	if (argc >= 2) {
		if (!strcmp(argv[1], "simple")) {
			sched_ops = &simple_ops;
		} else if (!strcmp(argv[1], "numa")) {
			sched_ops = &numa_ops;
		} else if (!strcmp(argv[1], "ias")) {
			sched_ops = &ias_ops;
		} else {
			print_usage();
			return -EINVAL;
		}
	} else {
		sched_ops = &simple_ops;
	}
	cfg.no_hw_qdel = true;
	for (i = 2; i < argc; i++) {
		if (!strcmp(argv[i], "noht")) {
			cfg.noht = true;
		} else if (!strcmp(argv[i], "nobw")) {
			cfg.nobw = true;
		} else if (!strcmp(argv[i], "no_hw_qdel")) {
			cfg.no_hw_qdel = true;
		} else if (!strcmp(argv[i], "selfpair")) {
			cfg.ias_prefer_selfpair = true;
		} else if (!strcmp(argv[i], "nicpci")) {
			if (i == argc - 1) {
				fprintf(stderr, "missing nicpci argument\n");
				return -EINVAL;
			}
			nic_pci_addr_str = argv[++i];
			ret = pci_str_to_addr(nic_pci_addr_str, &nic_pci_addr);
			if (ret) {
				log_err("invalid pci address: %s", nic_pci_addr_str);
				return -EINVAL;
			}
		} else if (!strcmp(argv[i], "bwlimit")) {
			if (i == argc - 1) {
				fprintf(stderr, "missing bwlimit argument\n");
				return -EINVAL;
			}
			cfg.ias_bw_limit = atof(argv[++i]);
			log_info("setting bwlimit to %.5f", cfg.ias_bw_limit);
		} else if (string_to_bitmap(argv[i], input_allowed_cores, NCPU)) {
			fprintf(stderr, "invalid cpu list: %s\n", argv[i]);
			fprintf(stderr, "example list: 0-24,26-48:2,49-255\n");
			return -EINVAL;
		} else {
			allowed_cores_supplied = true;
		}
	}
	// ACCESS_ONCE(main_pid) = syscall(SYS_gettid);
	// pthread_barrier_init(&dp_barrier, NULL, 2);
	// pthread_t dp_thread;
	// pthread_create(&dp_thread, NULL, dataplane_loop, NULL);
	char* node_id_str = getenv("VESSEL_CXL_NODE_ID");
	if(node_id_str == NULL) {
		cfg.cxl_node_id = 2;
	} else {
		sscanf(node_id_str, "%d", &cfg.cxl_node_id);
	}
	log_info("cxl_node_id: %d", cfg.cxl_node_id);

	ret = run_init_handlers("iokernel", iok_init_handlers,
			ARRAY_SIZE(iok_init_handlers));
	if (ret)
		return ret;
	//pthread_barrier_wait(&dp_barrier);
	//void * args = NULL;
	//pthread_join(dp_thread, &args);
	dataplane_loop(NULL);
	return 0;
}
