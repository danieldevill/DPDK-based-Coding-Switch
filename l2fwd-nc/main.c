/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//Modified by Daniel de Villiers, 2018.
//Additions marked by DD. Also GIT will dictate!
// Also my comments are "//" vs /* */ of dpdk (Linux).
//DD. I've "butchered" all  statistics code, just to clean.  

//Includes for Kodo-c library
#include <time.h>
#include <kodoc/kodoc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

//Added by DD.
#include "main.h"

//<VLAN,MAC,Type,port,coding_capable> table. Simular to CISCO switches? Maybe this will help in the future somewhere..
//Ive added a new idea where the mac table has a coding capable field.
#define MAC_ENTRIES 20
#define STATIC 0
#define DYNAMIC 1
static unsigned mac_counter = 0;
struct mac_table_entry {
	unsigned vlan;
	struct ether_addr d_addr;
	unsigned type;
	unsigned port;
	unsigned coding_capable;
};
struct mac_table_entry mac_fwd_table[MAC_ENTRIES]; 

//Other defines by DD
#define HW_TYPE_ETHERNET 0x0001
static uint32_t packet_counter = 0;
static int network_coding = 0; //Network coding disabled by default.

//Kodo-c init:
//Define num symbols and size.
//Values are just selected from examples for now.
static uint32_t max_symbols = 10;
static uint32_t max_symbol_size = 1400; //Size of MTU datatgram.
//Select codec
static uint32_t codec = kodoc_on_the_fly; //Sliding window can make use of feedback.
//Finite field to use
static uint32_t finite_field = kodoc_binary8;
//Coding
static uint8_t* decoded_symbols;

//Encoding buffers.
/*struct packet {
	struct ether_hdr eth_hdr;
	uint8_t payload[1400];
};
struct packet encoding_buffer[max_symbols];*/

static volatile bool force_quit;

/* MAC updating enabled by default */
// MAC updating disabled
/*static int mac_updating = 0;*/

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define NB_MBUF 8192

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */
#define MEMPOOL_CACHE_SIZE 256

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint32_t l2fwd_enabled_port_mask = 0;

/* list of enabled ports */
static uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];

static unsigned int l2fwd_rx_queue_per_lcore = 1;

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
	unsigned n_rx_port;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 1, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

struct rte_mempool * l2fwd_pktmbuf_pool = NULL;

#define MAX_TIMER_PERIOD 86400 /* 1 day max */
/* A tsc-based timer responsible for triggering statistics printout */
static uint64_t timer_period = 10; /* default period is 10 seconds */

static void
l2fwd_learning_forward(struct rte_mbuf *m, unsigned portid)
{
	struct rte_eth_dev_tx_buffer *buffer;

	const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Convert data to char.
	//Get ethernet dst and src
	struct ether_addr d_addr = { 
		{data[0],data[1],data[2],data[3],data[4],data[5]}
	};
	struct ether_addr s_addr = {
		{data[6],data[7],data[8],data[9],data[10],data[11]}
	};

	//Check if MAC forwarding table has port entry for dst.
	unsigned mac_add = 0; //Add src mac to table.
	unsigned mac_dst_found = 0; //DST MAC not found by default.
	for (int i=0;i<MAC_ENTRIES;i++)
	{
		if(memcmp(mac_fwd_table[i].d_addr.addr_bytes,s_addr.addr_bytes,sizeof(s_addr.addr_bytes)) == 0) //Check if table contains src address in table
		{
			mac_add = 1; //Dont add mac address as it is already in the table.
		}
		if(memcmp(mac_fwd_table[i].d_addr.addr_bytes,d_addr.addr_bytes,sizeof(d_addr.addr_bytes)) == 0) //Else handle like a normal packet forward.
		{
			//Send packet to dst port.
			buffer = tx_buffer[mac_fwd_table[i].port];
			mac_dst_found = 1;
			rte_eth_tx_buffer(mac_fwd_table[i].port, 0, buffer, m);
			packet_counter++;
		}
	}
	if(mac_dst_found == 0) //Flood the packet out to all ports
	{
		for (uint port = 0; port < rte_eth_dev_count(); port++)
		{
			if(port!=portid)
			{
				buffer = tx_buffer[port];
				rte_eth_tx_buffer(port, 0, buffer, m);
			}
		}
        packet_counter = packet_counter+3;
	}
	if(unlikely(mac_add == 0)) //Add MAC address to MAC table.
	{
		mac_fwd_table[mac_counter].d_addr = s_addr;
		mac_fwd_table[mac_counter].type = DYNAMIC;
		mac_fwd_table[mac_counter].port = portid;
		mac_fwd_table[mac_counter].coding_capable = 0; //Default coding capable to not capable (0) for the time being.
		mac_counter++; //Increment MAC counter.

		printf("\nUpdated MAC TABLE.\n");
		for (int i=0;i<MAC_ENTRIES;i++)
		{
			if(mac_fwd_table[i].d_addr.addr_bytes[0] != 0)
			{
				printf("%u ", mac_fwd_table[i].vlan);
				for (uint j = 0; j < sizeof(mac_fwd_table[i].d_addr.addr_bytes); ++j)
				{
					printf("%02x:", mac_fwd_table[i].d_addr.addr_bytes[j]);
				}
				printf(" %u %u\n", mac_fwd_table[i].type,mac_fwd_table[i].port);
			}
		}
	}
	//Display number of packets sent.
	//printf(" packets forwarded. \r%u", packet_counter);
	//fflush(stdout);
}

static void 
net_encode(struct rte_mbuf *m, unsigned portid, kodoc_coder_t *encoder, kodoc_coder_t *decoder)
{
	//Get recieved packet
	const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Convert data to char.
	//Get ethernet dst and src
	struct ether_addr d_addr = { 
		{data[0],data[1],data[2],data[3],data[4],data[5]}
	};
	struct ether_addr s_addr = {
		{data[6],data[7],data[8],data[9],data[10],data[11]}
	};

	//Create data buffers
	uint32_t block_size = kodoc_block_size(*encoder);
	uint8_t* payload = (uint8_t*) malloc(kodoc_payload_size(*encoder));
	uint8_t* data_in = (uint8_t*) malloc(block_size);

	//Fill data_in with data.
	for(uint j=0;j<max_symbol_size-1;j++)
	{
		if(j<rte_pktmbuf_data_len(m))
		{
			data_in[j] = data[j+12]; //Data starts at 12th byte position, after src and dst. Include eth_type.
		}
		else
		{
			data_in[j] = 0; //Pad with zeros after payload
		}
	}

	//If encoder rank is less than number of symbols, then encode data.
	uint32_t rank = kodoc_rank(*encoder);
	if(rank < kodoc_symbols(*encoder))
	{
		//Assign data buffer to encoder
		kodoc_set_const_symbol(*encoder, rank, data_in, kodoc_symbol_size(*encoder));
	}

	//Writes a symbol to the payload buffer.
	uint32_t bytes_used = kodoc_write_payload(*encoder, payload);
	printf("Payload generated by encoder, rank = %d, bytes used = %d\n", rank, bytes_used);

	//Create mbuf for encoded reply
	struct rte_mbuf* encoded_mbuf = rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);
	char* encoded_data = rte_pktmbuf_append(encoded_mbuf,rte_pktmbuf_data_len(m));
	struct ether_hdr eth_hdr = {	
		d_addr, //Same as incoming source addr.
		s_addr, //Port mac address
		0x2020 //My custom NC Ether type?
	};	
	encoded_data = rte_memcpy(encoded_data,&eth_hdr,ETHER_HDR_LEN);
	encoded_data = rte_memcpy(encoded_data+ETHER_HDR_LEN,payload,max_symbol_size);

	free(data_in);
	free(payload);

	//l2fwd_learning_forward(encoded_mbuf,portid);

	net_decode(encoded_mbuf, portid, decoder);
	rte_pktmbuf_free(encoded_mbuf);

}

static void
net_decode(struct rte_mbuf *m, unsigned portid, kodoc_coder_t *decoder)
{
	//Get recieved packet
	const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Convert data to char.
	//Get ethernet dst and src
	struct ether_addr d_addr = { 
		{data[0],data[1],data[2],data[3],data[4],data[5]}
	};
	struct ether_addr s_addr = {
		{data[6],data[7],data[8],data[9],data[10],data[11]}
	};

	//Create Data buffers
	uint32_t block_size = kodoc_block_size(*decoder);
	uint8_t* payload = (uint8_t*) malloc(kodoc_payload_size(*decoder));
	uint8_t* data_out = (uint8_t*) malloc(block_size);

	//Fill payload to decode, with data.
	for(uint j=0;j<max_symbol_size-1;j++)
	{
		if(j<rte_pktmbuf_data_len(m))
		{
			payload[j] = data[j+14]; //Data starts at 14th byte position, after src and dst. Exclude eth_type (Which will be NC type).
		}
		else
		{
			payload[j] = 0; //Pad with zeros after payload
		}
	}

	//Specifies the data buffer where the decoder will store the decoded symbol.
	kodoc_set_mutable_symbols(*decoder,data_out, block_size);
	//kodoc_set_mutable_symbol(*decoder,kodoc_rank(*decoder) ,data_out, kodoc_symbol_size(*decoder));

	//Pass payload to decoder
	kodoc_read_payload(*decoder,payload);

	//Decoder rank indicates how many symbols have been decoded.
	printf("Payload processed by decoder, current rank = %d and %d",
	       kodoc_rank(*decoder),kodoc_is_partially_complete(*decoder));

	//Start decoding process
	if(kodoc_has_partial_decoding_interface(*decoder) && kodoc_is_partially_complete(*decoder)) //Check if decoder supports partial decoding (which it should) and if symbols have been decoded.
	{
		//Loop through all symbols in generation. Find which symbol has been decoded.
		for(uint i = 0;i<kodoc_symbols(*decoder);i++)
		{
			if(!decoded_symbols[i] && kodoc_is_symbol_uncoded(*decoder,i)) //Symbol is decoded, check if symbol has already been seen.
			{
				//Flag symbol as decoded and seen.
				decoded_symbols[i] = 1;

				uint8_t* data_no_ethertype = (uint8_t*) malloc(block_size);

				//This symbol is newely decoded, send through to normal forwarding.
				//Print decoded message
				printf("\nDecoded Message:\n");
				for(uint k =2;k<max_symbol_size-1;k++)
				{
					data_no_ethertype[k-2] = data_out[k];
					printf("%c ", data_no_ethertype[k-2]);
				}
				printf("\n");

				//Get ether type
				uint16_t original_ether_type = (data_out[0] | (data_out[1] << 8));

				//Create mbuf for decoded reply
			  	struct rte_mbuf* decoded_mbuf = rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);
				char* decoded_data = rte_pktmbuf_append(decoded_mbuf,rte_pktmbuf_data_len(m));
				struct ether_hdr eth_hdr = {	
					d_addr, //Same as incoming source addr.
					s_addr, //Port mac address
					original_ether_type//Ether_type from decoded packet.
				};	
				decoded_data = rte_memcpy(decoded_data,&eth_hdr,ETHER_HDR_LEN);
				rte_memcpy(decoded_data+ETHER_HDR_LEN,data_no_ethertype,max_symbol_size);

				//Dump packets into a file
				FILE *mbuf_file;
				mbuf_file = fopen("mbuf_dump.txt","a");
				fprintf(mbuf_file, "\n ------------------ \n Port:%d ----",portid);
				rte_pktmbuf_dump(mbuf_file,decoded_mbuf,1414);
				fprintf(mbuf_file,"------Decoded------\n"); //Decode raw frame.
				fclose(mbuf_file);
				
			  	l2fwd_learning_forward(decoded_mbuf,portid);
			  	rte_pktmbuf_free(decoded_mbuf);
			  	free(data_no_ethertype);
			}
		}
	}

	free(data_out);
	free(payload);
}

static void
net_recode(struct rte_mbuf *m, unsigned portid, kodoc_coder_t *encoder, kodoc_coder_t *decoder)
{
	printf("%ld %ld %ld %ld\n",sizeof(m), sizeof(portid),sizeof(encoder),sizeof(decoder) );
}

/* main processing loop */
static void
l2fwd_main_loop(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	unsigned i, j, portid, nb_rx;
	struct lcore_queue_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
			BURST_TX_DRAIN_US;
	struct rte_eth_dev_tx_buffer *buffer;

	prev_tsc = 0;
	timer_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	if (qconf->n_rx_port == 0) {
		RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_port; i++) {

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);

	}

	//Network coding.
	//Encoded and decoder factory
	kodoc_factory_t encoder_factory = kodoc_new_encoder_factory(codec,finite_field,max_symbols,max_symbol_size);
	kodoc_factory_t decoder_factory = kodoc_new_decoder_factory(codec,finite_field,max_symbols,max_symbol_size);
	//Create encoder and decoder
	kodoc_coder_t encoder = kodoc_factory_build_coder(encoder_factory);
	kodoc_coder_t decoder = kodoc_factory_build_coder(decoder_factory);
	//Coding Capable flag
	uint8_t coding_capable = 0;

	decoded_symbols = (uint8_t*)malloc(max_symbols*sizeof(uint8_t));
	memset(decoded_symbols, '\0', max_symbols*sizeof(uint8_t)); //Set all elements to zero.

	while (!force_quit) {

		//Reset encoder and decoder after full rank.
		if(kodoc_rank(encoder) == max_symbols)
		{
			kodoc_delete_coder(encoder);
			encoder = kodoc_factory_build_coder(encoder_factory);
		}
		if(kodoc_rank(decoder) == max_symbols)
		{
			kodoc_delete_coder(decoder);
			memset(decoded_symbols, '\0', max_symbols*sizeof(uint8_t)); //Set all elements to zero.
			decoder = kodoc_factory_build_coder(decoder_factory);
		}

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {
			for (i = 0; i < qconf->n_rx_port; i++) {
				portid = l2fwd_dst_ports[qconf->rx_port_list[i]];
				buffer = tx_buffer[portid];
				rte_eth_tx_buffer_flush(portid, 0, buffer);
			}

			/* if timer is enabled */
			if (timer_period > 0) {

				/* advance the timer */
				timer_tsc += diff_tsc;

				/* if timer has reached its timeout */
				if (unlikely(timer_tsc >= timer_period)) {

					/* do this only on master core */
					if (lcore_id == rte_get_master_lcore()) {
						/* reset the timer */
						timer_tsc = 0;
					}
				}
			}
			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_port; i++) {
			//Retrieves up to a maximum of MAX_PKT_BURST packets from NIC queue.
			portid = qconf->rx_port_list[i];
			nb_rx = rte_eth_rx_burst(portid, 0,
						 pkts_burst, MAX_PKT_BURST);
			for (j = 0; j < nb_rx; j++) {
				//Send recieved packets to tx, for each packet recieved
				m = pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));

				//Get recieved packet
				const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Convert data to char.

				//Get ether type
				uint16_t ether_type = (data[13] | (data[12] << 8));

				//Get dst address
				struct ether_addr d_addr = { 
					{data[0],data[1],data[2],data[3],data[4],data[5]}
				};

				if(likely(network_coding == 1))
				{
					//Determine if packet must be encoded (1), decoded (2), recoded (3) or sent to nocode (normal forwarding) (4).
					//Encoded, Decoded and Recoded algorithms will send to normal forwarding once complete. i.e 4 possible directions.

					//Check if next hop is coding capable, or has coding capable nodes.
					for (int k=0;k<MAC_ENTRIES;k++)
					{
						if((memcmp(mac_fwd_table[k].d_addr.addr_bytes,d_addr.addr_bytes,sizeof(d_addr.addr_bytes)) == 0) && mac_fwd_table[k].coding_capable == 1) //Go to encode(1) or recode(3).
						{
							coding_capable = 1;
							break;
						}
						else if(d_addr.addr_bytes[5] == 7) //Temp to say that port1 (debB is coding capable)
						{
							coding_capable = 1;
						}
					}
					if(coding_capable == 1) //Go to encode(1) or recode(3).
					{
						//Check if packet is encoded (NC type).
						if(ether_type == 0x2020) //Go to recode()3.
						{
							net_recode(m, portid, &encoder, &decoder);
							printf("\nRecode\n");
						}
						else //Go to encode(1).
						{
							net_encode(m, portid, &encoder, &decoder);
							printf("\nEncode\n");
						}
					}
					else //Go to decode(2) or nocode(4).
					{
						//Check if packet is encoded (NC type).
						if(ether_type == 0x2020) //Go to decode(2).
						{
							net_decode(m, portid, &decoder);
							printf("\nDecode\n");
						}
						else //Go to nocode(4).
						{
							l2fwd_learning_forward(m, portid);
							printf("\nNocode\n");
						}
					}
					//Reset coding capable flag.
					coding_capable = 0;
				}	
				else //Operate like a normal learning switch.
				{
					l2fwd_learning_forward(m, portid);
				}
			}
		}
	}

	//Cleanup after network coding
	kodoc_delete_coder(encoder);
	kodoc_delete_coder(decoder);
	kodoc_delete_factory(encoder_factory);
	kodoc_delete_factory(decoder_factory);
}

static int
l2fwd_launch_one_lcore(__attribute__((unused)) void *dummy)
{
	l2fwd_main_loop();
	return 0;
}

static int
l2fwd_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (pm == 0)
		return -1;

	return pm;
}

static unsigned int
l2fwd_parse_nqueue(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;
	if (n == 0)
		return 0;
	if (n >= MAX_RX_QUEUE_PER_LCORE)
		return 0;

	return n;
}

static int
l2fwd_parse_timer_period(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n >= MAX_TIMER_PERIOD)
		return -1;

	return n;
}

static const char short_options[] =
	"p:"  /* portmask */
	"q:"  /* number of queues */
	"T:"  /* timer period */
	;

#define CMD_LINE_OPT_NETWORK_CODING "network-coding"
#define CMD_LINE_OPT_NO_NETWORK_CODING "no-network-coding"

enum {
	/* long options mapped to a short option */

	/* first long only option value must be >= 256, so that we won't
	 * conflict with short options */
	CMD_LINE_OPT_MIN_NUM = 256,
};

static const struct option lgopts[] = {
	{ CMD_LINE_OPT_NETWORK_CODING, no_argument, &network_coding, 1},
	{ CMD_LINE_OPT_NO_NETWORK_CODING, no_argument, &network_coding, 0},
	{NULL, 0, 0, 0}
};

/* Parse the argument given in the command line of the application */
static int
l2fwd_parse_args(int argc, char **argv)
{
	int opt, ret, timer_secs;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, short_options,
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			l2fwd_enabled_port_mask = l2fwd_parse_portmask(optarg);
			break;
		/* nqueue */
		case 'q':
			l2fwd_rx_queue_per_lcore = l2fwd_parse_nqueue(optarg);
			break;
		/* timer period */
		case 'T':
			timer_secs = l2fwd_parse_timer_period(optarg);
			timer_period = timer_secs;
			break;
		/* long options */
		case 0:
			break;
		default:
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	if(network_coding== 1)
	{
		printf("Network Coding Enabled.\n");
	}

	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint16_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		if (force_quit)
			return;
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if (force_quit)
				return;
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf(
					"Port%d Link Up. Speed %u Mbps - %s\n",
						portid, link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n", portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		force_quit = true;
	}
}

int
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	struct rte_eth_dev_info dev_info;
	int ret;
	uint16_t nb_ports;
	uint16_t portid, last_port;
	unsigned lcore_id, rx_lcore_id;
	unsigned nb_ports_in_mask = 0;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	argc -= ret;
	argv += ret;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* parse application arguments (after the EAL ones) */
	l2fwd_parse_args(argc, argv);

	/* convert to number of cycles */
	timer_period *= rte_get_timer_hz();

	/* create the mbuf pool */
	l2fwd_pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF,
		MEMPOOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
		rte_socket_id());

	nb_ports = rte_eth_dev_count();

	/* reset l2fwd_dst_ports */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
		l2fwd_dst_ports[portid] = 0;
	last_port = 0;

	/*
	 * Each logical core is assigned a dedicated TX queue on each port.
	 */
	for (portid = 0; portid < nb_ports; portid++) {

		if (nb_ports_in_mask % 2) {
			l2fwd_dst_ports[portid] = last_port;
			l2fwd_dst_ports[last_port] = portid;
		}
		else
			last_port = portid;

		nb_ports_in_mask++;

		rte_eth_dev_info_get(portid, &dev_info);
	}
	if (nb_ports_in_mask % 2) {
		printf("Notice: odd number of ports in portmask.\n");
		l2fwd_dst_ports[last_port] = last_port;
	}

	rx_lcore_id = 0;
	qconf = NULL;

	/* Initialize the port/queue configuration of each logical core */
	for (portid = 0; portid < nb_ports; portid++) {
		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
		       lcore_queue_conf[rx_lcore_id].n_rx_port ==
		       l2fwd_rx_queue_per_lcore) {
			rx_lcore_id++;
		}

		if (qconf != &lcore_queue_conf[rx_lcore_id])
			/* Assigned a new logical core in the loop above. */
			qconf = &lcore_queue_conf[rx_lcore_id];

		qconf->rx_port_list[qconf->n_rx_port] = portid;
		qconf->n_rx_port++;
		printf("Lcore %u: RX port %u\n", rx_lcore_id, portid);
	}

	/* Initialise each port */
	for (portid = 0; portid < nb_ports; portid++) {
		/* init port */
		printf("Initializing port %u... ", portid);
		fflush(stdout);
		ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, portid);

		ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
						       &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot adjust number of descriptors: err=%d, port=%u\n",
				 ret, portid);

		rte_eth_macaddr_get(portid,&l2fwd_ports_eth_addr[portid]);

		/* init one RX queue */
		fflush(stdout);
		ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					     rte_eth_dev_socket_id(portid),
					     NULL,
					     l2fwd_pktmbuf_pool);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, portid);

		/* init one TX queue on each port */
		fflush(stdout);
		ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid),
				NULL);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
				ret, portid);

		/* Initialize TX buffers */
		tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
				RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
				rte_eth_dev_socket_id(portid));
		if (tx_buffer[portid] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
					portid);

		rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);

		void *userdata = (void *)1;
		ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
				rte_eth_tx_buffer_count_callback,userdata);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
			"Cannot set error callback for tx buffer on port %u\n",
				 portid);

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				  ret, portid);

		printf("done: \n");

		//DD. Promiscuous mode enabled.
		rte_eth_promiscuous_enable(portid);

		printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
				portid,
				l2fwd_ports_eth_addr[portid].addr_bytes[0],
				l2fwd_ports_eth_addr[portid].addr_bytes[1],
				l2fwd_ports_eth_addr[portid].addr_bytes[2],
				l2fwd_ports_eth_addr[portid].addr_bytes[3],
				l2fwd_ports_eth_addr[portid].addr_bytes[4],
				l2fwd_ports_eth_addr[portid].addr_bytes[5]);

	}

	check_all_ports_link_status(nb_ports, l2fwd_enabled_port_mask);

	ret = 0;
	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0) {
			ret = -1;
			break;
		}
	}

	//Close ports when finished
	for (portid = 0; portid < nb_ports; portid++) {
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("Closing port %d...", portid);
		rte_eth_dev_stop(portid);
		rte_eth_dev_close(portid);
		printf(" Done\n");
	}

	return ret;
}
