# ZCU106 B2 確認チェックリスト

全項目はの実測/原本確認が未実施のためNOT_RUN。静的PASSを転記しない。記入用JSONは[b2_hardware_checklist.json](b2_hardware_checklist.json)をコピーし、[取得手順](b2_hardware_readiness.md)に従う。各項目に実測値、file/hash、停止点/CPU/EL/Security State、reviewerとUTC日時を記録する。PASS/BLOCKED/NOT_RUN（不一致はFAIL）を使用する。PASSの証跡はsource/測定の意味を人がreviewする必要があり、hash一致だけで内容の正当性は証明しない。

実機のload/runが必要な停止点は、現在の禁止条件の下では取得できない。NOT_RUNで残し、取得したふりをしない。GPIO/DMAは対象外として保留。runtime_ready=0、load_authorized=falseを変更しない。

## B2-HW-01 構成・出所

- 何を：Vitis/board/XSA/BSP/sourceの同一性
- どこで：Vitis About、Platform hardware/domain、board revision
- どう確認：版表示・生成物・source dirty差分hash・board revisionを保存
- 期待条件：2025.1、psu_cortexa53_0/AArch64/standalone、XSA/SDT/BSP/hashと実boardを対応づける

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-02 FSBL ELF

- 何を：FSBL entry/全LOAD/p_memsz
- どこで：Vitis FSBL componentのBuild/ELF/map
- どう確認：readelf -h -l -S -s。FSBL processor/entry/LOAD rawを保存
- 期待条件：元FSBL ELFのhash、entry、program headersを取得し報告4 LOADと照合。不一致なら理由を確認

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-03 FSBL runtime

- 何を：stack/heap/BSS/DDR scratch・生存期間
- どこで：FSBL生成linker、map、source、handoff code
- どう確認：stack/heap symbol、linker MEMORYとscratch allocation sourceを追跡。空き穴を推測しない
- 期待条件：全runtime予約とscratch/bitstream buffer、引渡し後に解放可かを根拠つきで確定

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-04 PMUFW ELF

- 何を：PMUFW entry/全LOAD/architecture
- どこで：Vitis PMUFW componentのELF/map
- どう確認：readelf/既存ELF inventoryで32/64bitとprocessor viewを保存。A53 ELFと同一扱いしない
- 期待条件：元PMUFW ELF/hash/processor/endian/LOADを取得し報告3 LOADと照合

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-05 PMUFW runtime

- 何を：PMUFW stack/heap/常駐・並行アクセス
- どこで：生成linker/map、PMUFW sourceと起動設定
- どう確認：生成mapとxpfw_xpu等の設定呼出し元を追跡。init戻り値のみで判断しない
- 期待条件：常駐領域、stack/heap/予約、XMPU/ISRを変更する処理とlifetimeを列挙

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-06 DDR effective map

- 何を：DDR0/DDR1/OCMの実効範囲
- どこで：XSA/SDT/linkerとPS DDR初期化証跡
- どう確認：生成memory mapとUG1085第10章を照合。実DDR初期化結果は別証跡で確認
- 期待条件：DDR0 0..0x7ff00000という報告と実aperture/除外領域を照合。DDR1は不明なら除外継続

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-07 システム物理address

- 何を：各processor view/VA/PA/alias
- どこで：FSBL/PMUFW map、APU/DAP address space、TRM
- どう確認：role別のprocessor range→system PA対応表と出所を保存。debugger view選択も記録
- 期待条件：各LOAD/runtime予約をsystem physicalへ根拠付き変換。非線形aliasを一律biasで処理しない

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-08 B2予約

- 何を：BL31/BL32/BL33/SHMの排他使用
- どこで：B2 JSON、全firmware map、実予約review
- どう確認：p_memszと全stack/heap/TA poolを含め区間表を照合。A/B1同時配置なし
- 期待条件：BL31 0x1000..0x80000、BL32 0x60000000..0x70000000、BL33 0x10200000..0x10500000、SHM 0x10500000..0x10600000を他使用から排除

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-09 debug/boot mode

- 何を：JTAG認可と実BOOT_MODE
- どこで：board boot switch、Vitis target connection、PS register view
- どう確認：board状態、読出し可能なBOOT_MODE等と接続設定を保存。不可逆設定は変更しない
- 期待条件：JTAG予定/Local表示だけでJTAG boot branchとしない。認可と実boot modeを確認

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-10 初期化と停止点

- 何を：PSU/DDR/PMUFW/FSBLの順序
- どこで：launch.json、生成initialization Tcl、FSBL完了点
- どう確認：script/launchをreview。固定sleepを完了判定にしない。現段階でreset/runしない
- 期待条件：Reset Entire System ON/APU-RPU OFFの作用、PMUFW有無、DDR準備完了とA53停止点を確定

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-11 BL31 entry

- 何を：EL3/Security/PC/SP/EL3 MMU/cache
- どこで：将来承認済みBL31 entry停止点のA53 Register Inspector
- どう確認：停止点名・PC・CurrentEL・SCR_EL3・SCTLR等を保存。A用M/C/I契約を自動転用しない
- 期待条件：AArch64 EL3 Secure、PCとargs/SP、SCTLR_EL3/TTBR/TCR/MAIR/VBAR等をRESET_TO_BL31冷起動経路と照合

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-12 Secure handoff

- 何を：BL31→opteed→BL32→Normal
- どこで：固定TF-A source/ELF、将来承認済み停止点
- どう確認：JTAG default branchかFSBL handoff branchか、entry_point_infoとSecure UARTを対応づける
- 期待条件：SPD=opteed、BL32 entry=0x60000000、SPSR/args/security/初期化復帰を確認

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-13 BL33 entry

- 何を：BL31からNS BL33へ渡すEL/PC/SP
- どこで：将来承認済みBL33 _start停止点
- どう確認：CurrentELとSCR/SPSR/ELRは読めるELで取得。NS EL1からEL3 registerを読まない
- 期待条件：PC=0x10200000、NS EL2またはreview済みNS EL1、SP/SPSel/SPSR/ELRを照合

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-14 Stage 2/timer

- 何を：EL2 stateとEL1への遷移
- どこで：将来承認済みfrom_el2/start_el1停止点
- どう確認：HCR_EL2/VTTBR_EL2/VTCR_EL2をEL2以上で記録。NS EL1直接入口では上位snapshotを別途取得
- 期待条件：NS stage2無効HCR.VM=0、RW=1、CNTHCTL/CNTVOFF、EL1H/SP/VBARを確認

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-15 BL33 MMU/cache入口

- 何を：EL1 M/C/Iとdirty cache
- どこで：将来承認済みMMU準備前停止点
- どう確認：SCTLR/TTBR/TCR/MAIR/DAIF/SP/VBARと前stageのcache maintenance sourceを保存。値ゼロだけでdirty無しを証明しない
- 期待条件：EL1、SCTLR_EL1.M/C/I=0、dirty D-cacheなしの引渡しとcoherency手順を確認

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-16 SHM physical/cache

- 何を：両worldのSHM PA/属性
- どこで：生成Core conf、BL33 map、将来MMU snapshot
- どう確認：静的configと実table/TCR/MAIR/PA対応を別々に保存。静的一致だけで実機PASSにしない
- 期待条件：0x10500000..0x10600000、VA=PA、Normal WB inner-shareableをCore/NSで一致。実機での予約/保護も確認

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-17 XMPU snapshot/lock

- 何を：全関連DDR XMPUの実値
- どこで：Vitis APU/IP register view、UG1087各module
- どう確認：bankごとのbase/日時/CPU stateを付記。read side-effectと権限確認後のみ個別read、write/clearしない
- 期待条件：CTRL/POISON/LOCK/全16 region START/END/MASTER/CONFIGと既存policyを保存

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-18 XMPU master/route

- 何を：APU Secure/NS masterとDDR経路
- どこで：UG1085第16章、XSA/PS設定、live XMPU
- どう確認：bank0 base一致だけで全経路を推測しない。生成return 1はpolicy設定の証拠にしない
- 期待条件：master ID/mask、S/NS transaction、port経路、優先順位/default policyを根拠付きで確定

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-19 保護対象/lifetime

- 何を：BL31/Core/TA pool/SHMへの保護割当
- どこで：実予約表と全関連XMPU policy review
- どう確認：NS page tableでSecureをunmappedにした事実と物理protectionを区別。拒否攻撃試験は現段階でしない
- 期待条件：Secure code/data/stacks/TA poolを保護し、SHMだけNS共有として利用できる設計を確認

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-20 UART

- 何を：instance/clock/MIO/baud/console所有
- どこで：BSP/PSU init、UART/clock/MIO register view
- どう確認：3イメージのconsole設定とchannel対応表を保存。FIFO読出しや送信を準備確認に混ぜない
- 期待条件：候補UART0 base 0xff000000と配線/clock/baud/権限をTF-A/Core/BL33で照合

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-21 GIC/他core

- 何を：GIC400 group/routing/priority/他core state
- どこで：GICD/GICC register view、TF-A/Core source
- どう確認：IARは読んでacknowledgeしない。EOIR/SGI/clearを実行せず、banked view/CPUを記録
- 期待条件：GICD_CTLR/TYPER/IIDR/IGROUPR/enable/priority、GICC_CTLR/PMR/BPR、SCR/DAIFと他A53状態をreview

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-22 boot sequence review

- 何を：JTAG/FSBL/PMUFW/BL31/BL32/BL33の順序
- どこで：launch/Tclと手順書、承認済み設計review
- どう確認：未確定なら実行scriptを作動させない。今回収集不能な動的停止点はNOT_RUNで残す
- 期待条件：初期化→load→entry/handoff→停止/復旧をrole別に定義。BL32/BL33を独立conしない

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|

## B2-HW-23 load review package

- 何を：必要証跡と相互整合の最終review
- どこで：全チェック票、元file/hash、旧安全ゲート
- どう確認：確認済み結果の意味と未実施機能/危険設定を整理。load許可は別の明示的な実装・review工程
- 期待条件：証跡が揃っても自動READYにしない。runtime_ready=0/BLOCKEDを維持してreviewへ渡す

|記録欄|での記入|
|---|---|
|実測値／取得値|未取得|
|証跡file・SHA-256|未取得|
|board/CPU/停止点/EL/Security State/run ID|未取得|
|reviewer／UTC日時|未記入|
|判定 PASS／BLOCKED／NOT_RUN／FAIL|NOT_RUN|
|理由・次の作業|の原本/実機証跡は未取得|
