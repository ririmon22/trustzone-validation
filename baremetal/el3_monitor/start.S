#include "memory_map.h"
#include "exception_capture.inc"
.section .text.boot,"ax"
.global _start
_start:
    msr daifset, #15
#if PLATFORM_REQUIRE_EL3_ENTRY_GUARD
    /* 未確認のELや有効な変換・キャッシュのままスタックとBSSへ触れない。 */
    .equ EL3_CURRENTEL_ENCODING, (3 << 2)
    .equ SCTLR_TRANSLATION_CACHE_MASK, ((1 << 0) | (1 << 2) | (1 << 12))
    mrs x0, CurrentEL
    cmp x0, #EL3_CURRENTEL_ENCODING
    b.ne entry_state_blocked
    mrs x1, sctlr_el3
    ldr x2, =SCTLR_TRANSLATION_CACHE_MASK
    tst x1, x2
    b.ne entry_state_blocked
    msr spsel, #1
    isb
#endif
    ldr x0, =__stack_top
    mov sp, x0
    bl clear_bss
    bl monitor_main
1:  wfe
    b 1b
#if PLATFORM_REQUIRE_EL3_ENTRY_GUARD
    /* UARTを使わず停止し、x0に入口EL、EL3時だけx1にSCTLRを残す。 */
.global entry_state_blocked
entry_state_blocked:
    wfe
    b entry_state_blocked
#endif
.global enter_world
enter_world:
    mov sp, x0
    b restore_frame
.section .vectors,"ax"
.balign 2048
.global exception_vectors
exception_vectors:
    FATAL_VECTOR 3, 0
    FATAL_VECTOR 3, 1
    FATAL_VECTOR 3, 2
    FATAL_VECTOR 3, 3
    FATAL_VECTOR 3, 4
    FATAL_VECTOR 3, 5
    FATAL_VECTOR 3, 6
    FATAL_VECTOR 3, 7
    b lower_sync
    .balign 128
    FATAL_VECTOR 3, 9
    FATAL_VECTOR 3, 10
    FATAL_VECTOR 3, 11
    FATAL_VECTOR 3, 12
    FATAL_VECTOR 3, 13
    FATAL_VECTOR 3, 14
    FATAL_VECTOR 3, 15
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
    CAPTURE_SYNC 3
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
/* 互換シンボル。個々のvectorは種別を保存する専用入口へ分岐する。 */
.global unexpected_vector
unexpected_vector:
    CAPTURE_FATAL 3, 255
