/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <inttypes.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_udp.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

// #define PKT_TX_IPV4          (1ULL << 55)
// #define PKT_TX_IP_CKSUM      (1ULL << 54)

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 128
#define MAX_FLOW_NUM 100
#define PORT_NUM 5001

uint64_t NUM_PING = 100;

/* Define the mempool globally */
struct rte_mempool *mbuf_pool = NULL;
static struct rte_ether_addr my_eth;
static size_t message_size = 1000;
static uint32_t seconds = 1;

uint64_t flow_size = 10000;
int packet_len = 1000;
int flow_num = 1;

size_t window_len = 10;
bool window_ack_mask[10][35];
uint64_t window_sent_time[10][35];
int window_left[10];
uint64_t start_time;

pthread_t ptid[10];
pthread_mutex_t window_info_mutex[10];

static uint64_t raw_time(void) {
    struct timespec tstart = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    uint64_t t = (uint64_t)(tstart.tv_sec * 1.0e9 + tstart.tv_nsec);
    return t;
}

static uint64_t time_now(uint64_t offset) {
    return raw_time() - offset;
}

uint32_t
checksum(unsigned char *buf, uint32_t nbytes, uint32_t sum) {
    unsigned int i;

    /* Checksum all the pairs of bytes first. */
    for (i = 0; i < (nbytes & ~1U); i += 2) {
        sum += (uint16_t)ntohs(*((uint16_t *)(buf + i)));
        if (sum > 0xFFFF)
            sum -= 0xFFFF;
    }

    if (i < nbytes) {
        sum += buf[i] << 8;
        if (sum > 0xFFFF)
            sum -= 0xFFFF;
    }

    return sum;
}

uint32_t
wrapsum(uint32_t sum) {
    sum = ~sum & 0xFFFF;
    return htons(sum);
}

static int parse_packet(struct sockaddr_in *src,
                        struct sockaddr_in *dst,
                        void **payload,
                        size_t *payload_len,
                        struct rte_mbuf *pkt) {
    // packet layout order is (from outside -> in):
    // ether_hdr
    // ipv4_hdr
    // udp_hdr
    // client timestamp
    uint8_t *p = rte_pktmbuf_mtod(pkt, uint8_t *);
    size_t header = 0;

    // check the ethernet header
    struct rte_ether_hdr *const eth_hdr = (struct rte_ether_hdr *)(p);
    p += sizeof(*eth_hdr);
    header += sizeof(*eth_hdr);
    uint16_t eth_type = ntohs(eth_hdr->ether_type);
    struct rte_ether_addr mac_addr = {};

    rte_eth_macaddr_get(1, &mac_addr);
    if (!rte_is_same_ether_addr(&mac_addr, &eth_hdr->dst_addr)) {
        printf("Bad MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
               " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
               eth_hdr->dst_addr.addr_bytes[0], eth_hdr->dst_addr.addr_bytes[1],
               eth_hdr->dst_addr.addr_bytes[2], eth_hdr->dst_addr.addr_bytes[3],
               eth_hdr->dst_addr.addr_bytes[4], eth_hdr->dst_addr.addr_bytes[5]);
        return -1;
    }
    if (RTE_ETHER_TYPE_IPV4 != eth_type) {
        printf("Bad ether type\n");
        return -2;
    }

    // check the IP header
    struct rte_ipv4_hdr *const ip_hdr = (struct rte_ipv4_hdr *)(p);
    p += sizeof(*ip_hdr);
    header += sizeof(*ip_hdr);

    // In network byte order.
    in_addr_t ipv4_src_addr = ip_hdr->src_addr;
    in_addr_t ipv4_dst_addr = ip_hdr->dst_addr;

    if (IPPROTO_UDP != ip_hdr->next_proto_id) {
        printf("Bad next proto_id\n");
        return -3;
    }

    src->sin_addr.s_addr = ipv4_src_addr;
    dst->sin_addr.s_addr = ipv4_dst_addr;

    // check udp header
    struct rte_udp_hdr *const udp_hdr = (struct rte_udp_hdr *)(p);
    p += sizeof(*udp_hdr);
    header += sizeof(*udp_hdr);

    // In network byte order.
    in_port_t udp_src_port = udp_hdr->src_port;
    in_port_t udp_dst_port = udp_hdr->dst_port;

    int ret = rte_be_to_cpu_16(udp_hdr->dst_port) - PORT_NUM;
    if (ret < 0 || ret >= MAX_FLOW_NUM) {
        printf("Bad port number %d\n", rte_be_to_cpu_16(udp_hdr->dst_port));
        return -4;
    }

    src->sin_port = udp_src_port;
    dst->sin_port = udp_dst_port;

    src->sin_family = AF_INET;
    dst->sin_family = AF_INET;

    *payload_len = pkt->pkt_len - header;
    *payload = (void *)p;
    return ret;
}
/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */

/* Main functional part of port initialization. 8< */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool) {
    struct rte_eth_conf port_conf;
    const uint16_t rx_rings = 1, tx_rings = 1;
    uint16_t nb_rxd = RX_RING_SIZE;
    uint16_t nb_txd = TX_RING_SIZE;
    int retval;
    uint16_t q;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf txconf;

    if (!rte_eth_dev_is_valid_port(port))
        return -1;

    memset(&port_conf, 0, sizeof(struct rte_eth_conf));

    retval = rte_eth_dev_info_get(port, &dev_info);
    if (retval != 0) {
        printf("Error during getting device (port %u) info: %s\n",
               port, strerror(-retval));
        return retval;
    }

    if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
        port_conf.txmode.offloads |=
            RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

    /* Configure the Ethernet device. */
    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
    if (retval != 0)
        return retval;

    /* Allocate and set up 1 RX queue per Ethernet port. */
    for (q = 0; q < rx_rings; q++) {
        retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
                                        rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval < 0)
            return retval;
    }

    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    /* Allocate and set up 1 TX queue per Ethernet port. */
    for (q = 0; q < tx_rings; q++) {
        retval = rte_eth_tx_queue_setup(port, q, nb_txd,
                                        rte_eth_dev_socket_id(port), &txconf);
        if (retval < 0)
            return retval;
    }

    /* Starting Ethernet port. 8< */
    retval = rte_eth_dev_start(port);
    /* >8 End of starting of ethernet port. */
    if (retval < 0)
        return retval;

    /* Display the port MAC address. */
    retval = rte_eth_macaddr_get(port, &my_eth);
    if (retval != 0)
        return retval;

    printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
           " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
           port, RTE_ETHER_ADDR_BYTES(&my_eth));

    /* Enable RX in promiscuous mode for the Ethernet device. */
    retval = rte_eth_promiscuous_enable(port);
    /* End of setting RX port in promiscuous mode. */
    if (retval != 0)
        return retval;

    return 0;
}
/* >8 End of main functional part of port initialization. */

// a thread responsible to handle ack msg from receiver.
void listen_ack()
{
    int nb_rx = 0;
    struct rte_mbuf *pkts[BURST_SIZE];
    while (true) {
        nb_rx = rte_eth_rx_burst(1, 0, pkts, BURST_SIZE);
        if (nb_rx == 0) {
            continue;
        }

        // printf("Received burst of %u\n", (unsigned)nb_rx);
        for (int i = 0; i < nb_rx; i++) {
            struct sockaddr_in src, dst;
            void *payload = NULL;
            size_t payload_length = 0;
            int p = parse_packet(&src, &dst, &payload, &payload_length, pkts[i]);
            if (p >= 0) {
                int seq_num = *(int *)payload;
                if (seq_num >= window_left[p] && seq_num < window_left[p] + window_len) {
                    pthread_mutex_lock(&window_info_mutex[p]);
                    // if (seq_num % 1000 == 0) {
                    //     uint64_t used_time = time_now(start_time);
                    //     // printf("seq_num: %d, used_time: %" PRIu64 "\n", seq_num, used_time);
                    // }
                    uint64_t used_time = time_now(start_time);
                    if (seq_num % 100 == 0) printf("port: %d, seq_num: %d, used_time: %" PRIu64 "\n", p, seq_num, used_time);
                    window_ack_mask[p][seq_num - window_left[p]] = true;
                    pthread_mutex_unlock(&window_info_mutex[p]);
                }
            }
            rte_pktmbuf_free(pkts[i]);
        }
    }
}

/* >8 End Basic forwarding application lcore. */

static void*
lcore_main(void *port_id_ptr) {
    struct rte_mbuf *pkts[BURST_SIZE];
    struct rte_mbuf *pkt;
    // char *buf_ptr;
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ipv4_hdr;
    struct rte_udp_hdr *udp_hdr;

    // Specify the dst mac address here:
    struct rte_ether_addr dst = {{0xEC, 0xB1, 0xD7, 0x85, 0x1A, 0x33}};

    struct sliding_hdr *sld_h_ack;
    uint16_t nb_rx;
    uint64_t reqs = 0;
    // uint64_t cycle_wait = intersend_time * rte_get_timer_hz() / (1e9);

    // TODO: add in scaffolding for timing/printing out quick statistics
    int port_id = *(int *)port_id_ptr;
    printf("port_id: %" PRIu16 "\n", port_id);
    bool send_done = false;
    pthread_mutex_lock(&window_info_mutex[port_id]);
    window_left[port_id] = 0;
    for (size_t j = 0; j < window_len; j++)
    {
        window_ack_mask[port_id][j] = false;
        window_sent_time[port_id][j] = 0;
    }
    pthread_mutex_unlock(&window_info_mutex[port_id]);
    while (!send_done)
    {
        // printf("test\n");
        pthread_mutex_lock(&window_info_mutex[port_id]);
        if (window_ack_mask[port_id][0]) {
            int shift = 1;
            for (size_t i = 1; i < window_len; i++) {
                if (!window_ack_mask[port_id][i]) break;
                shift++;
            }
            for (size_t i = 0; i < window_len - shift; i++) {
                window_ack_mask[port_id][i] = window_ack_mask[port_id][i + shift];
                window_sent_time[port_id][i] = window_sent_time[port_id][i + shift];
            }
            for (size_t i = window_len - shift; i < window_len; i++) {
                window_ack_mask[port_id][i] = false;
                window_sent_time[port_id][i] = 0;
            }
            window_left[port_id] += shift;
            // printf("***window_len: %lu, window_left: %d, NUM_PING: %u\n", window_len, win_left, NUM_PING);
            if (window_left[port_id] >= NUM_PING) {
                send_done = true;
                break;
            }
        }
        pthread_mutex_unlock(&window_info_mutex[port_id]);
        // printf("window_len: %lu, window_left: %d\n", window_len, window_left);
        
        for (size_t i = 0; i < window_len; i++) {
            int seq_num = window_left[port_id] + i;
            if (seq_num >= NUM_PING) continue;
            pthread_mutex_lock(&window_info_mutex[port_id]);
            if (window_sent_time[port_id][i] == 0 || (!window_ack_mask[port_id][i] && time_now(window_sent_time[port_id][i]) > (uint64_t)1e9))
            {
                // printf("seq_num: %d, sent_time: %" PRIu64 ", ack: %d\n", seq_num, window_sent_time[port_id][i], window_ack_mask[port_id][i]);
                // send a packet
                pkt = rte_pktmbuf_alloc(mbuf_pool);
                if (pkt == NULL) {
                    printf("Error allocating tx mbuf\n");
                    return;
                }
                size_t header_size = 0;

                uint8_t *ptr = rte_pktmbuf_mtod(pkt, uint8_t *);
                /* add in an ethernet header */
                eth_hdr = (struct rte_ether_hdr *)ptr;

                rte_ether_addr_copy(&my_eth, &eth_hdr->src_addr);
                rte_ether_addr_copy(&dst, &eth_hdr->dst_addr);
                eth_hdr->ether_type = rte_be_to_cpu_16(RTE_ETHER_TYPE_IPV4);
                ptr += sizeof(*eth_hdr);
                header_size += sizeof(*eth_hdr);

                /* add in ipv4 header*/
                ipv4_hdr = (struct rte_ipv4_hdr *)ptr;
                ipv4_hdr->version_ihl = 0x45;
                ipv4_hdr->type_of_service = 0x0;
                ipv4_hdr->total_length = rte_cpu_to_be_16(sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) + message_size);
                ipv4_hdr->packet_id = rte_cpu_to_be_16(1);
                ipv4_hdr->fragment_offset = 0;
                ipv4_hdr->time_to_live = 64;
                ipv4_hdr->next_proto_id = IPPROTO_UDP;
                ipv4_hdr->src_addr = rte_cpu_to_be_32(0x0A000001);  // 10.0.0.1
                ipv4_hdr->dst_addr = rte_cpu_to_be_32(0x0A000002);  // 10.0.0.2

                uint32_t ipv4_checksum = wrapsum(checksum((unsigned char *)ipv4_hdr, sizeof(struct rte_ipv4_hdr), 0));
                // printf("Checksum is %u\n", (unsigned)ipv4_checksum);
                ipv4_hdr->hdr_checksum = rte_cpu_to_be_32(ipv4_checksum);
                header_size += sizeof(*ipv4_hdr);
                ptr += sizeof(*ipv4_hdr);

                /* add in UDP hdr*/
                udp_hdr = (struct rte_udp_hdr *)ptr;
                uint16_t srcp = PORT_NUM + port_id;
                uint16_t dstp = PORT_NUM + port_id;
                udp_hdr->src_port = rte_cpu_to_be_16(srcp);
                udp_hdr->dst_port = rte_cpu_to_be_16(dstp);
                udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(struct rte_udp_hdr) + packet_len);

                uint16_t udp_cksum = rte_ipv4_udptcp_cksum(ipv4_hdr, (void *)udp_hdr);

                // printf("Udp checksum is %u\n", (unsigned)udp_cksum);
                udp_hdr->dgram_cksum = rte_cpu_to_be_16(udp_cksum);
                ptr += sizeof(*udp_hdr);
                header_size += sizeof(*udp_hdr);

                /* set the payload */
                memset(ptr, 'a', packet_len);
                memcpy(ptr, &seq_num, sizeof(int));

                pkt->l2_len = RTE_ETHER_HDR_LEN;
                pkt->l3_len = sizeof(struct rte_ipv4_hdr);
                // pkt->ol_flags = PKT_TX_IP_CKSUM | PKT_TX_IPV4;
                pkt->data_len = header_size + packet_len;
                pkt->pkt_len = header_size + packet_len;
                pkt->nb_segs = 1;
                int pkts_sent = 0;

                unsigned char *pkt_buffer = rte_pktmbuf_mtod(pkt, unsigned char *);

                pkts_sent = rte_eth_tx_burst(1, 0, &pkt, 1);
                if (pkts_sent == 1) {
                    window_sent_time[port_id][i] = raw_time();
                    // window_ack_mask[port_id][i] = true;
                    reqs ++;
                    // printf("window_len: %lu, window_left: %d\n", window_len, window_left[port_id]);
                    if (seq_num % 100 == 0) printf("port: %d, seq_num: %d\n", port_id, seq_num);
                }

                uint64_t last_sent = rte_get_timer_cycles();
                // printf("Sent packet at %u, %d is outstanding, intersend is %u\n", (unsigned)last_sent, outstanding, (unsigned)intersend_time);
            }
            pthread_mutex_unlock(&window_info_mutex[port_id]);
        }
        
    }
    printf("PortID %" PRIu16 ": Sent %" PRIu64 " packets.\n", port_id, reqs); 
    pthread_exit(NULL);
}
/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */

int main(int argc, char *argv[]) {
    unsigned nb_ports;
    uint16_t portid;

    if (argc == 3) {
        flow_num = (int)atoi(argv[1]);
        flow_size = (uint64_t)atoi(argv[2]);
    } else {
        printf("usage: ./lab1-client <flow_num> <flow_size>\n");
        return 1;
    }
    printf("FLOW_SIZE: %" PRIu64 "\n", flow_size);
    NUM_PING = flow_size / packet_len;
    printf("NUM_PING: %" PRIu64 "\n", NUM_PING);

    /* Initializion the Environment Abstraction Layer (EAL). 8< */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    /* >8 End of initialization the Environment Abstraction Layer (EAL). */

    argc -= ret;
    argv += ret;

    nb_ports = rte_eth_dev_count_avail();
    /* Allocates mempool to hold the mbufs. 8< */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
                                        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    /* >8 End of allocating mempool to hold mbuf. */

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Initializing all ports. 8< */
    RTE_ETH_FOREACH_DEV(portid)
    if (portid == 1 && port_init(portid, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n",
                 portid);
    /* >8 End of initializing all ports. */

    if (rte_lcore_count() > 1)
        printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

    /* Call lcore_main on the main core only. Called on single lcore. 8< */
    // initialize mutex
    for (int i = 0; i < flow_num; i++) {
        if (pthread_mutex_init(&window_info_mutex[i], NULL) != 0) { 
            printf("\n mutex init has failed\n"); 
            return ; 
        }    
    }
    pthread_create(&ptid[flow_num], NULL, &listen_ack, NULL);
    start_time = raw_time();
    int port_id_list[10];
    for (int i = 0; i < flow_num; i++) {
        port_id_list[i] = i;
        pthread_create(&ptid[i], NULL, lcore_main, (void*)&port_id_list[i]);
    }
    for (int i = 0; i < flow_num; i++) {
        pthread_join(ptid[i], NULL);
        pthread_mutex_destroy(&window_info_mutex[i]);
    }
    pthread_detach(ptid[flow_num]);
    uint64_t used_time = time_now(start_time);
    printf("used_time: %" PRIu64 "\n", used_time);
    /* >8 End of called on single lcore. */
    printf("Done!\n");
    /* clean up the EAL */
    rte_eth_dev_stop(1);
    rte_eth_dev_close(1);
    rte_eal_cleanup();

    return 0;
}