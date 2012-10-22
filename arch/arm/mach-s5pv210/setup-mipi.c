/* linux/arch/arm/mach-s5pv210/setup-mipi.c
 *
 * Samsung MIPI-DSI DPHY driver.
 *
 * Author: InKi Dae <inki.dae@xxxxxxxxxxx>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/gpio.h>

#include <mach/map.h>
#include <mach/regs-clock.h>

#include <plat/mipi-dsi.h>
#include <plat/regs-dsim.h>
#include <plat/gpio-cfg.h>

static int s5p_mipi_enable_d_phy(struct dsim_device *dsim, unsigned int enable)
{
	unsigned int reg;

	reg = readl(S5P_MIPI_CONTROL) & ~(1 << 0);
	reg |= (enable << 0);
	writel(reg, S5P_MIPI_CONTROL);

	return 0;
}

static int s5p_mipi_enable_dsi_master(struct dsim_device *dsim,
	unsigned int enable)
{
	unsigned int reg;

	reg = readl(S5P_MIPI_CONTROL) & ~(1 << 2);
	reg |= (enable << 2);
	writel(reg, S5P_MIPI_CONTROL);

	return 0;
}

int s5p_mipi_part_reset(struct dsim_device *dsim)
{
	writel(S5P_MIPI_M_RESETN, S5P_MIPI_CONTROL);

	return 0;
}

int s5p_mipi_init_d_phy(struct dsim_device *dsim, unsigned int enable)
{
	/**
	 * DPHY and Master block must be enabled at the system initialization
	 * step before data access from/to DPHY begins.
	 */
	s5p_mipi_enable_d_phy(dsim, !!enable);

	s5p_mipi_enable_dsi_master(dsim, !!enable);

	return 0;
}

int s5p_mipi_power(struct dsim_device *dsim, unsigned int enable)
{
	/*MIPI IP 1.1v power*/

	/*dphy 1.8v power*/
	s3c_gpio_setpull(MIPI_DSIM_POWER, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpin(MIPI_DSIM_POWER, !!enable);
	s3c_gpio_cfgpin(MIPI_DSIM_POWER, S3C_GPIO_OUTPUT);
	
	return 0;
}

int s5p_mipi_dphy_power(struct dsim_device *dsim, unsigned int enable)
{
	s3c_gpio_setpull(MIPI_DSIM_POWER, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpin(MIPI_DSIM_POWER, !!enable);
	s3c_gpio_cfgpin(MIPI_DSIM_POWER, S3C_GPIO_OUTPUT);
	
	return 0;
}
