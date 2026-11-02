# フォルダの役割と移動対応

[入口](../README.md)。現行のbuild入力は `Makefile`、`build_config/components.mk`、
Git管理外の `local.mk` と、下表のC/ASM/ヘッダ/リンカ・外部ソースである。
Makeから読むPythonは追加Coreの配置用 `scripts/prepare_core.py` とその署名パッチ依存だけ。
通常buildにlegacy/やJSONは入らない。

| 現在 | 役割 | 元の場所 |
|---|---|---|
| boot/bl33_start.S | TF-Aから渡されるBL33の入口。TF-A本体ではない | tfa_standalone/integration/start.S |
| runtime/ | BL33のCPU命令、例外、メモリ操作、計測、ログ、試験報告 | common/ |
| normal_world/client/ | Normal WorldのOP-TEEメッセージ・署名RPC | optee_standalone/client/ |
| secure_world/ta/、ta_signed/ | Secure EL0のTA-A/B・外部署名TA | optee_standalone/ta/、ta_signed/ |
| secure_world/pta/、loader/ | Core権限の追加PTAと署名ローダ監査 | optee_standalone/pta/、loader/ |
| tests/validation/ | BL33で動く全機能・異常系・性能・物理試験 | optee_standalone/tests/ |
| tests/host/ | PC上のSHM契約・ページテーブル比較 | 同左 |
| platform/zcu106/ | ボード依存の配置・リンカ・MMUと実機情報 | 同左 |
| build_config/、local.mk.example | 明示的な個別build設定と利用者パス例 | 同左 |
| scripts/ | 現行のCore配置・署名・検査・回収・decode | 同左（下記旧専用を除く） |
| legacy/scripts/ | field、build_zcu106、build_external、build_workspace、check_build_environment、source_snapshot、zip_sources、stage_optee_overlay、select_b2_test、check_b2_integration の参照用.py | scripts/の同名ファイル |
| scripts/elf_symbols.py | 上記integrationのELF symbol/payload/config読取り部分を移設。処理は維持 | scripts/check_b2_integration.pyの共有関数 |
| legacy/config/b2.example.json | 旧JSONビルド設定 | config/b2.example.json |
| legacy/docs/zcu106_build.md | 旧build資料（参照専用） | docs/zcu106_build.md |
| legacy/compiler-runtime.mk | 旧外部Clang用レシピ（参照専用） | Makefileのllvm-runtimeターゲット |
| docs/build.md、profiles.md | 現行の詳しいbuild設定・追加試験 | 旧READMEの対応節 |
| docs/ | 低レイヤの読み方、仕様、実機点検、検証記録 | 同左＋現行入口の資料分割 |

フォルダ単位の移動は配下の全ファイルが対象。ソース内容・処理順・メモリABIを変えず、
相対includeとMake・Python・資料からの参照を更新した。
`secure_world/loader/signed_loader_audit_pta.c` の `../kernel/` includeは
OP-TEEのcore/ptaへ配置された後の参照なので変更しない。

## 残すJSONと用途

| ファイル | 用途 |
|---|---|
| legacy/config/b2.example.json | 参照専用の旧build設定 |
| config/b2_hardware_capture.example.json、xmpu_test.example.json | 実機入力採取・限定XMPU準備。build入力ではない |
| platform/zcu106/b2/memory_map.json | 静的予約の提案。現行の配置検査でも使用 |
| platform/zcu106/{board_profile,boot_plan,jtag_b2_inputs,mmio_catalog,vitis_observations}.json | 実機証跡・起動提案・未確認状態。build入力ではない |
| third_party/dependencies.lock.json | 外部ソース来歴資料 |
| results/、build/以下のmanifest等 | 検査・生成結果。Git管理外 |

TF-A/OP-TEEのMake設定、`platform/zcu106/b2/memory_map.h`、親のmemory_map.h、
`linker_bl33.ld` の配置は対応している。BL31=0x1000..0x80000、
BL32=0x60000000..0x70000000、BL33 code/data/stack=0x10200000/0x10300000/0x10400000、
SHM=0x10500000..0x10600000。変更時は全箇所を同時に照合し、
readelfのPT_LOAD/section/map、Core conf.mk、TF-Aの実build引数を確認する。
リンカASSERTは領域超過を検査する。JSONの提案を実機保護確認済みに読み替えない。
