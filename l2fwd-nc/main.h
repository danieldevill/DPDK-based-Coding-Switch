#ifndef MAIN_H
#define MAIN_H

static void net_encode(struct rte_mbuf *m, unsigned portid, kodoc_coder_t *encoder, kodoc_coder_t *decoder);
static void net_decode(uint8_t* payload, unsigned portid, kodoc_coder_t *encoder, kodoc_coder_t *decoder);

#endif