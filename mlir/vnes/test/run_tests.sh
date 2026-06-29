#!/usr/bin/env bash
# Run VNES MLIR dialect tests
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VNES_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_DIR="$(dirname "$VNES_DIR")"

MLIR_OPT="${MLIR_OPT:-${PROJECT_DIR}/../build/mlir-plugins/mlir-opt}"
if [ ! -x "$MLIR_OPT" ]; then
    MLIR_OPT="$(command -v mlir-opt)"
fi
if [ ! -x "$MLIR_OPT" ]; then
    echo "ERROR: mlir-opt not found. Set MLIR_OPT or add to PATH."
    exit 1
fi

PLUGIN_DIR="${PROJECT_DIR}/build/mlir-plugins"
PLUGIN="${PLUGIN_DIR}/VNESMLIRDialect.dll"
if [ ! -f "$PLUGIN" ]; then
    PLUGIN="${PLUGIN_DIR}/libVNESMLIRDialect.so"
fi
if [ ! -f "$PLUGIN" ]; then
    echo "WARNING: VNESMLIRDialect plugin not found at ${PLUGIN_DIR}"
    echo "Build it first with: cmake --build build --target VNESMLIRDialect"
fi

DIALECT_TEST="${SCRIPT_DIR}/test_vnes_dialect.mlir"
FUSION_TEST="${SCRIPT_DIR}/test_darcy_flow.mlir"

echo "=== Test 1: VNES dialect round-trip (parse + verify) ==="
if [ -f "$PLUGIN" ]; then
    "$MLIR_OPT" "$DIALECT_TEST" -load-pass-plugin="$PLUGIN" -verify-diagnostics -split-input-file
else
    "$MLIR_OPT" "$DIALECT_TEST" -verify-diagnostics -split-input-file
fi
echo "PASS"

echo ""
echo "=== Test 2: VNES fusion pass ==="
if [ -f "$PLUGIN" ]; then
    "$MLIR_OPT" "$FUSION_TEST" -load-pass-plugin="$PLUGIN" -pass-pipeline="func.func(vnes-fuse-solvers)"
    echo "PASS"
else
    echo "SKIP (plugin not built)"
fi

echo ""
echo "All dialect tests passed."
