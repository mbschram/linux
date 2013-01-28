/*
 * Broadcom BCM63xx common USB device configuration code
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2008 Maxime Bizon <mbizon@freebox.fr>
 * Copyright (C) 2012 Kevin Cernekee <cernekee@gmail.com>
 * Copyright (C) 2012 Broadcom Corporation
 *
 */
#include <linux/spinlock.h>
#include <linux/export.h>

#include <bcm63xx_cpu.h>
#include <bcm63xx_regs.h>
#include <bcm63xx_io.h>
#include <bcm63xx_usb_priv.h>

static DEFINE_SPINLOCK(usb_priv_reg_lock);

void bcm63xx_usb_priv_select_phy_mode(u32 portmask, bool is_device)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&usb_priv_reg_lock, flags);

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

	spin_unlock_irqrestore(&usb_priv_reg_lock, flags);
}
EXPORT_SYMBOL(bcm63xx_usb_priv_select_phy_mode);

void bcm63xx_usb_priv_select_pullup(u32 portmask, bool is_on)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&usb_priv_reg_lock, flags);

	val = bcm_rset_readl(RSET_USBH_PRIV, USBH_PRIV_UTMI_CTL_6368_REG);
	if (is_on)
		val &= ~(portmask << USBH_PRIV_UTMI_CTL_NODRIV_SHIFT);
	else
		val |= (portmask << USBH_PRIV_UTMI_CTL_NODRIV_SHIFT);
	bcm_rset_writel(RSET_USBH_PRIV, val, USBH_PRIV_UTMI_CTL_6368_REG);

	spin_unlock_irqrestore(&usb_priv_reg_lock, flags);
}
EXPORT_SYMBOL(bcm63xx_usb_priv_select_pullup);

/* The following array represents the meaning of the DESC/DATA
 * endian swapping with respect to the CPU configured endianness
 *
 * DATA	ENDN	mmio	descriptor
 * 0	0	BE	invalid
 * 0	1	BE	LE
 * 1	0	BE	BE
 * 1	1	BE	invalid
 *
 * Since BCM63XX SoCs are configured to be in big-endian mode
 * we want configuration at line 3.
 */
void bcm63xx_usb_priv_ohci_cfg_set(void)
{
	u32 reg;
	unsigned long flags;

	spin_lock_irqsave(&usb_priv_reg_lock, flags);

	if (BCMCPU_IS_6348())
		bcm_rset_writel(RSET_OHCI_PRIV, 0, OHCI_PRIV_REG);
	else if (BCMCPU_IS_6358()) {
		reg = bcm_rset_readl(RSET_USBH_PRIV, USBH_PRIV_SWAP_6358_REG);
		reg &= ~USBH_PRIV_SWAP_OHCI_ENDN_MASK;
		reg |= USBH_PRIV_SWAP_OHCI_DATA_MASK;
		bcm_rset_writel(RSET_USBH_PRIV, reg, USBH_PRIV_SWAP_6358_REG);
		/*
		 * The magic value comes for the original vendor BSP
		 * and is needed for USB to work. Datasheet does not
		 * help, so the magic value is used as-is.
		 */
		bcm_rset_writel(RSET_USBH_PRIV, 0x1c0020,
				USBH_PRIV_TEST_6358_REG);

	} else if (BCMCPU_IS_6328() || BCMCPU_IS_6362() || BCMCPU_IS_6368()) {
		reg = bcm_rset_readl(RSET_USBH_PRIV, USBH_PRIV_SWAP_6368_REG);
		reg &= ~USBH_PRIV_SWAP_OHCI_ENDN_MASK;
		reg |= USBH_PRIV_SWAP_OHCI_DATA_MASK;
		bcm_rset_writel(RSET_USBH_PRIV, reg, USBH_PRIV_SWAP_6368_REG);

		reg = bcm_rset_readl(RSET_USBH_PRIV, USBH_PRIV_SETUP_6368_REG);
		reg |= USBH_PRIV_SETUP_IOC_MASK;
		bcm_rset_writel(RSET_USBH_PRIV, reg, USBH_PRIV_SETUP_6368_REG);
	}

	spin_unlock_irqrestore(&usb_priv_reg_lock, flags);
}

void bcm63xx_usb_priv_ehci_cfg_set(void)
{
	u32 reg;
	unsigned long flags;

	spin_lock_irqsave(&usb_priv_reg_lock, flags);

	if (BCMCPU_IS_6358()) {
		reg = bcm_rset_readl(RSET_USBH_PRIV, USBH_PRIV_SWAP_6358_REG);
		reg &= ~USBH_PRIV_SWAP_EHCI_ENDN_MASK;
		reg |= USBH_PRIV_SWAP_EHCI_DATA_MASK;
		bcm_rset_writel(RSET_USBH_PRIV, reg, USBH_PRIV_SWAP_6358_REG);

		/*
		 * The magic value comes for the original vendor BSP
		 * and is needed for USB to work. Datasheet does not
		 * help, so the magic value is used as-is.
		 */
		bcm_rset_writel(RSET_USBH_PRIV, 0x1c0020,
				USBH_PRIV_TEST_6358_REG);

	} else if (BCMCPU_IS_6328() || BCMCPU_IS_6362() || BCMCPU_IS_6368()) {
		reg = bcm_rset_readl(RSET_USBH_PRIV, USBH_PRIV_SWAP_6368_REG);
		reg &= ~USBH_PRIV_SWAP_EHCI_ENDN_MASK;
		reg |= USBH_PRIV_SWAP_EHCI_DATA_MASK;
		bcm_rset_writel(RSET_USBH_PRIV, reg, USBH_PRIV_SWAP_6368_REG);

		reg = bcm_rset_readl(RSET_USBH_PRIV, USBH_PRIV_SETUP_6368_REG);
		reg |= USBH_PRIV_SETUP_IOC_MASK;
		bcm_rset_writel(RSET_USBH_PRIV, reg, USBH_PRIV_SETUP_6368_REG);
	}

	spin_unlock_irqrestore(&usb_priv_reg_lock, flags);
}
