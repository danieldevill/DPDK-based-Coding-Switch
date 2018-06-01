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
// Also my comments are "//" vs /* */ of dpdk.
//DD. I've "butchered" all  statistics code, just to clean.  

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

//ARP table
#define ARP_ENTRIES 100
static uint64_t arp_table[ARP_ENTRIES][3];
static unsigned arp_counter = 0;

//<VLAN,MAC,Type,port> table. Simular to CISCO switches? Maybe this will help in the future somewhere..
#define MAC_ENTRIES 20
#define STATIC 0
#define DYNAMIC 1
static unsigned mac_counter = 0;
struct mac_table_entry {
	unsigned vlan;
	struct ether_addr d_addr;
	unsigned type;
	unsigned port;
};
struct mac_table_entry mac_fwd_table[MAC_ENTRIES]; 

//Other defines by DD
#define HW_TYPE_ETHERNET 0x0001
static uint32_t packet_counter = 0;

static volatile bool force_quit;

/* MAC updating enabled by default */
// MAC updating disabled
static int mac_updating = 0;

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

	//DD

	//Get recieved packet
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
	//Display number of packets sent.
	printf(" packets forwarded. \r%u", packet_counter);
	fflush(stdout);
	if(unlikely(mac_add == 0)) //Add MAC address to MAC table.
	{
		mac_fwd_table[mac_counter].d_addr = s_addr;
		mac_fwd_table[mac_counter].type = DYNAMIC;
		mac_fwd_table[mac_counter].port = portid;
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
}

//DD.
//Method to reply to ARP requests. 
void
l2fwd_arp_reply(struct rte_mbuf* m, unsigned portid)
{
	//Construct ARP reply
	unsigned char* arp_data = (unsigned char*)rte_pktmbuf_mtod(m, void *) + 14;
	//Target Protocol Address
	uint32_t trg_ptcl_addr = (arp_data[24] << 24) | (arp_data[25] << 16) | (arp_data[26] << 8) | arp_data[27];
	//Check if port is the target protocol address.
	if(likely(port_ip_lookup(&trg_ptcl_addr,portid) != 0))
	{
		return;
	}
	//Hardware Type
	uint16_t hw_type = (arp_data[0] << 8) | arp_data[1];
	//Protocol Type
	uint16_t ptcl_type = (arp_data[2] << 8) | arp_data[3];
	//Operation Code
	uint16_t op_code = (arp_data[6] << 8) | arp_data[7];
	//Source Hardware Address
	uint64_t src_hw_addr = ((uint64_t)arp_data[8] << 40) | ((uint64_t)arp_data[9] << 32) | ((uint64_t)arp_data[10] << 24) | ((uint64_t)arp_data[11] << 16) | ((uint64_t)arp_data[12] << 8) | (uint64_t)arp_data[13];
	//Source Protocol Address
	uint32_t src_ptcl_addr = (arp_data[14] << 24) | (arp_data[15] << 16) | (arp_data[16] << 8) | arp_data[17];

	//Check ARP counter. If so reset.
	if(arp_counter >= ARP_ENTRIES)
	{
		arp_counter = 0;
	}

	//Begin ARP reception algorithm, based on: http://www.danzig.jct.ac.il/tcp-ip-lab/ibm-tutorial/3376c28.html
	//Correct hw type: Ethernet
	if(hw_type != HW_TYPE_ETHERNET)
	{
		return;
	}
	//Correct protocol: IP
	if(ptcl_type != ETHER_TYPE_IPv4)
	{
		return;
	}
	//ARP table entry flag
	int arp_tbl_ent = 0; //False
	//Check if <protocol type, sender protocol address> is in table
	int i;
	for(i=0;i<ARP_ENTRIES;i++)
	{
		if(arp_table[i][0] == ptcl_type && arp_table[i][1] == src_ptcl_addr)
		{
			//Update table with sender hardware address
			arp_table[i][2] = src_hw_addr;
			//Set table entry flag to true.
			arp_tbl_ent = 1;
		}
	}
	//Check if flag is still false
	if(arp_tbl_ent == 0)
	{	
		//Add ARP table triplet to table.
		arp_table[arp_counter][0] = ptcl_type;
		arp_table[arp_counter][1] = src_ptcl_addr;
		arp_table[arp_counter][2] = src_hw_addr;
		//Increment arp_counter
		arp_counter++;
	}
	//Check if request
	if(op_code != 1)
	{
		return;
	}
	//Reply to requesting host.
	//Send back ARP packet as ARP Reply
	//Create mbuf packet struct and ether header.
	uint8_t src_ptcl_addr_ar[] = {192,168,portid+1,254};
	uint8_t trg_ptcl_addr_ar[] = {192,168,portid+1,2};
	uint8_t arp_header[] = {0x00,0x01,0x08,0x00,0x06,0x04,0x00,0x02};
	struct rte_mbuf* arp_mbuf;
	struct ether_addr s_addr;
	struct ether_addr d_addr = { //Same as ARP request src_hw_addr.
		{arp_data[8],arp_data[9],arp_data[10],arp_data[11],arp_data[12],arp_data[13]}
	};
	rte_eth_macaddr_get(portid,&s_addr); 
	struct ether_hdr eth_hdr = {	
		d_addr, //Same as incoming source addr.
		s_addr, //Port mac address
		0x0608 //ARP Ether type
	};	
	//Allocate mbuf to pool.
	arp_mbuf = rte_pktmbuf_alloc(l2fwd_pktmbuf_pool);
	char* pkt_data = rte_pktmbuf_append(arp_mbuf,44); //Returns pointer to where new appended packet data starts.
	pkt_data = rte_memcpy(pkt_data,&eth_hdr,ETHER_HDR_LEN); //Append header to packet.
	//Append ARP data to packet
	pkt_data = rte_memcpy(pkt_data+ETHER_HDR_LEN,&arp_header,sizeof(arp_header)); //Append arp_header to packet.
	pkt_data = rte_memcpy(pkt_data+sizeof(arp_header),&s_addr,sizeof(eth_hdr.s_addr.addr_bytes)); //Append src_hw_addr to packet.
	pkt_data = rte_memcpy(pkt_data+sizeof(eth_hdr.s_addr.addr_bytes),&src_ptcl_addr_ar,sizeof(src_ptcl_addr_ar)); //Append src_pctl_addr to packet.
	pkt_data = rte_memcpy(pkt_data+sizeof(src_ptcl_addr_ar),&d_addr,sizeof(eth_hdr.d_addr.addr_bytes)); //Append trg_hw_addr to packet.
	rte_memcpy(pkt_data+sizeof(eth_hdr.d_addr.addr_bytes),&trg_ptcl_addr_ar,sizeof(trg_ptcl_addr_ar)); //Append trg_pctl_addr to packet.
	
	//Get tx buffer of dst_port
	unsigned dst_port = portid;
	struct rte_eth_dev_tx_buffer *buffer = tx_buffer[dst_port];

	//Add data to tx buffer, to be sent out when full.
	rte_eth_tx_buffer(dst_port, 0, buffer, arp_mbuf);
	rte_pktmbuf_free(arp_mbuf);
}

//DD
//Check if IP address belongs to port. For use with ARP reply.
int
port_ip_lookup(uint32_t *trg_ptcl_addr, unsigned portid)
{
	if(portid+1 == ((*trg_ptcl_addr << 16) >> 24) )
	{
		return 0;
	}
	else
	{
		return 1;
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
				l2fwd_learning_forward(m, portid);
			}
		}
	}
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

#define CMD_LINE_OPT_MAC_UPDATING "mac-updating"
#define CMD_LINE_OPT_NO_MAC_UPDATING "no-mac-updating"

enum {
	/* long options mapped to a short option */

	/* first long only option value must be >= 256, so that we won't
	 * conflict with short options */
	CMD_LINE_OPT_MIN_NUM = 256,
};

static const struct option lgopts[] = {
	{ CMD_LINE_OPT_MAC_UPDATING, no_argument, &mac_updating, 1},
	{ CMD_LINE_OPT_NO_MAC_UPDATING, no_argument, &mac_updating, 0},
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
