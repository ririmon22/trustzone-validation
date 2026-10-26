#ifndef CLIENT_TEST_RUNTIME_H
#define CLIENT_TEST_RUNTIME_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
extern _Alignas(8) uint8_t client_shared_buffer[8192];
#define SHARED_MEMORY_BASE ((uint64_t)(uintptr_t)client_shared_buffer)
#define SHARED_MEMORY_END (SHARED_MEMORY_BASE + 8192UL)
struct smc_result { uint64_t x0, x1, x2, x3; };
struct smc_result smc_call(uint64_t fid, uint64_t a1, uint64_t a2, uint64_t a3);
#endif
