# B2全試験ID・実装棚卸し

機械可読の定義は[tests/b2_inventory.json](../tests/b2_inventory.json)、期待値は[tests/expectations.json](../tests/expectations.json)。実装の有無と実行結果を分離する。全42 IDを分類し、B2は23機能ID＋8性能IDを現在出力する。A/B1専用8 IDとB2でbackend不足の性能3 IDも省略しない。

## 判定の範囲

- IMPLEMENTEDはsourceと出力入口が存在する意味。静的検査だけで機能PASSにはしない。
- CURRENTは現行Standalone/Early TA/同期SMCのまま実施可能。今回その範囲の既知の未実装4機能と2測定を追加した。
- VALIDATION_OVERLAYはQEMU用の明示的Core canary/stats PTAが必要。通常ZCU106 Coreと区別する。
- NEEDS_BACKEND/DEFERREDは不足backendまたは保留。部分的な代替PASSで親特性を昇格しない。
- A_ONLY/B1_ONLYは現在その構成だけにIDが割り当てられている。B2で同じ保護・割込み・debug試験が完了したという意味ではない。新しいSecure特権serviceや実機保護経路が必要な移植は別の構成拡張として扱う。

## 全ID一覧

QEMU欄は今回の実行証跡の範囲。ZCU106はすべてNOT_RUNで、load gateもBLOCKED。性能のMEASUREDは機能PASSではない。PERF-03/04/07のBLOCKEDはbackend不足による構成判定であり、実行logにRESULT行があるという意味ではない。

|ID|B2実装/実施条件|source・主関数|QEMU|残る理由・範囲|
|---|---|---|---|---|
|TEE-01|IMPLEMENTED / CURRENT|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>`standalone_main`|PASS|2セッション・echo・未知コマンド拒否。正常復帰と返却値を照合する。|
|TEE-02|PARTIAL / NEEDS_BACKEND|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>[optee_standalone/ta/security.c](../optee_standalone/ta/security.c)<br>`standalone_main`, `security_invoke`|BLOCKED|TA認可と固定token smokeは実装済み。信頼できるNormal要求者認証・credential provisionは未実装。|
|TEE-05|PARTIAL / NEEDS_BACKEND|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>[optee_standalone/ta/security.c](../optee_standalone/ta/security.c)<br>`standalone_main`, `security_invoke`|BLOCKED|AES既知解・鍵使用・API拒否は実装済み。raw鍵メモリ抽出耐性は未実装。|
|TEE-04|NOT_IMPLEMENTED / NEEDS_BACKEND|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>`standalone_main`|BLOCKED|通常署名TAの実runtime loader/RPCがない。Early TA組込みとhost署名検査では代用しない。|
|TEE-06|NOT_IMPLEMENTED / NEEDS_BACKEND|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>`standalone_main`|BLOCKED|永続storage backend/RPC、再起動・改ざん検査が未実装。|
|TEE-08|NOT_IMPLEMENTED / NEEDS_BACKEND|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>`standalone_main`|BLOCKED|認証付きboard boot chain未構成。JTAG起動を代替PASSにしない。|
|TEE-07|IMPLEMENTED / CURRENT|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>`standalone_main`|PASS|Aのpanic検出、B継続、Aの再open/echoを照合する。|
|TEE-AUTH-TA|IMPLEMENTED / CURRENT|[optee_standalone/tests/security_tests.c](../optee_standalone/tests/security_tests.c)<br>[optee_standalone/ta/security.c](../optee_standalone/ta/security.c)<br>`test_identity`, `trusted_read`, `trusted_service_test`|PASS|Core管理identityとTA-A allowlistで認可し、PUBLIC・偽loginを拒否する。|
|TEE-SHM-01|IMPLEMENTED / CURRENT|[optee_standalone/tests/security_tests.c](../optee_standalone/tests/security_tests.c)<br>`test_shared_memory`|PASS|正常・短いサイズ・範囲外・Secure・wrap指定、拒否元・非改変・継続を検査する。|
|TEE-KEY-API|IMPLEMENTED / CURRENT|[optee_standalone/tests/security_tests.c](../optee_standalone/tests/security_tests.c)<br>[optee_standalone/ta/security.c](../optee_standalone/ta/security.c)<br>[optee_standalone/ta/ta.c](../optee_standalone/ta/ta.c)<br>`test_key_api`, `key_policy_test`, `run_crypto_test`|PASS|Secure生成鍵、非extractable usage、独自export拒否、AES既知解と鍵継続利用。|
|TEE-ISO-TA|IMPLEMENTED / CURRENT|[optee_standalone/tests/security_tests.c](../optee_standalone/tests/security_tests.c)<br>`probe_other_ta`|PASS|許可された自己アクセス、VA alias排除、実read/write fault、被害TAの非改変・再利用。|
|TEE-AUTH-NS|NOT_IMPLEMENTED / NEEDS_BACKEND|[optee_standalone/tests/security_tests.c](../optee_standalone/tests/security_tests.c)<br>`run_security_tests`|BLOCKED|信頼できるNormal要求者credentialのprovisionがない。|
|TEE-KEY-MEM|NOT_IMPLEMENTED / NEEDS_BACKEND|[optee_standalone/tests/security_tests.c](../optee_standalone/tests/security_tests.c)<br>`run_security_tests`|BLOCKED|raw memoryからの鍵抽出攻撃と鍵非改変の検査は未実装。|
|TEE-03|IMPLEMENTED / VALIDATION_OVERLAY|[optee_standalone/tests/security_tests.c](../optee_standalone/tests/security_tests.c)<br>[optee_standalone/pta/validation_canary.c](../optee_standalone/pta/validation_canary.c)<br>`run_security_tests`, `invoke_canary`|PASS|TA分離＋Core canaryの合成試験。通常ZCU106 Coreには検証PTAがなく、QEMU overlayと区別する。|
|TEE-ISO-CORE|IMPLEMENTED / VALIDATION_OVERLAY|[optee_standalone/tests/security_tests.c](../optee_standalone/tests/security_tests.c)<br>[optee_standalone/pta/validation_canary.c](../optee_standalone/pta/validation_canary.c)<br>`run_security_tests`, `invoke_canary`|PASS|検証用Core canary PTAとFAR/ESRが必要。通常ZCU106 portではbackend未接続。|
|TEE-STRESS-01|IMPLEMENTED / VALIDATION_OVERLAY|[optee_standalone/tests/stress_tests.c](../optee_standalone/tests/stress_tests.c)<br>[optee_standalone/pta/validation_canary.c](../optee_standalone/pta/validation_canary.c)<br>`cycle`, `run_stress_tests`, `invoke_canary`|PASS|32 warmup＋512回。Core heap/TA RAM/割当失敗数の統計は検証PTAが必要。|
|TEE-SESSION-01|IMPLEMENTED / CURRENT|[optee_standalone/tests/boundary_tests.c](../optee_standalone/tests/boundary_tests.c)<br>`run_boundary_tests`|PASS|client上限4、重複open、local closed handle、Coreへのclose完了。Coreの容量上限試験ではない。|
|TEE-ABI-01|IMPLEMENTED / CURRENT|[optee_standalone/tests/boundary_tests.c](../optee_standalone/tests/boundary_tests.c)<br>`run_boundary_tests`|PASS|Coreが不正attr、TAが未知commandを拒否。u32境界echoと継続を検査する。|
|TEE-BOUNDARY-01|IMPLEMENTED / CURRENT|[optee_standalone/tests/boundary_tests.c](../optee_standalone/tests/boundary_tests.c)<br>`run_boundary_tests`|PASS|交渉済みSHM末尾、1byte超過、0size、message重複、内容非改変。|
|B2-SMCCC-01|IMPLEMENTED / CURRENT|[optee_standalone/tests/extended_tests.c](../optee_standalone/tests/extended_tests.c)<br>[common/arch.c](../common/arch.c)<br>`run_extended_tests`, `smc_call`|PASS|統合BL31でSMCCC/PSCI versionを読む。PSCI電源遷移は対象外。|
|TEE-SESSION-CORE-01|IMPLEMENTED / CURRENT|[optee_standalone/tests/extended_tests.c](../optee_standalone/tests/extended_tests.c)<br>[optee_standalone/client/client.c](../optee_standalone/client/client.c)<br>`run_extended_tests`, `tee_probe_session_id`|PASS|close直後のraw IDをCoreが拒否、他TA継続、再openの認可状態初期化。|
|TEE-PARAM-01|IMPLEMENTED / CURRENT|[optee_standalone/tests/extended_tests.c](../optee_standalone/tests/extended_tests.c)<br>[optee_standalone/ta/security.c](../optee_standalone/ta/security.c)<br>[optee_standalone/ta/ta.c](../optee_standalone/ta/ta.c)<br>`run_extended_tests`, `security_invoke`, `TA_InvokeCommandEntryPoint`|PASS|transport有効だがTA型不正、内容不正を拒否し、非改変・修復後の成功を照合。|
|TEE-KEY-ATTR-01|IMPLEMENTED / CURRENT|[optee_standalone/tests/extended_tests.c](../optee_standalone/tests/extended_tests.c)<br>[optee_standalone/ta/security.c](../optee_standalone/ta/security.c)<br>`run_extended_tests`, `key_attribute_test`|PASS|許可control鍵の属性取得成功、非extractable秘密属性のCore拒否、buffer非改変、鍵継続利用。鍵bytesはNSへ返さない。|
|PERF-01|IMPLEMENTED / CURRENT|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>[common/benchmark/benchmark.c](../common/benchmark/benchmark.c)<br>`standalone_main`, `benchmark_run`, `benchmark_run_scoped`|MEASURED|B2専用fast SMC往復。EL3/Core dispatchとSHM範囲検査を含み、pure切替時間ではない。|
|PERF-02|IMPLEMENTED / CURRENT|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>[common/benchmark/benchmark.c](../common/benchmark/benchmark.c)<br>`standalone_main`, `benchmark_run`, `benchmark_run_scoped`|MEASURED|Normalのcached SHMへ64 u64 store＋DSB。範囲queryを含む。Secure DDRの性能ではない。|
|PERF-05|IMPLEMENTED / CURRENT|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>[common/benchmark/benchmark.c](../common/benchmark/benchmark.c)<br>`standalone_main`, `benchmark_run`, `benchmark_run_scoped`|MEASURED|TA echo invokeをmessage/SMC込みで測定。|
|PERF-06|IMPLEMENTED / CURRENT|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>[common/benchmark/benchmark.c](../common/benchmark/benchmark.c)<br>`standalone_main`, `benchmark_run`, `benchmark_run_scoped`|MEASURED|TA AES生成鍵往復＋別既知鍵KAT。transportを含む。|
|PERF-08|IMPLEMENTED / CURRENT|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>[common/benchmark/benchmark.c](../common/benchmark/benchmark.c)<br>`standalone_main`, `benchmark_run`, `benchmark_run_scoped`|MEASURED|他sessionを維持したTA instanceのopenだけを測定。初回loadではない。|
|PERF-09|IMPLEMENTED / CURRENT|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>[common/benchmark/benchmark.c](../common/benchmark/benchmark.c)<br>`standalone_main`, `benchmark_run`, `benchmark_run_scoped`|MEASURED|他sessionを維持したcloseだけを測定。最終instance破棄ではない。|
|PERF-10|IMPLEMENTED / CURRENT|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>[common/benchmark/benchmark.c](../common/benchmark/benchmark.c)<br>`standalone_main`, `benchmark_run`, `benchmark_run_scoped`|MEASURED|panic診断・dead session回収・再open・echoを含む。最後のcloseを除外。|
|PERF-11|IMPLEMENTED / CURRENT|[optee_standalone/tests/main.c](../optee_standalone/tests/main.c)<br>[common/benchmark/benchmark.c](../common/benchmark/benchmark.c)<br>`standalone_main`, `benchmark_run`, `benchmark_run_scoped`|MEASURED|TARGET_DEAD観測・回収後から再open/echoまで。panic診断時間を除外。|
|PERF-03|NOT_IMPLEMENTED / NEEDS_BACKEND|[docs/b2_jtag_test_plan.md](../docs/b2_jtag_test_plan.md)<br>―|BLOCKED|Secure IRQ発生・routing・確認済みhandlerの接続が未完了。|
|PERF-04|NOT_IMPLEMENTED / DEFERRED|[docs/b2_jtag_test_plan.md](../docs/b2_jtag_test_plan.md)<br>―|BLOCKED|DMA route/masterは実機未確認、検証保留。|
|PERF-07|NOT_IMPLEMENTED / NEEDS_BACKEND|[docs/b2_jtag_test_plan.md](../docs/b2_jtag_test_plan.md)<br>―|BLOCKED|storage backend/RPCがないため測定未実装。|
|TZ-SYS-01|OTHER_CONFIGURATION / NOT_APPLICABLE|[baremetal/nonsecure_app/main.c](../baremetal/nonsecure_app/main.c)<br>`nonsecure_main`|対象外|現在はAのシステム試験に割り当て。B2は出力しない。TAサービス/分離の成功を同じ特性のB2 PASSへ流用しない。|
|TZ-SYS-02|OTHER_CONFIGURATION / NOT_APPLICABLE|[baremetal/nonsecure_app/main.c](../baremetal/nonsecure_app/main.c)<br>`nonsecure_main`|対象外|現在はAのシステム試験に割り当て。B2は出力しない。TAサービス/分離の成功を同じ特性のB2 PASSへ流用しない。|
|TZ-SYS-03|OTHER_CONFIGURATION / NOT_APPLICABLE|[baremetal/nonsecure_app/main.c](../baremetal/nonsecure_app/main.c)<br>`nonsecure_main`|対象外|現在はAのシステム試験に割り当て。B2は出力しない。TAサービス/分離の成功を同じ特性のB2 PASSへ流用しない。|
|TZ-SYS-04|OTHER_CONFIGURATION / NOT_APPLICABLE|[baremetal/nonsecure_app/main.c](../baremetal/nonsecure_app/main.c)<br>`nonsecure_main`|対象外|現在はAのシステム試験に割り当て。B2は出力しない。TAサービス/分離の成功を同じ特性のB2 PASSへ流用しない。|
|TZ-SYS-05|OTHER_CONFIGURATION / NOT_APPLICABLE|[baremetal/nonsecure_app/main.c](../baremetal/nonsecure_app/main.c)<br>`nonsecure_main`|対象外|現在はAのシステム試験に割り当て。B2は出力しない。TAサービス/分離の成功を同じ特性のB2 PASSへ流用しない。|
|TZ-SYS-06|OTHER_CONFIGURATION / NOT_APPLICABLE|[baremetal/nonsecure_app/main.c](../baremetal/nonsecure_app/main.c)<br>`nonsecure_main`|対象外|現在はAのシステム試験に割り当て。B2は出力しない。TAサービス/分離の成功を同じ特性のB2 PASSへ流用しない。|
|TZ-SYS-07|OTHER_CONFIGURATION / NOT_APPLICABLE|[baremetal/nonsecure_app/main.c](../baremetal/nonsecure_app/main.c)<br>`nonsecure_main`|対象外|現在はAのシステム試験に割り当て。B2は出力しない。TAサービス/分離の成功を同じ特性のB2 PASSへ流用しない。|
|B1-SMCCC-01|OTHER_CONFIGURATION / NOT_APPLICABLE|[tfa_standalone/integration/main.c](../tfa_standalone/integration/main.c)<br>`standalone_main`|対象外|B1専用IDを保持。B2の実測は別ID B2-SMCCC-01で記録する。|

## 今回埋めた実装可能な項目

|追加ID|以前足りなかった検査|現在のPASS条件|
|---|---|---|
|B2-SMCCC-01|B1だけの標準呼出しsmoke|B2統合BL31でSMCCC/PSCI version応答。電源遷移は試験しない|
|TEE-SESSION-CORE-01|local closed flagだけではCoreの拒否を証明しない|close成功直後・ID再利用前にraw IDを送ってCore origin/BAD_PARAMETERS、他TA継続、再openの認可state初期化|
|TEE-PARAM-01|不正attrではCoreで止まり、TAの型/内容検査を通らない|Core有効なVALUE_INPUTをTAが拒否、正しい型の不正内容をTAが拒否、buffer非改変、修復後成功|
|TEE-KEY-ATTR-01|独自KEY_EXPORTコマンドの拒否だけではCore鍵usageの強制を試していない|Secure内の許可control鍵属性取得成功、非extractable秘密属性取得拒否、出力buffer不変、鍵継続利用。鍵bytesはNSへ返さない|
|PERF-01（B2）|Aの独自monitor往復しか測定していなかった|OP-TEE GET_SHM_CONFIG fast SMCを測る。EL3/Core dispatchと範囲検査を含み、TA呼出しやpure world-switch時間と混同しない|
|PERF-02（B2）|AのMMU/cache off memoryしか測定していなかった|Normalのcached SHM末尾へ64 u64 store＋DSB。範囲queryを含む。Secure DDR性能ではない|

追加先は[extended_tests.c](../optee_standalone/tests/extended_tests.c)、[client.c](../optee_standalone/client/client.c)、[security.c](../optee_standalone/ta/security.c)、[main.c](../optee_standalone/tests/main.c)。鍵試験は固定OP-TEE 4.5の内部_utee_cryp_obj_get_attrを検証目的で使用する。通常TEE_GetObjectBufferAttributeはこの拒否をpanicへ変換するため、その経路と区別した。独自暗号実装は追加していない。

## 残る未実装・BLOCKED

信頼できるNormal要求者認証/credential provision、raw key memory抽出、署名通常TAの実runtime load、永続storage/RPC、認証付き起動、Secure IRQ実経路、DMAは未実装または未接続のまま。現在の期待条件を満たすには新しいbackend・構成・実機情報が必要で、固定token/API拒否/Early TA/host署名検査/停止PCだけで同義のPASSにしない。GPIO/DMAは保留。

TA分離のtranslation faultはTAの仮想アドレス隔離の証拠であり、TZ-SYS-03の物理TrustZone拒否の証拠ではない。FAR/ESR・読み書き方向・被害TA/Core canary非改変・継続と照合する。QEMU overlayの結果を通常ZCU106へ転記しない。

## 証跡と再検査

今回の実測はresults/jtag-addendum/b2-inventory-report.jsonに対応づける。source/ELF/command/実行logは同directoryのsummaryとsource-build receiptから追跡する。カタログ自体には実行済みというflagを入れず、実行logを別入力する。

```bash
python3 scripts/check_b2_inventory.py --output results/b2/inventory-static.json
```

実行後のoptee.jsonとoptee-benchmarks.csvがある場合は`--results <実ファイル> --benchmarks <実CSV>`を追加する。現在sourceの出力IDが欠ける旧logは、現在版を実行した証明として受理しない。compile成功・QEMU実行成功・ZCU106実行成功を分ける。

性能系列は構成＋IDの組で区別する。Aの2系列とB2の8系列は合計10系列。既存8系列profileの過去log再解析も維持し、欠落や重複を許すためのtoleranceは追加しない。CPU clock/IRQ/cache/区間・warmup16/sample128/OVERHEAD128を保存し、実機性能へ換算しない。
