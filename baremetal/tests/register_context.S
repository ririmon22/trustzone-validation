#include "runtime_asm.h"
.text
.global test_register_context
test_register_context:
    sub sp, sp, #CALLEE_FRAME_BYTES
    stp x19, x20, [sp, #0]
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]
    mov x19, #REGISTER_SENTINEL_19
    mov x20, #REGISTER_SENTINEL_20
    mov x21, #REGISTER_SENTINEL_21
    mov x22, #REGISTER_SENTINEL_22
    mov x23, #REGISTER_SENTINEL_23
    mov x24, #REGISTER_SENTINEL_24
    mov x25, #REGISTER_SENTINEL_25
    mov x26, #REGISTER_SENTINEL_26
    mov x27, #REGISTER_SENTINEL_27
    mov x28, #REGISTER_SENTINEL_28
    mov x29, #REGISTER_SENTINEL_29
    ldr x0, =ASM_SMC_RUN_SERVICE
    mov x1, #ASM_SERVICE_GET_EL
    mov x2, xzr
    mov x3, xzr
    smc #0
    cmp x0, #ASM_EXPECTED_EL1
    b.ne register_failure
    cmp x19, #REGISTER_SENTINEL_19
    b.ne register_failure
    cmp x20, #REGISTER_SENTINEL_20
    b.ne register_failure
    cmp x21, #REGISTER_SENTINEL_21
    b.ne register_failure
    cmp x22, #REGISTER_SENTINEL_22
    b.ne register_failure
    cmp x23, #REGISTER_SENTINEL_23
    b.ne register_failure
    cmp x24, #REGISTER_SENTINEL_24
    b.ne register_failure
    cmp x25, #REGISTER_SENTINEL_25
    b.ne register_failure
    cmp x26, #REGISTER_SENTINEL_26
    b.ne register_failure
    cmp x27, #REGISTER_SENTINEL_27
    b.ne register_failure
    cmp x28, #REGISTER_SENTINEL_28
    b.ne register_failure
    cmp x29, #REGISTER_SENTINEL_29
    b.ne register_failure
    mov x0, #1
    b register_restore
register_failure:
    mov x0, #0
register_restore:
    ldp x19, x20, [sp, #0]
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #80]
    add sp, sp, #CALLEE_FRAME_BYTES
    ret
