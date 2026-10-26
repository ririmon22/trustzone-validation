ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); vectors PT_LOAD FLAGS(5); }
SECTIONS {
    . = 0xe020000;
    .text : { KEEP(*(.text.boot)) *(.text .text.*) *(.rodata .rodata.*) } :text
    . = 0xe030000;
    .data : { *(.data .data.*) } :data
    .bss (NOLOAD) : { __bss_start = .; *(.bss .bss.* COMMON) . = ALIGN(16); __bss_end = .; } :data
    ASSERT(. <= 0xe040000, "data overflow")
    . = ALIGN(2048);
    .vectors : { KEEP(*(.vectors)) } :vectors
    ASSERT(. <= 0xe040000, "vectors overflow")
    __stack_top = 0xe050000;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
