#include "client.h"
#include <assert.h>
#include <string.h>
#define GET_SHM 0xb2000007UL
#define CAPABILITIES 0xb2000009UL
#define CLOSE 2U
_Alignas(8) uint8_t client_shared_buffer[8192];
static unsigned int live_sessions;
static bool close_error;
struct smc_result smc_call(uint64_t fid, uint64_t a1, uint64_t a2, uint64_t a3)
{
    (void)a1; (void)a2; (void)a3;
    if (fid == CAPABILITIES) { return (struct smc_result){ 0U, 1U, 0U, 0U }; }
    if (fid == GET_SHM) { return (struct smc_result){ 0U, SHARED_MEMORY_BASE, sizeof(client_shared_buffer), 0U }; }
    uint32_t *words = (uint32_t *)client_shared_buffer;
    words[5] = TEE_SUCCESS;
    words[6] = 3U;
    if (words[0] == 0U) { words[2] = ++live_sessions; }
    else if (words[0] == 1U) { words[5] = TEE_ERROR_TARGET_DEAD; }
    else if (words[0] == CLOSE) {
        if (close_error) { words[5] = TEE_ERROR_BAD_PARAMETERS; }
        else { assert(live_sessions > 0U); --live_sessions; }
    }
    return (struct smc_result){ 0U, 0U, 0U, 0U };
}
int main(int count, char **arguments)
{
    assert(tee_client_initialize() == TEE_CLIENT_OK);
    close_error = count == 2 && strcmp(arguments[1], "close-error") == 0;
    struct tee_session session = { 0U, false };
    if (close_error) {
        assert(tee_session_open(&session, validation_ta_uuid).result == TEE_SUCCESS);
        assert(tee_invoke(&session, TA_COMMAND_PANIC, 0U).transport == TEE_CLIENT_BAD_STATE);
        assert(tee_session_open(&session, validation_ta_uuid).transport == TEE_CLIENT_BAD_STATE);
        return 0;
    }
    for (unsigned int index = 0U; index < 512U; ++index) {
        assert(tee_session_open(&session, validation_ta_uuid).result == TEE_SUCCESS);
        struct tee_reply reply = tee_invoke(&session, TA_COMMAND_PANIC, 0U);
        assert(reply.transport == TEE_CLIENT_OK && reply.result == TEE_ERROR_TARGET_DEAD);
        assert(!session.open && live_sessions == 0U);
    }
    return 0;
}
