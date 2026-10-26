# ZCU106バックエンドの統合条件

全実機試験はNOT_RUN。追加コードはビルド・ホストmockで検査したが、実機の接続経路は未確定。
デフォルトのboard_profileは未確認値をfalse/nullに保ち、既存monitorの停止条件も維持する。

## 配置とXMPU

XMPU保護窓は `XMPU_PROTECTED_BASE/END`、検査バッファは `PROTECTED_BUFFER_BASE/END`。
暫定案では1 MiB窓0x62000000..0x62100000の内側に64 KiB buffer0x62010000..0x62020000を置く。
Secureコード・データ・stack、BL31/BL32予約枠と分離している。
生成BSP、FSBL/PMUFWの実使用範囲がないため、安全な実配置と確定した意味ではない。
`check_memory_map.py` は既知allocation/予約との競合、buffer包含、窓alignment、header一致を検査する。
`--require-protection` は未知のfirmware予約もBLOCKEDとする。

`xmpu_program_secure_region()` は確認済みDDR bankの無効slotだけを設定する。
START/END/MASTERをreadbackした後CONFIGを有効化する。
CTRL/POISONは事前に正しく設定されていることを実値と照合する。これらの既存全体設定を勝手に上書きしない。
`xmpu_verify_secure_region()` は試験後にも再確認できる読出専用API。
全portの経路、masterのmask/ID、他regionの優先順位とdefault許可、lock状態の確認が別途必要。
単一slot設定だけで全masterからの隔離成立と判定しない。

## XPPU

`xppu_program_secure_aperture()` は既存の有効XPPU構成から権限を縮小してSecure-onlyにする。
20個のMASTER_ID profileとCTRL、変更前apertureを照合する。
parity込みの新しい値は確認済みprofileから与える。parity方式を推測して値を生成しない。
`xppu_verify_secure_aperture()` で試験後にprofileとpermissionを再検査する。
対象周辺・apertureは未選定。UART/JTAG/boot等を巻き込む設定は作成していない。

## 違反情報とCPUアクセス

`violation.c` はISRをW1Cで消し、clearのreadback後に一回の試験をarmする。
`xpu_probe_capture()` はISR→ERR_STATUS1→ERR_STATUS2→ISRを取得し、途中で状態が変化した証跡を拒否する。
`xpu_capture_matches()` はstatus、4 KiB address page、masterの一致を検査する。
XPPUもaddress page単位までを使用し、sink offsetを使うbyte単位の復元は行わない。
同一page内の別accessと区別するにはFAR等と試験の排他的実行が必要。
PMUFW等が並行してISRを消す場合はexclusive条件を満たさずBLOCKEDにする。

`access_probe.c/.S` は32/64-bitの実load/storeを用意した。
復帰はarmしたaccessのPC、FAR、read/write、例外class/DFSC、FnVを検査して限定する。
予期しないabortやSErrorは成功扱いにしない。対象CSRの幅・副作用は周辺仕様で確定する。
周辺の非改変判定は対象デバイスの実状態を読むSecure側adapterが必要。

## DMA

`zdma.c` は実ZDMAのAPB simple descriptorへ両address/sizeを書き込み、CTRL2.ENで転送を開始する。
対象は確認済みADMA/GDMA channel、normal memoryのincrement burst、通常read/write、over-fetchなし。
busy/paused channelを再利用せず、DATA_ATTRの確認済みsnapshot、flow-controlなし、buffer/cache条件を要求する。
ISRの古いDONEを消してから開始し、DONE＋idle、error、timeoutを分けて返す。
descriptorのreadback、raw ISR/STATUS、TOTAL_BYTE、poll回数、startedを記録できる。
TOTAL_BYTEだけで転送内容が正しいとはしない。

`dma_validation.c` が既存 `dma_test_ops` に接続する。
通常copyは転送後の内容を比較するcallbackを必須にし、禁止read/writeはXMPUの新しい違反とmaster/pageを照合する。
無関係のDMAエラーやtimeoutはアクセス拒否のPASSへ変換しない。
Secure seed/readbackはSecure service側callback、NS bufferの準備・比較はNormal側callbackへ接続する。
既存判定はread側destinationの非改変も要求するため、poison応答がzeroを書き込む構成では保守的にFAILとなり得る。
その場合はDDR応答と観測内容を記録して試験期待値を再設計し、結果だけをPASSへ変更しない。

timeout/error後のchannel reset/recoveryは未実装。EN/stateを確認し、必要な回復手順が確定するまで再試験を止める。
DMA masterのNon-secure属性はDATA_ATTRのcache設定だけでは確認できない。
TrustZone制御・routing registerとXMPU経路を生成hardware/実設定から確認して初めてverifiedにする。

## GICと例外ベクタ

`gic.c` はZynqMP GIC-400のSecure設定としてgroup0/1の二つのSGIとpriorityを設定しreadbackする。
security extension、IIDR、変更前CTRL/IGROUPR、pending/activeなしを確認する。
`gic_send_sgi()` は実SGIRへself targetとsecurity属性を書き込む。
`gic_acknowledge()/gic_end_interrupt()` はIAR tokenを取得し、送信CPU情報を保持してEOIRへ返す。
spurious IDへEOIを書かない。EOI split modeは本版では扱わない。

`test_vectors.S` は任意採用のEL1 vectorで、全整数registerを保存してIRQ/FIQ handlerへ接続する。
使用するときは `.zcu106_vectors` をlinker scriptに明示的に保持し、実ELFの2048-byte alignmentを検査する。
DAIFをマスクしてSecure/Normalそれぞれのhandlerをbindし、VBARを設定した後に必要なmaskだけを解除する。
本repoの既存ELFへ自動でlink/installはしていない。
`interrupt_dispatch.c` は実IARのIDに応じたcounterを増やすが、resumed/context_preservedを勝手に成功へ設定しない。
復帰した試験callerがregister sentinelと処理継続を検査する。

Normal実行中のSecure FIQをEL3経由でSecure EL1へ渡すための非同期world切替は、現monitorにまだ統合していない。
SCR/GIC routing、各worldのpriority/mask、Secure context保存を確認するまで
`interrupt_test_ops.deliver` を実機で有効化しない。
単一CPU・GPRのみ・非nested handlerという範囲であり、SMP/FPU/一般OS用driverではない。

## 公式根拠

- AMD [UG1085 Chapter 16](https://docs.amd.com/v/u/en-US/ug1085-zynq-ultrascale-trm)：保護単位とsystem経路。Chapter 19：ZDMA simple mode。
- [UG1087 ZDMA module](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/ZDMA-Module)：revision 1.11、2025-06-20、channel base/offset。
- [UG1087 ERR_STATUS1 DDR](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/ERR_STATUS1-XMPU_DDR-Register?contentId=mlm8WlcsXejIL3nvXTG1Ig)：address pageの符号化。
- [UG1087 ISR DDR](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/ISR-XMPU_DDR-Register?contentId=hUKxfuN6zFHTXs2nNudmuw)：W1Cとsecurity/read/writeのstatus。
- [UG1087 GIC400](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/GIC400-Module)、[GICC_CTLR](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/GICC_CTLR-GIC400-Register?contentId=2xxB63b0f3LNNR9f~gXMlQ)：実base/offset。bit意味はArm GICv2仕様への参照がある。
- AMD公式embeddedsw commit `c0aed2eff7a30f307238ec853fa8fbc45dcabdda` の [xzdma_hw.h](https://github.com/Xilinx/embeddedsw/blob/c0aed2eff7a30f307238ec853fa8fbc45dcabdda/XilinxProcessorIPLib/drivers/zdma/src/xzdma_hw.h)、[xscugic_hw.h](https://github.com/Xilinx/embeddedsw/blob/c0aed2eff7a30f307238ec853fa8fbc45dcabdda/XilinxProcessorIPLib/drivers/scugic/src/xscugic_hw.h)、[xpfw_xpu.c](https://github.com/Xilinx/embeddedsw/blob/c0aed2eff7a30f307238ec853fa8fbc45dcabdda/lib/sw_apps/zynqmp_pmufw/src/xpfw_xpu.c)：register bit/offset、違反取得方法の照合。

公式ソースを改変して取り込んだものではない。自作backendと公式定義の照合であり、実機動作確認の代わりにはしない。
