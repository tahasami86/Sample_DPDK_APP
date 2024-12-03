
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
#include <math.h>
/*
 * RTE_LIBRTE_RING_DEBUG generates statistics of ring buffers. However, SEGV is occurred. (v16.07）
 * #define RTE_LIBRTE_RING_DEBUG
 */
#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_errno.h>
#include <rte_timer.h>
#include <rte_log.h>

volatile uint8_t quit_signal;
static uint64_t delayed_time_in_us;
static uint32_t nb_queue;

#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1


static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

static struct rte_eth_conf port_conf_default=  {
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = RTE_ETH_RSS_UDP,
		},
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
};

struct rte_mempool *pktmbuf_pool = NULL;

#define NUM_SEND_PKTS 512
#define NUM_MBUFS 4096
#define MEMPOOL_BUF_SIZE RTE_MBUF_DEFAULT_BUF_SIZE /* 2048 */
#define MEMPOOL_CACHE_SIZE 512
#define BURST_SIZE 32
#define PKT_BURST_TX 32

/* Rx Burst req Variables*/
struct rte_mbuf *rx_bufs[BURST_SIZE];
uint16_t nb_rx;
uint16_t buf_unsent;

/*Tc Burst Variables*/
struct rte_mbuf *send_buf[PKT_BURST_TX];
uint16_t nb_rx;
uint16_t tx_sent;
uint8_t dest_port;
uint16_t dis_buf;


/* Assigment of MAIN thread to a specific CPU core.*/

struct rte_ring *rx_to_worker;
struct rte_ring *rx_to_worker2;
struct rte_ring *worker_to_tx2;
struct rte_ring *worker_to_tx;



#define MAIN_CORE 0
#define   RX_CORE2           1 
#define   WORKER_THREAD      2   
#define   WORKER_THREAD2     3 

void display_all_core(void);
void display_main_core(void);
void display_last_core(void);
void print_usage(const char *prgname);
int parse_delayed(const char *q_arg);
int queue_parse(const char *q_arg);
int check_all_ports_link_status(uint8_t port_num);
int port_start(uint8_t port_id,struct rte_mempool *pktmbuf_pool,uint32_t queue_nb);
int rx_side(int portid);
int tx_side(void);

/*ring functions*/

int rx_side_ring(int port_id);
int worker_side (int port_id);
int tx_side_ring(int port_id);

/***********************/

#if 1
static struct rte_eth_rxconf rx_conf = {
	.rx_thresh = {                    /**< RX ring threshold registers. */
		.pthresh = 8,             /**< Ring prefetch threshold. */
		.hthresh = 8,             /**< Ring host threshold. */
		.wthresh = 0,             /**< Ring writeback threshold. */
	},
	.rx_free_thresh = 32,             /**< Drives the freeing of RX descriptors. */
	.rx_drop_en = 0,                  /**< Drop packets if no descriptors are available. */
	.rx_deferred_start = 0,           /**< Do not start queue with rte_eth_dev_start(). */
};

static struct rte_eth_txconf tx_conf = {
	.tx_thresh = {                    /**< TX ring threshold registers. */
		.pthresh = 32,
		.hthresh = 0,
		.wthresh = 0,
	},
	.tx_rs_thresh = 32,               /**< Drives the setting of RS bit on TXDs. */
	.tx_free_thresh = 32,             /**< Start freeing TX buffers if there are less free descriptors than this value. */
	.tx_deferred_start = 0,            /**< Do not start queue with rte_eth_dev_start(). */
};

#endif



/**
 * Get the last enabled lcore ID
 *
 * @return
 *   The last enabled lcore ID.
 */
static unsigned int
get_last_lcore_id(void)
{
	int i;

	for (i = RTE_MAX_LCORE - 1; i >= 0; i--)
		if (rte_lcore_is_enabled(i))
			return i;
	return 0;
}


/**
 * Get the previous enabled lcore ID
 * @param id
 *  The current lcore ID
 * @return
 *   The previous enabled lcore ID or the current lcore
 *   ID if it is the first available core.
 */
static unsigned int
get_previous_lcore_id(unsigned int id)
{
	int i;

	for (i = id - 1; i >= 0; i--)
		if (rte_lcore_is_enabled(i))
			return i;
	return id;
}



#if 1
int rx_side_ring(int port_id){
	// uint8_t nb_ports = rte_eth_dev_count_avail();
	struct rte_mbuf *rx_pkts[BURST_SIZE];
	int nb_rx_pkts;
	int numenq;

	RTE_LOG(DEBUG, APP, "%s() started on lcore %u\n", __func__, rte_lcore_id());
	while (!quit_signal)
	{
		
		nb_rx_pkts = rte_eth_rx_burst(port_id,0,rx_pkts,BURST_SIZE);

		if(unlikely(nb_rx_pkts == 0))
			continue;

		if (port_id == 0){
			numenq = rte_ring_enqueue_burst(rx_to_worker,(void *)rx_pkts,nb_rx_pkts,NULL);
		}
		else {
			numenq = rte_ring_enqueue_burst(rx_to_worker2,(void *)rx_pkts,nb_rx_pkts,NULL);
		}
		if (unlikely(numenq<nb_rx_pkts)){

			rte_pktmbuf_free_bulk(&rx_pkts[numenq],nb_rx_pkts - numenq);
		}		

	}

	return 0;
}

int worker_side(int port_id){
	uint16_t burst_size=0;
	//uint8_t nb_port= rte_eth_dev_count_avail();
	struct rte_mbuf *worker_pkts[BURST_SIZE];
	int i;
	unsigned lcore_id;
	int status;
	lcore_id =rte_lcore_id();
	RTE_LOG(DEBUG, APP, "Entering main worker on lcore %u\n", lcore_id);

	while (!quit_signal) {

		if(port_id == 0)
			burst_size = rte_ring_sc_dequeue_burst(rx_to_worker,(void *)worker_pkts,BURST_SIZE,NULL);
		else
			burst_size =  rte_ring_sc_dequeue_burst(rx_to_worker2,(void *)worker_pkts,BURST_SIZE,NULL);

		if (unlikely(burst_size == 0))
			continue;
				
		i=0;

		while (i !=burst_size)
		{
			if (__sync_bool_compare_and_swap((uint64_t *)&worker_pkts[i]->dynfield1[0], 0, 1)) 
			{
			if (port_id == 0){
				do{
				status = rte_ring_enqueue(worker_to_tx2,worker_pkts[i]);
				}while(status == -ENOBUFS);
			}
		
		else{
			do{
				status = rte_ring_enqueue(worker_to_tx,worker_pkts[i]);
			}while(status == -ENOBUFS);
			}

		}
		else {
			rte_pktmbuf_free(worker_pkts[i]);
		}
		 i++;
		// rte_delay_ms(delayed_time_in_us);
		}
		
	}


	return 0;
}

int tx_side_ring(int port_id){

	struct rte_mbuf *tx_pkts[PKT_BURST_TX];
	struct rte_ring **cring;
	unsigned lcore_id;
	uint32_t numdeq = 0;
	uint16_t sent;
	
	lcore_id = rte_lcore_id();
	RTE_LOG(DEBUG, APP, "Entering main tx loop on lcore %u portid %u\n", lcore_id, port_id);

	while (!quit_signal)
	{
		if(port_id == 0)
			cring=&worker_to_tx;
		else
			cring = &worker_to_tx2;

		numdeq = rte_ring_sc_dequeue_burst(*cring,(void *)tx_pkts,PKT_BURST_TX,NULL);

		if (unlikely(numdeq == 0))
			continue;

		sent=0;
		while(sent < numdeq){
			sent+= rte_eth_tx_burst(port_id,0,tx_pkts,numdeq);
		}
		RTE_LOG(DEBUG, APP, "demu sent nb_tx packets %u on portid %u\n", sent, port_id);

	}
	
	return 0;
}


#endif

static void
int_handler(int sig_num)
{
	printf("Exiting on signal %d\n", sig_num);
	quit_signal = 1;
}

void display_all_core(void){

    while(!quit_signal){
    //printf("%s() on lcore %u\n", __func__, lcore_id);
    printf("%s() on lcore %u\n", __func__, rte_lcore_id());
    rte_delay_ms((delayed_time_in_us));
    }

}


void display_main_core(void){
    	
    while(!quit_signal){
   	//printf("%s() on lcore %u\n", __func__, lcore_id);
    printf("%s() on lcore %u\n", __func__, rte_lcore_id());
    rte_delay_ms(delayed_time_in_us);
    }

}

void display_last_core(void){
    	
    while(!quit_signal){
  // 	printf("%s() on lcore %u\n", __func__, lcore_id);
    printf("%s() on lcore %u\n", __func__, rte_lcore_id());
    rte_delay_ms((delayed_time_in_us));
    }

}

static int
launch_core_loop(__attribute__((unused)) void *dummy){
	unsigned lcore_id;
    unsigned int last_lcore_id;

	lcore_id = rte_lcore_id();
    last_lcore_id   = get_last_lcore_id();

    if (lcore_id == MAIN_CORE) 
		rx_side_ring(0);							
    
	 if (lcore_id == RX_CORE2) 
		rx_side_ring(1);

    if (lcore_id == last_lcore_id) 
		tx_side_ring(0);									


	if (lcore_id == (last_lcore_id -1)) 
		tx_side_ring(1);

	if(lcore_id == WORKER_THREAD)
       worker_side(0);

    if(lcore_id == WORKER_THREAD2)
       worker_side(1);
	
	
    if (quit_signal)
        return 0;

    return 0;
}



int port_start(uint8_t port_id,struct rte_mempool *pktmbuf_pool,uint32_t queue_nb){
		int ret;

	/*Ethernet Address of Ports*/
	struct rte_ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];
	struct rte_eth_conf  port_conf = port_conf_default; 
 		RTE_LOG(INFO, APP, "Initializing port %u\n", (unsigned) port_id);

 		ret = rte_eth_dev_configure(port_id, 3, 3, &port_conf);
        if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
					ret, (unsigned) port_id);
		
		ret=rte_eth_macaddr_get(port_id,&ports_eth_addr[port_id]);
		
		RTE_LOG(INFO, APP, "Port %u, MAC address: " RTE_ETHER_ADDR_PRT_FMT " \n \n",
					port_id,RTE_ETHER_ADDR_BYTES(&ports_eth_addr[port_id]));

		for (uint32_t queueid = 0; queueid < queue_nb; queueid++) {
		/*inint one RX queue on each port*/
        ret=rte_eth_rx_queue_setup(port_id,0,nb_rxd,rte_eth_dev_socket_id(port_id),
                                    &rx_conf,pktmbuf_pool);
        if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
					ret, (unsigned) port_id);
		}
        
		//tx_conf = dev_info.default_txconf;
		tx_conf.offloads = port_conf.txmode.offloads;
        /* init one TX queue on each port */
		ret = rte_eth_tx_queue_setup(port_id, 0, nb_txd,
				rte_eth_dev_socket_id(port_id),
				&tx_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
					ret, (unsigned) port_id);

		/* Start device */
		ret = rte_eth_dev_start(port_id);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
					ret, (unsigned) port_id);

		rte_eth_promiscuous_enable(port_id);

		return 0;
}



void print_usage(const char *prgname){

printf("%s \n\n[EAL options] -- -d Delayed time [ms] (default is 0s)\n"
		"                 -q number of queues (max_queue < 1024)\n\n",
			prgname);

}



/*delay input option*/
int parse_delayed(const char *q_arg){

    unsigned long pm;
	char *end = NULL;

    /* parse hexadecimal string */
	pm = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}

int queue_parse(const char *q_arg){
	unsigned long pm;
	char *end = NULL;

	/*parse queue values*/
	pm = strtoul(q_arg,&end,10);

	if((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}

int check_all_ports_link_status(uint8_t port_num)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	struct rte_eth_link link;
	uint8_t port;

	RTE_LOG(INFO, APP, "Checking link status\n");

	for(port=0;port<port_num;port++){
		if(quit_signal)
			return 0;
		
		rte_eth_link_get(port, &link);

		if(link.link_status == RTE_ETH_LINK_DOWN){
			RTE_LOG(INFO, APP, "Port: %u Link DOWN\n", port);
			return -1;
		}

		RTE_LOG(INFO, APP, "  Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)port,
						(unsigned)link.link_speed,
						(link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX) ?
						("full-duplex") : ("half-duplex\n"));
	}
		printf("\n");
		return 0;
}


static int
parse_args_test(int argc, char **argv){
    int opt;
	char **argvopt;
    int longindex = 0;
    int64_t val;
	char *prgname = argv[0];
    const struct option longopts[] = {
		{0, 0, 0, 0}
	};

    argvopt = argv;
    
    while ((opt = getopt_long(argc, argvopt, "d:q:",
					longopts, &longindex)) != EOF) {

            switch (opt)
            {
            case 'd':
                /* code */
                val = parse_delayed(optarg);
                if(val<0){
                   printf("Invalid value: delayed time\n");
				   print_usage(prgname);
				   return -1; 
                }
                delayed_time_in_us=val;
                printf("the delayed time is %ld \n\n",delayed_time_in_us);
                break;
			case 'q':
				val = queue_parse(optarg);
				if(val< 0 || val > RTE_MAX_QUEUES_PER_PORT){
				  printf("Invalid queue value or queue value greater than 1024 \n\n");
				  print_usage(prgname);
				  return -1;
				}
				nb_queue = val;
				printf("The Queues are %d \n\n ",nb_queue);
				break;
            default:
                print_usage(prgname);
			    return -1;
                break;
            }
        }
        if (optind <= 1) {
		print_usage(prgname);
		return -1;
        }
    
    argv[optind-1] = prgname;
    optind = 1; /* reset getopt lib */
	return 0;
}

int main(int argc , char **argv){

    int ret=0;
    uint8_t nb_ports;
    uint8_t port_id;
    int lcore_id;
    
    /* Init EAL. 8< */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");
    
    argc -= ret;
	argv += ret;

    /* catch ctrl-c so we can print on exit */
	signal(SIGINT, int_handler);

    /* parse application arguments (after the EAL ones) */
	ret = parse_args_test(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,"Invalid DEMU arguments\n");

    pktmbuf_pool = rte_pktmbuf_pool_create("mbuf_pool",
			NUM_MBUFS  , MEMPOOL_CACHE_SIZE, 0, MEMPOOL_BUF_SIZE,
			rte_socket_id());
    
	if (pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "mbuf_pool create failed\n");

	rx_to_worker = rte_ring_create("rx_to_worker",NUM_MBUFS,rte_socket_id(),RING_F_SP_ENQ | RING_F_SC_DEQ);
	if(rx_to_worker == NULL)
		rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));

	worker_to_tx =rte_ring_create("worker_to_tx",NUM_SEND_PKTS,rte_socket_id(),RING_F_SP_ENQ | RING_F_SC_DEQ);
	if(worker_to_tx == NULL)
		rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));

	rx_to_worker2 = rte_ring_create("rx_to_worker2",NUM_MBUFS,rte_socket_id(),RING_F_SP_ENQ | RING_F_SC_DEQ);
	if(rx_to_worker2 == NULL)
			rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));

	worker_to_tx2 =rte_ring_create("worker_to_tx2",NUM_SEND_PKTS,rte_socket_id(),RING_F_SP_ENQ | RING_F_SC_DEQ);
	if(worker_to_tx2 == NULL)
		rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));



    nb_ports = rte_eth_dev_count_avail();

    if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

    /* Initialise each port */
    for(port_id=0;port_id < nb_ports;port_id++){
	   if(port_start(port_id,pktmbuf_pool,nb_queue)!=0)
			rte_exit(EXIT_FAILURE, "port init failed\n");
    }


	printf("the number of ports are %d \n\n",nb_ports);
	ret = check_all_ports_link_status(nb_ports);
	if (ret < 0)
		RTE_LOG(WARNING, APP, "Some ports are down\n");


    rte_eal_mp_remote_launch(launch_core_loop, NULL, CALL_MAIN);
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
            break;
	}

	for (port_id = 0; port_id < nb_ports; port_id++) {
		struct rte_eth_stats stats;
		RTE_LOG(INFO, APP, "Closing port %d\n", port_id);
		rte_eth_stats_get(port_id, &stats);
		RTE_LOG(INFO, APP, "port %d: in pkt: %ld out pkt: %ld in missed: %ld in errors: %ld out errors: %ld\n",
			port_id, stats.ipackets, stats.opackets, stats.imissed, stats.ierrors, stats.oerrors);
		rte_eth_dev_stop(port_id);
		rte_eth_dev_close(port_id);
	}
  	rte_eal_cleanup();

    return ret;
}
