/* BL33の入口。TF-AからNormal WorldのEL2またはEL1で呼ばれる。
 * EL2ならAArch64のEL1へ例外復帰し、stack/BSS/vectorを用意してstandalone_mainへ渡す。
 * MMU/cacheが無効で、リンカ定義のRAMが利用可能であることは起動側の前提。
 * このコードはXMPUやSecure RAMの保護を設定しない。 */
#include "memory_map.h"
.section .text.boot,"ax"
.global _start
_start:
    mov x19, #0
    mov x20, #0
    mov x21, #0
    mov x22, #0
    /* stack/vector未準備の間は非同期例外をマスクする。未知のELでは停止する。 */
    msr daifset, #15
    mrs x0, CurrentEL
    cmp x0, #8
    b.eq from_el2
    cmp x0, #4
    b.eq start_el1
1:  wfe
    b 1b
from_el2:
    /* HCR.RW=1でEL1をAArch64にする。EL2の状態は後でRAMログへ渡す。 */
    mov x0, #(1 << 31)
    msr hcr_el2, x0
    mov x19, #1
    mrs x20, hcr_el2
    mrs x21, vttbr_el2
    mrs x22, vtcr_el2
    /* EL1から物理counter/timerを利用可能にし、仮想counterのoffsetをゼロにする。 */
    mov x0, #3
    msr cnthctl_el2, x0
    msr cntvoff_el2, xzr
    /* SCTLR_EL1のRES1を含む既存初期値。M/C/Iは0で、変換・cacheをまだ使わない。 */
    mov x0, #0x30d0
    lsl x0, x0, #16
    add x0, x0, #0x800
    msr sctlr_el1, x0
    ldr x0, =__stack_top
    msr sp_el1, x0
    adr x0, start_el1
    msr elr_el2, x0
    /* SPSRはDAIF全マスク・EL1h（SP_EL1使用）。eretはELRのstart_el1へ移る。 */
    mov x0, #0x3c5
    msr spsr_el2, x0
    isb
    eret
start_el1:
    ldr x0, =__stack_top
    mov sp, x0
    /* Cの静的ゼロ初期化を成立させてから、保存した引渡し情報とvectorを公開する。 */
    bl clear_bss
    adrp x0, debug_handoff
    add x0, x0, :lo12:debug_handoff
    stp x19, x20, [x0]
    stp x21, x22, [x0, #16]
    ldr x0, =exception_vectors
    msr vbar_el1, x0
    /* 以降の例外が新しいVBARを使うよう、命令実行側に反映してからCへ渡す。 */
    isb
    bl standalone_main
    b 1b
