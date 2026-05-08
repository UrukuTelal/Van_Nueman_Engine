#!/usr/bin/env python3
"""
Dependency graph generator for Pillar AI Colab.

Parses pillar configs and interactions to generate a dependency graph
showing relationships between pillars. Outputs DOT (Graphviz) format
and text-based adjacency list.

Usage:
    python scripts/dependency_graph.py                    # Print text graph
    python scripts/dependency_graph.py --dot > graph.dot  # Export DOT
    python scripts/dependency_graph.py --mermaid          # Mermaid markdown
"""

import json
import os
import re
import sys
from pathlib import Path

# Pillar name mappings
TOTAL_PILLARS = 16

PILLAR_NAMES = {
    0: "Awareness", 1: "Willpower", 2: "Force", 3: "Influence",
    4: "Resistance", 5: "Integrity", 6: "Cohesion", 7: "Relation",
    8: "Presence", 9: "Warmth", 10: "Memory", 11: "Attraction",
    12: "Harm", 13: "Distortion", 14: "Flux", 15: "Depth"
}

# Reverse mapping
NAME_TO_INDEX = {v.lower(): k for k, v in PILLAR_NAMES.items()}


def find_pillar_references(text):
    """Find references to other pillars in text."""
    if not text:
        return set()

    refs = set()
    text_lower = text.lower()

    # Check for pillar names
    for name, idx in NAME_TO_INDEX.items():
        # Match pillar name as word boundary
        if re.search(rf'\b{re.escape(name)}\b', text_lower):
            refs.add(idx)

    # Check for "pillar_N" pattern
    for match in re.finditer(r'pillar[_]?(\d+)', text_lower):
        try:
            idx = int(match.group(1))
            if 0 <= idx < TOTAL_PILLARS:
                refs.add(idx)
        except ValueError:
            pass

    return refs


def load_pillar_configs(base_dir):
    """Load all pillar configurations."""
    configs = {}
    for i in range(TOTAL_PILLARS):
        pillar_dir = Path(base_dir) / f"Pillar_{i:02d}_{PILLAR_NAMES[i]}"
        config_path = pillar_dir / f"{PILLAR_NAMES[i].lower()}_config.json"
        if config_path.exists():
            with open(config_path, 'r', encoding='utf-8') as f:
                configs[i] = json.load(f)
    return configs


def extract_dependencies_from_config(config, pillar_idx):
    """Extract pillar dependencies from a config."""
    deps = set()

    # Check logic_mechanics (dict of mechanic_name -> {formula, impact})
    mechanics = config.get('logic_mechanics', {})
    if isinstance(mechanics, dict):
        for mech_name, mech_data in mechanics.items():
            if isinstance(mech_data, dict):
                formula = mech_data.get('formula', '')
                impact = mech_data.get('impact', '')
                deps.update(find_pillar_references(formula))
                deps.update(find_pillar_references(impact))

    # Check failure_modes (dict of mode_name -> {condition, result})
    failure_modes = config.get('failure_modes', {})
    if isinstance(failure_modes, dict):
        for mode_name, mode_data in failure_modes.items():
            if isinstance(mode_data, dict):
                condition = mode_data.get('condition', '')
                result = mode_data.get('result', '')
                deps.update(find_pillar_references(condition))
                deps.update(find_pillar_references(result))

    # Check operational_constants (dict of const_name -> value or object)
    constants = config.get('operational_constants', {})
    if isinstance(constants, dict):
        for const_name, const_data in constants.items():
            if isinstance(const_data, dict):
                desc = const_data.get('description', '')
                deps.update(find_pillar_references(desc))
            else:
                deps.update(find_pillar_references(str(const_data)))

    # Remove self-reference
    deps.discard(pillar_idx)

    return deps


def parse_interactions_md(interactions_path):
    """Parse pillar_interactions.md to extract interaction pairs."""
    interactions = []

    if not Path(interactions_path).exists():
        return interactions

    with open(interactions_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Find interaction patterns like "Pillar A + Pillar B"
    pattern = r'(\w+)\s*\+\s*(\w+)'
    for match in re.finditer(pattern, content, re.IGNORECASE):
        p1_name = match.group(1).lower()
        p2_name = match.group(2).lower()

        p1_idx = NAME_TO_INDEX.get(p1_name)
        p2_idx = NAME_TO_INDEX.get(p2_name)

        if p1_idx is not None and p2_idx is not None:
            interactions.append((p1_idx, p2_idx))

    return interactions


def build_graph(configs, interactions_path):
    """Build dependency graph from configs and interactions."""
    graph = {i: set() for i in range(TOTAL_PILLARS)}

    # Extract dependencies from configs
    for idx, config in configs.items():
        deps = extract_dependencies_from_config(config, idx)
        graph[idx].update(deps)

    # Add interactions (bidirectional)
    interactions = parse_interactions_md(interactions_path)
    for p1, p2 in interactions:
        graph[p1].add(p2)
        graph[p2].add(p1)

    return graph


def print_text_graph(graph):
    """Print a text-based adjacency list."""
    print("=" * 60)
    print("PILLAR DEPENDENCY GRAPH")
    print("=" * 60)

    for i in range(TOTAL_PILLARS):
        name = PILLAR_NAMES[i]
        deps = sorted(graph[i])
        if deps:
            dep_names = [PILLAR_NAMES[d] for d in deps]
            print(f"{name:12s} -> {', '.join(dep_names)}")
        else:
            print(f"{name:12s} -> (no dependencies)")

    print("=" * 60)

    # Print statistics
    total_edges = sum(len(deps) for deps in graph.values()) // 2
    print(f"\nTotal pillars: {TOTAL_PILLARS}")
    print(f"Total connections: {total_edges}")

    # Find most connected
    connectivity = [(i, len(graph[i])) for i in range(TOTAL_PILLARS)]
    connectivity.sort(key=lambda x: x[1], reverse=True)

    print(f"\nMost connected pillars:")
    for idx, count in connectivity[:5]:
        print(f"  {PILLAR_NAMES[idx]:12s}: {count} connections")


def generate_dot(graph, configs):
    """Generate DOT format for Graphviz."""
    lines = ["digraph PillarDependencies {", "  rankdir=LR;"]
    lines.append("  node [shape=box, style=filled, fontname=\"Arial\"];")
    lines.append("")

    # Color mapping based on pillar category
    colors = {
        0: "#FFE5E5", 1: "#E5FFE5", 2: "#E5E5FF", 3: "#FFFFE5",
        4: "#FFE5FF", 5: "#E5FFFF", 6: "#FFF0E5", 7: "#F0FFE5",
        8: "#E5F0FF", 9: "#FFE5F0", 10: "#F5E5FF", 11: "#E5FFF5",
        12: "#FFF5E5", 13: "#F5F5E5", 14: "#E8F5E9", 15: "#E3F2FD"
    }

    # Add nodes
    for i in range(TOTAL_PILLARS):
        name = PILLAR_NAMES[i]
        color = colors.get(i, "#FFFFFF")
        label = f"{name}\\n(Pillar {i})"

        # Add short description if available
        if i in configs:
            desc = configs[i].get('description', '')
            if desc:
                short_desc = desc[:40] + "..." if len(desc) > 40 else desc
                label = f"{name}\\n{short_desc}"

        lines.append(f'  p{i} [label="{label}", fillcolor="{color}"];')

    lines.append("")

    # Add edges
    for src in range(TOTAL_PILLARS):
        for dst in sorted(graph[src]):
            if dst > src:  # Avoid duplicate edges
                lines.append(f'  p{src} -> p{dst};')

    lines.append("}")
    return "\n".join(lines)


def generate_mermaid(graph):
    """Generate Mermaid markdown graph."""
    lines = ["```mermaid", "graph LR"]

    # Add nodes
    for i in range(TOTAL_PILLARS):
        name = PILLAR_NAMES[i]
        lines.append(f"    P{i}[{name}]")

    lines.append("")

    # Add edges
    seen = set()
    for src in range(TOTAL_PILLARS):
        for dst in sorted(graph[src]):
            edge_key = tuple(sorted((src, dst)))
            if edge_key not in seen:
                lines.append(f"    P{src} --- P{dst}")
                seen.add(edge_key)

    lines.append("```")
    return "\n".join(lines)


def main():
    base_dir = Path(__file__).parent.parent
    interactions_path = base_dir / "Interactions" / "pillar_interactions.md"

    # Load configs
    configs = load_pillar_configs(base_dir)

    if len(configs) < TOTAL_PILLARS:
        print(f"[WARN] Only {len(configs)} pillar configs loaded", file=sys.stderr)

    # Build graph
    graph = build_graph(configs, interactions_path)

    # Parse command line args
    args = sys.argv[1:]

    if "--dot" in args:
        print(generate_dot(graph, configs))
    elif "--mermaid" in args:
        print(generate_mermaid(graph))
    else:
        print_text_graph(graph)
        print("\nTip: Use --dot for Graphviz output, --mermaid for Mermaid markdown")


if __name__ == "__main__":
    main()
