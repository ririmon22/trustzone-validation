ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); vectors PT_LOAD FLAGS(5); }
SECTIONS {
    . = 0x60000000;
    .text : { KEEP(*(.text.boot)) *(.text .text.*) *(.rodata .rodata.*) } :text
    . = ALIGN(0x1000);
    .data : { *(.data .data.*) } :data
    .bss (NOLOAD) : { __bss_start = .; *(.bss .bss.* COMMON) . = ALIGN(16); __bss_end = .; } :data
    ASSERT(. <= 0x60010000, "data overflow")
    . = 0x60010000;
    .vectors : { KEEP(*(.vectors)) } :vectors
    ASSERT(. <= 0x60020000, "vectors overflow")
    __stack_top = 0x60070000;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
