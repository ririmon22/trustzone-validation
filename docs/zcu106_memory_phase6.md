# ZCU106メモリ配置と実ELF（Phase 6）

全アドレスは半開区間。既存配置は暫定であり実機ロード不可。ELF inventoryはファイルとBSSを含むが、stack/heap/dynamicの全量を証明しない。

|領域|Base|End|Size|Security|Owner|用途|
|---|---|---|---|---|---|---|
|el3_firmware|0x60000000|0x60010000|0x10000|secure|el3|code|
|exception_vector|0x60010000|0x60020000|0x10000|secure|el3|vector|
|secure_code|0x60020000|0x60030000|0x10000|secure|secure|code|
|secure_data|0x60030000|0x60040000|0x10000|secure|secure|data|
|secure_stack|0x60040000|0x60050000|0x10000|secure|secure|stack|
|protected_buffer|0x62010000|0x62020000|0x10000|secure|secure|64 KiB inspection buffer inside separate DDR XMPU window|
|el3_stack|0x60060000|0x60070000|0x10000|secure|el3|stack|
|nonsecure_code|0x10200000|0x10300000|0x100000|nonsecure|nonsecure|code|
|nonsecure_data|0x10300000|0x10400000|0x100000|nonsecure|nonsecure|data|
|nonsecure_stack|0x10400000|0x10500000|0x100000|nonsecure|nonsecure|stack|
|shared_memory|0x10500000|0x10600000|0x100000|shared|client|shared|
|tfa_reserved|0x60080000|0x60100000|0x80000|secure|tfa|reserved|
|optee_reserved|0x60100000|0x61000000|0xf00000|secure|optee|reserved|

XMPU保護窓は0x62000000..0x62100000（1 MiB）。64 KiB buffer 0x62010000..0x62020000とは別定義。実DDR経路・master未確認。

|生成ELF|Entry|最小PT_LOAD|最大PT_LOAD終端|
|---|---|---|---|
|build/zcu106/el3.elf|0x60000000|0x60000000|0x60010800|
|build/zcu106/secure.elf|0x60020000|0x60020000|0x60031000|
|build/zcu106/nonsecure.elf|0x10200000|0x10200000|0x10301000|
|build/zcu106/tfa_bl33.elf|0x10200000|0x10200000|0x10301000|
|build/zcu106/optee_bl33.elf|0x10200000|0x10200000|0x10301000|
|build/external/tfa-zynqmp/zynqmp/release/bl31/bl31.elf|0xfffea000|0xfffea000|0x100000000|

FSBL、PMUFW、ZCU106 BL32は未取得/未生成でentryと予約範囲は未確定。既存tfa_reservedのDDR案と実BL31のOCM配置は不一致。AのELFとB1/B2のBL33は相互排他であり同時ロードしない。

`boot_plan.json` は構成別に複製し、実ELF path/hash/entry、system physicalへのaddress変換、stack/heap/dynamicのreserved/unusedと根拠を記録する。unusedも確認済みの根拠が必要。allocationsに既知BSSだけを記録してheapの存在を無視しない。

FSBL/PMUFWはboot後も保持される範囲を調べる。現ゲートは寿命違いを理由とする重複の自動許可をせず、保守的に競合とする。領域再利用を採用する場合はhandoff時点・継続参照・JTAG reset/再起動も別途検証する。
