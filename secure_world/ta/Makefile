TA_VARIANT ?= a
ifeq ($(TA_VARIANT),b)
BINARY=6dc13419-534f-4642-9138-215876304910
# SDK側user_ta_header.cにもBのUUIDを渡す。cppflags-yではTA本文だけに届く。
CPPFLAGS_ta_arm64 += -DTA_VARIANT_B
else
BINARY=6dc13418-534f-4642-9138-215876304910
endif
include $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk
