#include "client.h"
#ifdef B2_SIGNED_TA_PROFILE
#include "signed_rpc.h"
#endif
#define OPTEE_SMC_GET_SHM_CONFIG 0xb2000007UL
#define OPTEE_SMC_EXCHANGE_CAPABILITIES 0xb2000009UL
#define OPTEE_SMC_CALL_WITH_ARG 0x32000004UL
#define OPTEE_SMC_RETURN_OK 0UL
#define OPTEE_SMC_RPC_MASK 0xffff0000UL
#define OPTEE_SMC_RPC_PREFIX 0xffff0000UL
#define OPTEE_NS_CAP_UNIPROCESSOR 1UL
#define OPTEE_SEC_CAP_RESERVED_SHM 1UL
#define OPTEE_MSG_OPEN_SESSION 0U
#define OPTEE_MSG_INVOKE_COMMAND 1U
#define OPTEE_MSG_CLOSE_SESSION 2U
#define OPTEE_MSG_ATTR_VALUE_INPUT 1UL
#define OPTEE_MSG_ATTR_VALUE_INOUT 3UL
#define OPTEE_MSG_ATTR_TMEM_INPUT 9UL
#define OPTEE_MSG_ATTR_META (1UL << 8)
#define OPTEE_LOGIN_PUBLIC 0UL
#define OPTEE_LOGIN_TRUSTED_APP 0xf0000000UL
#define ADDRESS_HIGH_SHIFT 32U
struct message_parameter { uint64_t attr; uint64_t a; uint64_t b; uint64_t c; };
struct message_argument {
    uint32_t command, function, session, cancel_id, padding, result, origin, count;
    struct message_parameter parameters[2];
};
_Static_assert(offsetof(struct message_argument, parameters) == 32U, "message ABI header");
_Static_assert(sizeof(struct message_parameter) == 32U, "message ABI parameter");
static volatile struct message_argument *message;
static unsigned int active_sessions;
static uint64_t shared_base;
static uint64_t shared_end;
static bool transport_poisoned;
/* UUIDはOP-TEEメッセージのバイト列形式で定義する。 */
const uint8_t validation_pta_uuid[16] = {
    0x6d, 0xc1, 0x34, 0x1a, 0x53, 0x4f, 0x46, 0x42,
    0x91, 0x38, 0x21, 0x58, 0x76, 0x30, 0x49, 0x10
};
const uint8_t validation_ta_uuid[16] = {
    0x6d, 0xc1, 0x34, 0x18, 0x53, 0x4f, 0x46, 0x42,
    0x91, 0x38, 0x21, 0x58, 0x76, 0x30, 0x49, 0x10
};
const uint8_t validation_ta_b_uuid[16] = {
    0x6d, 0xc1, 0x34, 0x19, 0x53, 0x4f, 0x46, 0x42,
    0x91, 0x38, 0x21, 0x58, 0x76, 0x30, 0x49, 0x10
};
static void reset_message(void)
{
    volatile uint64_t *words = (volatile uint64_t *)message;
    for (unsigned int i = 0U; i < sizeof(*message) / sizeof(uint64_t); ++i) { words[i] = 0UL; }
}
static void synchronize_message(void)
{
#ifndef TEE_CLIENT_HOST_TEST
    __asm__ volatile("dsb sy" ::: "memory");
#endif
}
static struct tee_reply transport_call(void)
{
    uint64_t address = (uint64_t)message;
    synchronize_message();
    struct smc_result result;
#if defined(B2_SIGNED_TA_PROFILE) && B2_SIGNED_TA_PROFILE
    if (signed_rpc_enabled()) {
        if (!signed_rpc_call(address,&result)) { transport_poisoned=true;return (struct tee_reply){TEE_CLIENT_RPC_REQUIRED,0U,0U,0U}; }
    } else
#endif
    { result = smc_call(OPTEE_SMC_CALL_WITH_ARG, address >> ADDRESS_HIGH_SHIFT, address & UINT32_MAX, 0UL); }
    synchronize_message();
    if (result.x0 != OPTEE_SMC_RETURN_OK) {
        /* 未実装RPCを成功扱いせず、停止中の呼出しを再利用しない。 */
        transport_poisoned = true;
        return (struct tee_reply){ result.x0 != UINT32_MAX && (result.x0 & OPTEE_SMC_RPC_MASK) == OPTEE_SMC_RPC_PREFIX ? TEE_CLIENT_RPC_REQUIRED : TEE_CLIENT_TRANSPORT, 0U, 0U, 0U };
    }
    return (struct tee_reply){ TEE_CLIENT_OK, message->result, message->origin, (uint32_t)message->parameters[0].a };
}
uint32_t tee_client_initialize(void)
{
    if (message != NULL || transport_poisoned) { return TEE_CLIENT_BAD_STATE; }
    struct smc_result capabilities = smc_call(OPTEE_SMC_EXCHANGE_CAPABILITIES, OPTEE_NS_CAP_UNIPROCESSOR, 0UL, 0UL);
    if (capabilities.x0 != OPTEE_SMC_RETURN_OK || (capabilities.x1 & OPTEE_SEC_CAP_RESERVED_SHM) == 0UL) { return TEE_CLIENT_SHM_UNAVAILABLE; }
    struct smc_result config = smc_call(OPTEE_SMC_GET_SHM_CONFIG, 0UL, 0UL, 0UL);
    if (config.x0 != OPTEE_SMC_RETURN_OK || config.x1 < SHARED_MEMORY_BASE ||
        config.x1 >= SHARED_MEMORY_END || config.x2 < sizeof(*message) ||
        config.x2 > SHARED_MEMORY_END - config.x1 || (config.x1 & 7UL) != 0UL) { return TEE_CLIENT_SHM_UNAVAILABLE; }
#ifdef TEE_SHARED_CACHED
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    if (config.x3 != 1UL || (sctlr & 5UL) != 5UL) { return TEE_CLIENT_SHM_UNAVAILABLE; }
#else
    if (config.x3 != 0UL) { return TEE_CLIENT_SHM_UNAVAILABLE; }
#endif
    shared_base = config.x1;
    shared_end = config.x1 + config.x2;
    message = (volatile struct message_argument *)config.x1;
    return TEE_CLIENT_OK;
}
struct tee_reply tee_session_open(struct tee_session *session, const uint8_t uuid[16])
{
    if (message == NULL || transport_poisoned || session == NULL || uuid == NULL || session->open || active_sessions >= TEE_CLIENT_MAX_SESSIONS) {
        return (struct tee_reply){ TEE_CLIENT_BAD_STATE, 0U, 0U, 0U };
    }
    reset_message();
    message->command = OPTEE_MSG_OPEN_SESSION;
    message->count = 2U;
    message->parameters[0].attr = OPTEE_MSG_ATTR_VALUE_INPUT | OPTEE_MSG_ATTR_META;
    volatile uint8_t *destination = (volatile uint8_t *)&message->parameters[0].a;
    for (unsigned int i = 0U; i < 16U; ++i) { destination[i] = uuid[i]; }
    message->parameters[1].attr = OPTEE_MSG_ATTR_VALUE_INPUT | OPTEE_MSG_ATTR_META;
    message->parameters[1].c = OPTEE_LOGIN_PUBLIC;
    struct tee_reply reply = transport_call();
    if (reply.transport == TEE_CLIENT_OK && reply.result == TEE_SUCCESS) {
        session->id = message->session;
        session->open = true;
        active_sessions++;
    }
    return reply;
}
struct tee_reply tee_invoke(struct tee_session *session, uint32_t command, uint32_t value)
{
    if (message == NULL || transport_poisoned || session == NULL || !session->open) { return (struct tee_reply){ TEE_CLIENT_BAD_STATE, 0U, 0U, 0U }; }
    reset_message();
    message->command = OPTEE_MSG_INVOKE_COMMAND;
    message->session = session->id;
    message->function = command;
    message->count = 1U;
    message->parameters[0].attr = OPTEE_MSG_ATTR_VALUE_INOUT;
    message->parameters[0].a = value;
    struct tee_reply reply = transport_call();
    if (reply.transport == TEE_CLIENT_OK && reply.result == TEE_ERROR_TARGET_DEAD) {
        /* TARGET_DEADでもCore側sessionをcloseして参照とmemoryを解放する。 */
        reset_message();
        message->command = OPTEE_MSG_CLOSE_SESSION;
        message->session = session->id;
        struct tee_reply closed = transport_call();
        if (closed.transport != TEE_CLIENT_OK || closed.result != TEE_SUCCESS) {
            transport_poisoned = true;
            return (struct tee_reply){ TEE_CLIENT_BAD_STATE, closed.result, closed.origin, 0U };
        }
        session->open = false;
        session->id = 0U;
        active_sessions--;
    }
    return reply;
}
struct tee_reply tee_session_close(struct tee_session *session)
{
    if (message == NULL || transport_poisoned || session == NULL || !session->open) { return (struct tee_reply){ TEE_CLIENT_BAD_STATE, 0U, 0U, 0U }; }
    reset_message();
    message->command = OPTEE_MSG_CLOSE_SESSION;
    message->session = session->id;
    struct tee_reply reply = transport_call();
    if (reply.transport == TEE_CLIENT_OK && reply.result == TEE_SUCCESS) { session->open = false; session->id = 0U; active_sessions--; }
    return reply;
}

/* 攻撃試験専用。pointerをNS側で参照せずCoreへ不正範囲も送信する。 */
struct tee_reply tee_probe_buffer_unchecked(struct tee_session *session, uint64_t address, uint64_t size)
{
    if (message == NULL || transport_poisoned || session == NULL || !session->open) {
        return (struct tee_reply){ TEE_CLIENT_BAD_STATE, 0U, 0U, 0U };
    }
    reset_message();
    message->command = OPTEE_MSG_INVOKE_COMMAND;
    message->session = session->id;
    message->function = TA_COMMAND_BUFFER_CHECK;
    message->count = 1U;
    message->parameters[0].attr = OPTEE_MSG_ATTR_TMEM_INPUT;
    message->parameters[0].a = address;
    message->parameters[0].b = size;
    return transport_call();
}
struct tee_reply tee_invoke_buffer(struct tee_session *session, uint64_t address, uint64_t size)
{
    if (address < shared_base || address >= shared_end ||
        size > shared_end - address || size == 0UL ||
        (address < (uint64_t)message + sizeof(*message) && address + size > (uint64_t)message)) {
        return (struct tee_reply){ TEE_CLIENT_SHM_UNAVAILABLE, TEE_ERROR_BAD_PARAMETERS, 0U, 0U };
    }
    return tee_probe_buffer_unchecked(session, address, size);
}

/* Normal側からTRUSTED_APPを自己申告してもCoreが拒否することを検査する。 */
struct tee_reply tee_probe_identity_open(void)
{
    if (message == NULL || transport_poisoned) { return (struct tee_reply){ TEE_CLIENT_BAD_STATE, 0U, 0U, 0U }; }
    reset_message();
    message->command = OPTEE_MSG_OPEN_SESSION;
    message->count = 2U;
    message->parameters[0].attr = OPTEE_MSG_ATTR_VALUE_INPUT | OPTEE_MSG_ATTR_META;
    message->parameters[1].attr = OPTEE_MSG_ATTR_VALUE_INPUT | OPTEE_MSG_ATTR_META;
    volatile uint8_t *target = (volatile uint8_t *)&message->parameters[0].a;
    volatile uint8_t *claimed = (volatile uint8_t *)&message->parameters[1].a;
    for (unsigned int i = 0U; i < 16U; ++i) { target[i] = validation_ta_b_uuid[i]; claimed[i] = validation_ta_uuid[i]; }
    message->parameters[1].c = OPTEE_LOGIN_TRUSTED_APP;
    struct tee_reply reply = transport_call();
    if (reply.transport == TEE_CLIENT_OK && reply.result == TEE_SUCCESS) {
        uint32_t unexpected_session = message->session;
        reset_message();
        message->command = OPTEE_MSG_CLOSE_SESSION;
        message->session = unexpected_session;
        (void)transport_call();
    }
    return reply;
}

unsigned int tee_client_active_sessions(void)
{
    return active_sessions;
}

bool tee_client_shared_range(uint64_t *base, uint64_t *end)
{
    if (message == NULL || base == NULL || end == NULL || transport_poisoned) { return false; }
    *base = shared_base;
    *end = shared_end;
    return true;
}
/* ABI攻撃試験だけで使用する。不正属性をCoreへ送り拒否後の継続性を確認する。 */
struct tee_reply tee_probe_attribute(struct tee_session *session, uint64_t attr)
{
    if (message == NULL || transport_poisoned || session == NULL || !session->open) {
        return (struct tee_reply){ TEE_CLIENT_BAD_STATE, 0U, 0U, 0U };
    }
    reset_message();
    message->command = OPTEE_MSG_INVOKE_COMMAND;
    message->session = session->id;
    message->function = TA_COMMAND_ECHO;
    message->count = 1U;
    message->parameters[0].attr = attr;
    return transport_call();
}

/* 閉じたIDをCoreへ直接送る試験用。通常APIのlocal状態検査とは区別する。 */
struct tee_reply tee_probe_session_id(uint32_t session_id)
{
    if (message == NULL || transport_poisoned) {
        return (struct tee_reply){ TEE_CLIENT_BAD_STATE, 0U, 0U, 0U };
    }
    reset_message();
    message->command = OPTEE_MSG_INVOKE_COMMAND;
    message->session = session_id;
    message->function = TA_COMMAND_ECHO;
    message->count = 1U;
    message->parameters[0].attr = OPTEE_MSG_ATTR_VALUE_INOUT;
    return transport_call();
}

/* TAを経由しないSecure Coreとのfast SMC往復を検査する。 */
bool tee_client_probe_configuration(void)
{
    if (message == NULL || transport_poisoned) { return false; }
    struct smc_result config = smc_call(OPTEE_SMC_GET_SHM_CONFIG, 0UL, 0UL, 0UL);
#ifdef TEE_SHARED_CACHED
    const uint64_t cached = 1UL;
#else
    const uint64_t cached = 0UL;
#endif
    return config.x0 == OPTEE_SMC_RETURN_OK && config.x1 == shared_base &&
        config.x2 == shared_end - shared_base && config.x3 == cached;
}

/* 読出し専用検証PTAの固定長snapshotだけに使う汎用output memref。 */
struct tee_reply tee_invoke_output(struct tee_session *session,uint32_t command,uint64_t address,uint64_t bytes)
{
    if (message==NULL || transport_poisoned || session==NULL || !session->open || bytes==0UL ||
        address<shared_base+sizeof(*message) || address>=shared_end || bytes>shared_end-address) {
        return (struct tee_reply){TEE_CLIENT_SHM_UNAVAILABLE,0U,0U,0U};
    }
    reset_message();message->command=OPTEE_MSG_INVOKE_COMMAND;message->session=session->id;
    message->function=command;message->count=1U;
    message->parameters[0].attr=10UL;message->parameters[0].a=address;message->parameters[0].b=bytes;
    return transport_call();
}
