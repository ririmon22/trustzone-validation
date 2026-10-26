# メモリ配置

プラットフォーム別のJSONが検査入力、headerとlinker scriptsがビルド入力。

- [QEMU配置](../platform/qemu/memory_map.md)
- [ZCU106暫定配置](../platform/zcu106/memory_map.md)

領域のEndは排他的。重複・End・size・alignment・共有security・保護窓・ヘッダ・リンカ起点／stack・
ELF PT_LOADの実アドレスと属性・複数ELF間の重複を検査する。
ELFの予約領域への侵入も所有者に基づいて拒否する。
リンカ内の任意式を完全解析する仕組みではなく、実ELF検査を併用する。
Phase 3でDDR XMPU保護窓とbufferを別定義にし、外部firmwareの未知予約も準備停止条件にした。
外部TF-A/OP-TEE ELFの配置統合は未完了。XMPU/XPPU設定値の実readback APIは追加したが実機未実行。
現在のTF-A予約領域はAとの共存を仮定しない暫定枠で、B1/B2の実ELF配置を表すものではない。

JSONのmemory_typeは割当予定属性。MMUを無効化したコードは通常データのNormal-NC mappingを実装していない。
MMU無効時のデータ属性はアーキテクチャの既定に従い、Normal共有領域の属性成立を保証しない。
OP-TEE用にはpage tableとcache属性を確認した別構成が必要。

ZCU106はUG1085だけでは確定できない。生成BSPのlscript.ld、DDR容量、FSBL/PMUFWの作業領域、
BL31/BL32/BL33とTA領域、スタックと例外ベクタを照合した後でJSON/header/linkerを同時に更新する。
`confirmed=true`だけを変更しても実保護は成立しない。
`--require-protection`は未確認構成を止めるための検査で、保護動作の試験に代わるものではない。

## Phase 4のTF-A/OP-TEE用QEMU配置

完全ベアメタルの配置と別に `platform/qemu/external/{memory_map.h,memory_map.json,linker_bl33.ld}` を使用する。
TF-A 0x0e000000..0x0e100000、OP-TEE 0x0e100000..0x0f000000、BL33 code/data 0x60000000..0x60200000、stack 0x60200000..0x60300000、SHM 0x42000000..0x42200000。TF-A/OP-TEEの実ELF、header、FIPを `prepare_qemu_firmware.py` で照合する。これはQEMU配置検査でありZCU106保護検証ではない。

ZynqMP BL31の今回生成ELFはOCM entry0xfffea000、LOAD終端0x100000000。既存ZCU106 JSONの暫定DDR予約と一致しない。FSBL/PMUFWとの照合がないため暫定予約を確認済みにせず実機ロードを禁止している。
