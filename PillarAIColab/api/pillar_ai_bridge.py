"""
PillarAIColab Bridge for Van Nueman Project
Consolidated within the PillarAIColab submodule.
Integrates PillarAIColab constraints with the project's AI systems.
"""

import sys
import os
from pathlib import Path
from datetime import datetime, timezone

# Fix Windows encoding for UTF-8
if sys.platform == 'win32':
    os.environ['PYTHONIOENCODING'] = 'utf-8'
    try:
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except:
        pass

# PillarAIColab base path is this package's parent directory
PILLAR_COLAB_PATH = Path(__file__).parent.parent
sys.path.insert(0, str(PILLAR_COLAB_PATH / "src"))

# Blackboard log path
BLACKBOARD_PATH = PILLAR_COLAB_PATH / "colab_blackboard" / "colab_blackboard.md"

try:
    from pillar_loader import PillarLoader
except ImportError:
    print(f"Warning: Cannot import PillarLoader from {PILLAR_COLAB_PATH}")
    PillarLoader = None


def log_to_blackboard(entry_dict):
    """
    MANDATORY: Append a log entry to the blackboard for cross-session continuity.
    Also translates entry to WHT signal for efficient storage.
    
    entry_dict must contain:
    - agent_id: str (e.g., "opencode_session_001", "project_ai_neuro")
    - task: str (description of action)
    - delta_h: float (harm calculation)
    - approved: bool (whether action was approved)
    - pillar_state: dict (optional, pillar values before/after)
    - failure_modes: list (optional, warnings detected)
    - notes: str (optional, additional context)
    """
    # NEW: Also translate to WHT signal
    try:
        from scripts.memory_translator import MemoryTranslator
        translator = MemoryTranslator()
        text = f"{entry_dict.get('task', '')} - {entry_dict.get('notes', '')}"
        signal, metadata = translator.translate_memory_entry(text)
        if signal is not None:
            translator.store_signal(signal, metadata)
    except Exception as e:
        print(f"[WARN] WHT translation failed: {e}")
    timestamp = datetime.now(timezone.utc).isoformat()
    agent_id = entry_dict.get("agent_id", "unknown_agent")
    task = entry_dict.get("task", "unspecified_task")
    delta_h = entry_dict.get("delta_h", 0.0)
    approved = entry_dict.get("approved", True)
    failure_modes = entry_dict.get("failure_modes", [])
    notes = entry_dict.get("notes", "")
    
    # Build log entry
    status = "APPROVED" if approved else "REJECTED"
    log_lines = [
        f"### [{timestamp}] {agent_id}",
        f"- **Task**: {task}",
        f"- **ΔH**: {delta_h:.4f}",
        f"- **Status**: {status}",
    ]
    
    if failure_modes:
        log_lines.append(f"- **Failure Modes**: {', '.join(failure_modes)}")
    
    if notes:
        log_lines.append(f"- **Notes**: {notes}")
    
    log_lines.append("")  # Blank line separator
    
    log_entry = "\n".join(log_lines)
    
    # Read current blackboard
    with open(BLACKBOARD_PATH, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Insert new entry after <!-- LOG_START --> marker
    marker = "<!-- LOG_START -->"
    if marker in content:
        parts = content.split(marker, 1)
        new_content = parts[0] + marker + "\n\n" + log_entry + parts[1]
    else:
        # Fallback: append at end
        new_content = content + "\n\n" + log_entry
    
    # Write back
    with open(BLACKBOARD_PATH, 'w', encoding='utf-8') as f:
        f.write(new_content)
    
    return True


class PillarAIEngine:
    """Bridge between PillarAIColab constraints and Van Nueman project AI."""
    
    def __init__(self, colab_path=None):
        self.colab_path = Path(colab_path) if colab_path else PILLAR_COLAB_PATH
        self.loader = None
        self.pillars_loaded = False
        
    def load_pillars(self):
        """Load all 16 pillars from PillarAIColab."""
        if PillarLoader is None:
            raise ImportError("PillarLoader not available")
        
        self.loader = PillarLoader(str(self.colab_path))
        self.loader.load_all()
        self.pillars_loaded = True
        return self.loader.summary()
    
    def get_context_for_task(self, task_description, include_pillars=None):
        """Build LLM context for a specific task."""
        if not self.pillars_loaded:
            self.load_pillars()
        
        return self.loader.build_context(
            task=task_description,
            include_pillars=include_pillars
        )
    
    def get_pillar_state_vector(self):
        """Get current pillar state vector (normalized 0-1)."""
        if not self.pillars_loaded:
            self.load_pillars()
        
        return self.loader.build_state_vector()
    
    def calculate_harm_delta(self, action_description, resistance=0.5, integrity=0.5):
        """
        Calculate predicted harm delta for an action.
        Returns (delta_h, should_proceed)
        """
        if not self.pillars_loaded:
            self.load_pillars()
        
        harm_pillar = self.loader.get_pillar(12)  # Harm
        if harm_pillar:
            threshold = harm_pillar.operational_constants.get("transformation_threshold", 0.7)
            incoming_damage = 0.3
            delta_h = max(0, incoming_damage - resistance - integrity)
            should_proceed = delta_h <= threshold
            return delta_h, should_proceed
        
        return 0.0, True
    
    def check_failure_modes(self, active_pillars):
        """
        Check if current pillar combination creates failure modes.
        active_pillars: dict of {pillar_name: value}
        """
        warnings = []
        
        if active_pillars.get("Distortion", 0) > 0.5 and active_pillars.get("Influence", 0) > 0.5:
            warnings.append("PROPAGANDA RISK: High Distortion + Influence detected")
        
        if active_pillars.get("Distortion", 0) > 0.5 and active_pillars.get("Presence", 0) > 0.8:
            warnings.append("MASKED INCOMPETENCE: High Presence hiding Distortion")
        
        if active_pillars.get("Warmth", 0) > 0.7 and active_pillars.get("Force", 0) < 0.3:
            warnings.append("COMPLACENCY: High comfort, low execution")
        
        return warnings
    
    def apply_to_taichi_simulation(self, pillars_sim):
        """
        Apply PillarAIColab constraints to Taichi-based PillarsSimulation.
        pillars_sim: instance of PillarsSimulation from pillars_api.py
        """
        if not self.pillars_loaded:
            self.load_pillars()
        
        config_mapping = {}
        for idx, pillar in self.loader.pillars.items():
            config_mapping[pillar.name.lower()] = {
                "index": idx,
                "constants": pillar.operational_constants,
                "mechanics": pillar.logic_mechanics
            }
        
        return config_mapping


def verify_pillar_constraints(task, agent_id="project_ai", log_entry=True):
    """
    MANDATORY: Verify pillar constraints before executing a task.
    Follows the 5-step Agent Navigation Logic.
    Automatically logs to blackboard unless log_entry=False.
    """
    engine = PillarAIEngine()
    engine.load_pillars()
    
    awareness = engine.loader.get_pillar(0)
    snr_threshold = awareness.operational_constants.get("snr_threshold", 0.5)
    
    delta_h, should_proceed = engine.calculate_harm_delta(task)
    failure_modes = engine.check_failure_modes(engine.get_pillar_state_vector())
    
    result = {
        "approved": should_proceed,
        "delta_h": delta_h,
        "failure_modes": failure_modes,
        "context": engine.get_context_for_task(task)
    }
    
    if not should_proceed:
        result["reason"] = f"Predicted harm delta {delta_h} exceeds threshold"
    
    if log_entry:
        log_to_blackboard({
            "agent_id": agent_id,
            "task": task,
            "delta_h": delta_h,
            "approved": should_proceed,
            "failure_modes": failure_modes,
            "notes": result.get("reason", "")
        })
    
    return result


if __name__ == "__main__":
    engine = PillarAIEngine()
    print(engine.load_pillars())
    print("\n" + "="*60)
    print("Context for neuroevolution optimization:")
    print("="*60)
    print(engine.get_context_for_task("Optimize neuroevolution parameters"))
