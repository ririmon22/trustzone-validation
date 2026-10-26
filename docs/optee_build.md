# OP-TEEとStandalone統合

固定upstream4.5.0 commitはlock参照。AMD/Vitis/ZCU106との互換性は未確認。QEMU用Core、ARM64 SDK、TA-A/BをClang21.1.8でビルドしLinuxなしで実行した。

```sh
python3 scripts/source_snapshot.py optee pyelftools compiler-rt
make llvm-runtime qemu-external
python3 scripts/build_external.py optee-sdk
python3 scripts/build_external.py ta-a
python3 scripts/build_external.py ta-b
python3 scripts/build_external.py optee \
    --early-ta build/external/ta-a/6dc13418-534f-4642-9138-215876304910.stripped.elf \
    --early-ta build/external/ta-b/6dc13419-534f-4642-9138-215876304910.stripped.elf
python3 scripts/build_external.py tfa --configuration b2
bash scripts/run_qemu.sh optee
```

SDK：`build/external/optee/export-ta_arm64`。Core：`build/external/optee/core/tee.elf`とv2 binary群。各TAは `build/external/ta-{a,b}`。builderは固定版ソース、tools、SDK、Early TA存在を検査しcommandと終了値を `results/phase4/external-*.log`へ保存する。実際のbuild依存は `PYTHONPATH=third_party/pyelftools`、Python cryptography、LLVM compiler-rtの128-bit除算実装。ダミーruntimeを使用しない。

設定は `PLATFORM=vexpress-qemu_armv8a CFG_ARM64_core=y CFG_USER_TA_TARGETS=ta_arm64 CFG_TEE_CORE_NB_CORE=1`。REE FS/RPMB/REE TA loading/PKCS11/in-tree TA/async notificationを無効化し、Early TAを有効化した。software PRNGはQEMU用であり実機のentropy品質を証明しない。

|ABI機能|実装／実測|制限|
|---|---|---|
|SMC CALL_WITH_ARG/capability/GET_SHM_CONFIG|実行成功|単一CPU・同期のみ|
|静的予約SHM|0x42000000..0x42200000、cached応答確認|外部B2のみNS MMU/cache WB属性を設定、旧raw clientはcached応答を拒否|
|session open/invoke/close|2sessionと異常command、再open成功|max4、value parameterのみ|
|動的SHM/memref|未実装|Coreが対応してもclientは利用しない|
|RPC/foreign IRQ|未実装、検出時transport停止|該当service BLOCKED|
|TA policy|test tokenの許可/拒否、終了後拒否成功|利用者認証ではない、TEE-02 BLOCKED|
|TA crypto|AES生成鍵往復とAES-128 ECB零鍵/零平文の既知解成功|鍵抽出/削除攻撃未実装、TEE-05 BLOCKED|
|TA fault isolation|TA-A panic、TA-B継続、TA-A reopen成功|TEE-07 PASS、メモリアクセス分離試験TEE-03とは別|
|REE FS/RPMB|未実装|TEE-06/PERF-07 BLOCKED|
|通常署名TAロード|未実装|TEE-04 BLOCKED|

TAの署名済み`.ta`ファイルはdevelopment keyでbuildされるが、実際の起動は `.stripped.elf` のEarly TA組込み。[OP-TEE 4.5.0 Trusted Applications / TA locations](https://optee.readthedocs.io/en/4.5.0/architecture/trusted_applications.html#ta-locations)で説明される別経路であり、署名検証やSecure BootにPASSを付けない。

Secure Core UARTに構成警告と意図したTA panicを保存する。Normal UARTのFATALやEND欠落は実行失敗。ログと未完成項目は [Phase 4報告](phase4_progress.md)。

## Phase 5

最新の再現は `bash scripts/run_phase5.sh`。追加TA source `security.c` をSDKでビルドし、従来と同じ2 UUIDをEarly TAへ組み込む。clientは静的SHMのTMEM_INPUTにも対応し、通知された範囲とmetadata領域の重複を検査する。任意pointer送信API、秘密値アドレス公開、TA間/Core probeは検証専用。

新しいログは `results/phase5/qemu` に分離する。`run_qemu.sh optee` は補助ID欠落とSecure FAR/ESR不一致も実行失敗とする。第三者ソースの変更や保護ゲートの無効化は行わない。詳細は [Phase 5](phase5_progress.md)。

Core canaryを用いる最終Phase 5 buildは `build_external.py optee --validation-pta`。原本検証後、`build/source/optee-validation`へ明示的なoverlayを適用してbuildし、変更内容とhashをログへ保存する。CoreのpointerをPTAから取得し、ASLR後の実virtual addressを使用する。
