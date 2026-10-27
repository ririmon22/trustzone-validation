# ZCU106 B2：配置・SHM・キャッシュ契約の整合化

今回解消したのはソースと生成物の不一致であり、実機予約・保護設定の確認ではない。B2専用の**暫定software配置案**を新設し、A/B1は変更せず保持した。全実機NOT_RUN、hardware load BLOCKED、GPIO/DMA保留。commit/pushなし。

## 前回の証跡の確認と保全

作業開始時の未コミット33ファイルをresults/b2-alignment/baselineへfile copy/hashとgit diff patchで保存した。docs/b2_integration.md、results/b2-integration-summary.json、そのreceiptが指す生成ELFとlogを確認した。前回のZCU106 ELFは、BL31 entry 0x1000（LOAD終端約0x1c000）、BL32 entry 0x60000000（BSSを含むLOAD終端0x600c5380）、BL33 entry 0x10200000（stack終端0x10500000）だった。旧A共通mapのtfa_reserved=0x60080000..0x60100000、optee_reserved=0x60100000..0x61000000と比較していたことが不一致の一因だった。

過去のsummary・run log・ELFは上書きしない。今回の結果は[別集計](../results/b2-alignment-summary.json)とresults/b2-alignment/へ保存する。

## 設定値の所在と変更前後

すべてEndは排他的。hardware_confirmed=false、load_authorized=falseのまま。

|項目|変更前|変更後と根拠|設定の所在|
|---|---|---|---|
|BL31予約|生成entry=0x1000とA共通予約が不一致|0x1000..0x80000。固定TF-Aのopteed既定と同じ範囲を明示したsoftware案|platform/zcu106/b2/memory_map.json → ZYNQMP_ATF_MEM_BASE/SIZE → TF-A platform_def.h|
|BL32/TZDRAM|Core=0x60000000、A共通予約start=0x60100000|0x60000000..0x70000000。upstream既定TZDRAMを保持し、B2予約とTF-A上限を合わせる|同JSON → ZYNQMP_BL32_MEM_BASE/SIZE、CFG_TZDRAM_START/SIZE|
|BL33 code/data/stack|0x10200000..0x10500000|維持。A/B1と同時loadしない|B2 JSON、親NS header、B2 linker_bl33.ld|
|Core SHM|0x70000000..0x80000000|0x10500000..0x10600000。既存client SHM設計に合わせた1MiB案|B2 JSON → CFG_SHMEM_START/SIZE。generated Core conf.mkで検査|
|BL33 SHM|0x10500000..0x10600000、uncached要求|同じ範囲、cached transportを選択|B2 memory_map.h、MakefileのZCU106 optee_bl33だけ -DTEE_SHARED_CACHED=1|
|Core DDR登録|upstream CFG_DDR_SIZE=4GiBで高位DDRも登録|報告DDR0長0x7ff00000だけを登録。未確認DDR1を使用しない|B2 JSON → CFG_DDR_SIZE。元main.cのregister_ddr分岐は改変なし|
|boot plan|A共通mapからの誤った予約参照|B2 JSONと同じallocations、software_memory_plan参照を明示|platform/zcu106/boot_plan.json、check_boot_plan.py|

build_external.pyがJSONを読み、TF-AとOP-TEEの公式Make変数を設定する。第三者原本は改変しない。SDK生成にも同じCore設定を渡す。B2 C header、JSON、実ELF、boot planの不一致は検査で拒否する。

BL33のJTAG fallback PCは引き続きPRELOADED_BL33_BASE=0x10200000を明示する。JTAG実boot modeなら固定TF-Aはbuild defaultsを使用し、それ以外はFSBL/XBL handoffを使う。Local launchやFSBL指定だけからどちらの分岐を実行したか確定しない。

## B2専用配置案

|用途|Base|End|含むもの|
|---|---:|---:|---|
|BL31|0x1000|0x80000|code/data/BSS、TF-A stack・runtime領域|
|BL32|0x60000000|0x70000000|Core code/data/BSS、Secure stack、heap、TA RAM/dynamic pool|
|BL33 code|0x10200000|0x10300000|RO実行可|
|BL33 data|0x10300000|0x10400000|BSS、24KiB page tables、分離したvector RX page|
|BL33 stack|0x10400000|0x10500000|1MiB NOLOAD・RW/XN|
|SHM|0x10500000|0x10600000|NS Normal WB/inner-shareable、1MiB|

BL32の実LOADは予約の一部だけだが、CFG_TZDRAM_SIZE全体をTA pool等のruntime予約として検査する。PT_LOADのp_memszを使いBSS/zero-fillを含める。BL33のstackも独立NOLOAD segmentで検査する。FSBL4件・PMUFW3件の**報告PhysAddr/MemSiz**と全予約を比較する。数値上の非重複は、processor/system address alias、DDR scratch、heap/stack、生存期間の確認を意味しない。OCM全域はB2 appに使用しない。

Aのprotected_buffer窓0x62000000..0x62100000はB2では使用せず、B2 Core予約に含まれる。A/B1/B2の全regionを同時配置してよいとはしない。A共通memory_map.jsonの旧tfa_reserved/optee_reservedは既存検査との互換用に残し、B2比較に使用しない。B2専用案を実機予約としてCONFIRMEDにしない。

## キャッシュ／MMUの不一致と修正

固定OP-TEEはMEM_AREA_NSEC_SHMをNormal cachedとして扱い、GET_SHM_CONFIGのa3でcachedを返す。一方、以前のZCU106 BL33はuncached a3=0を要求し、範囲を直すだけでは初期化が失敗する構造だった。QEMUのTEE_SHARED_CACHEDと専用MMUは既にこの契約を満たしていた。

新しいplatform/zcu106/b2/client_mmu.cは4KiB page tableで、以下の**software mapping**を準備する。

|領域|EL1属性|
|---|---|
|BL33 code|Normal WB、inner-shareable、privileged RO/RX、EL0 XN|
|data/stack/SHM|Normal WB、inner-shareable、privileged RW、PXN/UXN|
|例外vector|BSSと4KiB境界で分離した専用pageだけRO/RX|
|UART|既存sourceのUART0候補0xff000000..0xff001000だけDevice-nGnRnE、RW/XN|
|Secure TZDRAM、他DDR、DDR1、その他MMIO|このNS client tableではunmapped|

B2 linkerを分離した理由は、従来vectorsがdata/BSSと同じpageを共有する可能性があるため。data全体をXNにするだけでは例外処理が壊れるため、vectorを4KiBで分離してRXとし、隣のBSSはRW/XNに保つ。page tables自身はBSS内の4KiB aligned 6 tables（24KiB）で、実ELF symbol位置とLOAD範囲を監査する。

MMU設定はVA=PA、39bit VA、40bit PA、TTBR0のみ、MAIRのindex0=Device/index1=Normal WB、TLB/IC invalidateとDSB/ISBを用いる。Normal/Secure双方で同じSHM PAとcache属性を使い、SMC前後の既存DSBを維持する。**このNS tableでSecure memoryをunmappedにしたことを、XMPU/TrustZone保護PASSとは扱わない。** TA分離試験やAの物理拒否試験とは別。

## 引渡し条件とruntime gate

B2_RUNTIME_READY=0をheaderとELF内read-only初期値で検査する。platform_prepare_tee_clientはこの値が0ならtable install/sysreg操作前にfalseを返す。デバッガで値だけ変更して迂回しない。従来のhardware_profile/boot-plan/require-protectionゲートも保持する。

将来runtimeを許可するには、少なくとも次が必要。

1. 実boot modeとFSBL/PMUFW/TF-Aの引渡しを確定し、NS EL2 entry→既存EL1 trampoline、または直接NS EL1 entryの条件を確認する。
2. EL2 HCR.VM（stage2）、timer access、NS EL1 SCTLR.M/C/I、MAIR/TCR/TTBR、SP/VBAR/DAIFをsnapshotで照合する。
3. 本initializerは入口EL1、M/C/I=0を要求する。cacheが有効なら黙ってoffにせず停止する。過去のdirty D-cacheが残らないhandoff、instruction/data coherency、他core状態も別途確認する。
4. UART clock/MIO/実console、GIC/IRQ条件、XMPU master/security/lock/全関連bank、Core/TZDRAM/SHMの実保護を確認する。
5. 全予約を生成XSA/SDT/BSP/FSBL/PMUFWと照合し、Secure code/data/stack/TA poolを実際に保護する。software JSONのsecurityラベルは保護設定ではない。

MMU準備処理のホスト試験はdescriptor生成・table walkだけを行い、実CPUのMMUを有効化しない。実機でこの処理が正しく作動した結果はNOT_RUN。

## 再現コマンドと判定

完全checkoutをGitから事前取得する。開発subsetだけ--allow-snapshotsを明示する。以下はrepo rootのbashで実行する。Clang/LLD 21.1.8を使用し、Vitis/GNU buildとは別。

```bash
python3 scripts/offline_build.py --configuration optee --platform zcu106 --compiler clang --allow-snapshots
python3 scripts/offline_build.py --configuration all --platform qemu --compiler clang --allow-snapshots --qemu --host-tests
```

ZCU106はBUILDとsoftware_contract_statusを別に記録する。software契約がPASSでもintegration_status/hardware_load_statusはBLOCKEDのまま。QEMUは従来Core canary overlay込みの検証構成で再build/runする。

```bash
export TZ_B2_BOARD="$(python3 -c 'import json; print(json.load(open("results/phase8/latest-optee-zcu106.json"))["workspace"])')"
python3 scripts/check_b2_integration.py --platform zynqmp --root "$TZ_B2_BOARD" --output results/b2-alignment/readiness.json --require-integrated
```

最後のcommandは残る実機条件によりexit2になる。これをFAILの隠蔽やload許可に変換しない。今回の実結果・全command/log/ELF hashは[整合化結果](../results/b2-alignment-summary.json)。

## 参照根拠

- 固定TF-A dd3512…のplat/xilinx/zynqmp/include/platform_def.h：opteed時のBL31既定、BL32/BL33 build overrides。[公式2.12 §7.45.1/7.45.5](https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/xilinx-zynqmp.html)はbuild options/handoffの一般根拠。
- 固定OP-TEE 0919de…のplat-zynqmp/conf.mk/main.c、core/mm/core_mmu.cのMEM_AREA_NSEC_SHM、teecore_init_pub_ram、entry_fast.cのGET_SHM_CONFIG。第三者原本を変更せず公式変数を使用。[Core公式説明](https://optee.readthedocs.io/en/stable/architecture/core.html)。
- page/table/AP/PXN/UXN/MAIR属性は固定TF-A include/lib/xlat_tables/xlat_tables_defs.hの定義とも照合。index番号自体は本clientのMAIR設定に対応する独自選択。
- DDR0/OCM、FSBL/PMUFWの数値はユーザー報告のvitis_observations.json。元ELF/XSAや実機snapshotの代用にしない。

## 実行結果

|区分|今回の結果|実証範囲|
|---|---|---|
|ZCU106 B2 BUILD|PASS|BL31/Core/SDK/Early TA/BL33をsourceから生成、Clang使用|
|software静的契約|PASS|SHM/cache/DDR上限/entry/全LOAD・BSS/全予約/報告firmwareとの非重複|
|host回帰|146件PASS|descriptor生成・table walkを含む。実MMU動作は未検証|
|QEMU再build/static/run|A 3、B1 1、B2 9 PASS、FAIL 0|B2反復512回、性能8系列各128 sample。検証Core overlayを使用|
|ZCU106統合・load準備|BLOCKED|実機条件とB2 runtime ready=0を維持|
|ZCU106/Vitis GNU実行|NOT_RUN|実機load/JTAG/MMIO accessを実施せず|

旧QEMU 24 artifactと旧ZCU106 B2 8 artifactは全hashが不変。旧summaryとlogを保持し、今回の新runと区別した。作業は未コミット状態で、index・HEADを変更せず、commit/pushしない。

## 実機証跡の取得準備

次工程の[取得手順](b2_hardware_readiness.md)と[チェック票](b2_hardware_checklist.md)を追加した。前回のsoftware PASSをlive項目に転記せず、現在は全23項目NOT_RUNで、runtime ready/load gateを維持する。
