# Phase 5 TEEセキュリティ検証報告

2026-10-26。依頼文末尾は「通常TAの署名検証経」で途切れていたため、受信した要求を対象に実装し、署名検証は確認できた範囲を明示する。Linux/PetaLinuxをターゲットに使用しない。実機ロード・不可逆設定・ホスト保護設定の変更は行っていない。

## Phase 4の監査

変更前に保存済みELFのSHA-256を照合し、記録対象の全ELFで一致した（`results/phase5/baseline/artifact-check.json`）。第三者sourceの固定commit/blob/modeを検査し、ホスト91件を再実行した。3構成を再起動してAのTZ-SYS-01/02/03、B1のSMCCC/PSCI、B2のTEE-01/07を再確認した。証跡は `results/phase5/baseline/replay`。過去のログもbaselineへコピーし、Phase 4の証跡を保存した。

ソースの判定を確認した結果、TZ-SYS-03は許可access→禁止read/write→Secure側の非改変再確認を行い、例外だけではPASSにしていない。TEE-02の固定tokenを要求元認証とはしない扱い、TEE-03/05の不足をBLOCKEDにする扱いも妥当だった。B1の補助試験を全TZ機能の代替にせず、ホスト試験と実行試験も区別していた。監査は実機での成立を証明しない。

## 最終結果

|対象|実測結果|証跡|
|---|---|---|
|自作ELF|既存12 ELF BUILD PASS、ZCU106 backend archiveも維持|`build-qemu.log`、`build-zcu106.log`、`build-clients.log`|
|ホスト基盤試験|105件成功、既存91件を含む|`host-tests.log`|
|A QEMU|TZ-SYS-01/02/03 PASS、FAIL 0、SKIP 1、BLOCKED 3|`qemu/baremetal.log`|
|B1 QEMU|B1-SMCCC-01 PASS、FAIL 0、SKIP 1、BLOCKED 6|`qemu/tfa.log`|
|B2 QEMU|PASS 8、FAIL 0、BLOCKED 7（8基本ID＋7補助ID）|`qemu/optee.log`、`qemu/optee-secure.log`|
|OP-TEE/TA/TF-A B2|固定ソースで再ビルド・組込み成功|`external-optee.log`、`external-ta-a.log`、`external-ta-b.log`、`external-tfa-qemu-b2.log`|
|通常TA署名のホスト検査|正常受理、署名改ざん／payload改ざん／別鍵／別UUIDを拒否、5ケース成功|`signed-ta/results.json`と各log|
|ZCU106|全実機試験NOT_RUN、ロードゲートBLOCKED|`zcu106-load-gate.log`、`zcu106-profile-gate.log`|

表の証跡pathは特記なき場合 `results/phase5` からの相対path。機能試験の時間は未測定（ticks=0を実測ゼロ時間とはしない）。PERF-01/02/05/06の128 sampleと統計は同directoryの構成別CSV、QEMU counter値のみ。

## 認証と認可

`TEE-AUTH-TA PASS`：認証対象は**呼出元TA**。TA-AからBへの内部sessionでは、OP-TEE Coreが実行中TAのUUIDとTEE_LOGIN_TRUSTED_APPを設定する。TA-Bは `gpd.client.identity` を読み、TRUSTED_APPかつTA-A UUIDだけを許可する。PUBLIC呼出しでAを名乗るvalueを渡しても拒否する。Normal側からOPEN_SESSION metaへTRUSTED_APPとA UUIDを偽装してもCoreがBAD_PARAMETERSで拒否することを実行した。

TA-AはPUBLICからBへの自己試験を起動できるが、Bのprivate値を内部で比較した後にゼロだけを返す。Bの秘密をNormalへ転送する汎用proxyにはしない。これはTA identityの検証であり、人間・Normal application・端末ユーザーの認証ではない。信頼の前提は現在ロードされたCoreとEarly TAのコードであり、Secure Boot/コード真正性の証明ではない。

`TEE-AUTH-NS BLOCKED`：Normalの要求者を認証するcredential provisioningや信頼できるidentity経路は未実装。従来tokenは認可smoke専用。従って総合TEE-02はBLOCKEDを維持する。

## TA分離とCoreへの攻撃

`security.c`にTA-A/Bで異なるvolatile private値を保持する。検査commandは実メモリを既知fixtureと比較し、成功tagだけを返す。tagはhashではなく検査完了識別子。機密値とアドレスは攻撃試験用fixtureであり製品用機密として利用しない。

`TEE-ISO-TA PASS`：Bがprivate値を保持している間に、AがBの実user VAへload/storeを試みる。自TAにも同じVA mappingがある場合は試験として採用せず、その事実をPROBE_VA_ALIASへ記録する。Aを閉じて再作成し最大8回で異なるASLR配置を探す。見つからなければBLOCKED。mapping衝突を拒否成功に数えない。

実際のアクセスでAがTARGET_DEADとなり、Bでprivate値維持を再検査し、Aを再openして自己状態を検査する。NormalログのPROBE_FAULTとSecureログのFAR/ESRを照合し、アドレス、読書き方向、EL0 Data Abort、translation fault、FAR有効性を要求する（`qemu/security-evidence.json`）。無関係のpanic/例外でPASSにしない。ホスト側検査にはFAR違い・方向違い・例外class違い・欠落を拒否する単体試験を追加した。

`TEE-ISO-CORE PASS`：Core内にvolatile private canaryを置く自作PTAを追加した。PTAは実canaryのuser addressではないCore VAを攻撃試験へ提供し、実メモリの整合性を検査する。Aからread/writeを各実行し、各拒否後に特権PTAが同じcanaryの値を再確認する。PTAはKEEP_ALIVEで、比較対象をsession再openで初期化せず、値を書き換えるcommandも提供しない。Core ASLR後のpointerを使用し、固定の物理addressやELF entryを現在のCore VAと混同しない。

TAとCore双方の試験が成功したため、今回のfixture範囲では総合TEE-03をPASSとした。TA/Coreの全領域やあらゆる攻撃経路を網羅する証明ではない。初期のCore entryへのprobeだけの結果は非改変確認不足でBLOCKEDだったが、最終結果はcanaryを使った独立の再検査に基づく。

PTAは `optee_standalone/pta/validation_canary.c`。固定第三者原本を変更せず、`stage_optee_overlay.py` が `build/source/optee-validation` にcopyを作り、2自作fileとsub.mkの1行を追加する。`results/phase5/optee-validation-overlay.json` に原本commit、追加file、変更前後SHA-256を保存する。派生copyのbaseline manifestは `.base_source_manifest.json` とし、未変更sourceと誤表示しない。毎回copyを作り直して未記録の派生sourceを持ち越さない。この機能は `--validation-pta` で明示した検証build専用。製品Coreへ組み込まない。

## 鍵保護

`TEE-KEY-API PASS`：TA instance内でAES鍵を生成し、TEE_USAGE_EXTRACTABLEを除去する。実object usageを確認し、暗号操作ができることを検査する。KEY_EXPORT要求はACCESS_DENIED、返却valueはゼロ。拒否後に同じinstanceの鍵が利用可能なことを確認する。別の既知鍵でAES-128零鍵・零平文の既知暗号文を検査し、ランダム鍵の往復試験とは分けている。Normalに鍵bytesを返すAPIは設けていない。

`TEE-KEY-MEM BLOCKED`：生メモリからの鍵抽出攻撃を実施していない。object usage確認とexport APIの拒否をメモリ抽出耐性とはしない。libuteeのattribute exportを直接呼ぶ試験も未実施。総合TEE-05はBLOCKED。QEMU構成のsoftware PRNGは実機の安全なentropy源を証明しない。

## 共有メモリ

`TEE-SHM-01 PASS`：静的予約SHMのTMEM_INPUTを実装した。safe APIはCoreから通知された範囲、加算overflow、message領域との重複、零sizeを検査する。TAは固定16-byte入力をprivate stackへ一度コピーし、patternを検査する。

攻撃用の別APIはNS側で指定pointerを参照せず、そのままCoreへ送る。正常bufferの受理、15-byteの不一致をTAで拒否、SHM終端跨ぎ・Secure領域・64-bit wrapをCoreで拒否、不正要求後の正常処理、元bufferの非改変を実行した。各応答のtransport/result/originもNormal UARTへ残す。通常APIだけで不正要求を止めてCoreの検査を実施したことにはしない。

初回はTMEM_INPUTをRMEMの値と取り違え、正常bufferもCoreに拒否された。固定版 `optee_msg.h` の値9へ修正し、upstream定義との照合試験を追加した。`optee-attempt-01/02.log`、`qemu-attempt-01/02-console.log`に失敗を残した。結果をPASSへ固定して回避していない。

この試験は同期inputのみ。memref output/inout、TOCTOU競合、動的SHM登録、malformed message全域のfuzzingは未実装。

## 通常TA署名

Early TAはCoreへ組み込んだ `.stripped.elf` を使用する。生成された通常署名付き `.ta` は別の成果物として、固定版OP-TEE公式 `sign_encrypt.py verify` をホストで実行した。正常、署名bit改ざん、payload bit改ざん、別public key、別UUIDを独立に検査し、command/終了値/入力SHA-256を保存した。development public keyのみを証跡へ保存し、秘密鍵は新たに保存していない。

通常TAのruntimeロードは `CFG_REE_FS_TA=n`、RPC loader未実装のためBLOCKED。署名済みfile生成、ホストsignature verify、Early TA起動、実行時の署名検証、Secure Bootを区別する。TEE-04/08にPASSを付けない。

## 実機移植と次の作業

ZCU106のXMPU/XPPU/ZDMA/GIC backendと安全ゲートは変更しない。今回追加したTEE fixtureを実機で実行していない。master ID、NS属性、port/route、XPPU profiles/parity、FSBL/PMUFW/BSP使用範囲、BL31 OCM予約、OP-TEE実機port、vector/FIQ routingが未確定。取得方法は `docs/zcu106_phase4_handoff.md`。値を推測でconfirmedへ変えず、ゲート成功前はロード禁止。

次の実装は、TA/Core攻撃の対象領域・権限・証跡の拡充、Normal要求者認証、鍵memory攻撃、通常TAロードのRPC、storage backendと改ざん/再起動試験。実機用の安全なentropyとboot chainも必要。不可逆設定を自動化しない。

## 再現と根拠

```sh
bash scripts/run_phase5.sh
```

固定第三者原本は改変せず、明示的な検証PTA overlayをbuild copyへ適用する。source verification→12 ELF/TA/Core/TF-A build→ホスト署名検査→3構成QEMU→証跡照合→結果集約を行う。`results/status.json`と`results/report.md`が個別結果、`results/phase5/artifacts.json`が今回のhash。過去Phase 4 hashとの一致は開始時の監査結果であり、変更後の同一bytesを保証するという意味ではない。

公式根拠はOP-TEE4.5.0固定commit `0919de0f7c79ad35ad3c8ace5f823ad1344b4716` の以下のsource/functionを確認した（章節番号は推測しない）：

- `core/tee/tee_svc.c:syscall_open_ta_session`：Coreが呼出元TA identityを設定。
- `core/tee/entry_std.c:get_open_session_meta`：Normal側で許可されるlogin class、`set_tmem_param`：shared buffer範囲検査。
- `core/include/optee_msg.h`：TMEMとRMEMのmessage ABI。
- `lib/libutee/tee_api_property.c:TEE_GetPropertyAsIdentity`、`tee_api_objects.c`：identity/object usage API。
- `scripts/sign_encrypt.py:command_verify`：署名/digest/UUID検証。
- [OP-TEE 4.5.0 Trusted Applications: TA locations](https://optee.readthedocs.io/en/4.5.0/architecture/trusted_applications.html#ta-locations)：Early TAと通常TAのロード経路の区別。

TrustZoneの定義とZCU106 register根拠は既存 `docs/references.md`、`docs/zcu106_backends.md`を維持する。GlobalPlatform TEE System Architecture v1.3は根拠に使用していない。
