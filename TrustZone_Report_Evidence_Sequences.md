# TrustZone試験 T-01〜T-21：report.txt・シーケンス・コード根拠

改訂日：2026-11-08。**利用者提示の21項目表の連番・項目名・順序に統一した改訂版。** 前版の10グループの番号は使用しない。

対象コード：`refactor/readable-components-review-20261102`、commit `a94646ea4130b448961c7921c57d1d317da8f36b`。リンクはこの版へ固定。実際のreport・raw dump・ロード済みELFは未受領のため、本書の期待値はコード上の条件であり実測結果ではない。手元の未push修正、特に物理試験・TF-AのSError対応は差分確認が必要。

**読む順番：T番号で項目を選ぶ → reportの正式IDを検索 → checkと期待値 → シーケンス → コード → 公式資料。**

各章の冒頭に「要求表の検証方法・合格基準」を置き、その下で現行実装の範囲を説明する。表が求める範囲より狭い試験のPASSを、そのまま要求全体の合格にしない。

## 1. T番号とreport名・実装上の親番号の対応

**T番号と`B2_REVIEW_PARENT`は一部で順番が違う。** T-11を実行するためにparent=11を選ぶと鍵API試験になり、起動署名試験にはならない。必ず下表の正式IDを介して照合する。

|連番ID|元の21項目ID／reportの親test_id|検証項目|B2_REVIEW_PARENT|reportの詳細証拠|現行結果の採用範囲|
|---|---|---|---|---|---|
|[T-01](#t-01)|`TZ-SYS-01-V01`|Secure／Normalの別々の実行|1|TEE-01|reportのPASSはサービス往復。両側EL/Stateの直接証拠は別途|
|[T-02](#t-02)|`TZ-SYS-02-V01`|Secure呼出し・復帰|2|TEE-01|往復・Normal継続。復帰PCの直接比較は別途|
|[T-03](#t-03)|`TZ-SYS-02-V02`|繰返し切替と状態保持|3|親自身の16check|256回と指定状態の保持|
|[T-04](#t-04)|`TZ-SYS-03-V01`|Secureメモリの読書き禁止|4|TZ-SYS-03＋PHYSICAL EVIDENCE|reviewはBLOCKED。専用実行で4probeと追加証跡|
|[T-05](#t-05)|`TZ-SYS-03-V02`|秘密領域と共有領域の使い分け|5|TEE-SHM-01 / TEE-BOUNDARY-01 / TEE-PARAM-01|SHM・型・内容・境界の実装範囲|
|[T-06](#t-06)|`TZ-SYS-04-V01`|Secure周辺機器の保護|6|なし|SKIP・未実施|
|[T-07](#t-07)|`TZ-SYS-05-V01`|DMAからのSecureメモリ保護|7|なし|SKIP・未実施|
|[T-08](#t-08)|`TZ-SYS-06-V01`|Secure割込みの配送|8|なし|BLOCKED・未実施|
|[T-09](#t-09)|`TZ-SYS-07-V01`|JTAGデバッグ制限|9|なし|SKIP・未実施|
|[T-10](#t-10)|`EXT-AUTH-V01`|サービスの認証・認可|10|TEE-AUTH-TA＋親token検査|TA identityと固定tokenのみ。本物のNormal要求者認証は未実装|
|[T-11](#t-11)|`EXT-BOOT-V01`|起動イメージ署名検証|15|なし|SKIP・未実施|
|[T-12](#t-12)|`EXT-KEY-V01`|鍵を外に出さない暗号サービス|11|TEE-KEY-API / TEE-KEY-ATTR-01|API・暗号のみ。生メモリ抽出は未実施|
|[T-13](#t-13)|`EXT-STORE-V01`|Secureデータ永続保存|16|なし|SKIP・未実施|
|[T-14](#t-14)|`EXT-TA-V01`|User TAの呼出し|12|TEE-01 / TEE-SESSION-01 / TEE-ABI-01 / TEE-SESSION-CORE-01 / TEE-PARAM-01|指定入力・client4枠・session等の範囲|
|[T-15](#t-15)|`EXT-TA-V02`|2つのUser TAの分離|13|TEE-07 / TEE-ISO-TA＋Core・stress補足|子2件＋Core canary＋512反復を別々に確認|
|[T-16](#t-16)|`EXT-TA-V03`|署名TAのロード・改ざん拒否|14|TEE-04＋SIGNED LOADER|8ケースとCore署名/digest監査が必要|
|[T-17](#t-17)|`PERF-01`|Secure呼出しの往復時間|17|BENCHMARKS.series_id=PERF-01|MEASURED・閾値なし|
|[T-18](#t-18)|`PERF-02`|メモリアクセス性能|18|BENCHMARKS.series_id=PERF-02|NS cached SHM store区間のみMEASURED|
|[T-19](#t-19)|`PERF-03`|Secure割込み応答時間|19|なし|BLOCKED・未測定|
|[T-20](#t-20)|`PERF-04`|許可領域のDMA転送性能|20|なし|SKIP・未測定|
|[T-21](#t-21)|`PERF-05`|TA呼出し・暗号処理時間|21|BENCHMARKSのPERF-05 / 06 / 08 / 09 / 10 / 11|6系列MEASURED。Storage未測定|

親単独を選ぶ一般形は`B2_TEST_INDEX=17 B2_REVIEW_PARENT=<上表の番号>`。**この説明は選択対応であり、未実装項目を有効にする操作ではない。** T-04の物理実probeは別選択15、T-16の署名单独は選択16。実行profile・Core・ゲートも別に一致させる。

|特に間違えやすい組合せ|T番号|実装上の親番号|
|---|---|---|
|EXT-BOOT-V01|T-11|15|
|EXT-KEY-V01|T-12|11|
|EXT-STORE-V01|T-13|16|
|EXT-TA-V01|T-14|12|
|EXT-TA-V02|T-15|13|
|EXT-TA-V03|T-16|14|

### 「OK」の3段階を区別する

|段階|表示／判定|意味|
|---|---|---|
|回収|Collection COMPLETE|必要な実行記録が揃う。FAILやBLOCKEDを完全に回収できる場合もある|
|実装した小試験|reportのPASS、またはMEASURED|コードに書かれた範囲だけが成立|
|利用者の要求表|各T項目の合格基準を満たすか|未実装・未測定の小項目も含めて判断。reportがPASSでも不足が残り得る|

本書の「未実施／保留／不足」は要求表に対するレビュー上の扱いであり、reportを書換えて生成したstatusではない。T-10/T-12の現行reportは限定範囲でPASSを出すが、要求表全体のPASSに読み替えない。

## 2. report.txtを開いたら最初に見るところ

### 2.1 reportはどこから来るか

1. BL33の試験関数が操作し、`b2_check_value`に期待値と実測値を渡す。
2. `test_report`が子試験statusを`b2_results`へ保存する。
3. review実行では、子結果を親へ引き継ぎ、親自身の追加検査を`b2_review_results`へ保存する。
4. 停止後、JTAGで結果RAM・ログ・追加証跡を`.bin`として保存する。
5. PC上の`decode_b2_results.py`がELF・run ID・回収計画との対応を検査して`report.txt`、`.json`、性能`.csv`を出力する。

`report.txt`はBL33から直接出るファイルではない。RAMの結果をPCで整形したもの。TF-A/OP-TEE/TAの全内部traceも含まれない。

根拠：[tests/validation/functional.c：b2_check_value](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L116)、[tests/validation/functional.c：b2_save_report](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L141)、[runtime/test_runner/runner.c：test_report](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/runtime/test_runner/runner.c#L21)、[scripts/decode_b2_results.py](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/scripts/decode_b2_results.py#L1)。

### 2.2 共通の証拠確認

|見る場所・検索文字|OKの目安|何が分かるか|
|---|---|---|
|冒頭`Collection=`|`COMPLETE`|必要記録を回収・解析できた。全試験合格とは別|
|同じ行`state=`、`phase=`|`COMPLETE`、`DONE`|子結果の実行記録が終了した|
|`cleanup_errors=`|`0`|記録したsession等の後始末に失敗していない|
|子試験行`selected=`|評価する子は`True`|未選択をPASSに数えない|
|子試験行のstatus|対象子が`PASS`|子の複合判定が成立した|
|子の比較行`expected`/`actual`/`matched`|期待値と一致、`matched=True`|記録された比較が一致した|
|`REVIEW`の対象`test_id`|対象機能は`status: PASS`、`phase: DONE`|親の集計。子証拠とセットで読む|
|`REVIEW`内`cleanup_errors` / `collection_status`|`0` / `COMPLETE`|親追加試験を含む後始末・記録完了|
|`SIGNED LOADER`|`status: PASS`。UNVERIFIEDでない|T-16の追加監査をPC側でも照合できた|
|`PHYSICAL EVIDENCE`|4 attemptの厳密条件|T-04。別途XMPU設定・違反情報も必要|
|`BENCHMARKS`|`collection_status: COMPLETE`、`errors: 0`|T-17・T-18・T-21の測定成立|
|`BL33 RAM LOG`|対象のREPLY/CHECK/REVIEW/RESULTと`END`|操作の返り値や途中経過。全体完了と併読|

**report本文にFAIL/BLOCKED/SKIP/NOT_RUNがあること自体を隠さない。** 一式では未対応・別実行の項目も正しく表示される。選択していないNOT_RUNは未実施。別runのPASSを埋め込まない。

decoderは終了code0でも親だけのBLOCKEDを集計しない経路がある。終了codeやPASS件数だけで判定しない。各対象ID・reason・追加証跡を見る。

### 2.3 出力の形と数値の読み方

以下は**形式例で実測ではない**。子の行はテキスト、REVIEW等の詳細はJSON形式で同じtxtに埋め込まれる。

```text
Collection=COMPLETE state=COMPLETE phase=DONE cleanup_errors=0
TEE-01 PASS phase=DONE selected=True two-sessions+echo+invalid-command
  first-echo expected=0x1 actual=0x1 matched=True
  second-echo expected=0x1 actual=0x1 matched=True
  unknown-command-rejected expected=0x1 actual=0x1 matched=True
```

```json
{"test_id":"TZ-SYS-01-V01","status":"PASS","phase":"DONE",
 "checks":[{"name":"functional-status","expected":0,"actual":0,"matched":true}]}
```

|値|意味|
|---|---|
|`expected=1 actual=1`の複合check|`transport/result/value`等の条件をまとめたboolがtrue。返却データ1とは限らない|
|`functional-status expected=0 actual=0`|test enumのPASSが0なので一致。処理未実行という意味ではない|
|`transport=0`|自作client通信が正常完了。TA処理の成功とは別|
|`result=0x00000000`|TEE_SUCCESS|
|`origin=3`|OP-TEE Core側の結果|
|`origin=4`|TAサービス側の結果。PTAの戻りにも出る|
|`0xffff0001`|ACCESS_DENIED。意図した拒否ならOK|
|`0xffff0006`|BAD_PARAMETERS。意図した拒否ならOK|
|`0xffff000f`|SECURITY|
|`0xffff3024`|TARGET_DEAD。意図したTA異常終了ならOK|

親詳細JSONは数値が10進になる。例えば`4294901761`は`0xffff0001`。Pythonで`hex(4294901761)`として確認できる。`REVIEW_RESULT ... status=0`等のRAMログは数値enum、PC側親表は文字列statusである。

根拠：[scripts/decode_b2_results.py](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/scripts/decode_b2_results.py#L1)、[scripts/decode_b2_review.py](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/scripts/decode_b2_review.py#L1)。値の公式定義は後述O5。

## 3. 共通シーケンス：最初に一度だけ説明する

### 3.1 登場人物

|名前|担当|コード|
|---|---|---|
|BL33|Normal EL1で試験を実行し、応答と期待値を比較してRAMへ保存|`tests/validation/`、`normal_world/client/`|
|TF-A|BL31、EL3。SMCを受け、OP-TEEとのWorld切替を仲介|外部`services/spd/opteed/opteed_main.c`|
|OP-TEE|BL32、Secure EL1。message検査、session管理、TA実行、鍵API等|外部`core/tee/entry_std.c`、`core/kernel/tee_ta_manager.c`|
|TA-A|通常の試験対象TA。session認可・counter・鍵・攻撃側など|`secure_world/ta/ta.c`、`security.c`|
|TA-B|別UUIDのTA。被保護側・継続確認・TA間認可先|同じソースを`TA_VARIANT_B`で別ビルド|

同じA53上で順番に実行が切り替わる。5つの別CPUが並列動作する図ではない。PTAはOP-TEEの箱の中に書く。署名TAはTA-A/Bとは別UUIDなのでT-16だけ追加の登場人物として示す。

### 3.2 Open → Invoke → Close

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    N->>N: tee_session_openでUUIDとOpen messageをSHMへ
    N->>F: smc_call CALL_WITH_ARGとmessageのPA
    F->>O: opteedがSecureへ切替
    O->>O: 入力検査とsession管理
    O->>A: 必要ならCreate、OpenSessionEntryPoint
    A-->>O: TEE_Result
    O-->>F: session IDとmessageの結果を用意
    F-->>N: Normalへ復帰
    N->>N: tee_invokeでsession IDとTA command、引数をSHMへ
    N->>F: CALL_WITH_ARG
    F->>O: Secureへ切替
    O->>A: TA_InvokeCommandEntryPoint
    A-->>O: TEE_Resultと出力parameter
    O-->>F: messageのretとret_originを更新して戻る
    F-->>N: Normalへ復帰
    N->>N: transport、result、origin、valueを比較
    N->>F: Close messageをSMCで渡す
    F->>O: session Close
    O->>A: CloseSessionEntryPoint、必要ならDestroy
    O-->>F: Close結果
    F-->>N: 復帰してclient管理状態を更新
    Note over B: この共通例では未使用
```

矢印の「返る」は制御と結果の伝達を模式化したもの。TAがTF-Aへ直接returnするわけではない。TAとCore間はuser mode entry/syscall、World間はSMCという別の境界。

|段階|コードを開く場所|見る処理|公式根拠|
|---|---|---|---|
|実行選択|[tests/validation/main.c：standalone_main](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/main.c#L284)、[tests/validation/functional.c：b2_run_functional_suite](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L268)|準備後、selectedの関数を呼ぶ|試験選択・PASS条件は自作仕様|
|message作成|[normal_world/client/client.c：tee_session_open](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/client.c#L151)、[normal_world/client/client.c：tee_invoke](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/client.c#L177)|command/session/function/count/attr/aを書込|O1・O2|
|SMC|[normal_world/client/client.c：transport_call](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/client.c#L71)、[runtime/arch.c：smc_call](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/runtime/arch.c#L24)|DSB後、message PAをx1/x2に分けて渡す|O2・O3・O8|
|Coreへdispatch|O3 `opteed_smc_handler`|Normal/Secure context切替|O3|
|Core入力・session|O4 `entry_open_session` / `entry_invoke_command` / `entry_close_session`|memref・型・session検査、TAへdispatch、結果をmessageへ|O1・O4|
|TA処理|[secure_world/ta/ta.c：TA_InvokeCommandEntryPoint](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/ta.c#L142)、[secure_world/ta/security.c：security_invoke](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/security.c#L169)|独自command番号で分岐|O5はAPIの根拠。command番号・期待値は独自|
|復帰・比較|[normal_world/client/client.c：transport_call](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/client.c#L71)、各試験関数|SMC完了とmessageの処理結果を別々に読む|O2・O4|
|結果保存|[tests/validation/functional.c：b2_check_value](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L116)、[tests/validation/review.c：inherit](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L102)|expected/actual・子status・親status|自作判定規則|

`tee_invoke`はこのrepoの自作関数。GlobalPlatformの`TEEC_InvokeCommand`そのものではない。Linux driver/tee-supplicantを使う一般例と、本構成のbare-metal BL33＋限定RPCを区別する。

以後の図で、途中を`SMCで要求`とまとめた箇所もこの共通経路を通る。TA内からCore APIを呼ぶたびにTF-Aへ出るわけではない。

<a id="t-01"></a>

## T-01：Secure／Normalの別々の実行（TZ-SYS-01-V01）

|要求表の欄|内容|
|---|---|
|検証観点|実行状態の分離|
|検証方法|両側の試験コードでCurrentELとSecurity Stateを確認する。|
|合格判定基準|各停止点のEL・Security Stateが設計値に一致し、両側の処理が実行される。|
|reportの親test_id|`TZ-SYS-01-V01`|
|B2_REVIEW_PARENT|`1`。T番号とは区別|

### 要求表に対して足りる証拠・足りない証拠

現行コードはBL33入口でCurrentEL=EL1を検査し、TEE-01成功からSecure TAサービスの実行を確認する。一方、**この親のPASS判定にSecure側CurrentEL・両側Security State・各停止点の実測一致は含まれていない。** よってreportだけで要求表のT-01全体を合格とはしない。

要求全体を採用するには、設計した停止点ごとのEL/Worldとその観測根拠を別記する。BL33のCurrentELはEL番号の証拠であり、CurrentEL単独でSecurity Stateを観測したことにはならない。TA-A/BはSecure user TA、OP-TEE Coreとは実行ELが異なるため、「Secure側」の停止点がCoreかTAかも指定する。

コード：[tests/validation/main.c：void standalone_main](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/main.c#L284)、[runtime/arch.c：uint64_t read_current_el](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/runtime/arch.c#L6)。公式の境界はO1・O3・O10・O11。

### 現行コードが実施するサービス試験

**説明すること：Normalが要求を送り、SecureのTAが応答し、Normalがその結果を検査して次の処理・Closeまで進む。** 本項目とT-02は同じ子TEE-01を継承するが、要求表の観点は異なる。

### reportで見る場所

|検索する項目|OK条件|意味|
|---|---|---|
|`TEE-01 PASS`|`selected=True`、`phase=DONE`|子試験を実行した|
|`first-echo`、`second-echo`|各expected=actual=`1`、matched=True|各sessionで通信成功＋TEE_SUCCESS＋value=0x45a7|
|`unknown-command-rejected`|expected=actual=`1`|未知commandは通信正常でBAD_PARAMETERS|
|REVIEW `TZ-SYS-01-V01`|PASS、`functional-status`=0|同じTEE-01から引き継いだ|
|RAM `REPLY echo-first` / `echo-second`|transport0、result0、value0x45a7|boolの中身を確認|
|RAM `REPLY invalid-command`|transport0、result0xffff0006|意図した拒否|
|RAM `REPLY close`、全体cleanup|Close成功、cleanup_errors0|応答後の後始末も成立|

### 詳細シーケンス

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    loop 同じTA-Aへ2セッション
        N->>F: OPEN_SESSION
        F->>O: Secureへ切替
        O->>A: OpenSession、個別contextを確保
        A-->>O: 成功
        O-->>F: session ID
        F-->>N: Open結果
    end
    loop firstとsecondの各セッション
        N->>F: ECHO 0x45a7
        F->>O: INVOKE_COMMAND
        O->>A: command 0、VALUE_INOUT
        A->>A: 値を変更せずTEE_SUCCESS
        A-->>O: 成功と元の値
        O-->>F: 結果
        F-->>N: 復帰
    end
    N->>F: UINT32_MAXの未知command
    F->>O: dispatch
    O->>A: 未知command
    A-->>O: BAD_PARAMETERS
    O-->>F: 拒否結果
    F-->>N: 復帰
    N->>N: 3条件を比較してTEE-01を保存
    Note over N,A: 共通経路で両sessionをClose
    Note over B: 未使用。2sessionはAとBではない
```

### コードで裏付ける

- 呼ぶ操作と3つの判定：[tests/validation/functional.c：b2_run_service](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L200)。
- 実際に値とcommandをmessageへ入れる：[normal_world/client/client.c：tee_invoke](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/client.c#L177)。
- TAでECHOが成功し、未知commandがBAD_PARAMETERSになる：[secure_world/ta/ta.c：TA_InvokeCommandEntryPoint](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/ta.c#L142)。
- 親へ継承：[tests/validation/review.c：b2_run_review](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L351)の`service[]={0U}`、`inherit`。

**注意：ECHO本体は値を計算し直さずそのままにする。** 値一致だけより、成功result、不正commandの拒否、session状態試験を合わせて説明する。「ECHOだけで全メモリ保護を証明した」とは言わない。公式の通信方式はO1〜O4、ECHO値・3条件は自作試験仕様。

<a id="t-02"></a>

## T-02：Secure呼出し・復帰（TZ-SYS-02-V01）

|要求表の欄|内容|
|---|---|
|検証観点|World間通信|
|検証方法|Normalから既知の入力でSecureサービスを呼び、出力と復帰先を確認する。|
|合格判定基準|想定した経路で往復し、出力・戻り先が一致する。TA実行確認とは分けて記録する。|
|reportの親test_id|`TZ-SYS-02-V01`|
|B2_REVIEW_PARENT|`2`。T番号とは区別|

### 要求表との対応

現行の親`TZ-SYS-02-V01`はTEE-01の成功を継承し、SMC後にNormalの比較・後続Invoke・Closeへ進むことを確認する。**復帰先PCを期待アドレスと直接比較して保存するcheckはない。** 復帰を確認する根拠は、`smc_call`から戻り後続コードが実行された制御フローと、応答・後始末の結果である。要求表で「戻り先一致」を直接観測必須とするなら、指定復帰点の実行証拠を追加する。

### reportで見る場所

|検索する項目|OK条件|説明|
|---|---|---|
|REVIEW `TZ-SYS-02-V01`|PASS、DONE、functional-status=0|TEE-01結果を継承|
|子`TEE-01`|PASS、selected=True、DONE|T-01と共通の実行|
|`first-echo` / `second-echo`|1 / 1|各sessionでtransport0/result0/value0x45a7|
|`unknown-command-rejected`|1|通信正常、BAD_PARAMETERS|
|RAM `REPLY echo-first`等|返値と比較行がある|SMC後のNormalコードが実行された|
|全体cleanup_errors、Close結果|0、Close成功|後続処理まで到達|

### 呼出しと復帰の詳細シーケンス

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    Note over N,A: sessionをOpen済み
    N->>N: tee_invokeがSHMへcommand、session、VALUE_INOUTを作成
    N->>F: transport_callからsmc_call、CALL_WITH_ARG
    F->>O: opteedがSecureへ切替
    O->>O: message・sessionを検査
    O->>A: ECHOをdispatch
    A-->>O: SUCCESS、入力値を維持
    O->>O: messageへresultとoriginを書込
    O-->>F: Secure処理終了
    F-->>N: SMCの後続へ制御を戻す
    N->>N: transportとmessage結果を別々に読む
    N->>N: ECHO比較、次のInvoke、Closeへ進む
    Note over B: この子では未使用
```

### コードと公式根拠

- messageを作る：[normal_world/client/client.c：struct tee_reply tee_invoke](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/client.c#L177)
- SMCと戻ったmessageを読む：[normal_world/client/client.c：static struct tee_reply transport_call](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/client.c#L71)
- CPUのSMC命令とCへのreturn：[runtime/arch.c：struct smc_result smc_call](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/runtime/arch.c#L24)
- Normalの後続比較とClose：[tests/validation/functional.c：void b2_run_service](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L200)
- 親への引継ぎ：[tests/validation/review.c：void b2_run_review](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L351)

公式O2・O3・O4・O8。TA処理そのものの確認はT-14にも関連付け、T-02では通信の往復と復帰後のNormal継続を説明する。二つの観点に同じrunを使えるが、別実行を行ったように記録しない。

<a id="t-03"></a>

## T-03：繰返し切替と状態保持（TZ-SYS-02-V02）

|要求表の欄|内容|
|---|---|
|検証観点|切替時の状態保持|
|検証方法|両側の状態値を用意し、規定回数切り替えて前後を比較する。|
|合格判定基準|全回正常復帰し、保持すべき状態が一致する。回数と対象register/dataは詳細レビューで確定。|
|reportの親test_id|`TZ-SYS-02-V02`|
|B2_REVIEW_PARENT|`3`。T番号とは区別|

### 要求表との対応

要求表で未確定としていた回数は、現行実装では256回。各TA sessionのcounterは128増分。保持対象は以下の16checkに限定する。これを今回の採用範囲とするかレビューで確定し、全レジスタ保持と拡張しない。

**説明すること：A/Bへ交互に呼んでも、戻り値、session内counter、指定したNormal側状態が保たれる。** 実装上の親番号3 `TZ-SYS-02-V02`。子TEE-01のPASSの言い換えではなく、追加で実行する。

### reportで見る場所：REVIEW → TZ-SYS-02-V02

|check名|OK値・条件|
|---|---|
|`iterations`|256（0x100）|
|`per-session-counter-steps`|128（0x80）|
|`input-seed`|0x59e1a273。ただしこれは入力値の記録で、それ単体は保持の証明にならない|
|`x19-x29-SP-errors`|0|
|`SCTLR_EL1` / `TTBR0_EL1` / `TCR_EL1` / `DAIF`|expectedとactualが一致|
|`A-secret` / `B-secret`|0x6d3a4719 / 0xb5208c14|
|`last-ECHO`|最終入力`seed ^ (255 * 0x10201)`と一致|
|`last-counter`|128|
|`last-Normal-state`|`seed ^ 255`と一致|
|`ECHO-transport-result` / `counter-transport-result`|0（通信0とresult0を連結）|
|`TA-origins`|0x0000000400000004（ECHO/counterともorigin4）|

この16checkの一致、親PASS、DONE、親cleanup_errors0を見る。各回の個別checkを256行保存するのではなく、ループ内で逐次比較して完了数・最終結果・集約値を残す。

### 詳細シーケンス

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    Note over N,B: 共通経路でA/Bを各1session Open
    N->>N: EL1設定とNormal状態を保存
    loop iが0から255
        N->>N: inputと期待counterを計算、wrapperでGPRを設定
        N->>F: 偶数はA、奇数はBへECHO
        F->>O: Secureへ切替
        alt 偶数
            O->>A: ECHO
            A-->>O: 成功、同じ値
        else 奇数
            O->>B: ECHO
            B-->>O: 成功、同じ値
        end
        O-->>F: ECHO応答
        F-->>N: 復帰、wrapperで保持対象を比較
        Note over N,B: 同じsessionへSESSION_COUNTERを共通経路でInvoke
        N->>N: ECHO値、counter、Normal状態、GPR差分を比較
    end
    Note over N,B: A/BのSECRET_CHECKを共通経路でInvoke
    N->>N: 設定前後と最終結果を比較して16check保存
    Note over N,B: A/BをClose、親結果を保存
```

### コードと公式根拠

- 反復・入力・期待値・16check：[tests/validation/review.c：run_state_retention](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L118)。
- GPRへ19〜28を入れ、x29にSPを保持し、`tee_invoke`後に比較：[tests/validation/state_call.S](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/state_call.S#L1)。
- session別counter：[secure_world/ta/ta.c：TA_InvokeCommandEntryPoint](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/ta.c#L142)の`TA_COMMAND_SESSION_COUNTER`。
- 秘密自体を返さず検査tagを返す：[secure_world/ta/security.c：security_invoke](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/security.c#L169)の`TA_COMMAND_SECRET_CHECK`。

O6（AAPCS64）にcallee-savedの根拠。**C関数呼出しを含む往復の指定GPR/SP整合性を検査するもので、SMC時点の全CPU contextの網羅検査ではない。** x0〜x18、NZCV、SIMDは保持対象に含めない。EL1設定の比較も指定4項目のみ。

<a id="t-04"></a>

## T-04：Secureメモリの読書き禁止（TZ-SYS-03-V01）

|要求表の欄|内容|
|---|---|
|検証観点|物理メモリの保護|
|検証方法|専用領域でSecure正常対照と同一PAへのNormal read/writeを比較。変換・XMPU・例外を照合する。|
|合格判定基準|NS stage1未写像だけによらない拒否を確認し、書込み後も対象非改変、正常系が継続する。|
|reportの親test_id|`TZ-SYS-03-V01`|
|B2_REVIEW_PARENT|`4`。T番号とは区別|

**説明すること：Normal側のStage1変換は同じ物理ページまで成立したうえで、外部の保護機構がread/writeを拒否し、Secureデータが変化せず、例外後にNormalとTAが続行できる。**

### まずreportを選ぶ

- review一式の親`TZ-SYS-03-V01`はBLOCKEDを記録するだけ。そこをPASSにする試験経路ではない。
- `PROFILE=physical B2_TEST_INDEX=15`で実probeを行った**別runのreport**を使う。子`TZ-SYS-03`を見る。
- 公開commitには手元で試したSError復帰変更がない。以下は**公開コードが厳密に要求する同期external abortのPASS条件**。SErrorでpanic・途中停止したrunはこの完全PASS条件を満たさない。

### reportで見る場所

|項目|OK値・条件|
|---|---|
|`TZ-SYS-03`|PASS、selected=True、phase=DONE|
|子`Secure-allocation-bytes`|4096が1件|
|子`precise-external-abort`|3が4件|
|子`Secure-page-check`|0x50485953が4件|
|子`post-probe-TA-ECHO` / `Normal-control`|各0xa35b|
|`PHYSICAL EVIDENCE` `state` / `entry_el`|2 / 1|
|`page_pa` / `page_bytes`|リンク済み許可予約内、4KiB整列 / 4096|
|`el2_known` / `hcr`|1 / Stage2 enable bit0が0|
|`sctlr` / `probe_mair`|MMUとD-cache有効、probe用MAIR slot2が0x44（Normal Non-cacheable）|
|`attempts`|4件すべて下表を満たす|
|`cleanup_errors`|0。alias除去・PTA release・Closeに失敗なし|
|`xmpu_attribution`|`NOT_ESTABLISHED_BY_ESR_ALONE`は正常な留保。設定・違反の別証拠で補う|

|attemptの順番|offset / write|completed|classification_name|追加条件|
|---|---|---|---|---|
|0|0 / 0（read）|2|PRECISE_EXTERNAL_ABORT|FAR=試行VA、ELR=read probe命令、WnR0|
|1|0 / 1（write）|2|PRECISE_EXTERNAL_ABORT|FAR=試行VA、ELR=write probe命令、WnR1|
|2|4088 / 0（read）|2|PRECISE_EXTERNAL_ABORT|末尾8B位置、readの条件|
|3|4088 / 1（write）|2|PRECISE_EXTERNAL_ABORT|末尾8B位置、writeの条件|

全attemptで`classification=3`、ESR.EC=0x25、DFSC=0x10、FnV/S1PTW=0。`par_read` / `par_write`はfault bit0が0で、変換PA＋offsetが記録した`pa`と一致する。PAR値を0と比較するのではない。

`read_value_valid=false`はabortで正常に読み終えていない場合に妥当。値0が読めたことを保護根拠にしない。分類1（translation）や2（permission）はMMU段階の拒否なので物理保護PASSにしない。

### 詳細シーケンス

TA-Bは使わない。5登場人物の範囲を保ち、この図だけ未使用TA-Bを外して外部保護経路を加える。

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant X as XMPUとDDR経路
    N->>N: 実行許可、EL1、EL2既知、Stage2無効等を検査
    N->>F: physical PTAへOpen、PREPAREとSIZE
    F->>O: Core内PTAへ
    O->>X: 専用4KiBに既知pattern、cache cleanとinvalidate
    O->>O: 自身でもpatternを検査
    O-->>F: PAと4096B
    F-->>N: 復帰
    N->>N: alias VAを同じPAへmap、ATとPARでR/W変換を確認
    N->>N: 許可されたNormal SHMの読書き対照
    loop 先頭read、先頭write、末尾read、末尾write
        N->>X: NSのloadまたはstore
        X-->>N: 期待する同期Data Abort
        N->>N: EL1 handlerでFAR、ELR、方向等を照合して専用復帰先へ
        N->>F: PTAへVERIFY
        F->>O: 検証要求
        O->>X: cache invalidate後、ページ全体を再読
        O->>O: 全512個のu64がpatternと一致するか
        O-->>F: CHECK_TAG
        F-->>N: 復帰、分類と非改変を保存
    end
    Note over N,A: 共通経路でTA-AのECHO、Normal control再確認
    N->>N: aliasとTLBを除去、MAIR復元
    Note over N,O: RELEASEでページをzeroし、PTAとTAのsessionをClose
```

直接load/store部分にはSMCもTA-Aも介在しない。Secure側の準備と非改変確認にはSMCでPTAを呼ぶ。ハードウェア拒否→EL1復帰という図はこのコードが期待する経路であり、XMPU拒否が常にこの例外形になるという保証ではない。

### コードと公式根拠

|処理|コード|
|---|---|
|全体・同一PA検証・4回の試行・非改変・後始末|[tests/validation/physical_memory.c：b2_run_physical_memory](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/physical_memory.c#L118)|
|VA aliasとMAIR設定|同ファイル`install_alias` / `translation` / `translated_to`|
|実load/storeと復帰ラベル|[tests/validation/physical_probe.S](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/physical_probe.S#L1)|
|限定された例外だけ復帰|[runtime/el1_fault.c](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/runtime/el1_fault.c#L1)、[tests/validation/physical_memory.c：b2_handle_phys_fault](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/physical_memory.c#L42)|
|例外分類|[tests/validation/physical_policy.c：b2_classify_physical_abort](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/physical_policy.c#L3)|
|Secureページ準備・cache処理・全体照合・release|[secure_world/pta/physical_probe.c：prepare_page](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/pta/physical_probe.c#L57) / `verify_page` / `release_page`|
|PC側の厳密条件|[scripts/decode_b2_physical.py：validate_physical_pass](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/scripts/decode_b2_physical.py#L26)、[scripts/decode_b2_results.py](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/scripts/decode_b2_results.py#L1)|

公式O11（Arm）でAT/PAR・ESR/FAR/ELR・page属性、O12（AMD）でXMPUの設定・master/route・応答・違反情報を確認する。Cの`register_phys_mem`はOP-TEEのmapping登録であって、XMPU設定そのものではない。

### reportに加えて残す別証拠

XMPU対象bank、region START/END/MASTER/CONFIG、CTRL/POISON/LOCK、アクセス経路、違反情報の読戻しと採取時点。実行物hash・run IDと同じrunに結び付ける。XMPU帰属をESRだけで断定しない。

**手元で確認した「拒否されてSError/TF-A panicで止まる」は、拒否の観測として説明できるが、4アクセス・非改変・後続処理・cleanupの完全合格とは分ける。** 全DDR・全master・DMA・全アクセス幅の保護を示すものではない。

<a id="t-05"></a>

## T-05：秘密領域と共有領域の使い分け（TZ-SYS-03-V02）

|要求表の欄|内容|
|---|---|
|検証観点|保護と通信の両立|
|検証方法|秘密/共有領域を分け、正常な共有通信と不正memref・境界値を比較する。|
|合格判定基準|秘密領域を共有扱いせず、正常通信成功。不正入力の拒否層とerror/originが一致し、bufferが不変。|
|reportの親test_id|`TZ-SYS-03-V02`|
|B2_REVIEW_PARENT|`5`。T番号とは区別|

### 要求表との対応

正常通信・不正memref・型・内容・境界の小試験を照合する。originまで判定する入力と、resultのみを判定する入力は後述のコードどおりに分ける。すべての拒否についてoriginを自動比較していると説明しない。物理PAへの直接アクセス保護はT-04。

**説明すること：正しい共有入力は受け付け、不正な範囲・長さ・型・内容を想定した層で拒否し、bufferと正常経路が保たれる。** 実装上の親番号5 `TZ-SYS-03-V02`。物理PAへNormalから直接loadする試験ではない。

### reportで見る場所

親PASSに加え、次の3子すべてPASS、記載checkがexpected=actual=1。

|子試験ID|check名|含まれる操作と期待する拒否|
|---|---|---|
|`TEE-SHM-01`|`SHM-reject-and-nonmodification`|正常16B成功、15BはTAがBAD_PARAMETERS/origin4、SHM越境・Secure領域・uint64 wrapはCoreがBAD_PARAMETERS/origin3。通常clientのSecure領域指定もlocal拒否。正常再試行と非改変|
|`TEE-BOUNDARY-01`|`shared_boundaries-predicates`|SHM末尾の16B成功、1Bはみ出し・0B拒否、messageとの重複をclient拒否、正常再試行、16B非改変|
|`TEE-PARAM-01`|`parameter_validation-predicates`|Coreにとって有効なVALUE_INPUTをTA期待INOUTと違えてTA拒否、buffer先頭改変でSECURITY/origin4、拒否後非改変、内容を戻すと成功、Bへの正常呼出し継続|

RAMの検索文字：`SECURITY_REPLY valid` / `short` / `overflow` / `secure` / `wrap`、`REPLY last-valid` / `one-byte-outside` / `zero-size`、`REPLY wrong-TA-type` / `malformed-buffer`。

### 詳細シーケンス

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    Note over N,B: 各子が必要なsessionをOpen
    N->>N: SHMへ0x50から始まる16Bを用意
    N->>F: BUFFER_CHECKと範囲
    F->>O: messageを渡す
    O->>O: memref範囲と属性を検査
    O->>A: 正常なmemrefを渡す
    A->>A: private snapshotへコピーし、型・16B長・内容を検査
    A-->>O: 成功
    O-->>F: 結果
    F-->>N: 復帰
    loop 越境、Secure領域、wrap等
        N->>F: unchecked probeで不正入力
        F->>O: Coreへ届ける
        O-->>F: 範囲不正を拒否、TAへ進めない
        F-->>N: BAD_PARAMETERS、origin3
    end
    Note over N,A: 15B、異なる型、不正内容はCore通過後にTAが拒否
    N->>N: buffer非改変と正常再試行を確認
    Note over N,B: PARAM子ではBのECHOも確認、sessionをClose
```

### コードと公式根拠

|知りたい処理|コード|
|---|---|
|5種類のbuffer入力|[tests/validation/security_tests.c：test_shared_memory](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L70)|
|末尾・1B越境・0B・message重複|[tests/validation/boundary_tests.c：b2_run_shared_boundaries](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/boundary_tests.c#L70)|
|型と内容の不正|[tests/validation/extended_tests.c：b2_run_parameter_validation](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/extended_tests.c#L67)|
|通常clientの範囲拒否と、敢えてCoreへ送るprobeの違い|[normal_world/client/client.c：tee_invoke_buffer](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/client.c#L240)、[normal_world/client/client.c：tee_probe_buffer_unchecked](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/client.c#L224)|
|TAがコピーして内容を検査|[secure_world/ta/security.c：security_invoke](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/security.c#L169)のBUFFER_CHECK分岐|

O1・O2・O4がSHM/message検査の根拠。16B・0x50+iという内容ルールは独自仕様。`unchecked`は攻撃試験のためclientの通常範囲検査を迂回する補助で、製品用APIの推奨ではない。境界子では全拒否のoriginを固定していないので、コードが要求していない条件まで「自動検査済み」としない。

<a id="t-06"></a>

## T-06：Secure周辺機器の保護（TZ-SYS-04-V01）

|要求表の欄|内容|
|---|---|
|検証観点|周辺機器のアクセス制御|
|検証方法|試験用周辺機器とXPPU等の経路を特定し、Secure/Normalの同じ操作を比較する。|
|合格判定基準|Secure許可操作が成功、禁止したNormal操作が拒否され機器状態が意図せず変わらない。対象機器・操作を要確定。|
|reportの親test_id|`TZ-SYS-04-V01`|
|B2_REVIEW_PARENT|`6`。T番号とは区別|

### 現行reportで見る場所と扱い

`REVIEW`内の`test_id: TZ-SYS-04-V01`を見る。review一式、または当該親を選択したrunでは**`status: SKIP`、`phase: DONE`**が現行コードの想定。これは試験に合格した意味ではない。未選択ならNOT_RUNのままなので、実施済みと数えない。

- 未実装の部分：対象周辺機器を操作するbackendがない。
- 要求表の合格基準に対する判定：**未実施／保留。PASSの実行証拠は生成されない。**
- T-04のDDRページ保護は周辺機器保護の代替ではない。

reasonは`OUT-OF-SCOPE: peripheral-DMA-debug-authenticated-boot-persistent-storage`を固定長フィールドへ保存する。JSONの`reason`は長い文字列の末尾が切れる場合があるため、`OUT-OF-SCOPE:`と正式ID・statusで識別する。

### 現行コードが実際に行う流れ

実装のない機能を動かす図は描かない。実際には以下の**結果記録だけ**を行い、要求表にある実操作は行わない。

|順序|担当|実際の処理|
|---|---|---|
|1|BL33|b2_run_reviewで対象親が選択されているか調べる|
|2|BL33|対象外親IDの分岐でfinishを呼ぶ|
|3|BL33|SKIP・reason・DONEを保存する|
|4|TF-A／OP-TEE／TA-A／TA-B|この親の要求操作は呼ばれない。他の選択試験で動くこととは区別|

### コードと必要な追加証拠

コード：[tests/validation/review.c：void b2_run_review](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L351)、[tests/validation/review.c：static void finish](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L79)。

実施するために確定する内容：周辺機器所有者、対象register/操作、XPPUと関連経路、Secure/Normal対照、状態非改変の観測点。

公式根拠：O12のAMD UG1085/UG1087でXPPU・対象周辺機器の保護設定を照合する。 ここで挙げる追加条件はレビューの不足事項で、現行の実行済みシーケンスではない。

<a id="t-07"></a>

## T-07：DMAからのSecureメモリ保護（TZ-SYS-05-V01）

|要求表の欄|内容|
|---|---|
|検証観点|CPU以外のmasterに対する保護|
|検証方法|DMA master ID・属性・保護経路を確認し、許可領域と禁止領域の転送を比較する。|
|合格判定基準|許可転送はデータ一致、禁止転送は拒否され保護領域非改変。CPUの拒否をDMAの証拠に流用しない。|
|reportの親test_id|`TZ-SYS-05-V01`|
|B2_REVIEW_PARENT|`7`。T番号とは区別|

### 現行reportで見る場所と扱い

`REVIEW`内の`test_id: TZ-SYS-05-V01`を見る。review一式、または当該親を選択したrunでは**`status: SKIP`、`phase: DONE`**が現行コードの想定。これは試験に合格した意味ではない。未選択ならNOT_RUNのままなので、実施済みと数えない。

- 未実装の部分：DMA転送backendがない。
- 要求表の合格基準に対する判定：**未実施／保留。PASSの実行証拠は生成されない。**
- A53のload/store拒否はDMA masterの拒否を証明しない。

reasonは`OUT-OF-SCOPE: peripheral-DMA-debug-authenticated-boot-persistent-storage`を固定長フィールドへ保存する。JSONの`reason`は長い文字列の末尾が切れる場合があるため、`OUT-OF-SCOPE:`と正式ID・statusで識別する。

### 現行コードが実際に行う流れ

実装のない機能を動かす図は描かない。実際には以下の**結果記録だけ**を行い、要求表にある実操作は行わない。

|順序|担当|実際の処理|
|---|---|---|
|1|BL33|b2_run_reviewで対象親が選択されているか調べる|
|2|BL33|対象外親IDの分岐でfinishを呼ぶ|
|3|BL33|SKIP・reason・DONEを保存する|
|4|TF-A／OP-TEE／TA-A／TA-B|この親の要求操作は呼ばれない。他の選択試験で動くこととは区別|

### コードと必要な追加証拠

コード：[tests/validation/review.c：void b2_run_review](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L351)、[tests/validation/review.c：static void finish](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L79)。

実施するために確定する内容：DMA engine、master ID・Security属性、保護bank/route、許可・禁止buffer、転送完了/拒否/非改変の観測点。

公式根拠：O12のAMD UG1085/UG1087でDMA masterとメモリ保護経路を照合する。 ここで挙げる追加条件はレビューの不足事項で、現行の実行済みシーケンスではない。

<a id="t-08"></a>

## T-08：Secure割込みの配送（TZ-SYS-06-V01）

|要求表の欄|内容|
|---|---|
|検証観点|割込みのrouting|
|検証方法|指定した割込みを発生し、GIC設定、受信EL/World、handler実行と復帰を確認する。|
|合格判定基準|設計したhandlerで規定回数処理され、誤ったWorldへ配送されず正常復帰する。発生源・routingを要確定。|
|reportの親test_id|`TZ-SYS-06-V01`|
|B2_REVIEW_PARENT|`8`。T番号とは区別|

### 現行reportで見る場所と扱い

`REVIEW`内の`test_id: TZ-SYS-06-V01`を見る。review一式、または当該親を選択したrunでは**`status: BLOCKED`、`phase: DONE`**が現行コードの想定。これは試験に合格した意味ではない。未選択ならNOT_RUNのままなので、実施済みと数えない。

- 未実装の部分：B2_SECURE_SGI=0でSGI PTAが選択Coreに含まれず、ZCU用配送構成もない。
- 要求表の合格基準に対する判定：**未実施／保留。PASSの実行証拠は生成されない。**
- 実機の設定値を読むだけではhandler実行試験が追加されたことにはならない。

検索するreasonは`Secure-SGI-not-in-selected-Core; ZCU-SGI-group-route-review-required`または`Secure-SGI-profile-not-enabled`。T-08/T-19の選択の組合せでreasonが分かれるため、文字列が一種類だけだと思わない。

### 現行コードが実際に行う流れ

実装のない機能を動かす図は描かない。実際には以下の**結果記録だけ**を行い、要求表にある実操作は行わない。

|順序|担当|実際の処理|
|---|---|---|
|1|BL33|b2_run_reviewで対象親が選択されているか調べる|
|2|BL33|run_secure_sgiのB2_SECURE_SGI != 1分岐へ入る|
|3|BL33|finishでBLOCKED・reason・DONEを保存し、PTA Openへ進まない|
|4|TF-A／OP-TEE／TA-A／TA-B|この親の要求操作は呼ばれない。他の選択試験で動くこととは区別|

### コードと必要な追加証拠

コード：[tests/validation/review.c：static void run_secure_sgi](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L268)、[tests/validation/review.c：static void finish](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L79)。

実施するために確定する内容：割込み発生源、GIC group・INTID・route、受信EL/World、handlerの回数と復帰、誤配送の観測。

公式根拠：O1のInterrupt handling、O12のGIC400レジスタ群。 ここで挙げる追加条件はレビューの不足事項で、現行の実行済みシーケンスではない。

<a id="t-09"></a>

## T-09：JTAGデバッグ制限（TZ-SYS-07-V01）

|要求表の欄|内容|
|---|---|
|検証観点|デバッグ権限|
|検証方法|承認したデバッグ設定で接続・停止・register/memoryアクセスを許可/禁止状態別に確認する。|
|合格判定基準|許可状態は利用可能、禁止状態は規定操作を拒否。設定の可逆性・対象範囲を事前確定する。|
|reportの親test_id|`TZ-SYS-07-V01`|
|B2_REVIEW_PARENT|`9`。T番号とは区別|

### 現行reportで見る場所と扱い

`REVIEW`内の`test_id: TZ-SYS-07-V01`を見る。review一式、または当該親を選択したrunでは**`status: SKIP`、`phase: DONE`**が現行コードの想定。これは試験に合格した意味ではない。未選択ならNOT_RUNのままなので、実施済みと数えない。

- 未実装の部分：debug認証と許可/禁止状態を切り替える試験構成がない。
- 要求表の合格基準に対する判定：**未実施／保留。PASSの実行証拠は生成されない。**
- 今JTAGで接続・RAM回収できることは、禁止状態の制御を検証した証拠ではない。

reasonは`OUT-OF-SCOPE: peripheral-DMA-debug-authenticated-boot-persistent-storage`を固定長フィールドへ保存する。JSONの`reason`は長い文字列の末尾が切れる場合があるため、`OUT-OF-SCOPE:`と正式ID・statusで識別する。

### 現行コードが実際に行う流れ

実装のない機能を動かす図は描かない。実際には以下の**結果記録だけ**を行い、要求表にある実操作は行わない。

|順序|担当|実際の処理|
|---|---|---|
|1|BL33|b2_run_reviewで対象親が選択されているか調べる|
|2|BL33|対象外親IDの分岐でfinishを呼ぶ|
|3|BL33|SKIP・reason・DONEを保存する|
|4|TF-A／OP-TEE／TA-A／TA-B|この親の要求操作は呼ばれない。他の選択試験で動くこととは区別|

### コードと必要な追加証拠

コード：[tests/validation/review.c：void b2_run_review](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L351)、[tests/validation/review.c：static void finish](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L79)。

実施するために確定する内容：承認された可逆設定、許可/禁止する操作、APとCPU debug経路、Secure/Normal対象範囲、復旧手順。

公式根拠：O12のデバイスdebug/security設定を使用版で確認する。 ここで挙げる追加条件はレビューの不足事項で、現行の実行済みシーケンスではない。

<a id="t-10"></a>

## T-10：サービスの認証・認可（EXT-AUTH-V01）

|要求表の欄|内容|
|---|---|
|検証観点|呼出し元の識別と権限|
|検証方法|TA identity、偽装、PUBLIC、認可状態、Normal要求者認証を個別に比較する。|
|合格判定基準|各小項目の許可/拒否が一致。本物のNormal要求者認証が未実装なら、固定token smoke成功だけで親項目をPASSにしない。|
|reportの親test_id|`EXT-AUTH-V01`|
|B2_REVIEW_PARENT|`10`。T番号とは区別|

### 要求表に対する判定：限定小試験のPASS、要求全体は保留

現行の`EXT-AUTH-V01 PASS`は、固定試験tokenによるsession状態とTA identityの許可リストを検査した結果。**本物のNormal要求者認証は実装していないため、提示されたT-10全体の合格条件は満たしたと判断しない。** 以下をTA identity／固定tokenの実施済み範囲として示し、Normal要求者認証を未実施小項目として残す。コードの親名と要求の親名が同じでも範囲は同じではない。

**説明すること：固定試験tokenで与えた許可は、そのsessionだけに限定され、取消・Close・再Openで引き継がれない。別途、TA-AからTA-Bへの呼出元identityを検査する。** 実装上の親番号10 `EXT-AUTH-V01`。

### reportで見る場所

|場所・check名|OK値・条件|
|---|---|
|子`TEE-AUTH-TA` / `TA-identity-policy`|PASS、expected=actual=1|
|親`before-token` / `wrong-token`|各0xffff0001|
|親`authorized-read`|TA-Aの試験秘密値0x59436e|
|親`other-session-denied`|0xffff0001|
|親`revoke` / `after-revoke`|各0xffff0001。誤token認証操作が許可を取り消す|
|親`closed-client-state`|4（TEE_CLIENT_BAD_STATE）。これはCoreへ送る前のclient拒否|
|親`reopen-without-token`|0xffff0001|
|親`TA-identity-subtest`|0（子のPASS enum）|

親の9check一致、親PASS、cleanup_errors0。拒否resultだけでなく、`run_token_policy`は該当操作のtransport0・origin4も判定に含める。認証成功`allow`等は別check名にせずbool式に含まれる。

### シーケンスA：session token

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    Note over N,A: TA-Aへfirstとotherの2sessionをOpen
    loop firstの許可前READ、誤token
        N->>F: READまたはAUTHENTICATE
        F->>O: Invoke
        O->>A: firstのsession context
        A-->>O: ACCESS_DENIED
        O-->>F: 結果
        F-->>N: 復帰
    end
    Note over N,A: 共通経路でfirstを正しいtokenで認証、READ成功
    Note over N,A: otherのREADは拒否、firstの誤tokenで取消、READも拒否
    Note over N,A: firstをClose、閉handleのclient拒否を確認、再Open
    N->>F: 新しいfirstでREAD
    F->>O: Invoke
    O->>A: 初期化されたcontextは未認証
    A-->>O: ACCESS_DENIED
    O-->>F: 結果
    F-->>N: 復帰、比較・Close
    Note over B: token検査では未使用
```

### シーケンスB：TA identity

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    N->>F: 偽のtrusted-app loginでOpen
    F->>O: Open message
    O-->>F: BAD_PARAMETERS、origin3
    F-->>N: 拒否
    Note over N,B: PUBLIC sessionでBへ偽ID値を渡してTRUSTED_READしても拒否
    N->>F: AへTRUSTED_SELFTEST
    F->>O: Invoke
    O->>A: 自己試験を開始
    A->>O: TEE_OpenTASessionでBへ
    O->>B: Coreが管理するAのidentityでOpen
    A->>O: TEE_InvokeTACommand TRUSTED_READ
    O->>B: Invoke
    B->>O: gpd.client.identityを取得
    O-->>B: TRUSTED_APPとAのUUID
    B-->>O: 許可したAへ試験秘密値
    O-->>A: 結果
    A->>A: Bの値を検査し、Normalへ出す値を0へ
    A->>O: B sessionをClose
    A-->>O: 自己試験の成否
    O-->>F: 結果
    F-->>N: 復帰、Bの継続も確認
```

### コードと公式根拠

- token検査全体：[tests/validation/review.c：run_token_policy](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L191)。
- `authenticated`をsession contextへ保存し、正しいtokenか判定：[secure_world/ta/ta.c：TA_OpenSessionEntryPoint](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/ta.c#L121)、[secure_world/ta/ta.c：TA_InvokeCommandEntryPoint](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/ta.c#L142)。
- identity子：[tests/validation/security_tests.c：test_identity](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L42)。
- A→BのOpen/Invoke/Closeと値の検査：[secure_world/ta/security.c：trusted_service_test](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/security.c#L69)。
- Bのallowlist：[secure_world/ta/security.c：trusted_read](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/security.c#L45)。**パラメータの自己申告IDを信用せず、Coreのsession propertyを読む。**

公式O5の`TEE_GetPropertyAsIdentity`、`TEE_OpenTASession`、`TEE_InvokeTACommand`。Core側login検査はO4。固定token0x7139a2とAのUUID許可は独自ポリシーで、本物の利用者・Normalプログラム本人の認証を実装したことにはならない。

<a id="t-11"></a>

## T-11：起動イメージ署名検証（EXT-BOOT-V01）

|要求表の欄|内容|
|---|---|
|検証観点|起動の真正性|
|検証方法|SoC認証bootで正規・改変・未信頼イメージの起動を比較する。|
|合格判定基準|正規のみ起動し、不正イメージを実行前拒否。JTAGロードやhost署名比較は代替にしない。|
|reportの親test_id|`EXT-BOOT-V01`|
|B2_REVIEW_PARENT|`15`。T番号とは区別|

### 現行reportで見る場所と扱い

`REVIEW`内の`test_id: EXT-BOOT-V01`を見る。review一式、または当該親を選択したrunでは**`status: SKIP`、`phase: DONE`**が現行コードの想定。これは試験に合格した意味ではない。未選択ならNOT_RUNのままなので、実施済みと数えない。

- 未実装の部分：SoC認証bootチェーン・正規/改変/未信頼イメージによる起動比較を構成していない。
- 要求表の合格基準に対する判定：**未実施／保留。PASSの実行証拠は生成されない。**
- T-16のTA runtime署名検査やhost署名比較、JTAG dow成功はSoC認証bootの代替ではない。

reasonは`OUT-OF-SCOPE: peripheral-DMA-debug-authenticated-boot-persistent-storage`を固定長フィールドへ保存する。JSONの`reason`は長い文字列の末尾が切れる場合があるため、`OUT-OF-SCOPE:`と正式ID・statusで識別する。

### 現行コードが実際に行う流れ

実装のない機能を動かす図は描かない。実際には以下の**結果記録だけ**を行い、要求表にある実操作は行わない。

|順序|担当|実際の処理|
|---|---|---|
|1|BL33|b2_run_reviewで対象親が選択されているか調べる|
|2|BL33|対象外親IDの分岐でfinishを呼ぶ|
|3|BL33|SKIP・reason・DONEを保存する|
|4|TF-A／OP-TEE／TA-A／TA-B|この親の要求操作は呼ばれない。他の選択試験で動くこととは区別|

### コードと必要な追加証拠

コード：[tests/validation/review.c：void b2_run_review](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L351)、[tests/validation/review.c：static void finish](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L79)。

実施するために確定する内容：BootROMからの認証対象と信頼起点、対象image、鍵設定、正規起動記録、不正イメージが実行前拒否された記録。

公式根拠：O12のAMD UG1085にあるboot/securityの説明。TAローダ仕様O9とは区別する。 ここで挙げる追加条件はレビューの不足事項で、現行の実行済みシーケンスではない。

<a id="t-12"></a>

## T-12：鍵を外に出さない暗号サービス（EXT-KEY-V01）

|要求表の欄|内容|
|---|---|
|検証観点|鍵の秘密保持と暗号正常動作|
|検証方法|Secure鍵生成/利用、usage制限、独自export拒否、Core属性取得拒否、生メモリ抽出、既知解を別小項目で検査する。|
|合格判定基準|各小項目の期待値に一致。API拒否と生メモリ抽出耐性を分け、未実施の抽出試験を含めた親項目のPASSを推定しない。|
|reportの親test_id|`EXT-KEY-V01`|
|B2_REVIEW_PARENT|`11`。T番号とは区別|

### 要求表に対する判定：API・暗号は確認可能、生メモリ抽出は未実施

現行の`EXT-KEY-V01 PASS`を採用できるのは、生成・usage・独自export拒否・Core属性取得拒否・暗号往復/KATという範囲。**生メモリ抽出試験は含まれない。** T-04の専用patternページ拒否も、実際の鍵が全経路で抽出できないことの代替にしない。要求表T-12の未実施小項目を含む全体PASSは保留する。

**説明すること：TA内の生成鍵を許可した暗号処理に使え、持出し禁止APIが拒否し、Coreの属性取得制約も働く。** 実装上の親番号11 `EXT-KEY-V01`。

### reportで見る場所

|子・check名|OK条件|どこまで確認するか|
|---|---|---|
|`TEE-KEY-API` `key-usable-before-after`|1|禁止exportの前後で鍵が使える|
|同 `export-rejected-zero-output`|1|独自KEY_EXPORTがACCESS_DENIEDかつvalue0|
|同 `AES-roundtrip-and-KAT`|1|TA内で生成鍵の暗号・復号一致と、別の既知鍵の既知解一致|
|`TEE-KEY-ATTR-01` `key_attributes-predicates`|1|抽出可能な対照鍵で取得成功、非抽出鍵はCore属性取得拒否、出力buffer無傷、その後も鍵が使える|
|REVIEW `EXT-KEY-V01`|PASS、functional-statusが2件とも0|両子PASSを継承|

RAMで`REPLY key-before` / `export-denied` / `AES-KAT` / `key-after` / `nonextractable-attribute-test`を検索。KEY_ATTRIBUTEの戻りSUCCESSは「属性取得できた」という意味ではなく、**TA内部で拒否されるべき操作の拒否まで検査できた**という自己試験成功。

### 詳細シーケンス

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    Note over N,B: A/BをOpen。Aのinstance作成時に鍵を準備
    A->>O: AllocateTransientObject、GenerateKey、RestrictObjectUsage
    O-->>A: 非抽出AES鍵handle
    Note over N,A: 共通経路でKEY_POLICYを要求
    A->>O: objectUsage確認と鍵を使うAES操作
    O-->>A: 成功
    Note over N,A: KEY_EXPORTはAがACCESS_DENIEDと出力0を返す
    Note over N,A: CRYPTO要求を受けたAが暗号試験を開始
    A->>O: 生成鍵で暗号化・復号
    O-->>A: 結果
    A->>A: 元の平文と復号文を比較
    A->>O: 既知のゼロ鍵でAES-ECBを実行
    O-->>A: 暗号文
    A->>A: 既知解16Bと比較し資源解放
    Note over N,A: KEY_ATTRIBUTE要求で対照と非抽出鍵を比較
    A->>O: 対照鍵のsecret属性を取得
    O-->>A: 成功
    A->>O: 非抽出鍵のsecret属性を取得
    O-->>A: BAD_PARAMETERS
    A->>A: 出力の0xa5が変わらず、鍵が使えることを確認
    Note over N,B: 結果をNormalへ返し比較、sessionをClose
```

### コードと公式根拠

- 呼出しと複合判定：[tests/validation/security_tests.c：test_key_api](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L112)、[tests/validation/extended_tests.c：b2_run_key_attributes](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/extended_tests.c#L117)。
- 鍵生成と使用制限：[secure_world/ta/security.c：security_create](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/security.c#L22)。
- usage確認と使用：[secure_world/ta/security.c：key_policy_test](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/security.c#L97)。
- Core属性取得を直接検査：[secure_world/ta/security.c：key_attribute_test](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/security.c#L129)。
- 暗号・復号・既知解とcleanup：[secure_world/ta/ta.c：run_crypto_test](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/ta.c#L18)。

O5が鍵APIの規約、O7が実使用版の`TEE_USAGE_EXTRACTABLE`と`syscall_cryp_obj_get_attr`の実装根拠。非抽出属性取得の内部syscall `_utee_cryp_obj_get_attr`はOP-TEE固有で、公開APIと同じ戻り方を仮定しない。通常公開APIのpanic挙動との混同を避けてCoreの拒否を直接観測している。

自作KEY_EXPORTが拒否するだけではCore制約の証拠にならないため、属性子も見る。生RAM抽出・DMA・side channel・乱数品質の保証には広げない。AES既知解はコード内16B `66e94bd4ef8a2c3b884cfa59ca342b2e`との一致で、独立した暗号認証試験ではない。

<a id="t-13"></a>

## T-13：Secureデータ永続保存（EXT-STORE-V01）

|要求表の欄|内容|
|---|---|
|検証観点|永続性・保存データ保護|
|検証方法|保存・読出し・再起動・改変・破損・削除を試す。|
|合格判定基準|正常再起動でデータ一致、改変等を仕様どおり検出/処理。不正データを正常利用しない。rollbackは別途対象可否を決める。|
|reportの親test_id|`EXT-STORE-V01`|
|B2_REVIEW_PARENT|`16`。T番号とは区別|

### 現行reportで見る場所と扱い

`REVIEW`内の`test_id: EXT-STORE-V01`を見る。review一式、または当該親を選択したrunでは**`status: SKIP`、`phase: DONE`**が現行コードの想定。これは試験に合格した意味ではない。未選択ならNOT_RUNのままなので、実施済みと数えない。

- 未実装の部分：永続Storage/RPC backendがない。
- 要求表の合格基準に対する判定：**未実施／保留。PASSの実行証拠は生成されない。**
- TA内の一時鍵・session counter・RAMログ保存は、永続保存の耐改変・再起動保持の証拠ではない。

reasonは`OUT-OF-SCOPE: peripheral-DMA-debug-authenticated-boot-persistent-storage`を固定長フィールドへ保存する。JSONの`reason`は長い文字列の末尾が切れる場合があるため、`OUT-OF-SCOPE:`と正式ID・statusで識別する。

### 現行コードが実際に行う流れ

実装のない機能を動かす図は描かない。実際には以下の**結果記録だけ**を行い、要求表にある実操作は行わない。

|順序|担当|実際の処理|
|---|---|---|
|1|BL33|b2_run_reviewで対象親が選択されているか調べる|
|2|BL33|対象外親IDの分岐でfinishを呼ぶ|
|3|BL33|SKIP・reason・DONEを保存する|
|4|TF-A／OP-TEE／TA-A／TA-B|この親の要求操作は呼ばれない。他の選択試験で動くこととは区別|

### コードと必要な追加証拠

コード：[tests/validation/review.c：void b2_run_review](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L351)、[tests/validation/review.c：static void finish](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L79)。

実施するために確定する内容：保存先、REE/RPMB方式、再起動、改変/破損/削除時の設計、復旧、rollbackを含めるかの範囲。

公式根拠：[OP-TEE 4.5 Secure storage](https://optee.readthedocs.io/en/4.5.0/architecture/secure_storage.html)は構成を追加する際の公式参照。今回はbackendを実装・検証したとはしない。 ここで挙げる追加条件はレビューの不足事項で、現行の実行済みシーケンスではない。

<a id="t-14"></a>

## T-14：User TAの呼出し（EXT-TA-V01）

|要求表の欄|内容|
|---|---|
|検証観点|TA利用と入力/セッション管理|
|検証方法|Open/Invoke/Close、形式/型/値/command、上限/重複/閉鎖ID/再Openを検査。標準SMC疎通は補助観点として扱う。|
|合格判定基準|正常処理と後始末が成功し、不正入力・状態を所定層で拒否。ローカル拒否とCore拒否を区別。|
|reportの親test_id|`EXT-TA-V01`|
|B2_REVIEW_PARENT|`12`。T番号とは区別|

### 要求表との対応

各小試験の正常対照・拒否層・後始末を個別に示す。4sessionはBL33 clientの上限で、Core全体の上限ではない。標準SMCのVERSION応答は補助子B2-SMCCC-01であり、TA利用成功の代替にしない。

**説明すること：正常呼出しが通り、clientの状態検査とCore/TAの入力検査をそれぞれ確認できる。** 実装上の親番号12 `EXT-TA-V01`。

### reportで見る場所

|子ID|check名とOK値|具体的な条件|
|---|---|---|
|`TEE-01`|T-01・T-02の3checkが1|正常往復と未知command拒否|
|`TEE-SESSION-01`|`client-limit-and-cleanup`=1|4枠Open、5件目と同じhandleへの二重Openをclient拒否。全Close、二重Close・閉handle Invoke拒否、active0|
|`TEE-ABI-01`|`message_abi-predicates`=1|attr0xffはCore BAD_PARAMETERS/origin3、未知commandはTA BAD_PARAMETERS/origin4、ECHO値0とUINT32_MAXが一致|
|`TEE-SESSION-CORE-01`|`closed_core_session-predicates`=1|閉じた生session IDをCoreへ送るとBAD_PARAMETERS/origin3、B継続、A再Openで未認証|
|`TEE-PARAM-01`|`parameter_validation-predicates`=1|T-05で説明した型・内容の拒否と継続|

親`EXT-TA-V01`のPASS、`functional-status`5件すべて0、子5件それぞれPASSを確認する。

### 詳細シーケンス

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    Note over N,A: 通常経路でAへ4sessionをOpen
    N->>N: 5件目・二重OpenはclientがBAD_STATEで拒否
    Note over N,A: 4sessionをClose、二重Closeと閉handle Invokeもlocal拒否
    N->>F: attr0xffの試験用Invoke
    F->>O: messageを渡す
    O-->>F: CoreがBAD_PARAMETERS、origin3
    F-->>N: TAへ進まず復帰
    Note over N,A: 未知commandはTAまで届き、BAD_PARAMETERSとorigin4
    Note over N,A: 0とUINT32_MAXのECHOを確認
    Note over N,B: A/BをOpen後、AだけClose
    N->>F: 閉じた生session IDをuncheckedで送る
    F->>O: Invoke
    O-->>F: sessionがなくBAD_PARAMETERS、origin3
    F-->>N: 復帰
    Note over N,B: BのECHOを確認、Aを再Openして未認証READ拒否を確認
    Note over N,B: 引数型・内容試験も実施し、所有sessionをClose
```

### コードと公式根拠

- client上限：[tests/validation/boundary_tests.c：b2_run_session_limits](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/boundary_tests.c#L12)。Core全体のsession上限試験ではない。
- attr・command・u32両端：[tests/validation/boundary_tests.c：b2_run_message_abi](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/boundary_tests.c#L41)。
- 生IDを敢えて送る：[normal_world/client/client.c：tee_probe_session_id](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/client.c#L309)、[tests/validation/extended_tests.c：b2_run_closed_core_session](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/extended_tests.c#L37)。
- 引数：[tests/validation/extended_tests.c：b2_run_parameter_validation](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/extended_tests.c#L67)。
- 4枠の管理・通常API拒否：[normal_world/client/client.c：tee_session_open](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/client.c#L151) / `tee_session_close` / `tee_invoke`。

公式O2のmessage ABI、O4の`entry_invoke_command`にあるsession検索・型検査。ローカルBAD_STATEは通信先が拒否した証拠ではないので、Coreの生ID試験を別に実施する。4枠・5件目等は独自client仕様。並列呼出しや全sessionライフサイクルを網羅したことにはしない。

<a id="t-15"></a>

## T-15：2つのUser TAの分離（EXT-TA-V02）

|要求表の欄|内容|
|---|---|
|検証観点|メモリ分離・障害の局所化|
|検証方法|AからBの秘密VAと同値のVAへread/write。B非改変、A再生成、panic時のB継続を確認。TA→Coreと反復資源確認も追加小項目とする。|
|合格判定基準|方向別の拒否・非改変・復旧が一致。VAとPAを区別し、TARGET_DEADだけでfault種類を断定しない。Core/統計PTA不足は該当小項目をBLOCKEDにする。|
|reportの親test_id|`EXT-TA-V02`|
|B2_REVIEW_PARENT|`13`。T番号とは区別|

### 要求表との対応

TA-A→TA-BのVM probeとpanic復旧が本体。TA→Core canary、512反復は追加小項目として分ける。方向別の子結果と追加小項目をすべて見る。Core/統計PTAがなければ当該補足はBLOCKEDで、TA間分離の観測まで消して未実施扱いにする必要はないが、補足込みの全体合格にはしない。

**説明すること：TA-Aが意図的に異常終了しても、TA-BとCoreの検査データが保たれ、Aを再生成して正常処理を続けられる。指定反復の前後で資源統計も照合する。** 実装上の親番号13 `EXT-TA-V02`。

### reportで見る場所

|場所|OK条件|
|---|---|
|子`TEE-07`|PASS、`expected-target-dead` / `other-TA-continues` / `reopen` / `recovered-echo`が各1|
|子`TEE-ISO-TA`|PASS、`read-probe-status` / `write-probe-status`が各0（PASS enum）|
|子内`private-state-tag`|Aは0x6d3a4719、Bは0xb5208c14。繰返し現れる|
|RAM `REPLY TA-VM-probe`|想定したprobeでtransport0、TARGET_DEAD、origin3|
|RAM `PROBE_FAULT domain=ta`|read/write双方の試行記録|
|REVIEW `EXT-TA-V02`|PASSに加え`Core-canary-status`=0、`resource-stress-status`=0、`stress-completed`=512|
|RAM `PROBE_FAULT domain=core`|canary対象read/writeの想定fault。CoreのCHECK_TAGは0x16ac45e3|
|RAM `STRESS_RESOURCE`|completed512、heap_after≤heap_before、ta_after≤ta_before、alloc_fail_after=alloc_fail_before|
|全体・親cleanup|0|

**この版では親PASSだけを根拠にしない。** `b2_run_review`で子2件の`inherit`後に`supplement_isolation`が親statusを再設定し、元の子FAIL/BLOCKEDを直接保持しない経路がある。したがって子2件がPASSであることを別途必須とする。`TEE-STRESS-01`はRAMログのreportで、冒頭の15子固定表に独立行として出るIDではない。

### シーケンスA：panicとTA間VM分離

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    Note over N,B: A/BをOpen
    N->>F: AへPANIC
    F->>O: Invoke
    O->>A: TEE_Panicを実行
    A-->>O: TAが異常終了
    O-->>F: TARGET_DEAD
    F-->>N: Normalへ復帰
    Note over N,B: BのECHO成功、A再OpenとECHO成功を確認
    Note over N,B: Bから試験用に秘密変数のVAを取得、A/B tagを確認
    N->>F: AへBのVAでPROBE_READまたはWRITE
    F->>O: Invoke
    O->>A: 試験用probeを実行
    A->>O: 自分のmappingでアクセス権を事前検査
    alt Aでも有効なVAだった
        O-->>A: aliasを検出
        A-->>O: ACCESS_CONFLICT、攻撃を実行しない
        O-->>F: 結果
        F-->>N: Aを作り直して最大8回再試行
    else Aでアクセス不能なVA
        A->>A: 実loadまたはstoreでfault
        O-->>F: AがTARGET_DEAD
        F-->>N: 復帰
    end
    Note over N,B: Bの秘密tag非改変、Aの再Openとtag復旧、Close
```

AへBのVA数値を渡している。AとBで同じVAが別物を指す可能性を排除してから実アクセスする。8回ともaliasならBLOCKEDで、拒否成功へ読み替えない。TA faultの内部ESRをこのreportが全て回収するわけではない。

### シーケンスB：Core canaryと512資源反復

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    Note over O: 検証用canary PTAとstatsを含むCoreが必要
    Note over N,O: 共通経路でcanary値と試験用VAを取得
    Note over N,A: AにCore canaryのread/writeを試させ、TARGET_DEADを確認
    Note over N,B: 各回canary非改変、A再Open、A/B tagを再確認
    N->>F: stats PTAをOpen
    F->>O: stats sessionを確保
    Note over N,B: 32回warmup後、資源統計beforeを取得
    loop 512回
        Note over N,B: A/BをOpen、ECHO、不正SHM拒否、鍵再生成、AES試験
        Note over N,B: 16回ごとにAをpanic、B継続とA復旧を確認
        Note over N,B: A/BをClose、clientにstats用1sessionだけ残ることを確認
    end
    Note over N,O: 資源統計afterを取得して比較、statsをClose
    N->>N: 完了数と補足statusを親へ保存
```

### コードと公式根拠

- panic復旧：[tests/validation/functional.c：b2_run_panic_recovery](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L233)。
- VM試験：[tests/validation/security_tests.c：probe_other_ta](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L140)、[tests/validation/security_tests.c：b2_run_ta_isolation](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L316)。
- TAで実際にload/storeする場所：[secure_world/ta/security.c：security_invoke](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/security.c#L169)のPROBE_READ/WRITE分岐。access rights検査を通ったという意味ではなく、**アクセス不能という事前条件を確認してから不正アクセスを試みる**。
- Core補足：[tests/validation/security_tests.c：probe_core_isolation](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L186)、[secure_world/pta/validation_canary.c：invoke_canary](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/pta/validation_canary.c#L14)。
- 反復：[tests/validation/stress_tests.c：cycle](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/stress_tests.c#L14) / `run_stress_tests`。
- 親status再設定の注意：[tests/validation/review.c：supplement_isolation](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L242)。

O4の`tee_ta_invoke_command`とTARGET_DEAD処理、O5の`TEE_Panic`、O1のTA/Coreメモリ分離が基礎。canary/stats PTAはこの試験のための追加コード。**TA間の仮想空間分離と、NormalからSecure PAへの物理保護は別。** 512回と観測統計の範囲であり、無期限・全資源のリーク不存在を示さない。

<a id="t-16"></a>

## T-16：署名TAのロード・改ざん拒否（EXT-TA-V03）

|要求表の欄|内容|
|---|---|
|検証観点|実行時TAの真正性|
|検証方法|実runtime loaderで正規署名TA、改変TA、未信頼鍵TAを比較する。|
|合格判定基準|正規のみロード・実行成功。不正TAは実行前拒否し既存サービス継続。Early TA組込みでは代替しない。|
|reportの親test_id|`EXT-TA-V03`|
|B2_REVIEW_PARENT|`14`。T番号とは区別|

### 要求表との対応

正常・改変・別鍵の実runtime loader検査に加え、不在UUID・過大サイズ・正常再試行を含む8ケースを実施する。Coreの署名/digest監査で拒否層を確認する。正常再試行は署名TAサービスの継続の証拠で、全ての既存サービスを負荷下で網羅した意味にはしない。

**説明すること：正しい署名TAをCoreが実際にロードし、改変payload・別鍵署名を想定段階で拒否し、拒否後の正常TA実行とRPC解放が成立する。** 実装上の親14 `EXT-TA-V03`、子`TEE-04`。旧alias `T-16`もあり、今回の要求表T-16と対応する。

### reportで見る場所

1. 冒頭の`TEE-04 PASS`。`signed-loader-subcase`の8件がすべてexpected=actual=1、`Core-loader-audit-read`が1。
2. `SIGNED LOADER (EXT-TA-V03 / T-16)`内の`status: PASS`。`verification: UNVERIFIED`なら監査未成立。
3. REVIEW実行なら`EXT-TA-V03`もPASS。専用selection16ではREVIEW表自体を生成しないため、ないことを不具合としない。
4. `rpc.live=0`、`unsupported=0`、`poisoned=0`、allocations=frees。
5. `subcases`のscenario0〜7を次表で確認。Core snapshotはfresh8件・overflow0・UUID順までdecoderが照合する。

### 8ケースの正解表（番号はコードどおり0始まり）

|scenario / image_variant|Open result / origin|invokes|core_flags|core_signature|core_digest|意味|
|---|---|---|---|---|---|---|
|0 normal|0 / 4|2|127（0x7f）|0|0|正常ロード・ECHO・counter・Close|
|1 tampered|0xffff000f / 3|0|95（0x5f）|0|0xffff000f|署名段階を通り、payload digestで拒否|
|2 normal|0 / 4|2|127|0|0|改変拒否後も正常動作|
|3 wrong-key|0xffff000f / 3|0|71（0x47）|0xffff000f|0xffffffff|別鍵を署名段階で拒否、digest未実行|
|4 normal|0 / 4|2|127|0|0|別鍵拒否後も正常動作|
|5 absent|0xffff0008 / 3|0|1|0xffffffff|0xffffffff|UUID不在を供給側で拒否、署名検査ではない|
|6 oversized|0xffff0006 / 3|0|1|0xffffffff|0xffffffff|宣言サイズ0x40001を供給側で拒否|
|7 normal|0 / 4|2|127|0|0|最後も正常、後始末|

各Openのtransportは0。正常ケースのECHO値0x7e168ac1、counter=1、origin4、Closeはtransport0/result0/origin3。**これらのECHO/counter/Closeの詳細はdecoder内部で検査するが、SIGNED LOADERのsubcases行には全部を再出力しない。** 元記録`b2_signed_audit.bin`、RAM `runtime-TA-echo` / `fresh-instance-count`、decoderの条件を根拠に説明する。正常ケースのqueries≥1/copies≥1、不在・過大ケースはcopies0/local拒否あり。各ケースlive0、allocations=frees。

### 詳細シーケンス

この試験のTAは**TA-A/Bと別UUIDの署名TA**。不要なTA-A/Bを図に置く代わりに「署名TA」を追加する。Core監査PTAはOP-TEEの箱の中。

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant T as 署名TA
    N->>N: bundle検査、scenarioと供給variantを選択
    N->>F: 外部UUIDでOPEN_SESSION
    F->>O: Secureへ切替
    O-->>F: RPCでTA供給を要求
    F-->>N: Normal側の限定RPCへ
    N->>N: サイズ照会、SHM確保、選択TA bytesをコピー
    N->>F: RETURN_FROM_RPC
    F->>O: ローダ処理再開
    O->>O: 署名を検査し監査記録
    O->>O: payload digestを検査し監査記録
    alt 正常
        O->>T: instance生成、Open
        T-->>O: 成功
        O-->>F: Open結果
        F-->>N: 復帰
        Note over N,T: 共通経路でECHOとcounter=1を確認してClose
    else 改変または別鍵
        O-->>F: SECURITY、origin3
        F-->>N: 復帰、TAへInvokeしない
    end
    N->>N: RPC解放数とlive0を検査
    Note over N,O: 8ケース後に監査PTAへsnapshotコピーを要求
    N->>N: snapshot取得と試験結果を保存
```

RPC要求は複数往復になる。図はサイズ照会・メモリ供給・復帰を代表化している。不在/過大は供給側拒否のため署名・digestまで進まない。

### コードと公式根拠

|処理|コード|
|---|---|
|8ケース・期待result・origin・Invoke0/2・cleanup|[tests/validation/signed_ta_tests.c：b2_run_signed_ta](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/signed_ta_tests.c#L25)|
|bundle選択とRPC loop、TA供給|[normal_world/client/signed_rpc.c：signed_rpc_call](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/signed_rpc.c#L269)、同ファイル`load_ta`|
|外部TAのECHO/counter|[secure_world/ta_signed/ta.c：TA_InvokeCommandEntryPoint](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta_signed/ta.c#L21)|
|Coreローダへの観測追加位置|[scripts/stage_signed_loader.py：apply](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/scripts/stage_signed_loader.py#L5)、[secure_world/loader/signed_loader_audit.h](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/loader/signed_loader_audit.h#L1)|
|PC側の厳密照合と上記正解表|[scripts/decode_signed_results.py：decode_signed](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/scripts/decode_signed_results.py#L7)|
|Core ELF hash必須・監査不備の扱い|[scripts/decode_b2_results.py](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/scripts/decode_b2_results.py#L1)|

公式O9の`ree_fs_ta_open`で署名検証、`ree_fs_ta_read`/`check_digest`でpayload照合を読む。O10がEarly TAと外部署名TAの違いを説明する。

通常のTA-A/BはEarly TAとして組み込むため、この署名試験とは別。監査は外部OSSの**ビルド用コピーへコードを追加**する仕組みで、署名アルゴリズム自体を自作に置き換えてはいない。「OSSは完全に無変更」とは説明しない。O9原本と`stage_signed_loader.py`の差分が観測追加の根拠。

Core監査はどこで拒否したかを示し、Normal側のエラー表示だけでは足りない。Secure Boot、鍵の安全な製品配備、rollback防止、永続Storageの検査には広げない。

<a id="t-17"></a>

## T-17：Secure呼出しの往復時間（PERF-01）

|要求表の欄|内容|
|---|---|
|検証観点|World間通信性能|
|検証方法|定義したSMCのcall前後を規定条件・サンプル数で計測する。|
|合格判定基準|正常応答・sample完備・clock/IRQ/cache/計測区間記録。速度合否は閾値を別途定める。|
|reportの親test_id|`PERF-01`|
|B2_REVIEW_PARENT|`17`。T番号とは区別|

### 現行実装の範囲

`PERF-01`は**GET_SHM_CONFIGのfast SMC往復**。TAを呼ばない。messageを使うTA Invokeとは別の区間で、C wrapper・返値の一致検査も含む。要求表の「定義したSMC」をこの区間に確定するなら、その測定結果として扱う。

### reportで見る共通条件

親は`status: MEASURED`・`phase: DONE`。`series-count=8`、`operation-errors=0`、`record-complete=2`が一致し、BENCHMARKSの`collection_status=COMPLETE`・`errors=0`。各対象系列のsampleとoverheadが各128件、warmup16、timer_hz>0を確認する。`MEASURED`は測定成立であり、速度閾値への合格ではない。

親単独を選んでも内部では8系列すべてを測るが、本項目の説明では下記の対象seriesだけを採用する。測定とlog出力の区間、IRQ/cache条件は後述「性能の共通説明」を参照する。

### 対象系列と返値検査

BENCHMARKSの`series`内で`parent_id: PERF-01`、`series_id: PERF-01`を検索する。Core応答のx0がOK、base/size/cache属性が初期化時と一致しなければ`benchmark_failed`を立てる。

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    N->>N: counter開始
    N->>F: GET_SHM_CONFIG fast SMC
    F->>O: Core fast serviceへ
    O-->>F: status、SHM base、size、cache属性
    F-->>N: 復帰
    N->>N: 初期化済みの構成と比較、counter終了
    Note over N: sample128件と測定条件を記録
    Note over A,B: この測定operationでは未使用
```

コード：[tests/validation/main.c：static void benchmark_transport](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/main.c#L26)、[normal_world/client/client.c：bool tee_client_probe_configuration](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/client.c#L324)、[runtime/benchmark/benchmark.c：void benchmark_run_scoped](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/runtime/benchmark/benchmark.c#L82)。公式O2のGET_SHM_CONFIG、O3のSMC dispatch、O11のcounter。TA処理時間と表現しない。

<a id="t-18"></a>

## T-18：メモリアクセス性能（PERF-02）

|要求表の欄|内容|
|---|---|
|検証観点|メモリ性能|
|検証方法|領域・属性・cache状態を固定し、規定read/writeを計測する。|
|合格判定基準|データ正常・sample完備・条件一致。現在のNS cached SHM結果をSecure DDR性能としない。速度閾値は別途。|
|reportの親test_id|`PERF-02`|
|B2_REVIEW_PARENT|`18`。T番号とは区別|

### 現行実装の範囲と要求表との差

現行は**Normal cached SHMへの64×u64 store＋範囲照会＋DSB**を測る。readbackとzeroは測定後の検証処理であり、read性能を独立測定しない。Secure DDR全体の性能でもない。要求表のread/writeのうちstore区間の限定測定として採用し、独立したread測定が必要なら未実施小項目として残す。

### reportで見る共通条件

親は`status: MEASURED`・`phase: DONE`。`series-count=8`、`operation-errors=0`、`record-complete=2`が一致し、BENCHMARKSの`collection_status=COMPLETE`・`errors=0`。各対象系列のsampleとoverheadが各128件、warmup16、timer_hz>0を確認する。`MEASURED`は測定成立であり、速度閾値への合格ではない。

親単独を選んでも内部では8系列すべてを測るが、本項目の説明では下記の対象seriesだけを採用する。測定とlog出力の区間、IRQ/cache条件は後述「性能の共通説明」を参照する。

### 対象系列とデータ検証

BENCHMARKSの`series`内で`parent_id: PERF-02`、`series_id: PERF-02`を検索する。使用領域は交渉したSHM末尾の512B。i番目のu64にiを書き、測定外で全64語を照合してzeroする。失敗は`benchmark_failed`へ反映する。

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    Note over N: 初期化時にSHM構成を確認済み
    N->>N: counter開始
    N->>N: shared_rangeを照会し末尾512Bを選ぶ
    N->>N: 64個のu64へiをstore
    N->>N: DSB、counter終了、sample保存
    N->>N: 測定外でreadbackしてiと比較しzero
    Note over F,B: この測定operationはNormal内で完結
```

コード：[tests/validation/main.c：static void benchmark_shared_memory](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/main.c#L32)、[tests/validation/main.c：static void verify_shared_memory](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/main.c#L46)、[runtime/benchmark/benchmark.c：void benchmark_run_scoped](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/runtime/benchmark/benchmark.c#L82)。O11のmemory attributes・barrier・counterが基礎。範囲照会とstore loopは独自の測定仕様。

<a id="t-19"></a>

## T-19：Secure割込み応答時間（PERF-03）

|要求表の欄|内容|
|---|---|
|検証観点|割込み性能|
|検証方法|割込み発生からSecure handlerの定義した時刻点までを計測する。|
|合格判定基準|配送正常、時刻点とclockが確定、sample完備。応答上限は別途定める。|
|reportの親test_id|`PERF-03`|
|B2_REVIEW_PARENT|`19`。T番号とは区別|

### 現行reportで見る場所と扱い

`REVIEW`内の`test_id: PERF-03`を見る。review一式、または当該親を選択したrunでは**`status: BLOCKED`、`phase: DONE`**が現行コードの想定。これは試験に合格した意味ではない。未選択ならNOT_RUNのままなので、実施済みと数えない。

- 未実装の部分：Secure SGIのPTAと配送構成がなく、応答時間sampleを取得していない。
- 要求表の合格基準に対する判定：**未実施／保留。PASSの実行証拠は生成されない。**
- PERF-05のTA Invoke時間や8系列のsample完備をSecure IRQ性能に流用しない。

検索するreasonは`Secure-SGI-not-in-selected-Core; ZCU-SGI-group-route-review-required`または`Secure-SGI-profile-not-enabled`。T-08/T-19の選択の組合せでreasonが分かれるため、文字列が一種類だけだと思わない。

### 現行コードが実際に行う流れ

実装のない機能を動かす図は描かない。実際には以下の**結果記録だけ**を行い、要求表にある実操作は行わない。

|順序|担当|実際の処理|
|---|---|---|
|1|BL33|b2_run_reviewで対象親が選択されているか調べる|
|2|BL33|run_secure_sgiのB2_SECURE_SGI != 1分岐へ入る|
|3|BL33|finishでBLOCKED・reason・DONEを保存し、PTA Openへ進まない|
|4|TF-A／OP-TEE／TA-A／TA-B|この親の要求操作は呼ばれない。他の選択試験で動くこととは区別|

### コードと必要な追加証拠

コード：[tests/validation/review.c：static void run_secure_sgi](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L268)、[tests/validation/review.c：static void finish](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L79)。

実施するために確定する内容：T-08の配送成立、発生時刻点とhandler内時刻点、同一clock、warmup/sample、取得条件、応答閾値。

公式根拠：O1のInterrupt handling、O11のGeneric Timer、O12のGIC設定。 ここで挙げる追加条件はレビューの不足事項で、現行の実行済みシーケンスではない。

<a id="t-20"></a>

## T-20：許可領域のDMA転送性能（PERF-04）

|要求表の欄|内容|
|---|---|
|検証観点|DMA性能|
|検証方法|許可領域間でsize・cache・master条件を固定して転送時間/量を計測する。|
|合格判定基準|転送データ一致、sample完備、条件記録。禁止領域の拒否試験とは分ける。速度閾値は別途。|
|reportの親test_id|`PERF-04`|
|B2_REVIEW_PARENT|`20`。T番号とは区別|

### 現行reportで見る場所と扱い

`REVIEW`内の`test_id: PERF-04`を見る。review一式、または当該親を選択したrunでは**`status: SKIP`、`phase: DONE`**が現行コードの想定。これは試験に合格した意味ではない。未選択ならNOT_RUNのままなので、実施済みと数えない。

- 未実装の部分：DMA転送backendがなく、転送時間/量を測っていない。
- 要求表の合格基準に対する判定：**未実施／保留。PASSの実行証拠は生成されない。**
- PERF-02のCPUによるSHM store時間はDMA帯域ではない。

reasonは`OUT-OF-SCOPE: peripheral-DMA-debug-authenticated-boot-persistent-storage`を固定長フィールドへ保存する。JSONの`reason`は長い文字列の末尾が切れる場合があるため、`OUT-OF-SCOPE:`と正式ID・statusで識別する。

### 現行コードが実際に行う流れ

実装のない機能を動かす図は描かない。実際には以下の**結果記録だけ**を行い、要求表にある実操作は行わない。

|順序|担当|実際の処理|
|---|---|---|
|1|BL33|b2_run_reviewで対象親が選択されているか調べる|
|2|BL33|対象外親IDの分岐でfinishを呼ぶ|
|3|BL33|SKIP・reason・DONEを保存する|
|4|TF-A／OP-TEE／TA-A／TA-B|この親の要求操作は呼ばれない。他の選択試験で動くこととは区別|

### コードと必要な追加証拠

コード：[tests/validation/review.c：void b2_run_review](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L351)、[tests/validation/review.c：static void finish](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/review.c#L79)。

実施するために確定する内容：T-07とは別の許可領域間転送、size・cache・master条件、転送一致、clockとsample、速度閾値。

公式根拠：O12の対象DMA engineと保護経路、O11の測定counter。 ここで挙げる追加条件はレビューの不足事項で、現行の実行済みシーケンスではない。

<a id="t-21"></a>

## T-21：TA呼出し・暗号処理時間（PERF-05）

|要求表の欄|内容|
|---|---|
|検証観点|TEEサービス性能|
|検証方法|ECHO、暗号、Open、Close、異常終了後復旧、Storageの測定を小項目に分ける。|
|合格判定基準|正常動作と各測定区間の一致、sample完備。未構成Storageは保留。各時間上限は別途。|
|reportの親test_id|`PERF-05`|
|B2_REVIEW_PARENT|`21`。T番号とは区別|

### 現行実装の範囲と要求表との差

現行の対象はTA ECHO、AES生成/往復/KAT、Open、Close、panic込み復旧、TARGET_DEAD観測後の復旧の6系列。**Storage測定は構成していないため保留。** 各時間上限も未設定で、6系列のMEASUREDをStorageを含む全項目の速度合格にしない。

### reportで見る共通条件

親は`status: MEASURED`・`phase: DONE`。`series-count=8`、`operation-errors=0`、`record-complete=2`が一致し、BENCHMARKSの`collection_status=COMPLETE`・`errors=0`。各対象系列のsampleとoverheadが各128件、warmup16、timer_hz>0を確認する。`MEASURED`は測定成立であり、速度閾値への合格ではない。

親単独を選んでも内部では8系列すべてを測るが、本項目の説明では下記の対象seriesだけを採用する。測定とlog出力の区間、IRQ/cache条件は後述「性能の共通説明」を参照する。

### 6系列の対応

BENCHMARKSの`parent_id: PERF-05`に属する`series_id`は、`PERF-05`、`PERF-06`、`PERF-08`、`PERF-09`、`PERF-10`、`PERF-11`。この`PERF-11`は**T-11（起動署名）とは無関係な系列番号**である。

|系列|測定対象|測定外|
|---|---|---|
|PERF-05|TA ECHO Invokeと返値照合|session準備・終了|
|PERF-06|鍵生成・AES往復・既知解とtransport|測定枠外の準備|
|PERF-08|既存TA instanceへのsession Open|その後のClose|
|PERF-09|他sessionが残る状態のClose|直前のOpen|
|PERF-10|panic診断、TARGET_DEAD確認、再Open、ECHO|初期Open、最終Close|
|PERF-11|TARGET_DEAD観測後の再Open、ECHO|事前のpanicと観測、最終Close|

```mermaid
sequenceDiagram
    participant N as BL33
    participant F as TF-A
    participant O as OP-TEE
    participant A as TA-A
    participant B as TA-B
    Note over N,A: 系列ごとのprepareを測定前に実行
    N->>N: counter開始
    N->>F: 対象operationのOpen、InvokeまたはClose
    F->>O: Secureへ切替
    O->>A: ECHO、暗号、session処理または意図的panic
    A->>O: 必要な暗号API等、またはTA異常終了
    O-->>F: 成功または期待するTARGET_DEAD
    F-->>N: 復帰
    Note over N,A: 復旧系列は区間内で再OpenとECHOも実行
    N->>N: operationの結果を照合、counter終了
    N->>N: 測定外cleanup、sample保存
    Note over B: 6系列の測定operationでは未使用
```

コード：[tests/validation/main.c：void b2_run_performance](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/main.c#L113)、[tests/validation/main.c：static void recover_session](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/main.c#L97)、[tests/validation/main.c：static void reopen_and_check](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/main.c#L81)、[secure_world/ta/ta.c：static TEE_Result run_crypto_test](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/secure_world/ta/ta.c#L18)。公式O4・O5がサービス/API、O11がtimerの根拠。各operationの包含区間はコードを根拠に説明する。

## 4. 補助子B2-SMCCC-01

これはT-02・T-14に関連する補助確認で、T番号ではない。BL33からTF-AへSMCCC_VERSION（0x80000000）、PSCI_VERSION（0x84000000）を問い合わせる。OP-TEEのTA messageやTA-A/Bは使わない。

|check名|OK値|実値の場所|
|---|---|---|
|SMCCC-version-in-range|1|RAM B2_STANDARD smccc=...|
|PSCI-version-in-range|1|RAM B2_STANDARD psci=...|

SMCCC=0x10000〜0x10007、PSCI=2〜0x10003の範囲を照合する。VERSION応答だけで電源制御・全SMCCC規約・全CPU contextを検証したとしない。

コード：[tests/validation/extended_tests.c：void b2_run_standard_versions](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/extended_tests.c#L20)。公式O8が背景。

## 5. reportの種類と子単独選択

reportの名前は解析時の`--output`で決まり、同名でも内容はrunごとに違う。

|B2_TEST_INDEX|reportの内容|
|---|---|
|0|基本機能13子。21項目一式ではなく署名・物理を含まない|
|17、親0|review一式。必要子＋親表＋署名監査＋8性能系列。物理実probeは別|
|17、親番号|選択した親と必要子。親番号は1章の対応表を使う|
|15|物理専用：子TZ-SYS-03とPHYSICAL EVIDENCE（T-04）|
|16|署名単独：子TEE-04とSIGNED LOADER（T-16）。親REVIEW表なし|
|18|性能8系列。SGIはBLOCKED、対象外SKIPも記録|
|14|旧legacy経路。通常の新結果表と区別|

### 子単独選択

|B2_TEST_INDEX|子ID|関連T項目|
|---|---|---|
|1|TEE-01|T-01、T-02、T-14|
|2|TEE-07|T-15|
|3|TEE-AUTH-TA|T-10|
|4|TEE-SHM-01|T-05|
|5|TEE-KEY-API|T-12|
|6|TEE-ISO-TA|T-15|
|7|TEE-SESSION-01|T-14|
|8|TEE-ABI-01|T-14|
|9|TEE-BOUNDARY-01|T-05|
|10|B2-SMCCC-01|T-02、T-14の補助|
|11|TEE-SESSION-CORE-01|T-14|
|12|TEE-PARAM-01|T-05、T-14|
|13|TEE-KEY-ATTR-01|T-12|
|15|TZ-SYS-03|T-04|
|16|TEE-04|T-16|

**T-11は起動署名、B2_REVIEW_PARENT=11は鍵API、B2_TEST_INDEX=11は閉Core sessionの子試験。** このように数値を直接対応させない。正式IDを検索してから選択・説明する。

## 性能の共通説明：T-17・T-18・T-21

### 親IDと系列IDは同じ番号体系ではない

|親ID|series_id|測っている区間|入口|
|---|---|---|---|
|PERF-01|PERF-01|GET_SHM_CONFIGのfast SMC往復＋wrapperと返値照合|`benchmark_transport` → `tee_client_probe_configuration`|
|PERF-02|PERF-02|Normal SHMに64×u64 store＋範囲照会＋DSB|`benchmark_shared_memory`。読戻し・zeroは測定外|
|PERF-05|PERF-05|TA ECHO Invoke。message作成・SMC・結果照合を含む|`benchmark_ta`|
|PERF-05|PERF-06|TA内の鍵生成・AES往復・KAT＋transport|`benchmark_crypto`|
|PERF-05|PERF-08|他sessionが開いているTAへのOpen|`prepare_session`を測定。後のCloseは外|
|PERF-05|PERF-09|他sessionを残したClose|Openは測定前、`close_session`を測定|
|PERF-05|PERF-10|PANIC要求・診断・TARGET_DEAD確認・再Open・ECHO|`recover_session`。最終Closeは外|
|PERF-05|PERF-11|TARGET_DEADを確認済みの状態から再Open・ECHO|panicは測定前、`reopen_and_check`を測定|

親17/18/21のどれかを単独選択しても内部では8系列全部を測る。`PERF-03`（Secure SGI）と`PERF-04`（DMA）は今回測定していない。

### 数値を説明する

- `statistics`の`minimum`、`maximum`、`mean`、`median`、`p95`、`p99`を見る。平均は`mean`。
- p95は小さい順に並べた128件の122番目、p99は127番目（1始まり）。統計検定のp値ではない。
- 時間[µs] = ticks × 1,000,000 ÷ timer_hz。平均・p95等にも同じ換算を適用できる。
- `overhead`は空operationの測定値。`overhead_subtracted=false`なので表示値から自動減算していない。
- 観測最大値は今回128件の最大であり、将来の最悪時間保証ではない。比較にはclock・cache・build・入力・デバッガ停止条件を揃える。

### コードと公式根拠

- 8系列の順とprepare/operation/cleanup：[tests/validation/main.c：b2_run_performance](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/main.c#L113)。
- 実測区間：[runtime/benchmark/benchmark.c：benchmark_run_scoped](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/runtime/benchmark/benchmark.c#L82)。counter開始・終了の間にある`operation()`を確認する。
- fast SMC照合：[normal_world/client/client.c：tee_client_probe_configuration](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/normal_world/client/client.c#L324)。
- カウンタ読み：[runtime/arch.c：counter_read](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/runtime/arch.c#L12) / `counter_frequency`。
- 平均とpercentileの定義：[scripts/analyze_benchmarks.py：summarize](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/scripts/analyze_benchmarks.py#L9)。
- reportの系列整合性とCSV：[scripts/decode_b2_review.py：benchmarks](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/scripts/decode_b2_review.py#L50)。

O11のGeneric Timerがカウンタの基礎。サンプル数・測定範囲・統計集計は独自仕様。測定中にbreakpointや手動停止を挟んだデータは性能値として採用しない。

## 6. 公式資料は何の根拠になるか

公式資料は仕組み・API・ハードウェアの意味の根拠。このrepoの独自T番号、0x45a7、16B、256回、512回、PASS論理を規定してはいない。**独自判定の根拠は試験コード、標準動作の根拠は公式仕様、実際に動いた根拠は同runの回収記録**という3種類を区別する。

外部コードの確認版：OP-TEE 4.5.0 `0919de0f7c79ad35ad3c8ace5f823ad1344b4716`、TF-A AMD版 `dd3512287a17eea866d5c82fa7b86ad58de59163`。手元で別版なら該当箇所を再照合する。GPの現行資料と使用版実装の挙動が同じとは自動的に仮定しない。

|参照ID|公式資料・探す見出し/関数|今回の説明で裏付けること|
|---|---|---|
|O1|[OP-TEE 4.5 Core](https://optee.readthedocs.io/en/4.5.0/architecture/core.html)：Shared Memory、world context switch、memory management|SHMはWorld間のデータ、SMC/monitorは制御の切替。Coreとuser TAの分離|
|O2|OP-TEEの`optee_smc.h`と`optee_msg.h`（後述リンク）|CALL_WITH_ARG、GET_SHM_CONFIG、RPC、message command・属性・ret/origin|
|O3|TF-Aの`opteed_main.c:opteed_smc_handler`|OP-TEE用SMCのdispatchとNormal/Secure contextの切替|
|O4|OP-TEEの`entry_std.c`、`tee_ta_manager.c`|Open/Invoke/Close、引数とsession検査、TA異常時のTARGET_DEAD|
|O5|[GlobalPlatform TEE Internal Core API](https://globalplatform.org/specs-library/tee-internal-core-api-specification/)：TEE_OpenTASession、TEE_InvokeTACommand、TEE_GetPropertyAsIdentity、TEE_Panic、Transient Object、Object Usage、Cipher API|TA側APIの契約。現行掲載版はv1.4であり、使用OP-TEEが全項目適合するという主張ではない|
|O6|[Arm AAPCS64](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst)：General-purpose Registers、Stack|callee-saved GPRとSPの呼出し規約。全SMC状態保証との違い|
|O7|OP-TEEの`tee_api_defines.h`、`tee_svc_cryp.c:syscall_cryp_obj_get_attr`|実使用版のresult値・usage属性・非抽出鍵の属性拒否|
|O8|[Arm SMC Calling Convention DEN0028](https://developer.arm.com/documentation/den0028/latest/)|SMC function IDと引数・返値・呼出し規約|
|O9|OP-TEEの`ree_fs_ta.c`|REE TAを要求し、署名とpayload digestを検証する実装|
|O10|[OP-TEE 4.5 Trusted Applications](https://optee.readthedocs.io/en/4.5.0/architecture/trusted_applications.html)：Pseudo Trusted Applications、Early TA、REE filesystem TA|PTAはCore権限、Early TAは組込み、外部TAは署名ファイルとしてロード|
|O11|[Arm Architecture Reference Manual DDI0487](https://developer.arm.com/documentation/ddi0487/latest/)：ESR_EL1、FAR_EL1、ELR_EL1、PAR_EL1、AT S1E1R/W、MAIR_EL1、Generic Timer|物理試験の変換・例外・属性、性能counterの意味。公開入口は本文取得できなかったため、版別節番号は断定しない|
|O12|[AMD UG1085](https://docs.amd.com/v/u/en-US/ug1085-zynq-ultrascale-trm)のXMPU説明と[UG1087](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers)のXMPU_DDRレジスタ群|bank・route/master・region設定・拒否応答・違反情報。任意の読取エラーだけでXMPU保護成立としない|

### 使用版の公式ソースへ直接飛ぶ

- O2 [core/arch/arm/include/sm/optee_smc.h：OPTEE_SMC_FUNCID_CALL_WITH_ARG](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/core/arch/arm/include/sm/optee_smc.h#L199)
- O2 [core/include/optee_msg.h：struct optee_msg_arg](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/core/include/optee_msg.h#L162)
- O3 [services/spd/opteed/opteed_main.c：static uintptr_t opteed_smc_handler](https://github.com/Xilinx/arm-trusted-firmware/blob/dd3512287a17eea866d5c82fa7b86ad58de59163/services/spd/opteed/opteed_main.c#L597)
- O4 [core/tee/entry_std.c：static void entry_open_session](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/core/tee/entry_std.c#L351)
- O4 [core/tee/entry_std.c：static void entry_invoke_command](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/core/tee/entry_std.c#L419)
- O4 [core/tee/entry_std.c：static void entry_close_session](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/core/tee/entry_std.c#L399)
- O4 [core/kernel/tee_ta_manager.c：TEE_Result tee_ta_invoke_command](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/core/kernel/tee_ta_manager.c#L750)
- O7 [lib/libutee/include/tee_api_defines.h：#define TEE_ERROR_TARGET_DEAD](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/lib/libutee/include/tee_api_defines.h#L127)
- O7 [core/tee/tee_svc_cryp.c：TEE_Result syscall_cryp_obj_get_attr](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/core/tee/tee_svc_cryp.c#L1282)
- O9 [core/kernel/ree_fs_ta.c：static TEE_Result ree_fs_ta_open](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/core/kernel/ree_fs_ta.c#L235)
- O9 [core/kernel/ree_fs_ta.c：static TEE_Result ree_fs_ta_read](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/core/kernel/ree_fs_ta.c#L546)

## 7. 試験を説明するときの順番と採用チェック

各T項目は次の5文で説明を組み立てると、ログとコードの対応が伝わりやすい。

1. 「この試験は○○を確認します」。対象World・TA・APIと範囲を指定する。
2. 「BL33のこの関数から、これを入力します」。関数を開き、実際の引数を指す。
3. 「CoreまたはTAのこの分岐が処理・拒否します」。TAまで届かない入力ならその検査層を示す。
4. 「Normalへ戻ったら、この条件を比較します」。boolを作る式とcheckを指す。
5. 「reportの正式IDとこのcheckが一致し、後始末と追加証跡も揃うので、この範囲をOKとします」。保証しない範囲も一文で添える。

### 各runの記録欄

|項目|記入する内容|
|---|---|
|本書T番号 / 実装上の親番号ID / 子ID|例：T-16 / EXT-TA-V03 / TEE-04|
|reportパス / run ID|実ファイルと先頭run。hex表記も併記|
|対象commitとローカル差分|公開コードとの差分、特に物理・TF-A修正|
|profile / selection / parent / compiler|実際に使った選択|
|BL33 / Core / BL31 / raw / bundle対応|実ロード物のhash、capture計画との一致|
|対象結果 / check|PASS/MEASURED等とexpected/actual|
|追加証跡|署名8監査、物理4attempt＋XMPU、性能sample条件等|
|後始末 / 回収|cleanup_errors0、Collection COMPLETE、ログ欠落なし|
|判定と限界|確認できた対象・範囲。未対応・未実施も記録|

実行物の対応を再確認する際は、ビルド済みELF・tee-raw.bin・bundle等を消してしまうとhashとコード位置を再照合しにくくなる。少なくとも実行物の保存先とhash、build設定、raw dump、capture計画、report、公開鍵の指紋を対応させる。秘密鍵をreportへ貼らない。

## 付録A. 子checkのコード上の検索先一覧

次表は取得したソースの`b2_check_value`呼出しから抽出した検索用索引。**expected列はCの式で、説明したPASS条件と併読する。** 全てのcheckが全runで出るわけではなく、選択・分岐・反復で出現数が変わる。判定条件にはcheck外のbool式もある。

|check名|expectedのC式|コード位置|
|---|---|---|
|`client-limit-and-cleanup`|`1UL`|[boundary_tests.c:36](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/boundary_tests.c#L36)|
|`message_abi-predicates`|`1UL`|[boundary_tests.c:62](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/boundary_tests.c#L62)|
|`shared_boundaries-predicates`|`1UL`|[boundary_tests.c:107](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/boundary_tests.c#L107)|
|`SMCCC-version-in-range`|`1UL`|[extended_tests.c:28](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/extended_tests.c#L28)|
|`PSCI-version-in-range`|`1UL`|[extended_tests.c:29](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/extended_tests.c#L29)|
|`closed_core_session-predicates`|`1UL`|[extended_tests.c:58](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/extended_tests.c#L58)|
|`parameter_validation-predicates`|`1UL`|[extended_tests.c:106](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/extended_tests.c#L106)|
|`key_attributes-predicates`|`1UL`|[extended_tests.c:129](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/extended_tests.c#L129)|
|`first-echo`|`1UL`|[functional.c:223](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L223)|
|`second-echo`|`1UL`|[functional.c:224](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L224)|
|`unknown-command-rejected`|`1UL`|[functional.c:225](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L225)|
|`expected-target-dead`|`1UL`|[functional.c:257](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L257)|
|`other-TA-continues`|`1UL`|[functional.c:258](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L258)|
|`reopen`|`1UL`|[functional.c:259](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L259)|
|`recovered-echo`|`1UL`|[functional.c:260](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/functional.c#L260)|
|`Secure-allocation-bytes`|`PHYSICAL_PAGE_BYTES`|[physical_memory.c:163](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/physical_memory.c#L163)|
|`precise-external-abort`|`3UL`|[physical_memory.c:232](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/physical_memory.c#L232)|
|`Secure-page-check`|`PHYSICAL_CHECK_TAG`|[physical_memory.c:233](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/physical_memory.c#L233)|
|`post-probe-TA-ECHO`|`PHYS_ECHO`|[physical_memory.c:245](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/physical_memory.c#L245)|
|`Normal-control`|`PHYS_ECHO`|[physical_memory.c:246](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/physical_memory.c#L246)|
|`private-state-tag`|`tag`|[security_tests.c:30](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L30)|
|`TA-identity-policy`|`1UL`|[security_tests.c:57](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L57)|
|`SHM-reject-and-nonmodification`|`1UL`|[security_tests.c:108](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L108)|
|`key-usable-before-after`|`1UL`|[security_tests.c:126](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L126)|
|`export-rejected-zero-output`|`1UL`|[security_tests.c:127](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L127)|
|`AES-roundtrip-and-KAT`|`1UL`|[security_tests.c:128](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L128)|
|`victim-and-recovery`|`TEST_PASS`|[security_tests.c:183](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L183)|
|`read-probe-status`|`TEST_PASS`|[security_tests.c:329](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L329)|
|`write-probe-status`|`TEST_PASS`|[security_tests.c:330](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/security_tests.c#L330)|
|`signed-loader-subcase`|`1UL`|[signed_ta_tests.c:108](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/signed_ta_tests.c#L108)|
|`Core-loader-audit-read`|`1UL`|[signed_ta_tests.c:136](https://github.com/ririmon22/trustzone-validation/blob/a94646ea4130b448961c7921c57d1d317da8f36b/tests/validation/signed_ta_tests.c#L136)|

## 付録B. 改訂時の照合範囲

- 提示された21項目表と同じT-01〜T-21・正式ID・項目名・順序に変更した。
- 実装の親番号はdecoderのPARENTS配列から対応付け、T-11〜T-16の並びの差を明示した。
- 実装済み小試験のcheck名・期待値・コードリンクを保持し、未実装の8項目には実際のSKIP/BLOCKED記録経路と不足証拠を記載した。
- T-01の両側EL/State、T-02の復帰PC、T-10の本物Normal認証、T-12の生メモリ抽出、T-18の独立read計測、T-21のStorageを、限定PASS/MEASUREDから推定しないようにした。
- T-17・T-18・T-21は対象区間の異なるシーケンスを用意した。Mermaidのブロック対応・参加者名・人数を機械点検するが、描画エンジンでのレンダリング確認は行っていない。
- 今回は説明資料のみを更新した。試験コード、reportの出力名やstatus、実機設定は変更していない。利用者の実reportと実機結果の最終照合は未実施。

