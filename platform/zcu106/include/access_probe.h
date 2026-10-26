#ifndef ZCU106_ACCESS_PROBE_H
#define ZCU106_ACCESS_PROBE_H
#include "hardware_tests.h"
struct zcu106_fault_probe {
    volatile bool armed, write, fault_seen;
    volatile uint64_t address, esr, far;
    volatile unsigned int width;
};
extern struct zcu106_fault_probe zcu106_fault_probe;
enum access_result zcu106_try_read64(uint64_t address, uint64_t *value, bool exception_path_verified);
enum access_result zcu106_try_write64(uint64_t address, uint64_t value, bool exception_path_verified);
enum access_result zcu106_try_read32(uint64_t address, uint32_t *value, bool exception_path_verified);
enum access_result zcu106_try_write32(uint64_t address, uint32_t value, bool exception_path_verified);
uint64_t zcu106_handle_probe_abort(uint64_t esr, uint64_t far, uint64_t pc);
#endif
