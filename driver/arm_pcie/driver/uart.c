#include <linux/delay.h>
#include <linux/device.h>
#include "bm_common.h"
#include "uart.h"

void bmdrv_uart_init(struct bm_device_info *bmdi)
{
	int baudrate = 115200;
	int uart_clock = 10000000;
	int divisor = uart_clock / (16 * baudrate);

	if (bmdi->cinfo.platform == PALLADIUM)
		divisor = 1;

	uart_reg_write(bmdi, UARTLCR, uart_reg_read(bmdi, UARTLCR) | UARTLCR_DLAB); /* enable DLL, DLLM programming */
	uart_reg_write(bmdi, UARTDLL, divisor & 0xff); /* program DLL */
	uart_reg_write(bmdi, UARTDLLM, (divisor >> 8) & 0xff); /* program DLL */
	uart_reg_write(bmdi, UARTLCR, uart_reg_read(bmdi, UARTLCR) & ~UARTLCR_DLAB); /* disable DLL, DLLM programming */

	uart_reg_write(bmdi, UARTLCR, UARTLCR_WORDSZ_8); /* 8n1 */
	uart_reg_write(bmdi, UARTIER, 0); /* no interrupt */
	uart_reg_write(bmdi, UARTFCR, UARTFCR_TXCLR | UARTFCR_RXCLR | UARTFCR_FIFOEN | UARTFCR_DMAEN); /* enable fifo, DMA */
	uart_reg_write(bmdi, UARTMCR, 0x3); /* DTR + RTS */
}
