#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H
#include <stdint.h>
#include <stdarg.h>
#ifndef RAM_LOG_CAPACITY
#define RAM_LOG_CAPACITY 32768U
#endif
#define RAM_LOG_MAGIC 0x545a4c47U
#define RAM_LOG_VERSION 1U
#define RAM_LOG_RUNNING 1U
#define RAM_LOG_COMPLETE 2U
#define RAM_LOG_FATAL 3U
struct ram_log_buffer {
    uint32_t magic, version, capacity, used;
    uint32_t dropped, state;
    uint64_t run_id;
    char data[RAM_LOG_CAPACITY];
};
struct boot_debug_state { uint64_t version, stage, stop_reason, run_id; };
struct exception_record {
    uint64_t valid, level, vector, esr, far, elr, spsr, sp, sp_el0, sp_el1;
    uint64_t x[31];
};
extern volatile struct ram_log_buffer debug_ram_log;
extern volatile struct boot_debug_state debug_boot;
extern volatile struct exception_record debug_exception;
extern volatile struct exception_record debug_sync_exception;
int log_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
void log_set_state(uint32_t state);
void debug_stage(uint64_t stage, uint64_t reason);
#endif
