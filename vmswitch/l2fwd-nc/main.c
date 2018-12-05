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
#include <rte_hash.h>
#include <rte_jhash.h>

//Added by DD.
#include "main.h"

//IGMP (Internet Group Managment Protocol) Snooping
#define IGMP_SNOOP_MEMBSHP_QUERY 0x11
#define IGMP_SNOOP_MEMBSHP_REPORT_V1 0x12
#define IGMP_SNOOP_MEMBSHP_REPORT_V2 0x16
#define IGMP_SNOOP_MEMBSHP_REPORT_V3 0x22
#define IGMP_SNOOP_MEMBSHP_LEAVE 0x17 

//<VLAN,MAC,Type,port,coding_capable> table. Simular to CISCO switches? Maybe this will help in the future somewhere..
//Ive added a new idea where the mac table has a coding capable field.
#define MAC_ENTRIES 200
#define ENCODING_RINGS 32
#define GENID_LEN 8
#define ETHER_TYPE_LEN 2

uint32_t gentable_size = 0;

#define STATIC 0
#define DYNAMIC 1
static unsigned mac_counter = 0;
static unsigned genIDcounter = 0;
static unsigned mltcst_counter = 0;

//MAC, genID and multicast tables.
struct mac_table_entry {
	unsigned vlan;
	struct ether_addr d_addr;
	unsigned type;
	unsigned port;
	unsigned coding_capable;
};
struct mac_table_entry *mac_fwd_table; 

struct generationID {
	char ID[GENID_LEN];
};
struct generationID *genID_table;

struct multicast_table_entry {
	uint8_t grp_addr[4];
	struct ether_addr s_addr;
	unsigned port;
	unsigned coding_capable;
};
struct multicast_table_entry *mltcst_fwd_tbl;

//Pointer to all dst_add ring structs. 
struct rte_ring encoding_rings[ENCODING_RINGS];

//Other defines by DD
#define HW_TYPE_ETHERNET 0x0001
static int network_coding;
//Link Sats
static struct rte_eth_link l2fwd_nc_links[RTE_MAX_ETHPORTS];
static struct rte_eth_stats l2fwd_nc_dev_stats[RTE_MAX_ETHPORTS]; 
//Struct for dst_addr status
struct dst_addr_status {
	int status; //The status of the dst_addr. Either 0,1 or 2 for not_found,found and found + coding capable. 3 Is multicast traffic. 4 is multicast + coding capable.
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

//Pkt count
int pktscnt = 0;

static volatile bool force_quit;

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

static int NB_MBUF;

static int MAX_PKT_BURST;

#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */
#define MEMPOOL_CACHE_SIZE 512

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

//DD
struct rte_mempool * codingmbuf_pool = NULL;

#define MAX_TIMER_PERIOD 86400 /* 1 day max */
/* A tsc-based timer responsible for triggering statistics printout */
static uint64_t timer_period = 10; /* default period is 10 seconds */

//Functions defined by D.B.B de Villiers

static void
l2fwd_learning_forward(struct rte_mbuf *m, struct dst_addr_status *status)
{
	struct rte_eth_dev_tx_buffer *buffer;
	if(status->status >= 3) //Multicast packet to dst ports.
	{
		const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Packet data to get grp_addr.
		for (uint port = 0; port < rte_eth_dev_count(); port++)
		{
			for (uint i = 0; i < mltcst_counter; i++)
			{
				if((mltcst_fwd_tbl[i].grp_addr[2] == *(data+4)) && (mltcst_fwd_tbl[i].grp_addr[3] == *(data+5)) && port == mltcst_fwd_tbl[i].port) //Check if grp_addr in table.
				{
					buffer = tx_buffer[port];
					rte_eth_tx_buffer(port, 0, buffer, m);
					rte_eth_tx_buffer_flush(port, 0, buffer);
					printf("Pkt sent out port: %d\n",port);
					break; //Prevents the multiple packets out the same port for each indv source entry.
				}
			}
		}
	}
	else if(status->status >= 1) //Send packet to dst port.
	{
		buffer = tx_buffer[status->dstport];
		rte_eth_tx_buffer(status->dstport, 0, buffer, m);
		rte_eth_tx_buffer_flush(status->dstport, 0, buffer);

		//Output if packet is dropped.
		if(buffer->length >= buffer->size)
		{
			printf("Pkt dropped.\n");
		}
	}
	else if(status->status == 0) //Flood the packet out to all ports
	{
		for (int port = 0; port < rte_eth_dev_count(); port++)
		{
			if(port!=status->srcport)
			{	
				buffer = tx_buffer[port];
				rte_eth_tx_buffer(port, 0, buffer, m);
				rte_eth_tx_buffer_flush(port, 0, buffer);
			}
		}
	}
}


static void 
net_encode(kodoc_factory_t *encoder_factory)
{
	//Loop through each dst_addr in the MAC table and check if the ring is full. If the ring is full then begin encoding on that queue.
	for(uint i=0;i<=mac_counter;i++)
	{
		char ring_name[30];
		sprintf(ring_name,"encoding_ring%d",i);

		struct rte_ring* encode_ring_ptr = rte_ring_lookup(ring_name);

		if(encode_ring_ptr!=NULL)
		{
			if(rte_ring_count(encode_ring_ptr)==MAX_SYMBOLS-1) //Check if ring is fulled, if so, begin encoding. Also need to add if the time limit is reached as an OR.
			{
				//Begin decoding on rings.
				uint* obj_left = 0;
				//rte_mbuf to hold the dequeued data.
				struct rte_mbuf *dequeued_data[MAX_SYMBOLS-1];
				rte_pktmbuf_alloc_bulk(codingmbuf_pool,dequeued_data,MAX_SYMBOLS);
				rte_ring_dequeue_bulk(encode_ring_ptr,(void **)dequeued_data,MAX_SYMBOLS-1,obj_left);
				if(rte_ring_count(encode_ring_ptr)==0) //Checks if dequeued correctly.
				{
					kodoc_coder_t encoder = kodoc_factory_build_coder(*encoder_factory);

					kodoc_set_systematic_off(encoder);

					uint32_t block_size = kodoc_block_size(encoder);

					//Create Mbuf for data_in and payload.
					struct rte_mbuf* rte_mbuf_data_in = rte_pktmbuf_alloc(codingmbuf_pool);

					uint8_t* data_in = rte_pktmbuf_mtod(rte_mbuf_data_in, void *);

					//Process data to be used by encoder.
					for(uint pkt=0;pkt<MAX_SYMBOLS-1;pkt++)
					{
						//Get recieved packet
						struct rte_mbuf *m = dequeued_data[pkt];
						const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Convert data to char.

						//Fill data_in with data.
						for(uint j=0;j<rte_pktmbuf_data_len(m);j++)
						{
							data_in[((MAX_SYMBOL_SIZE)*pkt)+j] = data[j+(2*ETHER_ADDR_LEN)]; //Data starts at 12th byte position, after src and dst. Include eth_type.
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
						struct ether_addr d_addr;
						rte_memcpy(d_addr.addr_bytes,data,ETHER_ADDR_LEN);
						struct ether_addr s_addr;
						rte_memcpy(s_addr.addr_bytes,data+ETHER_ADDR_LEN,ETHER_ADDR_LEN);
						//Allocate payload buffer
						struct rte_mbuf* rte_mbuf_payload = rte_pktmbuf_alloc(codingmbuf_pool);
						uint8_t* payload = rte_pktmbuf_mtod(rte_mbuf_payload, void *);

						//Writes a symbol to the payload buffer.
						int bytes_used = kodoc_write_payload(encoder, payload);
						printf("EncodedPkt Generated: rank:%d bytes_used:%d\n", kodoc_rank(encoder), bytes_used);

						//Use first symbol payload "random data" for generationID
						//Instead generate random array of characters. 
						//Create generationID 
						char genID[GENID_LEN];
						if(pkt == 0) //Create genID only during first pkt. use this genID for all other pkts in generation.
						{
							int genChar;
							for(genChar=0;genChar<GENID_LEN;genChar++)
							{
								genID[genChar] = 'A' + (random() % 26);
							}
						}

						//Create mbuf for encoded reply
						struct rte_mbuf* encoded_mbuf = rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);
						char* encoded_data = rte_pktmbuf_append(encoded_mbuf,rte_pktmbuf_data_len(m)+GENID_LEN+10);
						struct ether_hdr eth_hdr = {	
							d_addr, //Same as incoming source addr.
							s_addr, //Port mac address
							0x2020 //My custom NC Ether type?
						};
						encoded_data = rte_memcpy(encoded_data,&eth_hdr,ETHER_HDR_LEN); //Add eth_hdr
						encoded_data = rte_memcpy(encoded_data+ETHER_HDR_LEN,genID,sizeof(genID)); //Add generationID
						encoded_data = rte_memcpy(encoded_data+sizeof(genID),payload,kodoc_payload_size(encoder)); //Add payload

						struct dst_addr_status status = dst_mac_status(m, 0);
					  	l2fwd_learning_forward(encoded_mbuf, &status);
					  	rte_pktmbuf_free(rte_mbuf_payload);
					  	rte_pktmbuf_free(encoded_mbuf);
					  	//rte_pktmbuf_free();

					  	//Temp print encoded packet
					  	//rte_mempool_dump(stdout,l2fwd_pktmbuf_pool);
						//rte_pktmbuf_dump(stdout,encoded_mbuf,100);
					}

					rte_pktmbuf_free(rte_mbuf_data_in);
					kodoc_delete_coder(encoder);
				}
				else
				{
					printf("Encoding error occured. Packets not encoded due to queue being empty.\n");
				}
			}
		}
	} 
}

static void
net_decode(kodoc_factory_t *decoder_factory)
{
	//Loop through each decoding ring and check if the ring has atleast one object. 
	for(uint i=0;i<genIDcounter;i++)
	{
		char ring_id[GENID_LEN+1];
		rte_memcpy(ring_id,(genID_table+i)->ID,sizeof((genID_table+i)->ID));
		struct rte_ring* decoding_ring = rte_ring_lookup(ring_id);

		if(decoding_ring!=NULL)
		{
			printf("In Ring %s Count:%d\n",decoding_ring->name,rte_ring_count(decoding_ring));
			if(rte_ring_count(decoding_ring)>=MAX_SYMBOLS-1) //Check if ring is fulled, if so, begin decoding. Also need to add if the time limit is reached as an OR.
			{
				//Begin decoding on rings.
				uint* obj_left = 0;
				//rte_mbuf to hold the dequeued data.
				struct rte_mbuf *dequeued_data[MAX_SYMBOLS];
				int obj_dequeued = rte_ring_mc_dequeue_bulk(decoding_ring,(void **)dequeued_data,MAX_SYMBOLS-1,obj_left);

				if(obj_dequeued>=(int)MAX_SYMBOLS-1)
				{
					printf("Decoding..\n");
					//Create decoder
					kodoc_coder_t decoder = kodoc_factory_build_coder(*decoder_factory);
					//Create Data buffers
					uint32_t block_size = kodoc_block_size(decoder);

					struct rte_mbuf* rte_mbuf_data_out = rte_pktmbuf_alloc(codingmbuf_pool);

					uint8_t* data_out = rte_pktmbuf_mtod(rte_mbuf_data_out, void *);

					//Specifies the data buffer where the decoder will store the decoded symbol.
					kodoc_set_mutable_symbols(decoder , data_out, block_size);

					//Loop through each packet in the queue. In the future, it would be better to encode as a group using pointers instead.
					uint pkt=0;
					while (!kodoc_is_complete(decoder))
					{
						//Get recieved packet
						if(pkt>=(MAX_SYMBOLS-1))
						{
							rte_ring_free(decoding_ring);
							//Remove generationID from table.
							for(uint genIndex=i;genIndex<genIDcounter;genIndex++)//i is the position to remove the genID at.
							{
								genID_table[genIndex] = genID_table[genIndex+1]; 
							}
							memset(&genID_table[i], 0, sizeof(struct generationID));
							genIDcounter--;
							rte_pktmbuf_free(rte_mbuf_data_out);
							kodoc_delete_coder(decoder);
							printf("GENID removed\n");
							return;
						}
						struct rte_mbuf *m = dequeued_data[pkt];
						const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Convert data to char.

						struct rte_mbuf* rte_mbuf_payload = rte_pktmbuf_alloc(codingmbuf_pool);
						uint8_t* payload = rte_pktmbuf_mtod(rte_mbuf_payload, void *);

						int rank  = kodoc_rank(decoder);

						//Fill payload to decode, with data.
						for(uint j=0;j<rte_pktmbuf_data_len(m);j++)
						{
							payload[j] = (uint8_t)data[j+ETHER_HDR_LEN+GENID_LEN]; //Data starts at 14th byte position, after src and dst. Exclude eth_type (Which will be NC type).
						}

						//Pass payload to decoder
						kodoc_read_payload(decoder,payload);

						//Decoder rank indicates how many symbols have been decoded.
						printf("Payload processed by decoder, current rank = %d\n", rank);
						rte_pktmbuf_free(rte_mbuf_payload);

						pkt++;
					}

					uint8_t* pkt_ptr = data_out;
					for(uint pkt=0;pkt<MAX_SYMBOLS-1;pkt++) //Get decoded packets from data_out and send those out again. Resultant packets are systematic.
					{
						//Stores each packet from data_out
						//Get recieved packet
						struct rte_mbuf *m = dequeued_data[pkt];
						const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Convert data to char.
						
						//Get ethernet dst and src
						struct ether_addr d_addr;
						rte_memcpy(d_addr.addr_bytes,data,ETHER_ADDR_LEN);
						struct ether_addr s_addr;
						rte_memcpy(s_addr.addr_bytes,data+ETHER_ADDR_LEN,ETHER_ADDR_LEN);

						//Create mbuf for decoded reply
					  	struct rte_mbuf* decoded_mbuf = rte_pktmbuf_alloc(codingmbuf_pool);
						char* decoded_data = rte_pktmbuf_append(decoded_mbuf,rte_pktmbuf_data_len(m)-GENID_LEN-10);
						struct ether_hdr eth_hdr = {
							d_addr, //Same as incoming source addr.
							s_addr, //Port mac address
							(pkt_ptr[0] | ((pkt_ptr[1]) << 8)) //Ether_type from decoded packet.
						};	
						
						decoded_data = rte_memcpy(decoded_data,&eth_hdr,ETHER_HDR_LEN);
						decoded_data = rte_memcpy(decoded_data+ETHER_HDR_LEN-ETHER_TYPE_LEN,pkt_ptr,MAX_SYMBOL_SIZE); //Original ether header must be overwritten else it is writen twice. Allows using std ether_hdr struct.
						
						struct dst_addr_status status = dst_mac_status(decoded_mbuf, 0);
					  	l2fwd_learning_forward(decoded_mbuf, &status);

					  	rte_pktmbuf_free(decoded_mbuf);

					  	//Advance pointer to next decoded data of packet in data_out.
					  	pkt_ptr += MAX_SYMBOL_SIZE;
					}

					rte_pktmbuf_free(rte_mbuf_data_out);

					//Remove generationID from table.
					for(uint genIndex=i;genIndex<genIDcounter;genIndex++)//i is the position to remove the genID at.
					{
						genID_table[genIndex] = genID_table[genIndex+1]; 
					}
					memset(&genID_table[i], 0, sizeof(struct generationID));
					genIDcounter--;

					printf("GENID Deleted: GEN TABLE UPDATED:\n");
					for(uint genIndex=0;genIndex<=genIDcounter;genIndex++)
					{
						char ring_name[GENID_LEN+1];
						rte_memcpy(ring_name,(genID_table+genIndex)->ID,GENID_LEN);
						printf("%s:", ring_name);
						for(uint j = 0;j<GENID_LEN;j++)
						{
							printf("%02X ",genID_table[genIndex].ID[j]);
						}
						printf("\n");
					}

					//Free decoder.
					kodoc_delete_coder(decoder);
				}
				rte_ring_free(decoding_ring);
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
			genID_table = (struct generationID*)calloc(setting,setting * sizeof(struct generationID));
			//Table for IGMP multicast cache table. 
			mltcst_fwd_tbl = calloc(setting,setting * sizeof(struct multicast_table_entry));;

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

static struct rte_ring*
genID_in_genTable(char *generationID) //Need to add a flushing policy
{
	//Loop through genID_table, and check if generationID exists in table.
	int in_table = 0;
	for(uint i=0;i<genIDcounter;i++)
	{
		char ring_name[GENID_LEN+1];
		rte_memcpy(ring_name,(genID_table+i)->ID,GENID_LEN);
		if(memcmp(generationID,ring_name,GENID_LEN) == 0)
		{
			in_table = 1;
			return rte_ring_lookup(ring_name);
		}
	}
	if(in_table == 0) //GEN_ID not in table, so make new ring for decoded packets.
	{
		//Add genID to table.
		rte_memcpy(genID_table[genIDcounter].ID,generationID,GENID_LEN);
		
		//Create decoder queue for newly received generationID	
		char ring_name[GENID_LEN+1];
		sprintf(ring_name,"%s",genID_table[genIDcounter].ID);
		struct rte_ring *new_ring = rte_ring_create((const char *)ring_name,MAX_SYMBOLS,SOCKET_ID_ANY,0);

		if(new_ring!=NULL) //If ring created sucessfully.
		{
			printf("GEN TABLE UPDATED:\n");
			for(uint i=0;i<=genIDcounter;i++)
			{
				for(uint j = 0;j<GENID_LEN;j++)
				{
					printf("%02X ",genID_table[i].ID[j]);
				}
				printf("\n");
			}

			genIDcounter++;
		}
		return new_ring;
	}
	return NULL;
}

//Check if dst_addr exists, otherwise adds it to the MAC table. 1 if it exits, 2 if it exists and is coding capable, and 0 if it doesnt exist.
static struct 
dst_addr_status dst_mac_status(struct rte_mbuf *m, unsigned srcport)
{
	//Get recieved packet
	const unsigned char* data = rte_pktmbuf_mtod(m, void *); //Convert data to char.
	//Get ethernet dst and src
	struct ether_addr d_addr;
	rte_memcpy(d_addr.addr_bytes,data,ETHER_ADDR_LEN);
	struct ether_addr s_addr;
	rte_memcpy(s_addr.addr_bytes,data+ETHER_ADDR_LEN,ETHER_ADDR_LEN);

	struct dst_addr_status status;

	status.dstport = -1;
	status.srcport = srcport;
	status.status = 0;
	unsigned mac_add = 0; //Add src mac to table.

	//Check if Multicast traffic or normal.
	uint8_t multicast_mac[3] = {0x01,0x00,0x5E};

	//First check if packet is multicast traffic. First 3 bytes must be 01:00:5E of dst MAC addr.
	if(memcmp(d_addr.addr_bytes,multicast_mac,3)==0)
	{
		//IGMP Snooping based on the guidelines of RFC 4541. Modified for testing purposes. Not to full spec.. Local LAN only. No routing support.
		//Check if IGMP multicast or multicast data traffic.
		if(data[38] == 0x22 && data[5] == 0x16) //IGMP Forwarding (Control) L3.
		{
			//Check if INLCUDE or EXCLUDE
			if(data[46]== 0x03) //To INCLUDE 0 sources. SO do no receive traffic.
			{
				//Remove Port from multicast forward.
				for (int i=0;i<MAC_ENTRIES;i++)
				{
					if(memcmp(mltcst_fwd_tbl[i].s_addr.addr_bytes,s_addr.addr_bytes,sizeof(s_addr.addr_bytes))==0)
					{
						if(memcmp(mltcst_fwd_tbl[i].grp_addr,data+50,sizeof(mltcst_fwd_tbl[i].grp_addr))==0)
						{
							//Reset entry
							memset(&mltcst_fwd_tbl[i], 0, sizeof(struct multicast_table_entry));

							for(uint j=i;j<MAC_ENTRIES-1;j++)
							{
								mltcst_fwd_tbl[j] = mltcst_fwd_tbl[j+1];
							}

							mltcst_counter--;

							printf("\nUpdated Mltcst TABLE\n");
							for(uint i=0;i<=mltcst_counter;i++)
							{
								if(mltcst_fwd_tbl[i].s_addr.addr_bytes[0] != 0)
								{
									for (uint j = 0; j < sizeof(mltcst_fwd_tbl[i].grp_addr); ++j)
									{
										printf("%02x:", mltcst_fwd_tbl[i].grp_addr[j]);
									}
									printf("  ");
									for (uint j = 0; j < sizeof(mltcst_fwd_tbl[i].s_addr.addr_bytes); ++j)
									{
										printf("%02x:", mltcst_fwd_tbl[i].s_addr.addr_bytes[j]);
									}
									printf(" port: %d",mltcst_fwd_tbl[i].port);
								}
								printf("\n");
							}
						}
					}
				}
			}	
			else if(data[46]== 0x04) //To EXLUDE 0 sources. SO do receive traffic.
			{
				//Add Port for multicast forward.
				//Loop through multicast table and check if entry exists first.
				uint8_t in_mltcst_table = 0;
				for (int i=0;i<MAC_ENTRIES;i++)
				{
					if(memcmp(mltcst_fwd_tbl[i].s_addr.addr_bytes,s_addr.addr_bytes,sizeof(s_addr.addr_bytes))==0)
					{
						if(memcmp(mltcst_fwd_tbl[i].grp_addr,data+50,sizeof(mltcst_fwd_tbl[i].grp_addr))==0)
						{
							in_mltcst_table = 1;
							break;
						}
					}
				}
				if(in_mltcst_table==0)
				{
					//Add to multicast table.
					rte_memcpy(mltcst_fwd_tbl[mltcst_counter].grp_addr,data+50,sizeof(mltcst_fwd_tbl[mltcst_counter].grp_addr));
					mltcst_fwd_tbl[mltcst_counter].s_addr = s_addr;
					mltcst_fwd_tbl[mltcst_counter].port = srcport;
					mltcst_fwd_tbl[mltcst_counter].coding_capable = 1; //TEMP set coding capable to all multicast traffic.

					mltcst_counter++;

					printf("\nUpdated Mltcst TABLE\n");
					for(uint i=0;i<mltcst_counter;i++)
					{
						if(mltcst_fwd_tbl[i].s_addr.addr_bytes[0] != 0)
						{
							for (uint j = 0; j < sizeof(mltcst_fwd_tbl[i].grp_addr); ++j)
							{
								printf("%02x:", mltcst_fwd_tbl[i].grp_addr[j]);
							}
							printf("  ");
							for (uint j = 0; j < sizeof(mltcst_fwd_tbl[i].s_addr.addr_bytes); ++j)
							{
								printf("%02x:", mltcst_fwd_tbl[i].s_addr.addr_bytes[j]);
							}
							printf(" port: %d",mltcst_fwd_tbl[i].port);
						}
						printf("\n");
					}
				}
			}

			status.status = 0; //Flood IGMP traffic out all ports. Should be all router ports but this use case is not as complicated.
		}
		else //Data Forwarding (Data)
		{
			struct ether_addr grp_addr;
			uint8_t grp_data[6] = {0xe0,0x00,data[4],data[5],0,0};
			rte_memcpy(grp_addr.addr_bytes,grp_data,ETHER_ADDR_LEN);

			//Check if dst_ip inside or outside of 244.0.0.X range.
			if(likely(data[4] != 0)) //Out of 244.0.0.X and not IGMP
			{
				//Forward according to group-based port membership tables and forward on router ports. 
				//Add to encoding ring.
				for (int i=0;i<MAC_ENTRIES;i++)
				{					
					if(memcmp(mltcst_fwd_tbl[i].grp_addr,grp_addr.addr_bytes,sizeof(mltcst_fwd_tbl[i].grp_addr))==0) //Check if grp_addr in table.
					{
						status.status = 3;
					}	

					//Check if in MAC_table as well for encoding queues.
					//Check if MAC table contains grp address.
					if(memcmp(mac_fwd_table[i].d_addr.addr_bytes,grp_addr.addr_bytes,sizeof(mac_fwd_table[i].d_addr.addr_bytes)) == 0)
					{
						mac_add = 1; //Dont add mac address as it is already in the table.
						if(mac_fwd_table[i].coding_capable == 1)
						{
							status.status = 4;
							status.table_index = i;
							break;
						}
					}
				}

				if(unlikely(mac_add==0))
				{
					add_mac_addr(grp_addr,srcport,1);						
				}
			}
			else //In 244.0.0.X and not IGMP
			{
				//Forward on all ports. I.e flood traffic.
				status.status = 0;
			}
		}
	}
	else
	{
		for (int i=0;i<MAC_ENTRIES;i++)
		{
			if(memcmp(mac_fwd_table[i].d_addr.addr_bytes,s_addr.addr_bytes,sizeof(s_addr.addr_bytes)) == 0) //Check if table contains src address.
			{
				mac_add = 1; //Dont add mac address as it is already in the table.
			}

			if(memcmp(mac_fwd_table[i].d_addr.addr_bytes,d_addr.addr_bytes,sizeof(d_addr.addr_bytes)) == 0) //Check if table contains dst address.
			{
				status.status = 1; //Not coding capable.
				if(mac_fwd_table[i].coding_capable == 1)
				{
					status.status = 2; //Coding capable.
				}
				status.table_index = i;
				status.dstport = mac_fwd_table[i].port;
			}
		}

		if(unlikely(mac_add==0))
		{
			add_mac_addr(s_addr,srcport,0);
		}
	}

	return status;
}

static void add_mac_addr(struct ether_addr addr, unsigned srcport, unsigned coding_capable)
{
	mac_fwd_table[mac_counter].vlan = 0;
	mac_fwd_table[mac_counter].d_addr = addr;
	mac_fwd_table[mac_counter].type = DYNAMIC;
	mac_fwd_table[mac_counter].port = srcport;
	mac_fwd_table[mac_counter].coding_capable = coding_capable; //Default coding capable to not capable (0) for the time being.

	//Also create rte_ring for encoding queue, for each new MAC entry.
	char ring_name[30];
	sprintf(ring_name,"encoding_ring%d",mac_counter);
	encoding_rings[mac_counter] = *rte_ring_create((const char *)ring_name,MAX_SYMBOLS,SOCKET_ID_ANY,0);

	//Increment MAC counter.
	mac_counter++;

	//Print updated MAC table to Console.
	printf("\nUpdated MAC TABLE.\nVLAN D_ADDR             TYPE PORT CODING_CAPABLE\n");
	for(uint i=0;i<mac_counter;i++)
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

				//TEMP PRINT the the m buffer.
				//rte_pktmbuf_dump(stdout,m,100);

				//Get recieved packet
				const unsigned char* data = rte_pktmbuf_mtod(m, void *); 

				//Get ether type
				uint16_t ether_type = (data[13] | (data[12] << 8));

				//Get dst_addr status.
				struct dst_addr_status status = dst_mac_status(m, portid); 

				if(likely(network_coding == 1) && status.status != 0) //If status is 0, then default to normal forwarding. 
				{
					//Determine if packet must be encoded (1), decoded (2), recoded (3) or sent to nocode (normal forwarding) (4).
					//Encoded, Decoded and Recoded algorithms will send to normal forwarding once complete. i.e 4 possible directions.

					if(status.status == 2 || status.status == 4) //Go to encode(1) or recode(3).
					{
						//Check if packet is encoded (NC type).
						if(ether_type == 0x2020) //Go to recode()3.
						{
							printf("Recode\n");
							net_recode(&encoder_factory);
						}
						else //Go to encode(1).
						{
							printf("\nEncode");
							//Add packet to encoding ring.
							char ring_name[30];
							sprintf(ring_name,"encoding_ring%d",status.table_index);
							struct rte_ring* encode_ring_ptr = rte_ring_lookup(ring_name);
							rte_ring_enqueue(encode_ring_ptr,(void *)m);

							//Run encoder function.
							net_encode(&encoder_factory);
						}
					}
					else //Go to decode(2) or nocode(4).
					{
						//Check if packet is encoded (NC type).
						if(ether_type == 0x2020) //Go to decode(2).
						{
							printf("Decode\n");
							//Get genID from encoded packet.
							char genID[GENID_LEN];
							rte_memcpy(genID,&data[14],GENID_LEN);

							//Check if GENID is valid
							for(uint genchar=0;genchar<GENID_LEN;genchar++)
							{
								if(genID[genchar]==0) //Drop packet.
								{
									printf("Invalid Encoded Packet.\n");
									break;
								}
							}

							//Check if genID is in genTable
							struct rte_ring* decode_ring_ptr = genID_in_genTable(genID);
							if(decode_ring_ptr!=NULL)
							{
								//Add packet to decoding ring
								rte_ring_enqueue(decode_ring_ptr,(void *)m);
								//Call decoder
								net_decode(&decoder_factory);
							}
							else
							{
								printf("Decoding Ring does not exist.\n");
							}
						}
						else //Go to nocode(4).
						{
							printf("Nocode\n");
							l2fwd_learning_forward(m, &status);
						}
					}
				}	
				else //Operate like a normal learning switch.
				{
					printf("Nocode\n");
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

	//DD create the mbuf pool for data_in and payload for encoder. And for decoder.
	codingmbuf_pool = rte_pktmbuf_pool_create("coding_pool",NB_MBUF,MEMPOOL_CACHE_SIZE,0,MAX_SYMBOLS*MAX_SYMBOL_SIZE,SOCKET_ID_ANY);

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
/*
		void *userdata = (void *)1;
		ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
				rte_eth_tx_buffer_count_callback,userdata);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
			"Cannot set error callback for tx buffer on port %u\n",
				 portid);*/

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

	//TEMP. Pre update MAC table to make nodes debE -> debH coding_capable.
	for(int i = 0;i<4;i++)
	{
		struct ether_addr d_addr = { 
			{0xDE,0xAD,0xBE,0xEF,0x01,i+6}
		};

		mac_fwd_table[mac_counter].vlan = 0;
		mac_fwd_table[mac_counter].d_addr = d_addr;
		mac_fwd_table[mac_counter].type = DYNAMIC;
		mac_fwd_table[mac_counter].port = 4;
		mac_fwd_table[mac_counter].coding_capable = 1; //Default coding capable to not capable (0) for the time being.

		//Also create rte_ring for encoding queue, for each new MAC entry.
		char ring_name[30];
		sprintf(ring_name,"encoding_ring%d",mac_counter);
		encoding_rings[mac_counter] = *rte_ring_create((const char *)ring_name,MAX_SYMBOLS,SOCKET_ID_ANY,0);

		//Increment MAC counter.
		mac_counter++;
	}

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
	free(genID_table);

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
