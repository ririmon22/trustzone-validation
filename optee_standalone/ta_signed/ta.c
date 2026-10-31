#include <tee_internal_api.h>
static uint32_t echo_count;
TEE_Result TA_CreateEntryPoint(void)
{
    echo_count = 0U;
    return TEE_SUCCESS;
}
void TA_DestroyEntryPoint(void)
{
}
TEE_Result TA_OpenSessionEntryPoint(uint32_t types, TEE_Param params[4], void **context)
{
    (void)params;
    (void)context;
    return types == 0U ? TEE_SUCCESS : TEE_ERROR_BAD_PARAMETERS;
}
void TA_CloseSessionEntryPoint(void *context)
{
    (void)context;
}
TEE_Result TA_InvokeCommandEntryPoint(void *context, uint32_t command, uint32_t types, TEE_Param params[4])
{
    (void)context;
    if (types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT, 0, 0, 0))
    {
        return TEE_ERROR_BAD_PARAMETERS;
    }
    if (command == 0U)
    {
        echo_count++;
        return TEE_SUCCESS;
    }
    if (command == 1U)
    {
        params[0].value.a = echo_count;
        return TEE_SUCCESS;
    }
    return TEE_ERROR_BAD_PARAMETERS;
}
