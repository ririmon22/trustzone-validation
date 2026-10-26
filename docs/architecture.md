# 構成と実装範囲

Aは3つの独立ELFからなる。EL3がSMC例外を受け、Secure EL1を起動した後にNormal EL1へ渡す。
Normalからの要求をSecureへ渡し、SecureからのSMC復帰でNormalを再開する。
整数レジスタ、SP_EL1、復帰PC/PSTATE、および使用するEL1システム状態を保存する。
単一CPU、協調的な同期呼出し、MMU/キャッシュ無効、割込みマスクの条件に限定する。
FPU/SIMD、MMUを有効にした状態、非同期割込み、SMPのコンテキスト保存は対応していない。
EL1コードが自分のVBARを設定し、予期したアドレスの読み書きだけData Abort復帰を許す。
それ以外の例外はFATALで停止する。

TZ-SYS-03は通常共有領域への書込／読出、Secure側センチネル確認、不正読出／書込、
保護バッファ先頭と末尾、各不正書込後のSecure再確認を行う。
QEMU secure RAM全体の外周や隣接アドレスの全境界はまだ網羅していない。
例外だけで合格しない。QEMUの固定Secure RAMはZynqのXMPU設定を検証しない。

B1のBL33はTF-A標準SMCCC/PSCI versionを呼ぶ。EL2 handoffならNS EL1へ下げる。
EL3初期化を再実行しない。標準APIのsmoke試験はSecure EL1との分離や全状態保存の証明ではない。
専用Secureサービスは未実装。TF-A QEMUの実ロード位置は別配置で整合した。

B2はLinuxドライバ/libteecを使用せずSMC32 transportとOP-TEEメッセージを扱う。
Early TA、予約共有メモリ、同期セッション、value引数を対象にする。
RPCを受けた場合はBLOCKEDとして扱うためのtransport状態を返し、未完了スレッドへの再呼出しを止める。
TA-Aのサービスソースはecho、試験トークン認可、AES生成鍵往復と既知解、panicを用意する。
固定トークンは要求者の実認証ではなく、TEE-02全体をPASSにはしない。
TA-Bは別UUIDで同じソースをビルドする方式を用意した。TA-A panic後のTA-B継続とTA-A再open試験はQEMUで実行しPASS。
TA間／Core不正参照・鍵取得攻撃・永続化試験は未実装。

Phase 4ではA/B1/B2をQEMUで実行した。実測と未完成項目はphase4_progress.md参照。B2外部BL33はMMU/cacheを有効化してcached SHM属性をCoreと一致させる。Aのcache無効条件とは異なる。
