# 実機移植の確認票

全項目未確認。実機試験はNOT_RUN。記入した事実と証拠file/hashをboot_planへ反映する。

|確認項目|取得方法|未確定時の扱い|
|---|---|---|
|Vitis2025.1／board定義|実IDE version、内蔵ZCU106 hardware選択、XSA hash|Platform準備BLOCKED|
|processor／domain|SDT/BSPでA53#0 AArch64 Standalone、PMU processor確認|別CPUを推測選択しない|
|DDR/OCM/PMU RAM|生成map、address alias/translation、board revision|load禁止|
|FSBL/PMUFW|実ELF、map、stack/heap、boot後も残る予約、停止点|競合・handoff検査BLOCKED|
|BL31|ZynqMP実ELF、OCM全使用範囲、FSBL情報の受渡し|旧DDR案から勝手に置換しない|
|BL32|ZCU106 OP-TEE port、Core/TA RAM、stack/heap/dynamic、entropy源|QEMU Coreを流用しない|
|BL33|entry、NS EL1 handoff、linker、BSS/stack/heap/SHM|単体起動でboot chain成立としない|
|XMPU|全DDR port、region priority/default、master/mask/NS、lock、poison|保護成立BLOCKED|
|XPPU|対象周辺、aperture、20 profiles、parity、readback|UART/JTAG等を巻き込む変更禁止|
|DMA|ADMA/GDMA channel、排他、master/NS、DDR経路、cache、許可copy|転送エラーだけで保護PASSにしない|
|GIC|IIDR、group0/1、SGI、priority、SCR/DAIF、vectorとFIQ world routing|IRQ機能・性能NOT_RUN|
|debug／boot|board mode、JTAG認可、初期EL/security、resetとFSBL経路|不可逆設定変更なし|
|UART|board配線、instance、clock、baud、host device|固定値を推測しない|
|証跡|各image hash、起動順、console、ESR/FAR/XPU/DMA/GIC記録|RUN不明のPASSを作らない|
|性能|timer/CPU clock、cache/MMU、IRQ条件、warmup16/sample128、区間|QEMU値と実機値を別保存|

現ゲートにはDMA/GICとworld/例外接続の未完了も残る。証拠の欄を埋めただけではREADYにはならない。不可逆な設定は明示承認なしに変更しない。
