#!/usr/bin/env python3
"""Replace NUM_PILLARS macro with NumPillars enum value."""

import os
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

def should_process_file(filepath):
    """Check if file should be processed."""
    skip_dirs = ['.git', '.venv', 'deps', 'generated', 'build', '__pycache__']
    parts = filepath.parts
    for part in parts:
        if part in skip_dirs:
            return False
    return filepath.suffix in ['.cpp', '.h', '.cu', '.hpp']

def update_file(filepath):
    """Update a single file to replace NUM_PILLARS with NumPillars."""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return False
    
    original_content = content
    
    # Replace NUM_PILLARS with NumPillars
    # Be careful not to replace #define NUM_PILLARS or #ifndef NUM_PILLARS
    # We want to replace usage of NUM_PILLARS as a value, not its definition
    
    # Replace in array declarations: float arr[NUM_PILLARS] -> float arr[NumPillars]
    content = re.sub(r'\[NUM_PILLARS\]', '[NumPillars]', content)
    
    # Replace in template arguments: std::array<float, NUM_PILLARS> -> std::array<float, NumPillars>
    content = re.sub(r'<float,\s*NUM_PILLARS>', '<float, NumPillars>', content)
    content = re.sub(r'<int,\s*NUM_PILLARS>', '<int, NumPillars>', content)
    
    # Replace in for loops: for (int i = 0; i < NUM_PILLARS; i++)
    content = re.sub(r'(for\s*\([^;]*;\s*[^;]*<\s*)NUM_PILLARS(\s*;)', r'\1NumPillars\2', content)
    content = re.sub(r'(for\s*\([^;]*;\s*[^;]*\s*<=\s*)NUM_PILLARS(\s*;)', r'\1NumPillars\2', content)
    
    # Replace in function parameters: const float pillars[NUM_PILLARS]
    content = re.sub(r'(\w+\s*)\[NUM_PILLARS\]', r'\1[NumPillars]', content)
    
    # Replace standalone NUM_PILLARS in expressions
    content = re.sub(r'\bNUM_PILLARS\b', 'NumPillars', content)
    
    # Remove #define NUM_PILLARS 16 and #ifndef NUM_PILLARS blocks
    # But be careful - some files might legitimately define it
    # We'll remove the macro definition only if it's defined as 16
    lines = content.split('\n')
    new_lines = []
    i = 0
    while i < len(lines):
        line = lines[i]
        # Skip #define NUM_PILLARS 16
        if re.match(r'^\s*#\s*define\s+NUM_PILLARS\s+16\s*$', line):
            # Check if next line is #endif
            if i + 1 < len(lines) and re.match(r'^\s*#\s*endif\s*$', lines[i+1]):
                i += 2  # Skip both lines
                continue
            i += 1
            continue
        # Skip #ifndef NUM_PILLARS
        if re.match(r'^\s*#\s*ifndef\s+NUM_PILLARS\s*$', line):
            # Skip until #endif
            while i < len(lines) and not re.match(r'^\s*#\s*endif\s*$', lines[i]):
                i += 1
            if i < len(lines):
                i += 1  # Skip #endif
            continue
        new_lines.append(line)
        i += 1
    
    content = '\n'.join(new_lines)
    
    if content != original_content:
        try:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        except Exception as e:
            print(f"Error writing {filepath}: {e}")
            return False
    return False

def main():
    print("Scanning for files to update NUM_PILLARS...")
    updated_count = 0
    total_count = 0
    
    for root, dirs, files in os.walk(REPO_ROOT):
        dirs[:] = [d for d in dirs if d not in ['.git', '.venv', 'deps', 'generated', 'build', '__pycache__']]
        
        for file in files:
            filepath = Path(root) / file
            if should_process_file(filepath):
                total_count += 1
                if update_file(filepath):
                    updated_count += 1
                    print(f"Updated: {filepath.relative_to(REPO_ROOT)}")
    
    print(f"\nTotal files scanned: {total_count}")
    print(f"Files updated: {updated_count}")

if __name__ == "__main__":
    main()
