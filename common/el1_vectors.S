#include "exception_capture.inc"
.section .vectors,"ax"
.balign 2048
.global exception_vectors
exception_vectors:
    FATAL_VECTOR 1, 0
    FATAL_VECTOR 1, 1
    FATAL_VECTOR 1, 2
    FATAL_VECTOR 1, 3
    b el1_sync
    .balign 128
    FATAL_VECTOR 1, 5
    FATAL_VECTOR 1, 6
    FATAL_VECTOR 1, 7
    FATAL_VECTOR 1, 8
    FATAL_VECTOR 1, 9
    FATAL_VECTOR 1, 10
    FATAL_VECTOR 1, 11
    FATAL_VECTOR 1, 12
    FATAL_VECTOR 1, 13
    FATAL_VECTOR 1, 14
    FATAL_VECTOR 1, 15
.section .text,"ax"
el1_sync:
    sub sp, sp, #256
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]
    CAPTURE_SYNC 1
    mrs x0, esr_el1
    mrs x1, elr_el1
    bl handle_el1_sync
    cbz x0, el1_unhandled
    msr elr_el1, x0
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #240]
    add sp, sp, #256
    eret
el1_unhandled:
    /* 同期例外frameの元の整数registerを戻してから固定recordへ保存する。 */
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #240]
    add sp, sp, #256
    CAPTURE_FATAL 1, 4
el1_exception:
    CAPTURE_FATAL 1, 255
