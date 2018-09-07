#ifndef MAIN_H
#define MAIN_H

static void net_encode(kodoc_factory_t *encoder_factory);
static void net_decode(struct rte_mbuf *m, unsigned portid, kodoc_coder_t *decoder);
static void net_recode(kodoc_factory_t *encoder_factory);
static void reset_decoder(kodoc_coder_t *decoder);
static void coding_setup(void);
static void update_settings(void);
static int dst_mac_status(struct rte_mbuf *m, unsigned portid); //Chcks if the dst_addr exists in the MAC table. If it does not, then it adds it to the table. 1 if it exits, 2 if it exists and is coding capable, and 0 if not.
static int get_dst_port(struct rte_mbuf *m);

#endif