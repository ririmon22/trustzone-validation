#include "memory_map.h"
.section .text.boot,"ax"
.global _start
_start:
    msr daifset, #15
    ldr x0, =__stack_top
    mov sp, x0
    bl clear_bss
    bl monitor_main
1:  wfe
    b 1b
.global enter_world
enter_world:
    mov sp, x0
    b restore_frame
.section .vectors,"ax"
.balign 2048
.global exception_vectors
exception_vectors:
.rept 8
    b unexpected_vector
    .balign 128
.endr
    b lower_sync
    .balign 128
.rept 7
    b unexpected_vector
    .balign 128
.endr
.section .text,"ax"
/* SPを16バイト境界に保ちながら全整数レジスタを保存する。 */
lower_sync:
    sub sp, sp, #272
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
    mrs x1, elr_el3
    mrs x2, spsr_el3
    mrs x3, sp_el1
    stp x1, x2, [sp, #248]
    str x3, [sp, #264]
    mov x0, sp
    bl monitor_dispatch
restore_frame:
    ldp x1, x2, [sp, #248]
    ldr x3, [sp, #264]
    msr elr_el3, x1
    msr spsr_el3, x2
    msr sp_el1, x3
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
    add sp, sp, #272
    eret
unexpected_vector:
    mrs x0, esr_el3
    mrs x1, far_el3
    mrs x2, elr_el3
    bl fatal_exception
