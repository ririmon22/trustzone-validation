PLATFORM ?= qemu
SOURCE_MODE ?= locked
DEBUG_RUN_ID ?= 1
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
CFLAGS := -mcpu=cortex-a53 -mgeneral-regs-only -mstrict-align -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -O2 -g -Wall -Wextra -Werror -Wconversion -Wshadow -std=c11 -DDEBUG_RUN_ID=$(DEBUG_RUN_ID)UL
INCLUDES := -Icommon/include -Iplatform/$(PLATFORM) -Ioptee_standalone/client
LDFLAGS := -nostdlib -static -Wl,--build-id=none
COMMON := common/debug_state.c common/memory.c common/arch.c common/result_report/console.c platform/$(PLATFORM)/uart.c
EL1 := common/el1_vectors.S common/el1_fault.c common/probe.S
RUNNER := common/test_runner/runner.c
HEADERS := $(wildcard common/include/*.h) common/include/exception_capture.inc platform/$(PLATFORM)/memory_map.h optee_standalone/client/client.h optee_standalone/ta/validation_ta.h optee_standalone/pta/validation_canary.h
OPTEE_LINKER := platform/$(PLATFORM)/linker_nonsecure.ld
OPTEE_INCLUDES := $(INCLUDES)
OPTEE_FLAGS :=
OPTEE_PLATFORM_SOURCES :=
ifeq ($(PLATFORM),zcu106)
OPTEE_LINKER := platform/zcu106/b2/linker_bl33.ld
OPTEE_INCLUDES := -Iplatform/zcu106/b2 $(INCLUDES)
OPTEE_FLAGS := -DTEE_SHARED_CACHED=1 -DRAM_LOG_CAPACITY=131072U
OPTEE_PLATFORM_SOURCES := platform/zcu106/b2/client_mmu.c platform/zcu106/b2/client_mmu.h platform/zcu106/b2/memory_map.h platform/zcu106/b2/memory_map.json
endif
.PHONY: all check-map host-test run clean zcu106-prepare optee-client-check ta external
all: check-map $(BUILD)/el3.elf $(BUILD)/secure.elf $(BUILD)/nonsecure.elf $(BUILD)/tfa_bl33.elf $(BUILD)/optee_bl33.elf
	$(PYTHON) scripts/check_memory_map.py --platform $(PLATFORM) --elf $(BUILD)/el3.elf --elf $(BUILD)/secure.elf --elf $(BUILD)/nonsecure.elf
	$(PYTHON) scripts/check_memory_map.py --platform $(PLATFORM) --elf $(BUILD)/tfa_bl33.elf
	$(PYTHON) scripts/check_memory_map.py --platform $(PLATFORM) --elf $(BUILD)/optee_bl33.elf
check-map:
	$(PYTHON) scripts/check_memory_map.py --platform $(PLATFORM)
$(BUILD):
	mkdir -p $@
$(BUILD)/el3.elf: baremetal/el3_monitor/start.S baremetal/el3_monitor/monitor.c $(COMMON) $(HEADERS) platform/$(PLATFORM)/linker_el3.ld | $(BUILD)
	$(CC) $(TARGET_FLAGS) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -Wl,-T,platform/$(PLATFORM)/linker_el3.ld -Wl,-Map,$@.map $(filter %.c %.S,$^) -o $@
$(BUILD)/secure.elf: baremetal/secure_app/start.S baremetal/secure_app/main.c $(COMMON) $(EL1) $(HEADERS) platform/$(PLATFORM)/linker_secure.ld | $(BUILD)
	$(CC) $(TARGET_FLAGS) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -Wl,-T,platform/$(PLATFORM)/linker_secure.ld -Wl,-Map,$@.map $(filter %.c %.S,$^) -o $@
$(BUILD)/nonsecure.elf: baremetal/nonsecure_app/start.S baremetal/nonsecure_app/main.c baremetal/tests/register_context.S $(COMMON) $(EL1) $(RUNNER) common/benchmark/benchmark.c $(HEADERS) platform/$(PLATFORM)/linker_nonsecure.ld | $(BUILD)
	$(CC) $(TARGET_FLAGS) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -Wl,-T,platform/$(PLATFORM)/linker_nonsecure.ld -Wl,-Map,$@.map $(filter %.c %.S,$^) -o $@
$(BUILD)/tfa_bl33.elf: tfa_standalone/integration/start.S tfa_standalone/integration/main.c $(COMMON) $(EL1) $(RUNNER) $(HEADERS) platform/$(PLATFORM)/linker_nonsecure.ld | $(BUILD)
	$(CC) $(TARGET_FLAGS) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -Wl,-T,platform/$(PLATFORM)/linker_nonsecure.ld -Wl,-Map,$@.map $(filter %.c %.S,$^) -o $@
$(BUILD)/optee_bl33.elf: tfa_standalone/integration/start.S optee_standalone/tests/main.c optee_standalone/tests/security_tests.c optee_standalone/tests/stress_tests.c optee_standalone/tests/boundary_tests.c optee_standalone/tests/extended_tests.c optee_standalone/client/client.c common/benchmark/benchmark.c $(COMMON) $(EL1) $(RUNNER) $(HEADERS) $(OPTEE_PLATFORM_SOURCES) $(OPTEE_LINKER) | $(BUILD)
	$(CC) $(TARGET_FLAGS) $(CFLAGS) $(OPTEE_INCLUDES) $(OPTEE_FLAGS) $(LDFLAGS) -Wl,-T,$(OPTEE_LINKER) -Wl,-Map,$@.map $(filter %.c %.S,$^) -o $@
host-test:
	$(PYTHON) -m unittest discover -s tests -v
run: all
	bash scripts/run_qemu.sh baremetal
zcu106-prepare:
	$(MAKE) PLATFORM=zcu106 all
	$(PYTHON) scripts/check_memory_map.py --platform zcu106 --require-protection
external:
	$(PYTHON) scripts/source_snapshot.py tfa-amd optee
clean:
	rm -rf build

.PHONY: zcu106-drivers
ifeq ($(PLATFORM),zcu106)
all: zcu106-drivers
endif
zcu106-drivers: build/zcu106/drivers/performance.o build/zcu106/drivers/protection.o build/zcu106/drivers/hardware_tests.o build/zcu106/drivers/native_io.o build/zcu106/drivers/violation.o build/zcu106/drivers/zdma.o build/zcu106/drivers/gic.o build/zcu106/drivers/access_probe.o build/zcu106/drivers/access_probe_asm.o build/zcu106/drivers/test_vectors.o build/zcu106/drivers/interrupt_dispatch.o build/zcu106/drivers/dma_validation.o
	$(TARGET_AR) rcs build/zcu106/libprotection.a $^
build/zcu106/drivers/%.o: platform/zcu106/%.c platform/zcu106/include/protection.h platform/zcu106/include/hardware_tests.h platform/zcu106/include/backends.h platform/zcu106/include/access_probe.h
	mkdir -p build/zcu106/drivers
	$(CC) $(filter-out -fuse-ld=lld,$(TARGET_FLAGS)) $(CFLAGS) -Iplatform/zcu106/include -c $< -o $@

.PHONY: external-tfa external-optee external-ta-a external-ta-b zcu106-profile
external-tfa:
	$(PYTHON) scripts/build_external.py tfa
external-optee:
	$(PYTHON) scripts/build_external.py optee --early-ta build/external/ta-a/6dc13418-534f-4642-9138-215876304910.stripped.elf --early-ta build/external/ta-b/6dc13419-534f-4642-9138-215876304910.stripped.elf
external-ta-a:
	$(PYTHON) scripts/build_external.py ta-a
external-ta-b:
	$(PYTHON) scripts/build_external.py ta-b
zcu106-profile:
	$(PYTHON) scripts/check_hardware_profile.py

build/zcu106/drivers/access_probe_asm.o: platform/zcu106/access_probe.S
	mkdir -p build/zcu106/drivers
	$(CC) $(filter-out -fuse-ld=lld,$(TARGET_FLAGS)) $(CFLAGS) -c $< -o $@

.PHONY: external-optee-sdk
external-optee-sdk:
	$(PYTHON) scripts/build_external.py optee-sdk

build/zcu106/drivers/test_vectors.o: platform/zcu106/test_vectors.S
	mkdir -p build/zcu106/drivers
	$(CC) $(filter-out -fuse-ld=lld,$(TARGET_FLAGS)) $(CFLAGS) -c $< -o $@

.PHONY: qemu-rom
qemu-rom: build/qemu/baremetal-rom.bin
build/qemu/bootstrap.bin: platform/qemu/bootstrap/start.S platform/qemu/bootstrap/linker.ld
	mkdir -p build/qemu
	$(CC) $(TARGET_FLAGS) -mcpu=cortex-a53 -nostdlib -Wl,-T,platform/qemu/bootstrap/linker.ld -Wl,--oformat=binary $< -o $@
build/qemu/baremetal-rom.bin: build/qemu/bootstrap.bin build/qemu/el3.elf build/qemu/secure.elf build/qemu/nonsecure.elf scripts/pack_qemu_rom.py
	$(PYTHON) scripts/pack_qemu_rom.py --stub $< --output $@ build/qemu/el3.elf build/qemu/secure.elf build/qemu/nonsecure.elf

.PHONY: qemu-external
qemu-external: build/qemu/external/tfa_bl33.bin build/qemu/external/optee_bl33.bin
EXTERNAL_INCLUDES := -Iplatform/qemu/external -Icommon/include -Iplatform/qemu -Ioptee_standalone/client
build/qemu/external/tfa_bl33.elf: tfa_standalone/integration/start.S tfa_standalone/integration/main.c $(COMMON) $(EL1) $(RUNNER) platform/qemu/external/linker_bl33.ld platform/qemu/external/memory_map.h $(HEADERS)
	mkdir -p build/qemu/external
	$(CC) $(TARGET_FLAGS) $(CFLAGS) $(EXTERNAL_INCLUDES) $(LDFLAGS) -Wl,-T,platform/qemu/external/linker_bl33.ld -Wl,-Map,$@.map $(filter %.c %.S,$^) -o $@
build/qemu/external/optee_bl33.elf: tfa_standalone/integration/start.S optee_standalone/tests/main.c optee_standalone/tests/security_tests.c optee_standalone/tests/stress_tests.c optee_standalone/tests/boundary_tests.c optee_standalone/tests/extended_tests.c optee_standalone/client/client.c platform/qemu/external/client_mmu.c $(COMMON) $(EL1) $(RUNNER) common/benchmark/benchmark.c platform/qemu/external/linker_bl33.ld platform/qemu/external/memory_map.h $(HEADERS)
	mkdir -p build/qemu/external
	$(CC) $(TARGET_FLAGS) $(CFLAGS) $(EXTERNAL_INCLUDES) -DTEE_SHARED_CACHED=1 -DRAM_LOG_CAPACITY=131072U $(LDFLAGS) -Wl,-T,platform/qemu/external/linker_bl33.ld -Wl,-Map,$@.map $(filter %.c %.S,$^) -o $@
build/qemu/external/%.bin: build/qemu/external/%.elf
	$(TARGET_OBJCOPY) -O binary $< $@

.PHONY: llvm-runtime
llvm-runtime: build/toolchain-runtime/libbuiltins.a
build/toolchain-runtime/libbuiltins.a: third_party/compiler-rt/compiler-rt/lib/builtins/udivti3.c third_party/compiler-rt/compiler-rt/lib/builtins/udivmodti4.c
	$(PYTHON) scripts/source_snapshot.py --source-mode $(SOURCE_MODE) compiler-rt
	mkdir -p build/toolchain-runtime
	clang --target=aarch64-none-elf -O2 -ffreestanding -fno-builtin -mgeneral-regs-only -Ithird_party/compiler-rt/compiler-rt/lib/builtins -c $(word 1,$^) -o build/toolchain-runtime/udivti3.o
	clang --target=aarch64-none-elf -O2 -ffreestanding -fno-builtin -mgeneral-regs-only -Ithird_party/compiler-rt/compiler-rt/lib/builtins -c $(word 2,$^) -o build/toolchain-runtime/udivmodti4.o
	$(LLVM_AR) rcs $@ build/toolchain-runtime/udivti3.o build/toolchain-runtime/udivmodti4.o

.PHONY: baremetal standalone-tfa standalone-optee
baremetal: check-map $(BUILD)/el3.elf $(BUILD)/secure.elf $(BUILD)/nonsecure.elf
	$(PYTHON) scripts/check_memory_map.py --platform $(PLATFORM) --elf $(BUILD)/el3.elf --elf $(BUILD)/secure.elf --elf $(BUILD)/nonsecure.elf
ifeq ($(PLATFORM),zcu106)
	$(PYTHON) scripts/check_zcu106_a.py --build-dir $(BUILD) --output results/phase8/a-static-report.json
endif
standalone-tfa: check-map $(BUILD)/tfa_bl33.elf
	$(PYTHON) scripts/check_memory_map.py --platform $(PLATFORM) --elf $(BUILD)/tfa_bl33.elf
standalone-optee: check-map $(BUILD)/optee_bl33.elf
	$(PYTHON) scripts/check_memory_map.py --platform $(PLATFORM) --elf $(BUILD)/optee_bl33.elf

.PHONY: a-unified
a-unified:
	$(PYTHON) scripts/build_a_unified.py --platform $(PLATFORM) --compiler $(TOOLCHAIN) --cross-compile $(CROSS_COMPILE) --llvm-dir $(LLVM_DIR)
	$(PYTHON) scripts/check_a_unified.py --platform $(PLATFORM) --elf build/$(PLATFORM)/unified/a_unified.elf
