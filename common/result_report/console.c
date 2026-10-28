#include "runtime.h"
#include <limits.h>
#define HEX_DIGITS 16U
#define DECIMAL_DIGITS 20U
#define FORMAT_MAX_WIDTH 32U
#ifndef LOG_UART_ENABLED
#ifdef PLATFORM_REQUIRE_EL3_ENTRY_GUARD
#define LOG_UART_ENABLED (!PLATFORM_REQUIRE_EL3_ENTRY_GUARD)
#else
#define LOG_UART_ENABLED 1
#endif
#endif
volatile struct ram_log_buffer debug_ram_log __attribute__((aligned(64)));
_Static_assert(offsetof(struct ram_log_buffer, data) == 32U, "RAM log ABI");
_Static_assert(offsetof(struct exception_record, x) == 80U, "exception ABI");
void console_putc(char value)
{
    if (debug_ram_log.magic != RAM_LOG_MAGIC) {
        debug_ram_log.version = RAM_LOG_VERSION;
        debug_ram_log.capacity = RAM_LOG_CAPACITY;
        debug_ram_log.state = RAM_LOG_RUNNING;
        debug_ram_log.run_id = debug_boot.run_id;
        debug_ram_log.magic = RAM_LOG_MAGIC;
    }
    if (debug_ram_log.used < RAM_LOG_CAPACITY) {
        debug_ram_log.data[debug_ram_log.used] = value;
        debug_ram_log.used++;
    } else if (debug_ram_log.dropped != UINT32_MAX) { debug_ram_log.dropped++; }
#if LOG_UART_ENABLED
    platform_uart_putc(value);
#endif
}
void log_set_state(uint32_t state)
{
    debug_ram_log.state = state;
#ifdef TEE_SHARED_CACHED
    /* B2の完了時だけPoCへcleanする。途中dumpの整合性は保証しない。 */
    uintptr_t begin = (uintptr_t)&debug_ram_log;
    uintptr_t end = begin + offsetof(struct ram_log_buffer, data) + debug_ram_log.used;
    for (uintptr_t line = begin; line < end; line += 64U) {
        __asm__ volatile("dc cvac, %0" :: "r"(line) : "memory");
    }
    __asm__ volatile("dsb sy" ::: "memory");
#endif
}
static unsigned int output_unsigned(uint64_t value, unsigned int base, unsigned int width, char pad)
{
    const char alphabet[] = "0123456789abcdef";
    char digits[DECIMAL_DIGITS];
    unsigned int count = 0U, written = 0U;
    do { digits[count++] = alphabet[value % base]; value /= base; } while (value != 0UL);
    while (width > count) { console_putc(pad); width--; written++; }
    written += count;
    while (count != 0U) { console_putc(digits[--count]); }
    return written;
}
/* 整数・文字列だけを扱い、浮動小数点、%n、動的確保は提供しない。 */
int log_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    unsigned int written = 0U;
    while (*format != '\0') {
        if (*format != '%') { console_putc(*format++); written++; continue; }
        format++;
        char pad = *format == '0' ? '0' : ' ';
        unsigned int width = 0U, longs = 0U;
        while (*format >= '0' && *format <= '9') {
            width = width * 10U + (unsigned int)(*format++ - '0');
            if (width > FORMAT_MAX_WIDTH) { va_end(args); return -1; }
        }
        while (*format == 'l' && longs < 2U) { longs++; format++; }
        char kind = *format;
        if (kind == '\0') { va_end(args); return -1; }
        format++;
        if (kind == '%' && longs == 0U) { console_putc('%'); written++; }
        else if (kind == 's' && longs == 0U) {
            const char *text = va_arg(args, const char *);
            if (text == NULL) { text = "(null)"; }
            while (*text != '\0') { console_putc(*text++); written++; }
        } else if (kind == 'c' && longs == 0U) { console_putc((char)va_arg(args, int)); written++; }
        else if (kind == 'p' && longs == 0U) {
            console_putc('0'); console_putc('x'); written += 2U;
            written += output_unsigned((uintptr_t)va_arg(args, void *), 16U, HEX_DIGITS, '0');
        } else if (kind == 'u' || kind == 'x') {
            uint64_t value = longs == 2U ? va_arg(args, unsigned long long) : longs == 1U ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
            written += output_unsigned(value, kind == 'x' ? 16U : 10U, width, pad);
        } else if (kind == 'd') {
            int64_t value = longs == 2U ? va_arg(args, long long) : longs == 1U ? va_arg(args, long) : va_arg(args, int);
            if (value < 0) { console_putc('-'); written++; }
            uint64_t magnitude = value < 0 ? 0UL - (uint64_t)value : (uint64_t)value;
            written += output_unsigned(magnitude, 10U, width, pad);
        } else { va_end(args); return -1; }
    }
    va_end(args);
    return written <= INT_MAX ? (int)written : -1;
}
/* 旧呼出しは同じprintf形式実装へ集約し、既存利用者との互換性を保つ。 */
void console_write(const char *text) { (void)log_printf("%s", text); }
void console_hex(uint64_t value) { (void)log_printf("0x%016llx", (unsigned long long)value); }
void console_uint(uint64_t value) { (void)log_printf("%llu", (unsigned long long)value); }
_Noreturn void fatal_exception(uint64_t esr, uint64_t far, uint64_t pc)
{
    if (debug_exception.valid == 0U && debug_sync_exception.valid == 2U) { debug_exception = debug_sync_exception; }
    debug_stage(90U, 3U);
    (void)log_printf("FATAL esr=0x%016llx far=0x%016llx pc=0x%016llx\n", (unsigned long long)esr, (unsigned long long)far, (unsigned long long)pc);
    log_set_state(RAM_LOG_FATAL);
    platform_halt();
}
void clear_bss(void)
{
    for (char *p = __bss_start; p < __bss_end; ++p) { *p = 0; }
    debug_stage(10U, 0U);
}
