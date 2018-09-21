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

//Include for libconfig
#include <libconfig.h>
//Include for libsodium
#include <sodium.h>

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
static int MAC_ENTRIES = 20;
static int ENCODING_RINGS = 20;
static int DECODING_RINGS = 20;

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
struct mac_table_entry *mac_fwd_table; 
//Pointer to all dst_add ring structs. 
struct rte_ring *encoding_rings;
struct rte_ring *decoding_rings;

//Other defines by DD
#define HW_TYPE_ETHERNET 0x0001
static uint32_t nb_tx_total = 0;
static int network_coding;
//Link Sats
static struct rte_eth_link l2fwd_nc_links[RTE_MAX_ETHPORTS];
static struct rte_eth_stats l2fwd_nc_dev_stats[RTE_MAX_ETHPORTS]; 
//Struct for dst_addr status
struct dst_addr_status {
	int status; //The status of the dst_addr. Either 0,1 or 2 for not_found,found and found + coding capable.
	int table_index; //The index of the dst_addr on the table.
	int dstport; //The #ID of the dst_addr port. 
	int srcport; //The #ID of the in port.
};

//Kodo-c init:
//Define num symbols and size.
//Values are just selected from examples for now.
static uint32_t MAX_SYMBOLS;
static uint32_t MAX_SYMBOL_SIZE; //Size of MTU datatgram.
//Select codec
static uint32_t codec = kodoc_full_vector; //Sliding window can make use of feedback.
//Finite field to use
static uint32_t finite_field = kodoc_binary8;

static volatile bool force_quit;

/* MAC updating enabled by default */
// MAC updating disabled
/*static int mac_updating = 0;*/

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

static int NB_MBUF;

static int MAX_PKT_BURST;

#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */
#define MEMPOOL_CACHE_SIZE 256

/*
 * Configurable number of RX/TX ring descriptors
 */
static uint16_t nb_rxd;
static uint16_t nb_tx_totald;

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

//Functions defined by D.B.B de Villiers

static void
l2fwd_learning_forward(struct rte_mbuf *m, struct dst_addr_status *status)
{
	struct rte_eth_dev_tx_buffer *buffer;

	if(status->status >= 1) //Send packet to dst port.
	{
		buffer = tx_buffer[status->dstport];
		rte_eth_tx_buffer(status->dstport, 0, buffer, m);
		nb_tx_total++;
	}
	else if(status->status == 0) //Flood the packet out to all ports
	{
		for (int port = 0; port < rte_eth_dev_count(); port++)
		{
			if(port!=status->srcport)
			{
				buffer = tx_buffer[port];
				rte_eth_tx_buffer(port, 0, buffer, m);
				nb_tx_total++;
			}
		}
	}
}

static void 
net_encode(kodoc_factory_t *encoder_factory)
{
	//Loop through each dst_addr in the MAC table and check if the ring is full. If the ring is full then begin encoding on that queue.
	for(int i=0;i<=ENCODING_RINGS;i++)
	{
		if(rte_ring_count(&encoding_rings[i])>=MAX_SYMBOLS-1) //Check if ring is fulled, if so, begin encoding. Also need to add if the time limit is reached as an OR.
		{
			//Begin decoding on rings.
			uint* obj_left = 0;
			//rte_mbuf to hold the dequeued data.
			struct rte_mbuf *dequeued_data[MAX_SYMBOLS];
			if(rte_ring_dequeue_bulk(&encoding_rings[i],(void **)dequeued_data,MAX_SYMBOLS-1,obj_left)>0) //Checks if dequeued correctly.
			{
				printf("Encoding..\n");

				kodoc_coder_t encoder = kodoc_factory_build_coder(*encoder_factory);

				kodoc_set_systematic_off(encoder);

				uint32_t block_size = kodoc_block_size(encoder);

				uint8_t* data_in = (uint8_t*) malloc(block_size);

				//Process data to be used by encoder.
				for(uint pkt=0;pkt<MAX_SYMBOLS-1;pkt++)
				{
					//Get recieved packet
					struct rte_mbuf *m = dequeued_data[pkt];
					const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Convert data to char.

					//Fill data_in with data.
					for(uint j=0;j<MAX_SYMBOL_SIZE;j++)
					{
						if(j<rte_pktmbuf_data_len(m))
						{
							data_in[((MAX_SYMBOL_SIZE)*pkt)+j] = data[j+12]; //Data starts at 12th byte position, after src and dst. Include eth_type.
						}
						else
						{
							data_in[((MAX_SYMBOL_SIZE)*pkt)+j] = 0; //Pad with zeros after payload
						}
					}
				}

				//Assign data buffer to encoder
				kodoc_set_const_symbols(encoder, data_in, block_size);

				//Loop through each packet in the queue.
				for(uint pkt=0;pkt<MAX_SYMBOLS-1;pkt++)
				{
					//Get recieved packet
					struct rte_mbuf *m = dequeued_data[pkt];
					const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Convert data to char.
					//Get ethernet dst and src
					struct ether_addr d_addr = { 
						{data[0],data[1],data[2],data[3],data[4],data[5]}
					};
					struct ether_addr s_addr = {
						{data[6],data[7],data[8],data[9],data[10],data[11]}
					};
					//Create data buffers
					uint8_t* payload = (uint8_t*) malloc(kodoc_payload_size(encoder));

					//Writes a symbol to the payload buffer.
					int bytes_used = kodoc_write_payload(encoder, payload);
					printf("Payload generated by encoder, rank = %d, bytes used = %d\n", kodoc_rank(encoder), bytes_used);

					//Create generationID
					char genID[12];
					generate_generationID(genID);

					//Create mbuf for encoded reply
					struct rte_mbuf* encoded_mbuf = rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);
					char* encoded_data = rte_pktmbuf_append(encoded_mbuf,rte_pktmbuf_data_len(m));
					struct ether_hdr eth_hdr = {	
						d_addr, //Same as incoming source addr.
						s_addr, //Port mac address
						0x2020 //My custom NC Ether type?
					};	
					encoded_data = rte_memcpy(encoded_data,&eth_hdr,ETHER_HDR_LEN);
					encoded_data = rte_memcpy(encoded_data+ETHER_HDR_LEN,payload,MAX_SYMBOL_SIZE);

					//Add packet to decoding ring. TEMP
					struct dst_addr_status status = dst_mac_status(m, 0); //Set srcport to 0, doesnt matter as only need table_index

					rte_ring_enqueue(&decoding_rings[status.table_index],(void *)encoded_mbuf);

					//Dump packets into a file
					FILE *mbuf_file;
					mbuf_file = fopen("mbuf_dump.txt","a");
					fprintf(mbuf_file, "\n ------ENCODED------ \n Port: ----");
					rte_pktmbuf_dump(mbuf_file,encoded_mbuf,1414);
					fclose(mbuf_file);

					free(payload);
				}

				free(data_in);
				kodoc_delete_coder(encoder);
			}
			else
			{
				printf("Encoding error occured. Packets not encoded due to queue being empty.\n");
			}
		}
	} 
}

static void
net_decode(kodoc_factory_t *decoder_factory)
{
	//Loop through each decoding ring and check if the ring has atleast one object. 
	for(int i=0;i<=DECODING_RINGS;i++)
	{
		if(rte_ring_count(&decoding_rings[i])>=MAX_SYMBOLS-1) //Check if ring is fulled, if so, begin decoding. Also need to add if the time limit is reached as an OR.
		{
			//Begin decoding on rings.
			uint* obj_left = 0;
			//rte_mbuf to hold the dequeued data.
			struct rte_mbuf *dequeued_data[MAX_SYMBOLS];
			int obj_dequeued = rte_ring_dequeue_bulk(&decoding_rings[i],(void **)dequeued_data,MAX_SYMBOLS-1,obj_left);
			if(obj_dequeued>=(int)MAX_SYMBOLS-1)
			{
				printf("Decoding..");

				//Create decoder
				kodoc_coder_t decoder = kodoc_factory_build_coder(*decoder_factory);
				//Create Data buffers
				uint32_t block_size = kodoc_block_size(decoder);

				uint8_t* data_out = (uint8_t*) malloc(block_size);

				//Specifies the data buffer where the decoder will store the decoded symbol.
				kodoc_set_mutable_symbols(decoder , data_out, block_size);

				//Loop through each packet in the queue. In the future, it would be better to encode as a group using pointers instead.
				uint pkt=0;
				while (!kodoc_is_complete(decoder))
				{
					//Get recieved packet
					struct rte_mbuf *m = dequeued_data[pkt];
					const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Convert data to char.

					uint8_t* payload = (uint8_t*)malloc(kodoc_payload_size(decoder));

					int rank  = kodoc_rank(decoder);

					//Fill payload to decode, with data.
					for(uint j=0;j<MAX_SYMBOL_SIZE-1;j++)
					{
						if(j<rte_pktmbuf_data_len(m))
						{
							payload[j] = (uint8_t)data[j+14]; //Data starts at 14th byte position, after src and dst. Exclude eth_type (Which will be NC type).
						}
						else
						{
							payload[j] = 0; //Pad with zeros after payload
						}
					}

					//Pass payload to decoder
					kodoc_read_payload(decoder,payload);

					//Decoder rank indicates how many symbols have been decoded.
					printf("Payload processed by decoder, current rank = %d\n", rank);
					free(payload);
					pkt++;
				}

				for(uint pkt=0;pkt<MAX_SYMBOLS-1;pkt++) //Get decoded packets from data_out and send those out again.
				{
					//Stores each packet from data_out
					//Get recieved packet
					struct rte_mbuf *m = dequeued_data[pkt];
					const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Convert data to char.

					uint8_t* payload = (uint8_t*)malloc(kodoc_payload_size(decoder));
					
					for(uint j=0;j<MAX_SYMBOL_SIZE-1;j++)
					{
						payload[j] = data_out[((MAX_SYMBOL_SIZE)*pkt)+j];
						printf("%x_",payload[j]);
					}

					printf("\n");

					//Get ethernet dst and src
					struct ether_addr d_addr = {
						{data[0],data[1],data[2],data[3],data[4],data[5]}
					};
					struct ether_addr s_addr = {
						{data[6],data[7],data[8],data[9],data[10],data[11]}
					};

					//Get ether type
					uint16_t original_ether_type = (payload[0] | (payload[1] << 8));

					//Create mbuf for decoded reply
				  	struct rte_mbuf* decoded_mbuf = rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);
					char* decoded_data = rte_pktmbuf_append(decoded_mbuf,rte_pktmbuf_data_len(m));
					struct ether_hdr eth_hdr = {
						d_addr, //Same as incoming source addr.
						s_addr, //Port mac address
						original_ether_type//Ether_type from decoded packet.
					};	
					decoded_data = rte_memcpy(decoded_data,&eth_hdr,ETHER_HDR_LEN);
					rte_memcpy(decoded_data+ETHER_HDR_LEN-2,payload,MAX_SYMBOL_SIZE); //Original ether header must be overwritten else it is writen twice. Allows using std ether_hdr struct.
					
					//Dump packets into a file
				  	FILE *mbuf_file;
					mbuf_file = fopen("mbuf_dump.txt","a");
					fprintf(mbuf_file, "\n ------DECODED------ \n Port:----");
					rte_pktmbuf_dump(mbuf_file,decoded_mbuf,1414);
					fclose(mbuf_file);

					struct dst_addr_status status = dst_mac_status(m, 0);
				  	l2fwd_learning_forward(decoded_mbuf, &status);
				  	rte_pktmbuf_free(decoded_mbuf);
				  	free(payload);
				  }

				free(data_out);
				kodoc_delete_coder(decoder);
			}
		}	
	}
}

static void
net_recode(kodoc_factory_t *encoder_factory)
{
	printf("%ld\n",sizeof(encoder_factory));
}

//Libconfig setup. DD
static void
update_settings(void)
{
	config_t cfg;
	int setting;

	config_init(&cfg);

	if(!config_read_file(&cfg,"l2fwd-nc.cfg")) //Read config file and display any errors.
	{
		printf("An error has occured with the l2fwd-nc.cfg configuration. %s\n%d\n%s", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
	}
	else
	{
		//Get all config settings from file and set variables accordingly.
		//General Settings
		if(config_lookup_int(&cfg,"general_settings.network_coding",&setting))
		{
			network_coding = setting;
		}
		if(config_lookup_int(&cfg,"general_settings.MAC_ENTRIES",&setting))
		{
			mac_fwd_table = calloc(setting,setting * sizeof(struct mac_table_entry));
			//Allocate memory for encoding rings equal to MAC table size.
			encoding_rings = calloc(setting,setting * sizeof(struct rte_ring));
			decoding_rings = calloc(setting,setting * sizeof(struct rte_ring));
		}
		if(config_lookup_int(&cfg,"general_settings.NB_MBUF",&setting))
		{
			NB_MBUF = setting;
		}
		if(config_lookup_int(&cfg,"general_settings.MAX_PKT_BURST",&setting))
		{
			MAX_PKT_BURST = setting;
		}
		if(config_lookup_int(&cfg,"general_settings.RTE_TEST_RX_DESC_DEFAULT",&setting))
		{
			nb_rxd = setting;
		}
		if(config_lookup_int(&cfg,"general_settings.RTE_TEST_TX_DESC_DEFAULT",&setting))
		{
			nb_tx_totald = setting;
		}
		//Network Coding Settings
		if(config_lookup_int(&cfg,"network_coding_settings.MAX_SYMBOLS",&setting))
		{
			MAX_SYMBOLS = setting;
		}
		if(config_lookup_int(&cfg,"network_coding_settings.MAX_SYMBOL_SIZE",&setting))
		{
			MAX_SYMBOL_SIZE = setting;
		}
		/*if(config_lookup_int(&cfg,"network_coding_settings.codec",&setting))
		{
			printf("codec: %d\n",setting);
		}
		if(config_lookup_int(&cfg,"network_coding_settings.finite_field",&setting))
		{
			printf("finite_field: %d\n",setting);
		}*/		

	}

	config_destroy(&cfg);
}

//Libsodium generation ID generation.
static void 
generate_generationID(char generationID[12])
{
	if(sodium_init()<0)
	{
		printf("libsodium not found!\n");
	}

	randombytes_buf(generationID,12);

}

//Check if dst_addr exists, otherwise adds it to the MAC table. 1 if it exits, 2 if it exists and is coding capable, and 0 if it doesnt exist.
static struct dst_addr_status dst_mac_status(struct rte_mbuf *m, unsigned srcport)
{
	//Get recieved packet
	const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Convert data to char.

	//Get dst address
	struct ether_addr d_addr = { 
		{data[0],data[1],data[2],data[3],data[4],data[5]}
	};

	struct ether_addr s_addr = {
		{data[6],data[7],data[8],data[9],data[10],data[11]}
	};

	struct dst_addr_status status;

	status.dstport = -1;
	status.srcport = srcport;

	unsigned mac_add = 0; //Add src mac to table.
	unsigned mac_dst_found = 0; //DST MAC not found by default.
	for (int i=0;i<MAC_ENTRIES;i++)
	{

		if(memcmp(mac_fwd_table[i].d_addr.addr_bytes,s_addr.addr_bytes,sizeof(s_addr.addr_bytes)) == 0) //Check if table contains src address.
		{
			mac_add = 1; //Dont add mac address as it is already in the table.
		}

		if(memcmp(mac_fwd_table[i].d_addr.addr_bytes,d_addr.addr_bytes,sizeof(d_addr.addr_bytes)) == 0) //Check if table contains dst address.
		{
			mac_dst_found = 1; //Not coding capable.
			if(mac_fwd_table[i].coding_capable == 1)
			{
				mac_dst_found = 2; //Coding capable.
			}
			status.table_index = i;
			status.dstport = mac_fwd_table[i].port;
		}
	}

	if(unlikely(mac_add == 0)) //Add MAC address to MAC table.
	{
		mac_fwd_table[mac_counter].vlan = 0;
		mac_fwd_table[mac_counter].d_addr = s_addr;
		mac_fwd_table[mac_counter].type = DYNAMIC;
		mac_fwd_table[mac_counter].port = srcport;
		mac_fwd_table[mac_counter].coding_capable = 0; //Default coding capable to not capable (0) for the time being.
		if(s_addr.addr_bytes[5] == 7) //TEMP make debB coding capable.
		{
			mac_fwd_table[mac_counter].coding_capable = 1;
		}

		//Also create rte_ring for encoding queue, for each new MAC entry.
		char ring_name[30];
		sprintf(ring_name,"encoding_ring%d",mac_counter);
		encoding_rings[mac_counter] = *rte_ring_create((const char *)ring_name,MAX_SYMBOLS,SOCKET_ID_ANY,0);
		//TEMP create decoding ring at same time as encoding. I need to figure out where to put it.		
		sprintf(ring_name,"decoding_ring%d",mac_counter);
		decoding_rings[mac_counter] = *rte_ring_create((const char *)ring_name,MAX_SYMBOLS,SOCKET_ID_ANY,0);

		//Increment MAC counter.
		mac_counter++;

		//Print updated MAC table to Console.
		printf("\nUpdated MAC TABLE.\nVLAN D_ADDR             TYPE PORT CODING_CAPABLE\n");
		for(int i=0;i<MAC_ENTRIES;i++)
		{
			if(mac_fwd_table[i].d_addr.addr_bytes[0] != 0)
			{
				printf("%u    ", mac_fwd_table[i].vlan);
				for (uint j = 0; j < sizeof(mac_fwd_table[i].d_addr.addr_bytes); ++j)
				{
					printf("%02x:", mac_fwd_table[i].d_addr.addr_bytes[j]);
				}
				printf(" %u    %u         %u\n", mac_fwd_table[i].type,mac_fwd_table[i].port,mac_fwd_table[i].coding_capable);
			}
		}
	}

	status.status = mac_dst_found;
	return status;
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
	kodoc_factory_t encoder_factory = kodoc_new_encoder_factory(codec,finite_field,MAX_SYMBOLS-1,MAX_SYMBOL_SIZE);
	kodoc_factory_t decoder_factory = kodoc_new_decoder_factory(codec,finite_field,MAX_SYMBOLS-1,MAX_SYMBOL_SIZE);

	while (!force_quit) {
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

				//Get dst_addr status.
				struct dst_addr_status status = dst_mac_status(m, portid); 

				if(likely(network_coding == 1) && status.status != 0) //If status is 0, then default to normal forwarding. 
				{
					//Determine if packet must be encoded (1), decoded (2), recoded (3) or sent to nocode (normal forwarding) (4).
					//Encoded, Decoded and Recoded algorithms will send to normal forwarding once complete. i.e 4 possible directions.

					if(status.status == 2) //Go to encode(1) or recode(3).
					{
						//Check if packet is encoded (NC type).
						if(ether_type == 0x2020) //Go to recode()3.
						{
							printf("\nRecode\n");
							net_recode(&encoder_factory);
						}
						else //Go to encode(1).
						{
							printf("\nEncode\n");
							//Add packet to encoding ring.
							rte_ring_enqueue(&encoding_rings[status.table_index],(void *)m);

							//Dump packets into a file
						  	FILE *mbuf_file;
							mbuf_file = fopen("mbuf_dump.txt","a");
							fprintf(mbuf_file, "\n ------BEFORE------ \n Port:----");
							rte_pktmbuf_dump(mbuf_file,m,1414);
							fclose(mbuf_file);

							//Run encoder function.
							net_encode(&encoder_factory);
							//TEMP Run decoder right after encoder.
							net_decode(&decoder_factory);
						}
					}
					else //Go to decode(2) or nocode(4).
					{
						//Check if packet is encoded (NC type).
						if(ether_type == 0x2020) //Go to decode(2).
						{
							printf("\nDecode\n");
							net_decode(&decoder_factory);
						}
						else //Go to nocode(4).
						{
							printf("\nNocode\n");
							l2fwd_learning_forward(m, &status);
						}
					}
				}	
				else //Operate like a normal learning switch.
				{
					l2fwd_learning_forward(m, &status);
				}
			}
		}

		//Write stats on each link to a file.
		FILE *link_stats;
		int nb_devs = rte_eth_dev_count();
		link_stats = fopen("link.stats","r+");
		for(int port_id =0;port_id<nb_devs;port_id++) //Print stats for each dev
		{
			fprintf(link_stats,"port:%u;mac_address:%02X:%02X:%02X:%02X:%02X:%02X;",
					port_id,
					l2fwd_ports_eth_addr[port_id].addr_bytes[0],
					l2fwd_ports_eth_addr[port_id].addr_bytes[1],
					l2fwd_ports_eth_addr[port_id].addr_bytes[2],
					l2fwd_ports_eth_addr[port_id].addr_bytes[3],
					l2fwd_ports_eth_addr[port_id].addr_bytes[4],
					l2fwd_ports_eth_addr[port_id].addr_bytes[5]);
			
			rte_eth_link_get_nowait(port_id,&l2fwd_nc_links[port_id]); //Nowait version way faster, no details on one vs the other however. 

			fprintf(link_stats,"link_speed:%d;link_duplex:%d;link_status:%d;",
					l2fwd_nc_links[port_id].link_speed,
					l2fwd_nc_links[port_id].link_duplex,
					l2fwd_nc_links[port_id].link_status);

			rte_eth_stats_get(port_id,&l2fwd_nc_dev_stats[port_id]);

			fprintf(link_stats,"ipackets:%ld;opackets:%ld;ibytes:%ld;obytes:%ld;ierrors:%ld;oerrors:%ld;",
					l2fwd_nc_dev_stats[port_id].ipackets,
					l2fwd_nc_dev_stats[port_id].opackets,
					l2fwd_nc_dev_stats[port_id].ibytes,
					l2fwd_nc_dev_stats[port_id].obytes,
					l2fwd_nc_dev_stats[port_id].ierrors,
					l2fwd_nc_dev_stats[port_id].oerrors);
	
			fprintf(link_stats,"\n");
		}
		fclose(link_stats);
	}

	//Cleanup after network coding
	kodoc_delete_factory(encoder_factory);
	kodoc_delete_factory(decoder_factory);
}

//DPDK Specific Functions.

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

enum {
	/* long options mapped to a short option */

	/* first long only option value must be >= 256, so that we won't
	 * conflict with short options */
	CMD_LINE_OPT_MIN_NUM = 256,
};

static const struct option lgopts[] = {
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
	else
	{
		printf("Network Coding Disabled\n");
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
	//Update config settings first from l2fwd-nc.cfg
	//DD
	update_settings();

	//Print start_time to file.
	FILE *start_time;
	start_time = fopen("start.time","r+");
	fprintf(start_time,"%ld",time(NULL));
	fclose(start_time);	

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
						       &nb_tx_totald);
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
					     l2fwd_pktmbuf_pool); //Flush here? DD.
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, portid);

		/* init one TX queue on each port */
		fflush(stdout);
		ret = rte_eth_tx_queue_setup(portid, 0, nb_tx_totald,
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

	//Free rings and MAC_TABLE
	free(mac_fwd_table);
	free(encoding_rings);
	free(decoding_rings);

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
