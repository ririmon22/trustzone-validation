# QEMUの切り分け

Phase 4では `/usr/bin/qemu-system-aarch64` 10.2.1が存在し、実際に3構成を起動した。Phase 3の「未導入」は過去状態である。今回パッケージ導入やホスト設定変更は行っていない。

`results/phase4/qemu-diagnosis.json` に実行ファイル、version、socket作成結果、seccomp等を保存した。AF_INET STREAM/DGRAMはこの実行プロセスでEPERM、AF_UNIXは利用可能。どのfilter規則が拒否したか、通常ホスト端末にも同制限があるかは未確認。DNS設定やホスト全体のセキュリティ不具合とは断定しない。

```sh
python3 scripts/qemu_diagnose.py --smoke --output results/phase4/host-qemu-diagnosis.json
bash scripts/run_host_qemu.sh
```

独自パスは `QEMU_SYSTEM_AARCH64=/absolute/path/qemu-system-aarch64`。smokeは `-S` で停止中のQEMUが生存することだけを確認し、機能PASSを付けない。機能結果は `run_qemu.sh` のUARTと解析JSONを使用する。

|切り分け|観測／次の確認|
|---|---|
|実行ファイルなし|`command -v` と `--version`。通常Ubuntuホストでは `apt-cache policy qemu-system-arm`、`ls /var/cache/apt/archives/qemu*.deb`、必要なら管理者が `sudo apt install qemu-system-arm`|
|引数／machine不一致|`-machine help`、`-machine virt-9.2,help`、startup-stderr、versionを保存|
|このプロセスのsocket制限|診断scriptのAF_UNIX/AF_INET結果、`/proc/self/status`を比較|
|ホスト全体の禁止|同scriptを通常ホスト端末で実行して比較するまではUNKNOWN|
|起動したがguestが進まない|smokeとguest ENDを区別し、ELF配置・初期EL・UARTを調べる|

ネットワークなしの実行例はREADME参照。seccomp、LSM、firewall、DNS等を無断変更しない。Aでの旧loader timeoutはsocketエラーではなくguest起動経路の問題として保存している。
