#!/usr/bin/env python3
"""
Pillar Loader API
Loads pillar configurations and identity files for LLM context injection.
Designed for local LLMs (qwen2.5:0.5b, 1.5b) with efficient context building.
"""

import json
import re
from pathlib import Path
from typing import Optional
from dataclasses import dataclass, field

TOTAL_PILLARS = 16


@dataclass
class PillarConfig:
    """Represents a single pillar's configuration."""
    name: str
    index: int
    core_function: str
    description: str
    operational_constants: dict = field(default_factory=dict)
    logic_mechanics: dict = field(default_factory=dict)
    failure_modes: dict = field(default_factory=dict)
    identity_content: str = ""
    
    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        return {
            "name": self.name,
            "index": self.index,
            "core_function": self.core_function,
            "description": self.description,
            "operational_constants": self.operational_constants,
            "logic_mechanics": self.logic_mechanics,
            "failure_modes": self.failure_modes
        }
    
    def to_prompt(self) -> str:
        """Format as LLM-readable prompt text."""
        lines = [
            f"PILLAR {self.index}: {self.name.upper()}",
            f"Function: {self.core_function}",
            f"Description: {self.description}",
            "",
            "OPERATIONAL CONSTANTS:"
        ]
        for key, val in self.operational_constants.items():
            lines.append(f"  - {key}: {val}")
        
        lines.append("\nLOGIC MECHANICS:")
        for key, val in self.logic_mechanics.items():
            formula = val.get("formula", "N/A")
            impact = val.get("impact", val.get("result", "N/A"))
            lines.append(f"  - {key}:")
            lines.append(f"      Formula: {formula}")
            lines.append(f"      Impact: {impact}")
        
        lines.append("\nFAILURE MODES:")
        for key, val in self.failure_modes.items():
            lines.append(f"  - {key}:")
            lines.append(f"      Condition: {val.get('condition', 'N/A')}")
            lines.append(f"      Result: {val.get('result', 'N/A')}")
        
        return "\n".join(lines)


@dataclass
class Interaction:
    """Represents a pillar interaction."""
    pillars: tuple[str, str]
    result: str
    category: str = ""


class PillarLoader:
    """
    Main loader class for pillar configurations.
    
    Usage:
        loader = PillarLoader("/path/to/PillarAIColab")
        loader.load_all()
        
        # Get specific pillar
        pillar = loader.get_pillar(0)  # Awareness
        
        # Build context for LLM
        context = loader.build_context("optimize team productivity")
    """
    
    def __init__(self, pillar_dir: Optional[str] = None):
        """Initialize loader with pillar directory path."""
        if pillar_dir:
            self.pillar_dir = Path(pillar_dir)
        else:
            # Auto-detect: go up from scripts/
            self.pillar_dir = Path(__file__).parent.parent
        
        self.pillars: dict[int, PillarConfig] = {}
        self.interactions: list[Interaction] = []
        self.blackboard: str = ""
        self.defaults_loaded = False
    
    def load_pillar(self, index: int) -> Optional[PillarConfig]:
        """Load a single pillar by index."""
        if index in self.pillars:
            return self.pillars[index]
        
        # Find pillar directory
        pillar_dirs = list(self.pillar_dir.glob(f"Pillar_{index:02d}_*"))
        if not pillar_dirs:
            return None
        
        pillar_dir = pillar_dirs[0]
        config_file = next(pillar_dir.glob("*_config.json"), None)
        identity_file = next(pillar_dir.glob("*_identity.md"), None)
        
        if not config_file:
            return None
        
        try:
            with open(config_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            metadata = data.get("pillar_metadata", {})
            config = PillarConfig(
                name=metadata.get("name", f"Pillar_{index}"),
                index=index,
                core_function=metadata.get("core_function", ""),
                description=metadata.get("description", ""),
                operational_constants=data.get("operational_constants", {}),
                logic_mechanics=data.get("logic_mechanics", {}),
                failure_modes=data.get("failure_modes", {})
            )
            
            # Load identity file if exists
            if identity_file:
                with open(identity_file, 'r', encoding='utf-8') as f:
                    config.identity_content = f.read()
            
            self.pillars[index] = config
            return config
            
        except Exception as e:
            print(f"Error loading pillar {index}: {e}")
            return None
    
    def load_all(self) -> dict[int, PillarConfig]:
        """Load all 16 pillars (0-15)."""
        for i in range(TOTAL_PILLARS):
            self.load_pillar(i)
        
        self._load_interactions()
        self._load_blackboard()
        return self.pillars
    
    def _load_interactions(self):
        """Load pillar interactions from markdown file."""
        interaction_file = self.pillar_dir / "Interactions" / "pillar_interactions.md"
        if not interaction_file.exists():
            return
        
        with open(interaction_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Parse interaction table entries
        # Format: **Pillar1 + Pillar2**: Description.
        pattern = r'\*\*([A-Za-z]+) \+ ([A-Za-z]+)\*\*:\s*(.+?)\.'
        matches = re.findall(pattern, content, re.DOTALL)
        
        for pillar1, pillar2, description in matches:
            # Determine category from section headers
            category = ""
            if "Growth" in content[:content.find(f"{pillar1} + {pillar2}")]:
                category = "Growth & Expansion"
            elif "Stability" in content[:content.find(f"{pillar1} + {pillar2}")]:
                category = "Stability & Defense"
            # ... etc
            
            self.interactions.append(Interaction(
                pillars=(pillar1, pillar2),
                result=description.strip(),
                category=category
            ))
    
    def _load_blackboard(self):
        """Load the blackboard governance document."""
        blackboard_file = self.pillar_dir / "colab_blackboard" / "colab_blackboard.md"
        if blackboard_file.exists():
            with open(blackboard_file, 'r', encoding='utf-8') as f:
                self.blackboard = f.read()
    
    def get_pillar(self, index: int) -> Optional[PillarConfig]:
        """Get a loaded pillar by index."""
        return self.pillars.get(index)
    
    def get_pillar_by_name(self, name: str) -> Optional[PillarConfig]:
        """Get a loaded pillar by name (case-insensitive)."""
        name_lower = name.lower()
        for pillar in self.pillars.values():
            if pillar.name.lower() == name_lower:
                return pillar
        return None
    
    def get_interaction(self, pillar_a: str, pillar_b: str) -> Optional[str]:
        """Get interaction description for two pillars."""
        pillar_a_upper = pillar_a.upper()
        pillar_b_upper = pillar_b.upper()
        
        for interaction in self.interactions:
            if (interaction.pillars[0].upper() == pillar_a_upper and 
                interaction.pillars[1].upper() == pillar_b_upper):
                return interaction.result
            if (interaction.pillars[0].upper() == pillar_b_upper and 
                interaction.pillars[1].upper() == pillar_a_upper):
                return interaction.result
        
        return None
    
    def build_context(self, task: str = "", include_pillars: Optional[list[int]] = None, 
                       agent_id: str = "unknown_agent", log_context: bool = True) -> str:
        """
        Build RAG context for LLM injection.
        
        Args:
            task: Optional task description to filter relevant pillars
            include_pillars: Optional list of pillar indices to include
            agent_id: Agent/session ID for blackboard logging
            log_context: If True, log this context request to blackboard
        
        Returns:
            Formatted context string for LLM prompt
        """
        context_parts = []
        
        # 1. Blackboard (governance)
        if self.blackboard:
            context_parts.append("=== GOVERNANCE (BLACKBOARD) ===")
            context_parts.append(self.blackboard)
            context_parts.append("")
        
        # 2. Relevant pillars
        pillars_to_include = include_pillars or list(self.pillars.keys())
        
        if pillars_to_include:
            context_parts.append("=== PILLAR CONFIGURATIONS ===")
            for idx in sorted(pillars_to_include):
                if idx in self.pillars:
                    context_parts.append(self.pillars[idx].to_prompt())
                    context_parts.append("")
        
        # 3. Interactions
        if self.interactions:
            context_parts.append("=== PILLAR INTERACTIONS ===")
            for interaction in self.interactions[:20]:  # Limit for context size
                context_parts.append(
                    f"{interaction.pillars[0]} + {interaction.pillars[1]}: "
                    f"{interaction.result}"
                )
            context_parts.append("")
        
        # 4. Task-specific guidance
        if task:
            context_parts.append("=== CURRENT TASK ===")
            context_parts.append(task)
            context_parts.append("")
        
        # 5. MANDATORY: Log context request to blackboard for cross-session continuity
        if log_context and task:
            try:
                import sys
                sys.path.insert(0, str(self.pillar_dir.parent))
                from api.pillar_ai_bridge import log_to_blackboard
                from datetime import datetime, timezone
                
                log_to_blackboard({
                    "agent_id": agent_id,
                    "task": f"Context request: {task}",
                    "delta_h": 0.0,  # Context requests are low-harm
                    "approved": True,
                    "notes": f"Pillars included: {include_pillars}"
                })
            except Exception as e:
                print(f"Warning: Could not log to blackboard: {e}")
        
        return "\n".join(context_parts)
    
    def build_state_vector(self) -> dict[str, float]:
        """
        Build a Pillar State Vector from configs.
        Returns normalized values (0.0-1.0) for each pillar.
        """
        state = {}
        for idx, pillar in self.pillars.items():
            # Extract default/center values from operational_constants
            # This is a placeholder - actual values would come from simulation
            state[pillar.name] = 0.5  # Default center value
        return state
    
    def summary(self) -> str:
        """Generate summary of loaded pillars."""
        lines = [
            f"Pillar Loader Summary",
            f"=====================",
            f"Loaded: {len(self.pillars)}/{TOTAL_PILLARS} pillars",
            f"Interactions: {len(self.interactions)}",
            f"Blackboard: {'Loaded' if self.blackboard else 'Not loaded'}",
            "",
            "Pillars:"
        ]
        
        for idx in sorted(self.pillars.keys()):
            p = self.pillars[idx]
            lines.append(f"  {idx:2d}: {p.name:15} - {p.core_function}")
        
        return "\n".join(lines)


def main():
    """CLI interface for testing loader."""
    # Set UTF-8 for Windows console
    import sys
    if sys.platform == 'win32':
        sys.stdout.reconfigure(encoding='utf-8')
    
    loader = PillarLoader()
    
    print("Loading all pillars...")
    loader.load_all()
    
    print(loader.summary())
    
    print("\n" + "="*60)
    print("Sample context build (Awareness + Force):")
    print("="*60)
    context = loader.build_context(
        task="Execute precision task with full awareness",
        include_pillars=[0, 2]  # Awareness + Force
    )
    print(context[:2000])  # Print first 2000 chars


if __name__ == "__main__":
    main()
