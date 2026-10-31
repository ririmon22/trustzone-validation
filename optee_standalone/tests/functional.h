#ifndef B2_FUNCTIONAL_H
#define B2_FUNCTIONAL_H
#include "client.h"
#include "test_runner.h"
#define B2_CASE_COUNT 15U
#define B2_CHECK_COUNT 32U
#define B2_RESULT_MAGIC 0x42325253U
#define B2_RESULT_VERSION 3U
#define B2_ID_BYTES 24U
#define B2_REASON_BYTES 112U
#define B2_CHECK_NAME_BYTES 40U
#define B2_SELECTION_LEGACY 14U
#ifndef B2_REVIEW_PARENT
#define B2_REVIEW_PARENT 0U
#endif
#ifndef B2_TEST_INDEX
#define B2_TEST_INDEX 0U
#endif
/* 0は機能一式、1..13は一覧の順、15は物理保護試験、14だけが旧来の性能/PTA込み回帰。 */
enum b2_phase
{
    B2_IDLE,
    B2_PREPARE,
    B2_OPERATE,
    B2_EXPECTED_FAULT,
    B2_COMPARE,
    B2_SAVE,
    B2_CLEANUP,
    B2_DONE
};
enum b2_run_state
{
    B2_RUNNING = 1,
    B2_COMPLETE,
    B2_ERROR
};
struct b2_check
{
    char name[B2_CHECK_NAME_BYTES];
    uint64_t expected, actual;
};
struct b2_case_result
{
    char id[B2_ID_BYTES];
    uint32_t selected, status, phase, reported, check_count, overflow;
    char reason[B2_REASON_BYTES];
    struct b2_check checks[B2_CHECK_COUNT];
};
struct b2_result_buffer
{
    uint32_t magic, version, bytes, state;
    uint64_t run_id;
    uint32_t selection, current_case, phase, cleanup_errors;
    struct b2_case_result cases[B2_CASE_COUNT];
};
extern volatile struct b2_result_buffer b2_results;
extern volatile const uint32_t b2_test_selection;
void b2_results_begin(void);
void b2_results_end(void);
void b2_stage(enum b2_phase phase);
bool b2_check_value(const char *name, uint64_t expected, uint64_t actual);
void b2_note_reply(const char *name, struct tee_reply reply);
bool b2_open_pair(struct tee_session *a, struct tee_session *b);
void b2_close_session(struct tee_session *session);
void b2_run_service(void);
void b2_run_panic_recovery(void);
void b2_run_identity(void);
void b2_run_shared_memory(void);
void b2_run_key_api(void);
void b2_run_ta_isolation(void);
void b2_run_session_limits(void);
void b2_run_message_abi(void);
void b2_run_shared_boundaries(void);
void b2_run_standard_versions(void);
void b2_run_closed_core_session(void);
void b2_run_parameter_validation(void);
void b2_run_key_attributes(void);
void b2_run_physical_memory(void);
void b2_run_signed_ta(void);
void b2_run_functional_suite(void);
#endif
