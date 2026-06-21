#!/usr/bin/env python3
"""
Static analysis check for vncc/ugc SPIR-V kernel compatibility.

Verifies that kernel .cpp files destined for SPIR-V compilation:
  1. Do NOT call standard library math functions (must use builtin wrappers like acosf_, cosf_, etc.)
  2. Do NOT use heap allocation (new, delete, malloc, free, calloc, realloc)
  3. Do NOT use RTTI (dynamic_cast, typeid)
  4. Do NOT use exceptions (throw, try, catch)
  5. Do NOT include <iostream>, <fstream>, <sstream>, <string>, <vector>, <map>, <set>
     (these pull in heap/RTTI/exceptions on most implementations)

Returns exit code 0 if all checks pass, 1 otherwise.
"""

import os
import re
import sys

KERNEL_DIR = os.path.join(os.path.dirname(__file__), "..", "kernels")
KERNEL_PATTERNS = ["*.cpp"]

FORBIDDEN_STDLIB_MATH = re.compile(
    r'\b(?:'
    r'sqrtf|sqrt|'
    r'sinf|sin|'
    r'cosf|cos|'
    r'tanf|tan|'
    r'acosf|acos|'
    r'asinf|asin|'
    r'atanf|atan|atan2f|atan2|'
    r'coshf|cosh|'
    r'sinhf|sinh|'
    r'tanhf|tanh|'
    r'expf|exp|'
    r'logf|log|log10f|log10|'
    r'powf|pow|'
    r'floorf|floor|'
    r'ceilf|ceil|'
    r'roundf|round|'
    r'fabsf|fabs|'
    r'fmaxf|fmax|'
    r'fminf|fmin|'
    r'fmodf|fmod'
    r')\s*\('
)

ALLOWED_MATH = re.compile(
    r'\b(?:'
    r'sqrtf_|sinf_|cosf_|tanf_|'
    r'acosf_|asinf_|atanf_|atan2f_|'
    r'coshf_|sinhf_|tanhf_|'
    r'expf_|logf_|log10f_|'
    r'powf_|floorf_|ceilf_|roundf_|'
    r'fabsf_|fmaxf_|fminf_|fmodf_'
    r')\s*\('
)

FORBIDDEN_HEAP = re.compile(
    r'\b(?:new|delete|malloc|free|calloc|realloc)\s*\('
)

FORBIDDEN_RTTI = re.compile(
    r'\bdynamic_cast\s*<|'
    r'\btypeid\s*\('
)

FORBIDDEN_EXCEPTIONS = re.compile(
    r'\bthrow\s|'
    r'\btry\s*\{|'
    r'\bcatch\s*\('
)

FORBIDDEN_INCLUDES = re.compile(
    r'#include\s*[<"]'
    r'(?:iostream|fstream|sstream|string|vector|map|set|unordered_map|unordered_set|'
    r'memory|future|thread|mutex|condition_variable|'
    r'regex|chrono|random|filesystem)'
    r'[>"]'
)

BUILTIN_WRAPPER_PATTERN = re.compile(
    r'__builtin_elementwise_(?:sin|cos|acos|sqrt|abs|log|max|min)\s*\('
)


def is_comment_or_string(line):
    stripped = line.strip()
    return stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*")


def check_file(filepath):
    errors = []
    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        lines = f.readlines()

    for lineno, line in enumerate(lines, 1):
        if is_comment_or_string(line):
            continue

        # Check for stdlib math calls (not in comments)
        for match in FORBIDDEN_STDLIB_MATH.finditer(line):
            matched = match.group()
            # Verify this is actually a function call, not a variable name
            errors.append(f"  {filepath}:{lineno}: stdlib math call '{matched[:-1]}()' - must use builtin wrapper (e.g. '{matched[:-1]}_()')")

        # Check for heap allocation
        for match in FORBIDDEN_HEAP.finditer(line):
            matched = match.group()
            # 'delete' is a keyword, not just a function call; check more carefully
            if matched.startswith("delete"):
                if "delete[]" in line or re.search(r'\bdelete\s+\w', line):
                    errors.append(f"  {filepath}:{lineno}: heap deallocation via 'delete' is forbidden in SPIR-V kernels")
            else:
                errors.append(f"  {filepath}:{lineno}: heap allocation '{matched}' is forbidden in SPIR-V kernels")

        # Check for RTTI
        for match in FORBIDDEN_RTTI.finditer(line):
            matched = match.group()
            errors.append(f"  {filepath}:{lineno}: RTTI '{matched}' is forbidden in SPIR-V kernels")

        # Check for exceptions
        for match in FORBIDDEN_EXCEPTIONS.finditer(line):
            matched = match.group().strip()
            errors.append(f"  {filepath}:{lineno}: exception handling '{matched}' is forbidden in SPIR-V kernels")

        # Check for forbidden includes
        for match in FORBIDDEN_INCLUDES.finditer(line):
            matched = match.group()
            errors.append(f"  {filepath}:{lineno}: forbidden include {matched.strip()} (pulls in runtime deps)")

    return errors


def main():
    kernel_files = [
        os.path.join(KERNEL_DIR, f)
        for f in os.listdir(KERNEL_DIR)
        if f.endswith(".cpp") and not f.endswith(".cu.cpp")
    ]
    kernel_files.sort()

    all_errors = []
    for kf in kernel_files:
        errors = check_file(kf)
        if errors:
            all_errors.append(f"\n--- {os.path.relpath(kf, os.path.dirname(KERNEL_DIR))} ---")
            all_errors.extend(errors)

    if all_errors:
        print("SPIR-V KERNEL COMPATIBILITY ERRORS:")
        print("\n".join(all_errors))
        print(f"\nTotal errors: {sum(1 for e in all_errors if e.startswith('  '))}")
        sys.exit(1)
    else:
        print(f"All {len(kernel_files)} kernel files pass SPIR-V compatibility checks.")
        sys.exit(0)


if __name__ == "__main__":
    main()
