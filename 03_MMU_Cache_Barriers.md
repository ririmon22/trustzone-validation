# 03：仮想メモリ・MMU・キャッシュ・バリア

対象：ZCU106 B2 / TF-A + OP-TEE + Standalone BL33。作成日：2026-11-08。  
確認commit：`70bc1c288c962c3368ca53295a146db690213d8b`。基本原理と対象実装を区別し、実機結果は未再確認。  
[全体案内へ](00_README.md)

## 1. この分野が必要な理由

今回の不具合や判定は「どこへアクセスしたか」「本当に外部保護まで届いたか」「RAM dumpが最新か」を区別しないと説明できない。アドレス変換、属性、cache、順序は一つの機能ではない。

| 機能 | 解決する問い |
|---|---|
| リンカ配置 | コード/データをどのアドレスに置く設計か |
| ロード | 実際にどこへバイト列を配置したか |
| MMU | CPUのVAがどのPAへ行き、アクセス可か |
| memory属性 | 通常RAMかDeviceか、cache等をどう扱うか |
| cache maintenance | どの観測者が最新データを見られる状態にするか |
| barrier | どの処理の順序・完了・設定反映を要求するか |
| XMPU等 | 物理メモリ経路でそのmasterの要求を許可するか |

## 2. VA・PA・アドレス空間

VAは命令が使う仮想アドレス、PAは変換後の物理アドレス。MMU有効時、同じ数値のVAでも別TAのページテーブルでは別PAになり得る。

今回のNormalは必要領域だけVA=PA。数値が同じでもMMUが無いのではない。ページが無ければTranslation Fault、権限不一致ならPermission Fault等になり得る。

`0x60000000に置いた`と聞いたら、ELFのVMAか、JTAG配置PAか、Coreが使うVAかを確認する。単なる16進数だけでは意味が確定しない。

### Stage1とStage2

仮想化構成ではStage1がVA→IPA、Stage2がIPA→PAとなる場合がある。今回物理probeは上位EL2情報を確認し、HCR.VMが有効な状態を受け入れない。NormalのStage1だけ確認しても、さらに変換される環境なら最終PAの証拠が不足するため。

EL1直接入口でEL2情報が未記録なら、Stage2が既知で無効と推測しない。

## 3. ページテーブルの基本

メモリをページ単位で管理し、表のentryに次の表または物理ページと属性を記述する。表自体もRAMにあるデータ。CPUはTTBR等から表をたどる。

今回は4KiBページと64bit descriptor、1表512entry=4096バイト。39bit VAの構成で、index shiftは30/21/12、最下位12bitがページ内offset。

| VAの部分 | 今回の意味 |
|---|---|
| bits 38:30 | 最上位の使用table index |
| bits 29:21 | 中間table index |
| bits 20:12 | ページtable index |
| bits 11:0 | 4KiBページ内offset |

例えばVA 0x10501234はページ先頭0x10501000、offset0x234。恒等写像ならPAも0x10501234。別PA 0x20000000へそのページを写せば同offsetでPA 0x20000234となる。後者は説明用の仮定。

Table descriptorとPage descriptorは同じ意味ではない。entryの型とlevelに応じて解釈する。アドレスbitへ属性bitが混ざるので、PA等を取り出すときは適切なmaskが必要。

## 4. client_mmu.cが作る表

`zcu106_b2_build_page_tables()`は表をゼロにし、root→low/high→ページ表を構築する。root/low/high、client_first/client_second/uartの計6ページ、signed構成ではta_imagesを加え7ページ。

| 対象 | 基本の属性 | 理由 |
|---|---|---|
| code/rodata | Normal cacheable、読取専用、EL1実行可 | コード変更を抑え命令を実行 |
| data/BSS | Normal cacheable、書込可、実行不可 | 状態保存、データ実行抑止 |
| stack | Normal cacheable、書込可、実行不可 | 関数/例外frame |
| SHM | Normal cacheable、書込可、実行不可 | メッセージ・RPC |
| vectors | 専用ページで読取専用、実行可 | 例外入口の実行 |
| UART | Device、実行不可 | MMIOとしての扱い |
| 署名bundle | signed時のみ、読取専用、実行不可 | TA入力データの供給 |
| Secure RAM全体 | 通常のNormal表に含めない | 不要なアクセスを許可しない |

コードは一旦data予約全体をRW/XNへmapしたうえで、vector範囲のdescriptorだけRO/実行可へ変更する。linkerでvectorをページ分離している理由がここにつながる。

`map_region()`は特定2MiB範囲に対応する表を選ぶ限定実装。定数を任意番地へ変えれば自動的に対応する一般マッパではない。既存entryの二重mapや不整列も拒否する。

## 5. 制御レジスタの役割

| レジスタ | 何を決めるか | 今回のポイント |
|---|---|---|
| TTBR0_EL1 | 変換表の先頭等 | client_tablesの基点 |
| TCR_EL1 | 変換幅、粒度、table walk条件等 | T0SZ=25で39bit VAの構成 |
| MAIR_EL1 | 属性indexの意味 | descriptorのAttrIndxからNormal/Device等を選ぶ |
| SCTLR_EL1 | MMU/cacheなど | M=bit0、C=bit2、I=bit12 |
| PAR_EL1 | AT命令による変換結果 | 物理probeでVA→対象PAを確認 |

TCRの入力VA幅と、出力PAの対応幅は別field。今回の定数には39bit VA・40bit PAの設定が含まれるが、すべてのArmで同じ設定値を使えるわけではない。

## 6. MMUを有効化する前提と手順

platform_prepare_tee_clientは、runtime_ready=1、CurrentEL=EL1、入力時M/C/I無効、表領域がデータ予約内などを確認する。確認commitのruntime_readyは0なので、通常経路はその前提確認で止まる。

概念上は以下。

1. 使うcode/data/stack/vectorがすべて有効になる表をRAMへ作る。
2. 表の書込みを完了させる。
3. MAIR/TCR/TTBRを設定する。
4. 古い変換・命令状態等を整理する。
5. 必要なDSB/ISBを経てMMU/cacheを有効にする。
6. SCTLRを読み戻す。

単にSCTLR.Mを1にするだけでは、次の命令fetchやstackアクセスが未写像となり直ちにfaultする。dirty cacheを不用意に捨てる手順も避ける。今回の「入力時無効」の前提はその点にも関係する。

## 7. TLBとは

TLBはVA→PAと権限等の変換結果を保存するcache。PTEを書き換えてもTLBに旧結果が残る可能性がある。データcacheのinvalidateとTLB invalidateは別。

物理probeは限定aliasを作成/削除し、tableの書込み、TLBI、DSB/ISB等を組み合わせる。一般の有効mapping間の置換にはbreak-before-make等の規則が関わり、今回の空きentryへaliasを置く処理を任意の更新へそのまま転用しない。

## 8. キャッシュの基本

CPUのload/storeが毎回外部RAMへ直行するわけではない。cache lineという単位でデータを保持する。write-backでは書込み後の新値がcacheにだけあり、RAM側が古いことがある。

| 状態/操作 | 意味 |
|---|---|
| valid | lineに有効な内容がある |
| dirty | 下位側へまだ反映されていない変更がある |
| clean | dirty内容を指定された到達点へ反映する |
| invalidate | cache内のその内容を無効にする |
| clean + invalidate | 反映してから保持内容を無効化する |

**cleanは消去ではなく、invalidateは書戻しと同義ではない。** dirtyな内容を捨てると更新を失う危険がある。

今回のpublish系は64バイト刻みの処理とaligned(64)を使う。これは対象実装の前提で、全CPUのline sizeが64だという一般法則ではない。

## 9. 共有メモリとJTAGで観測者が違う

Normal CPUとSecure CPU contextが互換なcache属性・coherency条件を使う場合と、JTAGのAPから読む場合は同じとは限らない。同じA53でWorld切替しても、すべてのcache問題が消えるわけではない。

今回clientのsynchronize_messageはDSBを使うが、コメントにもあるようにDSBだけで非coherent経路のclean/invalidateは代替できない。SHM属性、Core設定、実機coherencyの前提を揃える。

`publish_results()`等はDC CVACで記録範囲をPoCへcleanしDSBする。PoCは必要な観測者が整合した内容を得るための到達点で、常に「DRAMチップそのもの」と同義ではない。JTAG読出しの実経路で最新値が見えるかは実機確認事項。

## 10. volatile・compiler barrier・CPU barrier・cache操作

| 仕組み | 主な役割 | 代わりにならないもの |
|---|---|---|
| volatile | 対象アクセスの省略等を抑制 | lock、cache操作、全CPU順序 |
| asmのmemory clobber | コンパイラへメモリ影響を伝える | CPU barrier |
| DMB | 対象accessの観測順序を制約 | cacheの全面書戻し |
| DSB | 指定範囲の同期・完了条件を満たして先へ進む | すべてのobserverに対する自動clean |
| ISB | 後続命令にcontext変更を反映 | 通常data bufferの共有同期全般 |
| DC CVAC等 | cache内容の反映/無効化 | 適切な順序・完了保証のすべて |

「DMBは順序、DSBは対象操作の完了も扱う、ISBは命令実行context」という区別から始める。厳密にはaccess種別・domain・操作による規則があり、「DSBはいつでもすべてのDRAMへ書き戻す」などと省略しない。

SYはsystem範囲、ISHはInner Shareable、STはstore等に関連する限定。意味とobserverを考えず、速そうなbarrierへ置換しない。

## 11. なぜ物理試験はcacheを気にするか

Secure側がpageへpatternを書いても、dirty cacheにしか新値がなければNormalの経路で正しい試験対象を見ていない可能性がある。逆にNormalがRAMを改変しても、Secureが古いcache内容を見続ければ非改変と誤判定し得る。

physical_probe.cは準備時にpatternを書き、clean/invalidateする。検証時もinvalidateして実データを確認する。Normal aliasは別MAIR slotでnon-cacheable属性を使う設計。これはcacheで偽の結果を作らないための条件であり、XMPUが正しく設定された証拠そのものではない。

cacheの属性aliasは一般には危険で、任意RAMを違う属性で二重mapしてよいわけではない。今回の専用領域・単一CPU・対象状態確認という限定を保つ。

## 12. 物理probeの変換確認

AT S1E1R/Wは指定VAについてEL1のStage1変換・権限の確認を行い、結果をPAR_EL1へ残す。実際のload/storeそのものではない。

PARに変換成功と期待PAが記録されても、DDR/XMPUでアクセスが受理された証拠ではない。その後の実ldr/strと、例外/戻り値/被害側の状態を観測する必要がある。

Translation Faultは表が無い/不正など、Permission Faultは表の権限、External Abortは変換後の外部経路の異常などを示す。External AbortだけでXMPUに原因を一意帰属することもせず、専用領域・設定・master経路・レジスタ証拠を結び付ける。

## 13. レビュー想定問答

| 質問 | 回答の核心 |
|---|---|
| MMUがonならVA≠PA？ | 恒等写像もある。今回Normalは必要領域でVA=PA |
| PTEを書いたら即反映？ | TLB、barrier、更新規則がある |
| DSBならcache clean不要？ | 属性/coherency次第。DSB単体でcleanを代替しない |
| volatileでJTAGから新値が見える？ | CPU cacheと観測経路の確認が別に必要 |
| RX LOADなら書込みは失敗？ | ELF属性と生きたPTE/XMPUは別 |
| AT成功なら保護も確認済み？ | 実accessと外部保護の観測は別 |
| 同じPAの異属性aliasは自由？ | 整合条件があり、今回の限定を越えて一般化しない |
| cache invalidateで秘密を安全にゼロ化？ | 内容消去・書戻し・寿命管理は別。invalidateだけではゼロにならない |

## 14. 理解確認

1. 4KiBページでVA末尾0x678は何か。→ページ内offset。
2. PTEを消したのにアクセスできる理由の候補は。→旧TLB等を確認する。
3. RAM dumpが前runの値に見える場合は。→run/hash/回収計画/完了/cache経路を照合する。
4. 許可されないloadがTranslation Faultで終わった。XMPU PASSか。→その証拠だけでは言えない。
5. 同じVAをTA-A/Bが使うと同じ秘密になるか。→別の仮想空間なのでPAは別になり得る。
6. codeだけmapしてMMUを有効にしてよいか。→stack、data、vector、表参照など必要範囲も確認する。

完成条件：client_mmu.c、physical_memory.c、publish_resultsとPTAのcache操作を、変換・属性・cache・順序・観測者に分けて説明できる。

## コード根拠と一次資料

### 今回のソース

- [platform/zcu106/b2/client_mmu.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/platform/zcu106/b2/client_mmu.c)
- [platform/zcu106/b2/client_mmu.h](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/platform/zcu106/b2/client_mmu.h)
- [platform/zcu106/b2/memory_map.h](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/platform/zcu106/b2/memory_map.h)
- [tests/validation/physical_memory.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/validation/physical_memory.c)
- [secure_world/pta/physical_probe.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/secure_world/pta/physical_probe.c)
- [tests/validation/functional.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/validation/functional.c)
- [normal_world/client/client.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/normal_world/client/client.c)
- [runtime/benchmark/benchmark.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/runtime/benchmark/benchmark.c)

### 基本原理を確認する資料

- [Arm：Memory Systems, Ordering, and Barriers / barrier scope](https://support.arm.com/documentation/102336/0100/Limiting-the-scope-of-memory-barriers?lang=en)
- [Arm：Memory access ordering / DMB・DSB・ISB](https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/memory-access-ordering-part-3---memory-access-ordering-in-the-arm-architecture)
- [GCC：Volatiles](https://gcc.gnu.org/onlinedocs/gcc/Volatiles.html)
- [OP-TEE 4.5.0：Core memory管理とSHM](https://optee.readthedocs.io/en/4.5.0/architecture/core.html)

外部資料は版や対象CPUで記述が異なる場合がある。AAPCS64の新拡張、一般OS構成、別SoCの設定をCortex-A53の今回構成へ無条件に適用しない。ここに記載した実装上の数値・判定は固定commitのコードを根拠とする。

次：[第04冊](04_OPTEE_ABI_SHM_RPC.md)
