# 参照資料と照合範囲

|資料|対象版・確認した章節|用途と未確認事項|
|---|---|---|
|[Arm TrustZone for Armv8-A](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/TrustZone%20for%20Armv8-A.pdf)|Version 1.0、2020-01-08。3.1/3.2/3.4/3.7、4.1/4.3/4.4/4.6/4.7/4.8、5.1.1/5.3|基本特性の根拠。公開PDFの目次で章節を確認|
|[QEMU virt documentation](https://www.qemu.org/docs/master/system/arm/virt)|Web master、参照2026-10-26。章題 virt、machine options|secure/virtualization/GIC等。実行版の機能確認は未実施|
|[QEMU virt.c](https://github.com/qemu/qemu/blob/v9.2.0/hw/arm/virt.c)|v9.2.0、base_memmap|QEMU配置調査基準。実行時mapとの照合は未実施|
|[AMD UG1085](https://docs.amd.com/v/u/en-US/ug1085-zynq-ultrascale-trm)|検索でv2.5、2025-03-21を確認。XMPU/XPPU項目、正確な章節番号は未照合|Zynq実装根拠候補。全register offset、DMA master ID、初期状態の照合は未完了|
|[Vitis UG1400](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/Creating-a-Platform-Component-from-XSA?contentId=4FyupGrchK_3UYq4MyTGBQ)|2025.1、Creating a Platform Component from XSA|作成フローの公開項目確認。内蔵ZCU106表示名と全GUIは未確認|
|[Vitis UG1702](https://docs.amd.com/r/2025.1-English/ug1702-vitis-accelerated-reference/Creating-a-Bare-metal-System)|2025.1、Creating a Bare-metal System|New Component/Platformのmenu。対象例はZCU106ではない|
|[AMD release details](https://edf.docs.amd.com/en/v25.05.1/downloads-and-release-notes.html)|v25.05.1、Commits Information|AMD TF-A候補commitの由来だけに利用。ターゲットでLinux/EDFを使う構成ではない|
|[TF-A QEMU documentation](https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/qemu.html)|2.12.0、7.29、7.29.3|BL1/BL2/BL31/FIP、BL32/BL33の構築参考。自作BL33との実統合未確認|
|[OP-TEE Core documentation](https://optee.readthedocs.io/en/stable/architecture/core.html)|stable Web、Shared memory/SMC/RPC。4.5.0固定版との全照合未完了|ABI方針。実message headerとcache契約は取得後再照合|
|[OP-TEE 4.5.0 release](https://lists.trustedfirmware.org/archives/list/op-tee@lists.trustedfirmware.org/thread/7HLRUXHYUWFM5F2B42H6JKUY2IFTV4UN/)|2025-01-17、リリース告知commit link|upstream固定版の由来|

未確認の章節番号は記入していない。GlobalPlatform TEE System Architecture v1.3は根拠に使用していない。
参照文書が存在すること、ソースがcompileすること、規定動作を実行確認したことはそれぞれ別に扱う。

## Phase 8追加マニュアルでの更新

2026-10-27に[マニュアル目次](manual/00_overview.md)の資料と固定sourceを再照合した。UG1085 v2.5の第16章System Protection Unitsを確認したが、全register/実master/配線の確定とはしない。UG1400/XD260 2025.1、UG1137 Boot Process Overview/FSBL Build Process、UG1283 Using Bootgen on the Command Lineの章見出しを確認し、実GUI/実機NOT_RUNと区別した。各章に実URLを記載した。節番号のないWeb項目は見出しを用い、未確認の番号を推測していない。OP-TEE stable Webと4.5.0固定ABIは別の版として扱う。固定embeddedsw startupのbranch条件はmanual/02にcommit/blobを記録した。
