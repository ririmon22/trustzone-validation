# Phase 4 実行・統合報告

記録日：2026-10-26。ログに基づきBUILD、QEMU、HARDWAREを区別する。全機能完成とはしない。

## 1. ビルド結果

既存QEMU/ZCU106向け各5 ELFは警告をエラー扱いにした設定でビルド成功。ZCU106 backend archiveも維持した。外部統合用BL33を別配置で2 ELF追加した。

- `results/phase4/build-qemu.log`
- `results/phase4/build-zcu106.log`
- `results/phase4/build-external-clients.log`

既存ELFの意味を変更する外部配置の上書きは行わず、`build/qemu/external`へ分離した。現時点のSHA-256とサイズは `results/phase4/artifacts.json`。過去のSHA証跡は履歴として残す。ソース変更を伴うため旧ELFとの全バイト一致を完了条件にはしない。

## 2. ホスト試験

開始時80件成功（`baseline-host-tests.log`）。追加後91件成功（`host-tests.log`）。追加試験は改変ソース、別commit、余分なソース、経路逸脱、FIP破損・範囲逸脱等を拒否する検査の試験。TrustZone/TEE機能結果へ換算しない。

## 3. QEMU機能試験

実行ファイル `/usr/bin/qemu-system-aarch64`、10.2.1を確認。新たなパッケージ導入やシステム設定変更は行っていない。AF_INET STREAM/DGRAMのsocket作成はこのプロセスでEPERM、AF_UNIXは利用可能。ホストOS全体の方針は不明。ネットワークbackendを要求しない設定で実際に起動したため、この制限は本構成のQEMU実行を妨げなかった。証跡：`qemu-diagnosis.json`、`results/qemu/version.log`。

|構成|PASS|FAIL|SKIP|BLOCKED|実行ログ|
|---|---|---|---|---|---|
|A 完全ベアメタル|3|0|1|3|`results/qemu/baremetal.log`|
|B1 TF-A＋Standalone|1|0|1|6|`results/qemu/tfa.log`|
|B2 TF-A＋OP-TEE＋Standalone|2|0|0|6|`results/qemu/optee.log`、`optee-secure.log`|

AではEL3→Secure EL1→Non-secure EL1のUART到達、独立counter、反復SMC切替、register保存、許可されたNSメモリアクセス、Secure領域のNS read/write拒否、期待するData Abortからの復帰、Secure側の非改変再確認と境界検査を実行した。拒否例外だけでPASSにしていない。Secure RAMはQEMU virtの実装に依存し、ZCU106 XMPUの試験ではない。

旧generic ELF loaderではUARTへ到達せずtimeoutした。実測を `baremetal-attempt-01.log` とdebug logへ保存した。flash bootstrapからELF PT_LOADをコピーする方法で起動した。検証済み配置以外はpackしない。

B1はSMCCC/PSCI version取得と未知FIDの応答を検査。TF-AからSecure EL1サービスへの独自試験は未実装のためTZ-SYS-01〜07を一括PASSにしない。

B2は2セッション、正常echo、不正command拒否、closeを実行。TA-Aの制御panicを検出し、TA-Bの継続、TA-A再open/echoを確認した。Secure UARTにTA panicとCore継続の証拠がある。固定tokenの許可/拒否/終了後拒否は `POLICY_SMOKE PASS`、AESランダム鍵生成・往復・既知解も実行成功したが、要求者認証と鍵取得攻撃が未実装なのでTEE-02/05はBLOCKED。Early TA起動は署名付きTAロード検証ではない。

PERF-01/02/05/06を各warmup16、128回測定。min/max/mean/median/p95/p99は `results/qemu/baremetal-benchmarks.csv`、`optee-benchmarks.csv`。generic counter周波数62.5 MHz、CPU実クロックは不明。AはMMU/cache off、B2はon、割込みmask条件と区間はBENCH_METAへ記録。TCGの測定値であり実機性能ではない。

## 4. TF-Aビルド

AMD fork 2.12、commit `dd3512287a17eea866d5c82fa7b86ad58de59163`。ZynqMPの `PLAT=zynqmp RESET_TO_BL31=1 bl31`、QEMU B1、opteed使用B2をビルドした。Clang/LLD 21.1.8とLLVM toolsを使用。

- ZynqMP BL31：`external-tfa-zynqmp.log`、初回の全compile/linkは `tfa-zynqmp-attempt-02.log`
- QEMU B1/B2：`external-tfa-qemu-b1.log`、`external-tfa-qemu-b2.log`

ZynqMP BL31の実entryは0xfffea000、LOAD予約終端は0x100000000（半開区間）。既存DDR予約と一致しない。FSBL/PMUFWのOCM利用、handoff、BSP/XSAがないため実機統合はBLOCKED。QEMU BL33はTF-AのNS_IMAGE_OFFSETに合わせ0x60000000、Aの0x40200000とは別物。

## 5. OP-TEEビルド

upstream 4.5.0、commit `0919de0f7c79ad35ad3c8ace5f823ad1344b4716`。QEMU ARM64 Core、SDK、Early TA組込みをビルドし実際に起動した。ZCU106 portのビルド／TF-Aとの実機互換性は未確認。

`CFG_REE_FS=n CFG_RPMB_FS=n CFG_REE_FS_TA=n`。無Linux、無tee-supplicant。固定予約SHM 0x42000000..0x42200000、NS側identity MMUとNormal WB cache属性を有効化してCoreのcached SHM応答と一致させた。clientは同期value parameterのみ。memref、動的SHM登録、RPC処理、foreign interrupt、SMPは未実装。未知RPCはtransport停止しBLOCKEDへ伝播する。

OP-TEE標準のsoftware PRNG構成をQEMU用に使用しており、実機の安全なentropy源を証明しない。development keyで生成したTA署名ファイルは生成物確認に留める。Coreが表示する構成警告もSecure UARTログへ残した。

`external-optee.log`が最終ビルド、`optee-core-attempt-01〜04.log`にpyelftools不足、compiler runtime不足、実行権限欠落と修正後結果を残した。128-bit除算は固定LLVM compiler-rtの実装をビルドして使用し、ダミー関数で解決していない。

## 6. Early TAビルド

TA-A/BをARM64 SDKで各buildし、両 `.stripped.elf` をEARLY_TA_PATHSでCoreへ組み込んだ。`external-optee-sdk.log`、`external-ta-a.log`、`external-ta-b.log`、`external-optee.log`。実際のTA起動はB2 UART/セッション結果が根拠。

取得ソースは固定commitの**検証済みbuild subset**であり完全git checkoutではない。TF-A1414、OP-TEE2629、pyelftools57、compiler-rt7ファイルをgit blob SHA-1、size、modeで検証。ローカル改変なし。未使用OP-TEE remoteprocの`user_ta.mk`と他boardは含まない。全checkoutに必要なファイルが揃ったと主張しない。manifestは `results/phase4/provenance`、最終検査は `source-verification-final.json`。通常ホストではfull commit checkout/bundleを推奨する。

## 7. ZCU106移植準備

既存XMPU/XPPU/ZDMA/GIC backend、例外probe、1 MiB保護窓/64 KiB bufferの分離とガードを維持。レジスタ根拠・確認項目は [backend](zcu106_backends.md)、[実機引継ぎ](zcu106_phase4_handoff.md)。`zcu106-load-gate.log` と `zcu106-profile-gate.log` は想定通りBLOCKED。実機試験は全NOT_RUN。

## 8〜10. 未完成機能、BLOCKED原因、次の具体的作業

|未完了／原因|次の作業|
|---|---|
|ZCU106 hardware/XSA/BSP/FSBL/PMUFW使用範囲と保護経路なし|実機資料・生成map・設定snapshotを取得、OCMを含む予約とmaster/route/parityを照合、安全ゲートを通す|
|割込み非同期world routingと既存vector/backend接続なし|SCR/GIC/FIQ方針を確定しSecure/NS context保存とhandler復帰を接続する|
|B1 Secure EL1検証serviceなし|標準TF-A dispatcher/serviceとして実装しcustom monitor初期化を混在させない|
|TEE-02要求者認証なし|信頼できるidentityとcredential provisioning設計、認可と分離してnegative試験|
|TEE-03他TA/Coreアクセス試験なし|制御fault、正規アクセス、他TAとCore非改変検査を追加|
|TEE-04署名ロード/RPCなし|通常TAロード用transportと改ざんfixtureを実装しEarly TAと別に試験|
|TEE-05鍵抽出攻撃・削除/再生成検証なし|許可/禁止APIとmemory probeを追加、実機entropyを選定|
|TEE-06/PERF-07 storageなし|永続backend/RPC選定、改ざんと再起動後検査|
|TEE-08 Secure Bootなし|可逆な認証boot試験を設計、不可逆設定は明示承認まで変更しない|
|PERF-03/04実行測定なし|実割込み/DMA経路の機能確認後に測定区間・clock/cache条件を確定|

再現は `bash scripts/run_phase4.sh`。このscriptはsource provenance検証後にbuildとQEMU実行を行う。ZCU106を起動せず保護ゲートも無効化しない。構成別statusは `results/status.json` と `results/report.md`。失敗した過去ログは削除せず区別して保存した。
