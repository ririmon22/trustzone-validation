import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def build_plan(component, prefix, output):
    if component == "tfa":
        return ["make", "-C", str(ROOT / "third_party/tfa-amd"), "PLAT=qemu", "ARCH=aarch64", f"CROSS_COMPILE={prefix}", f"BUILD_BASE={output}", "all"]
    if component in ("optee", "optee-sdk"):
        command = ["make", "-C", str(ROOT / "third_party/optee"), "PLATFORM=vexpress-qemu_armv8a", "CFG_ARM64_core=y", "CFG_USER_TA_TARGETS=ta_arm64", f"CROSS_COMPILE64={prefix}", f"CROSS_COMPILE_ta_arm64={prefix}", f"O={output}"]
        if component == "optee-sdk":
            command += ["CFG_EARLY_TA=y", "ta_dev_kit"]
        return command
    sdk = ROOT / "build/external/optee/export-ta_arm64"
    return ["make", "-C", str(ROOT / "optee_standalone/ta"), f"TA_DEV_KIT_DIR={sdk}", f"CROSS_COMPILE={prefix}", f"O={output}", f"TA_VARIANT={component[-1]}"]


def preflight(component, prefix, compiler="gcc"):
    issues = []
    tools = ("make", "git")
    if compiler == "clang":
        tools += ("clang", "ld.lld")
    else:
        tools += (prefix + "gcc", prefix + "ld", prefix + "objcopy")
    for tool in tools:
        if not shutil.which(tool):
            issues.append("Required tool is unavailable: " + tool)
    key = "tfa-amd" if component == "tfa" else "optee"
    source = ROOT / "third_party" / key
    if not (source / "Makefile").is_file():
        issues.append("Pinned source is unavailable: " + str(source))
    else:
        from source_snapshot import verify_source
        try:
            entry = json.loads((ROOT / "third_party/dependencies.lock.json").read_text())[key]
            verify_source(source, entry)
            if compiler == "clang" and component != "tfa":
                lock = json.loads((ROOT / "third_party/dependencies.lock.json").read_text())
                for dependency in ("compiler-rt", "pyelftools"):
                    verify_source(ROOT / "third_party" / dependency, lock[dependency])
        except (ValueError, OSError, subprocess.CalledProcessError) as error:
            issues.append("Source provenance failed: " + str(error))
    if component.startswith("ta-") and not (ROOT / "build/external/optee/export-ta_arm64/mk/ta_dev_kit.mk").is_file():
        issues.append("Generated OP-TEE ARM64 TA SDK is unavailable")
    return issues


def configured_plan(args):
    output_name = args.component
    if args.component == "tfa":
        output_name = ("tfa-zynqmp-b2" if args.configuration == "b2" else "tfa-zynqmp") if args.platform == "zynqmp" else "tfa-qemu-" + args.configuration
    elif args.component in ("optee", "optee-sdk"):
        output_name = "optee-zynqmp" if args.platform == "zynqmp" else "optee"
    output = ROOT / "build/external" / output_name
    command = build_plan(args.component, args.cross_compile, output)
    if args.component in ("optee", "optee-sdk") and args.platform == "zynqmp":
        command = ["PLATFORM=zynqmp-zcu106" if item.startswith("PLATFORM=") else item for item in command]
    if args.component.startswith("ta-") and getattr(args, "ta_sdk", None):
        command = ["TA_DEV_KIT_DIR=" + str(args.ta_sdk.resolve()) if item.startswith("TA_DEV_KIT_DIR=") else item for item in command]
    if args.component == "tfa":
        command = ["make", "-C", str(ROOT / "third_party/tfa-amd"), "PLAT=" + args.platform,
                   f"BUILD_BASE={output}", "BUILD_STRING=dd3512287a17eea866d5c82fa7b86ad58de59163"]
        if args.platform == "zynqmp":
            command += ["RESET_TO_BL31=1", "bl31"]
            if args.configuration == "b2":
                from zcu106_b2_layout import load_layout
                proposal, regions = load_layout(ROOT)
                bl33_entry = regions['bl33_code']['base']
                # JTAG用の既定入口を自作BL33の配置定義と合わせる。実機の引渡しは別途確認する。
                command += ["SPD=opteed", "PRELOADED_BL33_BASE=" + hex(bl33_entry)]
                for prefix, region in (('ZYNQMP_ATF_MEM', regions['bl31_reserved']), ('ZYNQMP_BL32_MEM', regions['bl32_reserved'])):
                    command += [prefix + '_BASE=' + hex(region['base']), prefix + '_SIZE=' + hex(region['size'])]
        else:
            bl33 = ROOT / "build/qemu/external" / ("tfa_bl33.bin" if args.configuration == "b1" else "optee_bl33.bin")
            command += ["BL33=" + str(bl33), "TRANSFER_LIST=0", "bl1", "bl2", "bl31", "fip"]
            if args.configuration == "b2":
                core = ROOT / "build/external/optee/core"
                command += ["SPD=opteed", "BL32_RAM_LOCATION=tdram", "BL32=" + str(core / "tee-header_v2.bin"),
                            "BL32_EXTRA1=" + str(core / "tee-pager_v2.bin"), "BL32_EXTRA2=" + str(core / "tee-pageable_v2.bin")]
    elif args.component in ("optee", "optee-sdk"):
        command += (["CFG_TEE_CORE_NB_CORE=1"] if args.platform == "qemu" else []) + ["CFG_DT_DRIVER_EMBEDDED_TEST=n", "CFG_CORE_ASYNC_NOTIF=n",
                    "CFG_REE_FS=n", "CFG_RPMB_FS=n", "CFG_REE_FS_TA=n", "CFG_PKCS11_TA=n",
                    "CFG_BUILD_IN_TREE_TA=n", "CFG_EARLY_TA=y", "CFG_WITH_SOFTWARE_PRNG=y"]
        if args.platform == 'zynqmp':
            from zcu106_b2_layout import load_layout
            proposal, regions = load_layout(ROOT)
            for prefix, region in (('CFG_TZDRAM', regions['bl32_reserved']), ('CFG_SHMEM', regions['shared_memory'])):
                command += [prefix + '_START=' + hex(region['base']), prefix + '_SIZE=' + hex(region['size'])]
            command += ['CFG_DDR_SIZE=' + hex(proposal['reported_ddr0']['end'] - proposal['reported_ddr0']['base'])]
    if args.compiler == "clang":
        llvm = args.llvm_dir.resolve()
        if args.component == "tfa":
            command += ["CC=" + str(llvm / "clang"), "LD=" + str(llvm / "ld.lld"), "OC=" + str(llvm / "llvm-objcopy"),
                        "OD=" + str(llvm / "llvm-objdump"), "AR=" + str(llvm / "llvm-ar")]
        else:
            runtime = ROOT / "build/toolchain-runtime/libbuiltins.a"
            command += ["COMPILER=clang", "OPTEE_CLANG_COMPILER_PATH=" + str(llvm) + "/",
                        "libgcccore=" + str(runtime), "libgccldelf=" + str(runtime), "libgccta_arm64=" + str(runtime)]
    elif args.component == "tfa":
        command.append("CROSS_COMPILE=" + args.cross_compile)
    if args.component != "tfa":
        command.append("PYTHON3=" + sys.executable)
    if args.component == "tfa" and args.platform == "qemu" and getattr(args, "host_cc", None):
        command.append("host-cc=" + args.host_cc)
    if args.early_ta:
        command.append("EARLY_TA_PATHS=" + " ".join(str(path.resolve()) for path in args.early_ta))
    command.append("-j" + str(args.jobs))
    return output, command


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("component", choices=("tfa", "optee", "optee-sdk", "ta-a", "ta-b"))
    parser.add_argument("--cross-compile", default="aarch64-linux-gnu-")
    parser.add_argument("--compiler", choices=("clang", "gcc"), default="clang")
    parser.add_argument("--ta-sdk", type=Path)
    parser.add_argument("--host-cc", default=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang"))
    parser.add_argument("--llvm-dir", type=Path, default=Path(shutil.which("clang") or "/usr/bin/clang").resolve().parent)
    parser.add_argument("--platform", choices=("qemu", "zynqmp"), default="qemu")
    parser.add_argument("--configuration", choices=("b1", "b2"), default="b1")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--validation-pta", action="store_true")
    parser.add_argument("--early-ta", type=Path, action="append", default=[])
    parser.add_argument("--log-dir", type=Path, default=ROOT / "results/phase4")
    args = parser.parse_args()
    if args.validation_pta and args.component != "optee":
        parser.error("validation PTA is an OP-TEE Core option")
    if args.jobs < 1 or (args.early_ta and args.component != "optee"):
        parser.error("invalid job count or Early TA component")
    output, command = configured_plan(args)
    args.log_dir.mkdir(parents=True, exist_ok=True)
    suffix = "-" + args.platform + ("-" + args.configuration if args.platform == "qemu" else "") if args.component == "tfa" else ""
    if args.component in ("optee", "optee-sdk") and args.platform == "zynqmp":
        suffix = "-zynqmp"
    if args.component == "tfa" and args.platform == "zynqmp" and args.configuration == "b2":
        suffix += "-b2"
    log_path = args.log_dir / ("external-" + args.component + suffix + ".log")
    issues = preflight(args.component, args.cross_compile, args.compiler)
    if args.component.startswith("ta-") and args.ta_sdk:
        issues = [item for item in issues if item != "Generated OP-TEE ARM64 TA SDK is unavailable"]
        if not (args.ta_sdk / "mk/ta_dev_kit.mk").is_file():
            issues.append("Selected ARM64 TA SDK is unavailable: " + str(args.ta_sdk))
    if args.component == "tfa" and args.platform == "qemu" and (not args.host_cc or not shutil.which(args.host_cc)):
        issues.append("QEMU FIP requires a native C compiler and OpenSSL development library")
    if args.compiler == "clang":
        for name in ("clang", "ld.lld", "llvm-ar", "llvm-objcopy", "llvm-objdump", "llvm-readelf", "llvm-nm"):
            if not (args.llvm_dir / name).is_file():
                issues.append("LLVM tool is missing: " + str(args.llvm_dir / name))
        if args.component != "tfa" and not (ROOT / "build/toolchain-runtime/libbuiltins.a").is_file():
            issues.append("Build the verified runtime with make llvm-runtime")
    issues.extend("Early TA ELF is missing: " + str(path) for path in args.early_ta if not path.is_file())
    if issues:
        log_path.write_text("BUILD BLOCKED (preflight; compiler not invoked)\n" + "\n".join(issues) + "\nCommand: " + repr(command) + "\n")
        print(log_path.read_text(), end="")
        return 2
    import os
    env = os.environ.copy()
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    env["PYTHONPATH"] = str(ROOT / "third_party/pyelftools") + os.pathsep + env.get("PYTHONPATH", "")
    if args.component == "optee":
        check = subprocess.run([sys.executable, "-c", "import elftools"], env=env, capture_output=True)
        if check.returncode:
            log_path.write_text("BUILD BLOCKED: pyelftools unavailable\n")
            return 2
    if args.validation_pta:
        from stage_optee_overlay import stage
        source, overlay = stage(ROOT)
        command[2] = str(source)
        command.append("CFG_TZ_VALIDATION_PTA=y")
        (args.log_dir / "optee-validation-overlay.json").write_text(json.dumps(overlay, indent=4) + "\n")
    output.mkdir(parents=True, exist_ok=True)
    with log_path.open("w") as log:
        log.write("COMMAND: " + repr(command) + "\n")
        log.flush()
        completed = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        if completed.returncode == 0 and args.component == "optee-sdk" and not (output / "export-ta_arm64/mk/ta_dev_kit.mk").is_file():
            log.write("BUILD FAIL: SDK artifact missing\n")
            return 1
        log.write(f"BUILD {'PASS' if completed.returncode == 0 else 'FAIL'} exit={completed.returncode}\n")
    print(log_path)
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
