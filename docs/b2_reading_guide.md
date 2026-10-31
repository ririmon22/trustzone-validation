# 3. 初学者向け：仕組みとコードの読み方

[読む順の入口](../README.md)／[範囲と21項目](b2_scope_21.md)／[各試験の仕様と図](b2_test_specification.md)。本書は111版のsourceを読むための説明で、実機操作の許可ではない。

## 1. このソフトは何をするものか

Normal側の小さなプログラムがSecure側へ「この入力を処理してほしい」と依頼し、結果・不正要求の拒否・復旧・後始末を確認する。Linuxは使わず、Normal側のStandalone BL33に必要最小限のTEE clientと、署名TAロード時だけのRPC応答を実装している。

**最初に、旧版でUSER_REPORTEDのECHOを読む。次にTA間分離、最後に署名TAロードへ進む。** これは理解の順番であり、111版でこれらを実機確認済みという意味ではない。

### 今回のcodeに結び付けた用語

|用語|今回の意味・次に読む場所|
|---|---|
|CPU（Central Processing Unit）|A53がNormal/Secureの命令を順番に実行。TAごとに別CPUを用意するわけではない。`tfa_standalone/integration/start.S`|
|World|ArmのSecurity Stateで区別するNormal（Non-secure）とSecure。Normal=Linuxという意味ではない。ここではNormalにBL33、SecureにCore/TA|
|EL（Exception Level：例外レベル）|EL3にBL31、Secure EL1にCore、Secure EL0にuser TA、Normal EL1に試験。Normal EL2入口をEL1へ下ろすcodeもある。`_start`と`read_current_el`|
|VA（Virtual Address：仮想アドレス）|命令が参照するアドレス。同じ数値でもTA-A/Bのmappingは別。`ta/security.c`のprobeはVAの分離試験|
|PA（Physical Address：物理アドレス）|変換後にシステムへ渡すアドレス。physical PTAが返すpage PAとNormal aliasの変換先を比較する。全PA aliasの実機対応は未確認|
|MMU（Memory Management Unit）|page tableに従ってVA→PA変換と権限検査をする。`platform/zcu106/b2/client_mmu.c`。未mapのTranslation FaultだけではXMPU保護の証明でない|
|XMPU（Xilinx Memory Protection Unit）|CPUのpage tableとは別に、アクセス元/security等に応じて物理memory経路を制御するハードウェア。`prepare_xmpu_test.py`と`xmpu_test.tcl`は候補専用regionの確認/限定設定。全経路を自動認定しない|
|SMC（Secure Monitor Call）|NormalからEL3へ入る命令。`common/arch.c:smc_call`がregisterにFIDと引数を置き、`smc #0`を実行|
|RPC（Remote Procedure Call）|この構成ではSecure CoreがNormalへ作業を依頼し、Normalが応答して中断処理を再開する仕組み。別PCへのnetwork通信ではない。`client/signed_rpc.c`|
|TA（Trusted Application）|Coreが管理するSecure EL0アプリ。A/BはEarly TA、署名TAはRAMから実行時ロード。`optee_standalone/ta`と`ta_signed`|
|PTA（Pseudo Trusted Application）|Core内のprivilegedサービス。通常user TAと同じEL0分離を持つ別アプリではない。検証canary/物理page/署名監査は`optee_standalone/pta`と`loader`|
|SHM（Shared Memory：共有メモリ）|Normal/Secureのmessage・buffer・RPCの受渡し領域。秘密鍵そのものの置場ではない。`client.c:message_argument`、B2 mapの0x10500000..0x10600000|
|ELF（Executable and Linkable Format）|entry、LOAD範囲、symbols等を含む実行形式。BL31/BL32/BL33の役割が異なる。ファイルがbuildできても実機handoffが正しいとは限らない|
|ABI（Application Binary Interface）|register/stack/構造体のbinary上の約束。`client.c`のstatic_assert、`state_call.S`のcallee-saved比較を参照|

## 2. 起動時に各ELFが何をするか

1. **PMUFW**：電源管理側を初期化・動作させる。実機対応のfirmware/XSAやready確認情報は別入力。BL33試験の関数をこのCPUへロードしない。
2. **FSBL**：PS/DDR等の起動初期化。`XFsbl_Exit`相当の停止点へ到達しただけでは、その後のEL/security/cacheが正しいとは判断できない。実source/実行時stack/heap予約は今回未確認。
3. **BL31**：TF-AのEL3 monitor。buildで`SPD=opteed`を指定し、OP-TEE payloadを初期化してNormal次イメージへ渡す。ZCU向けentry0x1000は過去buildの値で、都度生成ELFで確認する。
4. **BL32**：OP-TEE Core。SecureのMMU・TA管理・暗号・サービスを提供する。Early TA-A/Bはこのimageへ組み込む。通常署名TAのUUIDは組み込まない。TZDRAM予約0x60000000..0x70000000もソフトウェア案である。
5. **BL33**：`start.S:_start`→`clear_bss`→VBAR設定→`standalone_main`。EL2ならEL1へ下ろし、handoffを記録。EL1直接入口ではEL2状態を自動的に既知としない。
6. `standalone_main`→`b2_results_begin`→EL確認→`platform_prepare_tee_client`→`tee_client_initialize`→選択suite。**現111版はB2_RUNTIME_READY=0なのでこの準備で意図的に止まる。** 旧版の手動変更・成功版と同一だと推測しない。debuggerで定数を書き換える手順は本資料にない。

JTAGの`dow`はファイルを配置する操作で、FSBL/BL31/Core/BL33の正しい初期状態を自動的に同じ条件にするものではない。詳細な条件付き起動計画は[仕様書の起動図](b2_test_specification.md)。BOOT.binによるbootと今回のJTAG計画を混ぜない。

## 3. 最初の例：ECHOの1往復

旧版でユーザーがtest_report入口のTEE-01 PASSを確認した（USER_REPORTED）。これは入口の報告値であり、111版のELF/hash/runを結び付けた新実測ではない。

1. `optee_standalone/tests/functional.c:b2_run_service`はTA-Aへ2つのsessionを開く。sessionはCoreが返すIDと、Normal clientのopen状態で管理する。
2. `tee_session_open`は共有messageをzero初期化し、UUIDとPUBLIC login metadataを置く。session枠はNormal clientで最大4。この4という値はCore全体の容量制限ではない。
3. `tee_invoke(&first, TA_COMMAND_ECHO, 0x45a7)`が呼ばれる。`validation_ta.h`でECHO=0。`client.c`はmessageのcommand=INVOKE_COMMAND、function=0、session=ID、parameter=VALUE_INOUT、value.a=0x45a7へ設定する。
4. `transport_call`→`smc_call(0x32000004, message PA上位, 下位, 0)`。B2の通常mappingはVA=PAのidentityだがMMUは有効で、Secure領域を全部mapしているわけではない。
5. EL3のopteedを経由してCoreへ切り替わり、Coreがsession/parameterを検査してTAの`TA_InvokeCommandEntryPoint`へ渡す。
6. `ta.c`はsessionがNULLでないこと、VALUE_INOUT型であることを検査する。ECHO caseはvalueを書き換えずSUCCESSを返す。
7. Coreがmessage result/origin等を返し、NormalへSMCが戻る。`tee_reply`はtransport/result/origin/valueの4field。`b2_run_service`はtransport=OK、result=SUCCESS、value=0x45a7を比較する。
8. もう一つのsessionでもECHOし、未知commandはBAD_PARAMETERSとなることを確認。3条件が揃ってtest_reportする。両sessionをCloseし、cleanup不備がないことを別に確認する。

読むときは、最初からSMC assemblyをすべて理解しようとせず、「Cの入力値→message→TAの分岐→Cへ戻った値」を紙に書く。次に`common/arch.c:smc_call`でどのregisterが変わってよいかを見る。

## 4. 一つの試験の準備・操作・判定・後始末

TEE-01を例にすると以下になる。

|段階|何をするか|何を見ればよいか|
|---|---|---|
|準備|選択・ready/MMU/SHM初期化、sessionをOpen|b2_test_selection、debug_boot、PREPARE、Open reply|
|操作|ECHO2回と不正command1回|OPERATE、REPLYの4field|
|比較|期待条件をboolへ分解し、b2_check_valueで保存|COMPARE、expected/actual。1は複合条件一致|
|結果保存|test_report→b2_save_report|status/reason/reported=1、SAVE|
|後始末|所有sessionをClose、active数確認|CLEANUP、cleanup_errors=0、DONE|

code上は子のPASS報告がClose前にある。したがってtest_report入口だけで止めた場合、後始末まで成功したとはまだ言えない。`b2_results_end`とrun全体のCOMPLETE/cleanupを確認する。

### 次の例：TA間分離

`security_tests.c:b2_run_ta_isolation`→`probe_other_ta`→TAの`security_invoke`へ進む。

1. Bが自分のsecretのVAを返す。これは試験専用の情報公開で、製品TAへ持ち込む設計ではない。
2. Aからその数値のVAへread/writeを試す前に、A自身にそのVAがmapされていないことをCore syscallで確認する。aliasなら再Openして最大8回、解消しなければBLOCKED。
3. 実load/storeによりAがfaultしてCoreがTARGET_DEAD/origin3を返すことを確認する。意図したTA終了であり、CoreやBL33のfatalは期待結果ではない。
4. clientは死んだsessionもCoreへCloseしてからhandleを閉じる。Bのtagが変わらず、A再Open後の自分のtagも正常でなければPASSにしない。
5. read/writeの両方を比較し、親EXT-TA-V02ではさらにpanic/Core canary/資源反復を補足する。

**TA-AとBの仮想空間が分かれていることと、Normal CPUからSecureのPAを直接読めないことは別。** 後者はTZ-SYS-03-V01の専用PA probeで、現在は条件待ち。

### 最後の例：署名TAロード

`--test EXT-TA-V03`→`signed_ta_tests.c:b2_run_signed_ta`→`signed_rpc.c`→Coreの既存REE TAローダ→通常TAを読む。

1. A/B Early TAはCore内に埋め込まれていて、通常`.ta`ファイルの実ロード署名検証の証拠にはならない。
2. 専用UUIDの通常TAはNormal予約RAMのbundleへ置く。CoreがOpen中に「TAを渡してほしい」とRPCでNormalへ戻る。
3. NormalはRPC用memoryを確保し、サイズ照会とdata copyに応答。RETURN_FROM_RPCで**中断中のOpen**を再開する。新しいOpenを毎回開始するわけではない。
4. Coreの既存署名/digest検査が正常TAを受理し、改変又は別鍵を拒否する。NormalはECHO/count/Close/回収を比較する。
5. 両不正caseのresultはSECURITYで同じでも、Core監査で「改変はsignature成功後digest拒否」「別鍵はsignature拒否」を分ける。単なるファイル形式不良と混同しない。
6. Normalの結果だけでなくPC decoderでbundle hashとCore stageを検査する。不在UUID/size超過はNormal側の安全な受渡し拒否で、署名検証成功数にはしない。

## 5. 例外・エラー・BLOCKEDの違い

- **例外**はCPU制御が例外vectorへ移ること。TA probeの期待fault、NS物理probeのData Abort、予期しないBL33 fatalは別の経路。例外が起きた事実だけで合格にならない。
- **エラー結果**は通常の戻り値であることも多い。ACCESS_DENIED/BAD_PARAMETERSは、指定の不正入力に対しては期待された成功条件になる。ただしresult/origin/継続等が一致する必要がある。
- **FAIL**は実行して期待条件と違った場合。秘密patternがNSへ返る、victimが変化する、Closeに失敗する等。
- **BLOCKED**は必要条件/観測が不足し、意味のある合否を出せない場合。ready=0、PTAなし、VA alias未解消、NS変換だけで拒否、Core監査なし等。同じBLOCKEDでも「未実装」と「実機条件不足」を[21表](b2_scope_21.md)で分けてある。
- **SKIP**は今回方針で試験対象外として実行しない場合。対象外SGIはcodeがBLOCKEDを出すが、資料の範囲分類は「今回対象外」。scopeとruntime enumを混ぜない。
- **NOT_RUN**は未選択/未実施。親の実機NOT_RUNと、RAM内未選択子NOT_RUNも区別する。

BL33では`common/el1_vectors.S`と`common/include/exception_capture.inc`がCを呼ぶ前にESR（Exception Syndrome Register：原因情報）、FAR（Fault Address Register：対象address）、ELR（Exception Link Register：復帰PC）、SPSR（保存status）、SP/GPRを記録する。`debug_exception`は最初の致命的記録を保持する。`debug_sync_exception`は同期例外の記録なので後の想定probeで更新され得る。

物理probeの`b2_handle_phys_fault`は指定命令PC・FAR・read/write種別等が一致する場合だけ限定的に復帰。任意faultを「試験用だから」と握り潰さない。最適化されたC引数のGUI表示ではなくRAM recordを読む。

## 6. RAM結果とtxtの読み方

RAMにfilesystemがある想定ではない。XSDBがbinaryをPCへ保存し、PythonがPC上のtxt/JSON/CSVを作る。

1. 使用BL33/Core ELFで回収計画を作り、symbolからaddress/sizeを取る。ELFのSHA-256とrun IDを記録するが、デバイスがそのELFを実行したことの自動認証ではない。実際のロード履歴とも照合する。
2. 正常完了し、許可された停止状態からRAMを取得。cache有効時は完了publishでPoCへcleanするが、JTAG読出し経路とcoherencyは実機確認事項。途中dumpは部分証拠。
3. txt先頭のCollection/state/phase/cleanupを読む。COMPLETEでも、試験結果はFAIL/BLOCKEDの可能性がある。
4. 各子のselected/reported/status/phaseとchecksを読む。未選択をPASSにしない。
5. review欄で21親のstatusと理由を見る。署名欄はCoreの拒否stageも読む。性能欄のMEASUREDは速度閾値合格ではない。
6. `dropped>0`、run/ELF/version不一致、欠落、未完了は信頼できる完全結果として扱わない。raw入力を保存し、毎回新しいoutputを使う。

以下は**形式説明だけの模式例であり、実測ログではない**。

```text
B2 results version=3 run=<run_id> ELF=<sha256>
Collection=COMPLETE state=COMPLETE phase=DONE cleanup_errors=0
TEE-01 PASS phase=DONE selected=True two-sessions+echo+invalid-command
  first-echo expected=0x1 actual=0x1 matched=True
```

この行が意味するのは「両session、echo、不正command、後始末を含む指定範囲の証拠が揃う」こと。親のNormal/Secure物理保護を証明しない。

```text
REVIEW_RESULT TZ-SYS-03-V01 status=3 XMPU-path-master-lock-and-dedicated-region-review-required; use-selection-15
```

status3=BLOCKED。codeなしという意味ではない。専用probeを別選択すること、物理条件を確認することが必要。XMPUの設定readbackだけでも合格にはならない。

```text
PHASE TEE-ISO-TA stage=3
```

stage3=EXPECTED_FAULT。意図したTA fault区間へ入っただけで、この時点は未完了。後のTARGET_DEAD/origin、B非改変、A復旧、Closeと最終statusを読む。

`log_printf`は小さなfreestanding実装。整数/文字列中心、浮動小数点や動的確保なし。全Core/TAログがここへ集まるわけではない。TF-A/Core/TAの詳細trace、外部firmware起動前のUART出力は別機構。PCから回収する主証拠はBL33結果と、PTAが明示的に返したCore監査である。

## 7. 次に読むsourceと関数（順番）

|順|ファイル|注目する関数・定義|読む目的|
|---|---|---|---|
|1|optee_standalone/tests/functional.c|cases、b2_run_service、b2_run_functional_suite|試験の入口と一つの完成した例|
|2|optee_standalone/client/client.c|tee_session_open、tee_invoke、transport_call、tee_session_close|session/message/戻り値、TARGET_DEAD後のClose|
|3|optee_standalone/ta/ta.c|TA_OpenSessionEntryPoint、TA_InvokeCommandEntryPoint|ECHOとtoken分岐、session状態|
|4|optee_standalone/ta/validation_ta.h|TA_COMMAND_*、UUID、試験値|command番号とA/Bの差|
|5|common/arch.c|smc_call、counter_read、platform_halt|World切替要求、counter、待機。WFE待機とdebug停止は別|
|6|tfa_standalone/integration/start.S、platform/zcu106/b2/client_mmu.c|_start、platform_prepare_tee_client、zcu106_b2_build_page_tables|EL入口、ゲート、MMU/cacheと写像|
|7|optee_standalone/tests/security_tests.c、optee_standalone/ta/security.c|probe_other_ta、security_invoke、trusted_read、key_attribute_test|分離・TA認可・鍵属性の検査層|
|8|optee_standalone/tests/review.c|parent_ids、inherit、run_state_retention、run_token_policy、supplement_isolation|子の結果を親21項目へ対応付ける方法|
|9|optee_standalone/tests/physical_memory.c、physical_policy.c|b2_run_physical_memory、b2_handle_phys_fault、b2_classify_physical_abort|TA VM分離と物理保護の違い|
|10|scripts/prepare_xmpu_test.py、scripts/jtag/xmpu_test.tcl|prepare、tz_xmpu_test_apply|候補専用regionだけの確認・設定・readback。実機未検証|
|11|optee_standalone/tests/signed_ta_tests.c、client/signed_rpc.c|b2_run_signed_ta、signed_rpc_call、load_ta、signed_rpc_end|署名TAを実ローダへ渡す経路と回収|
|12|scripts/stage_signed_loader.py、optee_standalone/loader/*|apply、signed_loader_begin/note、監査PTA|外部Coreのどこを監査するか。元sourceを作業copyで変更|
|13|scripts/decode_b2_results.py、decode_signed_results.py、decode_b2_review.py|decode、decode_signed、review、benchmarks|RAMを証拠として受理する条件|
|14|scripts/build_zcu106.py、build_external.py、select_b2_test.py|build入口、configured_plan、選択CLI|Core/TA/BL31/BL33 profileの対応|

外部sourceは別配置のTF-A `bl31/bl31_main.c`と`services/spd/opteed/opteed_main.c`、OP-TEE `core/tee/entry_std.c`と`core/kernel/ree_fs_ta.c`へ進む。111版には外部本体がない。lockファイルは参照版であり、ZIP由来sourceの固定commit証明ではない。

## 今回未照合の部分と資料の限界

実Vitis GUI、XSDBのstate属性やfilterの挙動、PMUFW/FSBL実source・実行時使用、全XMPU経路/master/lock、実機cache/JTAG整合性はNOT_RUN又は未取得。資料作成でこれらを確定しない。codeの判定条件を説明しているのであって、試験に通らない場合に要件を足したり弱めたりしていない。
