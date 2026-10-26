# TZ-SYS-03再監査

対象は完全ベアメタルA。TEEのEL0 Translation FaultによるTA分離試験とは別の検査である。

- 同じ対象範囲：0x0e050000..0x0e060000。先頭と末尾8-byteをSecureがseed/readし、NSが各read/writeを試す。
- 両EL1のSCTLR.M=0を実測し、TTBR0/TCRも記録する。page tableは使っていない。HCR_EL2.VM=0も実測し、Stage 2も使用しない。したがって対象load/storeのVAはPAと同値。
- EL3のSCR readbackでSecure/NSを確認する。Secureからの同一PAは成功し、NSからはESR 0x96000010/0x96000050、DFSC=0x10（同期External Abort）、FARは要求PAと一致する。Translation Faultを許可条件にしない。
- 例外handlerはarmしたread/write、命令PC、FAR、FnV、EC/DFSC、方向を確認して限定復帰する。無関係のfaultはFATAL。
- NS共有RAMの正常accessも検査する。初期値と各禁止write後にSecure側で保護値を比較し、3回のMEM_RECHECK intact=1を要求する。

QEMU10.2.1の実binaryで `info mtree` も取得した。cpu-secure-memory-0にはvirt.secure-ramが存在し、NSのsystem memoryにはない。したがってここでの拒否はsecurity stateが選ぶ物理address-space decodeに由来する。Arm page-table Translation Faultによる拒否とは説明しない。[QEMU固定版virt.c](https://github.com/qemu/qemu/blob/v10.2.1/hw/arm/virt.c) のcreate_secure_ram/secure-memory接続と、[公式virt仕様](https://www.qemu.org/docs/master/system/arm/virt.html)も照合した。配布binaryのhashはlockへ記録し、upstream sourceと配布patchが全byte同一という主張はしない。

証跡：`results/phase7/qemu-mtree.log` とcommand JSON、clean-workspace.jsonが指すrunのqemu/baremetal.logとtz-audit.json。PCの実値は各buildのlog/ELFに依存する。`check_tz_audit.py` はTranslation Faultへの改変、FAR違い、状態欠落、data変更を拒否することも確認した（audit-negative-checks.json）。

結論：このQEMU構成・対象buffer・MMU無効条件に限定してTZ-SYS-03 PASSを維持する。Secure RAM全域や全経路、ZCU106 XMPUの成立を証明しない。QEMUの物理security viewでNSが未decodeとなる仕組みであり、汎用RAMにprogrammable TZASC/XMPUを設定した試験でもない。実機は全NOT_RUN。
