#!/usr/bin/env python3
"""
Pillar Interaction Simulation Engine.

Simulates how pillar states change over time based on interactions
and failure mode triggers. Useful for testing scenarios before
running them through LLMs.

Usage:
    python scripts/simulate.py --scenario "high_force_low_resistance"
    python scripts/simulate.py --interactive
"""

import json
import sys
import random
from pathlib import Path
from dataclasses import dataclass, field
from typing import Dict, List, Tuple

if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8')

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))


@dataclass
class PillarState:
    """Represents the state of a pillar."""
    index: int
    name: str
    value: float  # 0.0 to 1.0
    target: float = 0.5
    decay_rate: float = 0.01
    growth_rate: float = 0.02

    def update(self):
        """Update state toward target with decay."""
        if self.value < self.target:
            self.value = min(1.0, self.value + self.growth_rate)
        elif self.value > self.target:
            self.value = max(0.0, self.value - self.decay_rate)

    def apply_impact(self, impact: float):
        """Apply an impact value (-1.0 to 1.0)."""
        self.value = max(0.0, min(1.0, self.value + impact))

    def check_failure(self, thresholds: Dict[str, float]) -> List[str]:
        """Check if any failure modes are triggered."""
        failures = []
        if 'min' in thresholds and self.value < thresholds['min']:
            failures.append(f"below_min ({self.value:.2f} < {thresholds['min']})")
        if 'max' in thresholds and self.value > thresholds['max']:
            failures.append(f"above_max ({self.value:.2f} > {thresholds['max']})")
        return failures


class PillarSimulation:
    """Simulates pillar interactions over time."""

    def __init__(self):
        self.pillars: Dict[int, PillarState] = {}
        self.interactions: List[Tuple[int, int, float]] = []  # (src, dst, strength)
        self.history: List[Dict] = []
        self.step_count = 0

    def initialize_from_loader(self, loader):
        """Initialize pillar states from PillarLoader."""
        for idx, config in loader.pillars.items():
            name = config.name

            # Set initial value based on constants or default
            init_value = config.operational_constants.get('initial_state', 0.5)

            self.pillars[idx] = PillarState(
                index=idx,
                name=name,
                value=init_value,
                target=init_value
            )

        # Parse interactions from loader
        # Build name to index mapping
        name_to_idx = {p.name.lower(): idx for idx, p in loader.pillars.items()}

        for interaction in loader.interactions:
            p1_name, p2_name = interaction.pillars
            p1_idx = name_to_idx.get(p1_name.lower())
            p2_idx = name_to_idx.get(p2_name.lower())

            if p1_idx is not None and p2_idx is not None:
                # Determine strength from category or default
                strength = 0.5
                if 'enhance' in interaction.category.lower():
                    strength = 0.7
                elif 'conflict' in interaction.category.lower():
                    strength = -0.3

                self.interactions.append((p1_idx, p2_idx, strength))

    def set_pillar(self, index: int, value: float):
        """Set a pillar's value directly."""
        if index in self.pillars:
            self.pillars[index].value = max(0.0, min(1.0, value))

    def set_target(self, index: int, target: float):
        """Set a pillar's target value."""
        if index in self.pillars:
            self.pillars[index].target = max(0.0, min(1.0, target))

    def step(self):
        """Advance simulation by one step."""
        self.step_count += 1

        # Apply interactions
        for src, dst, strength in self.interactions:
            if src in self.pillars and dst in self.pillars:
                src_state = self.pillars[src].value
                impact = (src_state - 0.5) * strength * 0.1
                self.pillars[dst].apply_impact(impact)

        # Update all pillars (decay toward target)
        for pillar in self.pillars.values():
            pillar.update()

        # Record history
        snapshot = {
            'step': self.step_count,
            'pillars': {idx: p.value for idx, p in self.pillars.items()}
        }
        self.history.append(snapshot)

    def run(self, steps: int = 10):
        """Run simulation for N steps."""
        for _ in range(steps):
            self.step()

    def print_state(self):
        """Print current pillar states."""
        print(f"\nStep {self.step_count}:")
        print("-" * 50)
        for idx in sorted(self.pillars.keys()):
            p = self.pillars[idx]
            bar = "#" * int(p.value * 20) + "-" * (20 - int(p.value * 20))
            print(f"  {p.name:12s} [{bar}] {p.value:.2f}")

    def check_failures(self) -> Dict[int, List[str]]:
        """Check for failure mode triggers."""
        failures = {}
        thresholds = {
            0: {'min': 0.1, 'max': 0.95},  # Awareness
            1: {'min': 0.2},                # Willpower
            4: {'min': 0.3},                # Resistance
            12: {'max': 0.7},               # Harm
            13: {'max': 0.6},               # Distortion
        }

        for idx, pillar in self.pillars.items():
            if idx in thresholds:
                fails = pillar.check_failure(thresholds[idx])
                if fails:
                    failures[idx] = fails

        return failures

    def print_failures(self):
        """Print any triggered failure modes."""
        failures = self.check_failures()
        if failures:
            print("\n[WARN] Failure modes triggered:")
            for idx, fails in failures.items():
                p = self.pillars[idx]
                print(f"  {p.name}: {', '.join(fails)}")
        else:
            print("\n[OK] No failure modes triggered")

    def export_json(self) -> str:
        """Export simulation history as JSON."""
        return json.dumps({
            'steps': self.step_count,
            'history': self.history,
            'final_state': {idx: p.value for idx, p in self.pillars.items()}
        }, indent=2)


def scenario_high_force_low_resistance(sim: PillarSimulation):
    """Scenario: High force, low resistance."""
    sim.set_pillar(2, 0.9)   # Force high
    sim.set_pillar(4, 0.2)   # Resistance low
    sim.set_pillar(12, 0.3)  # Some Harm
    return sim


def scenario_awareness_breakdown(sim: PillarSimulation):
    """Scenario: Awareness drops, distortion rises."""
    sim.set_pillar(0, 0.15)  # Awareness low
    sim.set_pillar(13, 0.8)  # Distortion high
    sim.set_pillar(8, 0.9)   # Presence high (false confidence)
    return sim


SCENARIOS = {
    'high_force_low_resistance': scenario_high_force_low_resistance,
    'awareness_breakdown': scenario_awareness_breakdown,
}


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Pillar Simulation Engine")
    parser.add_argument('--scenario', choices=list(SCENARIOS.keys()),
                        help='Predefined scenario to run')
    parser.add_argument('--steps', type=int, default=10,
                        help='Number of simulation steps')
    parser.add_argument('--interactive', action='store_true',
                        help='Interactive mode')

    args = parser.parse_args()

    # Initialize simulation
    from src.pillar_loader import PillarLoader

    loader = PillarLoader()
    loader.load_all()

    sim = PillarSimulation()
    sim.initialize_from_loader(loader)

    # Apply scenario if specified
    if args.scenario:
        print(f"Running scenario: {args.scenario}")
        SCENARIOS[args.scenario](sim)

    # Run simulation
    print(f"\nInitial state:")
    sim.print_state()

    for i in range(args.steps):
        sim.step()
        sim.print_state()
        sim.print_failures()

    # Export option
    print(f"\n[OK] Simulation complete ({args.steps} steps)")
    print(f"Tip: Use --interactive for step-by-step mode")


if __name__ == "__main__":
    main()
