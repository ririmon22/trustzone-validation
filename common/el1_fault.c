/* EL1同期例外の判定。el1_vectors.SからESRと例外発生PCを受け取る。
 * 試験が期待したアドレス・命令・読書き方向だけを回復可能とし、復帰先PCを返す。
 * 0は回復不可。未知の例外を読み飛ばすと試験を誤って成功扱いするため拒否する。
 */
#include "runtime.h"
#define ESR_EC_DATA_ABORT 0x25UL
#define ESR_EC_SHIFT 26U
#define ESR_EC_MASK 0x3fUL
#define ESR_WRITE_BIT (1UL << 6)
#define ESR_DFSC_MASK 0x3fUL
#define DFSC_SYNC_EXTERNAL 0x10UL
#define DFSC_PERMISSION_BASE 0x0cUL
#define DFSC_PERMISSION_LAST 0x0fUL
extern char probe_read_fault[], probe_read_resume[], probe_write_fault[], probe_write_resume[];
volatile uint64_t expected_fault;
volatile uint64_t expected_fault_address;
volatile uint64_t fault_count;
volatile uint64_t last_fault_esr;
/* 基本構成では物理probeを回復しない。B2物理試験の強い定義がある場合だけ上書きする。 */
__attribute__((weak)) uint64_t b2_handle_phys_fault(uint64_t esr, uint64_t far, uint64_t pc,
                                                    uint64_t sctlr)
{
    (void)esr;
    (void)far;
    (void)pc;
    (void)sctlr;
    return 0UL;
}
/* vectorが保存した元PCとESRを受け取り、期待したprobeだけの復帰先を返す。
 * FARは障害アドレス。PC・方向・原因を一致させ、不一致は0を返して致命例外へ進む。 */
uint64_t handle_el1_sync(uint64_t esr, uint64_t pc)
{
    uint64_t far, sctlr;
    __asm__ volatile("mrs %0, far_el1; mrs %1, sctlr_el1" : "=r"(far), "=r"(sctlr));
    if (expected_fault == EXPECTED_FAULT_PHYSICAL_READ || expected_fault == EXPECTED_FAULT_PHYSICAL_WRITE) {
        return b2_handle_phys_fault(esr, far, pc, sctlr);
    }
    if ((esr & (1UL << 10)) != 0UL || far != expected_fault_address) {
        return 0UL;
    }
    if ((sctlr & 1UL) == 0UL && (esr & ESR_DFSC_MASK) != DFSC_SYNC_EXTERNAL) {
        return 0UL;
    }
    uint64_t dfsc = esr & ESR_DFSC_MASK;
    bool data_abort = ((esr >> ESR_EC_SHIFT) & ESR_EC_MASK) == ESR_EC_DATA_ABORT;
    bool denied = dfsc == DFSC_SYNC_EXTERNAL ||
                  (dfsc >= DFSC_PERMISSION_BASE && dfsc <= DFSC_PERMISSION_LAST);
    if (expected_fault == EXPECTED_FAULT_NONE || !data_abort || !denied) {
        return 0UL;
    }
    last_fault_esr = esr;
    (void)log_printf("%s", "MEM_ABORT address=");
    (void)log_printf("0x%016llx", (unsigned long long)(expected_fault_address));
    (void)log_printf("%s", " far=");
    (void)log_printf("0x%016llx", (unsigned long long)(far));
    (void)log_printf("%s", " esr=");
    (void)log_printf("0x%016llx", (unsigned long long)(esr));
    (void)log_printf("%s", " pc=");
    (void)log_printf("0x%016llx", (unsigned long long)(pc));
    (void)log_printf("%s", " sctlr=");
    (void)log_printf("0x%016llx", (unsigned long long)(sctlr));
    (void)log_printf("%s", "\n");
    if (expected_fault == EXPECTED_FAULT_READ && pc == (uint64_t)probe_read_fault && (esr & ESR_WRITE_BIT) == 0UL) {
        fault_count++;
        return (uint64_t)probe_read_resume;
    }
    if (expected_fault == EXPECTED_FAULT_WRITE && pc == (uint64_t)probe_write_fault &&
        (esr & ESR_WRITE_BIT) != 0UL) {
        fault_count++;
        return (uint64_t)probe_write_resume;
    }
    return 0UL;
}
