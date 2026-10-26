# QEMU向けビルド

```sh
make all
make host-test
```

5個の自作ELFをClangのAArch64 bare-metal targetでビルドする。
`-mcpu=cortex-a53 -mgeneral-regs-only -mstrict-align -ffreestanding`を使用し、
警告は `-Wall -Wextra -Werror -Wconversion -Wshadow`。
リンク時はlibcなし、例外ベクタ2048バイト境界、stack 16バイト境界を使用する。
ELFとmapは `build/qemu/`、実ログは `results/build/`。

`make TOOLCHAIN=gcc CROSS_COMPILE=aarch64-none-elf-` は未実行。
外部TF-A/OP-TEE/SDK/Early TAは `scripts/run_phase4.sh` でビルド・実行する。固定ソース配置と依存は各構築手順を参照。Clang21.1.8構成で確認済み。
ClangでBL33だけビルドできることは、GNUツールチェーンでTF-A全体がビルドできることを意味しない。
