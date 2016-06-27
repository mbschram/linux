/*
 * Distributed Switch Architecture loopback driver
 *
 * Copyright (C), Florian Fainelli <f.fainelli@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/platform_data/dsa.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>
#include <linux/export.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <net/dsa.h>

struct dsa_loop_priv {
	struct mii_bus	*bus;
	unsigned int	port_base;
};

static struct phy_device *phydevs[PHY_MAX_ADDR];

static enum dsa_tag_protocol dsa_loop_get_protocol(struct dsa_switch *ds)
{
	return DSA_TAG_PROTO_NONE;
}

#ifdef CONFIG_NET_DSA_LOOP_OLD_STYLE
static const char *dsa_loop_probe(struct device *dsa_dev,
				  struct device *host_dev,
				  int sw_addr, void **priv)
{
	struct dsa_loop_priv *ps;

	ps = devm_kzalloc(dsa_dev, sizeof(*ps), GFP_KERNEL);
	if (!ps)
		return NULL;

	ps->bus = dsa_host_dev_to_mii_bus(host_dev);
	if (!ps->bus)
		return NULL;

	*priv = ps;

	return "DSA loopback driver";
}
#else
#define dsa_loop_probe	NULL
#endif

static int dsa_loop_setup(struct dsa_switch *ds)
{
	return 0;
}

static int dsa_loop_set_addr(struct dsa_switch *ds, u8 *addr)
{
	return 0;
}

static int dsa_loop_phy_read(struct dsa_switch *ds, int port, int regnum)
{
	struct dsa_loop_priv *ps = ds->priv;
	struct mii_bus *bus = ps->bus;

	return mdiobus_read(bus, ps->port_base + port, regnum);
}

static int dsa_loop_phy_write(struct dsa_switch *ds, int port, int regnum, u16 value)
{
	struct dsa_loop_priv *ps = ds->priv;
	struct mii_bus *bus = ps->bus;

	return mdiobus_write(bus, ps->port_base + port, regnum, value);
}

static struct dsa_switch_ops dsa_loop_driver = {
	.get_tag_protocol = dsa_loop_get_protocol,
	.probe		= dsa_loop_probe,
	.setup		= dsa_loop_setup,
	.set_addr	= dsa_loop_set_addr,
	.phy_read	= dsa_loop_phy_read,
	.phy_write	= dsa_loop_phy_write,
};

#ifdef CONFIG_NET_DSA_LOOP_OLD_STYLE
static struct dsa_chip_data dsa_chip_data = {
	.port_names[DSA_MAX_PORTS - 1] = "cpu",
};

static struct dsa_platform_data dsa_pdata = {
	.nr_chips = 1,
	.chip = &dsa_chip_data,
};

static struct platform_device dsa_loop_pdevs[] = {
	{
		.name = "dsa",
		.id = 0,
		.dev.platform_data = &dsa_pdata,
	},
};

static int setup_dsa(struct net_device *dev)
{
	static struct fixed_phy_status status;
	unsigned int i;

	status.duplex = DUPLEX_FULL;
	status.link = 1;

	for (i = 0; i < DSA_MAX_PORTS - 1; i++) {
		if (i <= (DSA_MAX_PORTS - 1) / 2)
			status.speed = 100;
		else
			status.speed = 1000;
		phydevs[i] = fixed_phy_register(PHY_POLL, &status, -1, NULL);
		dsa_chip_data.port_names[i] = "port%d";
	}

	dsa_pdata.netdev = &dev->dev;
	dsa_chip_data.host_dev = platform_fmb_bus_get();

	register_switch_driver(&dsa_loop_driver);

	return 0;
}
#else

static DEFINE_SPINLOCK(phydevs_lock);

static int find_last_fixed_phy(void)
{
	unsigned int i;

	spin_lock(&phydevs_lock);
	for (i = ARRAY_SIZE(phydevs) - 1; i == 0; i--) {
		if (phydevs[i]) {
			spin_unlock(&phydevs_lock);
			return i;
		}
	}

	spin_unlock(&phydevs_lock);
	return 0;
}

/* Register DSA_MAX_SWITCHES - 1, since the first switch is hanging off
 * lo
 */
#define NUM_SWITCHES	DSA_MAX_SWITCHES - 2

static struct dsa2_platform_data dsa_pdata2[NUM_SWITCHES];

static struct platform_device dsa_loop_pdevs[NUM_SWITCHES];

static void dsa_fill_switch_link(struct dsa2_port_data *port,
				 struct dsa2_port_link *links,
				 unsigned int num_links)
{
	unsigned int i;

	port->name = "dsa";
	port->fixed_phy_status.speed = 1000;
	port->fixed_phy_status.duplex = DUPLEX_FULL;
	port->link_gpio = -1;

	for (i = 0; i < num_links; i++) {
		port->links[i].valid = true;
		port->links[i].index = links[i].index;
		port->links[i].port = links[i].port;
	}
}

/*
 * For each of these sub switches that we register, except for the first one,
 * we make Port 4 connect to the next switch and back
 */
static int dsa_loop_drv_probe(struct platform_device *pdev)
{
	struct dsa2_platform_data *pdata = &dsa_pdata2[pdev->id];
	struct fixed_phy_status status;
	struct dsa2_port_link links[2];
	struct dsa_loop_priv *ps;
	struct dsa_switch *ds;
	unsigned int i, port_base, num_links = 0;
	unsigned int num_ports = 2;
	int ret;

	ds = devm_kzalloc(&pdev->dev, sizeof(*ds) + sizeof(*ps), GFP_KERNEL);
	if (!ds)
		return -ENOMEM;

	dev_info(&pdev->dev, "allocated ds @ %p\n", ds);

	status.duplex = DUPLEX_FULL;
	status.speed = SPEED_100;
	status.link = 1;
	port_base = find_last_fixed_phy();

	pdata->tree = 0;
	pdata->index = pdev->id + 1;

	for (i = 0; i < num_ports; i++) {
		spin_lock(&phydevs_lock);
		phydevs[port_base + i] = fixed_phy_register(PHY_POLL, &status, -1, NULL);
		spin_unlock(&phydevs_lock);
		pdata->ports[i].name = kasprintf(GFP_KERNEL, "sw%dport%d", pdev->id + 1, i);
	}

	if (pdev->id == 0) {
		links[0].index = 2;
		links[0].port = 9;
		num_links++;
		dsa_fill_switch_link(&pdata->ports[5], links, num_links);
		links[0].index = 0;
		links[0].port = 5;
		dsa_fill_switch_link(&pdata->ports[6], links, num_links);
	} else {
		links[0].index = 1;
		links[0].port = 5;
		num_links++;
		links[1].index = 0;
		links[1].port = 5;
		num_links++;
		dsa_fill_switch_link(&pdata->ports[9], links, num_links);
	}

	ps = (struct dsa_loop_priv *)(ds + 1);

	ds->dev = &pdev->dev;
	ds->ops = &dsa_loop_driver;
	ds->priv = ps;
	ps->bus = dsa_host_dev_to_mii_bus(platform_fmb_bus_get());
	ps->port_base = port_base;
	ds->dev->platform_data = pdata;

	dev_set_drvdata(&pdev->dev, ds);

	dev_info(&pdev->dev, "Adding switch %d\n", pdev->id + 1);

	ret = dsa_register_switch(ds, ds->dev);
	if (ret)
		pr_err("failed to register switch!\n");

	return ret;
}

static int dsa_loop_drv_remove(struct platform_device *pdev)
{
	struct dsa_switch *ds = dev_get_drvdata(&pdev->dev);

	dev_info(&pdev->dev, "unregistering ds @ %p\n", ds);
	dsa_unregister_switch(ds);

	return 0;
}

static void dsa_loop_drv_shutdown(struct platform_device *pdev)
{
}

static struct platform_driver dsa_loop_drv = {
	.driver	= {
		.name	= "dsa-loop",
	},
	.probe	= dsa_loop_drv_probe,
	.remove	= dsa_loop_drv_remove,
	.shutdown = dsa_loop_drv_shutdown,
};

static struct dsa2_platform_data dsa_pdata = {
	.tree	= 0,
	.index	= 0,
	.ports	= {
		[5] = {
			.name	= "dsa",
			.links[0] = {
				.valid	= true,
				.index	= 1,
				.port	= 5,
			},
			.links[1] = {
				.valid	= true,
				.index	= 2,
				.port	= 6,
			},
			/*.fixed_phy_status = {*/
				/*.speed	= 1000,*/
				/*.duplex	= DUPLEX_FULL,*/
				/*.pause	= 1,*/
				/*.asym_pause = 1,*/
			/*},*/
			/*.link_gpio = -1,*/
		},
		[6] = {
			.name	= "cpu",
			.fixed_phy_status = {
				.speed	= 1000,
				.duplex	= DUPLEX_FULL,
				.pause	= 1,
				.asym_pause = 1,
			},
			.link_gpio = -1,
			.phy_iface = PHY_INTERFACE_MODE_RGMII,
		},
	},
};

static int setup_dsa(struct net_device *dev)
{
	struct fixed_phy_status status;
	struct dsa_loop_priv *ps;
	struct dsa_switch *ds;
	unsigned int port_base;
	unsigned int i;
	int ret;

	ds = kzalloc(sizeof(*ds) + sizeof(*ps), GFP_KERNEL);
	if (!ds)
		return -ENOMEM;

	dev->mem_end = (unsigned long)ds;

	status.duplex = DUPLEX_FULL;
	status.link = 1;
	status.speed = SPEED_1000;

	port_base = find_last_fixed_phy();

	for (i = 0; i < 4; i++) {
		spin_lock(&phydevs_lock);
		phydevs[port_base + i] = fixed_phy_register(PHY_POLL, &status, -1, NULL);
		spin_unlock(&phydevs_lock);
		dsa_pdata.ports[i].name = "port%d";
	}

	ps = (struct dsa_loop_priv *)(ds + 1);

	ds->dev = platform_fmb_bus_get();
	ds->ops = &dsa_loop_driver;
	ds->priv = ps;
	ps->bus = dsa_host_dev_to_mii_bus(ds->dev);
	ps->port_base = port_base;
	dsa_pdata.netdev = &dev->dev;
	ds->dev->platform_data = &dsa_pdata;

	ret = dsa_register_switch(ds, ds->dev);
	if (ret)
		pr_err("failed to register switch!\n");

	return ret;
}
#endif

static void unregister_fixed_phys(void)
{
	unsigned int i;

	for (i = 0; i < PHY_MAX_ADDR; i++)
		if (phydevs[i])
			fixed_phy_unregister(phydevs[i]);
}

int __init dsa_loop_init(void)
{
	struct net_device *dev;
	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(dsa_loop_pdevs); i++) {
		dsa_loop_pdevs[i].name = "dsa-loop";
		dsa_loop_pdevs[i].id = i;
		ret = platform_device_register(&dsa_loop_pdevs[i]);
		if (ret)
			return ret;
	}

#ifndef CONFIG_NET_DSA_LOOP_OLD_STYLE
	ret = platform_driver_register(&dsa_loop_drv);
	if (ret)
		goto out;
#endif
	dev = dev_get_by_name(&init_net, "lo");

	ret = setup_dsa(dev);
	if (ret)
		goto out2;

	return 0;
out2:
#ifndef CONFIG_NET_DSA_LOOP_OLD_STYLE
	platform_driver_unregister(&dsa_loop_drv);
out:
#endif
	for (i = 0; i < ARRAY_SIZE(dsa_loop_pdevs); i++)
		platform_device_unregister(&dsa_loop_pdevs[i]);
	return ret;
}
module_init(dsa_loop_init);

void __exit dsa_loop_exit(void)
{
	struct net_device *dev;
	struct dsa_switch *ds;
	unsigned int i;

	dev = dev_get_by_name(&init_net, "lo");
	ds = (struct dsa_switch *)dev->mem_end;

	dsa_unregister_switch(ds);

	for (i = 0; i < ARRAY_SIZE(dsa_loop_pdevs); i++)
		platform_device_unregister(&dsa_loop_pdevs[i]);
#ifndef CONFIG_NET_DSA_LOOP_OLD_STYLE
	platform_driver_unregister(&dsa_loop_drv);
#endif
	unregister_fixed_phys();
}
module_exit(dsa_loop_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florian Fainelli");
MODULE_DESCRIPTION("DSA loopback driver");

