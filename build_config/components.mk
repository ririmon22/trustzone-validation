# B2の暫定配置。C側: platform/zcu106/b2/memory_map.h、BL33: linker_bl33.ld。
# 実機の保護設定を表す値ではない。runtime-readyは変更しない。
TFA_SOURCE ?= $(CURDIR)/third_party/tfa-amd
OPTEE_SOURCE ?= $(CURDIR)/third_party/optee
EXTERNAL_CROSS_COMPILE ?= aarch64-linux-gnu-
EXTERNAL_COMPILER ?= gcc
PROFILE ?= basic
OUT := $(CURDIR)/build/components/$(PROFILE)
SDK := $(OUT)/optee/export-ta_arm64
TA_A := 6dc13418-534f-4642-9138-215876304910
TA_B := 6dc13419-534f-4642-9138-215876304910
TA_EXTERNAL := 6dc1341c-534f-4642-9138-215876304910

ifneq ($(PLATFORM),zcu106)
$(error Only PLATFORM=zcu106 is supported)
endif
ifeq ($(filter $(TOOLCHAIN),clang gcc),)
$(error TOOLCHAIN must be clang or gcc)
endif
ifneq ($(EXTERNAL_COMPILER),gcc)
$(error External Make route currently supports GCC only; legacy Clang procedure is retained in docs)
endif
ifeq ($(filter $(PROFILE),basic signed physical),)
$(error PROFILE must be basic, signed or physical)
endif

# 外部の設定・compilerを変えるときは、このPROFILEの出力を手動退避してから作り直す。
OPTEE_FLAGS_EXTERNAL = PLATFORM=zynqmp-zcu106 CFG_ARM64_core=y CFG_USER_TA_TARGETS=ta_arm64 \
 CROSS_COMPILE64=$(EXTERNAL_CROSS_COMPILE) CROSS_COMPILE_ta_arm64=$(EXTERNAL_CROSS_COMPILE) \
 O=$(OUT)/optee CFG_DT_DRIVER_EMBEDDED_TEST=n CFG_CORE_ASYNC_NOTIF=n \
 CFG_REE_FS=n CFG_RPMB_FS=n CFG_REE_FS_TA=n CFG_PKCS11_TA=n \
 CFG_BUILD_IN_TREE_TA=n CFG_EARLY_TA=y CFG_WITH_SOFTWARE_PRNG=y \
 CFG_TZDRAM_START=0x60000000 CFG_TZDRAM_SIZE=0x10000000 \
 CFG_SHMEM_START=0x10500000 CFG_SHMEM_SIZE=0x100000 CFG_DDR_SIZE=0x7ff00000 \
 PYTHON3=$(PYTHON)
TA_FLAGS = TA_DEV_KIT_DIR=$(SDK) CROSS_COMPILE=$(EXTERNAL_CROSS_COMPILE) \
 CROSS_COMPILE64=$(EXTERNAL_CROSS_COMPILE) CROSS_COMPILE_ta_arm64=$(EXTERNAL_CROSS_COMPILE)
CORE_SOURCE := $(OPTEE_SOURCE)
ifneq ($(PROFILE),basic)
CORE_SOURCE := $(OUT)/source/optee
OPTEE_FLAGS_EXTERNAL += CFG_TZ_VALIDATION_PTA=y CFG_WITH_STATS=y
endif
ifeq ($(PROFILE),signed)
OPTEE_FLAGS_EXTERNAL := $(filter-out CFG_REE_FS_TA=n,$(OPTEE_FLAGS_EXTERNAL))
OPTEE_FLAGS_EXTERNAL += CFG_REE_FS_TA=y CFG_REE_FS_TA_BUFFERED=n CFG_PREALLOC_RPC_CACHE=n CFG_TZ_SIGNED_AUDIT_PTA=y TA_PUBLIC_KEY=$(TA_PUBLIC_KEY)
TA_FLAGS += TA_SIGN_KEY=$(TA_SIGN_KEY)
endif
ifeq ($(PROFILE),physical)
OPTEE_FLAGS_EXTERNAL += CFG_TZ_PHYSICAL_PTA=y CFG_TZ_PHYSICAL_FIXED_BASE=0x10800000
endif

.PHONY: bl31 optee-sdk ta-a ta-b ta-external bl32 prepare-core
.PHONY: check-compiler check-optee check-sdk check-sign-key
# BL31はOP-TEEや署名鍵がなくても単独でビルドできる。
check-compiler:
	command -v $(EXTERNAL_CROSS_COMPILE)gcc
check-optee: check-compiler
	test -f $(OPTEE_SOURCE)/Makefile
check-sdk: check-compiler
	test -f $(SDK)/mk/ta_dev_kit.mk
check-sign-key:
ifeq ($(PROFILE),signed)
	test -f "$(TA_SIGN_KEY)"
endif
bl31: check-compiler
	test -f $(TFA_SOURCE)/Makefile
	$(MAKE) -C $(TFA_SOURCE) PLAT=zynqmp BUILD_BASE=$(OUT)/tfa CROSS_COMPILE=$(EXTERNAL_CROSS_COMPILE) RESET_TO_BL31=1 SPD=opteed PRELOADED_BL33_BASE=0x10200000 ZYNQMP_ATF_MEM_BASE=0x1000 ZYNQMP_ATF_MEM_SIZE=0x7f000 ZYNQMP_BL32_MEM_BASE=0x60000000 ZYNQMP_BL32_MEM_SIZE=0x10000000 bl31
# SDKは元のCoreソースから作る。TAを作った後にのみBL32へ組み込む。
optee-sdk: check-optee
	$(MAKE) -C $(OPTEE_SOURCE) $(OPTEE_FLAGS_EXTERNAL) ta_dev_kit
ta-a: check-sdk check-sign-key
	$(MAKE) -C secure_world/ta $(TA_FLAGS) O=$(OUT)/ta-a TA_VARIANT=a
ta-b: check-sdk check-sign-key
	$(MAKE) -C secure_world/ta $(TA_FLAGS) O=$(OUT)/ta-b TA_VARIANT=b
ta-external: check-sdk check-sign-key
	@test "$(PROFILE)" = signed
	$(MAKE) -C secure_world/ta_signed $(TA_FLAGS) O=$(OUT)/ta-external
prepare-core: check-optee
ifneq ($(PROFILE),basic)
	$(PYTHON) scripts/prepare_core.py --source $(OPTEE_SOURCE) --destination $(CORE_SOURCE) --profile $(PROFILE)
endif
bl32: check-compiler
ifeq ($(PROFILE),signed)
	test -f "$(TA_PUBLIC_KEY)"
endif
	test -f $(CORE_SOURCE)/Makefile
	test -f $(OUT)/ta-a/$(TA_A).stripped.elf
	test -f $(OUT)/ta-b/$(TA_B).stripped.elf
	PYTHONPATH=$(CURDIR)/third_party/pyelftools $(MAKE) -C $(CORE_SOURCE) $(OPTEE_FLAGS_EXTERNAL) EARLY_TA_PATHS="$(OUT)/ta-a/$(TA_A).stripped.elf $(OUT)/ta-b/$(TA_B).stripped.elf"
