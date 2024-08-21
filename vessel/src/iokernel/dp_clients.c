/*
 * dp_clients.c - functions for registering/unregistering dataplane clients
 */

#include <unistd.h>


#include <base/log.h>
#include <base/lrpc.h>

#include "defs.h"
#include "sched.h"
#include "ksched.h"

#define MAC_TO_PROC_ENTRIES	128

static struct lrpc_chan_out lrpc_data_to_control;
static struct lrpc_chan_in lrpc_control_to_data;

/*
 * Add a new client.
 */
static void dp_clients_add_client(struct proc *p)
{
	if (!sched_attach_proc(p)) {
		p->kill = false;
		dp.clients[dp.nr_clients++] = p;
	} else {
		log_err("dp_clients: failed to attach proc.");
		p->attach_fail = true;
		proc_put(p);
		return;
	}
}

void proc_release(struct ref *r)
{
	ssize_t ret;

	struct proc *p = container_of(r, struct proc, ref);
	if (!lrpc_send(&lrpc_data_to_control, CONTROL_PLANE_REMOVE_CLIENT,
			(unsigned long) p))
		log_err("dp_clients: failed to inform control of client removal");
	ret = write(data_to_control_efd, &(uint64_t){ 1 }, sizeof(uint64_t));
	WARN_ON(ret != sizeof(uint64_t));
}

/*
 * Remove a client. Notify control plane once removal is complete so that it
 * can delete its data structures.
 */
static void dp_clients_remove_client(struct proc *p)
{
	int i;

	for (i = 0; i < dp.nr_clients; i++) {
		if (dp.clients[i] == p)
			break;
	}

	if (i == dp.nr_clients) {
		WARN();
		return;
	}

	dp.clients[i] = dp.clients[dp.nr_clients - 1];
	dp.nr_clients--;

	/* TODO: free queued packets/commands? */

	/* release cores assigned to this runtime */
	p->kill = true;
	sched_detach_proc(p);
	proc_put(p);
}

/*
 * Process a batch of messages from the control plane.
 */
void dp_clients_rx_control_lrpcs(void)
{
	uint64_t cmd;
	unsigned long payload;
	uint16_t n_rx = 0;
	struct proc *p;
	int core_id, tmp;

	while (n_rx < IOKERNEL_CONTROL_BURST_SIZE &&
			lrpc_recv(&lrpc_control_to_data, &cmd, &payload)) {
		p = (struct proc *) payload;

		switch (cmd)
		{
		case DATAPLANE_ADD_CLIENT:
			
			dp_clients_add_client(p);
	
			sched_for_each_allowed_core(core_id, tmp) {
				set_ops(core_id);
			}
			break;
		case DATAPLANE_REMOVE_CLIENT:
			dp_clients_remove_client(p);
			break;
		default:
			log_err("dp_clients: received unrecognized command %lu", cmd);
		}

		n_rx++;
	}
}

/*
 * Initialize channels for communicating with the I/O kernel control plane.
 */
int dp_clients_init(void)
{
	int ret;

	ret = lrpc_init_in(&lrpc_control_to_data,
			lrpc_control_to_data_params.buffer, CONTROL_DATAPLANE_QUEUE_SIZE,
			lrpc_control_to_data_params.wb);
	if (ret < 0) {
		log_err("dp_clients: initializing LRPC from control plane failed");
		return -1;
	}

	ret = lrpc_init_out(&lrpc_data_to_control,
			lrpc_data_to_control_params.buffer, CONTROL_DATAPLANE_QUEUE_SIZE,
			lrpc_data_to_control_params.wb);
	if (ret < 0) {
		log_err("dp_clients: initializing LRPC to control plane failed");
		return -1;
	}

	dp.nr_clients = 0;

	return 0;
}
