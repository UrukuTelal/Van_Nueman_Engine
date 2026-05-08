#!/usr/bin/env python3
"""
Creature Design using Pillar AI Colab.
Uses Pillar State Vectors to define creature behaviors and characteristics.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent))

from scripts.agent_roles import route_task_to_role, get_model_for_role
from api.pillar_ai_bridge import PillarAIEngine
import json
import numpy as np

# Creature preset templates (14 pillars, Flux and Depth not included)
CREATURE_PRESETS = {
    "predator": {
        "name": "Predator",
        "description": "Fast, stealthy, high-force creature with hunting instincts",
        "pillars": {
            "Awareness": 0.8,    # High perception
            "Willpower": 0.7,    # Persistent hunter
            "Force": 0.9,        # Strong attack
            "Influence": 0.3,   # Low social influence
            "Resistance": 0.6,  # Moderate defense
            "Integrity": 0.7,   # Consistent behavior
            "Cohesion": 0.2,    # Solitary hunter
            "Relation": 0.3,    # Few relationships
            "Presence": 0.6,   # Moderate visibility
            "Warmth": 0.2,     # Cold, efficient
            "Memory": 0.6,      # Remembers hunting grounds
            "Attraction": 0.8,  # Strong prey drive
            "Harm": 0.7,        # High damage capability
            "Distortion": 0.2  # Clear predator instinct
        }
    },
    "herbivore": {
        "name": "Herbivore",
        "description": "Gentle creature with high warmth, flocking behavior",
        "pillars": {
            "Awareness": 0.5,
            "Willpower": 0.4,
            "Force": 0.3,
            "Influence": 0.6,
            "Resistance": 0.7,
            "Integrity": 0.8,
            "Cohesion": 0.9,    # Strong flocking
            "Relation": 0.8,    # Social bonds
            "Presence": 0.4,
            "Warmth": 0.9,     # Nurturing
            "Memory": 0.5,
            "Attraction": 0.4,
            "Harm": 0.2,        # Low damage
            "Distortion": 0.3
        }
    },
    "scavenger": {
        "name": "Scavenger",
        "description": "Opportunistic creature, high adaptability",
        "pillars": {
            "Awareness": 0.7,
            "Willpower": 0.5,
            "Force": 0.4,
            "Influence": 0.4,
            "Resistance": 0.5,
            "Integrity": 0.6,
            "Cohesion": 0.5,
            "Relation": 0.6,
            "Presence": 0.5,
            "Warmth": 0.4,
            "Memory": 0.7,      # Remembers food sources
            "Attraction": 0.6,
            "Harm": 0.4,
            "Distortion": 0.4   # Opportunistic nature
        }
    }
}

def design_creature(concept: str) -> dict:
    """
    Use Pillar AI to design a creature based on concept.
    Returns creature definition with pillar vector.
    """
    # Initialize Pillar AI
    try:
        engine = PillarAIEngine()
        engine.load_pillars()
        
        # Get context for creature design
        context = engine.get_context_for_task(f"Design a creature: {concept}")
        
        print(f"[Pillar AI] Context prepared for: {concept}")
        print(f"  Context length: {len(context)} characters")
        
    except Exception as e:
        print(f"[WARN] Pillar AI not available: {e}")
        # Fall back to preset matching
        concept_lower = concept.lower()
        for key, preset in CREATURE_PRESETS.items():
            if key in concept_lower:
                print(f"[Fallback] Using '{key}' preset")
                return preset
        # Default to herbivore
        return CREATURE_PRESETS["herbivore"]
    
    # Route to appropriate agent for creature design
    role, method = route_task_to_role(f"Design creature: {concept}")
    model = get_model_for_role(role)
    
    print(f"[Agent] Routed to: {role} (model: {model}, method: {method})")
    
    # Return matched preset (in real implementation, would query LLM)
    concept_lower = concept.lower()
    for key, preset in CREATURE_PRESETS.items():
        if key in concept_lower:
            return preset
    
    return CREATURE_PRESETS["herbivore"]

def create_creature_json(creature: dict, filename: str = "creature_output.json"):
    """Save creature definition to JSON file."""
    output_path = Path("bindings") / filename
    output_path.parent.mkdir(exist_ok=True)
    
    with open(output_path, 'w') as f:
        json.dump(creature, f, indent=2)
    
    print(f"[Save] Creature saved to: {output_path}")
    return output_path

def generate_skelley_config(creature: dict) -> dict:
    """
    Generate Skelly (skeletal) configuration for the creature.
    Maps pillar values to bone/muscle/organ layout.
    """
    pillars = creature["pillars"]
    
    # Scale factors based on pillars
    size_scale = 0.5 + pillars["Force"] * 1.0  # Force -> body size
    speed = 0.3 + pillars["Awareness"] * 0.7  # Awareness -> speed
    strength = pillars["Force"]
    
    skelly_config = {
        "creature_name": creature["name"],
        "scale": size_scale,
        "bones": [
            {"name": "spine", "length": 2.0 * size_scale, "flexibility": 0.8},
            {"name": "limb_front_left", "length": 1.5 * size_scale, "flexibility": 0.6},
            {"name": "limb_front_right", "length": 1.5 * size_scale, "flexibility": 0.6},
            {"name": "limb_rear_left", "length": 1.5 * size_scale, "flexibility": 0.6},
            {"name": "limb_rear_right", "length": 1.5 * size_scale, "flexibility": 0.6},
            {"name": "head", "length": 0.8 * size_scale, "flexibility": 0.9}
        ],
        "muscles": [
            {"name": "spine_flex", "activation": speed, "strands": 8},
            {"name": "limb_movers", "activation": strength, "strands": 4}
        ],
        "organs": [
            {"type": "pump", "name": "heart", "volume": 1.0 * size_scale},
            {"type": "factory", "name": "stomach", "volume": 0.8 * size_scale}
        ],
        "pillar_modifiers": {
            "Force": f"affects muscle activation (strength={strength:.1f})",
            "Awareness": f"affects limb speed (speed={speed:.1f})",
            "Warmth": f"affects energy output"
        }
    }
    
    return skelly_config

if __name__ == "__main__":
    print("=" * 60)
    print("Creature Design using Pillar AI Colab")
    print("=" * 60)
    print()
    
    # Test creature designs
    test_concepts = ["predator", "herbivore", "scavenger"]
    
    for concept in test_concepts:
        print(f"\n{'-' * 40}")
        print(f"Designing: {concept.upper()}")
        print(f"{'-' * 40}")
        
        creature = design_creature(concept)
        print(f"Name: {creature['name']}")
        print(f"Description: {creature['description']}")
        print(f"\nPillar Vector:")
        for pillar, value in creature["pillars"].items():
            bar = "#" * int(value * 20)
            print(f"  {pillar:15} [{bar:20}] {value:.2f}")
        
        # Generate Skelly config
        skelly = generate_skelley_config(creature)
        print(f"\nSkelly Config:")
        print(f"  Bones: {len(skelly['bones'])}")
        print(f"  Muscles: {len(skelly['muscles'])}")
        print(f"  Organs: {len(skelly['organs'])}")
        
        # Save to file
        safe_name = concept.replace(" ", "_")
        create_creature_json(creature, f"creature_{safe_name}.json")
    
    print(f"\n{'=' * 60}")
    print("Creature design complete!")
    print(f"{'=' * 60}")
