ENTRY(_start)
SECTIONS {
    . = 0;
    .text : { KEEP(*(.text.boot)) }
    ASSERT(. < 0x1000, "bootstrap exceeds manifest offset")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
