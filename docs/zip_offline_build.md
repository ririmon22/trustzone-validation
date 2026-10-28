# ZIP展開ソースからのオフラインB2構築

## モードと判定

`source_mode=zip`を明示した専用入口は`build_zip_b2.py`。既存の`build_vitis_offline.sh`/source_snapshot.pyの既定は固定Git/来歴検査のまま。`--allow-snapshots`は任意ZIPの許可ではなく、検証済み開発subset専用である。

ZIPモードは.gitやcloneを要求しない。元展開directoryを読取り、必要構造・version/interface・tree hashを確認して、新規workspaceへcopyする。SDKや過去のELFをinputにしない。build先はresults/zip-b2/<UTC>/workspaceで、元sourceを変更しない。source treeには.o/.a/.elf/.taなどの生成物を混ぜない。既に直接buildしたfolderで拒否された場合は元を消さず、新しいcleanな展開directoryを用意する。

|判定|意味|
|---|---|
|source_provenance=ZIP_UNVERIFIED / verified_commit=null|固定commit確認済みではない。folder名/宣言version/同じbuild成功からcommitを推測しない|
|DEPENDENCIES READY|tool/構造のpreflightのみ。全互換性やload許可ではない|
|build_status=PASS|ソースから必要生成物をbuildできた|
|static_status=PASS|生成ELF・SHM/cache・Early TA・opteedなどのsoftware契約が整合|
|hardware_load=BLOCKED|実機予約・保護・初期化・handoff等の証跡不足、runtime gate0|
|qemu_execution|要求した場合のみ実run結果。実機性能/動作ではない|

## 1. 配置と短い入口

repo rootで設定templateをコピーし、実在する展開directory/tool prefixを編集する。local設定はGitへ入れない。各pathはJSONのdirectoryに対して相対、または絶対path。

```bash
cp config/field.example.json config/field.local.json
python3 scripts/field.py b2-check config/field.local.json
python3 scripts/field.py b2-build config/field.local.json
```

設定する主項目：

- source_mode: zip。platformはzcu106が既定。compilerはgcc/clang/auto、cross_compileは実在するAArch64 GNU prefix。PCのgccをprefixとして使わない。GNU/Vitis routeは未検証。
- sources.tfa-amd/optee/pyelftools: directory、archive（任意。なければnull）、declared_version。GitHub ZIPの一段上のfolderではなくMakefileが直接存在するrootを指定する。
- compiler=clangの場合のみcompiler-rt sourceも必要。LLVM rootをdirectoryにする。llvm_dirはclang/ld.lld/llvm-objcopy等があるbin directory。
- host_testsをtrueにするとhost clangや、ローカルGit bundle fixture用のGitを使う試験も実行する。GitがなければそのfixtureだけSKIP。ZIP source検査にGitを必須にしない。
- archiveが存在すればZIP SHA-256を記録する。ただしarchiveと展開treeの同一性を自動証明してはいないので、bindingはUSER_SUPPLIED_NOT_VERIFIED。folderのみならDIRECTORY_ONLY。tree hashは別に保存する。

不足tool/module/fileはBLOCKED理由とcommands.logに表示する。取得・git clone・pip/APT installは行わない。buildのGit protocolは無効。host unittestだけはローカルbundle fixtureのためfile protocolを許可する（ネットワーク/cloneではない）。

## 2. 確認する版とinterface

比較用lockはthird_party/dependencies.lock.jsonに保持するが、ZIP modeではそれを照合済みcommitとして出力しない。

|source|このプロジェクトで扱う版|確認内容|
|---|---|---|
|AMD TF-A|2.12系 / 2025.1候補|Makefile VERSION_MAJOR/MINOR、ZynqMP platform、opteed、BL31 source等。upstreamとの同一性やAMD release由来は未証明|
|OP-TEE|4.5.0候補|SDK/Early TA生成script、AArch64/TF-A設定、zynqmp-zcu106 platform、必要API。標準展開物だけでは正確なversionを読めない場合がありobserved_version=null|
|pyelftools|0.32|__version__とELFFile import。pipは不要|
|compiler-rt（Clangのみ）|21.1.8候補|実udivti3/udivmodti4/headerをbuild。GNUではtoolchainのlibgccを検査|

明示したdeclared_versionが対応label外ならpreflightでBLOCKED。宣言がない、またはOP-TEEのように観測版が不明なら構造検査のみであり、別版互換性を確認済みと扱わない。構造が似ていても不足source/APIがあればbuild/staticで失敗する。TF-A bannerはZIP-UNVERIFIED、OP-TEEのTEE_IMPL_VERSIONもZIP-UNVERIFIEDであり、固定commit名を偽装しない。

## 3. 必要toolとPython

最小限はGNU Make、bash/coreutils/cmp、Python>=3.9、AArch64 C/ASM/link/ar/objcopy/objdump/nm/readelf一式。OP-TEE/TA署名は選択Pythonのcryptographyが必要。pyelftoolsは指定sourceからPYTHONPATHでimportする。これらはVitisに付属していると仮定しない。Clang routeにはLLVM同一系toolと上表runtime sourceが必要。QEMU実行時のみqemu-system-aarch64、TF-A QEMU FIP生成時のみnative C compiler/OpenSSL header/libcrypto/opensslが必要。検出したtoolのpath/versionと実AArch64 ELF生成probeをpreflightへ保存する。

## 4. 一括buildの順序

ZCU106ではruntime（Clangのみ）→SDK→TA-A/B→Early TA組込みCore→SPD=opteed付きB2 BL31→BL33→静的検査→host tests→load gate検査を行う。TAにはCROSS_COMPILE/CROSS_COMPILE64/CROSS_COMPILE_ta_arm64を全て指定する。

QEMUを選ぶ場合はplatform=qemu、qemu=trueにする。FIPがBL33 binaryを要求するため、Core→BL33→TF-A/FIPの順だけは異なる。QEMU Core canary/stats PTAは明示した作業用copyにoverlayし、base_commit=null・ZIP_UNVERIFIEDを記録する。原本と通常ZCU106 Coreへ無断で追加しない。

判定と出力はresults/zip-b2/<UTC>/result.json、source-inputs.json、preflight.json、workspace/results/phase8/*。原本はcacheを含むfile内容・modeのfingerprintでbefore/afterを比較する。preflightのPython importにもPYTHONDONTWRITEBYTECODE=1を指定し、inputへpycを作らない。ZIP SHAと展開tree SHA、build/static/hardwareの状態は別項目。失敗したrunも削除しない。

## 5. 検証範囲

ローカルで実施するZIP試験は、利用可能な固定blob開発subsetからZIPを再構成し、.git/.source_manifest.jsonを含めず展開したfixtureを用いる。GitHubから取得した元ZIPそのものではなく、完全GitHub ZIP・移送先のZIP・GNU/Vitis/Ubuntu24.04環境を検証済みとは主張しない。archiveありとdirectory-onlyの両方を検証する。既存ツール以外のオンライン導入はない。

自作ELFやCore/TA/SDKをtarへ含める必要はない。移送先のsource/toolから新規生成する。全実機実行はNOT_RUN、B2_RUNTIME_READY=0、PLATFORM_PROTECTION_CONFIRMED=0、安全ゲートBLOCKEDを維持する。
