.section .text.boot,"ax"
.global _start
_start:
    msr daifset, #15
    ldr x0, =__stack_top
    mov sp, x0
    bl clear_bss
    ldr x0, =exception_vectors
    msr vbar_el1, x0
    isb
    bl nonsecure_main
1:  wfe
    b 1b
