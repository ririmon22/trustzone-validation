# 継続作業

Phase 4でQEMU A/B1/B2実行、ZynqMP BL31、OP-TEE QEMU Core/SDK/Early TAのbuildは完了した。実ログは [報告](phase4_progress.md)。再現は `bash scripts/run_phase4.sh`。

次の優先順：

1. [ZCU106確認票](zcu106_phase4_handoff.md)に従い、生成hardware/BSP、FSBL/PMUFW map、OCM/DDR予約とmaster/route snapshotを取得する。既存ロードゲートを維持する。
2. ZynqMP BL31の実OCM配置とhandoffを照合し、安全なA/B1/B2別メモリ配置を確定する。
3. XMPU/XPPU/ZDMA backendと実Secure/NS試験、任意vectorを接続し、非同期Secure FIQのworld切替・状態保存を実装する。
4. B1 Secure EL1検証serviceを標準TF-A機構へ統合する。
5. trusted requester認証、TA間/Coreアクセス攻撃、鍵取得/削除、署名付き通常TA改ざん拒否、永続storage/RPCを実装する。
6. PERF-03/04/07を実経路へ接続し、Vitis2025.1実IDEの手順と実機試験を確認する。

会社での作業が実機設定と最終評価だけで済む段階にはまだ達していない。eFUSEや不可逆設定は自動変更しない。

Phase 5でTA/Core fixture分離、TA identity、共有memory、key exportの実行検証は追加済み。最新再現は `scripts/run_phase5.sh`。次はNormal要求者認証、鍵memory抽出、通常TA runtimeロード/RPC、storage、実機資料照合。[詳細](phase5_progress.md)。
