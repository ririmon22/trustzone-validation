# 検証報告（Phase 6）

BUILD、QEMU実行、実機結果を区別する。構成別の集約と再現方法は docs/phase6_progress.md。HARDWARE PASSはない。

---

Test ID: TZ-SYS-01

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: PASS

Expected result: independent secure and normal counters retain state

Actual result: independent-counters

Evidence: results/phase6/qemu/baremetal.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: baremetal

Execution time: 135951 generic-counter ticks

---

Test ID: TZ-SYS-02

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: PASS

Expected result: repeated service roundtrips return expected values and preserve state

Actual result: repeated-world-roundtrip

Evidence: results/phase6/qemu/baremetal.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: baremetal

Execution time: 79839 generic-counter ticks

---

Test ID: TZ-SYS-03

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: PASS

Expected result: allowed access succeeds; denied reads/writes fail; secure data unchanged

Actual result: allowed-access+denied-read-write+secure-recheck+boundaries

Evidence: results/phase6/qemu/baremetal.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: baremetal

Execution time: 26088 generic-counter ticks

---

Test ID: TZ-SYS-04

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: secure peripheral operation succeeds and prohibited operation has no effect

Actual result: secure-peripheral-test-not-implemented

Evidence: results/phase6/qemu/baremetal.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-05

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: SKIP

Expected result: allowed DMA succeeds and secure read/write DMA is denied without modification

Actual result: Zynq-DMA-not-on-virt

Evidence: results/phase6/qemu/baremetal.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-06

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: secure and nonsecure IRQ handlers receive only assigned interrupts and resume

Actual result: GIC-driver-and-interrupt-test-not-implemented

Evidence: results/phase6/qemu/baremetal.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-07

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: debug access follows configured permission

Actual result: requires-hardware-debug-policy

Evidence: results/phase6/qemu/baremetal.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-01

Target platform: qemu

Build status: BUILD PASS

Execution status: PASS

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement acquired; see current CSV for 128 samples and statistics

Evidence: results/phase6/qemu/baremetal.log, results/phase6/qemu/baremetal-benchmarks.csv

Limitation: QEMU measurement acquisition only; no performance threshold or hardware claim

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-02

Target platform: qemu

Build status: BUILD PASS

Execution status: PASS

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement acquired; see current CSV for 128 samples and statistics

Evidence: results/phase6/qemu/baremetal.log, results/phase6/qemu/baremetal-benchmarks.csv

Limitation: QEMU measurement acquisition only; no performance threshold or hardware claim

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-03

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-04

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-05

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-06

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-07

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-01

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: independent secure and normal counters retain state

Actual result: secure-EL1-payload-not-integrated

Evidence: results/phase6/qemu/tfa.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-02

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: repeated service roundtrips return expected values and preserve state

Actual result: SMCCC-smoke-does-not-cover-world-context-test

Evidence: results/phase6/qemu/tfa.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-03

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: allowed access succeeds; denied reads/writes fail; secure data unchanged

Actual result: platform-protection-test-not-integrated

Evidence: results/phase6/qemu/tfa.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-04

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: secure peripheral operation succeeds and prohibited operation has no effect

Actual result: peripheral-service-missing

Evidence: results/phase6/qemu/tfa.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-05

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: SKIP

Expected result: allowed DMA succeeds and secure read/write DMA is denied without modification

Actual result: Zynq-DMA-not-on-virt

Evidence: results/phase6/qemu/tfa.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-06

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: secure and nonsecure IRQ handlers receive only assigned interrupts and resume

Actual result: interrupt-test-missing

Evidence: results/phase6/qemu/tfa.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-07

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: debug access follows configured permission

Actual result: requires-hardware

Evidence: results/phase6/qemu/tfa.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-01

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-02

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-03

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-04

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-05

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-06

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-07

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TEE-01

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: PASS

Expected result: valid requests and multiple sessions succeed; malformed requests rejected

Actual result: two-sessions+echo+invalid-command

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-02

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: authentication and authorization independently enforced including stale session

Actual result: Normal-requester-authentication-missing-TA-identity-tested-separately

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-03

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: PASS

Expected result: TA and core isolation survives controlled illegal access

Actual result: TA-private-data-and-Core-canary-access-denied-and-unchanged

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-04

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: signed TA accepted and modified TA rejected

Actual result: Early-TA-is-not-signed-file-loader-test

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-05

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: secure key operation matches vector; key not exportable

Actual result: AES-KAT-and-export-policy-tested-memory-extraction-missing

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-06

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: secure persistent storage resists access/tampering and survives restart

Actual result: no-storage-backend-or-RPC

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-07

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: PASS

Expected result: TA panic detected and other TA remains usable

Actual result: TA-A-panic+TA-B-continuity+TA-A-reopen

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-08

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: authenticated boot accepts original and rejects modified image

Actual result: authenticated-boot-chain-not-configured

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-01

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-02

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-03

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-04

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-05

Target platform: qemu

Build status: BUILD PASS

Execution status: PASS

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement acquired; see current CSV for 128 samples and statistics

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-benchmarks.csv

Limitation: QEMU measurement acquisition only; no performance threshold or hardware claim

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-06

Target platform: qemu

Build status: BUILD PASS

Execution status: PASS

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement acquired; see current CSV for 128 samples and statistics

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-benchmarks.csv

Limitation: QEMU measurement acquisition only; no performance threshold or hardware claim

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-07

Target platform: qemu

Build status: NOT_BUILT (required implementation incomplete)

Execution status: BLOCKED

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: Measurement backend or applicable test is not integrated in this configuration

Evidence: docs/known_limitations.md

Limitation: Not measured; no hardware performance claim

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-01

Target platform: zcu106

Build status: BUILD PASS (self-authored payload only)

Execution status: NOT_RUN

Expected result: independent secure and normal counters retain state

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-02

Target platform: zcu106

Build status: BUILD PASS (self-authored payload only)

Execution status: NOT_RUN

Expected result: repeated service roundtrips return expected values and preserve state

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-03

Target platform: zcu106

Build status: PARTIAL: native register/probe/vector library BUILD PASS; board execution integration incomplete

Execution status: NOT_RUN

Expected result: allowed access succeeds; denied reads/writes fail; secure data unchanged

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-04

Target platform: zcu106

Build status: PARTIAL: native register/probe/vector library BUILD PASS; board execution integration incomplete

Execution status: NOT_RUN

Expected result: secure peripheral operation succeeds and prohibited operation has no effect

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-05

Target platform: zcu106

Build status: PARTIAL: native register/probe/vector library BUILD PASS; board execution integration incomplete

Execution status: NOT_RUN

Expected result: allowed DMA succeeds and secure read/write DMA is denied without modification

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-06

Target platform: zcu106

Build status: PARTIAL: native register/probe/vector library BUILD PASS; board execution integration incomplete

Execution status: NOT_RUN

Expected result: secure and nonsecure IRQ handlers receive only assigned interrupts and resume

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-07

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: debug access follows configured permission

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-01

Target platform: zcu106

Build status: BUILD PASS (self-authored payload only)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-02

Target platform: zcu106

Build status: BUILD PASS (self-authored payload only)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-03

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-04

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-05

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-06

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: PERF-07

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: baremetal

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-01

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: independent secure and normal counters retain state

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-02

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: repeated service roundtrips return expected values and preserve state

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-03

Target platform: zcu106

Build status: PARTIAL: native register/probe/vector library BUILD PASS; board execution integration incomplete

Execution status: NOT_RUN

Expected result: allowed access succeeds; denied reads/writes fail; secure data unchanged

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-04

Target platform: zcu106

Build status: PARTIAL: native register/probe/vector library BUILD PASS; board execution integration incomplete

Execution status: NOT_RUN

Expected result: secure peripheral operation succeeds and prohibited operation has no effect

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-05

Target platform: zcu106

Build status: PARTIAL: native register/probe/vector library BUILD PASS; board execution integration incomplete

Execution status: NOT_RUN

Expected result: allowed DMA succeeds and secure read/write DMA is denied without modification

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-06

Target platform: zcu106

Build status: PARTIAL: native register/probe/vector library BUILD PASS; board execution integration incomplete

Execution status: NOT_RUN

Expected result: secure and nonsecure IRQ handlers receive only assigned interrupts and resume

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TZ-SYS-07

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: debug access follows configured permission

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-01

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-02

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-03

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-04

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-05

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-06

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: PERF-07

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TEE-01

Target platform: zcu106

Build status: BUILD PASS (self-authored payload only)

Execution status: NOT_RUN

Expected result: valid requests and multiple sessions succeed; malformed requests rejected

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-02

Target platform: zcu106

Build status: BUILD PASS (self-authored payload only)

Execution status: NOT_RUN

Expected result: authentication and authorization independently enforced including stale session

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-03

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: TA and core isolation survives controlled illegal access

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-04

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: signed TA accepted and modified TA rejected

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-05

Target platform: zcu106

Build status: BUILD PASS (self-authored payload only)

Execution status: NOT_RUN

Expected result: secure key operation matches vector; key not exportable

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-06

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: secure persistent storage resists access/tampering and survives restart

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-07

Target platform: zcu106

Build status: BUILD PASS (self-authored payload only)

Execution status: NOT_RUN

Expected result: TA panic detected and other TA remains usable

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-08

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: authenticated boot accepts original and rejects modified image

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-01

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-02

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-03

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-04

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-05

Target platform: zcu106

Build status: BUILD PASS (self-authored payload only)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-06

Target platform: zcu106

Build status: BUILD PASS (self-authored payload only)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-07

Target platform: zcu106

Build status: NOT_BUILT (required implementation incomplete)

Execution status: NOT_RUN

Expected result: measurement includes warmup, samples, timing conditions and statistical summary

Actual result: No hardware execution; board, generated BSP/FSBL/PMUFW reservations and verified master/routing settings unavailable

Evidence: results/phase4/build-zcu106.log, results/phase4/zcu106-load-gate.log, results/phase4/zcu106-profile-gate.log, docs/zcu106_phase4_handoff.md

Limitation: NOT_RUN on ZCU106; OCM BL31 placement must be reconciled; load safety gates remain BLOCKED

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: B1-SMCCC-01

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: PASS

Expected result: SMCCC and PSCI return supported version values

Actual result: standard-interface-smoke-only

Evidence: results/phase6/qemu/tfa.log

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: tfa

Execution time: unmeasured (see execution status)

---

Test ID: TEE-AUTH-TA

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: PASS

Expected result: Core-supplied TA identity authorizes TA-A and denies PUBLIC even with claimed A identifier

Actual result: Core-client-identity+TA-A-allowlist+forged-login-and-PUBLIC-denied

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-AUTH-NS

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: Normal requester credentials verified independently of self-reported identifiers

Actual result: Normal-requester-authentication-not-provisioned

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-SHM-01

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: PASS

Expected result: valid memref accepted, wrong size and invalid ranges rejected, service and buffer intact

Actual result: valid+wrong-size+outside+secure+overflow+continuity+buffer-unchanged

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-KEY-API

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: PASS

Expected result: Secure key is nonextractable, export denied, AES KAT and subsequent key use succeed

Actual result: Secure-generated-nonextractable-key+export-denied+AES-KAT+key-still-usable

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-KEY-MEM

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: BLOCKED

Expected result: memory key extraction attempt denied with key intact

Actual result: raw-key-memory-extraction-not-tested

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-ISO-TA

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: PASS

Expected result: allowed own access succeeds, cross-TA read/write fault, victim secret unchanged

Actual result: B-secret-read-write-probes+A-fault+B-private-state-recheck+A-reopen

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-ISO-CORE

Target platform: qemu

Build status: BUILD PASS (executed image)

Execution status: PASS

Expected result: Core access denied and Core protected contents rechecked unchanged

Actual result: Core-private-canary+read-write-faults+privileged-recheck+TA-continuity

Evidence: results/phase6/qemu/optee.log, results/phase6/qemu/optee-secure.log, results/phase6/qemu/security-evidence.json

Limitation: QEMU TCG only; not ZCU106 hardware evidence

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-AUTH-TA

Target platform: zcu106

Build status: Client BUILD PASS; ZCU106 Core integration NOT_BUILT

Execution status: NOT_RUN

Expected result: Core-supplied TA identity authorizes TA-A and denies PUBLIC even with claimed A identifier

Actual result: No physical execution; QEMU validation result is not hardware evidence

Evidence: results/phase6/build-zcu106.log, results/phase6/zcu106-load-gate.log, results/phase6/zcu106-profile-gate.log

Limitation: Unverified board routing/reservations and OP-TEE port; load is prohibited

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-AUTH-NS

Target platform: zcu106

Build status: Client BUILD PASS; ZCU106 Core integration NOT_BUILT

Execution status: NOT_RUN

Expected result: Normal requester credentials verified independently of self-reported identifiers

Actual result: No physical execution; QEMU validation result is not hardware evidence

Evidence: results/phase6/build-zcu106.log, results/phase6/zcu106-load-gate.log, results/phase6/zcu106-profile-gate.log

Limitation: Unverified board routing/reservations and OP-TEE port; load is prohibited

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-SHM-01

Target platform: zcu106

Build status: Client BUILD PASS; ZCU106 Core integration NOT_BUILT

Execution status: NOT_RUN

Expected result: valid memref accepted, wrong size and invalid ranges rejected, service and buffer intact

Actual result: No physical execution; QEMU validation result is not hardware evidence

Evidence: results/phase6/build-zcu106.log, results/phase6/zcu106-load-gate.log, results/phase6/zcu106-profile-gate.log

Limitation: Unverified board routing/reservations and OP-TEE port; load is prohibited

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-KEY-API

Target platform: zcu106

Build status: Client BUILD PASS; ZCU106 Core integration NOT_BUILT

Execution status: NOT_RUN

Expected result: Secure key is nonextractable, export denied, AES KAT and subsequent key use succeed

Actual result: No physical execution; QEMU validation result is not hardware evidence

Evidence: results/phase6/build-zcu106.log, results/phase6/zcu106-load-gate.log, results/phase6/zcu106-profile-gate.log

Limitation: Unverified board routing/reservations and OP-TEE port; load is prohibited

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-KEY-MEM

Target platform: zcu106

Build status: Client BUILD PASS; ZCU106 Core integration NOT_BUILT

Execution status: NOT_RUN

Expected result: memory key extraction attempt denied with key intact

Actual result: No physical execution; QEMU validation result is not hardware evidence

Evidence: results/phase6/build-zcu106.log, results/phase6/zcu106-load-gate.log, results/phase6/zcu106-profile-gate.log

Limitation: Unverified board routing/reservations and OP-TEE port; load is prohibited

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-ISO-TA

Target platform: zcu106

Build status: Client BUILD PASS; ZCU106 Core integration NOT_BUILT

Execution status: NOT_RUN

Expected result: allowed own access succeeds, cross-TA read/write fault, victim secret unchanged

Actual result: No physical execution; QEMU validation result is not hardware evidence

Evidence: results/phase6/build-zcu106.log, results/phase6/zcu106-load-gate.log, results/phase6/zcu106-profile-gate.log

Limitation: Unverified board routing/reservations and OP-TEE port; load is prohibited

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: TEE-ISO-CORE

Target platform: zcu106

Build status: Client BUILD PASS; ZCU106 Core integration NOT_BUILT

Execution status: NOT_RUN

Expected result: Core access denied and Core protected contents rechecked unchanged

Actual result: No physical execution; QEMU validation result is not hardware evidence

Evidence: results/phase6/build-zcu106.log, results/phase6/zcu106-load-gate.log, results/phase6/zcu106-profile-gate.log

Limitation: Unverified board routing/reservations and OP-TEE port; load is prohibited

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-08

Target platform: qemu

Build status: BUILD PASS (client)

Execution status: PASS

Expected result: measure session open with existing TA instance

Actual result: {"test_id": "PERF-08", "platform": "qemu", "configuration": "optee", "status": "MEASURED", "count": 128, "timer_hz": 62500000, "cpu_hz": null, "ticks": {"minimum": 21389.0, "maximum": 27064.0, "mean": 21981.6796875, "median": 21592.0, "p95": 25599.0, "p99": 26862.0}, "microseconds": {"minimum": 342.224, "maximum": 433.024, "mean": 351.706875, "median": 345.472, "p95": 409.584, "p99": 429.792}, "conditions": "timer_hz=62500000 cpu_hz=unknown cache=on mmu=on irq=masked interval=session-open-only-existing-TA-instance warmup=16", "evidence": "results/phase6/qemu/optee-benchmarks.csv"}

Evidence: results/phase6/performance.json, results/phase6/qemu/optee.log

Limitation: Measurement acquisition only; QEMU is not hardware performance

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-09

Target platform: qemu

Build status: BUILD PASS (client)

Execution status: PASS

Expected result: measure session close while another session remains

Actual result: {"test_id": "PERF-09", "platform": "qemu", "configuration": "optee", "status": "MEASURED", "count": 128, "timer_hz": 62500000, "cpu_hz": null, "ticks": {"minimum": 17724.0, "maximum": 20912.0, "mean": 18731.546875, "median": 18704.5, "p95": 19547.0, "p99": 19722.0}, "microseconds": {"minimum": 283.584, "maximum": 334.592, "mean": 299.70475, "median": 299.272, "p95": 312.752, "p99": 315.552}, "conditions": "timer_hz=62500000 cpu_hz=unknown cache=on mmu=on irq=masked interval=session-close-only-other-session-remains-open warmup=16", "evidence": "results/phase6/qemu/optee-benchmarks.csv"}

Evidence: results/phase6/performance.json, results/phase6/qemu/optee.log

Limitation: Measurement acquisition only; QEMU is not hardware performance

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-10

Target platform: qemu

Build status: BUILD PASS (client)

Execution status: PASS

Expected result: measure panic plus cleanup and recovery through successful echo

Actual result: {"test_id": "PERF-10", "platform": "qemu", "configuration": "optee", "status": "MEASURED", "count": 128, "timer_hz": 62500000, "cpu_hz": null, "ticks": {"minimum": 3667029.0, "maximum": 3903083.0, "mean": 3738005.296875, "median": 3710820.5, "p95": 3890732.0, "p99": 3901198.0}, "microseconds": {"minimum": 58672.464, "maximum": 62449.328, "mean": 59808.08475, "median": 59373.128, "p95": 62251.712, "p99": 62419.168}, "conditions": "timer_hz=62500000 cpu_hz=unknown cache=on mmu=on irq=masked interval=panic-diagnostic+TA-reopen+echo-excluding-final-close warmup=16", "evidence": "results/phase6/qemu/optee-benchmarks.csv"}

Evidence: results/phase6/performance.json, results/phase6/qemu/optee.log

Limitation: Measurement acquisition only; QEMU is not hardware performance

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-11

Target platform: qemu

Build status: BUILD PASS (client)

Execution status: PASS

Expected result: measure post-TARGET_DEAD recovery through successful echo

Actual result: {"test_id": "PERF-11", "platform": "qemu", "configuration": "optee", "status": "MEASURED", "count": 128, "timer_hz": 62500000, "cpu_hz": null, "ticks": {"minimum": 3313735.0, "maximum": 3923194.0, "mean": 3385141.2109375, "median": 3370807.0, "p95": 3514824.0, "p99": 3555259.0}, "microseconds": {"minimum": 53019.76, "maximum": 62771.104, "mean": 54162.259375, "median": 53932.912, "p95": 56237.184, "p99": 56884.144}, "conditions": "timer_hz=62500000 cpu_hz=unknown cache=on mmu=on irq=masked interval=TARGET-DEAD-observed-to-TA-reopen-and-successful-echo warmup=16", "evidence": "results/phase6/qemu/optee-benchmarks.csv"}

Evidence: results/phase6/performance.json, results/phase6/qemu/optee.log

Limitation: Measurement acquisition only; QEMU is not hardware performance

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-08

Target platform: zcu106

Build status: BUILD PASS (client)

Execution status: NOT_RUN

Expected result: measure session open with existing TA instance

Actual result: {"test_id": "PERF-08", "platform": "zcu106", "status": "NOT_RUN", "count": 0, "ticks": null, "microseconds": null, "reason": "Hardware unavailable; load gate BLOCKED"}

Evidence: results/phase6/performance.json, results/phase6/qemu/optee.log

Limitation: Measurement acquisition only; QEMU is not hardware performance

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-09

Target platform: zcu106

Build status: BUILD PASS (client)

Execution status: NOT_RUN

Expected result: measure session close while another session remains

Actual result: {"test_id": "PERF-09", "platform": "zcu106", "status": "NOT_RUN", "count": 0, "ticks": null, "microseconds": null, "reason": "Hardware unavailable; load gate BLOCKED"}

Evidence: results/phase6/performance.json, results/phase6/qemu/optee.log

Limitation: Measurement acquisition only; QEMU is not hardware performance

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-10

Target platform: zcu106

Build status: BUILD PASS (client)

Execution status: NOT_RUN

Expected result: measure panic plus cleanup and recovery through successful echo

Actual result: {"test_id": "PERF-10", "platform": "zcu106", "status": "NOT_RUN", "count": 0, "ticks": null, "microseconds": null, "reason": "Hardware unavailable; load gate BLOCKED"}

Evidence: results/phase6/performance.json, results/phase6/qemu/optee.log

Limitation: Measurement acquisition only; QEMU is not hardware performance

Configuration: optee

Execution time: unmeasured (see execution status)

---

Test ID: PERF-11

Target platform: zcu106

Build status: BUILD PASS (client)

Execution status: NOT_RUN

Expected result: measure post-TARGET_DEAD recovery through successful echo

Actual result: {"test_id": "PERF-11", "platform": "zcu106", "status": "NOT_RUN", "count": 0, "ticks": null, "microseconds": null, "reason": "Hardware unavailable; load gate BLOCKED"}

Evidence: results/phase6/performance.json, results/phase6/qemu/optee.log

Limitation: Measurement acquisition only; QEMU is not hardware performance

Configuration: optee

Execution time: unmeasured (see execution status)
