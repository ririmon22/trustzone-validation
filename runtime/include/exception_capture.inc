/* 致命的例外はSPとCを使わず、最初のレコードを保存して停止する。 */
.macro CAPTURE_FATAL level, slot
    msr tpidr_el0, x0
    msr tpidrro_el0, x1
    adrp x0, debug_exception
    add x0, x0, :lo12:debug_exception
    ldr x1, [x0]
    cbnz x1, .Lfrozen\@
    mov x1, #1
    str x1, [x0]
    stp x2, x3, [x0, #96]
    stp x4, x5, [x0, #112]
    stp x6, x7, [x0, #128]
    stp x8, x9, [x0, #144]
    stp x10, x11, [x0, #160]
    stp x12, x13, [x0, #176]
    stp x14, x15, [x0, #192]
    stp x16, x17, [x0, #208]
    stp x18, x19, [x0, #224]
    stp x20, x21, [x0, #240]
    stp x22, x23, [x0, #256]
    stp x24, x25, [x0, #272]
    stp x26, x27, [x0, #288]
    stp x28, x29, [x0, #304]
    str x30, [x0, #320]
    mrs x1, tpidr_el0
    str x1, [x0, #80]
    mrs x1, tpidrro_el0
    str x1, [x0, #88]
    mov x1, #\level
    str x1, [x0, #8]
    mov x1, #\slot
    str x1, [x0, #16]
    mrs x1, esr_el\level
    str x1, [x0, #24]
    mrs x1, far_el\level
    str x1, [x0, #32]
    mrs x1, elr_el\level
    str x1, [x0, #40]
    mrs x1, spsr_el\level
    str x1, [x0, #48]
    mov x1, sp
    str x1, [x0, #56]
    mrs x1, sp_el0
    str x1, [x0, #64]
.if \level == 3
    mrs x1, sp_el1
.else
    mov x1, sp
.endif
    str x1, [x0, #72]
    dsb sy
    mov x1, #2
    str x1, [x0]
    mrs x1, sctlr_el\level
    tbz x1, #2, .Lno_clean\@
    mov x2, x0
    add x3, x0, #328
.Lclean\@:
    dc cvac, x2
    add x2, x2, #64
    cmp x2, x3
    b.lo .Lclean\@
    dsb sy
.Lno_clean\@:
    adrp x0, debug_boot
    add x0, x0, :lo12:debug_boot
    mov x1, #90
    str x1, [x0, #8]
    mov x1, #3
    str x1, [x0, #16]
    adrp x0, debug_ram_log
    add x0, x0, :lo12:debug_ram_log
    mov w1, #3
    str w1, [x0, #20]
    dsb sy
.Lfrozen\@:
    wfe
    b .Lfrozen\@
.endm
.macro FATAL_VECTOR level, slot
    b .Lfatal\@
    .pushsection .text,"ax"
.Lfatal\@:
    CAPTURE_FATAL \level, \slot
    .popsection
    .balign 128
.endm
/* 保存済み整数frameから同期例外の最後の状態をC呼出し前に採取する。 */
.macro CAPTURE_SYNC level
    adrp x0, debug_sync_exception
    add x0, x0, :lo12:debug_sync_exception
    mov x1, #1
    str x1, [x0]
    mov x1, #\level
    str x1, [x0, #8]
.if \level == 3
    mov x1, #8
.else
    mov x1, #4
.endif
    str x1, [x0, #16]
    mrs x1, esr_el\level
    str x1, [x0, #24]
    mrs x1, far_el\level
    str x1, [x0, #32]
    mrs x1, elr_el\level
    str x1, [x0, #40]
    mrs x1, spsr_el\level
    str x1, [x0, #48]
.if \level == 3
    add x1, sp, #272
.else
    add x1, sp, #256
.endif
    str x1, [x0, #56]
    mrs x1, sp_el0
    str x1, [x0, #64]
.if \level == 3
    mrs x1, sp_el1
.else
    add x1, sp, #256
.endif
    str x1, [x0, #72]
    .set saved_index, 0
    .rept 31
        ldr x1, [sp, #(saved_index * 8)]
        str x1, [x0, #(80 + saved_index * 8)]
        .set saved_index, saved_index + 1
    .endr
    dsb sy
    mov x1, #2
    str x1, [x0]
.endm
