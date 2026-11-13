# TF-A／OP-TEE ソフトウェア実装例・API利用レシピ

対象と参照版は[00_README](00_README.md)。本書はソフトウェア上の利用法を示す。ビルド手順は含まない。

コードは三つに区別する。

- **既存interfaceの利用例**：今回のwrapperや公開APIを呼ぶ例。初期化済み等の前提を明記する。
- **独立した説明例**：新しいTA等を作る際のサンプル。既存TAへ同名entry pointを重複追加しない。
- **設計用の抜粋／擬似コード**：資源やprotocolの設計を示すもの。完成実装とみなさない。

サンプルを今回の実機で実行したものではない。任意の製品用途、並行実行、攻撃耐性の保証は行わない。

## 目次

- [1. 最初にどのAPIを選ぶか](#section-01)
- [2. 今回のBL33でECHOを使う](#section-02)
- [3. Linux clientで同じ考え方を使う](#section-03)
- [4. 新しいTA commandを追加する](#section-04)
- [5. 共有inputを安全に受け取る](#section-05)
- [6. TA-BをTA-Aからだけ使わせる](#section-06)
- [7. 固定tokenではないNormal requester認証を追加する](#section-07)
- [8. 鍵をTA instanceで保持して使う](#section-08)
- [9. 暗号方式を選ぶときの実装契約](#section-09)
- [10. PTAを追加する場合の最小構造](#section-10)
- [11. TF-Aへ独自runtime serviceを追加する場合](#section-11)
- [12. TA内部traceを取り出す設計](#section-12)
- [13. 実装変更時の確認表](#section-13)
- [14. 今回の物理試験を理解する補足](#section-14)
- [15. 結果からコードへ戻る読み方](#section-15)
- [16. よくある疑問](#section-16)
- [17. 参照元](#section-17)
- [コード索引](#section-18)

<a id="section-01"></a>

## 1. 最初にどのAPIを選ぶか

|自分が書くコード|呼ぶinterface|使わないもの|
|---|---|---|
|Standalone BL33|今回の`tee_session_open/invoke/close`|Linux用TEECをそのまま持ち込まない|
|Linuxのclient app|`TEEC_*`|User TAのentry callbackを直接呼ばない|
|User TAのservice|`TEE_*`と自身のcommand handler|CoreのdriverやTF-Aのcm関数を直接呼ばない|
|TA-AからTA-B|`TEE_OpenTASession/InvokeTACommand/CloseTASession`|通常、Normal経由で同じ要求を送り直さない|
|Core権限の機能|Core driver／必要ならPTA|何でもUser TAへMMIO公開しない|
|EL3専用制御|TF-A runtime service|通常の認可や暗号serviceを理由なくEL3へ移さない|

<a id="section-02"></a>

## 2. 今回のBL33でECHOを使う

### 2.1 前提

platform初期化、EL1/MMU/cacheと共有memoryの前提、`tee_client_initialize()`が成功済みで、他の呼出しと競合しないこと。下記はその後の**利用関数例**であり、単独mainとして起動できるものではない。

```c
#include "client.h"

static bool reply_ok(struct tee_reply r)
{
    return r.transport == TEE_CLIENT_OK && r.result == TEE_SUCCESS;
}

/* 今回のclient APIを利用する説明例。 */
static bool check_echo_service(void)
{
    struct tee_session session = {0};
    struct tee_reply opened = tee_session_open(&session, validation_ta_uuid);
    if (!reply_ok(opened))
        return false;

    struct tee_reply echoed =
        tee_invoke(&session, TA_COMMAND_ECHO, 0x45a7U);
    bool valid = reply_ok(echoed) && echoed.value == 0x45a7U;

    /* sessionの所有者が後始末をする。TARGET_DEAD時には
       今回のclientがすでに閉じている場合がある。 */
    if (session.open) {
        struct tee_reply closed = tee_session_close(&session);
        valid = reply_ok(closed) && valid;
    }
    return valid;
}
```

### 2.2 この例が見ているもの

- Open：通信とCore/TAの接続処理が成功したか。
- ECHO：通信・処理結果とvalueを組み合わせて判定。
- Close：利用が終わったsessionを解放できたか。
- 返り値false：どこが失敗したかは別途transport/result/originを記録する。

`result==0`だけを見ない。transport失敗時に初期値0が入っている場合があり、処理未完了を成功と誤認する。

<a id="section-03"></a>

## 3. Linux clientで同じ考え方を使う

以下は**Linux/libteec環境向けの独立した説明例**。今回のBL33上では動かさない。対象は今回のTA-AのUUIDとECHO command 0を想定し、TAのVALUE_INOUT契約に合わせている。

```c
#include <stdint.h>
#include <stdio.h>
#include <tee_client_api.h>

int main(void)
{
    const TEEC_UUID uuid = {
        0x6dc13418, 0x534f, 0x4642,
        {0x91, 0x38, 0x21, 0x58, 0x76, 0x30, 0x49, 0x10}
    };
    TEEC_Context context;
    TEEC_Session session;
    TEEC_Operation operation = {0};
    uint32_t origin = 0;
    TEEC_Result result;
    int rc = 1;

    result = TEEC_InitializeContext(NULL, &context);
    if (result != TEEC_SUCCESS) {
        fprintf(stderr, "context result=0x%08x\n", result);
        return rc;
    }
    result = TEEC_OpenSession(&context, &session, &uuid,
                              TEEC_LOGIN_PUBLIC, NULL, NULL, &origin);
    if (result != TEEC_SUCCESS) {
        fprintf(stderr, "open result=0x%08x origin=%u\n", result, origin);
        goto release_context;
    }

    operation.paramTypes = TEEC_PARAM_TYPES(
        TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    operation.params[0].value.a = 0x45a7U;
    result = TEEC_InvokeCommand(&session, 0U, &operation, &origin);
    if (result == TEEC_SUCCESS && operation.params[0].value.a == 0x45a7U)
        rc = 0;
    else
        fprintf(stderr, "invoke result=0x%08x origin=%u value=0x%08x\n",
                result, origin, operation.params[0].value.a);

    TEEC_CloseSession(&session);
release_context:
    TEEC_FinalizeContext(&context);
    return rc;
}
```

### 3.1 なぜPUBLICを認証と呼ばないか

この例は誰を許可するかを認証していない。PUBLICのOpenをTAが許可する構成を想定する。正規アプリ限定にする要件があれば、TAが信頼できる材料をもとに認証・認可を行う追加設計が必要。

### 3.2 memref利用時の手順

1. input/outputの方向と最大サイズをTAと共有する。
2. temporary bufferか、再利用するshared memory objectかを選ぶ。
3. operationの型とbuffer/size/parent/offsetを対応させる。
4. Invoke中にbufferの寿命を終えない。
5. 戻り値、出力長、内容を評価する。
6. shared memory objectを最後にreleaseする。

<a id="section-04"></a>

## 4. 新しいTA commandを追加する

### 4.1 先に契約を書く

説明用command `ADD_CHECKED`を例にする。既存TAのcommand番号を上書きしない。番号の割当は別途決める。

|項目|契約例|
|---|---|
|入力|param0 VALUE_INPUTのa,b|
|出力|param1 VALUE_OUTPUTのa|
|正常|a+bがuint32へ収まるとSUCCESS|
|異常|型違い／overflowはBAD_PARAMETERS|
|状態|この説明例ではsession状態を使用しない|
|秘密|秘密dataを読み出さない|

### 4.2 TA側handlerの例

```c
#include <stdint.h>
#include <tee_internal_api.h>

static TEE_Result add_checked(uint32_t types, TEE_Param params[4])
{
    const uint32_t expected = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INPUT,
        TEE_PARAM_TYPE_VALUE_OUTPUT,
        TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE);
    if (types != expected)
        return TEE_ERROR_BAD_PARAMETERS;

    uint32_t a = params[0].value.a;
    uint32_t b = params[0].value.b;
    if (b > UINT32_MAX - a)
        return TEE_ERROR_BAD_PARAMETERS;

    params[1].value.a = a + b;
    params[1].value.b = 0;
    return TEE_SUCCESS;
}
```

この関数を自分の`TA_InvokeCommandEntryPoint()`のcommand分岐から呼ぶ。Coreへcommand番号の意味を実装する必要は通常ない。

**今回の`tee_invoke()`wrapperは1個のVALUE_INOUT用。上の2param契約をそのまま送れない。** BL33 wrapperとmessageの扱いを拡張するか、LinuxのTEEC_Operation等を使う環境で呼ぶ必要がある。

### 4.3 API変更として残すもの

command ID、version、param型、長さの単位、endianness、error/result/origin、状態遷移、副作用、取消・再試行、最大資源、互換性を定義する。Cの構造体をそのまま外部protocolにする場合はpaddingや32/64bit差を固定する。

<a id="section-05"></a>

## 5. 共有inputを安全に受け取る

以下は固定16Bを検査する**handler例**。認証ではなく形式検査であり、コピーしたsnapshotを検査・使用する考え方を示す。

```c
#include <stdint.h>
#include <tee_internal_api.h>

static TEE_Result check_fixed_input(uint32_t types, TEE_Param params[4])
{
    const uint32_t expected = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_MEMREF_INPUT, TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);
    uint8_t snapshot[16];
    if (types != expected)
        return TEE_ERROR_BAD_PARAMETERS;
    if (!params[0].memref.buffer || params[0].memref.size != sizeof(snapshot))
        return TEE_ERROR_BAD_PARAMETERS;

    TEE_MemMove(snapshot, params[0].memref.buffer, sizeof(snapshot));
    for (size_t i = 0; i < sizeof(snapshot); ++i) {
        if (snapshot[i] != (uint8_t)(0x50U + i)) {
            TEE_MemFill(snapshot, 0, sizeof(snapshot));
            return TEE_ERROR_SECURITY;
        }
    }
    /* 実際の処理も共有bufferを読み直さずsnapshotを使用する。 */
    TEE_MemFill(snapshot, 0, sizeof(snapshot));
    return TEE_SUCCESS;
}
```

可変長ならstackへ無制限に確保しない。上限、allocation失敗、header長とpayload長の関係、output容量を検査する。

<a id="section-06"></a>

## 6. TA-BをTA-Aからだけ使わせる

### 6.1 B側の認可判定例

```c
#include <tee_internal_api.h>

static TEE_Result require_ta_a(void)
{
    const TEE_UUID allowed = {
        0x6dc13418, 0x534f, 0x4642,
        {0x91, 0x38, 0x21, 0x58, 0x76, 0x30, 0x49, 0x10}
    };
    TEE_Identity identity;
    TEE_Result result = TEE_GetPropertyAsIdentity(
        TEE_PROPSET_CURRENT_CLIENT, "gpd.client.identity", &identity);
    if (result != TEE_SUCCESS)
        return result;
    if (identity.login != TEE_LOGIN_TRUSTED_APP ||
        TEE_MemCompare(&identity.uuid, &allowed, sizeof(allowed)) != 0)
        return TEE_ERROR_ACCESS_DENIED;
    return TEE_SUCCESS;
}
```

Open全体をAへ限定するならOpen callbackで使う。PUBLICへECHO等を公開し一部commandだけ制限するなら、そのcommand handlerで使う。今回のBはTRUSTED_READで制限する。Openへ一律制限すると既存試験のPUBLIC正常対照も変わる。

Aが正しいUUIDであることと、そのAのコードが信頼できることはセットで考える。Aを包含するCore／TA署名鍵／起動経路の信頼性が前提になる。

### 6.2 A側の呼出し例

説明用の関数。BのUUID、command、期待値、引数契約が一致する前提。

```c
static TEE_Result invoke_peer(const TEE_UUID *peer, uint32_t command,
                              uint32_t *output)
{
    TEE_TASessionHandle session = TEE_HANDLE_NULL;
    TEE_Param params[4] = {0};
    uint32_t origin = 0;
    TEE_Result result = TEE_OpenTASession(
        peer, TEE_TIMEOUT_INFINITE, 0, params, &session, &origin);
    if (result != TEE_SUCCESS)
        return result;

    uint32_t types = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_INOUT, TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);
    result = TEE_InvokeTACommand(session, TEE_TIMEOUT_INFINITE,
                                 command, types, params, &origin);
    if (result == TEE_SUCCESS)
        *output = params[0].value.a;
    TEE_CloseTASession(session);
    return result;
}
```

上記の`output`は自分のTA内部の有効pointerである前提。外部から受けた任意pointerへそのまま転用しない。詳細の失敗層を必要とする設計ではoriginも呼出し側へ戻す。

<a id="section-07"></a>

## 7. 固定tokenではないNormal requester認証を追加する

これは設計例であり、今回未実装。

|決める項目|理由|
|---|---|
|誰を認証するか|利用者、端末、アプリ、外部serverは異なる|
|Normal侵害を想定するか|Normalに置くcredentialの信頼限界を決める|
|信頼起点|TAに何を信頼させるか。公開鍵等の置換防止も必要|
|要求のbinding|端末ID、operation、引数、権限等を署名/MAC対象へ含める|
|新しさ|nonce/counter、有効期限、再起動後の扱い|
|失効・更新|盗難credentialや旧鍵をどう無効化するか|
|状態|失敗回数、session権限、Close/re-Open時の初期化|

ビルド時に固定requestへ署名しNormalに配布するだけでは、誰でもその正規requestをコピーして使える場合がある。承認済み内容の検証と送信者の本人認証を区別する。

<a id="section-08"></a>

## 8. 鍵をTA instanceで保持して使う

### 8.1 非抽出AES鍵の準備例

`security.c`と同じ考え方の説明例。既存TAへ同名global/callbackを重複追加しない。

```c
#include <tee_internal_api.h>

static TEE_ObjectHandle service_key = TEE_HANDLE_NULL;

static TEE_Result create_service_key(void)
{
    TEE_Result result = TEE_AllocateTransientObject(TEE_TYPE_AES, 128,
                                                   &service_key);
    if (result == TEE_SUCCESS)
        result = TEE_GenerateKey(service_key, 128, NULL, 0);
    if (result == TEE_SUCCESS)
        result = TEE_RestrictObjectUsage1(service_key,
                    TEE_USAGE_ENCRYPT | TEE_USAGE_DECRYPT);
    if (result != TEE_SUCCESS && service_key != TEE_HANDLE_NULL) {
        TEE_FreeTransientObject(service_key);
        service_key = TEE_HANDLE_NULL;
    }
    return result;
}

static void destroy_service_key(void)
{
    if (service_key != TEE_HANDLE_NULL) {
        TEE_FreeTransientObject(service_key);
        service_key = TEE_HANDLE_NULL;
    }
}
```

Createから生成関数を呼び、Destroyから解放する。鍵更新commandを設けるなら、古い鍵を参照中のoperation、複数sessionの利用、失敗時の状態を設計する。秘密値を返すcommandは作らず、許可したoperationの結果のみ返す。

### 8.2 APIの役割を混同しない

|操作|意味|
|---|---|
|GenerateKey|新しいmaterialを生成|
|RestrictObjectUsage|API上の用途を制限|
|SetOperationKey|operationへ鍵を関連付ける|
|FreeTransientObject|objectの寿命を終了|
|Secure storageへ保存|永続性の別設計。上記だけでは実現しない|

<a id="section-09"></a>

## 9. 暗号方式を選ぶときの実装契約

今回のECB1block自己試験から、製品dataをそのままECBで保護する設計へ進まない。機密性だけか、改変検出も必要かを先に決める。

AEAD利用の設計用フロー：

1. algorithmに対応する鍵・nonce/tag長を決める。
2. operationをAE modeで確保し鍵を設定する。
3. nonce、AAD、payload長等を契約どおり設定する。
4. AADとpayloadを処理しtagを生成／検証する。
5. 復号認証失敗ならoutputを正常dataとして公開しない。
6. 鍵とnonceの再利用制約、counter永続性を管理する。
7. operation解放と秘密bufferの扱いを実装する。

本書は暗号protocolの完成仕様ではない。鍵の種類、nonce、tag、AAD、errorを「APIを呼べた」という事実だけで省略しない。

<a id="section-10"></a>

## 10. PTAを追加する場合の最小構造

以下は**Core内のPTA callback構造の説明用抜粋**。製品用の認可や資源制御を実装した例ではない。

```c
#include <kernel/pseudo_ta.h>
#include <tee_api_defines.h>

static TEE_Result example_invoke(void *context, uint32_t command,
                                 uint32_t types, TEE_Param params[4])
{
    (void)context;
    const uint32_t expected = TEE_PARAM_TYPES(
        TEE_PARAM_TYPE_VALUE_OUTPUT, TEE_PARAM_TYPE_NONE,
        TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE);
    if (types != expected || command != 0)
        return TEE_ERROR_BAD_PARAMETERS;
    params[0].value.a = 1;
    params[0].value.b = 0;
    return TEE_SUCCESS;
}
```

実際の登録は`pseudo_ta_register`のUUID、name、flags、invoke callbackへ結び付ける。Open/Closeが必要ならsessionごとの所有資源を持つ。UUIDは衝突しないよう設計する。任意のPA/VA読書きを公開しない。

PTAからUser TA用libuteeの関数を何でも呼ぶ設計にしない。Core driverや内部APIを使い、TAのようなSVC往復を前提にしない。user bufferへアクセスする場合はCoreのcopy/validation/PANの契約も必要。

<a id="section-11"></a>

## 11. TF-Aへ独自runtime serviceを追加する場合

まず「TA／PTAで足りない理由」を明確にする。通常の暗号や製品認可のためだけにEL3を拡張しない。

以下は**handlerの考え方を示す未登録の抜粋**。FIDは承認された割当を使う。`MY_SERVICE_FID`の定義やservice登録は意図的に含めない。

```c
static uintptr_t example_smc(uint32_t smc_fid,
                              u_register_t x1, u_register_t x2,
                              u_register_t x3, u_register_t x4,
                              void *cookie, void *handle,
                              u_register_t flags)
{
    (void)x2; (void)x3; (void)x4; (void)cookie;
    if (!is_caller_non_secure(flags))
        SMC_RET1(handle, SMC_UNK);
    if (smc_fid != MY_SERVICE_FID)
        SMC_RET1(handle, SMC_UNK);
    if (GET_SMC_CC(smc_fid) == SMC_32)
        x1 = (uint32_t)x1;
    /* 例：秘密やMMIOを扱わない、限定された数値応答。
       本物のserviceでは操作の認可・状態・入力も検査する。 */
    SMC_RET2(handle, 0, x1);
}
```

`DECLARE_RT_SVC`のowner範囲とcall typeへ登録する仕組みは01を参照。通常Cのreturnだけで応答レジスタを設定したつもりにしない。

EL3 serviceのpointer入力は、EL3が全PAを信用してよい根拠にならない。必要なら許可された共有領域とサイズを限定する。競合、再入、CPU間同期、例外、途中失敗の設計が増える。

<a id="section-12"></a>

## 12. TA内部traceを取り出す設計

これは今回の追加予定機能の設計案であり、現在の実装済み機能ではない。

### 12.1 正常系trace

|要素|設計すること|
|---|---|
|記録|command ID、event ID、result、session用識別子、sequence|
|保管|TA private RAMの固定長ring buffer等|
|取得|専用commandでMEMREF_OUTPUTへ必要な記録だけコピー|
|権限|誰にtraceを返すか。秘密やraw pointerを出さない|
|不足|SHORT_BUFFER、必要長、上限、overflow/droppedを明示|
|整合|version、record size、count、sequence、run IDとの対応|
|初期化|instance生成時、試験開始時、clear操作の権限|

GET_TRACEも入力検査・認可が必要なservice。trace取得前に対象TAを破棄すると、TA私有記録は失われる場合がある。出力先は共有memoryであり、取得後はNormal側から読める情報になる。

### 12.2 panic/fault時のtrace

意図的PANICやVM faultのあとに、同じTAへGET_TRACEを呼べば必ず取れる設計にはしない。

- 正常時に逐次必要分を外へ出す。
- Core側の例外記録に残す。
- 検証用PTAが限定的に保持する。
- debuggerで故障前後を停止観測する。

Core内snapshotの取得APIも、公開範囲、所有者、version、長さ、初期化を管理する。Core内部メモリ全体を読むAPIにはしない。

### 12.3 性能測定との関係

traceへ書く処理は測定時間を変える。正式性能runはtrace条件を固定し、必要ならtrace無効の測定と分ける。BPで止めたrunの時間を性能結果へ混ぜない。

<a id="section-13"></a>

## 13. 実装変更時の確認表

|変更|合わせて確認すること|
|---|---|
|新command|ID、型、サイズ、認可、error、Normal wrapper|
|新session state|初期化、別session分離、Close、再Open|
|新instance global|複数session共有、破棄、panic復旧|
|新鍵|生成元、用途、寿命、更新、永続性、出力制限|
|新RPC|alloc/free、保留thread、未知要求、Normalの信頼性|
|新PTA|Core権限の必要性、入力pointer、caller制限、資源所有|
|新SMC|FID割当、呼出元、context、ABI、将来互換|
|例外回復|期待PC/方向/領域との照合、原因除去、再発、非改変、継続|

<a id="section-14"></a>

## 14. 今回の物理試験を理解する補足

Secure側の`physical_probe.c:prepare_page()`が専用pageへpatternを書き、cache clean/invalidateと検証を行う。その後NormalへPAを返す。Normalは別VAを同じPAへ写像し、read/writeを試す。Secure側のVERIFYはページ全体を再確認する。

|段階|意味|
|---|---|
|PREPARE成功|試験対象を準備できた|
|Normal probeの例外|指定アクセスで例外を受けた|
|限定handler復帰|試験処理へ戻れた|
|VERIFY成功|その時点で全pageが期待patternと一致|
|後続ECHO/Normal control|正常系が継続できた|
|release/Close|試験資源を片付けた|

PSUから8Bの初期pattern維持を観測しても、それだけで全pageやすべてのmasterの拒否を証明しない。XMPUは最初の違反を保持するため、readとwriteの帰属を分ける設計が必要。

<a id="section-15"></a>

## 15. 結果からコードへ戻る読み方

1. reportの正式IDを決める。
2. Normal試験の`b2_check_value`のpredicateを開く。
3. 呼んだwrapperとmessage契約を調べる。
4. TAのcommand分岐へ進む。
5. 使用TEE APIと資源の寿命を見る。
6. 拒否層を説明する必要がある場合、Coreのentryやsyscallへ降りる。
7. World切替やEL3停止を説明する場合のみTF-Aへ降りる。
8. 機能の存在、ソース上の期待値、実機観測を分けて説明する。

<a id="section-16"></a>

## 16. よくある疑問

|疑問|答え|
|---|---|
|TAの署名が正しいなら誰でも接続できないか|署名と利用権限は別。TAの認可による|
|TAに置けばNormalへ秘密が出ないか|応答やログで出せば見える。出力制限が必要|
|TEE APIを使えば全エラーは返値で戻るか|void APIや仕様違反によるpanic条件もある|
|session Closeで必ずTAが消えるか|instance属性と残sessionによる|
|同じUUIDなら同じsessionか|別。UUIDは宛先、session IDは接続|
|Coreが拒否したらTAの型検査は不要か|不要ではない。TA固有の型・値・意味を検査する|
|TAのheapはBL33と同じか|別の実行環境・管理領域|
|書込み命令から戻れたら書込み成功か|外部エラーが非同期で届く場合もあり断定できない|
|署名TAの秘密鍵はCoreにあるか|今回の署名秘密鍵はPC。Coreは公開鍵で検証|
|ライブラリ関数を呼ぶたびSMCか|TA→Coreは主にSVC。TA内完結の処理もある|

<a id="section-17"></a>

## 17. 参照元

APIの型・定数は固定版のheader、dispatchの動作は固定版Core、今回のwrapperはプロジェクトのソースを根拠とする。各冊のコード索引から参照できる。

- [OP-TEE TA作成・param・identity](https://optee.readthedocs.io/en/4.5.0/building/trusted_applications.html)
- [OP-TEE 4.5 FAQ](https://optee.readthedocs.io/en/4.5.0/faq/faq.html)
- [TF-A runtime service guide](https://trustedfirmware-a.readthedocs.io/en/latest/getting_started/rt-svc-writers-guide.html)

コード例は本資料の説明用に作成したもので、公式exampleの全文転載ではない。上記の前提を省いて既存firmwareへ貼り付ける用途にはしない。

<a id="section-18"></a>

## コード索引

|参照先|確認する実装|
|---|---|
|[normal_world/client/client.h：struct tee_reply](https://github.com/ririmon22/trustzone-validation/blob/7e51908c64e35808139845c8a07c05f0ca511dab/normal_world/client/client.h#L20)|`struct tee_reply`の周辺を読む。本文の役割・API契約と照合する|
|[normal_world/client/client.c：struct tee_reply tee_session_open](https://github.com/ririmon22/trustzone-validation/blob/7e51908c64e35808139845c8a07c05f0ca511dab/normal_world/client/client.c#L151)|`struct tee_reply tee_session_open`の周辺を読む。本文の役割・API契約と照合する|
|[secure_world/ta/ta.c：TEE_Result TA_InvokeCommandEntryPoint](https://github.com/ririmon22/trustzone-validation/blob/7e51908c64e35808139845c8a07c05f0ca511dab/secure_world/ta/ta.c#L142)|`TEE_Result TA_InvokeCommandEntryPoint`の周辺を読む。本文の役割・API契約と照合する|
|[secure_world/ta/security.c：static TEE_Result trusted_read](https://github.com/ririmon22/trustzone-validation/blob/7e51908c64e35808139845c8a07c05f0ca511dab/secure_world/ta/security.c#L45)|`static TEE_Result trusted_read`の周辺を読む。本文の役割・API契約と照合する|
|[secure_world/ta/security.c：static TEE_Result key_attribute_test](https://github.com/ririmon22/trustzone-validation/blob/7e51908c64e35808139845c8a07c05f0ca511dab/secure_world/ta/security.c#L129)|`static TEE_Result key_attribute_test`の周辺を読む。本文の役割・API契約と照合する|
|[secure_world/pta/physical_probe.c：static TEE_Result prepare_page](https://github.com/ririmon22/trustzone-validation/blob/7e51908c64e35808139845c8a07c05f0ca511dab/secure_world/pta/physical_probe.c#L57)|`static TEE_Result prepare_page`の周辺を読む。本文の役割・API契約と照合する|
|[secure_world/pta/physical_probe.c：static TEE_Result verify_page](https://github.com/ririmon22/trustzone-validation/blob/7e51908c64e35808139845c8a07c05f0ca511dab/secure_world/pta/physical_probe.c#L43)|`static TEE_Result verify_page`の周辺を読む。本文の役割・API契約と照合する|
|[libteec/include/tee_client_api.h：TEEC_Result TEEC_InitializeContext](https://github.com/OP-TEE/optee_client/blob/4.5.0/libteec/include/tee_client_api.h#L455)|`TEEC_Result TEEC_InitializeContext`の周辺を読む。本文の役割・API契約と照合する|
|[lib/libutee/include/tee_internal_api.h：TEE_Result TEE_GetPropertyAsIdentity](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/lib/libutee/include/tee_internal_api.h#L49)|`TEE_Result TEE_GetPropertyAsIdentity`の周辺を読む。本文の役割・API契約と照合する|
|[include/common/runtime_svc.h：rt_svc_handle_t](https://github.com/Xilinx/arm-trusted-firmware/blob/dd3512287a17eea866d5c82fa7b86ad58de59163/include/common/runtime_svc.h#L53)|`rt_svc_handle_t`の周辺を読む。本文の役割・API契約と照合する|

## 作成時の確認範囲

固定版ソースとAPI宣言、本文のコードリンク・章リンク、Mermaidの構造を照合した。Linux clientの独立例はoptee_client 4.5.0の実headerを使ったC11構文検査を通過した。TA/PTA/TF-A例は利用箇所や前提を示す抜粋を含み、全体リンク・実機動作は未実施。図のMermaidレンダリングは未確認。手元のローカル修正・ロード済みimage・実機ログの一致確認は本資料の検証に含まれない。
