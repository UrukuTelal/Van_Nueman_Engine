"""
Entity Manager: Python binding interface to CUDA entities + Pillar State Vectors.
From FULL_ARCHITECTURE.md: bindings/entity_manager
"""
import ctypes
import numpy as np

# Placeholder for CUDA shared library
# In production, this would load the compiled CUDA kernels as a shared library
_CUDA_LIB = None

try:
    import ctypes.util
    lib_path = ctypes.util.find_library('van_nueman_kernels')
    if lib_path:
        _CUDA_LIB = ctypes.CDLL(lib_path)
except:
    pass

# 16 Pillar indices (must match pillars_compute.cu)
PILLAR_NAMES = [
    "Awareness", "Willpower", "Force", "Influence", "Resistance",
    "Integrity", "Cohesion", "Relation", "Presence", "Warmth",
    "Memory", "Attraction", "Harm", "Distortion", "Flux", "Depth"
]
NUM_PILLARS = 16

# Scaled integer factor (2^20)
SCALE_FACTOR = 1048576

def to_scaled(f):
    return int(f * SCALE_FACTOR)

def from_scaled(i):
    return i / SCALE_FACTOR

class PillarStateVector:
    def __init__(self, values=None):
        if values is None:
            self.p = [0] * NUM_PILLARS
        else:
            self.p = list(values[:NUM_PILLARS])

    def get(self, pillar_idx):
        return from_scaled(self.p[pillar_idx])

    def set(self, pillar_idx, value):
        self.p[pillar_idx] = to_scaled(value)

    def as_dict(self):
        return {PILLAR_NAMES[i]: self.get(i) for i in range(NUM_PILLARS)}

class Entity:
    def __init__(self, eid, entity_type, position=(0,0,0)):
        self.id = eid
        self.type = entity_type
        self.position = position
        self.pillars = PillarStateVector()
        self.baseline = PillarStateVector()
        self.alive = True
        self.server_id = 0
        self.energy = 1.0
        self.health = 1.0

    def set_pillar(self, name, value):
        if name in PILLAR_NAMES:
            idx = PILLAR_NAMES.index(name)
            self.pillars.set(idx, value)

    def get_pillar(self, name):
        if name in PILLAR_NAMES:
            idx = PILLAR_NAMES.index(name)
            return self.pillars.get(idx)
        return None

# Entity type presets from FULL_ARCHITECTURE.md
ENTITY_PRESETS = {
    "celestial": {
        "Awareness": 0.0, "Willpower": 0.0, "Force": 0.15, "Influence": 0.25,
        "Resistance": 0.9, "Integrity": 0.95, "Cohesion": 0.7, "Relation": 0.1,
        "Presence": 0.4, "Warmth": 0.15, "Memory": 0.05, "Attraction": 0.75,
        "Harm": 0.15, "Distortion": 0.1
    },
    "living_planet": {
        "Awareness": 0.2, "Willpower": 0.2, "Force": 0.45, "Influence": 0.55,
        "Resistance": 0.7, "Integrity": 0.6, "Cohesion": 0.75, "Relation": 0.2,
        "Presence": 0.65, "Warmth": 0.55, "Memory": 0.45, "Attraction": 0.6,
        "Harm": 0.35, "Distortion": 0.2
    },
    "drone": {
        "Awareness": 0.5, "Willpower": 0.4, "Force": 0.55, "Influence": 0.2,
        "Resistance": 0.4, "Integrity": 0.6, "Cohesion": 0.1, "Relation": 0.3,
        "Presence": 0.2, "Warmth": 0.05, "Memory": 0.4, "Attraction": 0.3,
        "Harm": 0.45, "Distortion": 0.1
    },
    "human": {
        "Awareness": 0.8, "Willpower": 0.8, "Force": 0.65, "Influence": 0.75,
        "Resistance": 0.55, "Integrity": 0.7, "Cohesion": 0.65, "Relation": 0.75,
        "Presence": 0.7, "Warmth": 0.7, "Memory": 0.75, "Attraction": 0.7,
        "Harm": 0.6, "Distortion": 0.4
    },
    "server": {
        "Awareness": 0.9, "Willpower": 0.8, "Force": 0.75, "Influence": 0.75,
        "Resistance": 0.65, "Integrity": 0.8, "Cohesion": 0.65, "Relation": 0.8,
        "Presence": 0.75, "Warmth": 0.55, "Memory": 0.8, "Attraction": 0.65,
        "Harm": 0.55, "Distortion": 0.35
    },
    "federation": {
        "Awareness": 0.95, "Willpower": 0.9, "Force": 0.85, "Influence": 0.9,
        "Resistance": 0.75, "Integrity": 0.8, "Cohesion": 0.8, "Relation": 0.9,
        "Presence": 0.85, "Warmth": 0.65, "Memory": 0.9, "Attraction": 0.75,
        "Harm": 0.65, "Distortion": 0.45
    }
}

class EntityManager:
    def __init__(self, max_entities=500000):
        self.max_entities = max_entities
        self.entities = {}
        self.next_id = 1

    def create_entity(self, preset_name, position=(0,0,0)):
        if preset_name not in ENTITY_PRESETS:
            raise ValueError(f"Unknown preset: {preset_name}")
        e = Entity(self.next_id, preset_name, position)
        preset = ENTITY_PRESETS[preset_name]
        for name, value in preset.items():
            e.set_pillar(name, value)
        # Set baseline same as initial
        for i in range(NUM_PILLARS):
            e.baseline.p[i] = e.pillars.p[i]
        self.entities[e.id] = e
        self.next_id += 1
        return e

    def get_entity(self, eid):
        return self.entities.get(eid)

    def destroy_entity(self, eid):
        if eid in self.entities:
            del self.entities[eid]

    def to_numpy_arrays(self):
        """Export to numpy arrays for CUDA kernel input"""
        count = len(self.entities)
        positions = np.zeros((count, 3), dtype=np.float32)
        pillars = np.zeros((count, NUM_PILLARS), dtype=np.int32)
        alive = np.zeros(count, dtype=np.int32)
        ids = []
        for i, (eid, e) in enumerate(self.entities.items()):
            ids.append(eid)
            positions[i] = e.position
            pillars[i] = e.pillars.p
            alive[i] = 1 if e.alive else 0
        return ids, positions, pillars, alive

    def from_numpy_arrays(self, ids, pillars, alive):
        """Import updated pillar states from CUDA kernel output"""
        for i, eid in enumerate(ids):
            if eid in self.entities:
                e = self.entities[eid]
                e.pillars.p = list(pillars[i])
                e.alive = alive[i] == 1
