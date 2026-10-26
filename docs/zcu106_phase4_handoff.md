# ZCU106実機ロード前の最終確認

全実機試験NOT_RUN。既存backendはコンパイルとmock試験済みだがworld schedulingへ未接続。`make zcu106-prepare`、`python3 scripts/check_hardware_profile.py` が成功するまでロードしない。未確定値は `board_profile.json` のfalse/nullに維持し、チェックを回避するためにconfirmedを変更しない。

|対象|必要な証拠|取得方法／照合|
|---|---|---|
|DDR/OCMと予約|XSA/生成linker/BSP、FSBL/PMUFW map、TF-A/OP-TEEのELF PT_LOAD|Vitis2025.1で対象hardwareとStandalone domainを生成、mapとELFを保存しreadelfで比較|
|TF-A BL31|今回entry0xfffea000、LOAD終端0x100000000|FSBL/PMUFWのOCM利用・handoff・実起動時ELとNS属性を照合。暫定DDR TF-A予約と不一致なので予約案を確定し直す|
|DDR XMPU|全アクセス経路、port/bank、master ID/mask、default/region priority/lock、CTRL/POISON|UG1085 Chapter16、UG1087、生成hardwareとSecure側のread-only snapshot。CPU/DMAの許可/拒否が全経路で成立するか確認|
|保護窓／buffer|1MiB 0x62000000..0x62100000、64KiB 0x62010000..0x62020000は暫定案|Secureコード/data/stack、FSBL等と非重複を実mapで検査。1slot設定だけで保護成立としない|
|XPPU|対象周辺/aperture、20 master profiles、権限/parity/CTRL、試験の安全なregister|UG1085/UG1087、初期設定snapshot、基板接続図で選定。UART/JTAG/boot資源を推測で対象にしない|
|ZDMA|ADMA/GDMA channel、master ID、NS属性、routing、DATA_ATTR、buffer/cache、排他利用|UG1085 Chapter19、UG1087 ZDMA、embeddedsw固定版を照合。許可copyの内容比較と禁止read/write双方のXMPU新規違反を収集|
|GIC|GIC-400 IIDR、security extensions、group/priority/CTRL、SGI ID、SCR/DAIF routing|Secure/NS両worldから状態を確認。実SGIR/IAR/EOIRとcounter・register sentinelを使いhandler復帰を検査|
|例外vector|VBAR/2048-byte alignment、実link済み`.zcu106_vectors`|現在の任意vectorを実配置へ接続しreadelf/mapで保持確認。NS中Secure FIQのEL3経由切替は未実装|
|違反証拠|ISR/W1C、ERR_STATUS、master/page、CPU ESR/FAR/PC、DMA started/status|PMUFWによる並行clearを除外、試験前clear/readback後の新規違反だけを採用|
|非改変|Secure seed、拒否後Secure readback、境界、通常許可access|同一試験の証拠を紐付ける。例外/DMA errorだけでPASSにしない|

数値の根拠は [backend文書](zcu106_backends.md)の公式register/sourceリンクを参照。master IDやperipheralはこの表で確定しておらず、推測値の実機アクセスを禁止する。

Aはcustom EL3、B1はTF-A runtime、B2はopteedとOP-TEEを使用する。JTAG直接ロードとFSBL bootの初期状態をそれぞれ測り、QEMU flash imageを実機起動イメージへ流用しない。FSBL/PMUFWは今回未ビルド。Vitis GUIは実IDEで未確認。

実機試験に進む場合はread-only資料収集→予約/設定の整合→backendとvector接続→通常access→拒否access→Secure readbackの順で準備する。安全ゲートが通っただけでは機能PASSとしない。eFUSEや不可逆な認証/デバッグ設定は明示承認なしに変更しない。

## Phase 5補足

QEMUで追加したTA identity、shared input、key export、TA間/Core memory probeは実機では全NOT_RUN。Core probeはQEMU検証buildの自作PTAから得た実canary VAを使用する。ZCU106にはまだ適用しておらず、実機portとCore実配置、同PTAを採用するかの確認が必要。backendとboard profileの安全ゲートは変更していない。Phase 5の再検査ログは `results/phase5/zcu106-*-gate.log`。
