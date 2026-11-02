# 参照専用。旧外部Clang手順の抜粋。現行Makeからincludeしない。
llvm-runtime: build/toolchain-runtime/libbuiltins.a
build/toolchain-runtime/libbuiltins.a: third_party/compiler-rt/compiler-rt/lib/builtins/udivti3.c third_party/compiler-rt/compiler-rt/lib/builtins/udivmodti4.c
	test -f third_party/compiler-rt/compiler-rt/lib/builtins/udivti3.c
	mkdir -p build/toolchain-runtime
	clang --target=aarch64-none-elf -O2 -ffreestanding -fno-builtin -mgeneral-regs-only -Ithird_party/compiler-rt/compiler-rt/lib/builtins -c $(word 1,$^) -o build/toolchain-runtime/udivti3.o
	clang --target=aarch64-none-elf -O2 -ffreestanding -fno-builtin -mgeneral-regs-only -Ithird_party/compiler-rt/compiler-rt/lib/builtins -c $(word 2,$^) -o build/toolchain-runtime/udivmodti4.o
	$(LLVM_AR) rcs $@ build/toolchain-runtime/udivti3.o build/toolchain-runtime/udivmodti4.o
