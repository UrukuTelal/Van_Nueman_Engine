# Auto-generated from pillars.yaml -- DO NOT EDIT
# Regenerate with: python scripts/generate_pillar_sources.py
from enum import IntEnum

NUM_PILLARS = 16

PILLAR_NAMES = [
    "awareness",
    "willpower",
    "force",
    "influence",
    "resistance",
    "integrity",
    "cohesion",
    "relation",
    "presence",
    "warmth",
    "memory",
    "attraction",
    "harm",
    "distortion",
    "flux",
    "depth",
]

class PillarIndex(IntEnum):
    Awareness = 0
    Willpower = 1
    Force = 2
    Influence = 3
    Resistance = 4
    Integrity = 5
    Cohesion = 6
    Relation = 7
    Presence = 8
    Warmth = 9
    Memory = 10
    Attraction = 11
    Harm = 12
    Distortion = 13
    Flux = 14
    Depth = 15
    NumPillars = 16


# Mapping from pillar name to index
PILLAR_NAME_TO_INDEX: dict[str, int] = {
    "awareness": PillarIndex.Awareness,
    "willpower": PillarIndex.Willpower,
    "force": PillarIndex.Force,
    "influence": PillarIndex.Influence,
    "resistance": PillarIndex.Resistance,
    "integrity": PillarIndex.Integrity,
    "cohesion": PillarIndex.Cohesion,
    "relation": PillarIndex.Relation,
    "presence": PillarIndex.Presence,
    "warmth": PillarIndex.Warmth,
    "memory": PillarIndex.Memory,
    "attraction": PillarIndex.Attraction,
    "harm": PillarIndex.Harm,
    "distortion": PillarIndex.Distortion,
    "flux": PillarIndex.Flux,
    "depth": PillarIndex.Depth,
}
