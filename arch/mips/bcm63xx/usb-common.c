/*
 * Broadcom BCM63xx common USB device configuration code
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2012 Kevin Cernekee <cernekee@gmail.com>
 * Copyright (C) 2012 Broadcom Corporation
 *
 */
#include <linux/export.h>

#include <bcm63xx_cpu.h>
#include <bcm63xx_regs.h>
#include <bcm63xx_io.h>
#include <bcm63xx_usb_priv.h>

void bcm63xx_usb_priv_select_phy_mode(u32 portmask, bool is_device)
{
	u32 val;

	val = bcm_rset_readl(RSET_USBH_PRIV, USBH_PRIV_UTMI_CTL_6368_REG);
	if (is_device) {
		val |= (portmask << USBH_PRIV_UTMI_CTL_HOSTB_SHIFT);
		val |= (portmask << USBH_PRIV_UTMI_CTL_NODRIV_SHIFT);
	} else {
		val &= ~(portmask << USBH_PRIV_UTMI_CTL_HOSTB_SHIFT);
		val &= ~(portmask << USBH_PRIV_UTMI_CTL_NODRIV_SHIFT);
	}
	bcm_rset_writel(RSET_USBH_PRIV, val, USBH_PRIV_UTMI_CTL_6368_REG);

	val = bcm_rset_readl(RSET_USBH_PRIV, USBH_PRIV_SWAP_6368_REG);
	if (is_device)
		val |= USBH_PRIV_SWAP_USBD_MASK;
	else
		val &= ~USBH_PRIV_SWAP_USBD_MASK;
	bcm_rset_writel(RSET_USBH_PRIV, val, USBH_PRIV_SWAP_6368_REG);
}
EXPORT_SYMBOL(bcm63xx_usb_priv_select_phy_mode);

void bcm63xx_usb_priv_select_pullup(u32 portmask, bool is_on)
{
	u32 val;

	val = bcm_rset_readl(RSET_USBH_PRIV, USBH_PRIV_UTMI_CTL_6368_REG);
	if (is_on)
		val &= ~(portmask << USBH_PRIV_UTMI_CTL_NODRIV_SHIFT);
	else
		val |= (portmask << USBH_PRIV_UTMI_CTL_NODRIV_SHIFT);
	bcm_rset_writel(RSET_USBH_PRIV, val, USBH_PRIV_UTMI_CTL_6368_REG);
}
EXPORT_SYMBOL(bcm63xx_usb_priv_select_pullup);
