/*
 * B53 switch SerDes/SGMII PHY main logic
 *
 * Copyright (C) 2017 Florian Fainelli <f.fainelli@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/phy.h>
#include <linux/phylink.h>
#include <net/dsa.h>

#include "b53_regs.h"
#include "b53_priv.h"

/* Non-standard page used to access SerDes PHY registers on NorthStar Plus */
#define B53_SERDES_PAGE			0x16
#define B53_SERDES_BLKADDR		0x3e
#define B53_SERDES_LANE			0x3c

#define B53_SERDES_ID0			0x20
#define B53_SERDES_MII_REG(x)		(0x20 + (x) * 2)
#define B53_SERDES_DIGITAL_CONTROL1	0x22
#define B53_SERDES_DIGITAL_CONTROL2	0x24
#define B53_SERDES_DIGITAL_CONTROL3	0x26
#define B53_SERDES_DIGITAL_STATUS	0x28

#define SERDES_DIGITAL_CONTROL1		0x8300
#define  FIBER_MODE_1000X		BIT(0)
#define  TBI_INTERFACE			BIT(1)
#define  SIGNAL_DETECT_EN		BIT(2)
#define  INVERT_SIGNAL_DETECT		BIT(3)
#define  AUTODET_EN			BIT(4)
#define  SGMII_MASTER_MODE		BIT(5)
#define  DISABLE_DLL_PWRDOWN		BIT(6)
#define  CRC_CHECKER_DIS		BIT(7)
#define  COMMA_DET_EN			BIT(8)
#define  ZERO_COMMA_DET_EN		BIT(9)
#define  REMOTE_LOOPBACK		BIT(10)
#define  SEL_RX_PKTS_FOR_CNTR		BIT(11)
#define  MASTER_MDIO_PHY_SEL		BIT(13)
#define  DISABLE_SIGNAL_DETECT_FLT	BIT(14)

#define SERDES_DIGITAL_CONTROL2		0x8300
#define  EN_PARALLEL_DET		BIT(0)
#define  DIS_FALSE_LINK			BIT(1)
#define  FLT_FORCE_LINK			BIT(2)
#define  EN_AUTONEG_ERR_TIMER		BIT(3)
#define  DIS_REMOTE_FAULT_SENSING	BIT(4)
#define  FORCE_XMIT_DATA		BIT(5)
#define  AUTONEG_FAST_TIMERS		BIT(6)
#define  DIS_CARRIER_EXTEND		BIT(7)
#define  DIS_TRRR_GENERATION		BIT(8)
#define  BYPASS_PCS_RX			BIT(9)
#define  BYPASS_PCS_TX			BIT(10)
#define  TEST_CNTR_EN			BIT(11)
#define  TX_PACKET_SEQ_TEST		BIT(12)
#define  TX_IDLE_JAM_SEQ_TEST		BIT(13)
#define  CLR_BER_CNTR			BIT(14)

#define SERDES_DIGITAL_CONTROL3		0x8300
#define  TX_FIFO_RST			BIT(0)
#define  FIFO_ELAST_TX_RX_SHIFT		1
#define  FIFO_ELAST_TX_RX_5K		0
#define  FIFO_ELAST_TX_RX_10K		1
#define  FIFO_ELAST_TX_RX_13_5K		2
#define  FIFO_ELAST_TX_RX_18_5K		3
#define  BLOCK_TXEN_MODE		BIT(9)
#define  JAM_FALSE_CARRIER_MODE		BIT(10)
#define  EXT_PHY_CRS_MODE		BIT(11)
#define  INVERT_EXT_PHY_CRS		BIT(12)
#define  DISABLE_TX_CRS			BIT(13)

#define SERDES_DIGITAL_STATUS		0x8300
#define  SGMII_MODE			BIT(0)
#define  LINK_STATUS			BIT(1)
#define  DUPLEX_STATUS			BIT(2)
#define  SPEED_STATUS_SHIFT		3
#define  SPEED_STATUS_10		0
#define  SPEED_STATUS_100		1
#define  SPEED_STATUS_1000		2
#define  SPEED_STATUS_2500		3
#define  PAUSE_RESOLUTION_TX_SIDE	BIT(5)
#define  PAUSE_RESOLUTION_RX_SIDE	BIT(6)
#define  LINK_STATUS_CHANGE		BIT(7)
#define  EARLY_END_EXT_DET		BIT(8)
#define  CARRIER_EXT_ERR_DET		BIT(9)
#define  RX_ERR_DET			BIT(10)
#define  TX_ERR_DET			BIT(11)
#define  CRC_ERR_DET			BIT(12)
#define  FALSE_CARRIER_ERR_DET		BIT(13)
#define  RXFIFO_ERR_DET			BIT(14)
#define  TXFIFO_ERR_DET			BIT(15)

#define SERDES_ID0			0x8310
#define SERDES_MII_BLK			0xffe0
#define SERDES_XGXSBLK0_BLOCKADDRESS	0xffd0

static void b53_serdes_write_blk(struct b53_device *dev, u8 offset, u16 block,
				 u16 value)
{
	b53_write16(dev, B53_SERDES_PAGE, B53_SERDES_BLKADDR, block);
	b53_write16(dev, B53_SERDES_PAGE, offset, value);
}

static u16 b53_serdes_read_blk(struct b53_device *dev, u8 offset, u16 block)
{
	u16 value;

	b53_write16(dev, B53_SERDES_PAGE, B53_SERDES_BLKADDR, block);
	b53_read16(dev, B53_SERDES_PAGE, offset, &value);

	return value;
}

static void b53_serdes_set_lane(struct b53_device *dev, u8 lane)
{
	if (dev->serdes_lane == lane)
		return;

	b53_serdes_write_blk(dev, B53_SERDES_LANE,
			     SERDES_XGXSBLK0_BLOCKADDRESS, lane);
	dev->serdes_lane = lane;
}

static void b53_serdes_write(struct b53_device *dev, u8 lane,
			     u8 offset, u16 block, u16 value)
{
	b53_serdes_set_lane(dev, lane);
	b53_serdes_write_blk(dev, offset, block, value);
}

static u16 b53_serdes_read(struct b53_device *dev, u8 lane,
			   u8 offset, u16 block)
{
	b53_serdes_set_lane(dev, lane);
	return b53_serdes_read_blk(dev, offset, block);
}

int b53_serdes_init(struct b53_device *dev)
{
	dev_info(dev->dev, "SerDes ID0: 0x%04x\n",
		 b53_serdes_read(dev, 0, B53_SERDES_ID0, SERDES_ID0));
	dev_info(dev->dev, "Serdes MSB: 0x%04x\n",
		 b53_serdes_read(dev, 0, B53_SERDES_MII_REG(MII_PHYSID1),
			 	 SERDES_MII_BLK));
	dev_info(dev->dev, "Serdes LSB: 0x%04x\n",
		 b53_serdes_read(dev, 0, B53_SERDES_MII_REG(MII_PHYSID2),
			 	 SERDES_MII_BLK));

	return 0;
}

void b53_serdes_exit(struct b53_device *dev)
{
}
