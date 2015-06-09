#ifndef __BCM_MDIO_UNIMAC_H
#define __BCM_MDIO_UNIMAC_H

struct unimac_mdio_pdata {
	/* Bitmask of Ethernet PHYs the bus should probe for */
	u32	phy_mask;
};

#define UNIMAC_MDIO_DRV_NAME	"unimac-mdio"

#endif /* __BCM_MDIO_UNIMAC_H */
