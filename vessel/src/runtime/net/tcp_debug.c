/*
 * tcp_debug.c - prints TCP debug information
 */

#include <string.h>

#include <base/stddef.h>
#include <base/log.h>
#include <base/assert.h>

#include "tcp.h"

#if defined(DEBUG)
#define TCP_FLAG_STR_LEN 25



/* prints an outgoing TCP packet */
void tcp_debug_egress_pkt(tcpconn_t *c, struct mbuf *m)
{
	// list_for_each(&tcp_conns, temp, global_link) {
	// 	assert(c->rxq.n.next!=NULL);
	// 	assert(c->rxq.n.prev!=NULL);
	// }
// 	tcp_dump_pkt(c, (struct tcp_hdr *)mbuf_data(m),
// 		     mbuf_length(m) - sizeof(struct tcp_hdr), true);
}

/* prints an incoming TCP packet */
void tcp_debug_ingress_pkt(tcpconn_t *c, struct mbuf *m)
{
	// list_for_each(&tcp_conns, temp, global_link) {
	// 	assert(c->rxq.n.next!=NULL);
	// 	assert(c->rxq.n.prev!=NULL);
	// }
	// tcp_dump_pkt(c, (struct tcp_hdr *)mbuf_transport_offset(m),
	// 	     mbuf_length(m), false);
}

static const char *state_names[] = {
	"SYN-SENT",
	"SYN-RECEIVED",
	"ESTABLISHED",
	"FIN-WAIT1",
	"FIN-WAIT2",
	"CLOSE-WAIT",
	"CLOSING",
	"LAST-ACK",
	"TIME-WAIT",
	"CLOSED",
};

/* prints a TCP state change */
void tcp_debug_state_change(tcpconn_t *c, int last, int next)
{
	
	if (last == TCP_STATE_CLOSED) {
		log_debug("tcp: %p CREATE -> %s", c, state_names[next]);
	} else {
		log_debug("tcp: %p %s -> %s", c, state_names[last],
			  state_names[next]);
	}
}
#endif
