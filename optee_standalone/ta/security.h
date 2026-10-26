#ifndef SECURITY_H
#define SECURITY_H
#include <tee_internal_api.h>
TEE_Result security_create(void);
void security_destroy(void);
TEE_Result security_invoke(uint32_t command, uint32_t types, TEE_Param params[4]);
#endif
