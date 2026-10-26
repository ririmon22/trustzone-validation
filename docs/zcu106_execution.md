最新の起動構成、ロードゲート、JTAG/UART、未確定事項は [Phase 6手順](zcu106_phase6_porting.md) と [確認票](phase6_hardware_checklist.md) を参照。

# ZCU106実機試験準備

全実機試験はNOT_RUN。現状の暫定mapと未実装保護ドライバのままロードしない。
以下は実行前に完成させる試験票であり、検証済みJTAGスクリプトではない。

1. 基板revision、SoC、DDR容量、電源、UART/JTAG接続とboot-modeスイッチを基板資料で確認し記録する。
2. UARTポートとbaudを初期化設定と一致させ、電源投入時からログ取得する。
3. JTAG直接とboot-image起動を分ける。前者はJTAG modeを基板資料で確認、後者はSD/QSPIなど指定媒体へ設定する。
   スイッチのON/OFF方向を推測で設定しない。
4. デバッガ接続直後にCPUのhalt状態、CurrentEL、SCR_EL3、PC、SCTLR、cache/MMU、debug permissionを記録する。
   CurrentELだけからSecurity Stateを断定しない。EL3からSCRと遷移先contextも調べる。
5. PS/DDR/UART初期化は選択hardwareの生成初期化スクリプトまたはFSBLを使う。
   FSBLを起動したという事実だけでEL3 entryを仮定しない。handoff属性とCPU状態を確認する。
6. PMUFWの版と必要性を選択boot経路・FSBL依存から決める。別版PMUFWを黙って混ぜない。
7. ELF load rangeとFSBL/PMUFW/BL31/BL32の使用範囲を照合し、readbackで想定entryのcodeを確認する。

|構成|起動の責務|現在の不足|
|---|---|---|
|A/JTAG直接|PS初期化後のSecure EL3へ自作monitorをロード|確定map、XMPU、CPU状態保証|
|A/FSBL経由|FSBLのdestination EL/security属性を検証してmonitorへhandoff|BIFとhandoff試験|
|B1|FSBL/PMUFW→BL31→NS BL33|BL31配置と専用Secureサービス|
|B2|FSBL/PMUFW→BL31→OP-TEE BL32→NS BL33|Zynq OP-TEE port、dispatcher、SHM/RPC、全配置|

試験TZ-SYS-03は保護前の許可アクセスを記録してから保護設定を行い、拒否後にSecure側で非改変を確認する。
XMPUはCPUだけでなくDMA master ID/TrustZone属性を含めて調べる。
TZ-SYS-04ではUART/JTAG/boot資源を避けた検証用周辺を配線図と初期状態から選定し、
XPPUのaperture/master permissionを設定して実状態を比較する。対象周辺はまだ未選定。
TZ-SYS-05は通常DMA転送で制御試験を行い、次にSecure read/write両方向の拒否とerror/statusを確認する。
TZ-SYS-06はGIC版、group0/1S/1NS、優先度とroutingを確認し、timer/SGIによる発火と復帰を記録する。
TZ-SYS-07はdebug認証信号と許可状態を確認し、可逆的に試験できる範囲を確定する。
TEE-08は認証有効なboot chainと改ざんimageの拒否理由を比較する。
eFUSE、不可逆debug lock、boot認証設定を自動変更するコードは提供していない。

各試験はresults/zcu106へUART/raw debuggerログと設定dumpを保存する。
実機のCPUクロックはPLL/clock設定と計測根拠を記録する。CNTFRQをCPUクロックと混同しない。
