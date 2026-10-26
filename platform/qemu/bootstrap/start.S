#define MANIFEST_BASE 0x1000
#define MAX_SEGMENTS 32
#define CURRENT_EL3 12
#define ALL_EXCEPTION_MASKS 15
#define MANIFEST_HEADER_SIZE 16
#define RECORD_HALF_SIZE 16
.section .text.boot,"ax"
.global _start
_start:
    msr daifset, #ALL_EXCEPTION_MASKS
    mrs x0, CurrentEL
    cmp x0, #CURRENT_EL3
    b.ne halt
    mov x0, #MANIFEST_BASE
    /* segment数とEL3 entryをflash上のheaderから取得する。 */
    ldp x1, x2, [x0], #MANIFEST_HEADER_SIZE
    cmp x1, #MAX_SEGMENTS
    b.hi halt
    cbz x1, halt
segment:
    /* 検査済みPT_LOADをコピーし、残りのBSSをゼロ化する。 */
    ldp x3, x4, [x0], #RECORD_HALF_SIZE
    ldp x5, x6, [x0], #RECORD_HALF_SIZE
    cmp x5, x6
    b.hi halt
    sub x6, x6, x5
copy:
    cbz x5, zero
    ldrb w7, [x3], #1
    strb w7, [x4], #1
    sub x5, x5, #1
    b copy
zero:
    cbz x6, next
    strb wzr, [x4], #1
    sub x6, x6, #1
    b zero
next:
    subs x1, x1, #1
    b.ne segment
    dsb sy
    ic iallu
    dsb sy
    isb
    br x2
halt:
    wfe
    b halt
