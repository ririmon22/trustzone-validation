# TEE反復検証と性能基盤

## 反復試験

TEE-STRESS-01は32 warmup後に512 iterationを実行する。各iterationでA/Bをopenし、交互echo、不正Secure領域TMEM指定の拒否、拒否後のB echo、A鍵objectの解放・再生成・usage検査・暗号利用、AES生成鍵往復と別既知鍵KAT、両session closeを確認する。16 iterationごとにAをpanicさせ、Bの継続、A再open/echoを検査する。

測定512 iterationでは少なくとも1024回の通常open/closeと32回のpanic/reopen、512回の不正SHM・鍵cycleを実施する。warmup分は別途64 open＋2 panic/reopen。Core sessionの解放はTARGET_DEAD後の実CLOSE成功を要求する。local active session数だけでなく、検証PTAから取得したCore heap、TA RAM allocator使用量、allocation失敗数をwarmup後と終了時に比較する。増加・不完全iteration・返り値不正・FATAL・timeoutは成功にしない。

clean-workspace.jsonが指すrunのstress.jsonに実値と回数、results.jsonに実行ticks、optee.logにprogress/resourceを保存する。今回観測範囲でCore heap17200→17200、TA RAM942080→942080、malloc失敗0→0、512 iteration成功。これらは終了時のnet使用量であり、あらゆるallocator・瞬間的なリーク・無期限稼働を証明するものではない。実機・夜間耐久はNOT_RUN。鍵object削除の成功を生memoryの消去や抽出耐性の証明にしない。

Core canary/資源統計は明示的な検証PTA overlayで、通常の製品Coreとは別。基準点をwarmup後に採り、都合のよいtoleranceを加えてPASSにしていない。

## 性能

既存8系列はPERF-01/02/05/06/08/09/10/11。各warmup16/sample128、min/max/mean/median/p95/p99を保存する。区間の詳細はperformance_phase6.md。counterは実コードのCNTPCT_EL0、周波数はCNTFRQ_EL0から記録。CPU clockはunknown、Cortex-A53/TCG/1CPU。AはMMU/cache off、B2はon、NS IRQ masked。構成間の直接性能比較はしない。

Phase 7では各系列にempty indirect callをcounter readで挟んだ128回のOVERHEAD sampleを追加した。overhead.jsonは測定枠の基準値であり、Secure transition/TA service等のoverheadを全て分離したものではない。raw結果から自動減算しない。QEMUのスケジューリングやtranslationに依存するためZCU106 cycle値やCPU clockへ換算しない。

check_phase7_statistics.pyでraw SAMPLEから平均・中央値・nearest-rank p95/p99と極値を独立再計算してCSVと照合した。2回のclean runとも8系列が一致。countやmetadataもparser/reportが検査する。

Secure IRQ/DMAの実機性能は全NOT_RUN、QEMUでは未対応backendのためBLOCKED。hardware adapterをcompileできたことと実転送/IRQの測定成功を混同しない。
