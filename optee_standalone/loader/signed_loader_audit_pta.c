#include <kernel/pseudo_ta.h>
#include <tee_api_defines.h>
#include <string.h>
#include "../kernel/signed_loader_types.h"
#define AUDIT_UUID { 0x6dc1341d, 0x534f, 0x4642, {0x91,0x38,0x21,0x58,0x76,0x30,0x49,0x10} }
/* 検証結果だけを返す。Coreポインタ・鍵・任意メモリへのアクセスは公開しない。 */
static TEE_Result read_audit(void *context, uint32_t command, uint32_t types, TEE_Param params[4])
{
    (void)context;
    if (command!=0 || types!=TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_OUTPUT,0,0,0)) { return TEE_ERROR_BAD_PARAMETERS; }
    if (params[0].memref.size<sizeof(signed_loader_audit)) { params[0].memref.size=sizeof(signed_loader_audit); return TEE_ERROR_SHORT_BUFFER; }
    memcpy(params[0].memref.buffer,(const void *)&signed_loader_audit,sizeof(signed_loader_audit));
    params[0].memref.size=sizeof(signed_loader_audit);
    return TEE_SUCCESS;
}
pseudo_ta_register(.uuid=AUDIT_UUID,.name="signed-loader-audit.pta",.flags=PTA_DEFAULT_FLAGS,.invoke_command_entry_point=read_audit);
