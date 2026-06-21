#!/usr/bin/env python3
"""Read pillars.yaml and emit synchronized C++/Python pillar definitions.

Usage:
    python scripts/generate_pillar_sources.py

Outputs:
    generated/PillarEnum.h         - Unified C++ enum (Awareness = 0, NumPillars = 16)
    generated/pillar_constants.py  - Python PILLAR_NAMES list + PillarIndex IntEnum
"""

import os
import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    print("yaml not found. Install with: pip install pyyaml")
    sys.exit(1)

REPO_ROOT = Path(__file__).resolve().parent.parent
YAML_PATH = REPO_ROOT / "pillars.yaml"
GEN_DIR   = REPO_ROOT / "generated"

GEN_HEADER = GEN_DIR / "PillarEnum.h"
GEN_PYTHON = GEN_DIR / "pillar_constants.py"
GEN_CONSTRAINTS = GEN_DIR / "PillarConstraints.h"

# Also output to sibling Van_Nueman_AI repo
AI_REPO_ROOT = REPO_ROOT.parent / "Van_Nueman_AI"

HEADER_PREAMBLE = """// Auto-generated from pillars.yaml -- DO NOT EDIT
// Regenerate with: python scripts/generate_pillar_sources.py
#pragma once
#include <cstdint>

enum PillarIndex : uint32_t {
"""

HEADER_EPILOGUE = """
    NumPillars = 16
};

"""


def generate_unified_header(pillars):
    """Generate a single unified PillarIndex enum with lowercase naming and NumPillars."""
    lines = [HEADER_PREAMBLE]
    for p in pillars:
        name = p["name"].capitalize() if p["name"] != "force" else "Force"
        lines.append(f"    {name} = {p['index']},\n")
    lines.append(HEADER_EPILOGUE)
    text = "".join(lines)
    GEN_DIR.mkdir(parents=True, exist_ok=True)
    GEN_HEADER.write_text(text)
    print(f"  -> {GEN_HEADER}")

    # Also copy into the toolchain include dir so UGC kernel compilation can find it
    toolchain_dir = REPO_ROOT / "Van_Nueman_Toolchain" / "llvm-toolchain" / "include" / "vn"
    toolchain_dir.mkdir(parents=True, exist_ok=True)
    dest = toolchain_dir / "PillarEnumUGC.h"
    dest.write_text(text)
    print(f"  -> {dest} (copy for toolchain)")


def load_pillars():
    with open(YAML_PATH) as f:
        data = yaml.safe_load(f)
    return data["pillars"]


def generate_python(pillars):
    lines = [
        "# Auto-generated from pillars.yaml -- DO NOT EDIT\n",
        "# Regenerate with: python scripts/generate_pillar_sources.py\n",
        "from enum import IntEnum\n\n",
        f"NUM_PILLARS = {len(pillars)}\n\n",
        "PILLAR_NAMES = [\n",
    ]
    nl = "\n"
    for p in pillars:
        lines.append(f'    "{p["name"]}",{nl}')
    lines.append("]\n\n")

    lines.append("class PillarIndex(IntEnum):\n")
    for p in pillars:
        name = p["name"].capitalize() if p["name"] != "force" else "Force"
        lines.append(f'    {name} = {p["index"]}\n')
    lines.append(f'    NumPillars = {len(pillars)}\n')

    lines.append("\n\n# Mapping from pillar name to index\n")
    lines.append("PILLAR_NAME_TO_INDEX: dict[str, int] = {\n")
    for p in pillars:
        name = p["name"].capitalize() if p["name"] != "force" else "Force"
        lines.append(f'    "{p["name"]}": PillarIndex.{name},\n')
    lines.append("}\n")

    GEN_DIR.mkdir(parents=True, exist_ok=True)
    GEN_PYTHON.write_text("".join(lines))
    print(f"  -> {GEN_PYTHON}")

    # Also copy to sibling Van_Nueman_AI repo for Python-side pillar constants
    ai_gen_dir = AI_REPO_ROOT / "generated"
    ai_gen_dir.mkdir(parents=True, exist_ok=True)
    ai_dest = ai_gen_dir / "pillar_constants.py"
    ai_dest.write_text("".join(lines))
    print(f"  -> {ai_dest} (copy for AI repo)")
    _ensure_init_py(ai_gen_dir)


def _ensure_init_py(directory: Path):
    init = directory / "__init__.py"
    if not init.exists():
        init.write_text("")
        print(f"  -> {init} (created)")


def generate_constraints(pillars):
    """Generate PillarConstraints.h with PID_CONSTRAINTS from pillars.yaml."""
    enum_names = {}
    for p in pillars:
        name = p["name"].capitalize() if p["name"] != "force" else "Force"
        enum_names[p["name"]] = name

    lines = [
        "// Auto-generated from pillars.yaml -- DO NOT EDIT\n",
        "// Regenerate with: python scripts/generate_pillar_sources.py\n",
        "#pragma once\n",
        '#include "PillarEnum.h"\n',
        "\n",
        "// PID constraint: min/max Bloch values and allowed rotation targets per pillar\n",
        "struct PIDConstraint {\n",
        "    const char* name;\n",
        "    float min_val;\n",
        "    float max_val;\n",
        "    int allowed_targets[16];\n",
        "};\n",
        "\n",
        "static constexpr int NO_TARGET = -1;\n",
        "\n",
        "static constexpr PIDConstraint PID_CONSTRAINTS[16] = {\n",
    ]

    for p in pillars:
        c = p.get("constraints", {})
        min_val = c.get("min_val", 0.0)
        max_val = c.get("max_val", 1.0)
        targets = c.get("allowed_targets", [])
        target_list = ", ".join(enum_names[t] for t in targets)
        if targets:
            target_list += ", NO_TARGET"
        else:
            target_list = "NO_TARGET"
        name = p["name"].capitalize() if p["name"] != "force" else "Force"
        lines.append(f'    {{"{p["name"]}", {min_val}f, {max_val}f, {{{target_list}}}}}' + (
            "," if p["index"] < len(pillars) - 1 else ""
        ) + "\n")

    lines.append("};\n\n")

    # Also emit target validation helper
    lines.extend([
        "static inline bool is_valid_pillar_target(int operator_pillar, int target_pillar) {\n",
        "    if (operator_pillar < 0 || operator_pillar >= 16) return false;\n",
        "    if (target_pillar < 0 || target_pillar >= 16) return false;\n",
        "    for (int i = 0; i < 16; i++) {\n",
        "        int t = PID_CONSTRAINTS[operator_pillar].allowed_targets[i];\n",
        "        if (t == NO_TARGET) break;\n",
        "        if (t == target_pillar) return true;\n",
        "    }\n",
        "    return false;\n",
        "}\n",
    ])

    GEN_DIR.mkdir(parents=True, exist_ok=True)
    GEN_CONSTRAINTS.write_text("".join(lines))
    print(f"  -> {GEN_CONSTRAINTS}")

    # Also copy to toolchain include dir
    toolchain_dir = REPO_ROOT / "Van_Nueman_Toolchain" / "llvm-toolchain" / "include" / "vn"
    toolchain_dir.mkdir(parents=True, exist_ok=True)
    dest = toolchain_dir / "PillarConstraintsUGC.h"
    dest.write_text("".join(lines))
    print(f"  -> {dest} (copy for toolchain)")


def main():
    print(f"Loading pillars from {YAML_PATH}")
    pillars = load_pillars()
    print(f"Found {len(pillars)} pillars")

    generate_unified_header(pillars)
    generate_python(pillars)
    generate_constraints(pillars)
    print("Done.")


if __name__ == "__main__":
    main()
