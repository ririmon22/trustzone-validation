# 1. 最終111版の範囲と21項目一覧

[読む順の入口](../README.md) → 本書 → [試験仕様と図](b2_test_specification.md) → [仕組みとコードの読み方](b2_reading_guide.md)。操作だけを行う場合は[ビルド](zcu106_build.md)・[起動と回収](zcu106_run.md)。

## 対象版と証拠の扱い

対象は最終111ファイルZIPに収録されたZCU106/B2（TF-A BL31＋OP-TEE BL32＋Standalone BL33）のsource/config/script。公開main、dirty開発tree、QEMU/A/B1の動作と区別する。本資料版は111版を基準に説明書4本を3本へ統合し、性能ログの固定cache/MMU表示だけを採取済SCTLRからの表示へ修正した。試験範囲・計測区間・入力設定は変更していない。

**分類は12実行可能・1条件待ち・8今回対象外で一致する。** 「実行可能」はcodeと選択経路が揃う意味で、今すぐロード可でも実機PASSでもない。全項目共通でB2_RUNTIME_READY=0、安全ゲートBLOCKED。物理試験はB2_PHYS_REVIEWED=0も維持。正式レビューはこの21親項目だけ。TEE-xx等は親の小試験である。

理由分類：①必要codeを移送していない、②backend/新しい仕組み未実装、③codeはあるが実機条件未確認、④今回方針として対象外、⑤実行経路はあるが観測・証跡が不足。①②④を「技術的に不可能」と読み替えない。

### ローカル検証記号

- **L-B**：先の108版でZIP入力からSDK/TA/Core/BL31/BL33 buildと静的検査成功。111版のC/ASM/ヘッダ/リンカ/Makefileは108版とbyte一致の検査済み。ただし111版の実機・QEMUで親試験を実行した証拠ではない。
- **L-X**：111版の限定XMPU追加処理はホスト不正入力15ケースとTclモックの適用/読戻しを検査。実MMIO/XSDBは未実施。
- **L-D**：PC decoderの合成RAM・Tclモック検査。TA/Coreの実行成功の代替ではない。

本版で新たに行った検査は、性能metadata表示の8組合せ・未採取/古いrun等のホスト検査、測定ループとcallback順の不変検査、変更BL33のAArch64ビルド、既存性能decoder回帰。実機での再測定は行っていない。

各行の実機状況は**111版NOT_RUN（共通起動条件③・証跡⑤）**。対象外は実測結果を作らない。旧版USER_REPORTEDは末尾で別管理する。機能の合否と回収のCOMPLETEは別で、完全なFAIL/BLOCKEDの回収もあり得る。

## 全21項目

単独欄の`ID`は`python3 scripts/select_b2_test.py --config config/review.local.json --test ID`を表す（PC上のBL33 buildだけ）。一式は`--test review`。それぞれ実生成Core/profileと一致させる。物理と署名は後述の例外がある。

|元ID・項目名・目的|今回確認する範囲|今回確認しない範囲|必要code・主要関数|単独／一式|区分・具体的根拠|不足する実装・設定・情報|ローカル／111実機|合格が示すこと／示さないこと|
|---|---|---|---|---|---|---|---|---|
|TZ-SYS-01-V01：両Worldの正常実行<br>Normal制御とSecure TAが実行・復帰できる|2 session ECHO／未知command拒否|Aの自作両World、システム全体の物理分離|functional.c:b2_run_service → review.c:inherit|TZ-SYS-01-V01／review|**実行可能**：コードあり。review parent 1がTEE-01を実行・継承|共通起動条件。実機証跡不足⑤|L-B/L-D（当該親の実測なし）／NOT_RUN|NormalとTAの要求応答が成立／世界全体の分離は未証明|
|TZ-SYS-02-V01：呼出しと復帰<br>SMCからNormalへ処理を戻す|TEE-01の成功・継続・Close|PSCI電源操作、全CPU contextの網羅検査|functional.c:b2_run_service → review.c:inherit|TZ-SYS-02-V01／review|**実行可能**：コードあり。parent 2が同じTEE-01を継承|共通起動条件⑤|L-B/L-D（当該親の実測なし）／NOT_RUN|指定要求の往復／電源管理・全状態保存は未証明|
|TZ-SYS-02-V02：反復と状態保持<br>反復時に保持対象を壊さない|256 ECHO＋256 counter、x19..x29/SP・EL1状態・A/Bタグ|x0..x18/NZCV/SIMDの保存保証|review.c:run_state_retention、state_call.S:b2_state_echo|TZ-SYS-02-V02／review|**実行可能**：parent 3が自分でA/BをOpenし比較・Close|共通起動条件⑤|L-B/L-D（当該親の実測なし）／NOT_RUN|指定回数と状態の保持／全ABI・長時間無制限の保証でない|
|TZ-SYS-03-V01：物理メモリ保護<br>NS CPUの直接accessから専用Secureデータを守る|限定XMPU計画・適用・読戻しと、専用page同一PAへのread/write|稼働Core/TAへの保護変更、全DDR境界の網羅、MMU拒否だけのPASS|physical_memory.c:b2_run_physical_memory、physical_policy.c:b2_classify_physical_abort、physical_probe.c:prepare_page、prepare_xmpu_test.py:prepare、xmpu_test.tcl:tz_xmpu_test_apply|TZ-SYS-03（実probe）；親ID指定はBLOCKED報告のみ。reviewに実probeなし|**条件待ち**：コードあり。ただしparent 4はBLOCKED記録だけ。実probeは--test TZ-SYS-03（selection 15）|実機条件③：専用予約・route/master/lock/poison/priority、EL2/Stage2。NS MMU拒否と物理拒否の帰属証跡⑤|L-B/L-D、追加設定処理L-X／NOT_RUN|専用pageの指定アクセス拒否＋非改変・継続／全経路の保護、XMPU帰属は別証跡|
|TZ-SYS-03-V02：秘密とSHMの使い分け<br>共有入力を検査し秘密と混同しない|TEE-SHM-01、TEE-BOUNDARY-01、TEE-PARAM-01|NS CPUがSecure PAを直接読む試験|security_tests.c:test_shared_memory、boundary_tests.c:b2_run_shared_boundaries、extended_tests.c:b2_run_parameter_validation|TZ-SYS-03-V02／review|**実行可能**：parent 5が3小試験を継承|共通起動条件⑤|L-B/L-D（当該親の実測なし）／NOT_RUN|メッセージ境界・型・内容の拒否と非改変／CPU物理保護ではない|
|TZ-SYS-04-V01：周辺機器保護<br>不許可の周辺操作を防ぐ|SKIP理由の記録だけ|GPIO/XPPU操作試験|review.c:b2_run_review|TZ-SYS-04-V01／review|**今回対象外**：backend本体なし①、今回方針④。parent 6はSKIP|機器所有権・経路・XPPU backendが別途必要|選択時のSKIP/BLOCKED分岐を静的照合／NOT_RUN|今回合格判定なし／技術的に不可能という意味ではない|
|TZ-SYS-05-V01：DMA保護<br>CPU以外からの保護迂回を検査|SKIP理由の記録だけ|DMA転送・拒否検査|review.c:b2_run_review|TZ-SYS-05-V01／review|**今回対象外**：DMA backendなし①、方針④。parent 7はSKIP|DMA経路/master、転送backendが別途必要|選択時のSKIP/BLOCKED分岐を静的照合／NOT_RUN|今回合格判定なし|
|TZ-SYS-06-V01：Secure割込み<br>Secure側への配送と復帰を確認|BLOCKED理由の記録だけ|SGI生成・handler・配送|review.c:run_secure_sgi|TZ-SYS-06-V01／review|**今回対象外**：SGI PTAなし①、ZCU接続なし②、方針④。B2_SECURE_SGIを0に固定|PTA再導入＋ZCU group/番号/routeへの対応。実機情報だけでは実行可能にならない|選択時のSKIP/BLOCKED分岐を静的照合／NOT_RUN|今回合格判定なし|
|TZ-SYS-07-V01：JTAG制限<br>権限によるdebug access制御を確認|SKIP理由の記録だけ|debug認証、eFUSE設定|review.c:b2_run_review|TZ-SYS-07-V01／review|**今回対象外**：実機認証構成なし②、方針④。parent 9はSKIP|別のdebug権限構成と承認|選択時のSKIP/BLOCKED分岐を静的照合／NOT_RUN|今回合格判定なし|
|EXT-AUTH-V01：既存認可<br>sessionとTA identityで許可・拒否する|固定tokenの許可/取消/別session/再Open、A→B許可とPUBLIC/偽ID拒否|本物のNormal要求者認証基盤|review.c:run_token_policy、security_tests.c:test_identity、ta/security.c:trusted_read|EXT-AUTH-V01／review|**実行可能**：parent 10がTEE-AUTH-TA＋追加token比較を実行|共通起動条件⑤。本物認証は未実装②だが今回範囲外④|L-B/L-D（当該親の実測なし）／NOT_RUN|固定token/TA許可リストの動作／利用者本人性を証明しない|
|EXT-KEY-V01：鍵API保護<br>秘密鍵属性の持出しを制限し暗号を使う|生成・使用制限・自作export拒否・Core属性拒否・AES往復/KAT|生メモリ抽出攻撃、PUF、乱数源評価|security_tests.c:test_key_api、extended_tests.c:b2_run_key_attributes、ta/security.c:key_attribute_test、ta/ta.c:run_crypto_test|EXT-KEY-V01／review|**実行可能**：parent 11がTEE-KEY-API/ATTRの両方を継承|共通起動条件⑤。鍵周期はEXT-TA-V02内の512反復にも含む|L-B/L-D（当該親の実測なし）／NOT_RUN|APIでの制限と暗号の既知結果／全経路の鍵抽出耐性ではない|
|EXT-TA-V01：TA利用とsession<br>正常呼出しと異常入力・状態を検査|ECHO、client4枠、二重Open、閉ID、ABI、型/内容|並列・非同期client、Core全体のsession上限|functional.c:b2_run_service、boundary_tests.c:b2_run_session_limits/b2_run_message_abi、extended_tests.c:b2_run_closed_core_session/b2_run_parameter_validation|EXT-TA-V01／review|**実行可能**：parent 12が5小試験を継承|共通起動条件⑤|L-B/L-D（当該親の実測なし）／NOT_RUN|指定入力・状態の許否と後始末／並列安全性は未証明|
|EXT-TA-V02：TA分離と復旧<br>TA faultを隔離し他TA/サービスを継続する|TA VM read/write、panic復旧、検証Core canary、32 warmup＋512資源反復|NS/S物理保護、製品Core無改変の検証、無期限リーク不存在|security_tests.c:b2_run_ta_isolation/b2_probe_core_isolation、review.c:supplement_isolation、stress_tests.c:run_stress_tests|EXT-TA-V02／review|**実行可能**：parent 13はTA試験にcanary/statsを補足。overlayとB2_VALIDATION_PTA=1必須|共通起動条件⑤。overlayを省けば親BLOCKED、VA aliasが8回残れば子BLOCKED|L-B/L-D（当該親の実測なし）／NOT_RUN|指定VA fault＋victim非改変＋復旧・統計範囲／物理保護ではない|
|EXT-TA-V03：署名TA実ロード<br>実ローダで正常受理・改変拒否を確認|8ケース、最小RPC、Core署名/digest監査、Closeとlive0|Early TAのファイル署名検証、Secure Boot、rollback、Storage|signed_ta_tests.c:b2_run_signed_ta、client/signed_rpc.c:signed_rpc_call/load_ta、decode_signed_results.py:decode_signed|EXT-TA-V03（selection16）；reviewでは子TEE-04を実行・継承|**実行可能**：専用UUID、署名profile、bundle、監査PTAを保持。単独はselection16|公開鍵/Core/bundle一致、fresh Core監査、実機証拠⑤|L-B/L-D（当該親の実測なし）／NOT_RUN|通常TAの実署名/digest経路／boot認証ではない|
|EXT-BOOT-V01：認証付き起動<br>起動イメージの真正性を検査|SKIP理由の記録だけ|認証bootチェーン構築|review.c:b2_run_review|EXT-BOOT-V01／review|**今回対象外**：認証構成なし②、方針④。parent15はSKIP|別の鍵/boot認証設定が必要|選択時のSKIP/BLOCKED分岐を静的照合／NOT_RUN|JTAG起動で合格にしない|
|EXT-STORE-V01：永続保存<br>保存情報の保護を確認|SKIP理由の記録だけ|REE/RPMB保存、改ざん・再起動保持|review.c:b2_run_review|EXT-STORE-V01／review|**今回対象外**：Storage/RPC backendなし②、方針④。parent16はSKIP|Storage基盤を別途用意する必要|選択時のSKIP/BLOCKED分岐を静的照合／NOT_RUN|今回合格判定なし|
|PERF-01：Secure往復時間<br>指定SMC区間を測定する|fast GET_SHM_CONFIG往復、16 warmup/128 sample/128 baseline|TA本文時間、速度閾値|tests/main.c:benchmark_transport、benchmark.c:benchmark_run_scoped|PERF-01／review|**実行可能**：parent17が8系列を実測し集計。単独でも当該1系列だけにはならない|共通起動条件⑤。breakpointなし・RAMログ欠落なしが必要|L-B/L-D（当該親の実測なし）／NOT_RUN|測定区間完了／速度要件への合格ではない|
|PERF-02：メモリ時間<br>Normal SHMへの指定store区間を測定する|64×u64 store＋range照会＋DSB、測定外読戻し/zero|Secure/全DDR帯域、速度閾値|tests/main.c:benchmark_shared_memory/verify_shared_memory|PERF-02／review|**実行可能**：parent18も8系列を実測。NS WB cache条件を保存|共通起動条件⑤。timer HzとCPU Hzを混同しない|L-B/L-D（当該親の実測なし）／NOT_RUN|指定アクセス区間の測定／メモリ全性能ではない|
|PERF-03：割込み応答時間<br>Secure割込み区間を測定する|BLOCKED理由の記録だけ|Secure SGI実測|review.c:run_secure_sgi|PERF-03／review|**今回対象外**：SGI PTAなし①、ZCU接続なし②、方針④|TZ-SYS-06-V01と同じ。通常8系列にこの測定は入らない|選択時のSKIP/BLOCKED分岐を静的照合／NOT_RUN|今回測定完了なし|
|PERF-04：DMA性能<br>DMA転送時間を測定する|SKIP理由の記録だけ|DMA性能測定|review.c:b2_run_review|PERF-04／review|**今回対象外**：backendなし①、方針④。parent20はSKIP|DMA転送backend/実機経路が必要|選択時のSKIP/BLOCKED分岐を静的照合／NOT_RUN|今回測定完了なし|
|PERF-05：TA等の時間<br>TA/暗号/session/復旧の区間を測定する|ECHO/AES/Open/Close/panic回復/観測済TARGET_DEAD回復の6系列|速度閾値、暗号primitiveだけの時間|tests/main.c:b2_run_performance、benchmark.c:benchmark_run_scoped|PERF-05／review|**実行可能**：parent21が8系列を実測し6系列を本親へ対応付け|共通起動条件⑤。panic診断時間等の包含に注意|L-B/L-D（当該親の実測なし）／NOT_RUN|定義した区間完了／速度要件への合格ではない|

## 選択を誤らないための補足

1. `all`は機能13子IDだけ（ECHO、panic、TA identity、SHM、鍵API、TA分離、session、ABI、境界、SMCCC/PSCI版、閉Core session、引数、鍵属性）。親21項目の一式ではない。
2. `review`は13子＋署名TAを実行し、状態保持・token・Core canary/statsを補足、8性能系列を測定する。物理probeは呼ばず、物理親はBLOCKED。対象外親はSKIP又はBLOCKED。
3. `--test TZ-SYS-03-V01`は親のBLOCKED報告で、`--test TZ-SYS-03`が実probeの選択15。reviewで物理保護PASSを得る経路はない。
4. `--test EXT-TA-V03`はselection16で子TEE-04を実行する。親RAMのreview表は生成しない。PC側署名監査と合わせてEXT-TA-V03の証拠とする。
5. `--test PERF-01/02/05`も内部で8系列全部を測る。`performance`はselection18。PERF-03は含まれずBLOCKED。性能の生statusがPASSでもPC表現はMEASUREDで、速度合格を作らない。
6. 111版Makefileは`override B2_SECURE_SGI := 0`。`secure_sgi.c`も含めず、overlay生成も拒否する。TZ-SYS-06-V01/PERF-03は「実機条件待ち」ではなく**今回移送対象外**。実機情報取得だけでは復活しない。
7. legacy経路もcodeに残るが、新結果RAMの通常選択/親集計と同じものではない。legacyのTEE-02/05等の広い親意味を、今回の限定認可/鍵APIの成功へ読み替えない。

## USER_REPORTED：旧版での観測

ユーザーからの報告であり、生ELF/hash/run ID/dumpを本資料で新規照合したものではない。111版のPASSへ転記しない。

|旧版の観測箇所|報告内容|解釈の限界|
|---|---|---|
|test_report入口|TEE-01、TEE-07、TEE-AUTH-TA、TEE-SHM-01、TEE-KEY-API、TEE-ISO-TAがPASS|旧版6子試験の報告値。親21項目や111版全体の実機PASSではない|
|他のtest_report入口|その他10報告はBLOCKED、最後にplatform_haltへ到達|10報告のID/各根拠は今回未取得。残り全IDを推測しない|
|A53経由mrd|translation fault level2|debug読出し経路の観測。BL33の物理保護成立ではない|
|PSU経由mrd|DDR XMPU0〜5読出し成功、CTRL全て0xB、96 CONFIG全て0x8、region Enable=0|個別regionがdisabledとの報告。全保護経路の無効を断定しない|
|XMPU設定・物理保護試験|未実施|111版の限定設定も実XSDB/実機NOT_RUN|

0x10800000〜0x10900000は実機予約が確認されるまで候補。ELF sectionの非重複だけで、FSBL/PMUFW/他masterの実行時使用がないとは認めない。

## 文書照合で判明した制限（実装変更は行わない）

- 起動準備関数は現状必ずready flagで停止する。記載したシーケンスは**ゲートを満たした場合にcodeが通る経路**であり、111版で通過済みの実測図ではない。
- `prepare_xmpu_test.py`のレビュー証跡はファイルhashの一致を検査する。記載内容の正しさや全bank経路の網羅は人のレビュー事項。lock offset/期待解除値も人が公式仕様で確定する。Tclは生成planの証跡hashを再認証しない。編集済みTclを信頼しない。
- 同scriptは既定予約と報告済firmware LOADを検査するが、任意ELFの再取得/実行時予約の自動証明はしない。最新ELFを別の配置検査に渡す必要がある。
- `capture.tcl:tz_dump`は対象をfilter選択して停止状態を要求するが、独自の複数対象件数検査はない（XSDBのfilter選択動作に依存）。新規保存先を明示し、実機でtarget一意性とPA/cache整合性を確認する。
- 111版にあった固定cache/MMU表示は本版で修正した。`benchmark.c:report_memory_state_for_benchmark`はrun開始時にRAMへ採取したSCTLRのM/C/Iからmmu/cache（D-cache）/icacheを表示する。有効な同runの採取記録がなければUNKNOWN。測定時に状態を変更せず、測定ループにも読出しを追加しない。panic診断等の区間包含は従来どおり。
- 外部TF-A/OP-TEE本体は移送物外。別配置ZIPソースの関数を追加照合したが、固定Git commitや移送先のZIPとの同一性を証明していない。来歴はZIP_UNVERIFIED。FSBL/PMUFWの実source/実行状態は今回未照合。
- [試験仕様](b2_test_specification.md)の回収手順まで行わず、test_reportの値だけで署名拒否の層や非改変を立証しない。
