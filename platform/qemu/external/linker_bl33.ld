ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); vectors PT_LOAD FLAGS(5); }
SECTIONS {
    . = 0x60000000;
    .text : { KEEP(*(.text.boot)) *(.text .text.*) *(.rodata .rodata.*) } :text
    ASSERT(. <= 0x60100000, "external BL33 text overflow")
    . = 0x60100000;
    .data : { *(.data .data.*) } :data
    .bss (NOLOAD) : { __bss_start = .; *(.bss .bss.* COMMON) . = ALIGN(16); __bss_end = .; } :data
    . = ALIGN(2048);
    .vectors : { KEEP(*(.vectors)) } :vectors
    ASSERT(. <= 0x60200000, "external BL33 data overflow")
    __stack_top = 0x60300000;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
