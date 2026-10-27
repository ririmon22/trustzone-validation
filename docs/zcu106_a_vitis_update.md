# ZCU106 Vitis実測情報の反映：構成Aの静的移植準備

構成A（自作EL3＋Secure EL1＋Non-secure EL1）を優先する。で確認した数値を **USER_REPORTED** 情報として反映した。元のXSA/BSP/linker、FSBL/PMUFW ELF、launch.jsonは本環境へ未提供のため、hash・processor address view・実行時予約の確認済みとは扱わない。GPIOとDMAの機能・性能検証は保留し、既存実装は残した。全実機NOT_RUN、load gate BLOCKED。実機アクセス、JTAG接続、QEMU起動は今回行わない。

## 既存実装との差分

|対象|以前|今回の反映|
|---|---|---|
|DDR/OCM|生成物との照合なし|DDR0 0..0x7ff00000、OCM 0xfffc0000..0x100000000を報告値として管理|
|DDR1|使用しないが具体的な報告値なし|0x80000000＋0x80000000をそのまま保存。有効範囲未確定のため全域をA配置から除外|
|FSBL/PMUFW|予約情報nullのみ|entryと計7 PT_LOADのPhysAddr/MemSiz/排他的endを記録し、A全予約と数値上の重複を検査|
|A配置|EL3 0x60000000、S-EL1 0x60020000、NS-EL1 0x10200000|DDR0内かつ報告LOADと非重複のため入口を維持。TF-A/OP-TEEはA起動から除外|
|stack|header/linkerでtopのみ定義、ELFに全予約なし|3 linkerに独立RW・非実行NOLOAD segmentを追加し、PT_LOAD p_memszで全stackを検査|
|EL3 entry|SP/BSS初期化後にCでEL3を確認|ZCU106のみ、SP/BSS前にCurrentELとSCTLR_EL3.M/C/Iを確認。不一致はentry_state_blockedで停止|
|JTAG|具体的launch設定未提供|Local、FSBLあり、Entire System reset ON、APU/RPU reset OFFを報告値として保存。handoff/EL/PMUFWは未確定|
|XMPU|bank経路・実値不明|XMPU0 base=0xfd000000は既存定義と一致。initのreturn 1だけでは保護設定済みと判定しない|

データは `platform/zcu106/vitis_observations.json`、構成A計画は `jtag_a_plan.json`。従来B2向け `boot_plan.json`とは別であり、jtag_a_planをcheck_boot_plan.pyへ渡す形式ではない。全体mapの旧TF-A/OP-TEE予約は互換性維持のため残すが、Aの配置検査・load計画では使用しない。B1/B2の配置不整合は未解決のまま。

## メモリ配置案

全endは排他的。下表の範囲は**配置案**であり、Secure属性が実機で保護済みという意味ではない。全A予約をDDR0内に限定し、OCM全域とPMUFW報告範囲をアプリ配置に使用しない。既存保護設定や他masterのアクセスは未確認。

|用途|Base|End|備考|
|---|---:|---:|---|
|EL3 text/data/BSS|0x60000000|0x60010000|_start=0x60000000、text上限ASSERT追加|
|EL3 exception vectors|0x60010000|0x60020000|2048-byte alignment|
|Secure EL1 text|0x60020000|0x60030000|_start=0x60020000|
|Secure EL1 data/BSS/vectors|0x60030000|0x60040000|text/dataの境界を検査|
|Secure stack|0x60040000|0x60050000|64 KiB、NOLOAD RW|
|EL3 stack|0x60060000|0x60070000|64 KiB、NOLOAD RW|
|NS EL1 text|0x10200000|0x10300000|_start=0x10200000|
|NS EL1 data/BSS/vectors|0x10300000|0x10400000|1 MiB予約|
|NS stack|0x10400000|0x10500000|1 MiB、NOLOAD RW|
|Shared memory|0x10500000|0x10600000|1 MiB専用予約。通常アクセス可能性は未実行|
|DDR XMPU検査専用窓|0x62000000|0x62100000|1 MiB境界、code/stack/firmwareを含めない|
|実検査buffer|0x62010000|0x62020000|上記窓内の64 KiB|

heapは自作Aのfreestandingコードで提供せず、linkerの`__heap_start == __heap_end == __bss_end`で0 sizeを明示する。動的TA領域はA対象外。stack使用量の実行時high-water、割込みnest、MMU/cacheの残留状態は未検証で、予約が存在することとは別。NOLOAD/p_memsz予約によりJTAG loaderがzero-fillを行う場合も当該stack予約内へ限定する。

DDR0の報告上限は0x7ff00000で、0x7ff00000..0x80000000もアプリには使わない。DDR1の報告範囲はOCM・MMIO等と重なり、有効DDR apertureと判断できない。桁違いを疑って0x800000000へ自動修正することもせず、生成linker/SDT/XSAとUG1085第10章で再照合する。

## FSBL／PMUFWの報告LOADと静的検査

|Image / Entry|PhysAddr|MemSiz|End|
|---|---:|---:|---:|
|FSBL / 0xfffc0000|0xfffc0000|0x23b40|0xfffe3b40|
|FSBL|0xfffe3b40|0xed0|0xfffe4a10|
|FSBL|0xfffe9e00|0x88|0xfffe9e88|
|FSBL|0xffff0040|0xfc00|0xfffffc40|
|PMUFW / 0xffdcfab4|0xffdc0000|0x1ac60|0xffddac60|
|PMUFW|0xffddac60|0x1928|0xffddc588|
|PMUFW|0xffddf6e0|0x400|0xffddfae0|

MemSizを使い、BSS相当も含む数値範囲として扱う。FSBL全LOADは報告OCM内で、各imageのentryはその報告LOAD内。Aのtext/data/BSS/vector/stack/shared/検査窓とこれら7範囲に数値上の重複はない。LOAD間の隙間はfree RAMとみなさない。

この結果は元ELFの照合でも、FSBLのDDR scratch/heap/stack/引渡し後生存期間が全て判明したという意味でもない。PMUFWのprocessor viewとsystem view/aliasも未確認。従来 `external_reservations_complete=false` と外部runtime予約未確定のゲートを維持する。

## EL3開始条件とJTAG計画

1. 元launch.jsonと生成initialization scriptを取得し、FSBL指定、resetの発生順、PMUFW起動、A53 #0の停止点を確認する。Local/FSBL/Entire System resetの設定だけからEL3到達やXMPU初期化を推測しない。
2. PSU/DDR/MIO/clockを生成物と照合し、FSBLがcustom EL3へ渡すEL/security/PCを確認する。FSBLを最後まで走らせた後に単純にEL3 ELFを通常appとして開始できるとは限らない。
3. 他core/masterの状態、debug認可、実PC/CurrentEL/DAIF/SPSel、SCTLR_EL3/TTBR/TCR/MAIR、cache/dirty data、VBARとstack、XMPU状態を確認する。最初から安全なMMU/cache状態と仮定しない。
4. 今回の入口契約はEL3、SCTLR_EL3.M=0/C=0/I=0。満たさない場合は、確認済みの初期化手順を設計し直す。自作entryはcacheを強制無効化して続行しない。EL3確認後にSPSel=1を選び、予約SPを設定する。
5. ZCU106用entry guardはSP/BSSとUARTへ触れる前に停止できる補助機構。ただし入口への命令fetch自体の安全性やMMU変換、dirty cache/外部master、保護設定の証明にはならない。wrong EL時はx0=CurrentEL、EL3でcache/MMU不一致ならx1=SCTLR_EL3を保持する。通常のC側`PLATFORM_PROTECTION_CONFIRMED=0`による停止も維持する。
6. 将来の候補順は、確認済みPSU/DDR/PMUFW/FSBL初期化→A53停止→S/NS payload load→custom EL3 load→入口契約を確認→EL3だけ開始。Secure/NSをデバッガから個別conしない。この順序は未検証で、ロード/起動scriptは生成・実行しない。
7. 自作EL3がVBAR_EL3とSCR_EL3、各worldのSPSR/ELR/SPと状態保存を担当する。既存のSecure→Normal開始、SMC切替、register保持試験を再利用し、BSP標準startupやTF-Aを混在させない。

現在 `jtag_a_plan.json` のload_authorized=false、実行script=null。既存の広いハードウェアゲートは保持し、GPIO/DMA保留を理由に迂回しない。将来A専用のload許可条件を確定する場合も、未検証経路や保護条件を除外するだけの変更はしない。

## XMPU：静的に言えること／実機で必要なこと

|区分|内容|
|---|---|
|静的確認|XMPU0 base=0xfd000000は既存protection.h/公式UG1087定義と一致。DDR XMPU窓は1 MiB境界、buffer64 KiBは内包。code/stack/firmwareとは非重複|
|報告のみ|XMPU0〜5生成init関数のbodyがreturn 1のみ。取得された関数bodyにregister設定の証拠がないという範囲に限定する|
|推測不可|XMPUが無効、reset defaultのまま、FSBLや別処理も一切設定しない、unlock済み、NS read/write拒否済み、という推論はいずれも不可|
|実機確認|全関連bankのCTRL/POISON、region START/END/MASTER/CONFIG、lockと違反捕捉、APU NS/Secure master/route、default policy、優先順位、他core/master排他|
|保護試験|Secure/通常NSの許可access→NS禁止read/write→同一要求に対応する新規violation/ESR/FAR→Secure側非改変。例外だけでPASSにしない。Secure code/data/stack保護も別途必要|

GPIO/XPPU周辺試験、DMA機能/性能は保留（実行結果NOT_RUN、準備状態BLOCKED）。既存backend/host fixtureは削除せず、今回は新たなMMIOコードや推測master値を追加しない。

## ビルドと静的検査の再現

のGNU compilerが確認できる場合は[環境手順](manual/01_environment_setup.md)で`TZ_CROSS`を設定してから実行する。

```bash
bash scripts/build_vitis_offline.sh --configuration baremetal --platform zcu106 --compiler gcc --cross-compile "$TZ_CROSS"
```

本開発hostで今回実行したのはClang/LLD 21.1.8。Ubuntu24.04/Vitis/GNUでのBUILD PASSではない。外部Python package・TF-A/OP-TEE sourceはAのbuildに不要。

```bash
bash scripts/build_vitis_offline.sh --configuration baremetal --platform zcu106 --compiler clang
```

helperは3 ELF生成後にcheck_memory_map.pyとcheck_zcu106_a.pyを実行する。receiptのworkspace/evidenceに実pathを記録する。checker単体はELFの静的検査であり、コンパイルした証拠はbuild log/receiptで別に確認する。

```bash
export TZ_A_BUILD="$(python3 -c 'import json; print(json.load(open("results/phase8/latest-baremetal-zcu106.json"))["workspace"])')"
python3 scripts/check_zcu106_a.py --build-dir "$TZ_A_BUILD/build/zcu106" --output results/zcu106-a/static-report.json
python3 scripts/check_zcu106_a.py --build-dir "$TZ_A_BUILD/build/zcu106" --require-load-ready --output results/zcu106-a/load-readiness.json
```

最後のcommandはSTATIC PASSであってもload BLOCKEDとしてexit2を返す。既存check_hardware_profile.py/check_boot_plan.py/require-protection検査も維持する。結果集計と実logの対応は [A移植確認記録](../results/zcu106-a-summary.json)。過去Phase8のQEMU PASSは過去バイナリの証跡であり、この改修版の新規実行PASSにはしない。

## 根拠と未取得資料

- ユーザーが今回提示したVitis 2025.1の数値・launch設定：vitis_observations.jsonに出所を明記。元file未取得。
- [AMD UG1085 v2.5 第10章 System Addresses / Global Address Map](https://docs.amd.com/v/u/en-US/ug1085-zynq-ultrascale-trm)：アドレスapertureの照合。第16章System Protection Unitsは保護経路の根拠。生成メモリ範囲や保護実設定を代わりに確定しない。
- [AMD UG1087 Module Summary](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/Module-Summary)：DDR XMPU block base。個別registerは既存[backend根拠](zcu106_backends.md)と照合。
- [UG1400 2025.1 User Managed Mode](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/User-Managed-Mode)：通常Standaloneと自作entryのbuild/debug設定を区別。実GUI NOT_RUN。

次に必要なのは、生成linker/SDT/XSA、FSBL/PMUFW ELFとmap（runtime stack/heap含む）、元launch.jsonとinit script、CurrentEL/SCTLR等の停止時snapshot、全関連XMPU設定/lock/routeの取得。これらが揃うまでload条件を確定しない。

## 今回の実測結果

|確認|結果|証跡|
|---|---|---|
|Aソース再build|3 ELF BUILD PASS、Clang/LLD使用|results/zcu106-a-summary.json内のbuild_receipt/evidenceとbuild-baremetal.log|
|配置/ELF|STATIC PASS。全A予約は報告DDR0内、FSBL/PMUFW計7 LOADと非重複。stack NOBITS/RW全予約とentry一致|results/zcu106-a/static-report.json、各readelf.log|
|入口guard|逆アセンブルでCurrentEL/SCTLR確認→停止分岐→SPSel/SP→clear_bssの順を確認。実行は未実施|results/zcu106-a/el3-entry-disassembly.log|
|host regression|既存120件を含む133件PASS|results/zcu106-a/host-tests.log|
|B1/B2 BL33|共有linkerの回帰確認として2 ELF BUILD PASS。TF-A/Core本体は今回未再build|results/zcu106-a/bl33-linker-regression.log|
|過去Phase8 artifact|24個すべてhash不変|results/zcu106-a/previous-artifact-hash-check.json|
|load/readiness|BLOCKED。専用readiness/既存profile/旧boot plan/require-protectionは全て停止|results/zcu106-a/gate-results.json|
|実機/Vitis GUI/GNU/QEMU改修版実行|全てNOT_RUN|今回実行せず|

13追加host試験では、DDR1利用、DDR0範囲不足、報告firmwareとcode/保護窓の衝突、firmware同士の衝突、報告end/entry不整合、stack予約欠落/実行可能化、linker stack size不一致を拒否し、XMPU0 baseの一致だけではauthorityが成立しないことを確認した。host成功は保護実機試験の代わりではない。
