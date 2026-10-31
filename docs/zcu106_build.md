# ZCU106/B2 ソースビルド

## 前提と外部入力

Ubuntu 24.04＋Vitis 2025.1を最終対象とする。Vitis/GNU経路はこの開発環境ではNOT_RUN。移送物は自作sourceと必要な設定だけ。Docker、QEMU、ホスト回帰、ZIP検査は不要。外部sourceとtoolchainは別に準備する。自動download/clone/pipは行わない。

|入力|用途・参照版|
|---|---|
|TF-A AMD|BL31/opteed、xlnx_rebase_v2.12_2025.1|
|OP-TEE OS|Core、TA SDK、公式署名ローダ、4.5.0|
|pyelftools source|Core buildのELF処理、0.32|
|compiler-rt source|Clang構成のAArch64 builtinsだけ、21.1.8。GNUでは不要|
|XSAと対応embeddedsw/FSBL/PMUFW|実機起動用。来歴・runtime予約を照合する|

reference commit/repositoryは`third_party/dependencies.lock.json`。ZIP展開物は.git不要だが、固定commit確認済みとは扱わない。archiveがあればconfigへ指定してSHAを記録する。directoryだけでも構築でき、来歴はZIP_UNVERIFIEDのまま。元の展開sourceは変更せず、新規workspaceへコピーしてbuildする。

必要なhostツールはGNU Make、Python 3、AArch64 GCC/binutilsまたはClang/LLD/LLVM binutils、公式OP-TEE署名処理用Python cryptography。pyelftoolsは展開sourceで供給する。Vitis付属ツールの場所・版は実際に確認し、付属を推測しない。QEMU/FIP用のOpenSSL開発headers検査やホスト回帰用clang/Tclshは要求しない。

## 操作

1. `config/b2.example.json`を`config/review.local.json`へコピーする。
2. 各source directoryを指定する。相対パスはconfigのdirectory基準。compiler/cross_compile/llvm_dirと固有run_idを記入する。configのplatformはzcu106だけ。qemu/host_tests/secure_sgi開発profileは選択できない。
3. 事前確認する。
   ```bash
   python3 scripts/field.py b2-check config/review.local.json
   ```
   不足するtool/module/sourceは具体的に表示し、BLOCKEDで止まる。入力ZIPの構造確認はbuild互換性のためであり、移送ZIPの包装検査ではない。
4. 一括buildする。
   ```bash
   python3 scripts/field.py b2-build config/review.local.json
   ```
   SDK→Early TA-A/B→専用UUIDの通常署名TA→正常/改変/別鍵bundle→公開鍵と監査PTA入りCore→SPD=opteed BL31→BL33を生成する。CROSS_COMPILE/CROSS_COMPILE64/CROSS_COMPILE_ta_arm64を全て明示する。
5. 表示された`results/zcu106-build/<run>/`を確認する。source-inputs、preflight、source-hashes、result.jsonとworkspace/results/buildの各make logを保存する。BL31/BL32/BL33のentry・LOAD・BSS、予約、SHM/cache、Early TA payload、署名公開鍵、専用物理候補の照合が実施される。

`build_workspace.py`は実際のsource directoriesとMakefile・lockを新しいbuild領域へ複製する。包装用manifest、ホストtests、Docker設定を読み込まない。

## 試験を選ぶ

```bash
python3 scripts/select_b2_test.py --config config/review.local.json --test all
python3 scripts/select_b2_test.py --config config/review.local.json --test EXT-AUTH-V01
python3 scripts/select_b2_test.py --config config/review.local.json --test EXT-TA-V03
python3 scripts/select_b2_test.py --config config/review.local.json --test review
```

allは機能13件、reviewは21親項目の集計で、未確認・対象外を含む。既存legacyは比較用の選択として保持する。未選択はNOT_RUN。選択ELFを、同じsource/profile/公開鍵のCore・bundleと照合する。変更したELFだけを古いCoreへ無条件に組み合わせない。

## Vitisへの取込み

Vitis 2025.1のZCU106 Platform、psu_cortexa53_0、Standalone domainと、対応するFSBL/PMUFWを準備する。実GUI操作はNOT_RUN。

BL33は`optee_standalone/client`、`optee_standalone/tests`、common、`tfa_standalone/integration/start.S`、ZCU106/B2 MMUを用いる。実際のMake commandのC/ASM source、include、-mcpu=cortex-a53、-mgeneral-regs-only、freestanding/-nostdlibと`platform/zcu106/b2/linker_bl33.ld`を照合する。BSP startup・crtbegin・既定リンカを二重追加しない。BL31/Coreを通常Standalone applicationとして再リンクしない。生成Mapと実際のlink commandを証跡にする。

BUILD PASSと静的検査PASSは実機成功ではない。B2_RUNTIME_READY=0、B2_PHYS_REVIEWED=0、boot planのロード禁止を維持する。次に[zcu106_run.md](zcu106_run.md)を読む。
