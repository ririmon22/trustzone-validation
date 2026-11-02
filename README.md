# TrustZone検証コード：個別ビルドの入口

最初に `cp -n local.mk.example local.mk` を実行し、**local.mk** へツール・Python・試験鍵のパスを設定する。
依存の取得・実際の版は [PC確認記録](docs/pc_validation.md)、詳しい設定は [build手順](docs/build.md)。通常buildにJSONは使わない。

| 順序・部品 | コマンド | 基本構成の出力 |
|---|---|---|
| 1 BL31（独立） | `make -j4 bl31` | `build/components/basic/tfa/zynqmp/release/bl31/bl31.elf` |
| 2 OP-TEE SDK | `make -j4 optee-sdk` | `build/components/basic/optee/export-ta_arm64/` |
| 3 TA-A | `make ta-a` | `build/components/basic/ta-a/6dc13418-534f-4642-9138-215876304910.elf` |
| 4 TA-B | `make ta-b` | `build/components/basic/ta-b/6dc13419-534f-4642-9138-215876304910.elf` |
| 5 BL32 | `make -j4 bl32` | `build/components/basic/optee/core/tee.elf` |
| 6 BL33（独立） | `make TOOLCHAIN=clang bl33` / `make TOOLCHAIN=gcc bl33` | `build/bl33/<compiler>/basic/optee_bl33.elf` |

BL32はSDK→TA-A/B→BL32の順。署名・物理は `PROFILE=signed` / `PROFILE=physical` で出力を分ける。
追加PTA・外部TA・bundleの必要な順は [署名／物理手順](docs/profiles.md)。新しい空出力は `BUILD_ROOT=build/new-run` で指定する。

読む順：`boot/bl33_start.S` → `tests/validation/main.c` → `tests/validation/functional.c` →
`normal_world/client/client.c` → `runtime/arch.c` → `secure_world/ta/ta.c`。
ECHOの要求・SMC・TA・復帰を追える。[フォルダ役割・移動表](docs/layout.md) · [日本語の読み方](docs/b2_reading_guide.md)。

PC確認済み：全3構成の外部GCCビルド、Clang/GCC BL33、2回の空出力からの再build、
生成物・署名・ホスト／異常系・Tcl mock検査。[結果と全コマンド](docs/pc_validation.md) · [今回の生成物SHA-256](docs/pc_artifacts.sha256)。

未実行：Vitis/実XSDB、対象XSA由来FSBL/PMUFW等の不足入力と、実機の起動・TA/ローダ・物理保護・性能試験。
**runtime-ready／physical-reviewed／実行許可は0**。[実機確認の入口と残条件](docs/zcu106_run.md) に従う。

[試験仕様](docs/b2_test_specification.md) · [21項目の範囲](docs/b2_scope_21.md) · [旧JSON一括手順（参照専用・実行不可）](legacy/README.md)。
