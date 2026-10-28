ENTRY(_start)
PHDRS {
    el3_text PT_LOAD FLAGS(5); el3_data PT_LOAD FLAGS(6); el3_vector PT_LOAD FLAGS(5); el3_stack PT_LOAD FLAGS(6);
    secure_text PT_LOAD FLAGS(5); secure_data PT_LOAD FLAGS(6); secure_vector PT_LOAD FLAGS(5); secure_stack PT_LOAD FLAGS(6);
    nonsecure_text PT_LOAD FLAGS(5); nonsecure_data PT_LOAD FLAGS(6); nonsecure_vector PT_LOAD FLAGS(5); nonsecure_stack PT_LOAD FLAGS(6);
}
SECTIONS {
    _start = el3__start;
    secure_entry = secure__start;
    nonsecure_entry = nonsecure__start;
    . = 0xe000000;
    .el3_text : { KEEP(*/el3.world.o(.text.boot)) */el3.world.o(.text .text.* .rodata .rodata.*) } :el3_text
    ASSERT(. <= 0xe010000, "el3 text overflow")
    . = ALIGN(4096);
    .el3_data : { */el3.world.o(.data .data.*) } :el3_data
    .el3_bss (NOLOAD) : { el3___bss_start = .; */el3.world.o(.bss .bss.* COMMON) . = ALIGN(16); el3___bss_end = .; } :el3_data
    ASSERT(. <= 0xe010000, "el3 data overflow")
    . = 0xe010000;
    .el3_vectors : { KEEP(*/el3.world.o(.vectors)) } :el3_vector
    ASSERT(. <= 0xe020000, "el3 vectors overflow")
    . = 0xe060000;
    .el3_stack (NOLOAD) : { el3___stack_bottom = .; . += 0x10000; el3___stack_top = .; } :el3_stack
    ASSERT(el3___stack_top == 0xe070000, "el3 stack end")
    ASSERT((el3___stack_top & 15) == 0, "el3 stack alignment")
    . = 0xe020000;
    .secure_text : { KEEP(*/secure.world.o(.text.boot)) */secure.world.o(.text .text.* .rodata .rodata.*) } :secure_text
    ASSERT(. <= 0xe030000, "secure text overflow")
    . = 0xe030000;
    .secure_data : { */secure.world.o(.data .data.*) } :secure_data
    .secure_bss (NOLOAD) : { secure___bss_start = .; */secure.world.o(.bss .bss.* COMMON) . = ALIGN(16); secure___bss_end = .; } :secure_data
    ASSERT(. <= 0xe040000, "secure data overflow")
    . = ALIGN(2048);
    .secure_vectors : { KEEP(*/secure.world.o(.vectors)) } :secure_vector
    ASSERT(. <= 0xe040000, "secure vectors overflow")
    . = 0xe040000;
    .secure_stack (NOLOAD) : { secure___stack_bottom = .; . += 0x10000; secure___stack_top = .; } :secure_stack
    ASSERT(secure___stack_top == 0xe050000, "secure stack end")
    ASSERT((secure___stack_top & 15) == 0, "secure stack alignment")
    . = 0x40200000;
    .nonsecure_text : { KEEP(*/nonsecure.world.o(.text.boot)) */nonsecure.world.o(.text .text.* .rodata .rodata.*) } :nonsecure_text
    ASSERT(. <= 0x40300000, "nonsecure text overflow")
    . = 0x40300000;
    .nonsecure_data : { */nonsecure.world.o(.data .data.*) } :nonsecure_data
    .nonsecure_bss (NOLOAD) : { nonsecure___bss_start = .; */nonsecure.world.o(.bss .bss.* COMMON) . = ALIGN(16); nonsecure___bss_end = .; } :nonsecure_data
    ASSERT(. <= 0x40400000, "nonsecure data overflow")
    . = ALIGN(2048);
    .nonsecure_vectors : { KEEP(*/nonsecure.world.o(.vectors)) } :nonsecure_vector
    ASSERT(. <= 0x40400000, "nonsecure vectors overflow")
    . = 0x40400000;
    .nonsecure_stack (NOLOAD) : { nonsecure___stack_bottom = .; . += 0x100000; nonsecure___stack_top = .; } :nonsecure_stack
    ASSERT(nonsecure___stack_top == 0x40500000, "nonsecure stack end")
    ASSERT((nonsecure___stack_top & 15) == 0, "nonsecure stack alignment")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
