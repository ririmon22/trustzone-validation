# Vitis 2025.1・オフラインsource build

Ubuntu 24.04、Vitis 2025.1、Vitis付属GNU toolchain、IDE実操作は本環境ではNOT_RUN。本手順は公式2025.1資料と固定sourceを照合した移植手順であり、同環境でのBUILD PASSを表さない。ZCU106実機もNOT_RUNで、既存load禁止gateは維持する。

## 事前確認とコマンドラインbuild

[source取得](source_only_transfer.md)をbuild前に完了する。APT/pipをbuild scriptから実行しない。[最小依存表](minimum_dependencies.md)で構成を選び、実際のtoolを調べる。Vitisインストール先のsettings scriptが存在する場合に限りその指示に従って環境を設定する。`command -v make python3`、`command -v aarch64-none-elf-gcc`等を確認し、見つからなければ実インストール先を調べる。候補prefixは検出の例で、同梱を保証しない。

以下の`/actual/tool/bin/aarch64-none-elf-`は検出した絶対prefixへ置換する。選択Pythonはsystem Pythonを基本とし、Vitis専用Pythonの外部moduleがsystem Pythonから見えるとは仮定しない。

```sh
python3 scripts/check_build_environment.py --configuration baremetal --compiler gcc --cross-compile /actual/tool/bin/aarch64-none-elf- --output results/phase8/company-baremetal-environment.json
PYTHON=python3 bash scripts/build_vitis_offline.sh --configuration baremetal --platform zcu106 --compiler gcc --cross-compile /actual/tool/bin/aarch64-none-elf-
PYTHON=python3 bash scripts/build_vitis_offline.sh --configuration tfa --platform zcu106 --compiler gcc --cross-compile /actual/tool/bin/aarch64-none-elf-
PYTHON=python3 bash scripts/build_vitis_offline.sh --configuration optee --platform zcu106 --compiler gcc --cross-compile /actual/tool/bin/aarch64-none-elf-
```

`early-ta`はSDK＋TA-A/Bだけ、`all`はA/B1/B2をまとめる。既定は実機向けzcu106。毎回新しい`build/offline-<UTC>/`へsourceを配置し、以前のELF/BIN/SDKはコピーしない。結果は`results/phase8/runs/<UTC>/`と`latest-<構成>-<platform>.json`へ出す。BLOCKEDなら不足tool/sourceを補った後に再実行する。`--allow-snapshots`は開発subsetの明示許可であり、会社の完全Git checkoutでは使用しない。

QEMUを事前に用意できた場合は`--platform qemu --qemu`を追加する。B1/B2のQEMU用FIPだけはnative C compiler/OpenSSL開発fileが追加で必要。`--host-tests`は任意でnative Clang fixtureを使用する。実機ELF生成の必須依存とは別にする。性能log解析は`python3 scripts/check_build_environment.py --configuration performance`でstdlib要件を確認する。

## Platform Component

[AMD XD260 Create Platform 2025.1](https://docs.amd.com/r/2025.1-English/Vitis-Tutorials-Embedded-Software/Create-Platform)を根拠とする。以下のZCU106選択肢の同梱・実表示名は現地確認事項。

1. Vitis Unified IDE 2025.1を起動してworkspaceを作り、`File > New Component > Platform`を選ぶ。
2. hardware選択に内蔵ZCU106定義がある場合はそれを選ぶ。無い場合はインストールcomponentsと提供XSAを確認しBLOCKEDとする。内蔵定義の利用だけのためにVivadoでXSAを新規設計しない。
3. processor `psu_cortexa53_0`、OS `standalone`、AArch64のdomainを選び、Platformをbuildする。別CPUやLinux domainで代用しない。
4. XSA/hash、SDT、BSP、生成linker、UART instance/stdin/stdout、clock、cache/MMU/startup設定を保存する。boot artifacts生成を使用した場合はFSBL/PMUFWのELF/mapも保存する。
5. [Phase 6 FSBL/PMUFW手順](vitis_2025_1_phase6.md)に従い、生成した実際の使用領域をboot planへ照合する。追加sourceが必要なら固定embeddedswを会社Gitから事前取得する。本helperはFSBL/PMUFWを生成しない。

## Application Componentと取り込むsource

[AMD XD260 Create Application 2025.1](https://docs.amd.com/r/2025.1-English/Vitis-Tutorials-Embedded-Software/Create-Application)に従ってApplicationを作り、上記PlatformとStandalone domainを選ぶ。raw freestanding payloadには本repo Makeを使うUser Managed方式を第一候補とし、IDEが生成する標準BSP startupと自作`_start`を同時linkしない。画面内の詳細設定名は実IDEで確認する。

|component/output|取り込むpayload|linkerと起動条件|
|---|---|---|
|A EL3 / el3.elf|baremetal/el3_monitor/start.S, monitor.c|linker_el3.ld。EL3 entryと初期Security Stateを実測する|
|A Secure EL1 / secure.elf|baremetal/secure_app/start.S, main.c、EL1共通|linker_secure.ld。自作monitorからhandoff|
|A Non-secure EL1 / nonsecure.elf|baremetal/nonsecure_app/start.S, main.c、baremetal/tests/register_context.S、EL1共通、runner、benchmark|linker_nonsecure.ld。自作monitorからhandoff|
|B1 BL33 / tfa_bl33.elf|tfa_standalone/integration/start.S, main.c、EL1共通、runner|linker_nonsecure.ld。TF-AからNS EL1へhandoff|
|B2 BL33 / optee_bl33.elf|tfa_standalone/integration/start.S、optee_standalone/tests/{main,security_tests,stress_tests}.c、client/client.c、EL1共通、runner、benchmark|linker_nonsecure.ld。TF-A＋OP-TEEの起動完了後にNS EL1へhandoff|

全componentは`common/memory.c`、`common/arch.c`、`common/result_report/console.c`、`platform/zcu106/uart.c`を使用する。EL1共通は`common/el1_vectors.S`、`common/el1_fault.c`、`common/probe.S`。runnerは`common/test_runner/runner.c`、benchmarkは`common/benchmark/benchmark.c`。追加driver libraryはMakeの`zcu106-drivers`にあるファイルを対象とする。具体的target依存はMakefileを唯一のbuild定義とし、最新の全headerも取り込む。QEMU専用client_mmu.cやexternal linkerは実機componentへ混ぜない。

Includeは`common/include`、`platform/zcu106`、`optee_standalone/client`。driver単体には`platform/zcu106/include`。C/ASM共通target flagsは`-mcpu=cortex-a53 -mgeneral-regs-only -mstrict-align`、Cは`-ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -O2 -g -Wall -Wextra -Werror -Wconversion -Wshadow -std=c11`。MakeがC/ASMをまとめてdriverへ渡す現行commandを保存する。linkは`-nostdlib -static -Wl,--build-id=none`、対応する`platform/zcu106/linker_*.ld`を`-Wl,-T,...`、mapを`-Wl,-Map,...`で指定する。entryは`_start`。GNU版の対応可否は現地buildで判定し、警告を黙って抑制しない。

## 外部firmwareと生成物

外部projectはIDEの通常Applicationへ全sourceを混ぜず、公式Makeをhelper経由で実行する。

|生成物|source buildと出力（fresh workspace以下）|
|---|---|
|B1 BL31|固定AMD TF-A、`PLAT=zynqmp RESET_TO_BL31=1`、`build/external/tfa-zynqmp/zynqmp/release/bl31/bl31.elf`|
|B2 BL31|同上＋`SPD=opteed`、`build/external/tfa-zynqmp-b2/zynqmp/release/bl31/bl31.elf`|
|ARM64 TA SDK|固定OP-TEE `PLATFORM=zynqmp-zcu106 ta_dev_kit`、`build/external/optee-zynqmp/export-ta_arm64/`|
|Early TA-A/B|上記SDKからbuild、`build/external/ta-{a,b}/`のstripped ELFをCoreへ指定|
|BL32|`PLATFORM=zynqmp-zcu106`＋Early TA paths、`build/external/optee-zynqmp/core/tee.elf`とbin parts|
|BL33|上表の自作ELF。通常TA runtime署名loadはこのEarly TA buildとは別のBLOCKED試験|

SDK・署名TA・Coreはその場でsourceから生成する。公式開発用TA鍵は検証専用で、製品真正性の根拠にしない。QEMUの検証用Core canary overlayは通常のZCU106 buildには適用しない。

## load/run前の必須照合

固定upstream ZCU106 OP-TEEのTZDRAM/SHMEM既定値を、会社の実予約範囲とみなさない。B1とB2ではTF-AのBL31既定配置も異なる。ELF PT_LOAD/entryに加えBSS、stack、heap、動的TA領域、共有領域、FSBL/PMUFW継続領域を照合する。

```sh
python3 scripts/check_memory_map.py --platform zcu106 --require-protection
python3 scripts/check_hardware_profile.py
python3 scripts/check_boot_plan.py --output results/phase8/company-boot-plan.json
```

未確定ならBLOCKEDのまま。helperのBUILD PASSはgateを解除しない。JTAG設定は[実機手順](zcu106_phase6_porting.md)の確認項目を使用し、USB/JTAG target、UART配線、boot mode、PSU/DDR初期化、FSBL/PMUFW、実際のEL/security stateを記録する。AのEL3直接load、FSBL経由、B1/B2のBL31起動を別launch設定とし、一般Standalone launchの自動初期化で代用しない。gate READYになるまでdownload/runしない。eFUSEや不可逆設定は変更しない。
