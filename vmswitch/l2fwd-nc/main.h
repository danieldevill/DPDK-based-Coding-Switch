#ifndef MAIN_H
#define MAIN_H

static void net_encode(kodoc_factory_t *encoder_factory);
static void net_decode(kodoc_factory_t *decoder_factory);
static void net_recode(kodoc_factory_t *encoder_factory);
static void update_settings(void);
static void generate_generationID(char *generationID);
static struct dst_addr_status dst_mac_status(struct rte_mbuf *m, unsigned srcport); //Chcks if the dst_addr exists in the MAC table. If it does not, then it adds it to the table. 1 if it exits, 2 if it exists and is coding capable, and 0 if not.
static void genID_in_genTable(char *generationID);

#endif