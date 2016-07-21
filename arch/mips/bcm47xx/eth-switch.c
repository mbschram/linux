#include "bcm47xx_private.h"

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ssb/ssb.h>
#include <linux/phy.h>
#include <linux/platform_data/b53.h>
#include <linux/brcmphy.h>

#include <bcm47xx.h>
#include <bcm47xx_board.h>

static struct b53_platform_data b53_pdata;

/* b44.c registers a bus with just integers */
static struct mdio_board_info b53_board_info[] = {
	{
		.bus_id	= "0",
		.mdio_addr = BRCM_PSEUDO_PHY_ADDR,
		.platform_data = &b53_pdata,
	},
	{
		.bus_id	= "1",
		.mdio_addr = BRCM_PSEUDO_PHY_ADDR,
		.platform_data = &b53_pdata,
	},
};

#ifdef CONFIG_BCM47XX_SSB
static struct device *bcm47xx_get_ethernet_dev(void)
{
	struct ssb_bus *bus = &bcm47xx_bus.ssb;
	struct ssb_device *dev;
	unsigned int i;

	for (i = 0; i < bus->nr_devices; i++) {
		dev = &bus->devices[dev_i];

		switch (dev->id.coreid) {
		case SSB_DEV_ETHERNET_GBIT:
		case SSB_DEV_ETHERNET:
			return dev->dev;
		}
	}

	return NULL;
}
#endif
#ifdef CONFIG_BCM47XX_BCMA
static struct device *bcm47xx_get_ethernet_dev(void)
{
	return NULL;
}
#endif

struct b53_switch_info {
	int cpu_port;
	int last_port;
	int first_port;
};

static int __init bcm47xx_parse_vlanports(struct b53_switch_info *info)
{
	char fp, *cp = NULL, *p = NULL, *lp = NULL;
	char vlan_ports[3][100];
	unsigned int vid;

	bcm47xx_nvram_getenv("vlan0ports", vlan_ports[0], sizeof(vlan_ports[0]));
	bcm47xx_nvram_getenv("vlan1ports", vlan_ports[1], sizeof(vlan_ports[1]));
	bcm47xx_nvram_getenv("vlan2ports", vlan_ports[2], sizeof(vlan_ports[2]));

	/* We have 4 different flavors:
	 * vlan[0-1]ports includes ports 0 - 3, and CPU port either 5 or 8
	 * vlan[1-2]ports will include 4 and 5 or 8
	 * vlan[0-1]ports includes ports 1 - 4, and CPU port either 5 or 8
	 * vlan[1-2]ports will include 0 - 5 or 8
	 *
	 * so here we go:
	 * - first we find the CPU port (last digit in the string)
	 * - then we find the number of the first port
	 * - profit!
	 */
	for (vid = 0; vid < 3; vid++) {
		cp = strrchr(vlan_ports[vid], ' ') + 1;
		if (!cp)
			continue;

		if (vid != 2) {
			p = strrchr(vlan_ports[vid + 1], ' ') + 1;
			if (strcmp(cp, p)) {
				pr_err("%s vs %s: disjoint!?\n", cp, p);
				continue;
			}
		}

		fp = vlan_ports[vid][0];

		lp = strrchr((const char *)(vlan_ports[vid] - cp), ' ') + 1;
	}

	if (!cp || !fp || !lp)
		return -EINVAL;

	info->cpu_port = kstrtol(cp, 10, NULL);
	info->first_port = kstrtol(&fp, 10, NULL);
	info->last_port = kstrtol(lp, 10, NULL);

	return 0;
}

static void __init bcm47xx_setup_b53_pdata(void)
{
	struct dsa2_platform_data *pd = &b53_pdata.dsa_pd;
	struct b53_switch_info info;
	unsigned int port;
	int ret;

	ret = bcm47xx_parse_vlanports(&info);
	if (ret)
		return;

	pr_info("%s: CPU@%d, first: %d, last: %d\n",
		__func__, info.cpu_port, info.first_port, info.last_port);

	/* Assign the CPU port */
	pd->ports[info.cpu_port].name = "cpu";

	/* If first port is not 0, then WAN is 0 */
	if (info.first_port != 0)
		pd->ports[0].name = "wan";
	else
		pd->ports[info.last_port + 1].name = "wan";

	for (port = info.first_port; port < info.last_port + 1; port++)
		pd->ports[port].name = "lan%d";

	b53_pdata.enabled_ports = GENMASK(info.last_port + 1, info.first_port) |
				  BIT(info.cpu_port);

	pd->netdev = bcm47xx_get_ethernet_dev();
}

void __init bcm47xx_ethernet_switch_register(void)
{
	enum bcm47xx_board board = bcm47xx_board_get();

	switch (board) {
	case BCM47XX_BOARD_LINKSYS_E3000V1:
		b53_pdata.dsa_pd.ports[8].name = "cpu";
		break;

	default:
		bcm47xx_setup_b53_pdata();
		break;
	}

	mdiobus_register_board_info(b53_board_info, ARRAY_SIZE(b53_board_info));
}
