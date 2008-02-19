/*
 * <arch/arm/mach-davinci/i2c-emac.c
 *
 * Read MAC address from i2c-attached EEPROM
 * FIXME: Move into network driver once stabilized
 *
 * Author: Texas Instruments
 *
 * 2006 (c) Texas Instruments, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/errno.h>

/* Get Ethernet address from kernel boot params */
static unsigned char cpmac_eth_string[20] = "00:19:b8:00:de:ad";

/* This function gets the Ethernet MAC address from EEPROM
 * Input buffer to be of atlease 20 bytes in length
 */
int davinci_get_macaddr (char *ptr)
{
   strcpy (ptr, cpmac_eth_string);
	return 0;
}
EXPORT_SYMBOL(davinci_get_macaddr);

static int davinci_cpmac_eth_setup(char *str)
{
	/* The first char passed from the bootloader is '=', so ignore it */
        strcpy(&cpmac_eth_string[0], &str[1]);

        printk("TI DaVinci EMAC: Kernel Boot params Eth address: %s\n", cpmac_eth_string);

        return (1);
}
__setup("eth", davinci_cpmac_eth_setup);

