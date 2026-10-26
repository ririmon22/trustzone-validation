#include "validation_ta.h"
#define TA_UUID VALIDATION_TA_UUID
#define TA_FLAGS (TA_FLAG_SINGLE_INSTANCE | TA_FLAG_MULTI_SESSION)
#define TA_STACK_SIZE (8U * 1024U)
#define TA_DATA_SIZE (32U * 1024U)
#define TA_VERSION "0.1"
#define TA_DESCRIPTION "Standalone validation test policy"
