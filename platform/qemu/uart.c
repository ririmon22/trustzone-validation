#include "runtime.h"
#define UART_BASE 0x09000000UL
#define UART_DATA_OFFSET 0UL
#define UART_FLAGS_OFFSET 0x18UL
#define UART_TX_FULL (1U << 5)
#define UART_SPIN_LIMIT 1000000U
void console_putc(char value)
{
    volatile uint32_t *flags = (volatile uint32_t *)(UART_BASE + UART_FLAGS_OFFSET);
    unsigned int spins = UART_SPIN_LIMIT;
    while ((*flags & UART_TX_FULL) != 0U) { if (--spins == 0U) { platform_halt(); } }
    *(volatile uint32_t *)(UART_BASE + UART_DATA_OFFSET) = (uint32_t)(unsigned char)value;
}
