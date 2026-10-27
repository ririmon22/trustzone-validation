# Phase 7 再現性・検証妥当性の整備

2026-10-27。実機は使用していない。Ubuntu上のfresh source/build workspaceで2回の再構築・実行を確認した。Podman内のclean OS buildは実行環境の制限でBLOCKEDであり、Phase 7の全完了条件を満たしたとはしない。

## 実際の結果

|項目|結果|証跡|
|---|---|---|
|開始時監査|Phase 6の19成果物のSHA-256が全一致、host116件成功|results/phase7/baseline|
|clean build|生成物をコピーしない2つの新規workspaceで12 ELF、ZynqMP/QEMU TF-A、SDK/TA/OP-TEEを生成|clean-workspace.json、各export先のbuild/external logs|
|host試験|116件成功を維持。試験数増加を本フェーズの成果とはしない|export先host-tests.log|
|QEMU A|3 PASS、0 FAIL、1 SKIP、3 BLOCKED|latest-report.md、export先qemu/baremetal.log|
|QEMU B1|1 PASS、0 FAIL、1 SKIP、6 BLOCKED|export先qemu/tfa.log|
|QEMU B2|9 PASS、0 FAIL、7 BLOCKED。既存8 PASS＋反復試験1 PASS|export先qemu/optee.logとSecure log|
|メモリ再監査|TZ-SYS-03を限定範囲で維持。DFSC=0x10のExternal Abort、MMU/Stage2 off、FAR一致、Secure再確認|tz-audit.json、qemu-mtree.log、audit-negative-checks.json|
|TEE反復|32 warmup＋512 iteration。net heap/TA RAM増加なし、malloc失敗増加なし|stress.json、STRESS_META/PROGRESS/RESOURCE、RESULT ticks|
|性能|8系列各128 sample、overhead各128。rawから独立再計算一致|performance.md、overhead.json、statistics-check.json|
|署名ホスト検査|正常・改ざん・別鍵・別UUIDの5ケース成功|signed-ta/results.json。runtime署名ロードとは別|
|Podman|version5.7.0。初期化clone EPERMでimage build/run BLOCKED|podman-version.log、podman-info.log、podman-isolated-info.log|
|実機|全NOT_RUN、load BLOCKED|export先boot-plan-check.json、既存profile/mapゲート|

`results/phase7/clean-workspace.json` が最新workspaceとexport先を指す。最初のrunはresults/phase7/clean、2回目は別のclean-* directoryに保存した。`latest-report.md` はexportした実logへ参照を補正する。初期のincremental/audit logとclean成功runを区別する。source hash・実toolchain・各ELF hashを記録し、別pathや署名等によるbit差を隠していない。

## 成果物

1. [開発環境手順](phase7_environment.md)：host clean build、offline source、tool pin、container制約。
2. container/Containerfile、packages.lock、toolchain.lock.json、base.lock.json、scripts/podman_phase7.sh。
3. scripts/run_phase7.sh：fresh source staging→全build→memory/host/QEMU→性能→log→集計→Markdown。built workspaceはvalidate_phase7_built.shで再実行できる。
4. [QEMUレポート](../results/phase7/latest-report.md)とexportされたresults.json。各IDを実logに対応付ける。
5. [メモリ保護再監査](phase7_memory_audit.md)。単なるTranslation Faultではないことを確認。
6. [TEE反復・性能説明](phase7_stress_and_performance.md)とstress.json。無期限・全allocatorのleak freedomとはしない。
7. [Vitis package説明](phase7_vitis_package.md)、vitis-source-package.tar.gz、vitis-package.json。構成別component manifestとsource hash。
8. [hardware設定確認表](phase7_hardware_settings.md)：CONFIRMED／ASSUMED／REQUIRES_HARDWAREを区別。
9. 性能source、8系列の結果、overhead、独立統計check。QEMU値のみ。
10. 以下の未完了一覧とphase6_hardware_checklist.md。安全ゲートを維持。

## 未完了と次の具体的作業

- **Container検証はBLOCKED**：normal Podman対応hostでUbuntu26.04 amd64の実base digestを取得・保存し、exact package版が取得できることを確認してimage build/runする。本sessionでは保護設定を変更せず、host OS全体の制限とも断定していない。base.lockは未確認のままnull/false。コンテナ未検証をhost workspace成功で代用しない。
- 既存hostは全toolを備えており、fresh workspaceでのsource/生成物再現は確認したが、pristine OSへ依存をinstallする工程は未実行。package closureとAPT repositoryの長期保存も別途必要。
- 実機FSBL/PMUFW予約、実BL32、TF-A/OP-TEE/BL33配置、XMPU全port/master、XPPU profiles、DMA/GIC経路、EL/boot順序は未確定。実機ゲートはBLOCKED、試験・DMA/IRQ実機性能はNOT_RUN。
- Normal要求者認証、鍵memory抽出/消去耐性、通常署名TA runtimeロード、storage/Secure Bootは引き続き未完了。TA identity、export API拒否、object lifecycle、host signature verifyと混同しない。
- TEE耐久は記録した512 iterationとwarmup・既存benchmarkの範囲。時間単位のovernight耐久、SMP、RPC、動的SHM/fuzzingは未実施。

本フェーズで安全ゲート・eFUSE・host security policyを変更していない。の実機では生成hardware/live設定の収集と配置照合から始め、未確定値を推測で埋めない。
