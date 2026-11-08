# TrustZone検証：必要知識の学習・レビュー説明セット

## 1. このセットで目指すこと

今回のコードを、用語の暗記だけでなく「誰が、どの権限で、どのアドレスのデータを使い、何を確認して、なぜ合格/不合格/未判定にしたか」まで説明するための資料。

基本原理→今回の使い方→コード上の責任者→間違いやすい点→想定問答→理解確認、の順で分野別にまとめた。既存のリンカ・ローダ資料と合わせると、起動からTA呼出し、物理/暗号試験、RAM回収までつながる。

対象はZCU106 B2、TF-A BL31 + OP-TEE BL32 + Standalone BL33。Normal側にLinuxを置かない構成。確認commitは`70bc1c288c962c3368ca53295a146db690213d8b`。2026-11-08に確認したソースを説明するもので、手元の未push修正や実機ロード済みELFを再判定したものではない。

## 2. 収録ファイルと読む順

| 順 | ファイル | 分かるようになること |
|---|---|---|
| 1 | [01_C_ABI_Stack_Heap.md](01_C_ABI_Stack_Heap.md) | ポインタ/整数/構造体、stack/heap、CとASMの呼出し規約 |
| 2 | [02_EL_Exceptions_SMC.md](02_EL_Exceptions_SMC.md) | ELとWorld、例外ベクタ、保存/復帰、SMC |
| 3 | [03_MMU_Cache_Barriers.md](03_MMU_Cache_Barriers.md) | VA→PA、PTE、権限、cache、TLB、DSB/ISB |
| 4 | [04_OPTEE_ABI_SHM_RPC.md](04_OPTEE_ABI_SHM_RPC.md) | TA/PTA、session、message、SHM、RPC、panic復旧 |
| 5 | [05_Isolation_XMPU_Cryptography.md](05_Isolation_XMPU_Cryptography.md) | 認可、TA/Core分離、物理保護、署名、hash、鍵API |
| 6 | [06_Test_Evidence_Debug_Performance.md](06_Test_Evidence_Debug_Performance.md) | 合否、記録、回収、親項目、デバッグ、性能と反復試験 |

分割ファイルの相互リンクは同じフォルダへ置いた場合に使える。まとめて読む場合はZIPを展開し、この00_READMEから始める。

### 既存資料への入口

- [TrustZone_Folder_File_Guide.md](https://chatgpt.com/library?file_id=libfile_f0bbaad591188191915cea3fa098123a&account_id=personal)：フォルダ/ファイルを探す入口。
- [TrustZone_Linker_Loader_Detailed_Guide.md](https://chatgpt.com/library?file_id=libfile_03b069b860308191913030565f438769&account_id=personal)：コンパイル・リンク・ロード・起動・TAローダの説明。

参照位置の訂正：clear_bss()はruntime/result_report/console.c、memcpy/memsetはruntime/memory.c。前のフォルダ説明のclear_bssの所在は、この区別で読む。

## 3. 今回の流れを知識へ対応付ける

| コードの場面 | 必要な知識 | 該当冊 |
|---|---|---|
| _startでSPを設定 | linker symbol、stack、整列、ABI | リンカ資料 + 01 |
| EL2からEL1へ | EL、ELR/SPSR、ERET | 02 |
| BSS初期化・vector登録 | 静的記憶域、例外入口、配置 | 01/02 + リンカ資料 |
| MMU/cache準備 | PTE、TTBR/TCR/MAIR/SCTLR | 03 |
| client初期化 | capability、SHM契約、overflow検査 | 01/04 |
| ECHOのInvoke | session、message ABI、SMC、result | 02/04 |
| 不正buffer | 範囲・型、Core拒否、snapshot | 01/04/05 |
| TA-A/Bの分離 | アドレス空間、権限、TARGET_DEAD、復旧 | 02/03/04/05 |
| Secure物理pageへのprobe | PA、alias、cache、XMPU、外部abort | 02/03/05 |
| 外署名TAをOpen | bundle、RPC、署名/digest、ELF loader | 04/05 + リンカ資料 |
| 結果を保存・回収 | ABI、cache publish、run/hash、decoder | 01/03/06 |
| 正式21親項目へ集約 | 要求範囲と子試験の証拠 | 06 |
| 性能/反復 | counter、測定区間、条件、資源 | 03/06 |

## 4. 最初に区別する12組

| 混同しやすいもの | 区別 |
|---|---|
| ELとSecurity State | 権限レベルとSecure/Normalは別軸 |
| C関数callとSMC | 普通の呼出しと例外を通るWorld要求 |
| RETとERET | 通常復帰と保存状態に基づく例外復帰 |
| 例外入口とハンドラ | CPUが入るslotと、その後の原因分析処理 |
| 値とアドレス | 数値データとその置き場所 |
| stackとheapとBSS | 寿命・管理方法・用途の異なる領域 |
| ELF配置とMMU権限 | ファイル上の意図と実行中のCPUアクセス制御 |
| MMUとXMPU | CPU変換/権限とSoC物理経路の制御 |
| volatileと同期 | アクセス最適化の扱いと排他/順序/cacheは別 |
| TAとPTA | Secure user modeとCore権限のサービス |
| signatureとdigest/hash | 信頼鍵による認証と内容の整合確認 |
| COMPLETEとPASS | 実行/収集状態と試験合否は別 |

## 5. コードの読み方を固定する

ファイル全体を暗記するより、まず一つの経路を最後まで説明する。

### 経路A：ECHO一往復

`functional.c:b2_run_service`→`client.c:tee_invoke`→`arch.c:smc_call`→外部TF-A/Core→`ta.c:TA_InvokeCommandEntryPoint`→reply→比較→Close。

完成条件：UUID/session/command/message/PA/transport/result/origin/value/cleanupを使って説明する。

### 経路B：意図したメモリアクセス異常

probe準備→変換/権限確認→実load/store→正しいWorld/ELの例外入口→原因保存→限定復帰またはTARGET_DEAD→非改変/継続確認→後始末。

完成条件：TA faultとNormal faultを分け、Translation Faultだけでは物理保護を証明できない理由を説明する。

### 経路C：署名TAロード

bundle入力→Open→CoreからRPC→サイズ/データ供給→Coreの署名/digest検証とldelf→正常Invokeまたは期待拒否→Core監査→資源回収→PC照合。

完成条件：Normalの供給、Coreの信頼判断、TA ELFの実行準備、署名結果の証拠を分ける。

### 経路D：reportの根拠

選択番号/profile→試験関数→名前付きcheck→個別report→cleanup/完了→RAM回収→decoder→親review。

完成条件：「このPASSが何を言えて、何を言えないか」を対象範囲付きで答える。

## 6. 説明用テンプレート

> この試験は【要求/保護対象】を確認します。実行するのは【World/EL/主体】で、【前提】を確認した後、【入力/対象アドレス】へ【操作】します。【応答/レジスタ/記録】が【期待条件】に一致し、【被害側非改変・継続・後始末】まで確認した範囲で合格とします。【未試験の経路や不足証拠】はこの結果では保証しません。根拠は【関数/設定/記録】です。

最初はこの形でECHO、TA分離、物理probe、署名TAを各1回説明する。詰まった用語だけ該当冊へ戻る。

## 7. 学ぶ深さの目安

| 段階 | 到達点 |
|---|---|
| 用語が分かる | 定義を一言で言える |
| コードへ結び付く | 対応file/function/struct/registerを指せる |
| 因果を説明できる | なぜこの検査・barrier・Closeが必要か言える |
| 不具合を切り分ける | どの証拠が不足し、次に何を見るか言える |
| レビューに答える | 合否の根拠と試験範囲の限界を言える |

今回の目標は下の3段階まで。全Arm命令や暗号の数学を丸暗記する必要はなく、この構成が実際に使う概念を根拠へつなげる。

## 8. 読む必要が出たときに追加する分野

| 追加テーマ | 必要になる条件 |
|---|---|
| GIC/IRQ routingの詳細 | Secure割込みやSGI等を実装・検証する場合 |
| DMA/IOMMU/PL interconnect | CPU以外のmasterによる分離を試験する場合 |
| BL1/BL2/FIP/認証boot | 今回のJTAG方式からboot chain検証へ広げる場合 |
| 永続Secure storage/RPMB/rollback | 再起動後の保護・版戻しを扱う場合 |
| 多CPU・lock・atomic | 単一CPU同期clientの前提を外す場合 |
| SError/RASの詳細 | 非同期エラーの帰属・回復を実装する場合 |
| サイドチャネル/投機実行 | 論理アクセス制御以外の漏洩を対象にする場合 |

これらは重要な関連領域だが、現行の未実装機能を実装済みとして説明しない。現行コードの理解と、追加設計の要求を分ける。

## 9. 各冊共通の根拠の読み方

各冊末尾に対象commitへ固定したコードリンクと、基本原理の一次資料を付けた。コード上の値/判定を優先し、一般資料の異なる版・構成をそのまま適用しない。

確認commitのB2_RUNTIME_READY/B2_PHYS_REVIEWEDは0。これは対象ソースの状態で、別の手元版で報告された実機結果を否定する判定ではない。今回この資料作成では実機再試験や新しいELFビルドは行っていない。

## 10. 最終理解チェック

1. BL33、Core、user TA、PTAのWorld/ELを言える。
2. SP、BSS、一般heap、固定RPC slotの違いを説明できる。
3. CPUが保存する例外情報とASM保存の違いを説明できる。
4. VA→PAとTLB、MMUとXMPUを分けられる。
5. volatile、compiler clobber、DSB、cache cleanの役割を分けられる。
6. Open/Invoke/CloseとTARGET_DEAD後の寿命を追える。
7. 正しいidentityと要求parameterの自己申告を分けられる。
8. 署名とhash、鍵の用途と抽出を分けられる。
9. 子試験PASSと正式親項目の要件全体を分けられる。
10. raw/ELF/run/hash/完了と後始末を根拠に説明できる。
11. tickとCPU cycle、測定区間と純粋処理時間を分けられる。
12. 自分の説明がどのcommit・profile・selection・実行証拠を対象にするか明示できる。
