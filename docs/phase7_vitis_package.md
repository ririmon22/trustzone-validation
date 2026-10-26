# Vitis2025.1移植package

生成：`python3 scripts/package_vitis_phase7.py`。成功したclean-workspace.jsonと同一sourceを照合してから、source、設定、既存ZCU106暫定ELF、実ZynqMP BL31、SDK生成Early TA成果物をpackする。QEMU専用BL32とMMU/bootstrapは実機用として混ぜない。

成果物：results/phase7/vitis-source-package.tar.gz、vitis-package.json。archive内component_manifest.jsonがPlatform ComponentとApplication Component単位の正確な入力file一覧。

|構成|Application Component|外部build|起動とlinker|
|---|---|---|---|
|A|A-el3、A-secure、A-nonsecure|なし|各custom _start、対応linker_el3/secure/nonsecure.ld|
|B1|B1-bl33|AMD TF-A PLAT=zynqmp BL31|FSBL/PMUFW→BL31→NS EL1。BL33はnonsecure linker|
|B2|B2-bl33|TF-A opteed、実機OP-TEE port、SDKとEarly TA-A/B|BL31/BL32/BL33 handoffと実SHM属性照合が必要|

manifestはsource、include path、compiler flags、linker flags/script、startupを別fieldへ分離する。EL3/Secure/NSのC/.S、共通runner、例外vectors、memory map、client、TA/PTA、保護driverを含める。BSP標準startupと自作_startを二重linkしない。main.cだけを取り込んで必要moduleを省略しない。

Platform作成・domain・FSBL/PMUFWはdocs/vitis_2025_1_phase6.md、JTAG/UARTはzcu106_phase6_porting.md。hardwareは内蔵ZCU106定義があることを実IDEで確認し、生成XSA/hashを記録する。Vitis操作自体はNOT_RUN。

B2の実機MMU/cache/shared-memory adapterは未確定。QEMU用TEE_SHARED_CACHED定義をそのままZCU106へ設定しない。Core canary/資源統計PTA、pointer公開、故障注入は明示的な検証overlayであり製品Core用ではない。実機BL32は未ビルドでpackageに架空の成果物を含めない。Early TAのbuildは実機での署名TA runtimeロード検証の代替ではない。

実機用memory map、stack/heap/dynamic、BL31 OCM、FSBL/PMUFW予約、XMPU/XPPU/master、DMA/GIC、boot順序が確定するまでload禁止。packageのcompile成功をHARDWARE PASSにしない。Makeを使う場合はPLATFORM=zcu106を指定し、このsource packageを完全なQEMU再現workspaceと混同しない。
