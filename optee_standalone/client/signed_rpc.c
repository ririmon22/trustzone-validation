#include "signed_rpc.h"
#define RPC_PREFIX 0xffff0000UL
#define RPC_ALLOC 0U
#define RPC_FREE 2U
#define RPC_COMMAND 5U
#define RPC_LOAD_TA 0U
#define RPC_SHM_ALLOC 6U
#define RPC_SHM_FREE 7U
#define RPC_RETURN 0x32000003UL
#define RPC_STEP_LIMIT 128U
#define ERROR_NOT_FOUND 0xffff0008U
#define ERROR_NOT_SUPPORTED 0xffff000aU
#define ERROR_BAD_PARAMETERS 0xffff0006U
#define ERROR_SHORT_BUFFER 0xffff0010U
#define ATTRIBUTE_VALUE_IN 1UL
#define ATTRIBUTE_TMEM_OUT 10UL
#define ALLOCATION_ARG 1U
#define ALLOCATION_PAYLOAD 2U
struct rpc_parameter
{
    uint64_t attr, a, b, c;
};
struct rpc_argument
{
    uint32_t command, function, session, cancel, padding, result, origin, count;
    struct rpc_parameter parameters[2];
};
struct rpc_allocation
{
    uint64_t cookie, address, size;
    unsigned int kind;
};
struct rpc_registers
{
    uint64_t x[8];
};
volatile const uint32_t b2_signed_profile=B2_SIGNED_TA_PROFILE;
const uint64_t b2_signed_source[2] = {SIGNED_TA_SOURCE_BASE, SIGNED_BUNDLE_BYTES};
volatile struct signed_rpc_audit b2_signed_audit __attribute__((aligned(64)));
const uint8_t signed_ta_uuid[16] = {0x6d, 0xc1, 0x34, 0x1c, 0x53, 0x4f, 0x46, 0x42,
                                    0x91, 0x38, 0x21, 0x58, 0x76, 0x30, 0x49, 0x10};
static struct rpc_allocation allocations[SIGNED_RPC_SLOTS];
static uint64_t generation;
static bool active;
static unsigned int variant;
_Static_assert(sizeof(struct signed_bundle) == 176U, "bundle ABI");
_Static_assert(sizeof(struct rpc_argument) == 96U, "RPC ABI");
#if B2_SIGNED_TA_PROFILE
_Static_assert(SIGNED_RPC_BASE >= SHARED_MEMORY_BASE + 4096UL && SIGNED_RPC_END <= SHARED_MEMORY_END, "RPC arena exceeds SHM");
#endif
static bool equal_uuid(const volatile uint8_t *left, const uint8_t *right)
{
    for (unsigned int i = 0U; i < 16U; ++i)
    {
        if (left[i] != right[i])
        {
            return false;
        }
    }
    return true;
}
static void synchronize(void)
{
#ifndef B2_RPC_HOST_TEST
    __asm__ volatile("dsb sy" ::: "memory");
#endif
}
static struct rpc_registers exchange(struct rpc_registers regs)
{
#ifdef B2_RPC_HOST_TEST
    extern void b2_rpc_mock_exchange(uint64_t *registers);
    b2_rpc_mock_exchange(regs.x);
    return regs;
#else
    register uint64_t x0 __asm__("x0") = regs.x[0], x1 __asm__("x1") = regs.x[1];
    register uint64_t x2 __asm__("x2") = regs.x[2], x3 __asm__("x3") = regs.x[3];
    register uint64_t x4 __asm__("x4") = regs.x[4], x5 __asm__("x5") = regs.x[5];
    register uint64_t x6 __asm__("x6") = regs.x[6], x7 __asm__("x7") = regs.x[7];
    __asm__ volatile("smc #0"
                     : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5), "+r"(x6), "+r"(x7)::"x8",
                       "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "memory", "cc");
    return (struct rpc_registers){{x0, x1, x2, x3, x4, x5, x6, x7}};
#endif
}
static struct rpc_allocation *find_allocation(uint64_t cookie, unsigned int kind)
{
    for (unsigned int i = 0U; i < SIGNED_RPC_SLOTS; ++i)
    {
        if (cookie != 0UL && allocations[i].cookie == cookie && allocations[i].kind == kind)
        {
            return &allocations[i];
        }
    }
    return NULL;
}
static struct rpc_allocation *allocate(uint64_t size, uint64_t alignment, unsigned int kind)
{
    if (size == 0UL || size > SIGNED_TA_MAX_BYTES || alignment == 0UL || alignment > 4096UL ||
        (alignment & (alignment - 1UL)))
    {
        return NULL;
    }
    for (unsigned int i = 0U; i < SIGNED_RPC_SLOTS; ++i)
    {
        if (allocations[i].cookie != 0UL)
        {
            continue;
        }
        generation++;
        if (generation > (UINT64_MAX >> 8U))
        {
            return NULL;
        }
        allocations[i] =
            (struct rpc_allocation){(generation << 8U) | ((uint64_t)i + 1UL),
                                    SIGNED_RPC_BASE + (uint64_t)i * SIGNED_TA_MAX_BYTES, size, kind};
        volatile uint8_t *buffer = (volatile uint8_t *)allocations[i].address;
        for (uint64_t n = 0UL; n < size; ++n)
        {
            buffer[n] = 0U;
        }
        b2_signed_audit.allocations++;
        b2_signed_audit.live++;
        return &allocations[i];
    }
    return NULL;
}
static bool release(uint64_t cookie, unsigned int kind)
{
    struct rpc_allocation *allocation = find_allocation(cookie, kind);
    if (!allocation)
    {
        return false;
    }
    volatile uint8_t *buffer = (volatile uint8_t *)allocation->address;
    for (uint64_t i = 0UL; i < allocation->size; ++i)
    {
        buffer[i] = 0U;
    }
    *allocation = (struct rpc_allocation){0UL, 0UL, 0UL, 0U};
    b2_signed_audit.frees++;
    b2_signed_audit.live--;
    return true;
}
bool signed_rpc_begin(void)
{
    if (B2_SIGNED_TA_PROFILE != 1U || active)
    {
        return false;
    }
    const volatile struct signed_bundle *bundle =
        (const volatile struct signed_bundle *)SIGNED_TA_SOURCE_BASE;
    if (bundle->magic != SIGNED_BUNDLE_MAGIC || bundle->version != 1U ||
        bundle->bytes != SIGNED_BUNDLE_BYTES || bundle->count != 3U ||
        !equal_uuid(bundle->uuid, signed_ta_uuid))
    {
        return false;
    }
    for (unsigned int i = 0U; i < 3U; ++i)
    {
        uint64_t offset = bundle->images[i].offset, size = bundle->images[i].size;
        if (offset < 4096UL || offset > SIGNED_BUNDLE_BYTES || size == 0UL || size > SIGNED_TA_MAX_BYTES ||
            size > SIGNED_BUNDLE_BYTES - offset)
        {
            return false;
        }
        for (unsigned int j = 0U; j < i; ++j)
        {
            uint64_t other = bundle->images[j].offset, length = bundle->images[j].size;
            if (offset < other + length && other < offset + size)
            {
                return false;
            }
        }
    }
    volatile uint8_t *bytes = (volatile uint8_t *)&b2_signed_audit;
    for (unsigned int i = 0U; i < sizeof(b2_signed_audit); ++i)
    {
        bytes[i] = 0U;
    }
    b2_signed_audit.version = 2U;
    b2_signed_audit.bytes = sizeof(b2_signed_audit);
    b2_signed_audit.state = 1U;
    b2_signed_audit.run_id = debug_boot.run_id;
    active = true;
    variant = 0U;
    return true;
}
void signed_rpc_select(unsigned int selected)
{
    variant = selected;
}
bool signed_rpc_enabled(void)
{
    return active;
}
static void load_ta(volatile struct rpc_argument *arg)
{
    const volatile struct signed_bundle *bundle =
        (const volatile struct signed_bundle *)SIGNED_TA_SOURCE_BASE;
    if (arg->count != 2U || arg->parameters[0].attr != ATTRIBUTE_VALUE_IN ||
        arg->parameters[1].attr != ATTRIBUTE_TMEM_OUT)
    {
        return;
    }
    if (!equal_uuid((const volatile uint8_t *)&arg->parameters[0].a, signed_ta_uuid))
    {
        b2_signed_audit.local_refusals++;
        arg->result = ERROR_NOT_FOUND;
        return;
    }
    if (variant > 3U)
    {
        b2_signed_audit.local_refusals++;
        return;
    }
    unsigned int index = variant == 3U ? 0U : variant;
    uint64_t offset = bundle->images[index].offset,
             size = variant == 3U ? SIGNED_TA_MAX_BYTES + 1UL : bundle->images[index].size;
    if (b2_signed_audit.current_scenario < SIGNED_CASES)
    {
        b2_signed_audit.cases[b2_signed_audit.current_scenario].declared_size = size;
    }
    if (size > SIGNED_TA_MAX_BYTES || offset > SIGNED_BUNDLE_BYTES || size > SIGNED_BUNDLE_BYTES - offset)
    {
        b2_signed_audit.local_refusals++;
        return;
    }
    if (arg->parameters[1].a == 0UL && arg->parameters[1].c == 0UL && arg->parameters[1].b == 0UL)
    {
        arg->parameters[1].b = size;
        arg->result = TEE_SUCCESS;
        b2_signed_audit.queries++;
        return;
    }
    struct rpc_allocation *allocation = find_allocation(arg->parameters[1].c, ALLOCATION_PAYLOAD);
    if (!allocation || arg->parameters[1].a != allocation->address || arg->parameters[1].b < size ||
        arg->parameters[1].b > allocation->size)
    {
        b2_signed_audit.local_refusals++;
        return;
    }
    volatile uint8_t *destination = (volatile uint8_t *)allocation->address;
    const volatile uint8_t *source = (const volatile uint8_t *)(SIGNED_TA_SOURCE_BASE + offset);
    for (uint64_t i = 0UL; i < size; ++i)
    {
        destination[i] = source[i];
    }
    arg->parameters[1].b = size;
    arg->result = TEE_SUCCESS;
    b2_signed_audit.copies++;
}
static bool command(uint64_t cookie)
{
    struct rpc_allocation *allocation = find_allocation(cookie, ALLOCATION_ARG);
    if (!allocation || allocation->size < 32UL)
    {
        return false;
    }
    volatile struct rpc_argument *arg = (volatile struct rpc_argument *)allocation->address;
    if (arg->count > 2U || 32UL + (uint64_t)arg->count * 32UL > allocation->size)
    {
        return false;
    }
    b2_signed_audit.rpc_command = arg->command;
    arg->result = ERROR_BAD_PARAMETERS;
    arg->origin = 2U;
    if (arg->command == RPC_LOAD_TA)
    {
        load_ta(arg);
    }
    else if (arg->command == RPC_SHM_ALLOC && arg->count == 1U &&
             arg->parameters[0].attr == ATTRIBUTE_VALUE_IN && arg->parameters[0].a <= 1UL)
    {
        struct rpc_allocation *payload =
            allocate(arg->parameters[0].b, arg->parameters[0].c, ALLOCATION_PAYLOAD);
        if (payload)
        {
            arg->parameters[0] =
                (struct rpc_parameter){ATTRIBUTE_TMEM_OUT, payload->address, payload->size, payload->cookie};
            arg->result = TEE_SUCCESS;
        }
        else
        {
            arg->result = 0xffff000cU;
        }
    }
    else if (arg->command == RPC_SHM_FREE && arg->count == 1U &&
             arg->parameters[0].attr == ATTRIBUTE_VALUE_IN && arg->parameters[0].a <= 1UL)
    {
        arg->result = release(arg->parameters[0].b, ALLOCATION_PAYLOAD) ? TEE_SUCCESS : ERROR_BAD_PARAMETERS;
    }
    else
    {
        arg->result = ERROR_NOT_SUPPORTED;
        b2_signed_audit.unsupported++;
    }
    (void)log_printf("RPC command=%u ret=0x%08x live=%u\n", arg->command, arg->result, b2_signed_audit.live);
    return true;
}
bool signed_rpc_call(uint64_t message_address, struct smc_result *result)
{
    struct rpc_registers regs = {
        {0x32000004UL, message_address >> 32U, message_address & UINT32_MAX, 0UL, 0UL, 0UL, 0UL, 0UL}};
    for (unsigned int step = 0U; step < RPC_STEP_LIMIT; ++step)
    {
        synchronize();
        regs = exchange(regs);
        synchronize();
        if ((regs.x[0] & 0xffff0000UL) != RPC_PREFIX || regs.x[0] == UINT32_MAX)
        {
            *result = (struct smc_result){regs.x[0], regs.x[1], regs.x[2], regs.x[3]};
            return true;
        }
        b2_signed_audit.rpc_count++;
        b2_signed_audit.rpc_function = (uint32_t)(regs.x[0] & 0xffffUL);
        if (b2_signed_audit.rpc_function == RPC_ALLOC)
        {
            struct rpc_allocation *allocation = allocate(regs.x[1], 8UL, ALLOCATION_ARG);
            uint64_t address = allocation ? allocation->address : 0UL,
                     cookie = allocation ? allocation->cookie : 0UL;
            regs.x[1] = address >> 32U;
            regs.x[2] = address & UINT32_MAX;
            regs.x[4] = cookie >> 32U;
            regs.x[5] = cookie & UINT32_MAX;
        }
        else if (b2_signed_audit.rpc_function == RPC_FREE)
        {
            if (!release((regs.x[1] << 32U) | (regs.x[2] & UINT32_MAX), ALLOCATION_ARG))
            {
                break;
            }
        }
        else if (b2_signed_audit.rpc_function == RPC_COMMAND)
        {
            if (!command((regs.x[1] << 32U) | (regs.x[2] & UINT32_MAX)))
            {
                break;
            }
        }
        else
        {
            b2_signed_audit.unsupported++;
            break;
        }
        signed_rpc_publish();
        regs.x[0] = RPC_RETURN;
    }
    b2_signed_audit.poisoned = 1U;
    signed_rpc_publish();
    return false;
}
bool signed_rpc_end(void)
{
    if (b2_signed_audit.live != 0U || b2_signed_audit.poisoned != 0U || b2_signed_audit.unsupported != 0U)
    {
        b2_signed_audit.state = 3U;
        return false;
    }
    active = false;
    b2_signed_audit.state = 2U;
    return true;
}

/* RPC中断中でも、進行段階と所有中slotを停止済みJTAGから確認できるようにする。 */
void signed_rpc_publish(void)
{
#ifndef B2_RPC_HOST_TEST
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    if (sctlr & 4UL)
    {
        uintptr_t end = (uintptr_t)&b2_signed_audit + sizeof(b2_signed_audit);
        for (uintptr_t p = (uintptr_t)&b2_signed_audit; p < end; p += 64UL)
        {
            __asm__ volatile("dc cvac, %0" ::"r"(p) : "memory");
        }
    }
    __asm__ volatile("dsb sy" ::: "memory");
#endif
}
