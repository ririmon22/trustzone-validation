#include "runtime.h"
#define ESR_EC_SHIFT 26U
#define ESR_EC_MASK 63U
#define ESR_EC_SMC64 23U
#define NONSECURE_CONTEXT 0U
#define SECURE_CONTEXT 1U
#define SMC_UNKNOWN UINT64_MAX
struct world_context {
    uint64_t x[31];
    uint64_t pc;
    uint64_t pstate;
    uint64_t sp;
};
_Static_assert(sizeof(struct world_context) == 272U, "frame layout");
static struct world_context contexts[2];
struct el1_context {
    uint64_t vbar, sctlr, elr, spsr, tpidr, tpidrro, sp_el0;
};
static struct el1_context system_contexts[2];
static unsigned int current_world = SECURE_CONTEXT;
static bool secure_ready;
static bool request_pending;
extern _Noreturn void enter_world(struct world_context *frame);
static void copy_context(struct world_context *to, const struct world_context *from)
{
    *to = *from;
}
static void select_world(unsigned int world)
{
    uint64_t scr = SCR_RW | (world == NONSECURE_CONTEXT ? SCR_NS : 0UL);
    __asm__ volatile("msr scr_el3, %0; isb" :: "r"(scr) : "memory");
    current_world = world;
}
void monitor_dispatch(struct world_context *frame)
{
    uint64_t esr;
    __asm__ volatile("mrs %0, esr_el3" : "=r"(esr));
    if (((esr >> ESR_EC_SHIFT) & ESR_EC_MASK) != ESR_EC_SMC64) {
        fatal_exception(esr, 0UL, frame->pc);
    }
    uint64_t fid = frame->x[0];
    unsigned int next = current_world;
    struct el1_context *outgoing = &system_contexts[current_world];
    __asm__ volatile("mrs %0, vbar_el1; mrs %1, sctlr_el1; mrs %2, elr_el1; mrs %3, spsr_el1; mrs %4, tpidr_el1; mrs %5, tpidrro_el0; mrs %6, sp_el0"
        : "=r"(outgoing->vbar), "=r"(outgoing->sctlr), "=r"(outgoing->elr), "=r"(outgoing->spsr), "=r"(outgoing->tpidr), "=r"(outgoing->tpidrro), "=r"(outgoing->sp_el0));
    copy_context(&contexts[current_world], frame);
    if (current_world == SECURE_CONTEXT && fid == SMC_SECURE_READY && !secure_ready) {
        secure_ready = true;
        next = NONSECURE_CONTEXT;
    } else if (current_world == NONSECURE_CONTEXT && fid == SMC_RUN_SERVICE && secure_ready && !request_pending) {
        request_pending = true;
        contexts[SECURE_CONTEXT].x[0] = frame->x[1];
        contexts[SECURE_CONTEXT].x[1] = frame->x[2];
        next = SECURE_CONTEXT;
    } else if (current_world == SECURE_CONTEXT && fid == SMC_SERVICE_RETURN && request_pending) {
        request_pending = false;
        contexts[NONSECURE_CONTEXT].x[0] = frame->x[1];
        next = NONSECURE_CONTEXT;
    } else {
        contexts[current_world].x[0] = SMC_UNKNOWN;
    }
    select_world(next);
    const struct el1_context *incoming = &system_contexts[next];
    __asm__ volatile("msr vbar_el1, %0; msr sctlr_el1, %1; msr elr_el1, %2; msr spsr_el1, %3; msr tpidr_el1, %4; msr tpidrro_el0, %5; msr sp_el0, %6; isb"
        :: "r"(incoming->vbar), "r"(incoming->sctlr), "r"(incoming->elr), "r"(incoming->spsr), "r"(incoming->tpidr), "r"(incoming->tpidrro), "r"(incoming->sp_el0) : "memory");
    copy_context(frame, &contexts[next]);
}
_Noreturn void monitor_main(void)
{
    if (read_current_el() != EXPECTED_EL3) { console_write("FATAL expected EL3\n"); platform_halt(); }
    console_write("BOOT EL3\n");
    if (PLATFORM_PROTECTION_CONFIRMED == 0) { console_write("BLOCKED unconfirmed platform protection\n"); platform_halt(); }
    __asm__ volatile("msr vbar_el3, %0; msr cptr_el3, xzr; isb" :: "r"(exception_vectors) : "memory");
    /* 両EL1はMMUとキャッシュを無効化した既知状態で開始する。 */
    for (unsigned int world = 0U; world < 2U; ++world) {
        select_world(world);
        uint64_t scr;
        __asm__ volatile("mrs %0, scr_el3" : "=r"(scr));
        console_write("MEM_WORLD world="); console_write(world == SECURE_CONTEXT ? "secure" : "nonsecure"); console_write(" scr="); console_hex(scr); console_write("\n");
        __asm__ volatile("msr sctlr_el1, %0; msr cntkctl_el1, %1; isb" :: "r"(SCTLR_EL1_RES1), "r"(1UL) : "memory");
        contexts[world].pc = world == SECURE_CONTEXT ? SECURE_CODE_BASE : NONSECURE_CODE_BASE;
        contexts[world].sp = world == SECURE_CONTEXT ? SECURE_STACK_END : NONSECURE_STACK_END;
        contexts[world].pstate = SPSR_EL1H_MASKED;
        system_contexts[world].sctlr = SCTLR_EL1_RES1;
    }
    select_world(NONSECURE_CONTEXT);
    __asm__ volatile("msr hcr_el2, %0; msr cnthctl_el2, %1; msr cntvoff_el2, xzr; isb" :: "r"(1UL << 31U), "r"(3UL) : "memory");
    uint64_t hcr;
    __asm__ volatile("mrs %0, hcr_el2" : "=r"(hcr));
    console_write("MEM_HCR value="); console_hex(hcr); console_write("\n");
    select_world(SECURE_CONTEXT);
    struct world_context *frame = (struct world_context *)(EL3_STACK_END - sizeof(struct world_context));
    copy_context(frame, &contexts[SECURE_CONTEXT]);
    enter_world(frame);
}
