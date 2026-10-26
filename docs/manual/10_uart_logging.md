# 10 UARTログ取得

## 目的

追加Python packageなしでraw UARTを保存し、起動段階と試験IDを同じrunへ対応づける。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。会社Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

USB-UART、BSP/実UART clock/baud/pins、確認済みdevice、アクセス権。Vitis Serial Monitorも選択可能。

## GUIのメニュー名

UG1400のSerial Monitorを使う場合、実device/baud/framingを指定して接続する。ログexport機能の有無は実IDE確認。上記stdlib方式ならGUI不要。

## 操作手順・実行コマンド

1. 実USB-UART接続を確認し、deviceを列挙する。複数portがあればboard配線とUART instanceを照合し、番号順だけで決めない。

```bash
python3 - <<'EOF'
from pathlib import Path
for pattern in ('/dev/serial/by-id/*', '/dev/ttyUSB*', '/dev/ttyACM*'):
    import glob
    for name in glob.glob(pattern):
        print(name, '->', Path(name).resolve())
EOF
read -r -p 'Verified UART device path: ' TZ_UART_DEVICE
read -r -p 'Verified baud rate: ' TZ_UART_BAUD
export TZ_UART_DEVICE TZ_UART_BAUD
```

2. BSP stdin/stdoutとTF-A console/OP-TEE UARTがどの物理portへ出るか確認し、8bit/no parity/1 stop/no flow controlが対象設定と一致する場合に次を使う。異なる場合はcodeを勝手に変えず実設定を確認する。別terminal/Serial Monitorと同時openしない。権限不足でsudoやchmodを自動適用しない。
3. 次を実行してから別terminalで承認済み起動手順を進める。Ctrl-Cで終了する。これはPOSIX stdlibのみでpyserial不要。UART取得自体は実機NOT_RUN。

```bash
python3 - <<'EOF'
import json, os, select, termios
from pathlib import Path
port = os.environ['TZ_UART_DEVICE']
baud = int(os.environ['TZ_UART_BAUD'])
speed = getattr(termios, 'B' + str(baud), None)
if speed is None:
    raise SystemExit('BLOCKED unsupported verified baud')
output = Path(os.environ['TZ_LOG_DIR'])
fd = os.open(port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
old = termios.tcgetattr(fd)
try:
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CLOCAL | termios.CREAD | termios.CS8
    attrs[3] = 0
    attrs[4] = attrs[5] = speed
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    (output / 'uart-settings.json').write_text(json.dumps(dict(device=port, baud=baud, framing='8N1 no flow control'), indent=4))
    with (output / 'uart.log').open('xb') as log:
        while True:
            if select.select([fd], [], [], 1)[0]:
                data = os.read(fd, 4096)
                if not data:
                    raise RuntimeError('UART disconnected or empty read')
                log.write(data)
                log.flush()
except KeyboardInterrupt:
    pass
finally:
    try:
        termios.tcsetattr(fd, termios.TCSANOW, old)
    finally:
        os.close(fd)
EOF
```

4. `BOOT`、`RESULT`、`SUMMARY`、`END`と例外情報を確認する。途中切断を正常完了にしない。Secure/Normalが別portなら別fileへ同時取得し、時刻/構成を合わせる。raw logを整形して上書きしない。

## 出力ファイル・実行ログの保存先

uart.log（raw bytes）、uart-settings.json、board/source/ELF metadata。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

本章の期待出力を保存し、終了コードと内容を両方確認する。

## エラー時の対応

文字化けはclock/baud/framing、無出力はMIO/instance/電源/到達PCを確認。port競合とUSB切断を確認し、取り直しは別run directoryへ保存する。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の会社環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲は[Phase 8報告](../phase8_progress.md)と[追加作業の確認記録](../../results/manual-validation-summary.json)を参照する。



## 公式資料の文書番号と章節

- [UG1244 v1.4 — Chapter 3 Board Component Descriptions → Programmable Logic JTAG Programming Options, Figure 3-6](https://docs.amd.com/v/u/en-US/ug1244-zcu106-eval-bd)。
- [UG1400 2025.1 — Configuring a Domain → Board Support Package Settings Page](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/Board-Support-Package-Settings-Page)。
- [UG1400 2025.1 — Using the Vitis Unified IDE → Launching the Vitis Unified IDE](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/Launching-the-Vitis-Unified-IDE)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[前へ](09_jtag_optee.md) · [目次](00_overview.md) · [次へ](11_test_execution.md)
