# 05 構成B1のTF-AとBL33ビルド

## 目的

固定AMD TF-AのZynqMP BL31と自作NS EL1 BL33を生成する。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

third_party/tfa-amd固定commit、tfa_standalone/integration/、common/、platform/zcu106/。

## GUIのメニュー名

この章のcommandは端末で実行する。GUI操作なし。

## 操作手順・実行コマンド

1. 配置済みTF-Aの固定commit/clean treeを確認し、source-only helperを実行する。

```bash
python3 scripts/check_build_environment.py --configuration tfa --compiler gcc --cross-compile "$TZ_CROSS" --output "$TZ_LOG_DIR/preflight-B1.json" | tee "$TZ_LOG_DIR/preflight-B1.log"
bash scripts/build_vitis_offline.sh --configuration tfa --platform zcu106 --compiler gcc --cross-compile "$TZ_CROSS" | tee "$TZ_LOG_DIR/build-B1.log"
export TZ_BUILD_ROOT="$(python3 -c 'import json; print(json.load(open("results/phase8/latest-tfa-zcu106.json"))["workspace"])')"
python3 scripts/check_boot_plan.py --inventory "$TZ_BUILD_ROOT/build/external/tfa-zynqmp/zynqmp/release/bl31/bl31.elf" "$TZ_BUILD_ROOT/build/zcu106/tfa_bl33.elf" --output "$TZ_LOG_DIR/b1-inventory.json"
```

2. helperの公式Make commandは`PLAT=zynqmp RESET_TO_BL31=1 bl31`、BL33は`standalone-tfa`。QEMUのBL1/BL2/FIPやPL011 UARTを実機boot方式へ転用しない。実機BL31単体にnative fiptool/OpenSSL開発fileは不要。
3. 以前のClang実buildでB1 BL31 entryは0xfffea000だった。今回のELFを正とし、OCMのFSBL継続使用と競合しないことを確認する。BL33 entry/NS EL1 SPSRとFSBL handoff情報を一致させる。
4. TF-A公式7.45.5ではFSBLがimage handoff構造を作り、そのaddressをPMU_GLOBAL.GLOBAL_GEN_STORAGE6で伝える。JTAG直接起動でこの構造が存在すると仮定しない。固定AMD sourceのzynqmp_bl31_setup等とFSBL生成物を照合し、不足なら08章BLOCKED。
5. B1-SMCCC-01はSMCCC/PSCI version応答のsmoke試験。Secure EL1を独立実行した証拠や全TrustZone試験の代用にはしない。独自EL3 monitorやSCR再初期化をBL33へ足さない。

## 出力ファイル・実行ログの保存先

tfa_bl33.elf/map、BL31 ELF/BIN、external-tfa-zynqmp.log、b1-inventory.json。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

本章の期待出力を保存し、終了コードと内容を両方確認する。

## エラー時の対応

原因を保存して処理を止め、[トラブルシューティング](13_troubleshooting.md)で切り分ける。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲はPhase 8報告（別途生成・用意: `../phase8_progress.md`）と追加作業の確認記録（別途生成・用意: `../../results/manual-validation-summary.json`）を参照する。



## 公式資料の文書番号と章節

- [TF-A 2.12.0 — §7.45（ZynqMP）、§7.45.1（build設定）、§7.45.5（FSBLからの引渡し）](https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/xilinx-zynqmp.html)。
- [UG1137 2025.1 — 起動の概要 / FSBL build（同名節）](https://docs.amd.com/r/2025.1-English/ug1137-zynq-ultrascale-mpsoc-swdev/Boot-Process-Overview)。
- [UG1209 2025.1 — 起動と構成 → システムソフトウェア（同名節）](https://docs.amd.com/r/2025.1-English/ug1209-embedded-design-tutorial/Boot-and-Configuration)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[前へ](04_baremetal_build.md) · [目次](00_overview.md) · [次へ](06_optee_build.md)
