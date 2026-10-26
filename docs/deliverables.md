# 成果物一覧（Phase 4）

|成果物|所在|状態|
|---|---|---|
|自作ソース／Makefile／linker|baremetal、common、platform、tfa_standalone、optee_standalone|既存10 ELFと外部BL33 2 ELF BUILD PASS|
|メモリ配置／検査|platform/*、scripts/check_memory_map.py、prepare_qemu_firmware.py|QEMU A/B1/B2照合、ZCU106暫定・ロードBLOCKED|
|QEMU実行／一括build|scripts/run_qemu.sh、run_phase4.sh|3構成の実ログ取得済|
|ZCU106/Vitis手順|docs/zcu106_execution.md、vitis_2025_1_porting.md、zcu106_phase4_handoff.md|実機/IDE未検証、確認票あり|
|TF-A構築|docs/tfa_build.md、scripts/build_external.py|ZynqMP BL31/QEMU B1/B2 BUILD PASS|
|OP-TEE／Early TA構築|docs/optee_build.md、optee_standalone/ta|QEMU Core/SDK/TA-A/B BUILD PASS|
|Standalone TEE client|optee_standalone/client、platform/qemu/external/client_mmu.c|QEMU session/TA実行済、RPC/memref等未対応|
|検証一覧／判定|docs/test_matrix.md、tests/expectations.json、common/test_runner、scripts/parse_results.py|ホスト91試験、QEMU A3/B1補助1/B2 2項目PASS|
|性能|common/benchmark、scripts/analyze_benchmarks.py|QEMU PERF-01/02/05/06測定済、03/04/07未完了|
|build/run log|results/phase4、results/qemu|実ログ、失敗attemptも保存|
|source provenance|third_party/dependencies.lock.json、results/phase4/provenance|固定commit build subset、blob/mode検証|
|個別結果／制約|results/status.json、report.md、docs/known_limitations.md|未実行・未完成を区別|

HARDWAREは全NOT_RUN。最終要求の全機能完成ではない。詳細は [Phase 4報告](phase4_progress.md)。

最新Phase 5ではホスト105件、B2 8 PASS/0 FAIL/7 BLOCKED。Core canary PTAの明示的なbuild overlay、TA identity、TMEM input、鍵export policy、TA/Core memory probe、ホスト署名検査を追加した。[Phase 5報告](phase5_progress.md)と `results/phase5` を参照。
