# 05：信頼境界・メモリ分離・XMPU・暗号と鍵

対象：ZCU106 B2 / TF-A + OP-TEE + Standalone BL33。作成日：2026-11-08。  
確認commit：`70bc1c288c962c3368ca53295a146db690213d8b`。基本原理と対象実装を区別し、実機結果は未再確認。  
[全体案内へ](00_README.md)

## 1. セキュリティ試験を読む基本

「何かが拒否された」だけでは目的の保護を確かめたことにならない。誰を攻撃者とし、何を守り、どの経路で操作し、どこで拒否されたかを特定する。

| 観点 | 今回の例 |
|---|---|
| 攻撃者/要求者 | Normal client、他TA、許可されないsession |
| 保護対象 | TAのprivate状態、Core canary、Secure物理page、鍵属性、TAロードの真正性 |
| 能力 | 不正parameter、偽装login、他領域へのread/write、TAデータ差替え |
| 信頼境界 | Normal→Core、user TA→Core、TA-A→TA-B、物理master→DDR |
| 期待 | 拒否、秘密非開示、非改変、他処理継続、後始末 |
| 観測 | result/origin、fault PC/FAR/ESR、tag、Core監査、資源数等 |

この試験セットは、DMA/PL/全master、サイドチャネル、全boot chain等を一度に保証するものではない。対象外や未観測の経路を明示して説明する。

## 2. 認証・認可・分離・完全性

| 用語 | 問い | 今回との対応 |
|---|---|---|
| 認証 | 誰からの要求か | Core管理identityと自己申告の区別 |
| 認可 | その操作を許すか | TA-BのTA-A allowlist、sessionのauthenticated |
| 分離 | 他者の状態へ直接干渉できないか | TA間/Core/Normal-Secureのメモリ試験 |
| 機密性 | 秘密が読めないか | private value、Secure pattern、鍵属性 |
| 完全性 | 勝手に改変できないか | victim tag、Secure page検証、署名TA改変検出 |
| 可用性/回復 | 異常後もサービスが続くか | panic後の再Open/ECHO、他TA継続 |

固定tokenのテストはsessionごとの認可状態を確かめるもので、製品のユーザー認証・秘密管理が完成した証拠ではない。

## 3. メカニズムとポリシーを分ける

「TA-Bの秘密はTA-Aからだけ読める」がポリシー。Coreが管理するidentityを取得し、loginとUUIDを比較するコードがメカニズム。

要求parameterへTA-AのIDを入れても、それを信じないことが重要。security.cはgpd.client.identityを使う。PUBLIC側からの自己申告でtrusted accessが成立しないこと、正規TA-A経路が動くことを両方見る。

常に拒否する壊れたサービスも、不正要求の拒否だけなら通ってしまう。許可経路の対照が必要。

## 4. TA間分離の実装

被害側TA-Bは自分の秘密VAを試験用に返し、攻撃側TA-Aへその数値を渡してread/writeさせる。ただしA側に同じVAが自分のpageとしてmapされていると、Bの秘密を攻撃する試験にならない。

今回のTAは_utee_check_access_rightsで対象VAのmapping条件を確認し、aliasならACCESS_CONFLICT等として扱う。Normal側は再Open等で条件解消を試し、成立しなければBLOCKED。

条件が整った後、実load/storeで攻撃TAがfaultし、CoreがTARGET_DEAD/origin TEEを返すことを確認する。その後Bの秘密tagとA再生成後の正常状態、Closeを確認する。

| 観測 | 何の証拠か |
|---|---|
| alias除外 | 攻撃対象VAが攻撃側の正常pageではない |
| 実load/store | APIの事前拒否だけでなくCPUアクセスを試した |
| TARGET_DEAD/origin | 期待したTA側のfault経路 |
| B tag不変 | victimの整合性 |
| A再Open/ECHO/tag | 攻撃側の復旧 |
| Core canary等 | Core側影響の補足 |

全メモリ領域・全命令・全配置で絶対に分離される証明ではなく、指定条件のprobeの証拠。秘密VA公開コマンドは試験専用で製品へそのまま持ち込まない。

## 5. Core分離とPTAの注意

Core canaryをPTAで参照し、user TAからその領域へ直接アクセスさせる試験は、user TAとCoreの権限分離を確認する。PTA自体はCore権限なので、user TAと同じ分離境界の中に置かれた別アプリではない。

PTAへ任意アドレス読出し機能を追加すると、検証用サービスが迂回路になる。今回の監査PTAは固定記録のみ、物理PTAはsession所有の専用page等へ対象を限定する。

## 6. NormalのMMU分離と物理保護

Normal OS/EL1が攻撃者の想定なら、Normal自身のページテーブルでSecureをmapしないことだけでは足りない。攻撃者は自分の表を書き換える能力を持ち得るため、さらに物理経路の保護を確かめる必要がある。

一方で通常動作のPTEを必要最小限にするのは、バグ・不要なアクセス・実行権の抑制として意味がある。Normal PTEによる制限とXMPUの役割を混ぜない。

## 7. XMPUの基本

XMPUは、アドレス領域・master ID・security属性・read/write権等に基づいてメモリ要求を検査するSoC側の保護機構。

| 情報 | 必要な理由 |
|---|---|
| 対象DDR範囲 | どの物理pageを守るか |
| 経由するXMPU bank/port | 別経路なら設定が効かない可能性 |
| master ID/mask | 誰の要求かを適切に一致させる |
| secure/non-secure属性 | Security Stateに対応するtransaction制約 |
| read/write許可 | 機密性と完全性の両方 |
| regionの有効/重複/優先 | 別設定の干渉を避ける |
| lock/設定変更権 | 設定を誰が変更できるか |
| error/poison動作 | 拒否時に何が観測されるか |

`master`は要求の発行主体。APU CPU、DMA、PL側等が候補。CPU一経路で拒否できたことを、すべてのmasterの合格へ広げない。

今回のprepare_xmpu_test.pyとxmpu_test.tclは、限定専用領域の計画・設定/読戻しを扱う。全SoC保護の自動認定ツールではない。未知lock offsetやmaster設定を推測しない設計になっている。

## 8. poisonとabort

保護違反の観測は、必ず同じCPU同期例外とは限らない。XMPUの設定や経路によりpoison/error応答・割込み等が関係する。poisonという言葉を「秘密値は絶対に0が返る」と単純化しない。

確認commitの物理試験は、期待PAへのStage1変換を確認したうえで、限定した同期外部abortをPASS根拠に使う。別の拒否方式で秘密が漏れなくても、現在の判定条件ではBLOCKEDになる場合がある。

**BLOCKEDはハードウェアが壊れたという断定ではなく、その判定方式で保護の証拠を確定できない状態でもある。** 逆に期待Secure patternがNormalへ返れば機密性のFAIL根拠になる。

## 9. 物理試験の準備・操作・判定・後始末

1. 実機review gate、EL1、既知のEL2状態、Stage2等を確認する。
2. PTAで専用pageを準備し、PAとsizeを取得する。
3. PAが予約内・整列済みで、対象pageだけを使うことを確認する。
4. Normal側へ限定aliasを作り、AT/PARでread/writeの変換先を確認する。
5. 先頭/末尾付近のread/writeを実行する。
6. fault PC/FAR/方向/SCTLR等を照合し、ESRを分類する。
7. Secure側でpage patternの非改変を確認する。
8. 正常Normal bufferとTA ECHOで継続を確認する。
9. alias削除、MAIR復元、page解放、session Closeを行う。

物理pageのpatternは`PHYSICAL_PATTERN ^ index`。単に全word同じ定数にせずoffsetも対応させる。正しいPAを向いていることと、期待patternを観測/非観測したことを結び付ける。

### 分類の粒度

physical_policy.cはTranslation/Permission/限定External/その他へ分類し、FAR無効やtable walk等も条件で除外する。Normal側ハンドラで回復できた例外すべてがphysical PASSではない。後段判定がclassification等を使う。

## 10. 暗号の基本：暗号化・hash・署名

| 技術 | 主な目的 | 鍵 | 今回 |
|---|---|---|---|
| AES等の共通鍵暗号 | 鍵を使うデータ変換。用途はmode依存 | 秘密の共通鍵 | 暗号API動作/鍵属性検査 |
| SHA-256等のhash | 内容から固定長digestを得る | 通常は秘密鍵なし | bundle/ELFの対応・改変確認 |
| MAC | 共通秘密に基づく認証タグ | 秘密の共通鍵 | 比較用の基本知識。今回TA署名の方式とは別 |
| デジタル署名 | 秘密鍵による認証と公開鍵での検証 | 秘密鍵/公開鍵 | 外署名TAの検証 |

hash値をデータと一緒に攻撃者が変更できれば、そのhashだけでは真正性を保証できない。署名の検証公開鍵そのものが信頼される必要がある。

署名は通常暗号化ではなく、署名TAの内容がNormalから見えない保証ではない。今回の試験は受理/拒否の真正性・完全性が中心。

## 11. TA署名で何を信頼しているか

PCの試験秘密鍵で署名TAを作り、Coreへ対応公開鍵を設定する。正常、改変、別鍵の対照を実行する。

署名ヘッダの検証成功と、実際に取り込んだpayloadのdigest整合を別段階として記録する。Normal側の結果だけでなくCore監査でどちらが拒否されたか確認する。

Coreの公開鍵を自由に差し替えられる状況の対策は、Core/boot chainの真正性等へつながる。ただし今回のTA署名PASSがそのboot chain全体を証明するものではない。

rollbackも別問題。古い正規署名TAは署名自体が正しくても版ポリシーに反する可能性がある。今回CFG_RPMB_FS等の構成を越えたrollback防止を自動的に保証しない。

## 12. AES試験：往復と既知解

run_crypto_testは生成鍵でencrypt/decryptして戻ることと、別の既知鍵・既知入力に対する暗号文一致を確認する。

| 試験 | 何が分かるか | 単独では分からないこと |
|---|---|---|
| 生成鍵で往復 | APIの一連の操作が成立 | encrypt/decrypt双方が同じ誤りを持つ可能性の排除 |
| 既知解（KAT） | 指定入力が期待暗号文へなる | 全入力・全mode・全実装特性 |
| handle解放 | 資源の寿命 | 物理的残留ゼロ化の完全証明 |

今回AES_ECB_NOPADは1blockのAPI/KAT試験に使う。製品データ保護方式としてECBを推奨するものではない。

鍵が生成できたことは乱数源の十分な品質の証明ではない。今回ビルドにはsoftware PRNG設定もあり、製品のhardware entropy/RoT認証と混同しない。

## 13. 鍵の用途制限と抽出不可

security_createはprivate_keyを作り、usageをencrypt/decryptへ制限する。KEY_EXPORT commandが拒否することだけではCoreの属性取得制御を証明しないため、key_attribute_testはCore側属性取得syscallを用いた検査も行う。

抽出可能な対照鍵では属性取得が成功し、private_keyでは期待エラーとsentinel buffer非変更を確認し、それでも許可された暗号操作ができることを確認する。

対象commitで期待する低位syscallの拒否コードはBAD_PARAMETERS。一般に「抽出拒否なら常にACCESS_DENIED」と決め打ちしない。どのAPI層を試すかで結果が異なる。

鍵handleは秘密鍵バイト列そのものではなく、オブジェクトへの参照。handleを持つこと、鍵を抽出できること、暗号操作できることは別の権限。

## 14. 想定問答と理解確認

| 質問 | 回答の核心 |
|---|---|
| TA間faultでNormal/Secure物理保護もPASS？ | 仮想空間分離と物理経路保護は別 |
| XMPU設定読戻しだけで保護証明？ | 対象master・経路・実access・被害側を照合 |
| External Abortは必ずXMPU？ | 他原因もあり、設定/経路等の根拠が必要 |
| poisonで戻ったら即FAIL？ | 漏洩や改変ならFAIL。機構不明/現判定対象外はBLOCKED等を区別 |
| hash一致で正規TA？ | 信頼された署名等と別に確認 |
| 署名成功で暗号化済み？ | 真正性/完全性と機密性は別 |
| 暗号APIが動けば乱数も安全？ | entropy品質は別検証 |
| KEY_EXPORT拒否だけで抽出不可？ | Coreの属性取得等の迂回経路も検証する |

練習：NormalのPTEが未写像でfaultし、XMPU違反記録が無い。→Normal仮想制限の証拠にはなり得るが、対象PAへの実要求によるXMPU拒否は確認できていない。

完成条件：TA分離、物理probe、署名TA、鍵属性の各試験を、対象・攻撃能力・期待拒否・正規対照・継続・後始末・限界で説明できる。

## コード根拠と一次資料

### 今回のソース

- [tests/validation/security_tests.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/validation/security_tests.c)
- [secure_world/ta/security.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/secure_world/ta/security.c)
- [secure_world/ta/ta.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/secure_world/ta/ta.c)
- [secure_world/pta/validation_canary.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/secure_world/pta/validation_canary.c)
- [secure_world/pta/physical_probe.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/secure_world/pta/physical_probe.c)
- [tests/validation/physical_memory.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/validation/physical_memory.c)
- [tests/validation/physical_policy.c](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/tests/validation/physical_policy.c)
- [scripts/prepare_xmpu_test.py](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/scripts/prepare_xmpu_test.py)
- [scripts/jtag/xmpu_test.tcl](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/scripts/jtag/xmpu_test.tcl)
- [scripts/stage_signed_loader.py](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/scripts/stage_signed_loader.py)
- [scripts/decode_signed_results.py](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/scripts/decode_signed_results.py)
- [build_config/components.mk](https://github.com/ririmon22/trustzone-validation/blob/70bc1c288c962c3368ca53295a146db690213d8b/build_config/components.mk)

### 基本原理を確認する資料

- [AMD UG1085：XMPU](https://docs.amd.com/r/en-US/ug1085-zynq-ultrascale-trm/AMD-Memory-Protection-Unit?contentId=VkimP8hkmUti9rUJOFbXnQ)
- [AMD UG1085：設定とlock](https://docs.amd.com/r/en-US/ug1085-zynq-ultrascale-trm/Configuration?contentId=18E4XtiFaQHQ6DArej75uQ)
- [OP-TEE 4.5.0：署名TA等の構成](https://optee.readthedocs.io/en/4.5.0/architecture/trusted_applications.html)

外部資料は版や対象CPUで記述が異なる場合がある。AAPCS64の新拡張、一般OS構成、別SoCの設定をCortex-A53の今回構成へ無条件に適用しない。ここに記載した実装上の数値・判定は固定commitのコードを根拠とする。

次：[第06冊](06_Test_Evidence_Debug_Performance.md)
