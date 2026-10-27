ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); vectors PT_LOAD FLAGS(5); stack PT_LOAD FLAGS(6); }
SECTIONS {
    . = 0x10200000;
    .text : { KEEP(*(.text.boot)) *(.text .text.*) *(.rodata .rodata.*) } :text
    ASSERT(. <= 0x10300000, "text overflow")
    . = 0x10300000;
    .data : { *(.data .data.*) } :data
    .bss (NOLOAD) : { __bss_start = .; *(.bss .bss.* COMMON) . = ALIGN(16); __bss_end = .; } :data
    ASSERT(. <= 0x10400000, "data overflow")
    . = ALIGN(4096);
    .vectors : { __b2_vectors_start = .; KEEP(*(.vectors)) . = ALIGN(4096); __b2_vectors_end = .; } :vectors
    ASSERT(. <= 0x10400000, "vectors overflow")
    /* ヒープは提供しない。スタック全体をELFの実行時予約へ含める。 */
    __heap_start = __bss_end;
    __heap_end = __bss_end;
    . = 0x10400000;
    .stack (NOLOAD) : {
        __stack_bottom = .;
        . += 0x100000;
        __stack_reserved_end = .;
    } :stack
    __stack_top = 0x10500000;
    ASSERT(__stack_reserved_end == __stack_top, "stack reservation mismatch")
    ASSERT((__stack_top & 15) == 0, "stack alignment")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
