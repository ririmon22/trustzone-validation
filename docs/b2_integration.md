# TF-A＋OP-TEE統合検証（B2）

更新：共有memory/cacheとB2専用予約のソース不一致は [ZCU106 B2整合化](b2_zcu106_alignment.md) で修正した。以下の旧値の比較は初回統合監査時の記録として残す。現在のsoftware契約PASSと実機条件BLOCKEDは新しい結果（別途生成・用意: `../results/b2-alignment-summary.json`）を参照。

既存B2を統合環境として再利用する。新しい独自monitorやLinux clientは追加しない。TF-A/OP-TEE単体のBUILD結果だけで統合PASSとはせず、linked BL31/BL32/BL33、FIP、Early TAの実体、共有memory契約、実行証跡を別々に確認する。最終対象はZCU106。GPIO・DMAの実機機能/性能は保留、全実機NOT_RUN、load gate BLOCKEDを維持する。

## A／B1／B2の役割

|構成|EL3|Secure payload|Normal payload|用途|
|---|---|---|---|---|
|A|自作EL3 monitor|自作Secure EL1|自作NS EL1|TrustZone基礎、比較・回帰。TF-A/OP-TEEなし|
|B1|TF-A BL31、opteedなし|Secure EL1検証service未統合|Standalone BL33|SMCCC/PSCI version応答の比較。全TrustZone特性の代替にはしない|
|B2|TF-A BL31、SPD=opteed|OP-TEE BL32＋Early TA-A/B|Standalone TEE client/test BL33|本章の統合検証。TA session/SMC/shared-memory/security/stress/性能|

B2ではQEMUのBL1/BL2からBL31へ入り、opteedでBL32を初期化し、BL33を開始する。NS BL33はTF-AからEL2で入っても既存start.SでEL1へ降りる。NormalからSMC→EL3 opteed→OP-TEE→TA→Normalへ戻る経路を既存clientで検査する。ZynqMPではBootROM/FSBL/PMUFWとBL31の初期化・引渡しが別途必要。

## 接続のソースと確認方法

|項目|ソース・実体|確認|
|---|---|---|
|dispatcher|scripts/build_external.py、固定TF-A services/spd/opteed|B2 commandのSPD=opteedと、実BL31 ELF内opteed_setup/opteed_smc_handlerの定義を確認|
|Core|固定OP-TEE source、QEMU vexpress-qemu_armv8a／実機zynqmp-zcu106|effective conf.mkでCFG_WITH_ARM_TRUSTED_FW、CFG_EARLY_TA、CFG_CORE_RESERVED_SHMを確認|
|Early TA|optee_standalone/ta、SDKから生成したta-a/ta-b|Core内ts_bin_<UUID>のsymbol payloadを抽出・展開し、今回buildしたstripped TA ELFとbyte一致を検査|
|BL33|tfa_standalone/integration/start.S、optee_standalone/tests/{main,security_tests,stress_tests}.c|SMC transport、session、異常系、KAT、反復試験の実行主体|
|SMC ABI|optee_standalone/client/client.c、common/arch.c|EXCHANGE_CAPABILITIES、GET_SHM_CONFIG、CALL_WITH_ARG、message ABIとOPEN/INVOKE/CLOSE|
|共有memory|Core conf.mk、platformごとのmemory_map.h、BL33 compile command|範囲内包とcached transportの一致を静的確認。QEMUでは起動時の実応答とMMU/cache条件も検査|
|QEMU image|prepare_qemu_firmware.py|BL1/2/31、OP-TEE header/pager、BL33 ELF/BINの対応とFIP entry payloadのbyte一致を検査|
|ZCU106 boot plan|platform/zcu106/boot_plan.json|B2用BL31/Core/BL33のpathとopteed bindingを検査。load address/hash/権限の確定とは区別|

新しいscripts/check_b2_integration.pyをsource-only helperに組み込んだ。QEMU契約の不一致はbuild/test準備を失敗させる。ZCU106の未確定事項はartifact検査と分離してintegration_status=BLOCKEDとして出力し、ビルド成功でloadを許可しない。--require-integratedを付けるとBLOCKED時exit2になる。

## 今回の変更

1. boot_plan.jsonのBL31をB1用tfa-zynqmpからB2用tfa-zynqmp-b2へ修正した。BL32/BL33も統合builderの出力pathを指定した。hash/entry/translation_verified/runtime確認値はnull/falseのままにした。
2. check_boot_plan.pyにB2のartifact pathとSPD binding検査を追加した。B1 BL31/BL33へ戻した場合やopteed指定を失った場合は停止する。path名だけでlinked dispatcherの存在を証明するわけではなく、別途実ELFを検査する。
3. ZynqMP B2のJTAG用BL33既定entryをPRELOADED_BL33_BASEで自作NS codeの配置定義から導出するよう変更した。従来の固定source既定0x08000000と、実BL33 entry 0x10200000の不一致を解消するsoftware build設定。実機のboot mode/初期状態は未確認であり、load許可ではない。
4. linked opteed、Early TA byte identity、effective config、SHM/cache、QEMU FIPを継続検査する監査を追加した。既存A移植変更・安全ゲート・B1比較構成は保持した。第三者原本は改変せず、QEMU検証用Core canaryだけは既存の明示的build-copy overlayを使う。

## ソースからのビルドとQEMU試験

取得工程は先に完了させる。固定commitはthird_party/dependencies.lock.json。開発環境のsourceは固定Git blob検証済みsubsetであり、--allow-snapshotsを明示する。の完全Git checkoutではこのoptionを外す。build中の取得やAPT/pip installは行わない。Vitis/GNU routeは未検証。

```bash
bash
read -r -p "Repository root: " TZ_ROOT
cd "$TZ_ROOT"
set -o pipefail
mkdir -p results/b2-integration
python3 scripts/offline_build.py --configuration all --platform qemu --compiler clang --allow-snapshots --qemu --host-tests | tee results/b2-integration/qemu-build-console.log
```

TZ_ROOTには取得したrepo rootの絶対pathを入力する。allはA/B1/B2の比較回帰を含む。B2だけなら--configuration opteeを使う。毎回fresh workspaceと独立したresults/phase8/runs/<UTC>/を作り、過去ELF/BIN/SDKをinputにしない。QEMUはvirt-9.2,secure=on,virtualization=on,gic-version=2、Cortex-A53、1 CPU、TCGで実行する。

```bash
python3 scripts/offline_build.py --configuration optee --platform zcu106 --compiler clang --allow-snapshots | tee results/b2-integration/zcu106-build-console.log
```

こちらはZynqMP BL31＋ZCU106 Core/SDK/Early TA/BL33のbuild/staticだけで、JTAG/実機は使用しない。既存のGPIO/DMA host fixtureが回帰試験に含まれても実機検証ではない。

## 成果物・手動監査

|対象|BL31|BL32|BL33|
|---|---|---|---|
|QEMU B2|build/external/tfa-qemu-b2/qemu/release/bl31/bl31.elf|build/external/optee/core/tee.elf|build/qemu/external/optee_bl33.elf|
|ZCU106 B2|build/external/tfa-zynqmp-b2/zynqmp/release/bl31/bl31.elf|build/external/optee-zynqmp/core/tee.elf|build/zcu106/optee_bl33.elf|

全pathはfresh workspace内。生成SDKはbuild/external/optee[-zynqmp]/export-ta_arm64、TAはbuild/external/ta-{a,b}。latest-all-qemu.json / latest-optee-zcu106.jsonからworkspaceを得る。並列run時は共通latest.jsonではなく構成別receiptを使う。

```bash
export TZ_B2_QEMU="$(python3 -c 'import json; print(json.load(open("results/phase8/latest-all-qemu.json"))["workspace"])')"
(
    cd "$TZ_B2_QEMU"
    python3 scripts/check_b2_integration.py --platform qemu --output results/phase8/b2-integration-recheck.json --require-integrated
)
```

QEMU FIP再監査はそのsource workspaceのscriptで実行する。親repoから別workspaceのQEMU imageを生成しない。

```bash
export TZ_B2_BOARD="$(python3 -c 'import json; print(json.load(open("results/phase8/latest-optee-zcu106.json"))["workspace"])')"
python3 scripts/check_b2_integration.py --platform zynqmp --root "$TZ_B2_BOARD" --output results/b2-integration/zcu106-readiness.json --require-integrated
```

現状こちらはartifactがPASSでも統合契約がBLOCKEDのためexit2。従来check_hardware_profile.py、check_boot_plan.py、require-protectionゲートも維持する。

## 初回監査時のQEMUとZCU106の配置・契約（修正前）

|項目|QEMU integrated|ZCU106現在のsource build|
|---|---|---|
|BL31 Secure RAM / entry|0x0e000000..0x0e100000内、実ELFを検査|entry 0x1000。暫定tfa_reserved=0x60080000..0x60100000と不一致|
|OP-TEE Core|0x0e100000..0x0f000000内|entry 0x60000000。暫定optee_reserved=0x60100000..0x61000000と不一致|
|BL33/stack|0x60000000..0x60200000 / ..0x60300000|entry 0x10200000、stack 0x10400000..0x10500000|
|Core reserved SHM|0x42000000..0x42200000|0x70000000..0x80000000（upstream defaults）|
|client allowed SHM|Coreと同一|0x10500000..0x10600000。不一致|
|cache契約|BL33はTEE_SHARED_CACHED=1＋QEMU専用MMU準備|標準ZCU106 BL33はuncached transportを要求。不一致|
|DDR0|QEMU virt RAM定義|ユーザー報告のusable end=0x7ff00000。Core既定SHM末尾が超える|

QEMUの配置をZCU106へ転記しない。ZCU106用にB2独立の全予約/SHM/cache/boot設計を確定し、FSBL/PMUFW runtimeとXMPUを照合してから変更・再buildする必要がある。未確認のDDR1やlock/master値を補って通さない。Aの配置をB2と同時loadしない。

固定TF-A bl31_zynqmp_setup.cは実boot modeがJTAGならbuild-time defaults（BL32_BASE、PRELOADED_BL33_BASE、NS EL2 SPSR）を使用する。JTAG以外はFSBL/XBL handoff構造を使用する。BL33 start.SにはEL2→EL1 trampolineがあるため、NS EL2入口自体を不一致とはしない。Local launch/FSBL指定だけで実BOOT_MODE/JTAG branchを実行したと判断しない。debug権限、PSU/DDR、PMUFW/FSBL生存期間、EL3 entry/MMU/cache、GIC状態等の確認が必要。

## 実行した試験と未完了範囲

- B2のservice/session、TA identity認可、SHM範囲/サイズ不正、key export拒否/AES KAT、他TA read/write拒否、Core canary拒否、TA panic回復、512回のsession/command/panic/SHM/key cycleを既存コードで実施する。実結果は集計とUARTを参照する。
- TEE-02全体（信頼できるNormal要求者認証）、TEE-04通常署名TA runtime、TEE-05 memory key抽出耐性、TEE-06 storage/RPC、TEE-08 authenticated bootは未完了。Early TA build、TA identity認可、host署名検査、key API拒否をこれらの代用PASSにしない。
- Core canaryとCore statsの検証PTAはQEMUの明示的overlayだけ。通常ZCU106 Coreには適用せず、同等試験はBLOCKED。今回ZCU106実行は全NOT_RUN。
- Linux libteec/tee-supplicantは使用しない。clientは必要なSMC/message/shared-memory/session機能を実装し、未実装RPCが要求されたらtransportをpoisonして停止し、偽の成功を返さない。
- 性能はQEMU上の8系列各128 sampleを基盤確認に使う。ZCU106性能値として報告しない。GPIO/DMAは保留。

## 実ログとGit状態

最終のbuild/static/QEMU/host結果とID対応は 統合確認記録（別途生成・用意: `../results/b2-integration-summary.json`）。raw logはresults/b2-integration/とfresh run側に保持し、Gitにはコンパクトな集計だけを登録対象にする。作業開始時の未コミット変更22ファイルはresults/b2-integration/baselineへpatchとfile copy/hashで保存した。checkout/reset/clean、既存成果物削除、commit/pushは行わない。

## 参照資料

- [TF-A 2.12 §7.45/7.45.1/7.45.5](https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/xilinx-zynqmp.html)：ZynqMPの役割・build options・FSBL引渡し。JTAG branchの厳密な条件は固定AMD sourceのbl31_zynqmp_setup.cを優先。
- [OP-TEE Core：SMC、shared memory、RPC](https://optee.readthedocs.io/en/stable/architecture/core.html)：一般構造。ABIとcached NSEC_SHMの根拠は固定4.5.0のoptee_smc.h/optee_msg.h、entry_fast.c、core_mmu_type_to_attr。
- [固定OP-TEE ZCU106 config](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/core/arch/arm/plat-zynqmp/conf.mk)：upstream compile defaults。実機の設定済み値ではない。
