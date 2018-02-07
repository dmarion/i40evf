/*******************************************************************************
 *
 * Intel Ethernet Controller XL710 Family Linux Virtual Function Driver
 * Copyright(c) 2013 - 2015 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 ******************************************************************************/

/* ethtool support for i40evf */
#include "i40evf.h"

#ifdef SIOCETHTOOL
#include <linux/uaccess.h>

#ifndef ETH_GSTRING_LEN
#define ETH_GSTRING_LEN 32
#endif

#ifdef ETHTOOL_OPS_COMPAT
#include "kcompat_ethtool.c"
#endif

struct i40evf_stats {
	char stat_string[ETH_GSTRING_LEN];
	int stat_offset;
};

#define I40EVF_STAT(_name, _stat) { \
	.stat_string = _name, \
	.stat_offset = offsetof(struct i40evf_adapter, _stat) \
}

/* All stats are u64, so we don't need to track the size of the field. */
static const struct i40evf_stats i40evf_gstrings_stats[] = {
	I40EVF_STAT("rx_bytes", current_stats.rx_bytes),
	I40EVF_STAT("rx_unicast", current_stats.rx_unicast),
	I40EVF_STAT("rx_multicast", current_stats.rx_multicast),
	I40EVF_STAT("rx_broadcast", current_stats.rx_broadcast),
	I40EVF_STAT("rx_discards", current_stats.rx_discards),
	I40EVF_STAT("rx_unknown_protocol", current_stats.rx_unknown_protocol),
	I40EVF_STAT("tx_bytes", current_stats.tx_bytes),
	I40EVF_STAT("tx_unicast", current_stats.tx_unicast),
	I40EVF_STAT("tx_multicast", current_stats.tx_multicast),
	I40EVF_STAT("tx_broadcast", current_stats.tx_broadcast),
	I40EVF_STAT("tx_discards", current_stats.tx_discards),
	I40EVF_STAT("tx_errors", current_stats.tx_errors),
#ifdef I40E_ADD_PROBES
	I40EVF_STAT("tx_tcp_segments", tcp_segs),
	I40EVF_STAT("tx_tcp_cso", tx_tcp_cso),
	I40EVF_STAT("tx_udp_cso", tx_udp_cso),
	I40EVF_STAT("tx_sctp_cso", tx_sctp_cso),
	I40EVF_STAT("tx_ip4_cso", tx_ip4_cso),
	I40EVF_STAT("rx_tcp_cso", rx_tcp_cso),
	I40EVF_STAT("rx_udp_cso", rx_udp_cso),
	I40EVF_STAT("rx_sctp_cso", rx_sctp_cso),
	I40EVF_STAT("rx_ip4_cso", rx_ip4_cso),
	I40EVF_STAT("rx_tcp_cso_error", rx_tcp_cso_err),
	I40EVF_STAT("rx_udp_cso_error", rx_udp_cso_err),
	I40EVF_STAT("rx_sctp_cso_error", rx_sctp_cso_err),
	I40EVF_STAT("rx_ip4_cso_error", rx_ip4_cso_err),
#endif
};

#define I40EVF_GLOBAL_STATS_LEN ARRAY_SIZE(i40evf_gstrings_stats)
#define I40EVF_QUEUE_STATS_LEN(_dev) \
	(((struct i40evf_adapter *)\
		netdev_priv(_dev))->num_active_queues \
		  * 2 * (sizeof(struct i40e_queue_stats) / sizeof(u64)))
#define I40EVF_STATS_LEN(_dev) \
	(I40EVF_GLOBAL_STATS_LEN + I40EVF_QUEUE_STATS_LEN(_dev))

/**
 * i40evf_get_settings - Get Link Speed and Duplex settings
 * @netdev: network interface device structure
 * @ecmd: ethtool command
 *
 * Reports speed/duplex settings. Because this is a VF, we don't know what
 * kind of link we really have, so we fake it.
 **/
static int i40evf_get_settings(struct net_device *netdev,
			       struct ethtool_cmd *ecmd)
{
	/* In the future the VF will be able to query the PF for
	 * some information - for now use a dummy value
	 */
	ecmd->supported = 0;
	ecmd->autoneg = AUTONEG_DISABLE;
	ecmd->transceiver = XCVR_DUMMY1;
	ecmd->port = PORT_NONE;

	return 0;
}

/**
 * i40evf_get_sset_count - Get length of string set
 * @netdev: network interface device structure
 * @sset: id of string set
 *
 * Reports size of string table. This driver only supports
 * strings for statistics.
 **/
static int i40evf_get_sset_count(struct net_device *netdev, int sset)
{
	if (sset == ETH_SS_STATS)
		return I40EVF_STATS_LEN(netdev);
	else
		return -EINVAL;
}

/**
 * i40evf_get_ethtool_stats - report device statistics
 * @netdev: network interface device structure
 * @stats: ethtool statistics structure
 * @data: pointer to data buffer
 *
 * All statistics are added to the data buffer as an array of u64.
 **/
static void i40evf_get_ethtool_stats(struct net_device *netdev,
				     struct ethtool_stats *stats, u64 *data)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);
	int i, j;
	char *p;

	for (i = 0; i < I40EVF_GLOBAL_STATS_LEN; i++) {
		p = (char *)adapter + i40evf_gstrings_stats[i].stat_offset;
		data[i] =  *(u64 *)p;
	}
	for (j = 0; j < adapter->num_active_queues; j++) {
		data[i++] = adapter->tx_rings[j]->stats.packets;
		data[i++] = adapter->tx_rings[j]->stats.bytes;
	}
	for (j = 0; j < adapter->num_active_queues; j++) {
		data[i++] = adapter->rx_rings[j]->stats.packets;
		data[i++] = adapter->rx_rings[j]->stats.bytes;
	}
}

/**
 * i40evf_get_strings - Get string set
 * @netdev: network interface device structure
 * @sset: id of string set
 * @data: buffer for string data
 *
 * Builds stats string table.
 **/
static void i40evf_get_strings(struct net_device *netdev, u32 sset, u8 *data)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);
	u8 *p = data;
	int i;

	if (sset == ETH_SS_STATS) {
		for (i = 0; i < I40EVF_GLOBAL_STATS_LEN; i++) {
			memcpy(p, i40evf_gstrings_stats[i].stat_string,
			       ETH_GSTRING_LEN);
			p += ETH_GSTRING_LEN;
		}
		for (i = 0; i < adapter->num_active_queues; i++) {
			snprintf(p, ETH_GSTRING_LEN, "tx-%u.packets", i);
			p += ETH_GSTRING_LEN;
			snprintf(p, ETH_GSTRING_LEN, "tx-%u.bytes", i);
			p += ETH_GSTRING_LEN;
		}
		for (i = 0; i < adapter->num_active_queues; i++) {
			snprintf(p, ETH_GSTRING_LEN, "rx-%u.packets", i);
			p += ETH_GSTRING_LEN;
			snprintf(p, ETH_GSTRING_LEN, "rx-%u.bytes", i);
			p += ETH_GSTRING_LEN;
		}
	}
}

#ifndef HAVE_NDO_SET_FEATURES
/**
 * i40evf_get_rx_csum - Get RX checksum settings
 * @netdev: network interface device structure
 *
 * Returns true or false depending upon RX checksum enabled.
 **/
static u32 i40evf_get_rx_csum(struct net_device *netdev)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);

	return adapter->flags & I40EVF_FLAG_RX_CSUM_ENABLED;
}

/**
 * i40evf_set_rx_csum - Set RX checksum settings
 * @netdev: network interface device structure
 * @data: RX checksum setting (boolean)
 *
 **/
static int i40evf_set_rx_csum(struct net_device *netdev, u32 data)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);

	if (data)
		adapter->flags |= I40EVF_FLAG_RX_CSUM_ENABLED;
	else
		adapter->flags &= ~I40EVF_FLAG_RX_CSUM_ENABLED;

	return 0;
}

/**
 * i40evf_get_tx_csum - Get TX checksum settings
 * @netdev: network interface device structure
 *
 * Returns true or false depending upon TX checksum enabled.
 **/
static u32 i40evf_get_tx_csum(struct net_device *netdev)
{
	return (netdev->features & NETIF_F_IP_CSUM) != 0;
}

/**
 * i40evf_set_tx_csum - Set TX checksum settings
 * @netdev: network interface device structure
 * @data: TX checksum setting (boolean)
 *
 **/
static int i40evf_set_tx_csum(struct net_device *netdev, u32 data)
{
	if (data)
		netdev->features |= (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM);
	else
		netdev->features &= ~(NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM);

	return 0;
}

/**
 * i40evf_set_tso - Set TX segmentation offload settings
 * @netdev: network interface device structure
 * @data: TSO setting (boolean)
 *
 **/
static int i40evf_set_tso(struct net_device *netdev, u32 data)
{
	if (data) {
		netdev->features |= NETIF_F_TSO;
		netdev->features |= NETIF_F_TSO6;
	} else {
		netif_tx_stop_all_queues(netdev);
		netdev->features &= ~NETIF_F_TSO;
		netdev->features &= ~NETIF_F_TSO6;
#ifndef HAVE_NETDEV_VLAN_FEATURES
		/* disable TSO on all VLANs if they're present */
		if (adapter->vsi.vlgrp) {
			struct i40evf_adapter *adapter = netdev_priv(netdev);
			struct vlan_group *vlgrp = adapter->vsi.vlgrp;
			struct net_device *v_netdev;
			int i;

			for (i = 0; i < VLAN_GROUP_ARRAY_LEN; i++) {
				v_netdev = vlan_group_get_device(vlgrp, i);
				if (v_netdev) {
					v_netdev->features &= ~NETIF_F_TSO;
					v_netdev->features &= ~NETIF_F_TSO6;
					vlan_group_set_device(vlgrp, i,
							      v_netdev);
				}
			}
		}
#endif
		netif_tx_start_all_queues(netdev);
	}
	return 0;
}

#endif /* HAVE_NDO_SET_FEATURES */
/**
 * i40evf_get_msglevel - Get debug message level
 * @netdev: network interface device structure
 *
 * Returns current debug message level.
 **/
static u32 i40evf_get_msglevel(struct net_device *netdev)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);

	return adapter->msg_enable;
}

/**
 * i40evf_set_msglevel - Set debug message level
 * @netdev: network interface device structure
 * @data: message level
 *
 * Set current debug message level. Higher values cause the driver to
 * be noisier.
 **/
static void i40evf_set_msglevel(struct net_device *netdev, u32 data)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);

	if (I40E_DEBUG_USER & data)
		adapter->hw.debug_mask = data;
	adapter->msg_enable = data;
}

/**
 * i40evf_get_drvinfo - Get driver info
 * @netdev: network interface device structure
 * @drvinfo: ethool driver info structure
 *
 * Returns information about the driver and device for display to the user.
 **/
static void i40evf_get_drvinfo(struct net_device *netdev,
			       struct ethtool_drvinfo *drvinfo)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);

	strlcpy(drvinfo->driver, i40evf_driver_name, 32);
	strlcpy(drvinfo->version, i40evf_driver_version, 32);
	strlcpy(drvinfo->fw_version, "N/A", 4);
	strlcpy(drvinfo->bus_info, pci_name(adapter->pdev), 32);
}

/**
 * i40evf_get_ringparam - Get ring parameters
 * @netdev: network interface device structure
 * @ring: ethtool ringparam structure
 *
 * Returns current ring parameters. TX and RX rings are reported separately,
 * but the number of rings is not reported.
 **/
static void i40evf_get_ringparam(struct net_device *netdev,
				 struct ethtool_ringparam *ring)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);

	ring->rx_max_pending = I40EVF_MAX_RXD;
	ring->tx_max_pending = I40EVF_MAX_TXD;
	ring->rx_pending = adapter->rx_desc_count;
	ring->tx_pending = adapter->tx_desc_count;
}

/**
 * i40evf_set_ringparam - Set ring parameters
 * @netdev: network interface device structure
 * @ring: ethtool ringparam structure
 *
 * Sets ring parameters. TX and RX rings are controlled separately, but the
 * number of rings is not specified, so all rings get the same settings.
 **/
static int i40evf_set_ringparam(struct net_device *netdev,
				struct ethtool_ringparam *ring)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);
	u32 new_rx_count, new_tx_count;

	if ((ring->rx_mini_pending) || (ring->rx_jumbo_pending))
		return -EINVAL;

	new_tx_count = clamp_t(u32, ring->tx_pending,
			       I40EVF_MIN_TXD,
			       I40EVF_MAX_TXD);
	new_tx_count = ALIGN(new_tx_count, I40EVF_REQ_DESCRIPTOR_MULTIPLE);

	new_rx_count = clamp_t(u32, ring->rx_pending,
			       I40EVF_MIN_RXD,
			       I40EVF_MAX_RXD);
	new_rx_count = ALIGN(new_rx_count, I40EVF_REQ_DESCRIPTOR_MULTIPLE);

	/* if nothing to do return success */
	if ((new_tx_count == adapter->tx_desc_count) &&
	    (new_rx_count == adapter->rx_desc_count))
		return 0;

	adapter->tx_desc_count = new_tx_count;
	adapter->rx_desc_count = new_rx_count;

	if (netif_running(netdev)) {
		adapter->flags |= I40EVF_FLAG_RESET_NEEDED;
		schedule_work(&adapter->reset_task);
	}

	return 0;
}

/**
 * i40evf_get_coalesce - Get interrupt coalescing settings
 * @netdev: network interface device structure
 * @ec: ethtool coalesce structure
 *
 * Returns current coalescing settings. This is referred to elsewhere in the
 * driver as Interrupt Throttle Rate, as this is how the hardware describes
 * this functionality.
 **/
static int i40evf_get_coalesce(struct net_device *netdev,
			       struct ethtool_coalesce *ec)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);
	struct i40e_vsi *vsi = &adapter->vsi;

	ec->tx_max_coalesced_frames = vsi->work_limit;
	ec->rx_max_coalesced_frames = vsi->work_limit;

	if (ITR_IS_DYNAMIC(vsi->rx_itr_setting))
		ec->use_adaptive_rx_coalesce = 1;

	if (ITR_IS_DYNAMIC(vsi->tx_itr_setting))
		ec->use_adaptive_tx_coalesce = 1;

	ec->rx_coalesce_usecs = vsi->rx_itr_setting & ~I40E_ITR_DYNAMIC;
	ec->tx_coalesce_usecs = vsi->tx_itr_setting & ~I40E_ITR_DYNAMIC;

	return 0;
}

/**
 * i40evf_set_coalesce - Set interrupt coalescing settings
 * @netdev: network interface device structure
 * @ec: ethtool coalesce structure
 *
 * Change current coalescing settings.
 **/
static int i40evf_set_coalesce(struct net_device *netdev,
			       struct ethtool_coalesce *ec)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);
	struct i40e_hw *hw = &adapter->hw;
	struct i40e_vsi *vsi = &adapter->vsi;
	struct i40e_q_vector *q_vector;
	int i;

	if (ec->tx_max_coalesced_frames_irq || ec->rx_max_coalesced_frames_irq)
		vsi->work_limit = ec->tx_max_coalesced_frames_irq;

	if ((ec->rx_coalesce_usecs >= (I40E_MIN_ITR << 1)) &&
	    (ec->rx_coalesce_usecs <= (I40E_MAX_ITR << 1)))
		vsi->rx_itr_setting = ec->rx_coalesce_usecs;
	else
		return -EINVAL;

	if ((ec->tx_coalesce_usecs >= (I40E_MIN_ITR << 1)) &&
	    (ec->tx_coalesce_usecs <= (I40E_MAX_ITR << 1)))
		vsi->tx_itr_setting = ec->tx_coalesce_usecs;
	else
		return -EINVAL;

	if (ec->use_adaptive_rx_coalesce)
		vsi->rx_itr_setting |= I40E_ITR_DYNAMIC;
	else
		vsi->rx_itr_setting &= ~I40E_ITR_DYNAMIC;

	if (ec->use_adaptive_tx_coalesce)
		vsi->tx_itr_setting |= I40E_ITR_DYNAMIC;
	else
		vsi->tx_itr_setting &= ~I40E_ITR_DYNAMIC;

	for (i = 0; i < adapter->num_msix_vectors - NONQ_VECS; i++) {
		q_vector = adapter->q_vector[i];
		q_vector->rx.itr = ITR_TO_REG(vsi->rx_itr_setting);
		wr32(hw, I40E_VFINT_ITRN1(0, i), q_vector->rx.itr);
		q_vector->tx.itr = ITR_TO_REG(vsi->tx_itr_setting);
		wr32(hw, I40E_VFINT_ITRN1(1, i), q_vector->tx.itr);
		i40e_flush(hw);
	}

	return 0;
}

#ifdef ETHTOOL_GRXRINGS
/**
 * i40e_get_rss_hash_opts - Get RSS hash Input Set for each flow type
 * @adapter: board private structure
 * @cmd: ethtool rxnfc command
 *
 * Returns Success if the flow is supported, else Invalid Input.
 **/
static int i40evf_get_rss_hash_opts(struct i40evf_adapter *adapter,
				    struct ethtool_rxnfc *cmd)
{
	struct i40e_hw *hw = &adapter->hw;
	u64 hena = (u64)rd32(hw, I40E_VFQF_HENA(0)) |
		   ((u64)rd32(hw, I40E_VFQF_HENA(1)) << 32);

	/* We always hash on IP src and dest addresses */
	cmd->data = RXH_IP_SRC | RXH_IP_DST;

	switch (cmd->flow_type) {
	case TCP_V4_FLOW:
		if (hena & BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_TCP))
			cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
		break;
	case UDP_V4_FLOW:
		if (hena & BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_UDP))
			cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
		break;

	case SCTP_V4_FLOW:
	case AH_ESP_V4_FLOW:
	case AH_V4_FLOW:
	case ESP_V4_FLOW:
	case IPV4_FLOW:
		break;

	case TCP_V6_FLOW:
		if (hena & BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_TCP))
			cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
		break;
	case UDP_V6_FLOW:
		if (hena & BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_UDP))
			cmd->data |= RXH_L4_B_0_1 | RXH_L4_B_2_3;
		break;

	case SCTP_V6_FLOW:
	case AH_ESP_V6_FLOW:
	case AH_V6_FLOW:
	case ESP_V6_FLOW:
	case IPV6_FLOW:
		break;
	default:
		cmd->data = 0;
		return -EINVAL;
	}

	return 0;
}

/**
 * i40evf_get_rxnfc - command to get RX flow classification rules
 * @netdev: network interface device structure
 * @cmd: ethtool rxnfc command
 *
 * Returns Success if the command is supported.
 **/
static int i40evf_get_rxnfc(struct net_device *netdev,
			    struct ethtool_rxnfc *cmd,
#ifdef HAVE_ETHTOOL_GET_RXNFC_VOID_RULE_LOCS
			    void *rule_locs)
#else
			    u32 *rule_locs)
#endif
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);
	int ret = -EOPNOTSUPP;

	switch (cmd->cmd) {
	case ETHTOOL_GRXRINGS:
		cmd->data = adapter->num_active_queues;
		ret = 0;
		break;
	case ETHTOOL_GRXFH:
		ret = i40evf_get_rss_hash_opts(adapter, cmd);
		break;
	default:
		break;
	}

	return ret;
}

/**
 * i40evf_set_rss_hash_opt - Enable/Disable flow types for RSS hash
 * @adapter: board private structure
 * @cmd: ethtool rxnfc command
 *
 * Returns Success if the flow input set is supported.
 **/
static int i40evf_set_rss_hash_opt(struct i40evf_adapter *adapter,
				   struct ethtool_rxnfc *nfc)
{
	struct i40e_hw *hw = &adapter->hw;
	u64 hena = (u64)rd32(hw, I40E_VFQF_HENA(0)) |
		   ((u64)rd32(hw, I40E_VFQF_HENA(1)) << 32);

	/* RSS does not support anything other than hashing
	 * to queues on src and dst IPs and ports
	 */
	if (nfc->data & ~(RXH_IP_SRC | RXH_IP_DST |
			  RXH_L4_B_0_1 | RXH_L4_B_2_3))
		return -EINVAL;

	/* We need at least the IP SRC and DEST fields for hashing */
	if (!(nfc->data & RXH_IP_SRC) ||
	    !(nfc->data & RXH_IP_DST))
		return -EINVAL;

	switch (nfc->flow_type) {
	case TCP_V4_FLOW:
		switch (nfc->data & (RXH_L4_B_0_1 | RXH_L4_B_2_3)) {
		case 0:
			hena &= ~BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_TCP);
			break;
		case (RXH_L4_B_0_1 | RXH_L4_B_2_3):
			hena |= BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_TCP);
			break;
		default:
			return -EINVAL;
		}
		break;
	case TCP_V6_FLOW:
		switch (nfc->data & (RXH_L4_B_0_1 | RXH_L4_B_2_3)) {
		case 0:
			hena &= ~BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_TCP);
			break;
		case (RXH_L4_B_0_1 | RXH_L4_B_2_3):
			hena |= BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_TCP);
			break;
		default:
			return -EINVAL;
		}
		break;
	case UDP_V4_FLOW:
		switch (nfc->data & (RXH_L4_B_0_1 | RXH_L4_B_2_3)) {
		case 0:
			hena &= ~(BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_UDP) |
				  BIT_ULL(I40E_FILTER_PCTYPE_FRAG_IPV4));
			break;
		case (RXH_L4_B_0_1 | RXH_L4_B_2_3):
			hena |= (BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_UDP) |
				 BIT_ULL(I40E_FILTER_PCTYPE_FRAG_IPV4));
			break;
		default:
			return -EINVAL;
		}
		break;
	case UDP_V6_FLOW:
		switch (nfc->data & (RXH_L4_B_0_1 | RXH_L4_B_2_3)) {
		case 0:
			hena &= ~(BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_UDP) |
				  BIT_ULL(I40E_FILTER_PCTYPE_FRAG_IPV6));
			break;
		case (RXH_L4_B_0_1 | RXH_L4_B_2_3):
			hena |= (BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_UDP) |
				 BIT_ULL(I40E_FILTER_PCTYPE_FRAG_IPV6));
			break;
		default:
			return -EINVAL;
		}
		break;
	case AH_ESP_V4_FLOW:
	case AH_V4_FLOW:
	case ESP_V4_FLOW:
	case SCTP_V4_FLOW:
		if ((nfc->data & RXH_L4_B_0_1) ||
		    (nfc->data & RXH_L4_B_2_3))
			return -EINVAL;
		hena |= BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_OTHER);
		break;
	case AH_ESP_V6_FLOW:
	case AH_V6_FLOW:
	case ESP_V6_FLOW:
	case SCTP_V6_FLOW:
		if ((nfc->data & RXH_L4_B_0_1) ||
		    (nfc->data & RXH_L4_B_2_3))
			return -EINVAL;
		hena |= BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_OTHER);
		break;
	case IPV4_FLOW:
		hena |= (BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_OTHER) |
			 BIT_ULL(I40E_FILTER_PCTYPE_FRAG_IPV4));
		break;
	case IPV6_FLOW:
		hena |= (BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_OTHER) |
			 BIT_ULL(I40E_FILTER_PCTYPE_FRAG_IPV6));
		break;
	default:
		return -EINVAL;
	}

	wr32(hw, I40E_VFQF_HENA(0), (u32)hena);
	wr32(hw, I40E_VFQF_HENA(1), (u32)(hena >> 32));
	i40e_flush(hw);

	return 0;
}

/**
 * i40evf_set_rxnfc - command to set RX flow classification rules
 * @netdev: network interface device structure
 * @cmd: ethtool rxnfc command
 *
 * Returns Success if the command is supported.
 **/
static int i40evf_set_rxnfc(struct net_device *netdev,
			    struct ethtool_rxnfc *cmd)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);
	int ret = -EOPNOTSUPP;

	switch (cmd->cmd) {
	case ETHTOOL_SRXFH:
		ret = i40evf_set_rss_hash_opt(adapter, cmd);
		break;
	default:
		break;
	}

	return ret;
}

#endif /* ETHTOOL_GRXRINGS */
#ifdef ETHTOOL_GCHANNELS
/**
 * i40evf_get_channels: get the number of channels supported by the device
 * @netdev: network interface device structure
 * @ch: channel information structure
 *
 * For the purposes of our device, we only use combined channels, i.e. a tx/rx
 * queue pair. Report one extra channel to match our "other" MSI-X vector.
 **/
static void i40evf_get_channels(struct net_device *netdev,
				struct ethtool_channels *ch)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);

	/* Report maximum channels */
	ch->max_combined = adapter->num_active_queues;

	ch->max_other = NONQ_VECS;
	ch->other_count = NONQ_VECS;

	ch->combined_count = adapter->num_active_queues;
}

#endif /* ETHTOOL_GCHANNELS */
#define I40EVF_HLUT_ARRAY_SIZE ((I40E_VFQF_HLUT_MAX_INDEX + 1) * 4)
#define I40EVF_HKEY_ARRAY_SIZE ((I40E_VFQF_HKEY_MAX_INDEX + 1) * 4)
#if defined(ETHTOOL_GRSSH) && !defined(HAVE_ETHTOOL_GSRSSH)
/**
 * i40evf_get_rxfh_key_size - get the RSS hash key size
 * @netdev: network interface device structure
 *
 * Returns the table size.
 **/
static u32 i40evf_get_rxfh_key_size(struct net_device *netdev)
{
	return I40EVF_HKEY_ARRAY_SIZE;
}
#endif /* ETHTOOL_GRSSH */

#ifdef ETHTOOL_GRXFHINDIR
#ifdef HAVE_ETHTOOL_GRXFHINDIR_SIZE
/**
 * i40evf_get_rxfh_indir_size - get the rx flow hash indirection table size
 * @netdev: network interface device structure
 *
 * Returns the table size.
 **/
static u32 i40evf_get_rxfh_indir_size(struct net_device *netdev)
{
	return I40EVF_HLUT_ARRAY_SIZE;
}

#if defined(ETHTOOL_GRSSH) && !defined(HAVE_ETHTOOL_GSRSSH)
/**
 * i40evf_get_rxfh - get the rx flow hash indirection table
 * @netdev: network interface device structure
 * @indir: indirection table
 * @key: hash key
 *
 * Reads the indirection table directly from the hardware. Always returns 0.
 **/
#ifdef HAVE_RXFH_HASHFUNC
static int i40evf_get_rxfh(struct net_device *netdev, u32 *indir, u8 *key,
			   u8 *hfunc)
#else
static int i40evf_get_rxfh(struct net_device *netdev, u32 *indir, u8 *key)
#endif
#else
/**
 * i40evf_get_rxfh - get the rx flow hash indirection table
 * @netdev: network interface device structure
 * @indir: indirection table
 *
 * Reads the indirection table directly from the hardware. Always returns 0.
 **/
static int i40evf_get_rxfh_indir(struct net_device *netdev, u32 *indir)
#endif /* ETHTOOL_GRSSH */
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);
	struct i40e_hw *hw = &adapter->hw;
	u32 reg_val;
	int i, j;

#ifdef HAVE_RXFH_HASHFUNC
	if (hfunc)
		*hfunc = ETH_RSS_HASH_TOP;

#endif
	if (!indir)
		return 0;

	for (i = 0, j = 0; i <= I40E_VFQF_HLUT_MAX_INDEX; i++) {
		reg_val = rd32(hw, I40E_VFQF_HLUT(i));
		indir[j++] = reg_val & 0xff;
		indir[j++] = (reg_val >> 8) & 0xff;
		indir[j++] = (reg_val >> 16) & 0xff;
		indir[j++] = (reg_val >> 24) & 0xff;
	}
#if defined(ETHTOOL_GRSSH) && !defined(HAVE_ETHTOOL_GSRSSH)

	if (key) {
		for (i = 0, j = 0; i <= I40E_VFQF_HKEY_MAX_INDEX; i++) {
			reg_val = rd32(hw, I40E_VFQF_HKEY(i));
			key[j++] = (u8)(reg_val & 0xff);
			key[j++] = (u8)((reg_val >> 8) & 0xff);
			key[j++] = (u8)((reg_val >> 16) & 0xff);
			key[j++] = (u8)((reg_val >> 24) & 0xff);
		}
	}
#endif
	return 0;
}

#else
/**
 * i40evf_get_rxfh_indir - get the rx flow hash indirection table
 * @netdev: network interface device structure
 * @indir: indirection table
 *
 * Reads the indirection table directly from the hardware. Returns 0 or -EINVAL
 * if the supplied table isn't large enough.
 **/
static int i40evf_get_rxfh_indir(struct net_device *netdev,
				 struct ethtool_rxfh_indir *indir)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);
	struct i40e_hw *hw = &adapter->hw;
	u32 reg_val;
	int i, j;

	if (indir->size < I40EVF_HLUT_ARRAY_SIZE)
		return -EINVAL;

	for (i = 0, j = 0; i <= I40E_VFQF_HLUT_MAX_INDEX; i++) {
		reg_val = rd32(hw, I40E_VFQF_HLUT(i));
		indir->ring_index[j++] = reg_val & 0xff;
		indir->ring_index[j++] = (reg_val >> 8) & 0xff;
		indir->ring_index[j++] = (reg_val >> 16) & 0xff;
		indir->ring_index[j++] = (reg_val >> 24) & 0xff;
	}
	indir->size = ((I40E_VFQF_HLUT_MAX_INDEX + 1) * 4);
	return 0;
}
#endif /* HAVE_ETHTOOL_GRXFHINDIR_SIZE */
#endif /* ETHTOOL_GRXFHINDIR */
#ifdef ETHTOOL_SRXFHINDIR
#ifdef HAVE_ETHTOOL_GRXFHINDIR_SIZE
#if defined(ETHTOOL_SRSSH) && !defined(HAVE_ETHTOOL_GSRSSH)
/**
 * i40evf_set_rxfh - set the rx flow hash indirection table
 * @netdev: network interface device structure
 * @indir: indirection table
 * @key: hash key
 *
 * Returns -EINVAL if the table specifies an inavlid queue id, otherwise
 * returns 0 after programming the table.
 **/
#ifdef HAVE_RXFH_HASHFUNC
static int i40evf_set_rxfh(struct net_device *netdev, const u32 *indir,
			   const u8 *key, const u8 hfunc)
#else
static int i40evf_set_rxfh(struct net_device *netdev, const u32 *indir,
			   const u8 *key)
#endif
#else
/**
 * i40evf_set_rxfh_indir - set the rx flow hash indirection table
 * @netdev: network interface device structure
 * @indir: indirection table
 *
 * Returns -EINVAL if the table specifies an inavlid queue id, otherwise
 * returns 0 after programming the table.
 **/
static int i40evf_set_rxfh_indir(struct net_device *netdev, const u32 *indir)
#endif /* EHTTOOL_SRSSH */
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);
	struct i40e_hw *hw = &adapter->hw;
	u32 reg_val;
	int i, j;

#ifdef HAVE_RXFH_HASHFUNC
	if (hfunc != ETH_RSS_HASH_NO_CHANGE && hfunc != ETH_RSS_HASH_TOP)
		return -EOPNOTSUPP;

#endif
	if (!indir)
		return 0;

	/* Verify user input. */
	for (i = 0; i < I40EVF_HLUT_ARRAY_SIZE; i++) {
		if (indir[i] >= adapter->num_active_queues)
			return -EINVAL;
	}

	for (i = 0, j = 0; i <= I40E_VFQF_HLUT_MAX_INDEX; i++) {
		reg_val = indir[j++];
		reg_val |= indir[j++] << 8;
		reg_val |= indir[j++] << 16;
		reg_val |= indir[j++] << 24;
		wr32(hw, I40E_VFQF_HLUT(i), reg_val);
	}

#if defined(ETHTOOL_SRSSH) && !defined(HAVE_ETHTOOL_GSRSSH)
	if (key) {
		for (i = 0, j = 0; i <= I40E_VFQF_HKEY_MAX_INDEX; i++) {
			reg_val = key[j++];
			reg_val |= key[j++] << 8;
			reg_val |= key[j++] << 16;
			reg_val |= key[j++] << 24;
			wr32(hw, I40E_VFQF_HKEY(i), reg_val);
		}
	}
#endif
	return 0;
}
#else
/**
 * i40evf_set_rxfh_indir - set the rx flow hash indirection table
 * @netdev: network interface device structure
 * @indir: indirection table
 *
 * Returns -EINVAL if the table specifies an inavlid queue id, otherwise
 * returns 0 after programming the table.
 **/
static int i40evf_set_rxfh_indir(struct net_device *netdev,
				 const struct ethtool_rxfh_indir *indir)
{
	struct i40evf_adapter *adapter = netdev_priv(netdev);
	struct i40e_hw *hw = &adapter->hw;
	u32 reg_val;
	int i, j;

	if (indir->size < I40EVF_HLUT_ARRAY_SIZE)
		return -EINVAL;

	/* Verify user input. */
	for (i = 0; i < (I40E_VFQF_HLUT_MAX_INDEX + 1) * 4; i++) {
		if (indir->ring_index[i] >= adapter->num_active_queues)
			return -EINVAL;
	}

	for (i = 0, j = 0; i <= I40E_VFQF_HLUT_MAX_INDEX; i++) {
		reg_val = indir->ring_index[j++];
		reg_val |= indir->ring_index[j++] << 8;
		reg_val |= indir->ring_index[j++] << 16;
		reg_val |= indir->ring_index[j++] << 24;
		wr32(hw, I40E_VFQF_HLUT(i), reg_val);
	}

	return 0;
}
#endif /* HAVE_ETHTOOL_GRXFHINDIR_SIZE */
#endif /* ETHTOOL_SRXFHINDIR */

static const struct ethtool_ops i40evf_ethtool_ops = {
	.get_settings		= i40evf_get_settings,
	.get_drvinfo		= i40evf_get_drvinfo,
	.get_link		= ethtool_op_get_link,
	.get_ringparam		= i40evf_get_ringparam,
	.set_ringparam		= i40evf_set_ringparam,
#ifndef HAVE_NDO_SET_FEATURES
	.get_rx_csum		= i40evf_get_rx_csum,
	.set_rx_csum		= i40evf_set_rx_csum,
	.get_tx_csum		= i40evf_get_tx_csum,
	.set_tx_csum		= i40evf_set_tx_csum,
	.get_sg			= ethtool_op_get_sg,
	.set_sg			= ethtool_op_set_sg,
	.get_tso		= ethtool_op_get_tso,
	.set_tso		= i40evf_set_tso,
#endif /* HAVE_NDO_SET_FEATURES */
	.get_strings	= i40evf_get_strings,
	.get_ethtool_stats	= i40evf_get_ethtool_stats,
	.get_sset_count		= i40evf_get_sset_count,
	.get_msglevel		= i40evf_get_msglevel,
	.set_msglevel		= i40evf_set_msglevel,
	.get_coalesce		= i40evf_get_coalesce,
	.set_coalesce		= i40evf_set_coalesce,
#ifdef ETHTOOL_GRXRINGS
	.get_rxnfc		= i40evf_get_rxnfc,
	.set_rxnfc		= i40evf_set_rxnfc,
#endif
#if defined(ETHTOOL_SRSSH) && !defined(HAVE_ETHTOOL_GSRSSH)
	.get_rxfh_key_size	= i40evf_get_rxfh_key_size,
#endif
#ifndef HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT
#ifdef ETHTOOL_GRXFHINDIR
#ifdef HAVE_ETHTOOL_GRXFHINDIR_SIZE
	.get_rxfh_indir_size	= i40evf_get_rxfh_indir_size,
#endif /* HAVE_ETHTOOL_GRXFHINDIR_SIZE */
#ifdef ETHTOOL_GRSSH
	.get_rxfh		= i40evf_get_rxfh,
#else
	.get_rxfh_indir		= i40evf_get_rxfh_indir,
#endif /* ETHTOOL_GRSSH */
#endif /* ETHTOOL_GRXFHINDIR */
#ifdef ETHTOOL_SRXFHINDIR
#ifdef ETHTOOL_SRSSH
	.set_rxfh		= i40evf_set_rxfh,
#else
	.set_rxfh_indir		= i40evf_set_rxfh_indir,
#endif /* ETHTOOL_SRSSH */
#endif /* ETHTOOL_SRXFHINDIR */
#ifdef ETHTOOL_GCHANNELS
	.get_channels		= i40evf_get_channels,
#endif
#endif /* HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT */
};

#ifdef HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT
static const struct ethtool_ops_ext i40evf_ethtool_ops_ext = {
	.size			= sizeof(struct ethtool_ops_ext),
	.get_channels		= i40evf_get_channels,
	.get_rxfh_indir_size	= i40evf_get_rxfh_indir_size,
	.get_rxfh_indir		= i40evf_get_rxfh_indir,
	.set_rxfh_indir		= i40evf_set_rxfh_indir,
};
#endif /* HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT */

/**
 * i40evf_set_ethtool_ops - Initialize ethtool ops struct
 * @netdev: network interface device structure
 *
 * Sets ethtool ops struct in our netdev so that ethtool can call
 * our functions.
 **/
void i40evf_set_ethtool_ops(struct net_device *netdev)
{
	netdev->ethtool_ops = &i40evf_ethtool_ops;
#ifdef HAVE_RHEL6_ETHTOOL_OPS_EXT_STRUCT
	set_ethtool_ops_ext(netdev, &i40evf_ethtool_ops_ext);
#endif
}

#endif /* SIOCETHTOOL */
