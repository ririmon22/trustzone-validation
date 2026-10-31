#ifndef SIGNED_RPC_H
#define SIGNED_RPC_H
#include "client.h"
#ifndef B2_SIGNED_TA_PROFILE
#define B2_SIGNED_TA_PROFILE 0U
#endif
#define SIGNED_TA_MAX_BYTES 0x40000UL
#define SIGNED_BUNDLE_BYTES 0x100000UL
#define SIGNED_RPC_SLOTS 3U
#define SIGNED_RPC_BASE (SHARED_MEMORY_BASE + 0x10000UL)
#define SIGNED_RPC_END (SIGNED_RPC_BASE + SIGNED_RPC_SLOTS * SIGNED_TA_MAX_BYTES)
#define SIGNED_BUNDLE_MAGIC 0x5441424eU
#define SIGNED_CASES 8U
#define SIGNED_UUID_BYTES 16U
struct signed_image
{
    uint64_t offset, size;
    uint8_t hash[32];
};
struct signed_bundle
{
    uint32_t magic, version, bytes, count;
    uint8_t uuid[16];
    struct signed_image images[3];
};
struct signed_case_record
{
    uint32_t scenario, expected, expected_origin, invokes;
    struct tee_reply opened, echoed, queried, closed;
    uint32_t rpc_count, queries, copies, allocations, frees, live, unsupported, local_refusals;
    uint64_t declared_size;
};
struct signed_rpc_audit
{
    uint32_t version, bytes, state, completed;
    uint64_t run_id;
    uint32_t rpc_count, queries, copies, allocations, frees, live, unsupported, local_refusals;
    uint32_t current_scenario, rpc_function, rpc_command, poisoned;
    struct signed_case_record cases[SIGNED_CASES];
};
extern volatile uint8_t b2_signed_loader_snapshot[1040];
extern volatile struct tee_reply b2_signed_snapshot_status;
extern volatile struct signed_rpc_audit b2_signed_audit;
extern const uint8_t signed_ta_uuid[16];
void signed_rpc_publish(void);
bool signed_rpc_begin(void);
void signed_rpc_select(unsigned int variant);
bool signed_rpc_enabled(void);
bool signed_rpc_call(uint64_t message, struct smc_result *result);
bool signed_rpc_end(void);
void b2_run_signed_ta(void);
#endif
