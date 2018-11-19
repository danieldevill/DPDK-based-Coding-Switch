#ifndef MAIN_H
#define MAIN_H

static void net_encode(kodoc_factory_t *encoder_factory);
static void net_decode(kodoc_factory_t *decoder_factory);
static void net_recode(kodoc_factory_t *encoder_factory);
static void update_settings(void);
static struct dst_addr_status dst_mac_status(struct rte_mbuf *m, unsigned srcport);
static void add_mac_addr(struct ether_addr addr, unsigned srcport, unsigned coding_capable);
static struct rte_ring* genID_in_genTable(char *generationID);

#endif