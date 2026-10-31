#define TA_UUID { 0x6dc1341c, 0x534f, 0x4642, {0x91,0x38,0x21,0x58,0x76,0x30,0x49,0x10} }
/* KEEP_ALIVEもSINGLE_INSTANCEも指定せず、closeで完全に終了する。 */
#define TA_FLAGS 0
#define TA_STACK_SIZE (8U * 1024U)
#define TA_DATA_SIZE (32U * 1024U)
#define TA_VERSION "0.1"
#define TA_DESCRIPTION "Runtime signed loader validation only"
