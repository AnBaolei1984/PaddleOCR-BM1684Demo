#include <linux/delay.h>
#include <linux/device.h>
#include "bm_common.h"
#include "gpio.h"
#include "bm_irq.h"
#include "bm1684/bm1684_irq.h"

/*gpio register*/
#define GPIO_SWPORTA_DR		0x000
#define GPIO_SWPORTA_DDR	   0x004
#define GPIO_SWPORTA_CTL	   0x008
#define GPIO_INTEN			 0x030
#define GPIO_INTMASK		   0x034
#define GPIO_INITYPE_LEVEL	 0x038
#define GPIO_DEBOUNCE		  0x048
#define GPIO_INT_STAUS		 0x040
#define GPIO_PORTA_EOI		 0x04c

void bmdrv_gpio_irq(struct bm_device_info *bmdi)
{
	int reg_val;

	reg_val = gpio_reg_read(bmdi, GPIO_INT_STAUS);
	if (reg_val & 0x80000000)
		PR_TRACE("GPIO 31 interrupt\n");
	else
		PR_TRACE("GPIO unknown interrupt\n");
	gpio_reg_write(bmdi, GPIO_PORTA_EOI, 0xffffffff);
}

int bmdrv_pinmux_init(struct bm_device_info *bmdi)
{
	u32 regval;

	regval = top_reg_read(bmdi, 0x4c4);
	regval &= ~(0x3 << 20 | 0x3 << 4);
	top_reg_write(bmdi, 0x4c4, regval);

	/*select GPIO31 */
	regval = top_reg_read(bmdi, 0x4c8);
	regval &= ~(0x3 << 4);
	regval |= (0x1 << 4);
	top_reg_write(bmdi, 0x4c8, regval);

	/* reset i2c1 */
	regval = top_reg_read(bmdi, 0xc00);
	regval &= ~(1 << 27);
	top_reg_write(bmdi, 0xc00, regval);

	udelay(10);

	regval = top_reg_read(bmdi, 0xc00);
	regval |= (1 << 27);
	top_reg_write(bmdi, 0xc00, regval);

	return 0;
}

int bmdrv_gpio_init(struct bm_device_info *bmdi)
{
	u32 reg_val;

	reg_val = gpio_reg_read(bmdi, GPIO_INITYPE_LEVEL);
	reg_val |= 1 << 31;
	gpio_reg_write(bmdi, GPIO_INITYPE_LEVEL, reg_val);
	reg_val = gpio_reg_read(bmdi, GPIO_DEBOUNCE);
	reg_val |= 1 << 31;
	gpio_reg_write(bmdi, GPIO_DEBOUNCE, reg_val);
	reg_val = gpio_reg_read(bmdi, GPIO_INTEN);
	reg_val |= 1 << 31;
	gpio_reg_write(bmdi, GPIO_INTEN, reg_val);

	return 0;
}

#ifndef SOC_MODE
void bm_gpio_request_irq(struct bm_device_info *bmdi)
{
	bmdrv_submodule_request_irq(bmdi, GPIO_IRQ_ID, bmdrv_gpio_irq);
}

void bm_gpio_free_irq(struct bm_device_info *bmdi)
{
	bmdrv_submodule_free_irq(bmdi, GPIO_IRQ_ID);
}
#endif
