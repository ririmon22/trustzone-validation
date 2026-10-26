#include "memory_map.h"
.section .text.boot,"ax"
.global _start
_start:
    msr daifset, #15
    mrs x0, CurrentEL
    cmp x0, #8
    b.eq from_el2
    cmp x0, #4
    b.eq start_el1
1:  wfe
    b 1b
from_el2:
    mov x0, #(1 << 31)
    msr hcr_el2, x0
    mov x0, #3
    msr cnthctl_el2, x0
    msr cntvoff_el2, xzr
    mov x0, #0x30d0
    lsl x0, x0, #16
    add x0, x0, #0x800
    msr sctlr_el1, x0
    ldr x0, =__stack_top
    msr sp_el1, x0
    adr x0, start_el1
    msr elr_el2, x0
    mov x0, #0x3c5
    msr spsr_el2, x0
    isb
    eret
start_el1:
    ldr x0, =__stack_top
    mov sp, x0
    bl clear_bss
    ldr x0, =exception_vectors
    msr vbar_el1, x0
    isb
    bl standalone_main
    b 1b
