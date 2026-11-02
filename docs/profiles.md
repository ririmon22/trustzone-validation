# 署名TA・物理構成・試験の選択

[入口](../README.md) · [基本build](build.md) · [PC検証](pc_validation.md) · [実機](zcu106_run.md)

## 署名TA構成（全てPCで実ビルド・検査済み）

```sh
# 初回だけ生成する。既存の試験鍵があるときはこの生成コマンドを繰り返さない。
build/python/bin/python scripts/make_signed_bundle.py keys --keys build/signed-test-keys
# local.mkのTA_SIGN_KEY / TA_PUBLIC_KEYを上記のtrusted.pem / trusted.pub.pemへ設定
make -j4 PROFILE=signed bl31
make -j4 PROFILE=signed optee-sdk
make PROFILE=signed ta-a
make PROFILE=signed ta-b
make PROFILE=signed ta-external
make PROFILE=signed prepare-core
make -j4 PROFILE=signed bl32
make PROFILE=signed TOOLCHAIN=clang bl33
make PROFILE=signed TOOLCHAIN=gcc bl33
build/python/bin/python scripts/make_signed_bundle.py bundle \
  --keys build/signed-test-keys --ta-dir build/components/signed/ta-external \
  --output build/components/signed/bundle --optee third_party/optee
```

鍵は試験専用・Git管理外。正常・改変・別鍵TAを同じ1MiB bundleへ格納する。
TA-A/Bはstripped ELFをEarly TAとして組み込み、付随する.taファイルの署名を実ローダ試験の証拠には使わない。
`prepare-core` は元OP-TEEを変更せず、canary PTAと署名ローダ監査パッチをbuild用コピーへ適用する。
コピーが既存なら停止するので、新しいBUILD_ROOTを使う。対応版以外への監査パッチ適用は拒否する。

```sh
build/python/bin/python scripts/check_signed_profile.py --platform zynqmp \
  --core build/components/signed/optee/core/tee.elf --conf build/components/signed/optee/conf.mk \
  --bundle build/components/signed/bundle --keys build/signed-test-keys \
  --optee third_party/optee --output results/signed-profile.json
```

このPCで正常署名=受理、改変=拒否、別鍵=拒否、別鍵用公開鍵なら受理を確認した。
Core公開鍵・外部TA非Early・監査PTA・bundle slot/digestも検査する。
**PCの公式sign_encrypt.pyの結果であり、実OP-TEEローダでの8ケースは実機未実行。**

## 物理構成（PCで実ビルド・静的予約検査済み）

```sh
make -j4 PROFILE=physical bl31
make -j4 PROFILE=physical optee-sdk
make PROFILE=physical ta-a
make PROFILE=physical ta-b
make PROFILE=physical prepare-core
make -j4 PROFILE=physical bl32
make PROFILE=physical TOOLCHAIN=clang bl33
make PROFILE=physical TOOLCHAIN=gcc bl33
build/python/bin/python scripts/check_b2_review_profile.py --root . --platform zcu106 \
  --elf build/bl33/clang/physical/optee_bl33.elf \
  --core build/components/physical/optee/core/tee.elf \
  --conf build/components/physical/optee/conf.mk --output results/physical-profile.json
build/python/bin/python scripts/check_b2_physical_reservation.py \
  --elf build/bl33/clang/physical/optee_bl33.elf \
  --elf build/components/physical/optee/core/tee.elf --output results/physical-reservation.json
```

Coreにはcanary＋physical PTAと0x10800000..0x10900000の専用登録を追加する。
TA/Core allocatorと他のELF予約との静的整合を検査したが、DDR/XMPU/他master/aliasの実機成立は未確認。
`B2_PHYS_REVIEWED=0` を維持。静的PASSを実際のアクセス拒否PASSへ読み替えない。

## 個別選択

| Make指定 | 範囲 |
|---|---|
| `make bl33` | 元からのselection=14（従来機能ルート）。RAMログで確認し、b2_resultsの新しい集計表は使わない |
| `make B2_TEST_INDEX=0 bl33` | 機能13子試験。新しいb2_results表を使う |
| `make PROFILE=signed bl33` | selection=17、署名・canaryを含むreview。物理は含まない |
| `make PROFILE=signed B2_TEST_INDEX=16 bl33` | 外部署名TAの単独試験 |
| `make PROFILE=physical bl33` | selection=15、物理専用probe |
| `make PROFILE=signed B2_REVIEW_PARENT=10 bl33` | review親10（EXT-AUTH-V01）を選ぶ例 |
| `make PROFILE=signed B2_TEST_INDEX=18 bl33` | 8系列の性能測定。SGI/DMA性能は含まない |

全既存試験コードは維持。今回の実ビルド一式は基本14・署名review17・物理15の3構成であり、
他の単独選択の実機動作は未確認。`check_components.py` はこの3つの既定選択を厳密に照合する。
選択変更の際は、その設定を意図したものとして別に記録・検査する。legacyの選択CLIは使わない。
親番号は [21項目表](b2_scope_21.md) とreview.cのparent_ids、子番号はfunctional.cのcasesに対応する。
