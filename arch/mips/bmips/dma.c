/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2014 Kevin Cernekee <cernekee@gmail.com>
 */

#define pr_fmt(fmt)		"bmips-dma: " fmt

#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <dma-coherence.h>

/*
 * BCM338x has configurable address translation windows which allow the
 * peripherals' DMA addresses to be different from the Zephyr-visible
 * physical addresses.  e.g. usb_dma_addr = zephyr_pa ^ 0x08000000
 *
 * If the "brcm,ubus" node has a "dma-ranges" property we will enable this
 * translation globally using the provided information.  This implements a
 * very limited subset of "dma-ranges" support and it will probably be
 * replaced by a more generic version later.
 */

struct bmips_dma_range {
	u32			child_addr;
	u32			parent_addr;
	u32			size;
};

struct bmips_dma_ops {
	dma_addr_t (*phys_to_dma)(struct device *dev, phys_addr_t pa);
	unsigned long (*addr_to_phys)(struct device *dev, dma_addr_t dma);
};

static inline dma_addr_t bmips_noop_phys_to_dma(struct device *dev,
						phys_addr_t pa)
{
	return pa;
}

static inline unsigned long bmips_noop_addr_to_phys(struct device *dev,
						    dma_addr_t dma_addr)
{
	return dma_addr;
}

static struct bmips_dma_range *bmips_dma_ranges;

static struct bmips_dma_ops bmips_dma_ops = {
	.phys_to_dma = bmips_noop_phys_to_dma,
	.addr_to_phys = bmips_noop_addr_to_phys,
};

#define FLUSH_RAC		0x100

static dma_addr_t bmips_ubus_phys_to_dma(struct device *dev, phys_addr_t pa)
{
	struct bmips_dma_range *r;

	for (r = bmips_dma_ranges; r && r->size; r++) {
		if (pa >= r->child_addr &&
		    pa < (r->child_addr + r->size))
			return pa - r->child_addr + r->parent_addr;
	}
	return pa;
}

static unsigned long bmips_ubus_dma_addr_to_phys(struct device *dev,
						 dma_addr_t dma_addr)
{
	struct bmips_dma_range *r;

	for (r = bmips_dma_ranges; r && r->size; r++) {
		if (dma_addr >= r->parent_addr &&
		    dma_addr < (r->parent_addr + r->size))
			return dma_addr - r->parent_addr + r->child_addr;
	}
	return dma_addr;
}

static dma_addr_t bmips_phys_to_dma(struct device *dev, phys_addr_t pa)
{
	return bmips_dma_ops.phys_to_dma(dev, pa);
}

dma_addr_t plat_map_dma_mem(struct device *dev, void *addr, size_t size)
{
	return bmips_phys_to_dma(dev, virt_to_phys(addr));
}

dma_addr_t plat_map_dma_mem_page(struct device *dev, struct page *page)
{
	return bmips_phys_to_dma(dev, page_to_phys(page));
}

unsigned long plat_dma_addr_to_phys(struct device *dev, dma_addr_t dma_addr)
{
	return bmips_dma_ops.addr_to_phys(dev, dma_addr);
}

static int __init bmips_init_dma_ranges(void)
{
	struct device_node *np =
		of_find_compatible_node(NULL, NULL, "brcm,ubus");
	const __be32 *data;
	struct bmips_dma_range *r;
	int len;

	if (!np)
		return 0;

	data = of_get_property(np, "dma-ranges", &len);
	if (!data)
		goto out_good;

	len /= sizeof(*data) * 3;
	if (!len)
		goto out_bad;

	/* add a dummy (zero) entry at the end as a sentinel */
	bmips_dma_ranges = kzalloc(sizeof(struct bmips_dma_range) * (len + 1),
				   GFP_KERNEL);
	if (!bmips_dma_ranges)
		goto out_bad;

	for (r = bmips_dma_ranges; len; len--, r++) {
		r->child_addr = be32_to_cpup(data++);
		r->parent_addr = be32_to_cpup(data++);
		r->size = be32_to_cpup(data++);
	}

	bmips_dma_ops.phys_to_dma = bmips_ubus_phys_to_dma;
	bmips_dma_ops.addr_to_phys = bmips_ubus_dma_addr_to_phys;

out_good:
	of_node_put(np);
	return 0;

out_bad:
	pr_err("error parsing dma-ranges property\n");
	of_node_put(np);
	return -EINVAL;
}
arch_initcall(bmips_init_dma_ranges);
