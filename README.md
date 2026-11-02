# TrustZone検証コード：現行Makeの入口

最初に `cp -n local.mk.example local.mk` を実行し、**local.mk** に自分のツール・外部ソースのパスを設定する。
以下はリポジトリ直下で実行する。通常のELF生成にJSONは使わない。

| 順序・部品 | 個別コマンド | 生成物（基本構成） |
|---|---|---|
| 1 BL31（独立） | `make bl31` | `build/components/basic/tfa/zynqmp/release/bl31/bl31.elf` |
| 2 OP-TEE TA SDK | `make optee-sdk` | `build/components/basic/optee/export-ta_arm64/` |
| 3 TA-A | `make ta-a` | `build/components/basic/ta-a/6dc13418-534f-4642-9138-215876304910.elf` |
| 4 TA-B | `make ta-b` | `build/components/basic/ta-b/6dc13419-534f-4642-9138-215876304910.elf` |
| 5 BL32（TAを組込む） | `make bl32` | `build/components/basic/optee/core/tee.elf` |
| 6 BL33（独立） | `make bl33` | `build/zcu106/optee_bl33.elf` |

BL31・BL33は単独で作れる。BL32はSDK→TA-A/B→BL32の順。
署名・物理構成は `PROFILE=signed` / `PROFILE=physical`。
BL33は `build/zcu106/<PROFILE>/`、外部部品は `build/components/<PROFILE>/` に分離する。

読む順番：`boot/bl33_start.S` → `tests/validation/main.c` →
`tests/validation/functional.c` → `normal_world/client/client.c` → `runtime/arch.c` → `secure_world/ta/ta.c`。
ECHOの要求・SMC・TA処理・復帰をこの順で追う。

確認済み：Clang/LLD 21.1.8でBL33の基本・署名・物理の3構成、ホスト試験、ELF配置検査、回収計画生成。
未確認：GCC、外部ソースを使うBL31/SDK/TA/BL32、実機・XSDB。
`B2_RUNTIME_READY=0` と `B2_PHYS_REVIEWED=0` を維持し、ロード・実行はBLOCKED。

- [外部ソース・設定・全生成物パス](docs/build.md)
- [署名TA・物理試験・個別試験選択](docs/profiles.md)
- [現行の静的検査・JTAG準備・結果回収](docs/zcu106_run.md)
- [フォルダの役割・移動対応表・JSON分類](docs/layout.md)
- [初学者向けコードの読み方](docs/b2_reading_guide.md) · [試験仕様](docs/b2_test_specification.md) · [21項目の範囲](docs/b2_scope_21.md)
- [今回の検証記録](docs/layout_validation.md) · [前回の検証記録](docs/readable_build_validation.md)
- [旧JSON一括手順（参照専用・実行不可）](legacy/README.md)
