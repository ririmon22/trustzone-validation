#include "protection.h"
static bool native_read32(void *context, uint64_t address, uint32_t *value)
{
    (void)context;
    if (value == NULL || address % sizeof(uint32_t) != 0U) { return false; }
    *value = *(volatile uint32_t *)(uintptr_t)address;
    return true;
}
static bool native_write32(void *context, uint64_t address, uint32_t value)
{
    (void)context;
    if (address % sizeof(uint32_t) != 0U) { return false; }
    *(volatile uint32_t *)(uintptr_t)address = value;
    return true;
}
static void native_barrier(void *context)
{
    (void)context;
    __asm__ volatile("dsb sy; isb" ::: "memory");
}
/* 呼出側の確認済みpolicyがない限り、driverはこのIOを使用しない。 */
const struct protection_io zcu106_native_protection_io = {
    NULL, native_read32, native_write32, native_barrier
};
