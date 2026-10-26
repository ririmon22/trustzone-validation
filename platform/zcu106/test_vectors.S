#define VECTOR_FRAME_BYTES 256
#define VECTOR_LR_OFFSET 240
#define VECTOR_TABLE_ALIGNMENT 2048
#define VECTOR_SLOT_BYTES 128
.section .zcu106_vectors,"ax"
.balign VECTOR_TABLE_ALIGNMENT
.global zcu106_test_vectors
zcu106_test_vectors:
    b zcu106_sync_entry
    .balign VECTOR_SLOT_BYTES
    b zcu106_interrupt_entry
    .balign VECTOR_SLOT_BYTES
    b zcu106_interrupt_entry
    .balign VECTOR_SLOT_BYTES
    b zcu106_unhandled
    .balign VECTOR_SLOT_BYTES
    b zcu106_sync_entry
    .balign VECTOR_SLOT_BYTES
    b zcu106_interrupt_entry
    .balign VECTOR_SLOT_BYTES
    b zcu106_interrupt_entry
    .balign VECTOR_SLOT_BYTES
    b zcu106_unhandled
    .balign VECTOR_SLOT_BYTES
    b zcu106_unhandled
    .balign VECTOR_SLOT_BYTES
    b zcu106_unhandled
    .balign VECTOR_SLOT_BYTES
    b zcu106_unhandled
    .balign VECTOR_SLOT_BYTES
    b zcu106_unhandled
    .balign VECTOR_SLOT_BYTES
    b zcu106_unhandled
    .balign VECTOR_SLOT_BYTES
    b zcu106_unhandled
    .balign VECTOR_SLOT_BYTES
    b zcu106_unhandled
    .balign VECTOR_SLOT_BYTES
    b zcu106_unhandled
    .balign VECTOR_SLOT_BYTES
.text
.macro save_registers
    sub sp, sp, #VECTOR_FRAME_BYTES
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
    str x30, [sp, #VECTOR_LR_OFFSET]
.endm
.macro restore_registers
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
    ldr x30, [sp, #VECTOR_LR_OFFSET]
    add sp, sp, #VECTOR_FRAME_BYTES
.endm
zcu106_sync_entry:
    save_registers
    mrs x0, esr_el1
    mrs x1, far_el1
    mrs x2, elr_el1
    bl zcu106_handle_probe_abort
    cbz x0, zcu106_unhandled
    msr elr_el1, x0
    restore_registers
    eret
zcu106_interrupt_entry:
    save_registers
    bl zcu106_irq_dispatch
    restore_registers
    eret
zcu106_unhandled:
    mrs x0, esr_el1
    mrs x1, far_el1
    mrs x2, elr_el1
    bl fatal_exception
1:  wfe
    b 1b
