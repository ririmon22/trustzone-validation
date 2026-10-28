#ifndef RUNTIME_H
#define RUNTIME_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "memory_map.h"
#include "debug_log.h"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define SMC_RUN_SERVICE 0xc2000100UL
#define SMC_SERVICE_RETURN 0xc2000101UL
#define SMC_SECURE_READY 0xc2000102UL
#define SERVICE_ACCUMULATE 1UL
#define SERVICE_CHECK_BUFFER 2UL
#define SERVICE_GET_COUNTER 3UL
#define SERVICE_GET_EL 4UL
#define TEST_SENTINEL 0x5ac39e17UL
#define TEST_ITERATIONS 64U
#define SHARED_TOKEN 0x34abUL
#define EXPECTED_EL1 1UL
#define EXPECTED_EL3 3UL
#define SPSR_EL1H_MASKED 0x3c5UL
#define SCR_RW (1UL << 10)
#define SCR_NS 1UL
#define SCTLR_EL1_RES1 0x30d00800UL
#define VECTOR_ALIGNMENT 2048U
struct smc_result { uint64_t x0; uint64_t x1; uint64_t x2; uint64_t x3; };
void console_putc(char value);
void platform_uart_putc(char value);
void console_write(const char *text);
void console_hex(uint64_t value);
void console_uint(uint64_t value);
_Noreturn void platform_halt(void);
_Noreturn void fatal_exception(uint64_t esr, uint64_t far, uint64_t pc);
uint64_t read_current_el(void);
void report_memory_state(const char *world);
uint64_t counter_read(void);
uint64_t counter_frequency(void);
struct smc_result smc_call(uint64_t fid, uint64_t a1, uint64_t a2, uint64_t a3);
void clear_bss(void);
extern char __bss_start[], __bss_end[];
extern void exception_vectors(void);
#endif
