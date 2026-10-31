#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <stdbool.h>
#include "validation_ta.h"
#include "security.h"
struct session_context { bool authenticated; uint32_t counter; };
#define AES_KEY_BITS 128U
#define AES_BLOCK_BYTES 16U
/* 生成鍵による往復と、別の既知鍵によるAES既知解を分けて検査する。 */
static TEE_Result run_crypto_test(void)
{
    TEE_ObjectHandle generated = TEE_HANDLE_NULL;
    TEE_ObjectHandle known = TEE_HANDLE_NULL;
    TEE_OperationHandle operation = TEE_HANDLE_NULL;
    uint8_t plain[AES_BLOCK_BYTES] = { 0U };
    uint8_t encrypted[AES_BLOCK_BYTES], decrypted[AES_BLOCK_BYTES];
    const uint8_t expected[AES_BLOCK_BYTES] = {
        0x66U, 0xe9U, 0x4bU, 0xd4U, 0xefU, 0x8aU, 0x2cU, 0x3bU,
        0x88U, 0x4cU, 0xfaU, 0x59U, 0xcaU, 0x34U, 0x2bU, 0x2eU
    };
    TEE_Result result = TEE_AllocateTransientObject(TEE_TYPE_AES, AES_KEY_BITS, &generated);
    if (result != TEE_SUCCESS) { goto cleanup; }
    result = TEE_GenerateKey(generated, AES_KEY_BITS, NULL, 0U);
    if (result != TEE_SUCCESS) { goto cleanup; }
    result = TEE_AllocateOperation(&operation, TEE_ALG_AES_ECB_NOPAD, TEE_MODE_ENCRYPT, AES_KEY_BITS);
    if (result != TEE_SUCCESS) { goto cleanup; }
    result = TEE_SetOperationKey(operation, generated);
    if (result != TEE_SUCCESS) { goto cleanup; }
    TEE_CipherInit(operation, NULL, 0U);
    size_t size = sizeof(encrypted);
    result = TEE_CipherDoFinal(operation, plain, sizeof(plain), encrypted, &size);
    if (result != TEE_SUCCESS || size != sizeof(encrypted)) { result = TEE_ERROR_GENERIC; goto cleanup; }
    TEE_FreeOperation(operation); operation = TEE_HANDLE_NULL;
    result = TEE_AllocateOperation(&operation, TEE_ALG_AES_ECB_NOPAD, TEE_MODE_DECRYPT, AES_KEY_BITS);
    if (result != TEE_SUCCESS) { goto cleanup; }
    result = TEE_SetOperationKey(operation, generated);
    if (result != TEE_SUCCESS) { goto cleanup; }
    TEE_CipherInit(operation, NULL, 0U);
    size = sizeof(decrypted);
    result = TEE_CipherDoFinal(operation, encrypted, sizeof(encrypted), decrypted, &size);
    if (result != TEE_SUCCESS || size != sizeof(decrypted) || TEE_MemCompare(plain, decrypted, sizeof(plain)) != 0) { result = TEE_ERROR_GENERIC; goto cleanup; }
    TEE_FreeOperation(operation); operation = TEE_HANDLE_NULL;
    result = TEE_AllocateTransientObject(TEE_TYPE_AES, AES_KEY_BITS, &known);
    if (result != TEE_SUCCESS) { goto cleanup; }
    TEE_Attribute attribute;
    TEE_InitRefAttribute(&attribute, TEE_ATTR_SECRET_VALUE, plain, sizeof(plain));
    result = TEE_PopulateTransientObject(known, &attribute, 1U);
    if (result != TEE_SUCCESS) { goto cleanup; }
    result = TEE_AllocateOperation(&operation, TEE_ALG_AES_ECB_NOPAD, TEE_MODE_ENCRYPT, AES_KEY_BITS);
    if (result != TEE_SUCCESS) { goto cleanup; }
    result = TEE_SetOperationKey(operation, known);
    if (result != TEE_SUCCESS) { goto cleanup; }
    TEE_CipherInit(operation, NULL, 0U);
    size = sizeof(encrypted);
    result = TEE_CipherDoFinal(operation, plain, sizeof(plain), encrypted, &size);
    if (result == TEE_SUCCESS && (size != sizeof(expected) || TEE_MemCompare(encrypted, expected, sizeof(expected)) != 0)) { result = TEE_ERROR_GENERIC; }
cleanup:
    if (operation != TEE_HANDLE_NULL) { TEE_FreeOperation(operation); }
    if (generated != TEE_HANDLE_NULL) { TEE_FreeTransientObject(generated); }
    if (known != TEE_HANDLE_NULL) { TEE_FreeTransientObject(known); }
    TEE_MemFill(encrypted, 0, sizeof(encrypted));
    TEE_MemFill(decrypted, 0, sizeof(decrypted));
    return result;
}
TEE_Result TA_CreateEntryPoint(void) { return security_create(); }
void TA_DestroyEntryPoint(void) { security_destroy(); }
TEE_Result TA_OpenSessionEntryPoint(uint32_t types, TEE_Param params[4], void **context)
{
    (void)params;
    if (types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) { return TEE_ERROR_BAD_PARAMETERS; }
    struct session_context *session = TEE_Malloc(sizeof(*session), TEE_MALLOC_FILL_ZERO);
    if (session == NULL) { return TEE_ERROR_OUT_OF_MEMORY; }
    *context = session; return TEE_SUCCESS;
}
void TA_CloseSessionEntryPoint(void *context)
{
    if (context != NULL) { TEE_MemFill(context, 0, sizeof(struct session_context)); TEE_Free(context); }
}
TEE_Result TA_InvokeCommandEntryPoint(void *context, uint32_t command, uint32_t types, TEE_Param params[4])
{
    struct session_context *session = context;
    if (session == NULL) { return TEE_ERROR_BAD_STATE; }
    if (command == TA_COMMAND_SESSION_COUNTER) {
        /* session固有のSecure状態だけを進め、他sessionやNormalの状態とは共有しない。 */
        if (types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT, TEE_PARAM_TYPE_NONE,
                                    TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) { return TEE_ERROR_BAD_PARAMETERS; }
        if (session->counter == UINT32_MAX) { return TEE_ERROR_OVERFLOW; }
        params[0].value.a = ++session->counter;
        return TEE_SUCCESS;
    }
    if (command >= TA_COMMAND_TRUSTED_READ) { return security_invoke(command, types, params); }
    if (session == NULL || types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) { return TEE_ERROR_BAD_PARAMETERS; }
    switch (command) {
    case TA_COMMAND_ECHO: return TEE_SUCCESS;
    case TA_COMMAND_AUTHENTICATE:
        session->authenticated = params[0].value.a == TA_TEST_CREDENTIAL;
        return session->authenticated ? TEE_SUCCESS : TEE_ERROR_ACCESS_DENIED;
    case TA_COMMAND_READ:
        if (!session->authenticated) { return TEE_ERROR_ACCESS_DENIED; }
        params[0].value.a = TA_PRIVATE_VALUE; return TEE_SUCCESS;
    case TA_COMMAND_CRYPTO: return run_crypto_test();
    case TA_COMMAND_PANIC: TEE_Panic(0U); return TEE_ERROR_GENERIC;
    default: return TEE_ERROR_BAD_PARAMETERS;
    }
}
