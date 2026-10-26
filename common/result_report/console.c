#include "runtime.h"
/* 文字列と数値を環境別UARTへ出力する。 */
void console_write(const char *text)
{
    while (*text != '\0') { console_putc(*text++); }
}
void console_hex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    console_write("0x");
    for (unsigned int i = 16U; i > 0U; --i) {
        console_putc(digits[(value >> ((i - 1U) * 4U)) & 15U]);
    }
}
void console_uint(uint64_t value)
{
    char digits[20];
    unsigned int count = 0U;
    do { digits[count++] = (char)('0' + value % 10U); value /= 10U; } while (value != 0U);
    while (count != 0U) { console_putc(digits[--count]); }
}
_Noreturn void fatal_exception(uint64_t esr, uint64_t far, uint64_t pc)
{
    console_write("FATAL esr="); console_hex(esr);
    console_write(" far="); console_hex(far);
    console_write(" pc="); console_hex(pc); console_write("\n");
    platform_halt();
}
void clear_bss(void)
{
    for (char *p = __bss_start; p < __bss_end; ++p) { *p = 0; }
}
