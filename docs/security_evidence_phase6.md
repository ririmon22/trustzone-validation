# 特性・検証項目・実装・証跡の対応（Phase 6）

QEMUで実行した範囲の根拠を整理する。ZCU106全試験はNOT_RUN。以下のlogは `results/phase6/qemu`。詳細期待値は独立した `tests/expectations.json`。

|特性／ID|実際の操作と拒否機構|例外・非改変・範囲|source／証跡|
|---|---|---|---|
|実行状態 TZ-SYS-01|Secure/NSの別counterを反復処理|各状態の保持。禁止access試験ではない|baremetal/secure_app/main.c、nonsecure_app/main.c／baremetal.log|
|切替 TZ-SYS-02|SMCをcustom EL3が受けSecure/NS contextを保存復帰|反復要求の計算結果とregister sentinel。単一CPU/GPR/同期のみ|el3_monitor/monitor.c、tests/register_context.S／baremetal.log|
|保護 TZ-SYS-03|Secure RAMへのNS read/writeをQEMUのsecurity-aware memory decodeが拒否|許可memory access成功後に限定Data Abortから復帰しSecure側で各非改変・境界を確認。ZynqMP XMPUの証明ではない|common/el1_fault.c、probe.S、baremetal両app／baremetal.log|
|標準service B1-SMCCC-01|TF-A SMCCC/PSCI応答と未知FID|通常return、例外なし。Secure EL1分離試験の代替ではない|tfa_standalone/integration/main.c／tfa.log|
|service TEE-01|2session正常echo、不正commandはTAがBAD_PARAMETERS|session closeと継続。memory保護の代替ではない|client/client.c、ta/ta.c、tests/main.c／optee.log|
|TA分離 TEE-ISO-TA／TEE-03|AがBのprivate値の実VAへread/write。Core MMUが拒否|FAR一致、EL0 Data Abort、DFSC translation、WnR、FAR有効をSecure UARTで照合。Bの各非改変確認、A再open|ta/security.c、tests/security_tests.c、check_security_evidence.py／optee-secure.log、security-evidence.json|
|Core分離 TEE-ISO-CORE／TEE-03|AからCore canaryへのread/write。Coreのuser/kernel権限で拒否|各fault後にKEEP_ALIVE PTAが同じvolatile値を比較、TA/Bの継続。全Core領域の網羅ではない|pta/validation_canary.c、ta/security.c／同上、optee-validation-overlay.json|
|TA identity TEE-AUTH-TA|Core由来のTRUSTED_APP＋UUID allowlistでAを許可。PUBLICのclaimed IDと偽造loginを拒否|error returnのoriginも検査。Normal人間/アプリの認証ではない|ta/security.c、client.c identity probe／optee.log|
|鍵API TEE-KEY-API|Secure生成鍵のusage確認、export要求はTAが拒否|鍵を返さず、その後も鍵利用可能。AES KATは既知鍵fixtureと別に検査。memory抽出はBLOCKED|ta/security.c、ta/ta.c／optee.log|
|共有memory TEE-SHM-01|正常TMEM input。size不一致はTA、範囲外/Secure/wrapはCoreが拒否|transport/result/origin、新規正常要求、元bufferの非改変。input同期のみ|client.c、security.c、security_tests.c／SECURITY_REPLY行|
|障害 TEE-07|Aの制御panicをCoreが検出しTARGET_DEAD|BのechoとAの再open成功。Phase 6ではdead sessionのCore closeも確認|tests/main.c、client.c／optee.log、optee-secure.log|
|署名TA TEE-04|ホスト公式toolで正常と改ざん/別鍵/別UUID|HOSTのみ。runtime RPC loaderなしのためTEE-04はBLOCKED|check_signed_ta.py／results/phase6/signed-ta/results.json|

## 検証用構成と通常構成

Phase 6のB2は明示的な `--validation-pta` build。原本の固定OP-TEEは検証したまま保持し、派生copyへ自作canary PTAを加える。変更前後hashと追加fileをmanifestへ残す。Core canaryのPASSを、PTAなしの通常構成で同じ再検査を実施した結果としない。attack command・address公開・既知fixtureは製品用ではない。

TA VA alias除外では、Phase 6から固定SDKの `_utee_check_access_rights` を使う。高水準 `TEE_CheckMemoryAccessRights` はmallocの割当状態も確認するため、失敗だけではmappingがないと断定できない。Coreの実mappingでaliasを除外し、実load/storeのFAR/ESRを別に照合する。内部ABIに依存する検証拡張であり別OP-TEE版での互換性は未確認。

Normal要求者認証（TEE-02全体）、鍵memory抽出（TEE-05全体）、通常署名TA runtimeロード（TEE-04）、永続storage、Secure Bootは依然BLOCKED。署名ファイル生成やhost verify、鍵export拒否、TrustZone state切替を互いの代用にしない。
