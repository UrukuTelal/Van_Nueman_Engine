#!/usr/bin/env python3
"""Validate a SPIR-V binary file for structural conformance.

Checks:
  - Magic number (0x07230203)
  - Version word does not exceed requested SPIR-V version (if --spv-version given)
  - Minimum length (header is 5 words)
  - Generator magic word is not LLVM bitcode (0xDEC04342)

Usage:
  python validate_spirv.py <file.spv> [--spv-version MAJOR.MINOR]

Returns exit code 0 if valid, 1 otherwise.
"""

import argparse
import os
import struct
import sys

SPIRV_MAGIC = 0x07230203

# SPIR-V version encoding (word 1 of header) — ordered list for ≤ comparison
SPIRV_VERSIONS = [
    (1, 0),
    (1, 1),
    (1, 2),
    (1, 3),
    (1, 4),
    (1, 5),
    (1, 6),
]

SPIRV_VERSION_MAP = {(maj, min_): (maj << 16) | (min_ << 8) for maj, min_ in SPIRV_VERSIONS}

LLVM_BITCODE_MAGIC = 0xDEC04342  # BC_C0DE


def _version_key(ver: tuple[int, int]) -> int:
    return (ver[0] << 16) | ver[1]


def validate_spirv(path: str, max_version: tuple[int, int] | None) -> list[str]:
    errors: list[str] = []
    if not os.path.isfile(path):
        return [f"File not found: {path}"]

    data = open(path, "rb").read()
    if len(data) < 20:
        return [f"File too small ({len(data)} bytes) — need at least 20 bytes for SPIR-V header"]

    if len(data) % 4 != 0:
        errors.append(f"File size {len(data)} is not a multiple of 4 (not valid SPIR-V)")

    words = struct.unpack(f"<{len(data) // 4}I", data[:len(data) & ~3])

    if words[0] != SPIRV_MAGIC:
        errors.append(f"Magic number mismatch: got 0x{words[0]:08X}, expected 0x{SPIRV_MAGIC:08X} (SPIR-V)")
        if words[0] == LLVM_BITCODE_MAGIC:
            errors.append("  -> File appears to be LLVM bitcode, not SPIR-V")
        return errors

    actual_ver = ((words[1] >> 16) & 0xFF, (words[1] >> 8) & 0xFF)
    if max_version and _version_key(actual_ver) > _version_key(max_version):
        errors.append(
            f"Version {actual_ver[0]}.{actual_ver[1]} exceeds max {max_version[0]}.{max_version[1]}"
        )

    bound = words[3]
    if bound == 0:
        errors.append("Bound (word 3) is zero — no instructions in module")
    if bound < 5:
        errors.append(f"Bound ({bound}) is too small — must be ≥ 5 for valid header")

    if len(words) < bound + 5:
        errors.append(
            f"Declared bound ({bound}) exceeds actual word count ({len(words)})"
        )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate SPIR-V binary conformance")
    parser.add_argument("file", help="Path to .spv file")
    parser.add_argument("--spv-version", help="Max SPIR-V version (e.g. 1.6)")
    args = parser.parse_args()

    max_ver = None
    if args.spv_version:
        parts = args.spv_version.split(".")
        max_ver = (int(parts[0]), int(parts[1]) if len(parts) > 1 else 0)

    errors = validate_spirv(args.file, max_ver)
    if errors:
        print(f"SPIR-V validation FAILED for {args.file}:")
        for e in errors:
            print(f"  - {e}")
        return 1
    print(f"SPIR-V validation PASSED for {args.file}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
