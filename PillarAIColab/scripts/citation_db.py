#!/usr/bin/env python3
"""
Citation DB extraction tool for Pillar AI Colab.

Extracts [cite: XX] references from markdown files and builds
a citation database mapping citation numbers to sources.

Usage:
    python scripts/citation_db.py                          # Print citation summary
    python scripts/citation_db.py --json > citations.json # Export as JSON
    python scripts/citation_db.py --check                 # Validate citation consistency
"""

import json
import os
import re
import sys
from pathlib import Path
from collections import defaultdict


def extract_citations_from_file(filepath):
    """Extract all [cite: XX] references from a file."""
    citations = []

    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        # Find all [cite: XX] patterns (XX can be multiple numbers like "12" or "12,15,18")
        pattern = r'\[cite:\s*([\d,\s]+)\]'
        for match in re.finditer(pattern, content):
            cite_nums = [int(x.strip()) for x in match.group(1).split(',') if x.strip().isdigit()]
            line_num = content[:match.start()].count('\n') + 1
            citations.append({
                'numbers': cite_nums,
                'line': line_num,
                'context': get_context(content, match.start(), 50)
            })

    except Exception as e:
        print(f"[WARN] Error reading {filepath}: {e}", file=sys.stderr)

    return citations


def get_context(content, pos, window=50):
    """Get text context around a position."""
    start = max(0, pos - window)
    end = min(len(content), pos + window)
    return content[start:end].replace('\n', ' ').strip()


def build_citation_db(base_dir):
    """Build citation database from all markdown files."""
    citation_db = defaultdict(list)
    file_citations = {}

    # Scan all markdown files
    for root, dirs, files in os.walk(base_dir):
        # Skip scripts and src directories
        if 'scripts' in root or 'src' in root:
            continue

        for file in files:
            if file.endswith('.md'):
                filepath = os.path.join(root, file)
                rel_path = os.path.relpath(filepath, base_dir)

                cites = extract_citations_from_file(filepath)
                if cites:
                    file_citations[rel_path] = cites

                    # Add to citation DB
                    for cite in cites:
                        for num in cite['numbers']:
                            citation_db[num].append({
                                'file': rel_path,
                                'line': cite['line'],
                                'context': cite['context']
                            })

    return citation_db, file_citations


def print_summary(citation_db, file_citations):
    """Print citation summary."""
    print("=" * 60)
    print("CITATION DATABASE SUMMARY")
    print("=" * 60)

    print(f"\nTotal unique citations: {len(citation_db)}")
    print(f"Total files with citations: {len(file_citations)}")

    # Files summary
    print(f"\n{'File':<45} {'Citations':<10}")
    print("-" * 60)
    for filepath, cites in sorted(file_citations.items()):
        total_cites = sum(len(c['numbers']) for c in cites)
        print(f"{filepath:<45} {total_cites:<10}")

    # Most referenced citations
    print(f"\n{'Citation':<10} {'References':<10} {'Files'}")
    print("-" * 60)
    sorted_cites = sorted(citation_db.items(), key=lambda x: len(x[1]), reverse=True)
    for num, refs in sorted_cites[:10]:
        files = set(r['file'] for r in refs)
        print(f"[{num:<8}] {len(refs):<10} {', '.join(sorted(files))}")

    # Find orphaned citations (referenced but not defined)
    print(f"\nNote: This tool extracts citations but cannot validate")
    print(f"against source documents (Google AI outputs).")


def export_json(citation_db, file_citations):
    """Export citation database as JSON."""
    output = {
        'summary': {
            'total_unique_citations': len(citation_db),
            'total_files': len(file_citations)
        },
        'citations': {str(k): v for k, v in sorted(citation_db.items())},
        'files': {k: [{'numbers': c['numbers'], 'line': c['line']}
                       for c in v] for k, v in file_citations.items()}
    }
    print(json.dumps(output, indent=2))


def check_consistency(citation_db):
    """Check citation consistency."""
    print("=" * 60)
    print("CITATION CONSISTENCY CHECK")
    print("=" * 60)

    issues = 0

    # Check for gaps in citation numbers
    if citation_db:
        nums = sorted(citation_db.keys())
        expected = list(range(min(nums), max(nums) + 1))
        missing = [n for n in expected if n not in nums]

        if missing:
            print(f"\n[WARN] Gap in citations: missing {missing[:10]}...")
            issues += 1
        else:
            print(f"\n[OK] No gaps in citation numbers (range: {min(nums)}-{max(nums)})")

    # Check for duplicate citation numbers in same file
    print(f"\n[OK] No duplicate check implemented (requires source docs)")

    return issues


def main():
    base_dir = Path(__file__).parent.parent

    # Build citation DB
    citation_db, file_citations = build_citation_db(base_dir)

    # Parse command line args
    args = sys.argv[1:]

    if "--json" in args:
        export_json(citation_db, file_citations)
    elif "--check" in args:
        check_consistency(citation_db)
    else:
        print_summary(citation_db, file_citations)
        print(f"\nTip: Use --json for JSON export, --check for consistency check")


if __name__ == "__main__":
    main()
