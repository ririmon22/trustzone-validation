# 04：OP-TEE・TA/PTA・セッション・共有メモリ・RPC

対象：ZCU106 B2 / TF-A + OP-TEE + Standalone BL33。作成日：2026-11-08。  
確認commit：`70bc1c288c962c3368ca53295a146db690213d8b`。基本原理と対象実装を区別し、実機結果は未再確認。  
[全体案内へ](00_README.md)

## 1. OP-TEEの位置付け

OP-TEEはSecure側でTAの実行環境やサービスを管理する。Normal側へLinuxを要求する概念ではない。今回NormalはStandalone BL33で、必要なclient messageと限定RPCを実装する。

| 名前 | 実行場所 | 役割 |
|---|---|---|
| client | Normal EL1 | TA要求をmessageへ変換 |
| TF-A opteed | EL3 | World切替仲介 |
| OP-TEE Core | Secure EL1 | session、TA、メモリ、検証の管理 |
| user TA | Secure EL0 | アプリサービス |
| PTA | Secure EL1 | Core権限のサービスの呼出し口 |

TAは別CPUではなく、同じA53が権限・アドレス空間を切り替えて実行する。並行呼出し可能性と、このclient実装の単一CPU同期前提も分ける。

## 2. UUID、session、command、instance

| 概念 | 何を識別するか | 今回の例 |
|---|---|---|
| UUID | サービス/TAの種類 | TA-AとTA-Bは別UUID |
| session ID | Coreが管理する接続 | Open成功時に返る値 |
| command | 実行したい操作 | ECHO、READ、PANIC等 |
| instance | 実行中のTA実体 | 複数sessionが同じinstanceを使う場合がある |
| session context | 接続ごとのSecure状態 | authenticated、counter |

「OpenはTAを実行し、Invokeは何もしない」ではなく、Openが接続を用意し、Invokeが指定コマンドを実行する。Create/Destroyはinstance、Open/Closeはsessionの寿命に関係する。

TA-A/BはSINGLE_INSTANCE|MULTI_SESSION。instance内のstatic状態とsessionごとのcontextを区別する。外署名TAは別属性で、Close後の新しい状態を試験する。

## 3. Open→Invoke→Close

1. 閉じたtee_sessionを用意する。
2. OpenでUUIDとPUBLIC login metadataを渡す。
3. transportとCore結果が成功ならid/open/active数を更新する。
4. Invokeでsession ID、command、parameterを渡す。
5. Close成功でNormalのhandle状態も整理する。

clientの最大同時session数4はこの実装の制限で、OP-TEEシステム全体の普遍的上限ではない。

TAのsession_contextはOpenでTEE_Mallocし、Closeでzero化/freeする。Normalの`session.open`をfalseへ書き換えるだけではCore側資源の解放にならない。

## 4. 4種類の結果を読む

clientのtee_replyはtransport/result/origin/value。

| field | 問い | 例 |
|---|---|---|
| transport | client/SMCとして処理できたか | RPC未処理、状態不正等 |
| result | Core/TAの処理結果 | SUCCESS、ACCESS_DENIED、TARGET_DEAD |
| origin | どの層に由来する結果か | TEE=3、TA=4等 |
| value | サービスの返した値 | ECHO値、count、tag |

transportがOKでもTAが拒否するのは普通。不正要求試験では期待拒否が成功条件になる。一方、ACCESS_DENIEDという数値だけが一致しても想定した層が拒否したかはorigin等で確認する。

originはAPIが定めた帰属情報で、暗号的な実機認証ではない。値があることをもってCore/ELFの同一性が自動証明されたとはしない。

## 5. 共有メモリは両者の交換場所

今回SHMは`[0x10500000,0x10600000)`の1MiB予約。messageをこの範囲内へ置き、Coreと同じ構造体形式・cache属性で使う。

初期化はEXCHANGE_CAPABILITIESで予約SHM能力を確認し、GET_SHM_CONFIGでbase/size/cache属性を取得する。Coreの返答も無条件にポインタへ変換せず、範囲・最低容量・整列・属性等を検査する。

### 共有であることと秘密であることは別

SHMはNormal側からも見える。TAの秘密を何でも置いてよい場所ではない。入力は信頼境界を越えるデータなので、型・長さ・範囲・目的を検査する。

Coreが一時的にTAへ見せるmemrefのVAは、Normal側ポインタの数値と同じとは限らない。共有の物理データと、各空間の仮想位置を分ける。

## 6. messageとparameterのABI

message headerはcommand/function/session/cancel/padding/result/origin/count、続いてparameter。parameterはattrとa/b/c。

| 種類 | 主な解釈 |
|---|---|
| VALUE_INPUT | 値を渡す |
| VALUE_INOUT | 値を渡し返す |
| TMEM_INPUT/OUTPUT | bufferのアドレス・長さ等を渡す |
| META | UUIDやlogin等のmetadataとして使う |

同じaフィールドでもattrによって整数かアドレスか意味が違う。param count、types、offsetも含めて検証する。

OpenはOPTEE_MSG_OPEN_SESSION、InvokeはOPTEE_MSG_INVOKE_COMMAND。Invoke時の`message->function`にTAコマンドを入れる。外側message commandとTA commandは別の番号空間。

## 7. ECHOを最初から最後まで追う

1. functional.cのb2_run_serviceが2つのsessionをTA-Aへ開く。
2. tee_invokeへTA_COMMAND_ECHOと0x45a7を渡す。
3. client.cがsession ID、function、VALUE_INOUT、a=0x45a7をmessageへ置く。
4. transport_call→arch.cのSMCでmessage PAを渡す。
5. TF-A→Core→TAのTA_InvokeCommandEntryPointへ届く。
6. TAはcontextとtypesを検査し、ECHOで値を変更せずSUCCESSを返す。
7. clientがreplyを作る。
8. 試験はtransport/result/valueを比較し、不正commandも確認する。
9. sessionをCloseし、結果と後始末を完了する。

ECHOが通るのは要求/応答の基礎。Normal/Secureの全物理保護、認証ブート、鍵の永続保護を一つのECHOで証明することはできない。

## 8. bufferの二重の検査

通常API`tee_invoke_buffer()`はSHM内・非ゼロ・overflowなし・messageと非重複等を検査する。その後Coreも入力を検証する。

`tee_probe_buffer_unchecked()`はCoreの拒否を試すため、Normal側で同じ拒否をかけず不正範囲を要求として渡す専用入口。ただしNormal自身がその不正ポインタをdereferenceする処理ではない。製品用の通常経路と取り違えない。

TA側のBUFFER_CHECKは16バイトのsnapshotへ一度コピーして検査する。共有入力を複数回読んで途中変更の影響を受けにくくするが、コピー中の競合まで一律に原子的に解決するとは限らない。一般の非信頼共有入力は長さ・snapshot・再検査・所有期間を設計する。

## 9. RPC：SecureからNormalへの作業依頼

CALL_WITH_ARGの途中でCoreが必要作業をNormalへ依頼し、Normalが応答して中断した処理を再開する。別PCとのnetwork RPCではない。

今回の署名TAでは、CoreがTAのサイズとデータを要求し、signed_rpc.cが固定RAMのbundleから供給する。RETURN_FROM_RPCで同じOpenを再開する。毎回新しいsessionをOpenし直すと異なる処理になる。

| 要求 | 役割 |
|---|---|
| 引数メモリ確保/解放 | RPC message用slot |
| SHM確保/解放 | TA payload用slot |
| LOAD_TA | UUIDに対応するサイズ/データ供給 |
| 最終復帰 | Open結果へ戻る |

限定実装なので、未知RPCを「0を返せば成功」とは扱わない。未処理の中断処理は資源・状態が不明になり得る。

## 10. 所有権・cookie・poisoned

RPCのallocateは固定SHMの3slotを管理する。cookieにはslot番号と世代を含め、解放済み/別用途/古いcookieの誤使用を防ぐ。byte sizeと出力先も照合する。

releaseはbufferを消去し、slotを空にしてliveを減らす。終了時live=0、alloc=free、unsupported=0等を確認する。これは一般heapのmalloc/freeと実装は違うが、所有者と寿命の管理という原理は共通。

transport_poisonedやRPC poisonedは、安全に再利用できない状態を示す。途中状態を無かったことにして同じhandleで継続すると、別処理を上書きしたり残存資源を見失ったりする。

## 11. TA panicと復旧

TA_COMMAND_PANICでTAが意図的に終了し、CoreがTARGET_DEADを返す。Core自身がpanicした場合とは違う。

tee_invokeはTARGET_DEAD時にCore sessionのCloseを行い、成功ならNormal側handleを閉じる。Close失敗ならpoisonする。試験は新しくOpenし、ECHOや秘密tagが正常なこと、他TAへの影響がないこと、後始末が成立することまで見る。

「戻ってきた」だけで復旧成功とはしない。死んだsession IDの再利用と、新しいsessionの確立を区別する。

## 12. 認証と認可の入口

PUBLIC loginはNormalからの一般要求として扱うもので、OSのユーザーや個々のNormal requesterの強い本人確認が自動で付くわけではない。

TA-Bのtrusted_readはCore管理のgpd.client.identityを取得し、TRUSTED_APPかつTA-AのUUIDだけ許可する。要求parameterへ「俺はTA-A」と整数を入れる自己申告とは違う。NormalでTRUSTED_APPを偽装する要求はCore側の拒否を確認する。

## 13. 想定問答と理解確認

| 質問 | 回答の核心 |
|---|---|
| sessionとinstanceは同じ？ | 接続と実体。属性次第で複数sessionが同じinstanceを使う |
| UUIDを知れば秘密を読める？ | 識別と認可は別 |
| SHMはSecure専用？ | Normalとも共有する交換場所 |
| clientが検査するのでCoreは不要？ | 信頼境界ごとに検査。攻撃者は通常clientを迂回できる |
| RPCが出たら通信失敗？ | 作業依頼。対応できれば同じ処理を再開 |
| heap0なのにallocateできる？ | 固定予約slotを管理している |
| TARGET_DEADでClose不要？ | 今回は残存Core sessionをCloseして寿命を整理 |
| 成功ならoriginは何でもよい？ | 試験が期待する帰属と照合する |

練習：Open成功後、InvokeがTARGET_DEAD、続くCloseが失敗した。→単にopen=falseとして次へ進まず、transportの再利用不可と資源不確定を扱う。

完成条件：ECHO一往復と署名RPCを、UUID/session/message/PA/parameter/result/origin/cleanupの語で説明できる。

## コード根拠と一次資料

### 今回のソース

- [normal_world/client/client.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/normal_world/client/client.c)
- [normal_world/client/client.h](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/normal_world/client/client.h)
- [normal_world/client/signed_rpc.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/normal_world/client/signed_rpc.c)
- [normal_world/client/signed_rpc.h](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/normal_world/client/signed_rpc.h)
- [secure_world/ta/ta.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/secure_world/ta/ta.c)
- [secure_world/ta/security.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/secure_world/ta/security.c)
- [secure_world/ta/user_ta_header_defines.h](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/secure_world/ta/user_ta_header_defines.h)
- [tests/validation/functional.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/validation/functional.c)
- [tests/validation/security_tests.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/validation/security_tests.c)

### 基本原理を確認する資料

- [OP-TEE 4.5.0：Trusted Applications](https://optee.readthedocs.io/en/4.5.0/architecture/trusted_applications.html)
- [OP-TEE 4.5.0：Core](https://optee.readthedocs.io/en/4.5.0/architecture/core.html)

外部資料は版や対象CPUで記述が異なる場合がある。AAPCS64の新拡張、一般OS構成、別SoCの設定をCortex-A53の今回構成へ無条件に適用しない。ここに記載した実装上の数値・判定は固定commitのコードを根拠とする。

次：[第05冊](05_Isolation_XMPU_Cryptography.md)
