#!/usr/bin/env python3
"""Bulk replace old PILLAR_* naming with new Awareness naming convention."""

import os
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# Mapping from old to new naming
PILLAR_MAPPING = {
    'PILLAR_AWARENESS': 'Awareness',
    'PILLAR_WILLPOWER': 'Willpower',
    'PILLAR_FORCE': 'Force',
    'PILLAR_INFLUENCE': 'Influence',
    'PILLAR_RESISTANCE': 'Resistance',
    'PILLAR_INTEGRITY': 'Integrity',
    'PILLAR_COHESION': 'Cohesion',
    'PILLAR_RELATION': 'Relation',
    'PILLAR_PRESENCE': 'Presence',
    'PILLAR_WARMTH': 'Warmth',
    'PILLAR_MEMORY': 'Memory',
    'PILLAR_ATTRACTION': 'Attraction',
    'PILLAR_HARM': 'Harm',
    'PILLAR_DISTORTION': 'Distortion',
    'PILLAR_FLUX': 'Flux',
    'PILLAR_DEPTH': 'Depth',
}

# Also replace NUM_PILLARS with NumPillars (from the enum)
# But be careful - NUM_PILLARS might be a macro in some places

def should_process_file(filepath):
    """Check if file should be processed."""
    # Skip generated files, .git, .venv, deps
    skip_dirs = ['.git', '.venv', 'deps', 'generated', 'build', '__pycache__']
    parts = filepath.parts
    for part in parts:
        if part in skip_dirs:
            return False
    return filepath.suffix in ['.cpp', '.h', '.cu', '.hpp']

def update_file(filepath):
    """Update a single file with the new naming."""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return False
    
    original_content = content
    
    # Replace pillar index names
    for old, new in PILLAR_MAPPING.items():
        # Use word boundary to avoid partial matches
        pattern = r'\b' + re.escape(old) + r'\b'
        content = re.sub(pattern, new, content)
    
    # Replace NUM_PILLARS macro with NumPillars enum value
    # Only when it's used as an array size or loop bound, not as a macro definition
    # We'll be conservative and only replace in specific contexts
    # content = re.sub(r'\bNUM_PILLARS\b', 'NumPillars', content)
    
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
    print("Scanning for files to update...")
    updated_count = 0
    total_count = 0
    
    for root, dirs, files in os.walk(REPO_ROOT):
        # Skip directories
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
