# ZCU106/B2：現在使用する移送版 20261031-r1

TF-A BL31＋OP-TEE BL32＋Standalone BL33の検証source。111ファイル版を基準に、説明を次の3本へ統合し、性能metadataのcache/MMU固定表示を修正した。旧4説明書を置換したため、本資料版の移送ファイル数は110。ファイル数の差は資料の統合によるもので、試験範囲は変更していない。 公開main、QEMU、A/B1、旧実機成功版と混同しない。

## 読む順番

1. [現版の範囲・21項目一覧](docs/b2_scope_21.md)：12実行可能・物理保護1条件待ち・8今回対象外、旧版USER_REPORTEDと111版NOT_RUNの区別。
2. [試験仕様・構成図・シーケンス](docs/b2_test_specification.md)：配置と呼出しを別図にし、入力→検査→拒否→比較→後始末→回収を説明。
3. [仕組みとコードの読み方](docs/b2_reading_guide.md)：ECHO→TA間分離→署名TAの順に学ぶ。初学者は本書の用語表を先に読んでもよい。

操作手順は[ソースビルド](docs/zcu106_build.md)と[起動準備・RAM回収](docs/zcu106_run.md)。外部sourceとツールは別配置し、自動downloadをしない。

```bash
cp config/b2.example.json config/review.local.json
# 外部source位置、compiler、固有run_idを記入
python3 scripts/field.py b2-check config/review.local.json
python3 scripts/field.py b2-build config/review.local.json
```

B2_RUNTIME_READY=0、B2_PHYS_REVIEWED=0、ロード禁止gate BLOCKEDを維持。111版の実機・実XSDB・Vitis/GNUはNOT_RUN。ここに実機成功ログは同梱しない。BUILD/静的検査/ホスト合成RAM検査と実機機能成功を混同しない。署名TAのCore監査、物理probeの拒否層、採取SCTLRからの性能metadata表示も仕様書に記載している。

現在使用する配布ZIPは **trustzone-validation-zcu106-b2-20261031-r1.zip**。本READMEが入口。旧111版・旧資料版は比較/保全用として扱い、混在させない。SHA-256はZIPと同じ場所の同名`.sha256`で照合する。
