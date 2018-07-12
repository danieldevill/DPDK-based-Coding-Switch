#ifndef MAIN_H
#define MAIN_H

static void net_encode(struct rte_mbuf *m, unsigned portid, kodoc_coder_t *encoder, kodoc_coder_t *decoder);
static void net_decode(struct rte_mbuf *m, unsigned portid, kodoc_coder_t *decoder);
static void net_recode(struct rte_mbuf *m, unsigned portid, kodoc_coder_t *encoder, kodoc_coder_t *decoder);
static void reset_encoder(kodoc_coder_t *encoder, kodoc_factory_t *encoder_factory);
static void reset_decoder(kodoc_coder_t *decoder);
static void coding_setup(void);

#endif