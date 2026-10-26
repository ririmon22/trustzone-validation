# Vitis Unified IDE 2025.1操作手順

実IDEとZCU106は手元にないため操作はNOT_RUN。2025.1公式XD260を照合した手順で、内蔵ZCU106定義の同梱状態・実表示名はインストール先で確認する。Linux/PetaLinux domainは作成しない。

## PlatformとStandaloneアプリ

1. 2025.1を起動し新しいworkspaceを作る。版、インストールcomponents、ホストOSを記録する。
2. WelcomeのEmbedded DevelopmentからCreate Platform Component、またはFile → New Component → Platformを選ぶ。
3. hardware selectionで内蔵ZCU106定義が提示されれば選び、実際のXSA path/hashを保存する。表示されなければインストールのboard/hardware packageと選択可能なXSAを確認し、BLOCKEDとする。内蔵定義を使うだけのためにVivadoで新しい設計を作らない。カスタムhardwareが必要なときだけ別XSAを用意する。
4. SDTが列挙したprocessorで `psu_cortexa53_0`、OS `standalone`、AArch64を選ぶ。見当たらない場合は別CPUへ妥協せずhardware定義を再確認する。
5. boot自動生成を使う場合はGenerate boot artifactsを有効にする。PlatformのflowでBuildする。生成XSA/SDT、BSP、linker、FSBL/PMUFW、build logを保存する。この流れは [公式Create Platform 2025.1](https://docs.amd.com/r/2025.1-English/Vitis-Tutorials-Embedded-Software/Create-Platform) に基づく。
6. Application Componentは作成済みPlatformとStandalone domainを選ぶ。[公式Create Application](https://docs.amd.com/r/2025.1-English/Vitis-Tutorials-Embedded-Software/Create-Application) のtemplate手順で土台を作れる。AはEL3/Secure EL1/NS EL1を別component、B1/B2はStandalone BL33を作る。
7. 本repo Makefileの各targetに列挙されたC/.Sを取り込む。includeはcommon/include、platform/zcu106、B2 client。自作 `_start` とBSP標準startupを二重linkしない。初回は同じflagsでbuildできるUser Managedの方式も検討し、IDE設定と実commandを保存する。
8. `-mcpu=cortex-a53 -mgeneral-regs-only -mstrict-align`、freestanding、警告設定を実GNU compilerで確認する。現在の成功版はClang21.1.8でありGNU/Vitisのbuild成功を意味しない。
9. 生成linkerと暫定platform/zcu106/linker_*.ldを照合し、stack/heap/BSS/動的予約を含む実配置を確定する。暫定linkerをそのまま安全な配置としない。
10. ELF/map、全警告、compiler版、entryとPT_LOADを保存し、boot_planへ反映する。`check_boot_plan.py` と既存安全ゲートが通るまでload/runしない。

## FSBL／PMUFW

自動生成物があればまずそのmap・domain・build設定を確認する。FSBLを独立作成する場合、ExamplesのZynq MP FSBLから作成し、専用domainを生成する。通常Standalone domainにはFSBL用libraryが不足する。`xilffs`、`xilsecure`、`xilpm`を含むdomainとprocessorを確認する。これは [2025.1公式FSBL作成手順](https://docs.amd.com/r/2025.1-English/Vitis-Tutorials-Embedded-Software/Creating-and-Modifying-FSBL-in-Vitis-Unified-IDE) に基づき、掲載例のboardはZCU102なのでhardware自体はZCU106定義を使う。

PMUFWはPlatform生成物かPMU firmware templateを使用し、PMU processorをSDTで確認する（A53 appとして作らない）。固定embeddedsw版とVitis生成物の版を照合する。FSBL/PMUFWのELF・linker・map・stack/heap・OCM/PMU RAM・DDR scratch範囲と、継続使用する範囲を保存する。これらは本環境では未ビルド。

IDEの未確認な項目名やボード設定値を作り込まない。JTAGとUARTは [実機手順](zcu106_phase6_porting.md)、メモリは [配置表](zcu106_memory_phase6.md) を参照する。
