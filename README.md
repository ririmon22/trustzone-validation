# オフライン移送用ブランチ

移送・依存確認・再ビルドは[専用手順](docs/offline_transfer.md)を最初に読む。外部ソース・ツール・過去の結果は配布しない。実機ロードはBLOCKED。

# ZCU106 TrustZone / TEE validation

ZCU106（Zynq UltraScale+ MPSoC / Cortex-A53）のTrustZone分離、TEE機能、性能測定基盤を検証するプロジェクトです。ターゲットにLinux/PetaLinuxを使用しません。最終対象はUbuntu 24.04＋Vitis 2025.1 Unified IDE＋ZCU106。開発検証はUbuntu 26.04＋Clang/LLD 21.1.8＋QEMU 10.2.1で実施しました。

**ZCU106実機の検証結果は未取得です。全実機試験NOT_RUN、ロード禁止ゲートBLOCKEDを維持しています。** Ubuntu 24.04、Vitis GUI、Vitis付属GNU toolchainでのビルドもNOT_RUNです。

## での実機確認に向けた準備

[取得手順](docs/b2_hardware_readiness.md)、[23項目のチェックリスト](docs/b2_hardware_checklist.md)、[記入用JSON](docs/b2_hardware_checklist.json)を用意しました。原本・live snapshot・将来のhandoff停止点を分けて記録します。チェック票の検査はload許可を出さず、B2_RUNTIME_READY=0とBLOCKEDを維持します。ローカル再照合結果（別途生成・用意: `results/b2-hardware-prep-summary.json`）も参照してください。

## ZCU106 B2のsoftware整合化

[B2専用の暫定配置・SHM/cache修正](docs/b2_zcu106_alignment.md)を追加しました。Core/BL33 SHMを0x10500000..0x10600000へ統一し、TF-A/Coreの予約をB2専用planで設定します。BL33はcached通信と範囲限定MMU準備を持ちますが、B2_RUNTIME_READY=0と全実機loadゲートを維持します。値はソフトウェア配置案であり実機設定済みではありません。今回の結果と実log（別途生成・用意: `results/b2-alignment-summary.json`）を参照してください。

## TF-A＋OP-TEE統合構成（B2）の継続検証

既存B2を統合環境として再利用し、BL31のSPD=opteed、linked dispatcher、Core内Early TA-A/Bと今回生成したTAのbyte一致、BL33/SHM/cache/FIP接続を自動監査するようにしました。ZCU106 boot_planのBL31参照をB2用へ直し、BL32/BL33の出力pathを指定しています。QEMUでA 3／B1 1／B2 9 PASS、反復512回、host142試験を再確認しました。ZCU106はBUILD PASSでもSHM/cache/予約領域が不一致でintegration BLOCKED、実機NOT_RUNです。

[統合ビルド・起動・試験手順](docs/b2_integration.md)／今回の実績・証跡（別途生成・用意: `results/b2-integration-summary.json`）。A/B1は比較・回帰用に保持し、GPIO/DMAの実機検証は保留します。

## A環境の実機情報反映（既存変更を保持）

今回提示されたVitisのDDR0/OCM、FSBL/PMUFW LOAD、JTAG設定を報告値として取り込みました。[差分・配置案・開始条件](docs/zcu106_a_vitis_update.md)を参照してください。Aの入口アドレスは維持し、全stackをELF NOLOAD予約へ追加しました。ZCU106のEL3入口はEL3/MMU・cache条件不一致でSP/BSS初期化前に停止します。GPIO/DMA検証は保留。DDR1、XMPU実値/lock、FSBL handoff等は未確定で、**改修版の実機実行はNOT_RUN、loadはBLOCKED**です。Aの3 ELFとB1/B2 BL33のビルド、ホスト133試験が成功しました（Clang使用、実行試験ではありません）。再ビルドと静的検査の証跡はA移植確認記録（別途生成・用意: `results/zcu106-a-summary.json`）。過去QEMU結果は今回の改修版の実行結果ではありません。

## 検証構成と現在の状態

|構成|内容|開発hostでの確認|
|---|---|---|
|A|自作EL3＋Secure EL1＋Non-secure EL1|BUILD PASS、QEMU 3 PASS|
|B1|TF-A BL31＋Standalone NS EL1 BL33|BUILD PASS、QEMU SMCCC/PSCI 1 PASS|
|B2|TF-A BL31＋OP-TEE BL32＋Standalone BL33＋Early TA-A/B|BUILD PASS、QEMU 9 PASS、反復512回|

12自作ELF、host120試験、署名host検査5件、性能8系列各128サンプルを確認済みです。QEMUの性能値は実機性能ではありません。ZCU106向けBL31 B1/B2・BL32・SDK・Early TAもsource build済みですが、生成物の配置/共有memoryと暫定boot planが不一致でloadできません。

Phase8実績・実log対応（別途生成・用意: `docs/phase8_progress.md`）、コンパクトな集計（別途生成・用意: `results/phase8/summary.json`）、[特性とsource対応](docs/test_matrix.md)。BUILD PASS、QEMU PASS、HARDWARE PASSは別の結果です。BLOCKED/SKIPを含む詳細は報告を参照してください。

## 導入・ビルド・JTAGマニュアル

[00 全体手順・16冊の目次](docs/manual/00_overview.md)から開始してください。

- [01 環境](docs/manual/01_environment_setup.md)、[02 Platform](docs/manual/02_vitis_platform_creation.md)、[03 Application](docs/manual/03_vitis_application_creation.md)
- [04 Aビルド](docs/manual/04_baremetal_build.md)、[05 B1ビルド](docs/manual/05_tfa_build.md)、[06 B2ビルド](docs/manual/06_optee_build.md)
- [07 A JTAG](docs/manual/07_jtag_baremetal.md)、[08 B1 JTAG](docs/manual/08_jtag_tfa.md)、[09 B2 JTAG](docs/manual/09_jtag_optee.md)
- [10 UART](docs/manual/10_uart_logging.md)、[11 機能試験](docs/manual/11_test_execution.md)、[12 性能](docs/manual/12_performance_measurement.md)
- [13 障害切分け](docs/manual/13_troubleshooting.md)、[14 安全検査](docs/manual/14_hardware_safety_check.md)、[15 オフライン取得/Git管理](docs/manual/15_offline_source_transfer.md)

各章は入力/出力、番号付き操作、command、GUI項目、期待結果、エラー対応、証跡保存、公式資料の文書番号/節見出しと確認状況を記載しています。JTAGとBootgenは別工程です。未確定のdownload script/BIFは提供済みとして扱いません。

## 最小ビルド

source取得はbuild前にGitで完了します。完全baremetalはPython stdlib、GNU Make、bash/coreutils、実在するAArch64 toolchainで生成でき、追加Python package不要です。B2の公式暗号/署名処理はcryptographyを使用し、pyelftoolsは固定Git sourceから読み込みます。[構成別依存](docs/minimum_dependencies.md)と[全source一覧](docs/source_only_transfer.md)を参照してください。

以下は取得済みrepoのbash端末で実行します。実toolchain prefixを入力してください。Vitis付属pathは仮定しません。

```bash
read -r -p 'Actual AArch64 GNU prefix, including final -: ' TZ_CROSS
export TZ_CROSS
python3 -S scripts/check_build_environment.py --configuration baremetal --compiler gcc --cross-compile "$TZ_CROSS"
bash scripts/build_vitis_offline.sh --configuration baremetal --platform zcu106 --compiler gcc --cross-compile "$TZ_CROSS"
```

B1は`--configuration tfa`、B2は`optee`、SDK＋TAのみは`early-ta`、全体は`all`。helperはfresh workspaceを作り、旧ELF/BIN/SDK/containerに依存しません。build中の取得/インストールは行わず、ソース不足ならBLOCKEDにします。完全な固定Git checkoutが既定です。開発環境の検証済みsubsetだけは実験用`--allow-snapshots`を明示します。

## QEMUの再現

QEMUとClang等を実検出し、外部固定sourceを事前配置した環境で次を実行します。

```bash
bash scripts/build_vitis_offline.sh --configuration all --platform qemu --compiler clang --qemu --host-tests
```

開発subsetを使用する場合だけ`--allow-snapshots`を追加します。virt secure=on、Cortex-A53、1 CPUでA/B1/B2を実行し、結果は`results/phase8/runs/<UTC>/`へ保存します。QEMUのメモリmap・UART・保護機構はZCU106と別です。

## 実機実行前の条件と制限

FSBL/PMUFW実予約、XSA/BSP/PSU初期化、BL31/32/33配置とhandoff、XMPU/XPPU master/保護範囲、DMA/GIC/vector統合、debug/boot条件が未確定です。安全ゲートにはDMA/GIC未統合の停止条件が残ります。確認flagだけを変更してもloadを許可しません。自作EL3へBSP標準startupを混ぜず、BL33を通常アプリとして単独開始しないでください。

Normal要求者認証、鍵memory抽出耐性、通常署名TAのruntime load、storage/RPC、Secure Bootは未完了範囲です。TA-identity認可、key export拒否、検証用Core canaryの結果と区別します。eFUSE等の不可逆設定は自動変更しません。

## Git登録と証跡

source、手順、固定依存lock、コンパクトな結果/hashを登録対象とします。build生成物、外部source checkout、秘密情報、大量の実行logは除外し、既存logはローカルで保持します。過去の報告にあるlogリンクの一部はsource-only cloneでは再生成が必要です。追加作業確認記録（別途生成・用意: `results/manual-validation-summary.json`）に現在の再検査結果とGit登録状況を記録します。外部sourceはの許可Gitへ別途mirrorし、固定commitとlicenseを保持してください。
# trustzone-validation
# trustzone-validation
# trustzone-validation
