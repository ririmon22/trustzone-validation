#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include <utee_syscalls.h>
#include <stdint.h>
#include "validation_ta.h"
#include "security.h"
#define AES_BITS 128U
#define AES_BYTES 16U
#define KEY_ATTRIBUTE_SENTINEL 0xa5U
#define BUFFER_BYTES 16U
#define BUFFER_SEED 0x50U
#define PROBE_REPLACEMENT 0xdead1357U
#define READ_ACCESS (TEE_MEMORY_ACCESS_READ | TEE_MEMORY_ACCESS_ANY_OWNER)
#define WRITE_ACCESS (TEE_MEMORY_ACCESS_WRITE | TEE_MEMORY_ACCESS_ANY_OWNER)
static volatile uint32_t private_secret = TA_PRIVATE_VALUE;
static TEE_ObjectHandle private_key = TEE_HANDLE_NULL;

/* TA instance内だけに鍵を保持し、抽出許可を除去する。 */
TEE_Result security_create(void)
{
    TEE_Result result = TEE_AllocateTransientObject(TEE_TYPE_AES, AES_BITS, &private_key);
    if (result == TEE_SUCCESS) { result = TEE_GenerateKey(private_key, AES_BITS, NULL, 0U); }
    if (result == TEE_SUCCESS) { result = TEE_RestrictObjectUsage1(private_key, TEE_USAGE_ENCRYPT | TEE_USAGE_DECRYPT); }
    if (result != TEE_SUCCESS) { security_destroy(); }
    return result;
}
void security_destroy(void)
{
    if (private_key != TEE_HANDLE_NULL) { TEE_FreeTransientObject(private_key); private_key = TEE_HANDLE_NULL; }
}

/* identityは要求parameterではなくCore管理のsession propertyから取得する。 */
static TEE_Result trusted_read(TEE_Param params[4])
{
    const TEE_UUID allowed = VALIDATION_TA_A_UUID;
    TEE_Identity identity;
    TEE_Result result = TEE_GetPropertyAsIdentity(TEE_PROPSET_CURRENT_CLIENT, "gpd.client.identity", &identity);
    if (result != TEE_SUCCESS) { return result; }
#ifdef TA_VARIANT_B
    if (identity.login != TEE_LOGIN_TRUSTED_APP || TEE_MemCompare(&identity.uuid, &allowed, sizeof(allowed)) != 0) { return TEE_ERROR_ACCESS_DENIED; }
    params[0].value.a = private_secret;
    return TEE_SUCCESS;
#else
    (void)params; (void)allowed;
    return TEE_ERROR_ACCESS_DENIED;
#endif
}

/* PUBLICからは自己試験だけを起動でき、Bの秘密値は返さない。 */
static TEE_Result trusted_service_test(TEE_Param params[4])
{
#ifdef TA_VARIANT_B
    (void)params;
    return TEE_ERROR_ACCESS_DENIED;
#else
    const TEE_UUID target = VALIDATION_TA_B_UUID;
    TEE_TASessionHandle session = TEE_HANDLE_NULL;
    uint32_t origin = 0U;
    TEE_Param arguments[4] = { 0 };
    TEE_Result result = TEE_OpenTASession(&target, TEE_TIMEOUT_INFINITE, 0U, arguments, &session, &origin);
    if (result != TEE_SUCCESS) { return result; }
    result = TEE_InvokeTACommand(session, TEE_TIMEOUT_INFINITE, TA_COMMAND_TRUSTED_READ,
        TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE), arguments, &origin);
    if (result == TEE_SUCCESS && arguments[0].value.a != TA_PRIVATE_VALUE_B) { result = TEE_ERROR_SECURITY; }
    TEE_CloseTASession(session);
    params[0].value.a = 0U;
    return result;
#endif
}

static TEE_Result key_policy_test(void)
{
    TEE_ObjectInfo info;
    TEE_Result result = TEE_GetObjectInfo1(private_key, &info);
    if (result != TEE_SUCCESS) { return result; }
    if ((info.objectUsage & TEE_USAGE_EXTRACTABLE) != 0U) { return TEE_ERROR_SECURITY; }
    TEE_OperationHandle operation = TEE_HANDLE_NULL;
    result = TEE_AllocateOperation(&operation, TEE_ALG_AES_ECB_NOPAD, TEE_MODE_ENCRYPT, AES_BITS);
    uint8_t plain[AES_BYTES] = { 0U }, encrypted[AES_BYTES];
    size_t size = sizeof(encrypted);
    if (result == TEE_SUCCESS) { result = TEE_SetOperationKey(operation, private_key); }
    if (result == TEE_SUCCESS) {
        TEE_CipherInit(operation, NULL, 0U);
        result = TEE_CipherDoFinal(operation, plain, sizeof(plain), encrypted, &size);
        if (size != sizeof(encrypted)) { result = TEE_ERROR_SECURITY; }
    }
    if (operation != TEE_HANDLE_NULL) { TEE_FreeOperation(operation); }
    TEE_MemFill(encrypted, 0, sizeof(encrypted));
    return result;
}

/* Coreの属性取得syscallを直接検査する。通常APIが行うpanicとは分ける。 */
static TEE_Result key_attribute_test(void)
{
    TEE_ObjectHandle control = TEE_HANDLE_NULL;
    uint8_t buffer[AES_BYTES];
    uint64_t size = sizeof(buffer);
    TEE_Result result = TEE_AllocateTransientObject(TEE_TYPE_AES, AES_BITS, &control);
    if (result != TEE_SUCCESS) { return result; }
    result = TEE_GenerateKey(control, AES_BITS, NULL, 0U);
    if (result != TEE_SUCCESS) { goto cleanup; }
    /* 許可対象では実際の属性取得を成功させ、拒否だけで合格にしない。 */
    result = _utee_cryp_obj_get_attr((unsigned long)control, TEE_ATTR_SECRET_VALUE, buffer, &size);
    if (result != TEE_SUCCESS || size != sizeof(buffer)) { result = TEE_ERROR_SECURITY; goto cleanup; }
    TEE_MemFill(buffer, KEY_ATTRIBUTE_SENTINEL, sizeof(buffer));
    size = sizeof(buffer);
    result = _utee_cryp_obj_get_attr((unsigned long)private_key, TEE_ATTR_SECRET_VALUE, buffer, &size);
    if (result != TEE_ERROR_BAD_PARAMETERS) { result = TEE_ERROR_SECURITY; goto cleanup; }
    for (size_t i = 0U; i < sizeof(buffer); ++i) {
        if (buffer[i] != KEY_ATTRIBUTE_SENTINEL) { result = TEE_ERROR_SECURITY; goto cleanup; }
    }
    result = key_policy_test();
cleanup:
    TEE_MemFill(buffer, 0, sizeof(buffer));
    TEE_FreeTransientObject(control);
    return result;
}

TEE_Result security_invoke(uint32_t command, uint32_t types, TEE_Param params[4])
{
    if (command == TA_COMMAND_BUFFER_CHECK) {
        if (types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE) ||
            params[0].memref.size != BUFFER_BYTES || params[0].memref.buffer == NULL) { return TEE_ERROR_BAD_PARAMETERS; }
        /* 外部共有bufferを一度だけprivate領域へコピーして検査する。 */
        uint8_t snapshot[BUFFER_BYTES];
        TEE_MemMove(snapshot, params[0].memref.buffer, sizeof(snapshot));
        for (size_t i = 0U; i < sizeof(snapshot); ++i) {
            if (snapshot[i] != (uint8_t)(BUFFER_SEED + i)) { return TEE_ERROR_SECURITY; }
        }
        return TEE_SUCCESS;
    }
    if (types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) { return TEE_ERROR_BAD_PARAMETERS; }
    switch (command) {
    case TA_COMMAND_TRUSTED_READ: return trusted_read(params);
    case TA_COMMAND_TRUSTED_SELFTEST: return trusted_service_test(params);
    case TA_COMMAND_SECRET_CHECK:
        if (private_secret != TA_PRIVATE_VALUE) { return TEE_ERROR_SECURITY; }
        params[0].value.a = TA_SECRET_CHECK_TAG; return TEE_SUCCESS;
    case TA_COMMAND_SECRET_ADDRESS:
        /* 攻撃試験専用の情報公開。製品用TAへ組み込まない。 */
        if ((uintptr_t)&private_secret > UINT32_MAX) { return TEE_ERROR_NOT_SUPPORTED; }
        params[0].value.a = (uint32_t)(uintptr_t)&private_secret; return TEE_SUCCESS;
    case TA_COMMAND_PROBE_READ:
    case TA_COMMAND_PROBE_WRITE: {
        volatile uint32_t *target = (volatile uint32_t *)(uintptr_t)params[0].value.a;
        uint32_t access = command == TA_COMMAND_PROBE_READ ? READ_ACCESS : WRITE_ACCESS;
        /* heapの割当状態ではなくCoreの実mapping検査でVA aliasを除外する。 */
        if (_utee_check_access_rights(READ_ACCESS, (void *)(uintptr_t)target, sizeof(*target)) != TEE_ERROR_ACCESS_DENIED ||
            _utee_check_access_rights(access, (void *)(uintptr_t)target, sizeof(*target)) != TEE_ERROR_ACCESS_DENIED) { return TEE_ERROR_ACCESS_CONFLICT; }
        if (command == TA_COMMAND_PROBE_READ) { params[0].value.a = *target; }
        else { *target = PROBE_REPLACEMENT; }
        return TEE_ERROR_SECURITY;
    }
    case TA_COMMAND_KEY_CYCLE:
        security_destroy();
        if (private_key != TEE_HANDLE_NULL) { return TEE_ERROR_SECURITY; }
        { TEE_Result result = security_create(); if (result != TEE_SUCCESS) { return result; } }
        return key_policy_test();
    case TA_COMMAND_KEY_ATTRIBUTE:
        params[0].value.a = 0U; params[0].value.b = 0U;
        return key_attribute_test();
    case TA_COMMAND_KEY_POLICY: return key_policy_test();
    case TA_COMMAND_KEY_EXPORT:
        params[0].value.a = 0U;
        params[0].value.b = 0U;
        return TEE_ERROR_ACCESS_DENIED;
    default: return TEE_ERROR_BAD_PARAMETERS;
    }
}
