# 06 構成B2のOP-TEE・SDK・Early TAビルド

最新の統合構成・再build結果・接続監査は [B2統合手順](../b2_integration.md) を参照する。ZCU106 boot planはB2用BL31を指すが、共有memory/cache/予約の不一致によりloadはBLOCKEDのまま。

## 目的

BL32/SDK/Early TAをsourceから生成し、opteed BL31とStandalone BL33を別成果物として準備する。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

固定TF-A/OP-TEE/pyelftools、GNU/libgcc、Python cryptography、optee_standalone/{ta,client,tests}/。Clang routeのみ固定compiler-rt。

## GUIのメニュー名

この章のcommandは端末で実行する。GUI操作なし。

## 操作手順・実行コマンド

1. 選択Pythonのcryptographyと固定sourceを検査し、全B2を新規生成する。

```bash
python3 scripts/check_build_environment.py --configuration optee --compiler gcc --cross-compile "$TZ_CROSS" --output "$TZ_LOG_DIR/preflight-B2.json" | tee "$TZ_LOG_DIR/preflight-B2.log"
bash scripts/build_vitis_offline.sh --configuration optee --platform zcu106 --compiler gcc --cross-compile "$TZ_CROSS" | tee "$TZ_LOG_DIR/build-B2.log"
export TZ_BUILD_ROOT="$(python3 -c 'import json; print(json.load(open("results/phase8/latest-optee-zcu106.json"))["workspace"])')"
python3 scripts/check_boot_plan.py --inventory "$TZ_BUILD_ROOT/build/external/tfa-zynqmp-b2/zynqmp/release/bl31/bl31.elf" "$TZ_BUILD_ROOT/build/external/optee-zynqmp/core/tee.elf" "$TZ_BUILD_ROOT/build/zcu106/optee_bl33.elf" --output "$TZ_LOG_DIR/b2-inventory.json"
```

2. helperの順序は公式`PLATFORM=zynqmp-zcu106 ta_dev_kit`→生成SDKをTA_DEV_KIT_DIRとしてTA-A/Bをbuild→stripped ELFをCFG_EARLY_TAのpathsへ指定→Core再build→TF-A SPD=opteed→NS BL33。SDK/BINを外から持ち込まない。`--configuration early-ta`ならSDKとTAだけの再構築も可能。
3. upstream既定TZDRAM=0x60000000/256MiB、SHMEM=0x70000000/256MiBは未変更原本のcompile defaults。現builderはB2専用planからTZDRAMを保持し、SHMEM=0x10500000/1MiB、CFG_DDR_SIZE=0x7ff00000を明示する。software整合は検査するが、実機予約・MMU/cache入口未確認で利用はBLOCKED。[修正内容](../b2_zcu106_alignment.md)を参照。
4. B2 BL31の過去entry=0x1000、BL32=0x60000000、BL33=0x10200000は生成ELF実績であり許可load addressではない。Aの自作EL3領域とB2のCore領域を同時loadしない。
5. client/client.cはSMC transport、OP-TEE message ABI、shared buffer境界、OPEN/INVOKE/CLOSE、target-dead後のCLOSEを扱う。TA-A/B UUIDとTA protocol headerを一致させる。物理連続性・cache属性・共有memory予約をCore effective configと照合する。
6. echo/session/TA-identity認可/SHM拒否/AES KAT/panic回復はLinux/libteec/tee-supplicantなしで試験できる実装範囲。Normal要求者認証、鍵memory抽出耐性、通常署名TAのruntime loadは別の未完了試験。REE_FS/RPMB/REE_FS_TA等は選択configでoff。storageやfilesystem TA loadを使うにはRPC/backend代替が必要で、無実装ならBLOCKED。
7. Early TAはCoreへ埋め込む。SDKが署名.taを生成しても通常TA runtime署名検証を実施したことにはならない。QEMUのCore canary検証overlayは実機通常buildに含めない。09章の起動条件が確定するまではrun禁止。

## 出力ファイル・実行ログの保存先

optee-zynqmp/core/tee.elfとtee-*_v2.bin、export-ta_arm64、ta-a/ta-b、tfa-zynqmp-b2 BL31、optee_bl33.elf/map。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

本章の期待出力を保存し、終了コードと内容を両方確認する。

## エラー時の対応

原因を保存して処理を止め、[トラブルシューティング](13_troubleshooting.md)で切り分ける。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲はPhase 8報告（別途生成・用意: `../phase8_progress.md`）と追加作業の確認記録（別途生成・用意: `../../results/manual-validation-summary.json`）を参照する。

## 固定ソースとの照合

[OP-TEE固定ZCU106 conf.mk](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/core/arch/arm/plat-zynqmp/conf.mk)。公式Web stableの説明は参考とし、4.5.0固定header（optee_msg.h/optee_smc.h）と実effective configを優先してABIを照合する。

## 公式資料の文書番号と章節

- [OP-TEE documentation stable（閲覧2026-10-27） — Core → Normal World invokes OP-TEE OS using SMC / Shared memory / Remote Procedure Call](https://optee.readthedocs.io/en/stable/architecture/core.html)。
- [TF-A 2.12.0 — §7.45（ZynqMP）、§7.45.1（build設定）、§7.45.5（FSBLからの引渡し）](https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/xilinx-zynqmp.html)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[前へ](05_tfa_build.md) · [目次](00_overview.md) · [次へ](07_jtag_baremetal.md)
