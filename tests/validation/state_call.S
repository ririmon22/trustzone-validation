/* AAPCS64のcallee-savedレジスタとSPを実際のCクライアント呼出しの前後で比較する。
   x0..x18/NZCVは保持条件に含めない。浮動小数点はこのbuildで使用しない。 */
.section .text
.global b2_state_echo
.type b2_state_echo, %function
b2_state_echo:
    sub sp, sp, #128
    stp x19, x20, [sp, #0]
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]
    str x2, [sp, #96]
    str x8, [sp, #104]
    mov x19, #19
    mov x20, #20
    mov x21, #21
    mov x22, #22
    mov x23, #23
    mov x24, #24
    mov x25, #25
    mov x26, #26
    mov x27, #27
    mov x28, #28
    mov x29, sp
    mov x2, x1
    mov x1, #0
    bl tee_invoke
    mov x9, #0
    cmp x19, #19
    cinc x9, x9, ne
    cmp x20, #20
    cinc x9, x9, ne
    cmp x21, #21
    cinc x9, x9, ne
    cmp x22, #22
    cinc x9, x9, ne
    cmp x23, #23
    cinc x9, x9, ne
    cmp x24, #24
    cinc x9, x9, ne
    cmp x25, #25
    cinc x9, x9, ne
    cmp x26, #26
    cinc x9, x9, ne
    cmp x27, #27
    cinc x9, x9, ne
    cmp x28, #28
    cinc x9, x9, ne
    mov x10, sp
    cmp x29, x10
    cinc x9, x9, ne
    ldr x10, [sp, #96]
    str x9, [x10]
    ldp x19, x20, [sp, #0]
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #80]
    add sp, sp, #128
    ret
.size b2_state_echo, .-b2_state_echo
