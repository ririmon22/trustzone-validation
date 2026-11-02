# 旧JSON一括手順：参照専用・実行不可

通常の入口は [ルートREADME](../README.md) と [現行Make](../Makefile)。
ここには旧JSON build、workspace複製、ZIP出所検査、旧ログ結合integration検査と旧資料を移動した。
ファイルのコピーは作っていない。

旧経路は `build/external/`、旧ソース複製対象、旧command receiptに結合している。
移動後の動作維持は行わず、**legacy/scriptsのCLIは明示的に停止する**。
現行Makeの生成物に対して使用不可。過去の手順の再現が必要なら、基準commit
`672e4a05a8ed7be600d22d7713c2d77a5c79afc8` を別worktreeで参照する。
現行の外部ソース・鍵・生成物へ旧手順を重ねない。

| 場所 | 内容 |
|---|---|
| scripts/field.py、build_zcu106.py、build_external.py | 旧JSON一括build・plan・decode入口 |
| scripts/build_workspace.py、select_b2_test.py | 旧ソース複製・JSONによる試験選択 |
| scripts/check_build_environment.py、source_snapshot.py、zip_sources.py | 上記専用の環境・来歴検査 |
| scripts/stage_optee_overlay.py | 旧自動化用Core複製。現行はscripts/prepare_core.py |
| scripts/check_b2_integration.py | 旧ログ形式を要求するBL31/Core/TA統合検査。現行はscripts/check_components.pyへ必要な検査を移植 |
| config/b2.example.json | 旧ビルド入力例。通常Makeでは未使用 |
| docs/zcu106_build.md、compiler-runtime.mk | 旧ビルド資料・外部Clang runtimeレシピ（参照のみ） |

現行の署名生成・配置検査・回収・デコーダはルートの `scripts/` に残す。
共有ELF解析は旧integrationから `scripts/elf_symbols.py` へ処理を移し、旧buildに依存しない。
旧資料中のコマンドやパスは履歴を読むためのもので、現在の実行手順ではない。
