# 次フェーズの実装結果

## 実行環境調査

`results/phase2/environment-diagnosis.json` に実測結果を保存した。
DNSへのUDPソケットと固定IPへのTCPソケットがerrno=1 (EPERM)で拒否され、
環境変数 `CODEX_SANDBOX_NETWORK_DISABLED=1` も確認した。
このセッションのネットワーク隔離が直接の阻害要因である。ホスト全体のDNS障害とは断定しない。
`resolvectl` のsystem bus接続も拒否された。`resolv.conf`、systemd-resolved、apt設定は変更していない。

aptのローカル索引にはQEMU 10.2.1候補があるが、`apt-get download qemu-system-arm` は
名前解決失敗でexit=100。debキャッシュは空。home/tmp/opt/var/cacheのファイル検索では
QEMU実行ファイル/debは見つからず、コンテナ内の `qemu_fw_cfg.h` は実行ファイルではなかった。
既存のClang/LLDを使ってクロスビルドを継続した。

## 実測結果

|対象|結果|証跡|
|---|---|---|
|既存ホスト22試験|成功|results/phase2/baseline-host-tests.log|
|既存10 ELF|BUILD PASS|results/phase2/qemu-build.log, zcu106-build.log|
|追加XMPU/XPPU・検査ライブラリ|AArch64 BUILD PASS|results/phase2/zcu106-drivers-build.log|
|追加22ホスト試験|成功、全44件|results/phase2/host-tests.log|
|QEMU baremetal起動|BLOCKED：未導入、guest未起動|results/phase2/qemu-attempt.log|
|EL3/S-EL1/NS-EL1/SMC/例外/UART|実行未確認|同上|
|Secureメモリ3条件|QEMU BLOCKED、実機NOT_RUN|results/status.json|
|ZCU106準備|BLOCKED：配置・route・backend未確定|results/phase2/hardware-profile.log|
|TF-A/OP-TEE取得|BLOCKED：ネットワーク隔離|results/phase2/fetch-tfa.log, fetch-optee.log|
|TF-A/Core/TA-A/TA-B build|preflight BLOCKED、コンパイラ未起動|results/phase2/external-*.log|

ホストmockで拒否や非改変を検査できても、TrustZoneの保護を実行確認した意味にはしない。
旧10 ELFの再ビルドとは別に、追加ライブラリは `make PLATFORM=zcu106 zcu106-drivers` で生成する。

## ZCU106コードの範囲

`platform/zcu106/protection.c` はDDR XMPU regionのSTART/END/MASTER/CONFIGを生成し、
書込後のreadbackを検査し、有効化を最後に行う。DDRの1 MiB alignmentを要求する。
既存のprotected_bufferは64 KiB単位の暫定配置なので拒否する。勝手に隣接領域を含めて拡張しない。
これは全XMPU設定や全masterの隔離policyを完成させたものではない。
既存CTRL/POISON、無効region、lock確認、route、他master停止を事前に確定する必要がある。
他regionとの競合、default許可、DDRコントローラ応答も含む全体policyの確定は未完了。

XPPUは既存の有効構成のaperture権限を縮小し、Secure-onlyに変更する処理を実装した。
20 master profileとCTRL、変更前aperture値のreadbackを要求する。
既存master一覧の新規生成、全apertureのcold-initは行わない。
parityの符号化方式は今回確定できていないため、検証済みparity込みの入力値を必須にした。
profileが未確認ならレジスタ読出しも行わない。

`hardware_tests.c` は以下の判定手順を実装した。

- CPU/周辺アクセス：通常領域の正例、Secure seed/read、不正read/writeの拒否情報、各段階のSecure非改変、通常領域の再確認。
- DMA：通常copy、禁止read/write、read側出力先の非改変、Secureデータ非改変、違反の帰属確認。
- 割込み：両worldからの両groupを各16回検査し、正しいhandler回数、予期しないhandlerなし、復帰、context維持を要求。

実MMIO adapterは `native_io.c`。ただしこれらの試験を実ボードへ接続するDMA/GIC backend、
例外routingと違反レジスタからのmaster/address/read-write帰属処理は未実装。
そのため完成した実機試験アプリはまだ無い。`board_profile.json` は未確認値をnull/falseのままにし、
`check_hardware_profile.py` は現在必ずBLOCKEDを返す。フラグ変更だけでbackend実装済みとはしない。
既存EL3 monitorの実機停止ガードは維持し、ライブラリを勝手に実行経路へ追加していない。

## TF-A / OP-TEE

固定commitは前フェーズと同一。両プロジェクトを別々に取得しようとして失敗した。
`build_external.py` を追加し、sourceのpin/差分、GNU GCC、TA SDKの有無を検査してからmakeする。
ソースがない場合はBUILD FAILではなくpreflight BLOCKEDを記録する。
出力をbuild/externalへ分離する。Linux/kernelをtargetに含めない。

外部取得が可能なホストでの候補操作：

```sh
python3 scripts/fetch_dependencies.py tfa-amd optee
make external-tfa
make external-optee
make external-ta-a
make external-ta-b
python3 scripts/build_external.py optee \
    --early-ta build/external/ta-a/6dc13418-534f-4642-9138-215876304910.stripped.elf \
    --early-ta build/external/ta-b/6dc13419-534f-4642-9138-215876304910.stripped.elf
```

Early TAの変数名は `EARLY_TA_PATHS`。前フェーズ文書の `CFG_EARLY_TA_PATHS` を修正した。
公式masterのmk/config.mkでELF埋込方式を確認したが、固定4.5.0の実buildは未確認。
これらのコマンドはTF-A/OP-TEE/BL33の配置統合とflash image生成を完了させるものではない。
Standalone message ABI・共有cache属性・必要RPCも引き続き統合が必要。

## 参照資料

- [AMD UG1085 v2.5](https://docs.amd.com/v/u/en-US/ug1085-zynq-ultrascale-trm)：Chapter 16 System Protection Units、XMPU/XPPU。実ボードのroute/master値は未確定。
- [UG1087 R06_START DDR](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/R06_START-XMPU_DDR-Register)：revision 1.11、2025-06-20。DDRのaddress単位。
- [UG1087 R06_MASTER DDR](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/R06_MASTER-XMPU_DDR-Register?contentId=OCC1K~B~7yC0wUMkrtCf6w)：10-bit ID/mask。
- [UG1087 R11_CONFIG DDR](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/R11_CONFIG-XMPU_DDR-Register?contentId=6o_98V0u2nJxyLnR_bZZ4Q)：region enable/read/write/security。
- [UG1087 DDR POISON](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/POISON-XMPU_DDR-Register?contentId=UBeqonMNQxJdddagFkfeHQ)：attribute poisoning条件。
- [UG1087 XPPU CTRL](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/CTRL-XPPU-Register?contentId=GzwOEl_7QWpmg~VNLGFDaA)：permission checking enable。
- [UG1087 APERPERM_000](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/APERPERM_000-XPPU-Register?contentId=t7kuLwXgxGi3MmRflHZ9fw)：64 KiB aperture、profile permission、TrustZone、parityフィールド。
- [OP-TEE mk/config.mk](https://github.com/OP-TEE/optee_os/blob/master/mk/config.mk)：TA architectureとEARLY_TA_PATHS。master確認と固定版確認を区別する。
