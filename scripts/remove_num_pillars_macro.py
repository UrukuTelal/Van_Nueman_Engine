#!/usr/bin/env python3
"""Remove #define NumPillars 16 macro definitions."""

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
    """Remove #define NumPillars 16 from file."""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return False
    
    original_content = content
    
    # Remove #define NumPillars 16 lines
    lines = content.split('\n')
    new_lines = []
    for line in lines:
        # Skip #define NumPillars 16
        if re.match(r'^\s*#\s*define\s+NumPillars\s+16\s*$', line):
            continue
        new_lines.append(line)
    
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
    print("Scanning for files with #define NumPillars 16...")
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
