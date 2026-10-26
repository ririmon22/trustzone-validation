#ifndef TEE_CLIENT_H
#define TEE_CLIENT_H
#include "runtime.h"
#define TEE_CLIENT_OK 0U
#define TEE_CLIENT_TRANSPORT 1U
#define TEE_CLIENT_RPC_REQUIRED 2U
#define TEE_CLIENT_SHM_UNAVAILABLE 3U
#define TEE_CLIENT_BAD_STATE 4U
#define TEE_CLIENT_MAX_SESSIONS 4U
#define TEE_SUCCESS 0U
#define TEE_ERROR_ACCESS_DENIED 0xffff0001U
#define TEE_ERROR_TARGET_DEAD 0xffff3024U
#define TEE_ERROR_BAD_PARAMETERS 0xffff0006U
#include "../ta/validation_ta.h"
#include "../pta/validation_canary.h"
struct tee_session { uint32_t id; bool open; };
struct tee_reply { uint32_t transport; uint32_t result; uint32_t origin; uint32_t value; };
uint32_t tee_client_initialize(void);
unsigned int tee_client_active_sessions(void);
struct tee_reply tee_probe_identity_open(void);
struct tee_reply tee_session_open(struct tee_session *session, const uint8_t uuid[16]);
struct tee_reply tee_invoke(struct tee_session *session, uint32_t command, uint32_t value);
struct tee_reply tee_invoke_buffer(struct tee_session *session, uint64_t address, uint64_t size);
struct tee_reply tee_probe_buffer_unchecked(struct tee_session *session, uint64_t address, uint64_t size);
struct tee_reply tee_session_close(struct tee_session *session);
extern const uint8_t validation_pta_uuid[16];
extern const uint8_t validation_ta_uuid[16];
extern const uint8_t validation_ta_b_uuid[16];
#endif
