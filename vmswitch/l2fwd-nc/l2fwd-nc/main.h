#ifndef MAIN_H
#define MAIN_H

static void net_encode(kodoc_factory_t *encoder_factory);
static void net_decode(struct rte_mbuf *m, unsigned portid, kodoc_coder_t *decoder);
static void net_recode(kodoc_factory_t *encoder_factory);
static void reset_decoder(kodoc_coder_t *decoder);
static void coding_setup(void);
static void update_settings(void);

#endif