# TF-A構築

固定AMD fork commitは `third_party/dependencies.lock.json`。Clang/LLD 21.1.8、host GCC、make、LLVM binutilsで確認した。upstream版とAMD版を混同しない。

```sh
python3 scripts/source_snapshot.py tfa-amd
make qemu-external
python3 scripts/build_external.py tfa --platform zynqmp
python3 scripts/build_external.py tfa --platform qemu --configuration b1
# OP-TEE CoreとEarly TAのビルド後
python3 scripts/build_external.py tfa --platform qemu --configuration b2
```

各commandの実際のmake引数と終了値は `results/phase4/external-tfa-*.log`。LLVM pathはclangの実体から決定し、`--llvm-dir`でも指定可能。GNU版は `--compiler gcc --cross-compile aarch64-none-elf-`、今回未検証。

[TF-A 2.12公式ZynqMP文書](https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/xilinx-zynqmp.html) §7.45の `PLAT=zynqmp RESET_TO_BL31=1 bl31` を使用。ZynqMPではBootROM/FSBLがBL1/BL2に相当し、QEMUのflash/FIP起動手順を流用しない。

ZynqMP生成物：`build/external/tfa-zynqmp/zynqmp/release/bl31/bl31.elf` とrelease直下 `bl31.bin`。実entry0xfffea000、LOAD終端0x100000000。既存のDDR予約案とは異なるため実機ロード禁止。FSBLのhandoff構造とBL33 entry/security/EL1指定、PMUFW、OCM保持範囲、UART選択をVitis生成物で確認する。独自EL3初期化をBL31へ混ぜない。

QEMU生成物：`build/external/tfa-qemu-b{1,2}/qemu/release`。BL33は0x60000000へ別link。B2は `SPD=opteed BL32_RAM_LOCATION=tdram`、OP-TEE v2 header/pager/pageableを指定する。Coreが生成したpageableは空で、FIPにExtra2を持たない構成を検査している。

`prepare_qemu_firmware.py` の検査に成功してからBL1を0x40000までpadしFIPを連結する。BL1/BL2/BL31のSecure RAMとBL32/NS/SHM予約は `platform/qemu/external`。実行方法は [QEMU手順](qemu_run.md)。BUILD PASS、QEMU PASS、HARDWARE NOT_RUNを区別する。
