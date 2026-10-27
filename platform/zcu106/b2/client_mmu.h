#ifndef ZCU106_B2_CLIENT_MMU_H
#define ZCU106_B2_CLIENT_MMU_H
#include <stdbool.h>
#include <stdint.h>
#define B2_PAGE_SIZE 4096U
#define B2_TABLE_ENTRIES 512U
struct zcu106_b2_page_tables {
    _Alignas(B2_PAGE_SIZE) uint64_t root[B2_TABLE_ENTRIES];
    _Alignas(B2_PAGE_SIZE) uint64_t low[B2_TABLE_ENTRIES];
    _Alignas(B2_PAGE_SIZE) uint64_t high[B2_TABLE_ENTRIES];
    _Alignas(B2_PAGE_SIZE) uint64_t client_first[B2_TABLE_ENTRIES];
    _Alignas(B2_PAGE_SIZE) uint64_t client_second[B2_TABLE_ENTRIES];
    _Alignas(B2_PAGE_SIZE) uint64_t uart[B2_TABLE_ENTRIES];
};
bool zcu106_b2_build_page_tables(struct zcu106_b2_page_tables *tables, uint64_t vector_base, uint64_t vector_end);
bool platform_prepare_tee_client(void);
#endif
