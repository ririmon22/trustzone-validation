# 06：試験設計・RAM証拠・デバッグ・性能計測

対象：ZCU106 B2 / TF-A + OP-TEE + Standalone BL33。作成日：2026-11-08。  
確認commit：`70bc1c288c962c3368ca53295a146db690213d8b`。基本原理と対象実装を区別し、実機結果は未再確認。  
[全体案内へ](00_README.md)

## 1. コードの意味を合格条件へ結び付ける

試験とは単に関数を呼ぶことではなく、前提、入力、操作、観測、期待値、後始末を定義して比較すること。

| 段階 | 問い | 今回の例 |
|---|---|---|
| 前提 | 有効な試験を実行できるか | 正しいEL/Core/profile/MMU/SHM |
| 準備 | 何を作るか | session、秘密page、bundle |
| 操作 | 何を試すか | ECHO、不正param、load/store、改変TA |
| 観測 | 何が得られたか | reply、ESR、tag、Core audit |
| 比較 | 何なら合格か | 値・原因・帰属・非改変の一致 |
| 保存 | 後から説明できるか | 名前付きcheck、run ID、raw |
| 後始末 | 元の状態へ戻せたか | Close/free/alias除去 |

期待値を先に定義し、結果が合わないから後付けで「これでもOK」にしない。異なる保護方式の評価が必要なら、要求と判定範囲を明示した別の変更として扱う。

## 2. 状態と合否は別軸

| 表示 | 意味 |
|---|---|
| PASS | 指定試験条件に一致 |
| FAIL | 実行して期待と異なる |
| BLOCKED | 前提・機能・証拠等が不足して妥当な合否を出せない |
| SKIP | 方針等で対象外として実行しない |
| NOT_RUN | 未選択/未実施 |
| RUNNING | 実行/記録が途中 |
| COMPLETE | 記録/実行手順が完了。全試験PASSと同義ではない |
| ERROR | 実行/収集の成立条件に問題 |

FAIL/BLOCKEDを実行したうえで全体がCOMPLETEになることもある。statusとrun stateを一つの数字として読まない。

## 3. 選択番号とprofileの対応

functional.hのselectionを基準に読む。

| selection | 意味 |
|---|---|
| 0 | 基本機能一式 |
| 1〜13 | 各基本機能 |
| 14 | 明示された旧回帰経路 |
| 15 | 専用物理試験 |
| 16 | 専用署名試験 |
| 17 | 正式親項目のreview経路 |
| 18 | 性能経路 |

確認Makeの既定はbasic=14、signed=17、physical=15。つまりbasicの既定値14はmain.cのrun_legacy_suiteへ入る。profile名だけで「通常のfunctional結果バッファを必ず通る」と決めない。最終ELFのb2_test_selectionとmainの分岐を確認する。

profileはビルド機能、selectionは実行する試験経路、review parentは親項目の絞込み。三つを混同しない。

## 4. 名前付きcheckの読み方

b2_check_valueはname/expected/actualを保存する。report statusは試験側が比較結果等を用いて決める。関数呼出しが記録を残したことと、親項目の全要件が満たされたことは別。

例えばexpected=1、actual=1がboolean複合条件の場合、数値だけでは中身が分からない。名前と、その値を作ったCの式を読む。

ECHO試験では正常session、2つの応答値、不正command拒否等を組み合わせる。単一checkの1=1を見て全物理保護までPASSと言わない。

## 5. 段階の記録

PREPARE→OPERATE→EXPECTED_FAULT→COMPARE→SAVE→CLEANUP→DONE等を使う。全試験が必ず同じ順に全段階を通るとは限らない。

EXPECTED_FAULTで止まっているのは「期待fault区間に入った」証拠で、正しいfaultや復旧の完了ではない。SAVEの直後に止めた場合、Close前なら後始末の成立はまだ分からない。

b2_results_endは選択試験のreported、overflow、PASS時のcheck有無、cleanup等を確認する。全体DONE/COMPLETEと個別statusを合わせる。

## 6. 子試験と正式親項目

functional.c等の子結果とreview.cの正式21親項目は粒度が違う。reviewは必要な子を継承し、追加確認を行い、不足した親はBLOCKED等を維持する。

ある要求が「全経路・全masterの物理保護」を求める場合、一つのCPU probeのPASSをそのまま全体へ広げない。要求表の文章、実際の試験入力、観測、集約の条件を対応付ける。

T番号は説明資料の連番、正式IDはコード/結果の識別子。番号順だけで検索するのではなく、対象commitの正式IDと関数名へ結び付ける。

## 7. RAMには何が残るか

| 記録 | 内容 | 主な生成側 |
|---|---|---|
| debug_boot | 起動段階・停止理由・run ID | runtime/起動処理 |
| debug_handoff | EL2情報の既知性・HCR等 | 起動ASM |
| debug_ram_log | 文字列・使用量・dropped・状態 | console.c |
| debug_exception | 最初の致命記録 | capture macro |
| debug_sync_exception | 同期例外の状態 | CAPTURE_SYNC |
| b2_results | 個別試験・段階・check | functional.c |
| b2_review_results | 親項目の根拠と結果 | review.c |
| b2_physical_record | 変換/例外/物理probe記録 | physical_memory.c |
| b2_signed_audit | RPC・case・資源 | signed_rpc/署名試験 |
| b2_signed_loader_snapshot | Core監査のコピー | 署名試験/監査PTA |
| b2_bench_results | 計測条件とサンプル | benchmark.c |
| b2_stress_results | 反復試験の状態 | stress_tests.c |

これら全種類が全selectionで有効な結果になるわけではない。存在するsymbolと、今回実行された記録を分ける。

## 8. ELFとRAM dumpとreport

ELFは位置・size・形式・初期値等を持つ。RAM dumpは実行時の値。reportはそのdumpをPCで解釈したもの。

1. 使用ELFからsymbolとsizeを読んで回収計画を作る。
2. 正しいtarget・停止状態でRAMを取得する。
3. rawを保管し、run IDとELF/Core/bundle情報を対応付ける。
4. デコーダがmagic/version/bytes/状態/overflow等を検査する。
5. 個別check、親review、署名・物理・性能証拠を読む。

**RAMにfilesystemがあってreport.txtを書いている構成ではない。** XSDBがPCへ保存しPythonがテキスト等を作る。BL33のRAMログにCore/TAの全内部ログが自動集約されるわけでもない。

## 9. 同一性と完全性

| 照合 | 防ぎたい取り違え |
|---|---|
| ELF hash | 別compiler/profile/修正版 |
| Core hash/conf | PTA/公開鍵/機能違い |
| bundle hash/manifest | 別TAや改変scenario |
| run ID | 前runや混在dump |
| version/bytes | ABI違い |
| state/phase | 途中取得 |
| dropped/overflow | 証拠欠落 |
| 実ロード記録 | PCで計算したhashと実機imageの取り違え |

hashを記録しただけでは実機imageを認証したことにならない。run IDが一致しても、同じIDで何度も実行したり生成物を混ぜたりすれば区別できない。ロード履歴・新しい回収先・raw保持を一緒に扱う。

## 10. デコーダが失敗したら

まず入力の欠損・版・実行の完了・ELF対応を確認する。デコーダが拒否したからといって、checkを消してPASSになるまで緩めない。

物理/署名の補助デコーダは、期待するレコードsizeや原因、Core監査、bundleとの対応も検査する。文字列RESULT PASSだけを抜き出した集計は根拠を落とす場合がある。

## 11. PC試験と実機試験の範囲

| 検査 | 分かること | 分からないこと |
|---|---|---|
| client_shm_contract | 模擬SMC応答に対する範囲拒否・順序 | 実SMC/Core動作・cache |
| page_tables_equivalence | 基準/整理版が同じ表を作るか | CPUが表を使ったときの実権限 |
| jtag_mock | Tclの引数/順序/拒否 | XSDB/SoCの実挙動 |
| artifact checks | 破損/混在生成物の拒否 | 実機へそのファイルを置いた事実 |
| check_components | ELF/TA/conf/logの静的整合 | 実行時PTE/XMPU、TA実行 |
| 実機probe | 指定経路での動作と結果 | 未試験経路全体の保証 |

静的検査の価値は、投入前に不整合を見つけ、調べる範囲を減らすこと。実機の代替と過小/過大評価しない。

## 12. デバッグの順番

| 段階 | 最初に確認 |
|---|---|
| 入口に来ない | ロード先、最後のPC、e_entry、TF-A handoff、boot mode |
| ASM直後に落ちる | SP、BSS範囲、vector、RAM初期化、EL |
| MMU準備で停止 | ready gate、M/C/I状態、table範囲、mapping構築 |
| client初期化不可 | capability、SHM base/size/cache、Core構成 |
| Open不可 | UUID、TA埋込み、署名profile、RPC、origin |
| Invoke不可 | session、TA command/types、buffer範囲、Core/TA結果 |
| probe異常 | 正しいPC/FAR/ESR、expected_fault、対象VA→PA |
| dump不整合 | symbol計画、run/hash、state、cache/JTAG経路 |

Cのローカル引数はO2で最適化されているため、GUI表示だけで例外時値を決めない。ASM保存レコード、実register、逆アセンブルを照合する。

## 13. 正常対照・拒否・復旧・非改変・後始末

異常系は次の5点で読むと説明しやすい。

1. 正常に許可される操作は成功するか。
2. 対象の不正操作は期待した層と理由で拒否されるか。
3. 被害側の秘密/状態は変わらないか。
4. その後も正常サービスが動くか。
5. session、page、RPC slot、aliasが片付くか。

「一度拒否した」だけでは、全要求を拒否する故障や、拒否のたび資源漏れする状態を見落とす。

## 14. 性能計測：tickはCPU cycleとは限らない

arch.cのcounter_readはCNTPCT_EL0、frequencyはCNTFRQ_EL0。通常の変換は次。

`t_seconds = ticks / counter_frequency`

`time_ns = ticks × 1,000,000,000 / frequency`

例：counter周波数が仮に100MHz、差分250tickなら2.5μs。説明用で実測値ではない。counterはCPU core clockと同一とは限らず、今回ログもcpu_hz=unknownを区別する。

## 15. 測定区間を説明する

benchmark_run_scopedはprepare→時刻取得→operation→時刻取得→cleanup。prepare/cleanupは原則その区間外。operation自身が何を呼ぶかにより、SMC・Core処理・TA実行・診断出力等を含む。

| 対象の呼び方 | 注意 |
|---|---|
| SMC往復 | EL3/Core dispatch等を含む。命令1個の実行時間ではない |
| TA ECHO Invoke | message作成、転送、Core、TA、復帰を含み得る |
| AES API往復 | 鍵生成/操作確保や転送を含むか定義を読む |
| session Open | 既存instanceか新規ロードかで意味が違う |
| panic復旧 | 診断・再生成・ECHO等の含有を確認 |

warmupは初回cache等の影響を区別するため。overheadは測定基盤のコストを知るため。何を差し引いた値か、rawか、統計処理を確認する。最低値だけで一般性能を断定せずmedian/分散・条件を読む。

## 16. 性能と機能の関係

速いFAILを良い性能としない。期待操作が成立したサンプル、count、完了、エラーを確認する。MEASUREDはデータを採れた意味で、仕様上の速度閾値合格とは限らない。

cache/MMU、IRQ mask、Core/profile、compiler/O2、counter周波数、処理区間が異なる値を単純比較しない。割込みマスク下の測定を、そのまま実運用latencyの上限と扱わない。

## 17. stressと資源リーク

反復Open/Invoke/Close、周期的panic復旧、Coreヒープ/TA RAM等の前後情報を組み合わせる。1回成功でも、繰り返すとlive資源が増えて失敗する可能性がある。

資源値が一致しても、すべての漏れがない数学的証明ではない。観測範囲・分解能・試行回数と、機能継続の証拠として説明する。

## 18. 想定問答と理解確認

| 質問 | 回答の核心 |
|---|---|
| COMPLETEなら全部PASS？ | 実行/収集状態と試験statusは別 |
| RESULT PASSだけでレビューできる？ | check、前提、後始末、親要件も必要 |
| run IDがあれば実機同一性は完全？ | hash/ロード履歴/回収条件も照合 |
| mockが通ればJTAG実機もOK？ | 模擬の論理検査と実XSDB/SoCは別 |
| 100tickは100CPU cycle？ | counter周波数に基づく。CPU clockとは限らない |
| Open性能はいつも同じ？ | TA実体の有無、ロード・署名等の区間が違う |
| ERRORになったら判定を緩める？ | 原因の整合を調べ、要求を後付け変更しない |
| cleanup失敗は試験外？ | 残存資源や次試験への影響を含む重要結果 |

練習：子試験PASS、phase=CLEANUP、cleanup_errors=1。→子の比較一致だけではrun正常完了とは説明できず、後始末不備を調査する。

完成条件：任意の試験を1つ選び、要求→選択→関数→入力→check→report→cleanup→raw/decoder→親レビューの順に説明できる。

## コード根拠と一次資料

### 今回のソース

- [tests/validation/functional.h](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/validation/functional.h)
- [tests/validation/functional.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/validation/functional.c)
- [tests/validation/main.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/validation/main.c)
- [tests/validation/review.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/validation/review.c)
- [runtime/test_runner/runner.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/runtime/test_runner/runner.c)
- [runtime/result_report/console.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/runtime/result_report/console.c)
- [runtime/benchmark/benchmark.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/runtime/benchmark/benchmark.c)
- [runtime/include/benchmark.h](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/runtime/include/benchmark.h)
- [runtime/arch.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/runtime/arch.c)
- [scripts/prepare_jtag_capture.py](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/scripts/prepare_jtag_capture.py)
- [scripts/decode_b2_results.py](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/scripts/decode_b2_results.py)
- [scripts/decode_signed_results.py](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/scripts/decode_signed_results.py)
- [scripts/check_components.py](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/scripts/check_components.py)
- [tests/host/client_shm_contract.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/host/client_shm_contract.c)
- [tests/host/page_tables_equivalence.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/host/page_tables_equivalence.c)
- [tests/host/jtag_mock.tcl](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/host/jtag_mock.tcl)
- [tests/host/test_artifact_checks.py](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/host/test_artifact_checks.py)

### 基本原理を確認する資料

- [GNU readelf：ELFの配置・symbolの調査](https://sourceware.org/binutils/docs/binutils/readelf.html)
- [AMD UG1725：XSDB dow](https://docs.amd.com/r/en-US/ug1725-xsdb-reference-guide/dow)

外部資料は版や対象CPUで記述が異なる場合がある。AAPCS64の新拡張、一般OS構成、別SoCの設定をCortex-A53の今回構成へ無条件に適用しない。ここに記載した実装上の数値・判定は固定commitのコードを根拠とする。
