# 構成別の最小依存

Ubuntu 24.04＋Vitis 2025.1は未使用。実測hostはUbuntu26.04、Clang/LLD21.1.8、Python3.14.4。Python3.12.14でも完全ベアメタルを標準ライブラリだけで新規buildした。Ubuntu標準配布packageと「初期状態で必ず入っているもの」は異なる。存在は `check_build_environment.py` で確認し、Vitis同梱toolを推測しない。

## 構成別

|対象|buildに必要なもの|不要／別用途|実際に要求するcommand|
|---|---|---|---|
|完全ベアメタル3 ELF|bash/coreutils、GNU Make、Python>=3.9 stdlib、AArch64 C/ASM/link toolchain|Python外部package、BSP、TF-A、OP-TEE、host C compiler、OpenSSL、QEMUはELF生成には不要|`make PLATFORM=zcu106 baremetal`、check_memory_map.py、cross gcc driverまたはClang+LLD|
|TF-A＋Standalone実機向け|上記、固定AMD TF-A、Git（版確認）、target objcopy/objdump等|BL31単体にはhost fiptool/OpenSSL開発file不要|`build_external.py tfa --platform zynqmp` の `PLAT=zynqmp RESET_TO_BL31=1 bl31`、standalone-tfa|
|OP-TEE Core/BL32＋Standalone|上記target tool family、固定OP-TEE、Python cryptography、固定pyelftools source|Linux/libteec/tee-supplicant不要。Clangの場合は実compiler-rt除算sourceも必要|`build_external.py optee`、gen_tee_bin.py、gen_ldelf_hex.py、gen_ta_pub_key.py、standalone-optee|
|Early TAとTA SDK|固定OP-TEEからSDKを生成、target tool family、署名用cryptography|既存SDKを持ち込まない。SDK archive生成そのものと署名時の依存を区別する|`optee-sdk` の `ta_dev_kit` → `ta-a/ta-b --ta-sdk <生成先>` → `optee --early-ta <生成ELF>`|
|性能fw|埋め込む構成A/B2の依存を継承|統計解析自体はPython stdlibのみ|benchmark_run/scoped、analyze_benchmarks.py、statistics/csv/json|
|QEMU再実行|qemu-system-aarch64、Python stdlib、target objcopy、Make/既存toolchain|QEMUをsource buildするためのGUI/USB開発package等は不要|run_qemu.sh、prepare_qemu_firmware.py、capture/parse/analyze|
|QEMU TF-A FIP|native C compiler、host libc開発file、OpenSSL programとheader/libcrypto|実機BL31単体と混同しない|TF-A公式tools/fiptoolで `host-cc`、`/usr/bin/openssl version`、`-lcrypto`|
|任意のhost unittest|native Clang（現fixtureの指定）、Python stdlib|firmware deployの必須依存ではない。cryptographyなしでもimport可能に修正|`python3 -S -m unittest discover -s tests`|

TA SDK生成だけなら署名はまだ実行しない場合があるが、本helperのearly-ta構成はSDK→署名TAまで完結するためcryptographyを必須にする。Clang helperは実runtimeを生成して渡す。GCCでは検出したtoolchainのlibgccを使い、LLVM/compiler-rt sourceを要求しない。空のruntimeや暗号stubでリンクを通さない。

## Ubuntu配布／Vitis候補／外部追加の区別

|分類・package候補|必要な構成・purpose|検出と注意|
|---|---|---|
|Ubuntu基本：bash、coreutils、grep、sed、gawkまたはmawk、findutils|Makeのshell、echo/mkdir/cp/rm/ln等、file/文字列処理|限定PATHの実測でexternal echoも必要だった。新たな開発packageではない|
|Ubuntu配布：make|全source build|Vitis内のmakeを使うなら実path/versionを確認|
|Ubuntu配布：python3|自作検査と公式gen script|24.04の3.12系を候補とし、選択した実Pythonで確認。pyenv/pipは必須ではない|
|Ubuntu配布：git|取得（build前）とfixed commit/clean tree検査|build中のGit remote取得は禁止。source stagingはlocal file transportのみ|
|Vitis候補：AArch64 GNU toolchain一式|C、ASM、link、archive、objcopy/dump/nm/readelf|名前・path・版を実検出しELF64生成probeを行う。付属済みとは記載しない|
|不足時の外部候補：gcc-aarch64-linux-gnu、binutils-aarch64-linux-gnu|Vitisのtarget toolが利用できない場合の代替候補|会社の許可された配布経路で事前準備。build scriptはinstallしない。今回GNU routeは未実行|
|別route：clang/lld/llvmの対応版|今回検証したClang route|/usr/lib/llvm-21固定依存をMakeから除去。別版の互換性は実buildまで未確認|
|不足時：python3-cryptography|公式TA公開鍵生成・TA署名・host署名検査|既存Pythonでimportできれば追加不要。選択Pythonとshebangの実行先を統一。独自暗号に置き換えない|
|Git source：pyelftools v0.32|Core ELF解析|PYTHONPATHで固定sourceを使用。pip、python3-pyelftoolsのAPT追加は不要|
|Clang限定Git source：compiler-rt|AArch64 __udivti3/__udivmodti4|実C sourceをbuild。Vitis GNU/libgcc routeでは不要|
|QEMU FIP限定：native gcc/clang、libc6-dev、libssl-dev、openssl|fiptool native build、OpenSSL API選択とlibcrypto link|native compilerで実header/link probe。B1/B2実機BL31のみなら省略可能|
|QEMU検証限定：qemu-system-arm（qemu-system-aarch64）|TCG virt実行|省略時はQEMU NOT_RUN。Vitis付属QEMUの有無/secure virt対応は未確認|
|Vitis GUI側：CMake/Ninja/Bootgen/hw_server/XSDB等|Platform/BSP/boot image/debugの各工程|検出・実IDE確認が必要。raw Makeの自作ELF生成にはCMake/Ninjaは不要|

`cmp`（diffutils）はOP-TEEの生成config比較に使用する。従来TF-Aの`all`は未使用qemu_fw.bios/rom生成で`dd`を要求したため、今は公式 `bl1 bl2 bl31 fip` targetsのみを呼ぶ。使用するfirmware/FIPの生成を省略したものではない。DTCは今回の `CFG_DT_DRIVER_EMBEDDED_TEST=n` 等の選択構成では呼んでいない。全platform/全featureで不要という意味ではない。

## Python依存の整理

自作の配置・ELF解析、hash、Git検査、log解析、統計、report、stagingは標準ライブラリのみ。署名host検査のcryptography importを実行時に移し、無関係なhost unittestやmutation fixtureへの依存波及をなくした。公式OP-TEEの暗号/ELF処理は変更していない。

[OP-TEE公式prerequisites](https://optee.readthedocs.io/en/4.5.0/building/prerequisites.html) の全システム/QEMU/toolchain開発用package一覧を、そのまま本構成の必須一覧にはしない。本表は固定source、限定targets、今回のlogに基づく。Ubuntu24/Vitis実機環境での成功確認とは区別する。

QEMU Aだけはcompiler driverでbootstrap binaryを生成し、stdlib ELF packerを使うためtarget objcopy/ar/nm/readelfを必須にしない。表のtarget objcopy追加はB1/B2のBL33 binary変換に対応する。全tool familyを確認するB1/B2 helperは、公式buildの実commandに使用されるtoolと、設定検査用のtool availabilityをまとめて事前確認する。
