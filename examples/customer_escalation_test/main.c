/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <unistd.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_ethdev.h>


typedef uint8_t  lcoreid_t;
typedef uint16_t portid_t;
typedef uint16_t queueid_t;
typedef uint16_t streamid_t;


/*
 * Configurable number of RX/TX queues.
 */
queueid_t nb_hairpinq; /**< Number of hairpin queues per port. */
queueid_t nb_rxq = 1; /**< Number of RX queues per port. */
queueid_t nb_txq = 1; /**< Number of TX queues per port. */
//struct rte_port *ports;

/*
 * NIC configuration
 *
 ***/
static struct rte_eth_conf port_conf = {
	.link_speeds = RTE_ETH_LINK_SPEED_AUTONEG,
	.rxmode = {
		.mq_mode	= RTE_ETH_MQ_RX_RSS,
		.mtu = 9600,
		.max_lro_pkt_size = 65536,
		.offloads = RTE_ETH_RX_OFFLOAD_CHECKSUM,
	},
	.txmode = {
		.mq_mode	= RTE_ETH_MQ_RX_RSS,
		.offloads = RTE_ETH_RX_OFFLOAD_CHECKSUM,
		.pvid = 12,
		.hw_vlan_insert_pvid = 1,
		.hw_vlan_reject_tagged = 1,
		.hw_vlan_reject_untagged = 1,
	},
	.lpbk_mode = 1,
	.dcb_capability_en = RTE_ETH_DCB_PG_SUPPORT | RTE_ETH_DCB_PFC_SUPPORT,
	.intr_conf = {
		.lsc = 1,
		.rmv = 0,
		.rxq = 1
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_hf = RTE_ETH_RSS_IPV6,
			.rss_key = NULL,
			.rss_key_len = 10
		},
		.vmdq_dcb_conf = {
			.nb_queue_pools = RTE_ETH_32_POOLS,
			.enable_default_pool = 0,
			.default_pool = 0,
			.nb_pool_maps = 0,
			.pool_map = {{0, 0},},
			.dcb_tc = {0},
		},
		.dcb_rx_conf = {
				.nb_tcs = RTE_ETH_4_TCS,
				/** Traffic class each UP mapped to. */
				.dcb_tc = {0},
		},
		.vmdq_rx_conf = {
			.nb_queue_pools = RTE_ETH_32_POOLS,
			.enable_default_pool = 0,
			.default_pool = 0,
			.nb_pool_maps = 0,
			.pool_map = {{0, 0},},
		},
	},
	.tx_adv_conf = {
		.vmdq_dcb_tx_conf = {
			.nb_queue_pools = RTE_ETH_32_POOLS,
			.dcb_tc = {0},
		},
		.dcb_tx_conf = {
			.dcb_tc = 4,
			.nb_tcs = RTE_ETH_DCB_NUM_USER_PRIORITIES
		},
		.vmdq_tx_conf = {
			.nb_queue_pools = RTE_ETH_16_POOLS
		}
	},
};



int
configure_all_devices()
{
	uint16_t pid;
	int ret;
	//struct rte_port *port;
	struct rte_eth_conf conf;

	//conf = port_conf;

	RTE_ETH_FOREACH_DEV(pid) {
		//port = &ports[pid];
		printf("SD DEBUG Configuring DEV: %d\n", pid);
		ret = rte_eth_dev_configure(pid, nb_rxq, nb_txq, &port_conf);
		if (ret < 0) {
			fprintf(stderr,
				"Failed to re-configure port %d, ret = %d.\n",
				pid, ret);
			return -1;
		}
	}

	return ret;

}

int
do_work()
{
	unsigned lcore_id;
	lcore_id = rte_lcore_id();
	printf("Starting work on core %u\n", lcore_id);
	for(int i=0; i<100; i++) {
		if (lcore_id == 1) {
			printf("Core %d re-configuring device %d\n", lcore_id, lcore_id);
			rte_eth_dev_configure(lcore_id, nb_rxq, nb_txq, &port_conf);
		} else {
			printf("core %d tick %d\n", lcore_id, i);
		}
		sleep(10);
	}
	return 0;
}

int
start_actual_work()
{
	unsigned lcore_id;

	// fork for each core a thread to run do_work()
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		rte_eal_remote_launch(do_work, NULL, lcore_id);
	}

	// call it on main lcore too
	do_work();

	rte_eal_mp_wait_lcore();
}

/* Initialization of Environment Abstraction Layer (EAL). 8< */
int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");
	/* >8 End of initialization of Environment Abstraction Layer */


	ret = configure_all_devices();
	if (ret < 0)
		rte_panic("SD DEBUG; critical error when configuring all devices");

	start_actual_work();

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}



// int func_work() {
// while (1) {
//         if rte_lcore_id () == 1 {
//                 rte_eth_dev_configure(...)
//               // B
//         sleep(3600)
//         } else {
//                 do_actual_work_on_other_cores() // this accesses the same NIC
//         }
// }

// }
// int main() {
//     rte_eal_init(*..)

//     for (...) {
//                 rte_eth_dev_configure(...) // <- Maybe they pass in something that the dev doesn't like
// // A
//         }
//     start_actual_work() // this will fork for each core a thread to run func_work

//     // sleect {}..

// }