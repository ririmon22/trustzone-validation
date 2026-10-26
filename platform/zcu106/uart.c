#include "runtime.h"
/* UART選択とクロックは生成BSPとボード配線との照合が必要。 */
#define UART_BASE 0xff000000UL
#define UART_STATUS_OFFSET 0x2cUL
#define UART_FIFO_OFFSET 0x30UL
#define UART_TX_FULL (1U << 4)
#define UART_SPIN_LIMIT 1000000U
void console_putc(char value)
{
    unsigned int spins = UART_SPIN_LIMIT;
    while ((*(volatile uint32_t *)(UART_BASE + UART_STATUS_OFFSET) & UART_TX_FULL) != 0U) {
        if (--spins == 0U) { platform_halt(); }
    }
    *(volatile uint32_t *)(UART_BASE + UART_FIFO_OFFSET) = (uint32_t)(unsigned char)value;
}
