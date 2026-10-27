#include "client_mmu.h"
#include <stdio.h>
#include <string.h>
#define ADDRESS_MASK UINT64_C(0x0000fffffffff000)
#define VALID_TABLE UINT64_C(3)
#define TABLE_MASK UINT64_C(511)
#define PAGE_SHIFT 12U
#define BLOCK_SHIFT 21U
#define ROOT_SHIFT 30U
#define ATTR_MASK (UINT64_C(7) << 2)
#define ATTR_NORMAL (UINT64_C(1) << 2)
#define ATTR_SHAREABLE (UINT64_C(3) << 8)
#define READ_ONLY (UINT64_C(1) << 7)
#define PRIVILEGED_XN (UINT64_C(1) << 53)
#define USER_XN (UINT64_C(1) << 54)
#define TEST_VECTOR_BASE UINT64_C(0x10380000)
#define TEST_VECTOR_END UINT64_C(0x10381000)
static struct zcu106_b2_page_tables tables;
static uint64_t lookup(uint64_t address)
{
    uint64_t descriptor = tables.root[(address >> ROOT_SHIFT) & TABLE_MASK];
    if ((descriptor & VALID_TABLE) != VALID_TABLE) { return 0U; }
    const uint64_t *second = (const uint64_t *)(uintptr_t)(descriptor & ADDRESS_MASK);
    descriptor = second[(address >> BLOCK_SHIFT) & TABLE_MASK];
    if ((descriptor & VALID_TABLE) != VALID_TABLE) { return 0U; }
    const uint64_t *third = (const uint64_t *)(uintptr_t)(descriptor & ADDRESS_MASK);
    return third[(address >> PAGE_SHIFT) & TABLE_MASK];
}
int main(int argc, char **argv)
{
    if (argc != 2) { return 2; }
    if (strcmp(argv[1], "bad-vector") == 0) {
        return zcu106_b2_build_page_tables(&tables, TEST_VECTOR_BASE + 1U, TEST_VECTOR_END) ? 1 : 0;
    }
    if (!zcu106_b2_build_page_tables(&tables, TEST_VECTOR_BASE, TEST_VECTOR_END)) { return 1; }
    uint64_t address, expected, descriptor;
    if (strcmp(argv[1], "code-rx") == 0) {
        address = UINT64_C(0x10200000); descriptor = lookup(address);
        return (descriptor & ADDRESS_MASK) == address && (descriptor & READ_ONLY) != 0U &&
            (descriptor & PRIVILEGED_XN) == 0U && (descriptor & USER_XN) != 0U ? 0 : 1;
    }
    if (strcmp(argv[1], "vector-rx") == 0) {
        descriptor = lookup(TEST_VECTOR_BASE);
        return (descriptor & READ_ONLY) != 0U && (descriptor & PRIVILEGED_XN) == 0U &&
            (lookup(TEST_VECTOR_BASE - B2_PAGE_SIZE) & PRIVILEGED_XN) != 0U ? 0 : 1;
    }
    if (strcmp(argv[1], "shared-cache") == 0) {
        address = UINT64_C(0x10500000); expected = ATTR_NORMAL | ATTR_SHAREABLE | PRIVILEGED_XN | USER_XN;
        descriptor = lookup(address);
        return (descriptor & ADDRESS_MASK) == address && (descriptor & expected) == expected &&
            (descriptor & ATTR_MASK) == ATTR_NORMAL && (descriptor & READ_ONLY) == 0U ? 0 : 1;
    }
    if (strcmp(argv[1], "device-only-page") == 0) {
        descriptor = lookup(UINT64_C(0xff000000));
        return (descriptor & ADDRESS_MASK) == UINT64_C(0xff000000) && (descriptor & ATTR_MASK) == 0U &&
            (descriptor & PRIVILEGED_XN) != 0U && lookup(UINT64_C(0xff001000)) == 0U ? 0 : 1;
    }
    if (strcmp(argv[1], "unmapped-secure-ddr1") == 0) {
        return lookup(UINT64_C(0x60000000)) == 0U && lookup(UINT64_C(0x62010000)) == 0U &&
            lookup(UINT64_C(0x80000000)) == 0U && lookup(UINT64_C(0x800000000)) == 0U &&
            lookup(UINT64_C(0x10000000)) == 0U ? 0 : 1;
    }
    return 2;
}
