最新の公式2025.1資料との照合とFSBL/PMUFW手順は [Phase 6版](vitis_2025_1_phase6.md) を参照。以下は従来の導入案。

# Vitis Unified IDE 2025.1への導入案

Vitis未導入のためGUI実操作と全項目名の照合は未完了。以下は操作候補。
公式UG1400 2025.1の「Creating a Platform Component from XSA」と照合できたのは、
Platform Component作成とfixed XSAからembedded platformを作る流れである。
内蔵ZCU106定義の表示名と同梱有無は未確認。ない場合に架空の選択肢を案内しない。

1. Vitis 2025.1を起動し新しいworkspaceを選ぶ。版、ホストOS、インストールcomponentsを記録する。
2. `File > New Component > Platform` からPlatform作成を開始する。
3. 同梱ハードウェア定義の選択欄でZCU106があるか確認する。あればその定義を選ぶ。
   無い場合はインストール済みplatformのXSA取得元を確認する。内蔵定義のためだけにVivadoで再設計しない。
4. 対象processorを `psu_cortexa53_0`、OSを `standalone`、architectureを64bitにする。
   表示されない場合は入力hardwareが対象に一致しているか確認する。
5. Platform ComponentをBuildし、生成されたBSP、hardware handoff、lscript.ldを保存する。
6. DDR/OCM範囲、UART instance、stdin/stdout、clock、cache/MMU、startup、FSBL/PMUFW依存を確認する。
   [配置照合](memory_map.md)を終えるまでELFを実機へロードしない。
7. `File > New Component > Application` からアプリを作り、上記platformとStandalone domainを選ぶ。
8. 構成AではEL3/Secure/NSを別Application Componentにする。B1/B2ではBL33のみをApplicationにする。
9. 対象のC/.Sを取り込む。MakefileのCOMMON、EL1、RUNNERと各targetの一覧を参照する。
   BSPの既定crt0やmainと本プロジェクトの `_start` を同時リンクしない。
10. Include pathは `common/include`、`platform/zcu106`、B2は `optee_standalone/client`。
    C flagsはMakefileのCFLAGSを出発点とし、実GNU版で利用可能な警告を確認する。
11. Linker scriptは対応する `platform/zcu106/linker_*.ld` を照合・確定後に指定する。
    EL3のみのスクリプトをBL33に使わない。自作startのentryは `_start`。
12. BuildしELF/map、compiler版とコマンド、warnings、undefined referencesをログ保存する。
    `readelf -h -l -S`とcheck_memory_mapで実ELFを再検査する。

生成BSP startupが前提とするEL、MMU、cacheと、FSBL/TF-A handoffはそれぞれ確認が必要。
本プロジェクトの暫定UARTはUART0を仮定しており、ZCU106配線との照合がまだ無い。
BSPを利用する場合はplatform層だけを差し替え、commonへXilinx依存を入れない。
