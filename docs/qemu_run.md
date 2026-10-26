# QEMU実行（Phase 4確認済み）

QEMU 10.2.1、TCG、Cortex-A53、1CPU、`virt-9.2,secure=on,virtualization=on,gic-version=2`で実行した。versioned virtを使い配置を固定する。QEMU virtは実ZCU106の周辺機器モデルではない（[QEMU公式virt説明](https://www.qemu.org/docs/master/system/arm/virt.html)）。

```sh
make all
bash scripts/run_qemu.sh baremetal
# TF-A/OP-TEEの固定ソースを配置した後
bash scripts/run_phase4.sh
bash scripts/run_qemu.sh tfa
bash scripts/run_qemu.sh optee
```

Aは独自flash bootstrapと3 ELF、B1/B2はTF-A BL1/BL2/BL31/FIPを使用する。B2はOP-TEEとEarly TAを含む。B1/B2では `prepare_qemu_firmware.py` が毎回各ELFのentry/PT_LOAD、予約範囲、ELF→bin変換一致、FIP payloadの完全一致、OP-TEE image headerを検査してflashを作る。BL1の初期化dataだけはROM LMAとSecure RAM VMAが異なる正規配置として扱う。AとBを同時ロードしない。

`build/qemu/{baremetal-rom,tfa_flash,optee_flash}.bin`を`-bios`へ指定する。全構成の共通引数はREADME参照。B2は `-serial file:results/qemu/optee-secure.log` を追加してSecure UARTも保存する。

出力は `results/qemu/{baremetal,tfa,optee}.log`、解析結果は同名JSON。A/B1は30秒、B2は性能測定を含むため90秒以内のEND、必要なID、集計一致、FATALなしを要求する。BLOCKED/SKIPはPASSへ変換しない。PERF-01/02/05/06の統計は構成別CSV。QEMU counterによる性能値は実機性能ではない。

以前のgeneric loaderコマンドはSecure RAMへのpayload配置に失敗しtimeoutしたため使用しない。flash bootstrapで正常動作した実ログは `results/phase4`。

ネットワークbackend、GDB server、VNC、SPICEは指定しない。QEMU不在やソケット制限の切り分けは [診断手順](qemu_diagnosis.md)。

Phase 6の最新scriptは `scripts/run_phase6.sh`、ログは `results/phase6/qemu`。TA/Core分離を含む8 PASSを維持し、長いpanic復帰系列を含む。

Phase 7は長いTEE反復を含むためB2 capture既定240秒（1..300秒で指定可能）。FATAL/END欠落は依然失敗。phase7_environment.mdのscriptでfresh workspaceから実行する。
