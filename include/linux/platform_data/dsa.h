#ifndef __DSA_PDATA_H
#define __DSA_PDATA_H

#include <linux/kernel.h>
#include <net/dsa.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>

struct dsa2_port_link {
	bool		valid;
	u32		index;
	unsigned int	port;
};

struct dsa2_port_data {
	/*
	 * Name of the ports, can be unique or a template (e.g: port%d)
	 */
	const char *name;

	/*
	 * PHY interface
	 */
	phy_interface_t phy_iface;

	/*
	 * Fixed PHY status information, if needed by the port (e.g: CPU port)
	 */
	struct fixed_phy_status	fixed_phy_status;
	int link_gpio;

	/*
	 * Links to other switches in the tree
	 */
	struct dsa2_port_link	links[DSA_MAX_SWITCHES];
};

struct dsa2_platform_data {
	/*
	 * Reference to a Linux network interface that connects
	 * to this switch chip.
	 */
	struct device	*netdev;

	/*
	 * Tree number
	 */
	u32 tree;

	/*
	 * Switch chip index within the tree
	 */
	u32 index;

	/*
	 * Ports layout and description
	 */
	struct dsa2_port_data ports[DSA_MAX_PORTS];
};

#endif /* __DSA_PDATA_H */
