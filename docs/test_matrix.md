# 検証項目とソース対応

期待値の独立定義は `tests/expectations.json`。全結果は `results/status.json`。
未実装をNOT_RUNだけで隠さず、実装状況欄を併記する。

|ID|ソース／手順|実装・実行状況|根拠（URLはreferences.md）|
|---|---|---|---|
|TZ-SYS-01|baremetal/nonsecure_app/main.c; baremetal/secure_app/main.c|実装・BUILD PASS、QEMU A PASS|Arm 3.1/3.2/5.1|
|TZ-SYS-02|baremetal/el3_monitor/monitor.c; baremetal/tests/register_context.S; baremetal/nonsecure_app/main.c|実装・BUILD PASS、QEMU A PASS|Arm 3.2/3.7|
|TZ-SYS-03|baremetal/nonsecure_app/main.c; common/el1_fault.c; common/probe.S; platform/zcu106/protection.c, hardware_tests.c|部分境界試験実装・BUILD PASS、QEMU A PASS|Arm 3.4/4.1/4.3; AMD UG1085|
|TZ-SYS-04|platform/zcu106/protection.c, hardware_tests.c（設定・違反取得・実probe、実ボード未接続）|QEMU BLOCKED、実機 NOT_RUN|Arm 4.1/4.3; AMD XPPU章節未照合|
|TZ-SYS-05|platform/zcu106/hardware_tests.c＋zdma.c/dma_validation.c（実ZDMA、実ボード未接続）|virtでZynq DMAはSKIP、実機 NOT_RUN|Arm 4.4; AMD DMA/XMPU章節未照合|
|TZ-SYS-06|platform/zcu106/hardware_tests.c＋gic.c/test_vectors.S（実SGIとvector、world routing未接続）|BLOCKED、実機 NOT_RUN|Arm 4.6/4.7; AMD GIC章節未照合|
|TZ-SYS-07|docs/zcu106_execution.md（手順案）|BLOCKED、実機 NOT_RUN|Arm 4.8; AMD debug章節未照合|
|TEE-01|optee_standalone/client/client.c; optee_standalone/tests/main.c; optee_standalone/ta/ta.c|Core/TA/client BUILD PASS、QEMU B2 PASS|OP-TEE Core SMC/Shared memory/RPC|
|TEE-02|optee_standalone/ta/ta.c; optee_standalone/tests/main.c|試験トークンpolicyのみ、全体BLOCKED|Arm 5.1.1; OP-TEE session ABI|
|TEE-03|ta/security.c、pta/validation_canary.c、tests/security_tests.c|QEMU Phase 5 PASS：TA間とCore canaryへの実read/write拒否と非改変確認|固定OP-TEE sourceとPhase 5報告|
|TEE-04|なし（Early TAは本試験対象外）|BLOCKED|OP-TEE TA signing、固定版章節未照合|
|TEE-05|optee_standalone/ta/ta.c run_crypto_test; optee_standalone/tests/main.c|AES/KAT実行成功、key攻撃未実装、BLOCKED|OP-TEE Internal API crypto、固定版章節未照合|
|TEE-06|なし（未実装）|backend/RPCなし、BLOCKED|OP-TEE secure storage、固定版章節未照合|
|TEE-07|optee_standalone/tests/main.c; optee_standalone/ta/ta.c|QEMU B2 PASS：TA-A panic/TA-B継続/TA-A reopen|OP-TEE TA panic/target dead、固定版章節未照合|
|TEE-08|docs/zcu106_execution.md（手順案）|NOT_RUN、boot chain未構成|Arm 5.3; AMD boot認証章節未照合|
|PERF-01|common/benchmark/benchmark.c; baremetal/nonsecure_app/main.c|QEMU測定済|Arm 3.2/3.7|
|PERF-02|common/benchmark/benchmark.c; baremetal/nonsecure_app/main.c|QEMU測定済、64-store限定|Arm 3.4/3.5|
|PERF-03|なし（未実装）|BLOCKED|Arm 4.6/4.7|
|PERF-04|なし（未実装）|BLOCKED|AMD DMA章節未照合|
|PERF-05|optee_standalone/tests/main.c; common/benchmark/benchmark.c|QEMU測定済|OP-TEE message ABI|
|PERF-06|optee_standalone/tests/main.c; optee_standalone/ta/ta.c|Core/TA/client BUILD PASS、QEMU測定済|OP-TEE crypto API|
|PERF-07|なし（未実装）|BLOCKED|OP-TEE secure storage|

B1の補助試験 `B1-SMCCC-01` は `tfa_standalone/integration/main.c`。全TZ項目の代替にはしない。
性能統計はwarmup 16、samples 128、nearest-rank p95/p99。CPU Hzは未取得でunknownを出す。

## Phase 4の構成別実行結果

|ID|A|B1|B2|証跡／実装範囲|
|---|---|---|---|---|
|TZ-SYS-01/02/03|PASS|BLOCKED|対象外|baremetal.log、独立状態／反復切替／Secure RAM拒否と非改変|
|B1-SMCCC-01|対象外|PASS|対象外|tfa.log、SMCCC/PSCI標準応答|
|TZ-SYS-04/06/07|BLOCKED|BLOCKED|対象外|周辺保護・IRQ分離・debug未試験|
|TZ-SYS-05|SKIP|SKIP|対象外|ZynqMP DMAをvirtで再現しない|
|TEE-01|対象外|対象外|PASS|optee.log、複数session/echo/不正command|
|TEE-02|対象外|対象外|BLOCKED|token認可smoke成功、trusted requester認証なし|
|TEE-03/04（Phase 4時点）|対象外|対象外|BLOCKED|Phase 5でTEE-03はfixture範囲でPASS、TEE-04 runtimeは未実装|
|TEE-05|対象外|対象外|BLOCKED|AES/KAT成功、鍵抽出攻撃なし|
|TEE-06/08|対象外|対象外|BLOCKED|storage／認証bootなし|
|TEE-07|対象外|対象外|PASS|optee.log/secure.log、TA-A panic/TA-B継続/TA-A復旧|
|PERF-01/02|測定済|未測定|未測定|baremetal-benchmarks.csv|
|PERF-05/06|対象外|対象外|測定済|optee-benchmarks.csv|
|PERF-03/04/07|未測定|未測定|未測定|backend未接続|

上表の対象外は機能の実行結果を主張しないという意味。ZCU106は全項目NOT_RUN、ホスト91件は検査基盤のみ。B2追加ソースは `platform/qemu/external/client_mmu.c`、構成別配置は同directoryのheader/linker/JSON。期待値定義は従来どおり独立JSONを使用する。

## Phase 5の追加試験

|ID|実装|QEMU結果／制約|
|---|---|---|
|TEE-AUTH-TA|ta/security.c trusted_read/trusted_service_test、client.c tee_probe_identity_open、tests/security_tests.c|PASS：Core設定のTA identity、TA-A allowlist、PUBLICと偽装loginを拒否|
|TEE-AUTH-NS|未実装|BLOCKED：Normal要求者credentialsなし。TEE-02全体もBLOCKED|
|TEE-ISO-TA|ta/security.c private_secretと実load/store、tests/security_tests.c probe_other_ta|PASS：実Data Abort、Bのprivate値維持、A再open、FAR/ESR照合|
|TEE-ISO-CORE|ta/security.cの実read/write、pta/validation_canary.c|PASS：実Core canaryへ攻撃し特権側から各非改変を確認。TEE-03もfixture範囲でPASS|
|TEE-KEY-API|ta/security.c security_create/key_policy_test/KEY_EXPORT、ta.c AES KAT|PASS：非抽出usage、API export拒否、鍵利用とKAT|
|TEE-KEY-MEM|未実装|BLOCKED：raw memory抽出なし。TEE-05全体もBLOCKED|
|TEE-SHM-01|client.c TMEM、ta/security.c BUFFER_CHECK、tests/security_tests.c|PASS：正常、不整合size、範囲外、Secure、overflow、継続と非改変|
|通常TA署名ホスト検査|scripts/check_signed_ta.py＋固定版sign_encrypt.py|5ケース成功、HOSTのみ。TEE-04 runtimeはBLOCKED|

期待値は独立した `tests/expectations.json`。Secure abort証跡は `scripts/check_security_evidence.py` がaddress/direction/classを検査し、関連単体試験は `tests/test_phase5.py`。詳細と前提は [Phase 5](phase5_progress.md)。ZCU106は全NOT_RUN。

Phase 6の特性・拒否機構・非改変・対象範囲・source・証跡は [根拠表](security_evidence_phase6.md)。性能は [測定区間表](performance_phase6.md)。実機は全NOT_RUN。

Phase 7のTEE-STRESS-01はoptee_standalone/tests/stress_tests.c、clientのdead-session CLOSE、TAのkey cycle、明示的なPTA statsを使用。最新結果はresults/phase7/latest-report.md。TZ-SYS-03はcheck_tz_audit.pyでMMU-off External Abortを要求しTranslation Faultを合格にしない。

Phase 8では同じ機能試験をlocal Git source受領先からfresh buildして再実行した。IDごとの期待値・実値・ticks・UARTとbuild証跡は [Phase 8結果表](../results/phase8/results.json)、範囲と制約は [報告](phase8_progress.md)。QEMU A 3 PASS、B1 1 PASS、B2 9 PASSであり、既存BLOCKED/SKIPと全実機NOT_RUNを維持する。Vitis/GNU/Ubuntu24.04のbuildはNOT_RUN。

## B2統合再検証

TF-A＋OP-TEE＋Standaloneは既存B2そのもの。最新のsource-only再buildとQEMU A3/B1 1/B2 9 PASS、stress512回、host142件の証跡は[統合集計](../results/b2-integration-summary.json)、接続と未完了範囲は[統合手順](b2_integration.md)。既存TEEのBLOCKED項目と全実機NOT_RUNを維持する。Core canary/統計PTAはQEMU検証overlayに限定し、通常ZCU106 Coreの試験済みとしない。GPIO/DMAは保留。
