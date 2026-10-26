# ハードウェア設定確認表

CONFIRMEDは記載した仕様または生成物だけの確認。実機動作確認の意味ではない。ASSUMEDを実行許可に使わず、REQUIRES_HARDWAREは実機・生成BSP・live snapshotで確定する。

|分類|項目|確認範囲／仮定|根拠・次の確認|
|---|---|---|---|
|CONFIRMED|OCM address space|SoC仕様で0xfffc0000..0x100000000（半開）、256KiB|UG1085 v2.5 Chapter10 Table10-1/10-2|
|CONFIRMED|DDR address space|SoCのglobal address mapにDDR領域が定義される|UG1085 v2.5 Chapter10。boardの搭載量・初期化・実予約は別確認|
|REQUIRES_HARDWARE|実DDR有効範囲・OCM使用量|XSA/BSP/FSBL/PMUFWと実boot状態が必要|生成linker/map、stack/heap/dynamic、handoff後の寿命|
|CONFIRMED|XMPU仕様|DDR region単位・master/security条件を持つsystem protection|UG1085 Chapter16、既存backendの固定公式register照合|
|ASSUMED|XMPU配置案|1MiB窓0x62000000..0x62100000内に64KiB試験buffer|既存memory_map/protection_domains。実機で確定していない|
|REQUIRES_HARDWARE|XMPU実policy|全DDR port、master/mask、優先順位/default、NS、lock/poison|live registerと許可/禁止転送、Secure非改変確認|
|CONFIRMED|XPPU構造|apertureとmaster profilesを利用する保護機構|UG1085 Chapter16、UG1087と固定embeddedswの照合。詳細はzcu106_backends.md|
|REQUIRES_HARDWARE|XPPU適用先|対象周辺、20 profiles、parity、他の設定との競合|board配線とlive snapshot。UART/JTAG/bootを推測で保護しない|
|CONFIRMED|DMA controller区分|ADMAはLPD、GDMAはFPDのZDMA instances|UG1087 Module Summary、固定xzdma_hw.h|
|REQUIRES_HARDWARE|DMA実経路|channel、access master、NS属性、XMPU port、cache整合|XSAとlive register、通常copy成功を先に確認|
|CONFIRMED|GIC仕様|APU interrupt controllerはGIC400/GICv2|UG1087 Module Summaryと固定xscugic_hw.h|
|REQUIRES_HARDWARE|GIC運用|group、priority、SGI、SCR/DAIF、Secure FIQ経路|live snapshotとvector/context復帰。現在非同期world接続は未完了|
|CONFIRMED|FSBL/PMUFWの役割|FSBLはPSU初期化とhandoff、PMUFWはPMUで動作するfirmware|AMD2025.1 XD260/UG1209。必要libraryとprocessorを生成domainで確認|
|CONFIRMED|BL31生成物|固定AMD commitのPLAT=zynqmp ELF、entry0xfffea000と実PT_LOAD|clean build artifact。FSBLとのOCM共存成立は未確認|
|ASSUMED|BL31/BL32/BL33予約案|既存ZCU106 mapのDDR予約とNS配置は設計案|BL31実OCM配置とは不一致。確認前load禁止|
|REQUIRES_HARDWARE|BL32/BL33実配置とboot順序|ZCU106 OP-TEE port、SHM/MMU、FSBL→BL31 handoff、EL/security|実ELFとeffective CFG、boot descriptor、debug state|
|CONFIRMED|Security Stateの意味|CPU/transaction securityとOS/ユーザー認証は別機能|既存Arm TrustZone資料、TF-A/OP-TEE source根拠|
|REQUIRES_HARDWARE|実masterのSecure/NS属性|CPU以外のDMA/PL等にも経路固有の設定がある|UG1085 system path、XSA、live snapshot。ID値を推測しない|

公式資料：

- [UG1085 v2.5](https://docs.amd.com/api/khub/documents/xzMsp_c5sG9J6A3u7NkJYQ/content)：Chapter10のaddress mapとChapter16のsystem protection。ここでは仕様と実設定を分離する。
- [UG1087 Module Summary](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/Module-Summary)：controller instancesとGICv2区分。
- [AMD2025.1 Platform作成](https://docs.amd.com/r/2025.1-English/Vitis-Tutorials-Embedded-Software/Create-Platform)、[PMUFW](https://docs.amd.com/r/2025.1-English/ug1209-embedded-design-tutorial/Platform-Management-Unit-Firmware-PMUFW)：起動firmwareの準備。

未確認の節番号やboard固有IDを補っていない。phase6_hardware_checklist.md、boot_plan.json、board_profile.jsonの未確定項目は維持する。
