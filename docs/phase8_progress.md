# Phase 8 実績と未検証範囲

ソースのみのfresh build helper、構成別依存検査、オフライン導入手順を追加した。Ubuntu24.04/Vitis2025.1は未導入、GNU cross compilerもないため、その組合せはNOT_RUN。実機全試験NOT_RUN、load gate BLOCKEDを維持する。

## 成果物

- [最小依存表](minimum_dependencies.md)：構成・実command・OS基本tool・Vitis候補・追加packageを区別。
- [VitisオフラインbuildとIDE取り込み](vitis_2025_1_offline_build.md)：Aの自作EL3とB1/B2のNS BL33、source/compiler/linker/bootを区別。
- [Git source受け渡し](source_only_transfer.md)：固定commit、全取得対象、full checkoutと開発subsetの違い。
- [環境検査](../scripts/check_build_environment.py)、[build wrapper](../scripts/build_vitis_offline.sh)、[controller](../scripts/offline_build.py)。インストール処理・remote取得処理なし。

## 実測結果

|対象|結果と範囲|実際の証跡|
|---|---|---|
|Phase7 hash再確認|既存18 artifactすべて一致。以前のlogは上書きしていない|[hash](../results/phase8/phase7-hash-check.json)|
|stdlib完全baremetal|外部Python moduleなし、3 ELF新規生成。Python3.12.14でも成功|[receipt](../results/phase8/minimal-baremetal-result.json)、[3.12 log](../results/phase8/python312-baremetal-console.log)|
|source-only local Git試験|BUILD PASS。12自作ELF、TF-A、SDK、TA、Coreを空のbuild先で生成。prebuiltコピーなし|[source/mirror receipt](../results/phase8/source-only-git-trial.json)、[build/hash](../results/phase8/git-trial-results/build-result.json)|
|QEMU A|3 PASS / 0 FAIL / 3 BLOCKED / 1 SKIP|[UART](../results/phase8/git-trial-results/qemu/baremetal.log)、[再監査](../results/phase8/git-trial-results/qemu/tz-audit.json)|
|QEMU B1|1 PASS / 0 FAIL / 6 BLOCKED / 1 SKIP|[UART](../results/phase8/git-trial-results/qemu/tfa.log)|
|QEMU B2|9 PASS / 0 FAIL / 7 BLOCKED、stress512回。heap/TA使用量前後一致、allocation failure増分0|[Normal UART](../results/phase8/git-trial-results/qemu/optee.log)、[Secure UART](../results/phase8/git-trial-results/qemu/optee-secure.log)、[fault照合](../results/phase8/git-trial-results/qemu/security-evidence.json)|
|性能基盤|8系列各128サンプル。QEMU上の基盤検証のみ|[A CSV](../results/phase8/git-trial-results/qemu/baremetal-benchmarks.csv)、[B2 CSV](../results/phase8/git-trial-results/qemu/optee-benchmarks.csv)|
|host unittest|既存116件を含む120件成功、`python3 -S`使用|[最終log](../results/phase8/final-host-tests.log)、[fresh Git受領先log](../results/phase8/git-trial-results/host-tests.log)|
|ZCU106向けsource build|5自作ELFとdriver、BL31 B1/B2、ZCU106 BL32、ARM64 SDK、Early TA-A/B BUILD PASS|[receipt](../results/phase8/latest-all-zcu106.json)、[全log](../results/phase8/runs/20261026T213203906326/)、[ELF inventory](../results/phase8/board-elf-inventory.json)|
|実機load検査|BLOCKED、予約/配線/boot等未確定。実機NOT_RUN|[profile gate](../results/phase8/runs/20261026T213203906326/load-gate.log)、[boot gate](../results/phase8/boot-plan-gate.json)|

試験ID・期待値・実値・status・ticks・UARTの対応は[結果表](../results/phase8/results.json)、集計は[summary](../results/phase8/summary.json)。raw parser出力のbuild_status UNKNOWNは保存し、集計表で同一fresh runのbuild-resultを対応づけてBUILD PASSとした。UART原本は改変していない。archived logの原所在は[記録](../results/phase8/git-trial-evidence-origin.json)。実行時間はgeneric timer ticksで、実CPU性能を示さない。

新規生成したTAについて公式署名検査5ケース（正常・署名改変・payload改変・誤鍵・誤UUID）も再実行し全PASS。[host署名結果](../results/phase8/signed-ta/results.json)。これは通常TAのOP-TEE runtime loadのPASSを意味しない。使用した[cryptography版](../results/phase8/python-crypto-version.json)も保存した。

## 依存を削減した内容

完全baremetalはBSP、TF-A、OP-TEE、cryptography、pyelftools、host C、QEMUなしで3 ELFを生成できる。自作署名検査のcryptography importを署名実行時へ移し、無関係なhost testへの波及をなくした。公式OP-TEEの署名はcryptographyをそのまま使う。pyelftoolsは固定Git sourceをPYTHONPATHで使いpip不要。GNU routeではlibgccを使う設計で、Clang用compiler-rt sourceは不要になるがGNU routeの実buildは未実施。

QEMU FIP用native C/libcrypto依存と実機BL31だけのbuildを分けた。TF-A公式の必要targetsだけを指定し、未使用qemu_fw image生成を省いた。source自体の改変ではない。実行Pythonと公式scriptのshebang経由Pythonをprivate PATHで統一した。

固定OP-TEE commitには公式`plat-zynqmp`のZCU106 flavorが存在し、4原本fileを取得・Git blob照合してbuild subsetへ追加した。[固定conf.mk](https://github.com/OP-TEE/optee_os/blob/0919de0f7c79ad35ad3c8ace5f823ad1344b4716/core/arch/arm/plat-zynqmp/conf.mk) の既定TZDRAM 0x60000000/256MiB、SHMEM 0x70000000/256MiB、4core、UART0はcompile defaultsとしてのみ記録する。実機予約のCONFIRMED値ではない。TF-A B1 entry 0xfffea000に対し、B2 SPD=opteedはentry 0x1000へ変わる。暫定boot plan/headerと合わないため、build成功でloadを許可しない。

## 試験環境の限界と次の作業

実hostはUbuntu26.04.1、Clang/LLD21.1.8、QEMU10.2.1、Python3.14.4。追加でPython3.12.14のstdlib baremetalだけを試験した。toolの実path/versionは[environment](../results/phase8/git-trial-results/environment.json)。制限PATHは[一覧](../results/phase8/minimal-path.json)。既存OSのlibrary/headerは共有しており、最小OS/containerでの試験ではない。

local Git試験は固定upstream blobを検証したsubsetを一時mirror commitへ入れてcloneしたもの。原本commitのfull checkoutを試験したふりをせず、両commitをreceiptへ記録した。full pinned checkoutが既定であり、subsetを既定で拒否することも[確認](../results/phase8/strict-git-preflight.json)した。会社Gitの到達性・全object/依存取得は現地確認が必要。

build中はGitをfile protocolに限定しlazy fetchを禁止、PATHからdownload/install commandを拒否する試験を行った。通信syscall監査は[ptrace EPERM](../results/phase8/strace-probe.log)で未実施。OSの保護設定は変更していない。第三者任意build scriptの通信を隔離するnetwork sandboxを実装したとは主張しない。

次に会社で、実在するVitis toolchain prefixを検出してcheck_build_environmentを実行し、完全Git checkoutからbaremetal→B1→B2を順にbuildする。VitisのPlatform/BSP/FSBL/PMUFW生成物を保存し、予約領域・XMPU/XPPU master・DMA/GIC経路・起動順とboot planを確定するまで実機load/runしない。署名TA runtime load、Normal要求者認証、storage backend、鍵memory抽出耐性、DMA/Secure IRQの実機性能は既存の未完了状態を維持する。
