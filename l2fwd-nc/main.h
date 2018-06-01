#ifndef MAIN_H
#define MAIN_H

static void l2fwd_arp_reply(struct rte_mbuf* m, unsigned portid);
static int port_ip_lookup(uint32_t *trg_ptcl_addr, unsigned portid);

#endif