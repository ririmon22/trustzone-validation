/* 基準版と整理版を同じ領域に順に実行し、戻り値と表の全バイトを比較する。
 * CPU命令は実行しない。TTBR公開、TLB、cache、実機のアクセス権限は検証対象外。
 * baseline_build_page_tablesは基準版client_mmu.cを関数名変更してリンクする。 */
#include "client_mmu.h"
#include "memory_map.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

bool baseline_build_page_tables(struct zcu106_b2_page_tables *tables,
                               uint64_t vector_base, uint64_t vector_end);
static struct zcu106_b2_page_tables tables;
static unsigned char expected[sizeof(tables)];

int main(void)
{
    const uint64_t ranges[][2] = {
        {NONSECURE_DATA_BASE, NONSECURE_DATA_BASE + B2_PAGE_SIZE},
        {NONSECURE_DATA_END - B2_PAGE_SIZE, NONSECURE_DATA_END},
        {NONSECURE_DATA_BASE, NONSECURE_DATA_END},
        {NONSECURE_DATA_BASE - B2_PAGE_SIZE, NONSECURE_DATA_BASE},
        {NONSECURE_DATA_END, NONSECURE_DATA_END + B2_PAGE_SIZE},
        {NONSECURE_DATA_BASE, NONSECURE_DATA_BASE},
        {NONSECURE_DATA_BASE + 1U, NONSECURE_DATA_BASE + B2_PAGE_SIZE},
        {NONSECURE_DATA_BASE, NONSECURE_DATA_BASE + B2_PAGE_SIZE - 1U},
    };
    for (size_t index = 0; index < sizeof(ranges) / sizeof(ranges[0]); ++index) {
        memset(&tables, 0xa5, sizeof(tables));
        bool before = baseline_build_page_tables(&tables, ranges[index][0], ranges[index][1]);
        memcpy(expected, &tables, sizeof(tables));
        memset(&tables, 0xa5, sizeof(tables));
        bool after = zcu106_b2_build_page_tables(&tables, ranges[index][0], ranges[index][1]);
        assert(before == after);
        assert(memcmp(expected, &tables, sizeof(tables)) == 0);
    }
    assert(!zcu106_b2_build_page_tables(NULL, NONSECURE_DATA_BASE, NONSECURE_DATA_END));
    puts("page table equivalence: PASS (8 ranges + NULL)");
    return 0;
}
