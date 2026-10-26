#include "access_probe.h"
#define ESR_CLASS_SHIFT 26U
#define ESR_CLASS_MASK 0x3fU
#define ESR_DATA_ABORT_CURRENT 0x25U
#define ESR_WRITE_BIT (1U << 6)
#define ESR_FAR_INVALID (1U << 10)
#define ESR_FAULT_CODE_MASK 0x3fU
#define ESR_EXTERNAL_ABORT 0x10U
#define ESR_PERMISSION_FIRST 0x0cU
#define ESR_PERMISSION_LAST 0x0fU
struct zcu106_fault_probe zcu106_fault_probe;
extern uint64_t zcu106_probe_load(uint64_t address);
extern void zcu106_probe_store(uint64_t address, uint64_t value);
extern char zcu106_probe_load_fault[], zcu106_probe_load_resume[];
extern char zcu106_probe_store_fault[], zcu106_probe_store_resume[];
extern uint32_t zcu106_probe_load32(uint64_t address);
extern void zcu106_probe_store32(uint64_t address, uint32_t value);
extern char zcu106_probe_load32_fault[], zcu106_probe_load32_resume[];
extern char zcu106_probe_store32_fault[], zcu106_probe_store32_resume[];
static void arm_probe(uint64_t address, bool write, unsigned int width)
{
    zcu106_fault_probe.width = width;
    zcu106_fault_probe.address = address;
    zcu106_fault_probe.write = write;
    zcu106_fault_probe.fault_seen = false;
    zcu106_fault_probe.esr = 0U;
    zcu106_fault_probe.far = 0U;
    zcu106_fault_probe.armed = true;
}
enum access_result zcu106_try_read64(uint64_t address, uint64_t *value, bool exception_path_verified)
{
    if (!exception_path_verified || value == NULL || zcu106_fault_probe.armed || address % sizeof(uint64_t) != 0U) { return ACCESS_INCONCLUSIVE; }
    arm_probe(address, false, sizeof(uint64_t));
    uint64_t loaded = zcu106_probe_load(address);
    zcu106_fault_probe.armed = false;
    if (zcu106_fault_probe.fault_seen) { return ACCESS_DENIED; }
    *value = loaded;
    return ACCESS_SUCCEEDED;
}
enum access_result zcu106_try_write64(uint64_t address, uint64_t value, bool exception_path_verified)
{
    if (!exception_path_verified || zcu106_fault_probe.armed || address % sizeof(uint64_t) != 0U) { return ACCESS_INCONCLUSIVE; }
    arm_probe(address, true, sizeof(uint64_t));
    zcu106_probe_store(address, value);
    zcu106_fault_probe.armed = false;
    return zcu106_fault_probe.fault_seen ? ACCESS_DENIED : ACCESS_SUCCEEDED;
}
uint64_t zcu106_handle_probe_abort(uint64_t esr, uint64_t far, uint64_t pc)
{
    uint64_t fault = esr & ESR_FAULT_CODE_MASK;
    bool permitted_fault = fault == ESR_EXTERNAL_ABORT || (fault >= ESR_PERMISSION_FIRST && fault <= ESR_PERMISSION_LAST);
    if (!zcu106_fault_probe.armed || !permitted_fault || (esr & ESR_FAR_INVALID) != 0U ||
        ((esr >> ESR_CLASS_SHIFT) & ESR_CLASS_MASK) != ESR_DATA_ABORT_CURRENT ||
        far != zcu106_fault_probe.address || ((esr & ESR_WRITE_BIT) != 0U) != zcu106_fault_probe.write) { return 0U; }
    uint64_t expected_pc = zcu106_fault_probe.write ? (uint64_t)zcu106_probe_store_fault : (uint64_t)zcu106_probe_load_fault;
    uint64_t resume = zcu106_fault_probe.write ? (uint64_t)zcu106_probe_store_resume : (uint64_t)zcu106_probe_load_resume;
    if (zcu106_fault_probe.width == sizeof(uint32_t)) {
        expected_pc = zcu106_fault_probe.write ? (uint64_t)zcu106_probe_store32_fault : (uint64_t)zcu106_probe_load32_fault;
        resume = zcu106_fault_probe.write ? (uint64_t)zcu106_probe_store32_resume : (uint64_t)zcu106_probe_load32_resume;
    } else if (zcu106_fault_probe.width != sizeof(uint64_t)) { return 0U; }
    if (pc != expected_pc) { return 0U; }
    zcu106_fault_probe.esr = esr;
    zcu106_fault_probe.far = far;
    zcu106_fault_probe.fault_seen = true;
    zcu106_fault_probe.armed = false;
    return resume;
}

enum access_result zcu106_try_read32(uint64_t address, uint32_t *value, bool exception_path_verified)
{
    if (!exception_path_verified || value == NULL || zcu106_fault_probe.armed || address % sizeof(uint32_t) != 0U) { return ACCESS_INCONCLUSIVE; }
    arm_probe(address, false, sizeof(uint32_t));
    uint32_t loaded = zcu106_probe_load32(address);
    zcu106_fault_probe.armed = false;
    if (zcu106_fault_probe.fault_seen) { return ACCESS_DENIED; }
    *value = loaded;
    return ACCESS_SUCCEEDED;
}
enum access_result zcu106_try_write32(uint64_t address, uint32_t value, bool exception_path_verified)
{
    if (!exception_path_verified || zcu106_fault_probe.armed || address % sizeof(uint32_t) != 0U) { return ACCESS_INCONCLUSIVE; }
    arm_probe(address, true, sizeof(uint32_t));
    zcu106_probe_store32(address, value);
    zcu106_fault_probe.armed = false;
    return zcu106_fault_probe.fault_seen ? ACCESS_DENIED : ACCESS_SUCCEEDED;
}
