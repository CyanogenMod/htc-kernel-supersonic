/**********************************************************************
 * Author: Cavium Networks
 *
 * Contact: support@caviumnetworks.com
 * This file is part of the OCTEON SDK
 *
 * Copyright (c) 2003-2007 Cavium Networks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 2, as
 * published by the Free Software Foundation.
 *
 * This file is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/.
 *
 * This file may also be available under a different license from Cavium.
 * Contact Cavium Networks for more information
**********************************************************************/

/*
 * External interface for the Cavium Octeon ethernet driver.
 */
#ifndef OCTEON_ETHERNET_H
#define OCTEON_ETHERNET_H

/**
 * This is the definition of the Ethernet driver's private
 * driver state stored in netdev_priv(dev).
 */
struct octeon_ethernet {
	/* PKO hardware output port */
	int port;
	/* PKO hardware queue for the port */
	int queue;
	/* Hardware fetch and add to count outstanding tx buffers */
	int fau;
	/*
	 * Type of port. This is one of the enums in
	 * cvmx_helper_interface_mode_t
	 */
	int imode;
	/* List of outstanding tx buffers per queue */
	struct sk_buff_head tx_free_list[16];
	/* Device statistics */
	struct net_device_stats stats
;	/* Generic MII info structure */
	struct mii_if_info mii_info;
	/* Last negotiated link state */
	uint64_t link_info;
	/* Called periodically to check link status */
	void (*poll) (struct net_device *dev);
};

/**
 * Free a work queue entry received in a intercept callback.
 *
 * @work_queue_entry:
 *               Work queue entry to free
 * Returns Zero on success, Negative on failure.
 */
int cvm_oct_free_work(void *work_queue_entry);

/**
 * Transmit a work queue entry out of the ethernet port. Both
 * the work queue entry and the packet data can optionally be
 * freed. The work will be freed on error as well.
 *
 * @dev:     Device to transmit out.
 * @work_queue_entry:
 *                Work queue entry to send
 * @do_free: True if the work queue entry and packet data should be
 *                freed. If false, neither will be freed.
 * @qos:     Index into the queues for this port to transmit on. This
 *                is used to implement QoS if their are multiple queues per
 *                port. This parameter must be between 0 and the number of
 *                queues per port minus 1. Values outside of this range will
 *                be change to zero.
 *
 * Returns Zero on success, negative on failure.
 */
int cvm_oct_transmit_qos(struct net_device *dev, void *work_queue_entry,
			 int do_free, int qos);

/**
 * Transmit a work queue entry out of the ethernet port. Both
 * the work queue entry and the packet data can optionally be
 * freed. The work will be freed on error as well. This simply
 * wraps cvmx_oct_transmit_qos() for backwards compatability.
 *
 * @dev:     Device to transmit out.
 * @work_queue_entry:
 *                Work queue entry to send
 * @do_free: True if the work queue entry and packet data should be
 *                freed. If false, neither will be freed.
 *
 * Returns Zero on success, negative on failure.
 */
static inline int cvm_oct_transmit(struct net_device *dev,
				   void *work_queue_entry, int do_free)
{
	return cvm_oct_transmit_qos(dev, work_queue_entry, do_free, 0);
}

extern int cvm_oct_rgmii_init(struct net_device *dev);
extern void cvm_oct_rgmii_uninit(struct net_device *dev);
extern int cvm_oct_rgmii_open(struct net_device *dev);
extern int cvm_oct_rgmii_stop(struct net_device *dev);

extern int cvm_oct_sgmii_init(struct net_device *dev);
extern void cvm_oct_sgmii_uninit(struct net_device *dev);
extern int cvm_oct_sgmii_open(struct net_device *dev);
extern int cvm_oct_sgmii_stop(struct net_device *dev);

extern int cvm_oct_spi_init(struct net_device *dev);
extern void cvm_oct_spi_uninit(struct net_device *dev);
extern int cvm_oct_xaui_init(struct net_device *dev);
extern void cvm_oct_xaui_uninit(struct net_device *dev);
extern int cvm_oct_xaui_open(struct net_device *dev);
extern int cvm_oct_xaui_stop(struct net_device *dev);

extern int cvm_oct_common_init(struct net_device *dev);
extern void cvm_oct_common_uninit(struct net_device *dev);

extern int always_use_pow;
extern int pow_send_group;
extern int pow_receive_group;
extern char pow_send_list[];
extern struct net_device *cvm_oct_device[];

#endif
