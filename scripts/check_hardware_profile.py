import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLAGS = ("hardware_map_verified", "master_route_verified", "secure_el3_verified", "configuration_unlocked", "other_masters_quiesced", "gic_routing_verified")


def profile_issues(profile, mapping):
    issues = [f"{key} is not verified" for key in FLAGS if profile.get(key) is not True]
    required = ("protected_start", "protected_end", "apu_master_id", "dma_master_id", "dma_channel", "peripheral_base", "xppu_aperture")
    issues.extend(f"{key} is unresolved" for key in required if type(profile.get(key)) is not int)
    if mapping.get("confirmed") is not True:
        issues.append("hardware memory map is provisional")
    protected = mapping["protection_domains"][0]
    if protected["base"] % 0x100000 or protected["end"] % 0x100000:
        issues.append("DDR XMPU requires 1 MiB aligned protection bounds; protection window does not meet this")
    if type(profile.get("protected_start")) is int and type(profile.get("protected_end")) is int:
        if (profile["protected_start"], profile["protected_end"]) != (protected["base"], protected["end"]):
            issues.append("profile protection bounds differ from memory map; do not round silently")
    banks = profile.get("xmpu_bank_bases", [])
    valid_banks = {0xfd000000 + index * 0x10000 for index in range(6)}
    if not banks or any(type(bank) is not int or bank not in valid_banks for bank in banks):
        issues.append("DDR XMPU bank routing is unresolved or invalid")
    for key in ("apu_master_id", "dma_master_id"):
        value = profile.get(key)
        if type(value) is int and not 0 <= value <= 0x3ff:
            issues.append(key + " exceeds the 10-bit Master ID field")
    aperture, peripheral = profile.get("xppu_aperture"), profile.get("peripheral_base")
    if type(aperture) is int and type(peripheral) is int:
        if not 0 <= aperture < 256 or peripheral != 0xff000000 + aperture * 0x10000:
            issues.append("64 KiB XPPU aperture does not match peripheral address")
    if len(profile.get("xppu_master_profiles", [])) != 20:
        issues.append("all 20 XPPU master profiles require verified snapshots")
    if mapping.get("external_reservations_complete") is not True:
        issues.append("FSBL/PMUFW/BSP reservation reconciliation is incomplete")
    # フラグ変更だけで実行可能と判定しない。
    issues.append("DMA/GIC backends require board integration and exception/violation attribution")
    return issues


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", type=Path, default=ROOT / "platform/zcu106/board_profile.json")
    args = parser.parse_args()
    issues = profile_issues(json.loads(args.profile.read_text()), json.loads((ROOT / "platform/zcu106/memory_map.json").read_text()))
    if issues:
        print("HARDWARE_PROFILE BLOCKED\n" + "\n".join(issues))
        return 2
    print("HARDWARE_PROFILE READY")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
