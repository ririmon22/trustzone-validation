-include local.mk
PLATFORM ?= zcu106
PROFILE ?= basic
# CoreとBL33の追加機能を同じPROFILEで選ぶ。個別指定は対応するCoreと必ず照合する。
ifeq ($(PROFILE),signed)
B2_TEST_INDEX ?= 17
B2_SIGNED_TA_PROFILE ?= 1
B2_VALIDATION_PTA ?= 1
endif
ifeq ($(PROFILE),physical)
B2_TEST_INDEX ?= 15
B2_PHYSICAL_DEDICATED ?= 1
B2_VALIDATION_PTA ?= 1
endif
ifneq ($(filter-out 0,$(B2_SECURE_SGI)),)
$(error Secure SGI is unsupported on ZCU106)
endif
DEBUG_RUN_ID ?= 1
B2_REVIEW_PARENT ?= 0
B2_PHYSICAL_DEDICATED ?= 0
B2_VALIDATION_PTA ?= 0
override B2_SECURE_SGI := 0
B2_TEST_INDEX ?= 14
B2_SIGNED_TA_PROFILE ?= 0
TOOLCHAIN ?= clang
CROSS_COMPILE ?= aarch64-none-linux-gnu-
PYTHON ?= python3
# compiler/profileを分け、空のBUILD_ROOTを指定すれば全て新規に作れる。
BUILD_ROOT ?= build
BUILD := $(BUILD_ROOT)/bl33/$(TOOLCHAIN)/$(PROFILE)
# 設定変更時の取り違えを防ぐため、毎回コンパイルする。
.PHONY: FORCE
FORCE:
ifeq ($(TOOLCHAIN),clang)
CC := clang
TARGET_FLAGS := --target=aarch64-none-elf -fuse-ld=lld
else
CC := $(CROSS_COMPILE)gcc
TARGET_FLAGS :=
endif
CFLAGS := -mcpu=cortex-a53 -mgeneral-regs-only -mstrict-align -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -O2 -g -Wall -Wextra -Werror -Wconversion -Wshadow -std=c11 -DLOG_UART_ENABLED=0 -DDEBUG_RUN_ID=$(DEBUG_RUN_ID)UL -DB2_PHYSICAL_DEDICATED=$(B2_PHYSICAL_DEDICATED)U -DB2_VALIDATION_PTA=$(B2_VALIDATION_PTA)U -DB2_SECURE_SGI=$(B2_SECURE_SGI)U -DB2_REVIEW_PARENT=$(B2_REVIEW_PARENT)U -DB2_TEST_INDEX=$(B2_TEST_INDEX)U -DB2_SIGNED_TA_PROFILE=$(B2_SIGNED_TA_PROFILE)U
INCLUDES := -Iruntime/include -Iplatform/$(PLATFORM) -Inormal_world/client
LDFLAGS := -nostdlib -static -Wl,--build-id=none
COMMON := runtime/debug_state.c runtime/memory.c runtime/arch.c runtime/result_report/console.c
EL1 := runtime/el1_vectors.S runtime/el1_fault.c runtime/probe.S
RUNNER := runtime/test_runner/runner.c
HEADERS := $(wildcard runtime/include/*.h) runtime/include/exception_capture.inc platform/$(PLATFORM)/memory_map.h normal_world/client/client.h secure_world/ta/validation_ta.h secure_world/pta/validation_canary.h tests/validation/functional.h tests/validation/physical_memory.h secure_world/pta/physical_probe.h normal_world/client/signed_rpc.h
OPTEE_LINKER := platform/$(PLATFORM)/linker_nonsecure.ld
OPTEE_INCLUDES := $(INCLUDES)
OPTEE_FLAGS :=
OPTEE_PLATFORM_SOURCES :=
ifeq ($(PLATFORM),zcu106)
OPTEE_LINKER := platform/zcu106/b2/linker_bl33.ld
OPTEE_INCLUDES := -Iplatform/zcu106/b2 $(INCLUDES)
OPTEE_FLAGS := -DB2_BENCH_RAM=1 -DTEE_SHARED_CACHED=1 -DRAM_LOG_CAPACITY=131072U
OPTEE_PLATFORM_SOURCES := platform/zcu106/b2/client_mmu.c platform/zcu106/b2/client_mmu.h platform/zcu106/b2/memory_map.h
endif

.DEFAULT_GOAL := bl33
.PHONY: all standalone-optee
all: standalone-optee
$(BUILD):
	mkdir -p $@
standalone-optee: $(BUILD)/optee_bl33.elf
$(BUILD)/optee_bl33.elf: boot/bl33_start.S tests/validation/main.c tests/validation/security_tests.c tests/validation/stress_tests.c tests/validation/boundary_tests.c tests/validation/extended_tests.c tests/validation/functional.c tests/validation/review.c tests/validation/state_call.S tests/validation/signed_ta_tests.c normal_world/client/signed_rpc.c tests/validation/physical_memory.c tests/validation/physical_policy.c tests/validation/physical_probe.S normal_world/client/client.c runtime/benchmark/benchmark.c $(COMMON) $(EL1) $(RUNNER) $(HEADERS) $(OPTEE_PLATFORM_SOURCES) $(OPTEE_LINKER) FORCE | $(BUILD)
	$(CC) $(TARGET_FLAGS) $(CFLAGS) $(OPTEE_INCLUDES) -DB2_BENCH_RAM=1 $(OPTEE_FLAGS) $(LDFLAGS) -Wl,-T,$(OPTEE_LINKER) -Wl,-Map,$@.map $(filter %.c %.S,$^) -o $@


.PHONY: bl33
bl33: standalone-optee
include build_config/components.mk
