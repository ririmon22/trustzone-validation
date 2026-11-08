# 01：低レイヤC・呼出し規約・スタックとヒープ

対象：ZCU106 B2 / TF-A + OP-TEE + Standalone BL33。作成日：2026-11-08。  
確認commit：`70bc1c288c962c3368ca53295a146db690213d8b`。基本原理と対象実装を区別し、実機結果は未再確認。  
[全体案内へ](00_README.md)

## 1. この分野を学ぶ目的

Cの式を読めるだけでなく「どのメモリを、何バイト、誰の権限で触るか」「CとASMが同じ約束で値を渡しているか」を説明する。今回の直接の対象はclientの共有構造体、例外保存frame、固定SHM slot、TAのsession状態。

## 2. 値・アドレス・ポインタ

```c
/* 基礎の説明用。実装そのものではない */
uint32_t value = 7;
uint32_t *p = &value;
*p = 9;
```

`value`はデータ、`&value`は置かれた位置、`p`はその位置を保持する変数、`*p`はそこにある対象。pへの代入は参照先変更、*pへの代入は参照先への書込み。

**数値をポインタへキャストしても、そのアドレスが使えるようにはならない。** 対象の寿命・整列・mapping・権限・物理実在を満たす必要がある。今回のphysical probeはキャストだけでなく、PTEを作り、AT/PARで変換先も確認する。

### 配列とポインタの差

`uint8_t data[16]`は16バイトの配列。`uint8_t *p`はアドレスを保持するポインタで、指し先の容量を自動記憶しない。今回の対象ABIでは通常ポインタは8バイト。

`sizeof(data)`はその配列が見える場所では16だが、関数引数`uint8_t data[16]`は実質ポインタ引数に調整される。そこでsizeofを使って16バイトを知ったつもりにならない。入力にはアドレスと長さを一緒に渡す設計が必要。

### ポインタ演算

`uint32_t *p`の`p+1`は通常4バイト先。`uint8_t *p`なら1バイト先。バイトoffsetを扱うRPCでは、型の大きさによる倍率を取り違えない。配列外のポインタ演算や寿命切れ参照にはC言語の制約もある。

## 3. 整数の幅・符号・桁あふれ

| 型/記法 | 今回意識する意味 |
|---|---|
| uint8_t | 8bit。TAバイト列、UUID、RAM dump |
| uint32_t | 32bit。command/result/origin等 |
| uint64_t | 64bit。message parameter、レジスタ値、アドレス記録 |
| uintptr_t | 対象処理系でポインタ値を保持できる符号なし整数型 |
| size_t | sizeofやメモリ長を扱う処理系依存型 |
| UINT32_MAX | 32bit符号なし整数の最大値 |
| 1UL / UINT64_C(1) | シフト元の型幅を意識した定数 |

64bit CPUだから、すべてのAPI引数が64bitとは限らない。今回のTEE value.aは32bitとして使う場面があり、PTAが返すPAにも表現可能範囲を確認している。64bitポインタをuint32_tへ黙って切り詰めれば別アドレスになる。

### 範囲検査は順序が大切

単純な`address + size <= end`は、加算が折り返すと誤判定する可能性がある。今回のclientは先に基点を検査し、`size <= end - address`の形で長さを比較する。

```c
/* 説明用の範囲検査 */
if (address < base || address >= end)
    return false;
if (size == 0 || size > end - address)
    return false;
```

短絡評価により左から確認する。先にend-addressを計算すると、address>endのとき符号なし減算が折り返すため、順序を省略しない。

符号なし整数の演算は型幅に応じて折り返す。符号付きのオーバーフローはCで一般に未定義動作。型変換・比較の符号も含めて読む。

## 4. ビット演算をレジスタ設定へ結び付ける

| 式 | 意味 | 例 |
|---|---|---|
| 1UL << n | bit nだけ1 | SCTLRのM/C/I等 |
| value & mask | 必要なbitだけ残す | HCRのStage2有効bitを確認 |
| value \| mask | 指定bitを1にする | 既存状態へMMU/cache bitを追加 |
| value & ~mask | 指定bitを0にする | フィールド消去 |
| (value >> shift) & mask | フィールド抽出 | ESRのEC抽出 |
| value ^ mask | 指定bit反転 | 署名TAの改変対照 |

実際の`(esr >> 26U) & 0x3fUL`はESRのECを取り出す。シフト数が型幅以上の場合は安全な演算にならない。`1 << 40`などをintのまま書かない。

## 5. 構造体は通信フォーマットにもなる

client.cは次の形式を共有メモリ上で扱う。

```c
struct message_parameter {
    uint64_t attr, a, b, c;
};
struct message_argument {
    uint32_t command, function, session, cancel_id;
    uint32_t padding, result, origin, count;
    struct message_parameter parameters[2];
};
```

headerは32バイト、parameterは各32バイトなので、この定義の全体は96バイト。`offsetof(parameters)==32`と`sizeof(parameter)==32`を_Static_assertで検査する。単に双方で似たstructを書けばよいのではなく、幅・offset・属性の解釈まで一致させる。

### paddingとalignment

コンパイラは適切な整列のためメンバ間・末尾に隙間を入れることがある。例えば8bitの次に64bitを置くと、64bitの前にpaddingが入り得る。フィールドのsizeofの合計がstructのsizeofとは限らない。

`packed`を付ければ万事解決ではない。ずれたアクセス、CPU制約、ABI不一致等を招く場合がある。今回の固定幅・整列・offset検査を維持する。

### endiannessとUUID

対象ELFはLittle Endian。一方UUIDのテキスト表現、メモリ上のTEE_UUID構造体、OP-TEE message用16バイト列は、同じ見た目のuint32_t列として丸ごと扱ってよいとは限らない。clientのバイト列定義と、ELF側のUUID読解を混同しない。

### バイナリ結果とABI

functional.hの結果RAMもPCのstruct.unpack等が読むフォーマット。フィールド追加・型変更・配列容量変更はバージョン・bytes・デコーダ・回収計画へ波及する。C側がコンパイルできても旧デコーダは正しく読めない。

## 6. 記憶域・寿命・所有者

| 場所 | 典型的用途 | 寿命/管理 | 今回 |
|---|---|---|---|
| レジスタ | 一時値・引数 | 命令と呼出し規約 | x0〜x30 |
| スタック | 関数の作業、退避 | 関数呼出しに対応 | BL33 stack、例外frame |
| .data/.bss | 静的な状態 | イメージの稼働中等 | b2_results、debug_ram_log |
| ヒープ | 動的サイズ・長寿命データ | allocatorとfree | TAのsession_context、CoreのPTA context |
| 固定予約領域 | 通信用buffer等 | 独自の枠管理 | SHM内RPC slot |

`static`ローカル変数は関数を抜けても消えない。通常の自動変数は関数終了後に使えない。ローカル配列へのポインタを返して後で使う設計は寿命切れになる。

今回TAの`TA_OpenSessionEntryPoint()`はTEE_Mallocでsession_contextを確保して`*context`に渡す。Open関数が終了してもsession中は必要なため。Closeで消去してTEE_Freeする。ここをOpen内の普通のローカル変数にすると寿命が足りない。

## 7. スタック・ヒープを今回の配置で説明する

BL33のstackは`[0x10400000,0x10500000)`の1MiB。SP初期値は終端。通常AArch64ではSPを減らして領域を確保する。

```asm
/* 説明用の一般的な関数序盤 */
stp x29, x30, [sp, #-16]!
mov x29, sp
```

16バイト分SPを下げ、FP/LRを保存する。戻るときは対応する復元を行う。全関数が必ずこの形という意味ではない。

BL33の一般heapはstart=endで0。ログや結果はBSS等へ静的確保し、RPCは固定SHM slotを管理する。**ヒープ0は「全部stack」でも「Core/TAもmalloc不可」でもない。**

stack不足は深い再帰、大きなローカル配列、例外frame等で起こる。heapを0にしただけでstack overflowや全メモリ破壊が防げるわけではない。

## 8. AAPCS64：CとASMの約束

通常の基本PCSで整数/ポインタ引数は主にx0〜x7、小さな整数等の戻り値はx0等を使う。構造体戻り値は大きさ等で異なり、呼出し元の結果領域をx8で渡す場合もある。**すべての関数がx0だけで返すとは思わない。**

| レジスタ | 基本の役割 |
|---|---|
| x0〜x7 | 引数/結果、呼出しで変更され得る |
| x8 | 大きな戻り値の間接結果位置等 |
| x9〜x15 | 一時レジスタ |
| x16/x17 | linker veneer等でも使われる一時用途 |
| x18 | platform定義の用途。一般コードで無条件に自由とは扱わない |
| x19〜x29 | 基本PCSでcallee-saved |
| x29 | frame pointerとして使う場合がある |
| x30 | link register。BLで戻り先が入る |
| SP | stack pointer。公開呼出し境界等で16バイト整列 |

callee-savedは「呼ばれた関数が、使うなら元に戻す」約束。caller-savedは「必要なら呼ぶ側が退避」。x30はBLで変わるため、さらに別関数を呼んで元へ戻る関数は自分の復帰先を保つ必要がある。

state_call.Sはx19〜x28へsentinelを置き、x29/SPを使って保持を検査し、実際にtee_invokeを呼ぶ。x0〜x18やNZCV全保持を試しているわけではなく、浮動小数点状態も対象にしていない。通常の関数ABIとSMCCCのレジスタ約束も区別する。

## 9. `static`・`extern`・`const`・`volatile`

| 指定 | 分かること | 分からない/保証しないこと |
|---|---|---|
| file-scope static | その翻訳単位に名前を限定等 | Secure権限や暗号的秘密性 |
| extern | 別の場所に定義される対象を宣言 | 実体がリンク済みか |
| const | そのCアクセス経由で変更しない型上の制約 | 物理的書込禁止 |
| volatile | 処理系規則に従いアクセスの省略等を抑える | 原子性、排他、cache clean、CPU全体の順序 |

`volatile const uint32_t b2_test_selection`はビルド設定の観測等に使う。volatileならデバッガで定数を書き換えてよい、という意味ではない。

## 10. インラインASMとattribute

```c
__asm__ volatile("dsb sy" ::: "memory");
```

ASM本文、出力、入力、clobberをコロンで区切る。`"memory"`はコンパイラへメモリ影響を伝える。CPUへのbarrier命令そのものとは別の役割。空ASM＋memory clobberだけでCPUのDSBにはならない。

`smc_call()`はx0〜x7の入出力を示し、変更され得る一時レジスタ、memory、ccを宣言する。宣言不足はO2等でコンパイラが誤った前提を使う原因となる。

| 属性 | 今回の用途 |
|---|---|
| aligned(64) | 記録先頭をcache line単位へ揃える |
| aligned(4096)等 | ページテーブル整列 |
| weak | 既定実装を強い定義で置き換える |
| _Noreturn | 関数が通常復帰しないことを伝える |
| _Static_assert | sizeof/offsetof等をコンパイル時検査 |

weakなb2_handle_phys_faultはruntimeの既定では回復不可。物理試験側の強い定義がリンクされた構成で処理が置き換わる。

## 11. レビューで聞かれたら

| 質問 | 回答の核心 |
|---|---|
| ポインタを渡せばSecureが読める？ | VAはアドレス空間依存。共有memrefをCoreが検証・変換する |
| staticなら秘密？ | Cの名前/寿命の規則。アクセス制御は別 |
| volatileなら同期できる？ | 排他・順序・cache整合は別。今回単一CPU同期呼出しという前提も重要 |
| heap0でTA sessionを作れる？ | BL33 heapとTA heapは別。TAはTEE_Mallocする |
| 同じstruct名ならABI一致？ | 型幅・padding・offset・endian・versionまで一致が必要 |
| アドレス加算の前に何を見る？ | 基点・容量・整列・overflow・所有範囲 |
| 関数呼出しで全registerを保存？ | 通常ABIのcallee/callerと、例外保存の目的は異なる |
| memcpyはどこ？ | runtime/memory.c。clear_bssはconsole.c |

## 12. 理解確認

1. `uint64_t *p`のp+1は何バイト先か。→8バイト。
2. `uint32_t a[4]`とその配列を受ける関数の引数でsizeofは同じか。→一般に同じではない。
3. 0x104ffff0へ下げたSPはどの予約内か。→BL33 stack。
4. `struct tee_reply`が複数fieldでも、戻り値は必ずx0一つか。→違う。ABIの分類を確認する。
5. shared_endより大きいaddressで先に減算してよいか。→基点を先に検査する。
6. `*p`の読み取りを実行できたらpの所有者も正しいか。→寿命・認可は別に確認する。

完成条件：clientのstruct、範囲検査、state_call.S、TAのsession確保/解放を「幅・寿命・所有者・呼出し規約」で説明できる。

## コード根拠と一次資料

### 今回のソース

- [normal_world/client/client.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/normal_world/client/client.c)
- [normal_world/client/client.h](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/normal_world/client/client.h)
- [tests/validation/state_call.S](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/validation/state_call.S)
- [runtime/arch.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/runtime/arch.c)
- [runtime/memory.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/runtime/memory.c)
- [runtime/result_report/console.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/runtime/result_report/console.c)
- [runtime/include/debug_log.h](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/runtime/include/debug_log.h)
- [secure_world/ta/ta.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/secure_world/ta/ta.c)
- [platform/zcu106/b2/linker_bl33.ld](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/platform/zcu106/b2/linker_bl33.ld)

### 基本原理を確認する資料

- [Arm AAPCS64：基本PCSのregister・stack・結果受渡し](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst)
- [GCC：Extended Asm](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html)
- [GCC：Volatiles](https://gcc.gnu.org/onlinedocs/gcc/Volatiles.html)

外部資料は版や対象CPUで記述が異なる場合がある。AAPCS64の新拡張、一般OS構成、別SoCの設定をCortex-A53の今回構成へ無条件に適用しない。ここに記載した実装上の数値・判定は固定commitのコードを根拠とする。

次：[第02冊](02_EL_Exceptions_SMC.md)
