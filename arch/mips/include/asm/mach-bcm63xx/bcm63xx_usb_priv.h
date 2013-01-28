#ifndef BCM63XX_USB_PRIV_H_
#define BCM63XX_USB_PRIV_H_

#include <linux/types.h>

void bcm63xx_usb_priv_select_phy_mode(u32 portmask, bool is_device);
void bcm63xx_usb_priv_select_pullup(u32 portmask, bool is_on);

#endif /* BCM63XX_USB_PRIV_H_ */
