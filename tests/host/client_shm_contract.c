/* ホスト合成試験。SMCの返答を差し替え、共有範囲の拒否と呼出し順を調べる。
 * 実CPUのSMC、MMU、キャッシュ、Secure Coreの動作は検証しない。
 * 一度成功するとclientは再初期化できないため、ケースごとに別プロセスで実行する。
 */
#include "client.h"
#include <assert.h>
#include <stdlib.h>

static unsigned int calls;
static unsigned int scenario;

struct smc_result smc_call(uint64_t fid, uint64_t a1, uint64_t a2, uint64_t a3)
{
    (void)a1;
    (void)a2;
    (void)a3;
    calls++;
    if (calls == 1U) {
        assert(fid == 0xb2000009UL);
        if (scenario == 1U) {
            return (struct smc_result){0UL, 0UL, 0UL, 0UL};
        }
        return (struct smc_result){0UL, 1UL, 0UL, 0UL};
    }
    assert(calls == 2U);
    assert(fid == 0xb2000007UL);
    uint64_t base = SHARED_MEMORY_BASE;
    uint64_t bytes = 96UL;
    uint64_t cached = 0UL;
    uint64_t status = 0UL;
    switch (scenario) {
    case 2U:
        status = 1UL;
        break;
    case 3U:
        base--;
        break;
    case 4U:
        base = SHARED_MEMORY_END;
        break;
    case 5U:
        bytes = 95UL;
        break;
    case 6U:
        bytes = UINT64_MAX;
        break;
    case 7U:
        base++;
        break;
    case 8U:
        cached = 1UL;
        break;
    case 9U:
        bytes = SHARED_MEMORY_SIZE;
        break;
    default:
        break;
    }
    return (struct smc_result){status, base, bytes, cached};
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    scenario = (unsigned int)strtoul(argv[1], NULL, 10);
    uint32_t result = tee_client_initialize();
    bool valid = scenario == 0U || scenario == 9U;
    assert(result == (valid ? TEE_CLIENT_OK : TEE_CLIENT_SHM_UNAVAILABLE));
    assert(calls == (scenario == 1U ? 1U : 2U));
    return 0;
}
