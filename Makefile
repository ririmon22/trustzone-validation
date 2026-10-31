PLATFORM := zcu106
SOURCE_MODE ?= locked
DEBUG_RUN_ID ?= 1
B2_REVIEW_PARENT ?= 0
B2_PHYSICAL_DEDICATED ?= 0
B2_VALIDATION_PTA ?= 0
override B2_SECURE_SGI := 0
B2_TEST_INDEX ?= 14
B2_SIGNED_TA_PROFILE ?= 0
TOOLCHAIN ?= clang
CROSS_COMPILE ?= aarch64-none-elf-
PYTHON ?= python3
BUILD := build/$(PLATFORM)
LLVM_DIR ?= $(dir $(realpath $(shell command -v clang)))
LLVM_AR ?= $(LLVM_DIR)/llvm-ar
LLVM_OBJCOPY ?= $(LLVM_DIR)/llvm-objcopy
ifeq ($(TOOLCHAIN),clang)
TARGET_AR := $(LLVM_AR)
TARGET_OBJCOPY := $(LLVM_OBJCOPY)
else
TARGET_AR := $(CROSS_COMPILE)ar
TARGET_OBJCOPY := $(CROSS_COMPILE)objcopy
endif
ifeq ($(TOOLCHAIN),clang)
CC := clang
TARGET_FLAGS := --target=aarch64-none-elf -fuse-ld=lld
else
CC := $(CROSS_COMPILE)gcc
TARGET_FLAGS :=
endif
CFLAGS := -mcpu=cortex-a53 -mgeneral-regs-only -mstrict-align -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -O2 -g -Wall -Wextra -Werror -Wconversion -Wshadow -std=c11 -DLOG_UART_ENABLED=0 -DDEBUG_RUN_ID=$(DEBUG_RUN_ID)UL -DB2_PHYSICAL_DEDICATED=$(B2_PHYSICAL_DEDICATED)U -DB2_VALIDATION_PTA=$(B2_VALIDATION_PTA)U -DB2_SECURE_SGI=$(B2_SECURE_SGI)U -DB2_REVIEW_PARENT=$(B2_REVIEW_PARENT)U -DB2_TEST_INDEX=$(B2_TEST_INDEX)U -DB2_SIGNED_TA_PROFILE=$(B2_SIGNED_TA_PROFILE)U
INCLUDES := -Icommon/include -Iplatform/$(PLATFORM) -Ioptee_standalone/client
LDFLAGS := -nostdlib -static -Wl,--build-id=none
COMMON := common/debug_state.c common/memory.c common/arch.c common/result_report/console.c
EL1 := common/el1_vectors.S common/el1_fault.c common/probe.S
RUNNER := common/test_runner/runner.c
HEADERS := $(wildcard common/include/*.h) common/include/exception_capture.inc platform/$(PLATFORM)/memory_map.h optee_standalone/client/client.h optee_standalone/ta/validation_ta.h optee_standalone/pta/validation_canary.h optee_standalone/tests/functional.h optee_standalone/tests/physical_memory.h optee_standalone/pta/physical_probe.h optee_standalone/client/signed_rpc.h
OPTEE_LINKER := platform/$(PLATFORM)/linker_nonsecure.ld
OPTEE_INCLUDES := $(INCLUDES)
OPTEE_FLAGS :=
OPTEE_PLATFORM_SOURCES :=
ifeq ($(PLATFORM),zcu106)
OPTEE_LINKER := platform/zcu106/b2/linker_bl33.ld
OPTEE_INCLUDES := -Iplatform/zcu106/b2 $(INCLUDES)
OPTEE_FLAGS := -DB2_BENCH_RAM=1 -DTEE_SHARED_CACHED=1 -DRAM_LOG_CAPACITY=131072U
OPTEE_PLATFORM_SOURCES := platform/zcu106/b2/client_mmu.c platform/zcu106/b2/client_mmu.h platform/zcu106/b2/memory_map.h platform/zcu106/b2/memory_map.json
endif

.DEFAULT_GOAL := all
.PHONY: all standalone-optee llvm-runtime
all: standalone-optee
$(BUILD):
	mkdir -p $@
standalone-optee: $(BUILD)/optee_bl33.elf
$(BUILD)/optee_bl33.elf: tfa_standalone/integration/start.S optee_standalone/tests/main.c optee_standalone/tests/security_tests.c optee_standalone/tests/stress_tests.c optee_standalone/tests/boundary_tests.c optee_standalone/tests/extended_tests.c optee_standalone/tests/functional.c optee_standalone/tests/review.c optee_standalone/tests/state_call.S optee_standalone/tests/signed_ta_tests.c optee_standalone/client/signed_rpc.c optee_standalone/tests/physical_memory.c optee_standalone/tests/physical_policy.c optee_standalone/tests/physical_probe.S optee_standalone/client/client.c common/benchmark/benchmark.c $(COMMON) $(EL1) $(RUNNER) $(HEADERS) $(OPTEE_PLATFORM_SOURCES) $(OPTEE_LINKER) | $(BUILD)
	$(CC) $(TARGET_FLAGS) $(CFLAGS) $(OPTEE_INCLUDES) -DB2_BENCH_RAM=1 $(OPTEE_FLAGS) $(LDFLAGS) -Wl,-T,$(OPTEE_LINKER) -Wl,-Map,$@.map $(filter %.c %.S,$^) -o $@
llvm-runtime: build/toolchain-runtime/libbuiltins.a
build/toolchain-runtime/libbuiltins.a: third_party/compiler-rt/compiler-rt/lib/builtins/udivti3.c third_party/compiler-rt/compiler-rt/lib/builtins/udivmodti4.c
	$(PYTHON) scripts/source_snapshot.py --source-mode $(SOURCE_MODE) compiler-rt
	mkdir -p build/toolchain-runtime
	clang --target=aarch64-none-elf -O2 -ffreestanding -fno-builtin -mgeneral-regs-only -Ithird_party/compiler-rt/compiler-rt/lib/builtins -c $(word 1,$^) -o build/toolchain-runtime/udivti3.o
	clang --target=aarch64-none-elf -O2 -ffreestanding -fno-builtin -mgeneral-regs-only -Ithird_party/compiler-rt/compiler-rt/lib/builtins -c $(word 2,$^) -o build/toolchain-runtime/udivmodti4.o
	$(LLVM_AR) rcs $@ build/toolchain-runtime/udivti3.o build/toolchain-runtime/udivmodti4.o

