# 開発環境

実測値は `results/build/environment.json`。Ubuntu 26.04.1 LTS x86_64、Clang/LLD 21.1.8。
GNU Make、Python、Git、readelfを使用した。AArch64 GNU GCC、QEMU、Vitis、Bootgen、dtcは未導入。
シェルの外部通信はDNS解決失敗、sudoはno-new-privilegesで利用不可。
この環境ではapt導入や外部ソース取得ができなかった。公式文書調査は別のWeb閲覧経路を使用した。

管理可能なホストで必要に応じて導入する依存の例：

```sh
sudo apt-get update
sudo apt-get install build-essential gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
    qemu-system-arm git python3 python3-cryptography python3-pyelftools \
    device-tree-compiler libssl-dev uuid-dev acpica-tools
```

このaptコマンドは当環境では未実行。ディストリビューションの実パッケージとQEMU機能を確認する。
TF-A/OP-TEEの固定版に追加のPython依存があれば、その版の公式requirementsに従う。
VitisのUbuntu 26.04対応は確認していない。では2025.1のサポートOS上で作業する。

```sh
python3 scripts/environment_report.py
qemu-system-aarch64 --version
qemu-system-aarch64 -machine help
qemu-system-aarch64 -machine virt-9.2,help
qemu-system-aarch64 -cpu help
```

QEMU実装調査基準はupstream v9.2.0。利用実行ファイルの版は未取得。
`virt-9.2`が無い場合は黙って別machineへ変更せず、対応版とメモリマップを再調査する。

次フェーズでソケット作成がEPERMとなることを確認した。ホストDNS設定は変更していない。
詳細とapt取得実ログは [次フェーズ結果](phase2_progress.md) を参照。

## Phase 4の更新（上の初期状態より優先）

QEMU10.2.1は `/usr/bin/qemu-system-aarch64` に存在し実行成功。今回のセッションではパッケージ導入やシステム設定変更をしていない。TF-A/OP-TEEは承認済みGitHub connector経路から固定blobを取得しbuild subsetとして検証した。通常shellのAF_INET socket制限とは別経路であり、ホスト全体のネットワーク禁止とは判断しない。

実使用：Clang/LLD/LLVM tools21.1.8、host GCC、GNU Make、Python3＋cryptography、vendored pyelftools0.32、固定compiler-rtの128-bit除算実装。外部buildではGNU AArch64 GCCを必要としないClang構成を実行した。dtc依存を必要としない構成でOP-TEEをbuildした。Vitis/Bootgen/実機は依然未確認。

再現コマンドと実ログは [Phase 4](phase4_progress.md) と `results/phase4`。
