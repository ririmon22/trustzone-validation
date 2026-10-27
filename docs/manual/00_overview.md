# 00 全体手順と確認範囲

## 目的

のoffline Ubuntu 24.04＋Vitis Unified IDE 2025.1でsourceから再構築し、将来のZCU106 JTAG試験へ進むための入口。PetaLinux/Linux targetは使用しない。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

本repo、third_party/dependencies.lock.json、で取得済みの固定Git source。必要host toolは01章。

## GUIのメニュー名

この章のcommandは端末で実行する。GUI操作なし。

## 操作手順・実行コマンド

1. まず15章でGitからsourceを取得する。以下は取得済みrepo内の端末で実行する。環境依存値は`read`の質問に実pathを入力する。山括弧を残したcommandを実行する必要はない。

```bash
bash
export TZ_ROOT="$(pwd -P)"
test -f "$TZ_ROOT/Makefile" && test -f "$TZ_ROOT/third_party/dependencies.lock.json"
export TZ_LOG_DIR="$TZ_ROOT/results/company/$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$TZ_LOG_DIR"
set -o pipefail
cd "$TZ_ROOT"
git rev-parse HEAD > "$TZ_LOG_DIR/project-commit.txt"
```

2. 01章でtoolを検出、02・03章でPlatform/Applicationを準備する。04=A、05=B1、06=B2をそれぞれbuildする。
3. 14章を実行する。BLOCKEDなら実機loadを行わず、必要な生成物と確認事項を収集する。現在のゲートはDMA/GIC未統合を無条件停止理由にしており、単に確認flagをtrueにしても通らない。
4. 実機統合とreviewが済んだ将来のみ07/08/09章へ進む。UART=10、機能試験=11、性能=12、障害切分け=13。各章は実行前に読んで条件を確認する。

|構成|EL3所有者|Secure EL1|NS EL1|現在の実績|
|---|---|---|---|---|
|A|自作monitor|secure.elf|nonsecure.elf|BUILD/QEMU 3 PASS、実機NOT_RUN|
|B1|TF-A BL31|本構成のSecure EL1 service未統合|tfa_bl33.elf|BUILD/QEMU SMCCC 1 PASS、実機NOT_RUN|
|B2|TF-A BL31＋opteed|OP-TEE＋Early TA|optee_bl33.elf|BUILD/QEMU 9 PASS、実機NOT_RUN|

QEMU A/B1/B2はvirtの配置、実機はZynqMPの配置で別。QEMUのSecure RAM拒否は実機XMPUの証明ではない。現在のBL32と暫定header、BL31予約に不一致があり、自動ロード手順は完成済みとして扱わない。

## 出力ファイル・実行ログの保存先

environment.txt、Git/source commit記録、構成別build receipt、将来の実機UART/レジスタsnapshot。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

Phase 8の12自作ELF・host120件・QEMU A3/B1 1/B2 9 PASSは開発hostの過去実績。今回の追加作業ではhashとlogを再照合し、host試験と安全ゲートを再確認する。側のUbuntu/Vitis結果は別runに残す。

## エラー時の対応

原因を保存して処理を止め、[トラブルシューティング](13_troubleshooting.md)で切り分ける。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲は[Phase 8報告](../phase8_progress.md)と[追加作業の確認記録](../../results/manual-validation-summary.json)を参照する。

## マニュアル一覧

1. [00_overview.md](00_overview.md)
2. [01_environment_setup.md](01_environment_setup.md)
3. [02_vitis_platform_creation.md](02_vitis_platform_creation.md)
4. [03_vitis_application_creation.md](03_vitis_application_creation.md)
5. [04_baremetal_build.md](04_baremetal_build.md)
6. [05_tfa_build.md](05_tfa_build.md)
7. [06_optee_build.md](06_optee_build.md)
8. [07_jtag_baremetal.md](07_jtag_baremetal.md)
9. [08_jtag_tfa.md](08_jtag_tfa.md)
10. [09_jtag_optee.md](09_jtag_optee.md)
11. [10_uart_logging.md](10_uart_logging.md)
12. [11_test_execution.md](11_test_execution.md)
13. [12_performance_measurement.md](12_performance_measurement.md)
14. [13_troubleshooting.md](13_troubleshooting.md)
15. [14_hardware_safety_check.md](14_hardware_safety_check.md)
16. [15_offline_source_transfer.md](15_offline_source_transfer.md)

## 公式資料の文書番号と章節

- [UG1400 2025.1 — Using the Vitis Unified IDE → Launching the Vitis Unified IDE](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/Launching-the-Vitis-Unified-IDE)。
- [Arm TrustZone for Armv8-A Version 1.0 (2020-01-08) — 3.1/3.2/3.4/3.7、4.1/4.3/4.4/4.6/4.7/4.8、5.1.1/5.3（既存参照表で目次照合）](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/TrustZone%20for%20Armv8-A.pdf)。
- [UG1701 2025.1 — Developing Vitis Kernels and Applications](https://docs.amd.com/r/2025.1-English/ug1701-vitis-accelerated-embedded/Developing-Vitis-Kernels-and-Applications)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[目次](00_overview.md) · [次へ](01_environment_setup.md)

## 公式手順・実測・現地確認の区分

|区分|確認内容|状態|
|---|---|---|
|公式2025.1資料|UG1400のsettings64.sh読込みと`vitis -w`、Platform/BSP/User Managedの章見出し、XD260 Application作成の流れ|資料照合済み、実GUI NOT_RUN|
|公式boot仕様|UG1137のboot段階、TF-A §7.45/7.45.5のBL31とFSBL handoff、UG1283の別工程としてのBootgen|資料照合済み、実機NOT_RUN|
|固定source|embeddedsw boot.Sの入口ELとBSP設定の一致条件、OP-TEE ZCU106 portのcompile defaults|source照合済み、生成BSP/実機との一致は未確認|
|開発host|既存Phase8 artifact hash、UART解析/保護監査、host120試験|確認記録参照。今回QEMU新規起動は未実施|
|で必要|Vitis同梱toolとGNU build、内蔵ZCU106定義、生成XSA/BSP/FSBL/PMUFW、UART/DDR/OCMと予約|NOT_RUN|
|で必要|初期EL/security、XMPU/XPPU master/経路、DMA/GIC/vector統合、handoff、起動/復旧順|NOT_RUN、load BLOCKED|

[UG1137 2025.1 FSBL Build Process](https://docs.amd.com/r/2025.1-English/ug1137-zynq-ultrascale-mpsoc-swdev/FSBL-Build-Process)も参照する。UG1701はembedded/accelerated flowの位置付けの参考であり、EL3 startupやZCU106特有設定の直接根拠として使用しない。UG1209の他board/Linux例から配線やPetaLinux手順を取り込まない。
