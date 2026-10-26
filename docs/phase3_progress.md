# Phase 3 実装・検証状況

Phase 3はコードと移植準備を進めたが、QEMU/実機の機能検証は未完了。
ホスト試験をQEMU PASS/HARDWARE PASSへ転記していない。

|要求|今回の結果|残る条件|
|---|---|---|
|QEMU原因切り分け|実行ファイル不在、AF_INET socketはEPERM、AF_UNIX作成可能を区別|通常ホスト端末で比較、QEMU導入|
|socket backendなしの起動|TCG・file/pipe UARTのみのsmokeと機能試験scriptを用意|QEMU不在で起動可否未確認|
|EL3/両EL1/切替/分離/不正access/非改変|実行NOT_RUN、QEMU試験BLOCKED|実guestログが必要|
|XMPU配置|専用1 MiB窓＋内部64 KiB bufferへ分離。既知allocation/firmware競合検査を追加|FSBL/PMUFW/BSP、DDR容量、master/routeが未確定|
|XMPU/XPPU|設定・readback・試験後再検査、ISR/ERR_STATUS取得、32/64-bit実probeを追加|全体policy、実周辺、lock/parity/profile、例外接続の確認|
|DMA|実ZDMA descriptor/転送開始/完了/error/poll timeout取得を実装。判定器へのadapterを追加|実channel/master/Non-secure属性、cache、Secure serviceとbuffer接続|
|GIC|実group/priority設定、SGI、IAR/EOIR、任意EL1 vector/handlerを追加|world routingとEL3非同期context切替、実vector installと復帰確認|
|TF-A/OP-TEE|両pin存在を接続ツールで確認。embeddedswのpinも確定。bundle importerとSDK生成targetを追加|完全source checkout/toolchainが不足。build preflight BLOCKED|
|回帰|自作10 ELFビルド、ホスト80試験成功|実行性能値・実機結果はなし|

## 実ログ

- `results/phase3/baseline-host-tests.log`：既存44試験。
- `results/phase3/host-tests.log`：既存試験を含む80試験。
- `results/phase3/qemu-build.log`, `zcu106-build.log`：自作10 ELFと追加backend archiveのビルド。
- `results/phase3/backends-linkcheck.log`：全backend objectの部分link。残るundefinedは共通runtimeのconsole_write/fatal_exception/memcpy/memset/platform_haltだけ。
- `results/phase3/qemu-diagnosis.json`, `qemu-attempt.log`：QEMU不在とsocket診断。guest実行ログではない。
- `results/phase3/hardware-profile.log`, `zcu106-preparation.log`：実機準備BLOCKEDの理由。
- `results/phase3/external-*.log`：外部componentのpreflight。コンパイラ未起動。
- `results/phase3/source-verification.json`：接続ツールによる固定commitと公式headerの確認。

QEMU用5 ELFは共通ソースを変えず維持した。ZCU106用はbuffer移動により一部ELFのhashが変わるのが正常。
詳細は `results/phase3/summary.json` のhash比較を参照する。

## 導入・継続手順

- [QEMUのホスト切り分け](qemu_diagnosis.md)
- [ZCU106 backendの範囲と接続条件](zcu106_backends.md)
- [オフラインsource/tool導入とTA SDK生成](offline_setup.md)

QEMUがないこととCodexセッションでネットワーク取得ができないことを、QEMU起動設定エラーと混同しない。
実機がないためmaster IDや周辺対象を推測して実行することはしていない。
