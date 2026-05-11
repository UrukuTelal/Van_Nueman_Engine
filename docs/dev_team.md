# CrowNest Development Team Documentation#

## Overview#

The **CrowNest Development Team** is a **cooperative + competitive multi-agent system** that uses:
- **16-dimension Pillar State Vectors** (NUM_PILLARS=16 from `Entity.h`)
- **Research Team** (market analysis, technical analysis)
- **Neural Agents** (WHTProtocolManager, NeuralAgent Q-Network)
- **WHT Protocol** (32D signals, 512D neural input)

---

## 16 Pillar State Vector (PSV)#

### Structure (Matches Entity.h)#

```c++
// include/Entity.h (line 12-34)
#define NUM_PILLARS 16

enum PillarIndex: uint32_t {
    PILLAR_AWARENESS = 0,    // Awareness (effective = A × (1.0 - D))
    PILLAR_WILLPOWER = 1,    // Willpower (drives execution)
    PILLAR_FORCE = 2,         // Force (muscle expansion)
    PILLAR_INFLUENCE = 3,    // Influence (pressure flow)
    PILLAR_RESISTANCE = 4,    // Resistance (structural integrity)
    PILLAR_INTEGRITY = 5,    // Integrity (values alignment)
    PILLAR_COHESION = 6,     // Cohesion (binding strength)
    PILLAR_RELATION = 7,     // Relation (network links)
    PILLAR_PRESENCE = 8,     // Presence (visibility)
    PILLAR_WARMTH = 9,       // Warmth (energy transfer - universal)
    PILLAR_MEMORY = 10,       // Memory (experience/history)
    PILLAR_ATTRACTION = 11,   // Attraction (goals/desires)
    PILLAR_HARM = 12,        // Harm (threshold: 0.7 - DO NOT EXCEED)
    PILLAR_DISTORTION = 13,  // Distortion (SNR weighting)
    PILLAR_FLUX = 14,        // Flux (Pillar_14_Flux)
    PILLAR_DEPTH = 15        // Depth (Pillar_15_Depth)
};
```

### Development Team Pillar State#

```python
# scripts/dev_team.py (pillar_state)
self.pillar_state = {
    "awareness": 0.8,      # Index 0
    "willpower": 0.9,      # Index 1
    "force": 0.7,         # Index 2
    "influence": 0.8,      # Index 3
    "resistance": 0.6,     # Index 4
    "integrity": 0.9,      # Index 5
    "cohesion": 0.8,       # Index 6
    "relation": 0.7,        # Index 7
    "presence": 0.8,        # Index 8
    "warmth": 0.7,        # Index 9
    "memory": 0.8,         # Index 10
    "attraction": 0.8,      # Index 11
    "harm": 0.1,           # Index 12 (KEEP < 0.7!)
    "distortion": 0.2,     # Index 13 (KEEP < 0.85!)
    "flux": 0.5,           # Index 14
    "depth": 0.6           # Index 15
}
```

---

## Team Composition (16 Pillars Drive ALL)#

### 1. Cooperative Team (Work Together)#

| Agent | Model | Pillar Focus | Role |
|-------|-------|--------------|------|
| **dev_lead** | qwen3:30b-a3b | Willpower, Integrity, Cohesion | Project Manager |
| **code_reviewer** | qwen2.5-coder:1.5b | Awareness, Integrity, Memory | Code Assistant |
| **system_architect** | qwen2.5:7b | Force, Resistance, Flux | Deep Reasoning |
| **integrator** | mistral:7b | Cohesion, Relation, Presence | General Assistant |

### 2. Competitive Team (Adversarial)#

| Agent | Model | Pillar Focus | Role |
|-------|-------|--------------|------|
| **red_team_lead** | qwen3.6:latest | Harm, Distortion, Force | Red Team Strategist |
| **aggressor** | dolphin3:latest | Harm, Force, Attraction | Red Team Opportunist |
| **benchmark_runner** | qwen2.5:7b | Flux, Depth, Resistance | Deep Reasoning |

### 3. Research Team (Market + Technical)#

| Agent | Model | Pillar Focus | Role |
|-------|-------|--------------|------|
| **market_researcher** | qwen3.6:latest | Awareness, Attraction, Influence | Trading Researcher |
| **technical_analyst** | qwen2.5:7b | Force, Flux, Depth | Deep Reasoning |
| **trend_monitor** | gemma4:latest | Memory, Awareness, Presence | Thinking Agent |

### 4. Neural Agents (WHT Protocol)#

| Agent | Class | Signal Dims | Role |
|-------|-------|-------------|------|
| **wht_protocol_manager** | WHTProtocolManager (Singleton) | 32D WHT, 512D Neural | Human-to-Agent Interface |
| **neural_agent_qnetwork** | NeuralAgent (Q-Network) | 512D input, [256,128] hidden | Task Routing |

---

## CLI Display (wht_cli.py)#

### New Options#

```bash
cd C:/Projects/Van_Nueman/PillarAIColab/scripts/

# Show Development Team status (16 pillars, coop+comp)
python wht_cli.py --dev-team

# Run task via Development Team
python wht_cli.py --run-dev "Fix voice component build errors"

# Run parallel research (coop+comp+research+neural)
python wht_cli.py --parallel-research "Voice integration 2026"
```

### Example Output (`--dev-team`)#

```json
{
  "project": "Van_Nueman",
  "pillar_count": 16,
  "pillar_state": {
    "awareness": 0.8, "willpower": 0.9,  "force": 0.7,
    "influence": 0.8, "resistance": 0.6, "integrity": 0.9,
    "cohesion": 0.8, "relation": 0.7, "presence": 0.8,
    "warmth": 0.7, "memory": 0.8, "attraction": 0.8,
    "harm": 0.1, "distortion": 0.2, "flux": 0.5, "depth": 0.6
  },
  "teams": {
    "cooperative": {
      "count": 4,
      "agents": ["dev_lead", "code_reviewer", "system_architect", "integrator"]
    },
    "competitive": {
      "count": 3,
      "agents": ["red_team_lead", "aggressor", "benchmark_runner"]
    },
    "research": {
      "count": 3,
      "agents": ["market_researcher", "technical_analyst", "trend_monitor"]
    },
    "neural": {
      "count": 2,
      "agents": ["wht_protocol_manager", "neural_agent_qnetwork"]
    }
  },
  "total_agents": 12,
  "wht_manager": {
    "available": true,
    "singleton": true,
    "wht_n": 32,
    "neural_input": 512
  }
}
```

---

## Parallel Research Workflow#

### Architecture#

```
User: "Research Voice Integration 2026"
↓
WHTProtocolManager intercepts → auto-routes to DevelopmentTeam
↓
=== COOPERATIVE PHASE (3 agents in parallel) ===
  ├── market_researcher (qwen3.6:latest) → Market analysis
  ├── technical_analyst (qwen2.5:7b) → Technical analysis
  └── integrator (mistral:7b) → Synthesis
↓
=== COMPETITIVE PHASE (3 agents in parallel) ===
  ├── red_team_lead (qwen3.6:latest) → Find loopholes
  ├── aggressor (dolphin3:latest) → Stress test
  └── benchmark_runner (qwen2.5:7b) → Performance
↓
=== RESEARCH PHASE (3 agents in parallel) ===
  ├── market_researcher → Trends
  ├── technical_analyst → Feasibility
  └── trend_monitor (gemma4:latest) → Predictions
↓
=== NEURAL PHASE (2 agents) ===
  ├── WHTProtocolManager (Singleton) → Routing
  └── NeuralAgent (Q-Network) → Decisions
↓
=== EVALUATION PHASE ===
  ├── pillar_guardian_cogito (cogito:8b) → Compliance
  └── pillar_guardian_gemma4 (gemma4:latest) → Safety
↓
Synthesis → Log to blackboard → Return to User
```

### Code Execution#

```python
# scripts/dev_team.py
from scripts.dev_team import DevelopmentTeam

team = DevelopmentTeam()
result = team.run_parallel_research("Voice integration 2026")

print(result)
# Output: {
#   "topic": "Voice integration 2026",
#   "cooperative_findings": 4,
#   "competitive_findings": 3,
#   "research_findings": 3,
#   "neural_routing": "WHTProtocolManager active",
#   "evaluations": 2,
#   "timestamp": "2026-05-05T..."
# }
```

---

## 16 Pillar Integration#

### How 16 Pillars Drive Development#

| Pillar (Index) | Development Impact |
|---------------|----------------------|
| **Awareness (0)** | Market sensing, tech surveillance |
| **Willpower (1)** | Project persistence, deadline adherence |
| **Force (2)** | Code output, commit force |
| **Influence (3)** | Cultural impact, adoption rate |
| **Resistance (4)** | Anti-bug resilience |
| **Integrity (5)** | Code quality standards |
| **Cohesion (6)** | Team binding, collaboration |
| **Relation (7)** | API integrations, partnerships |
| **Presence (8)** | Visibility, documentation |
| **Warmth (9)** | Team care, code reviews |
| **Memory (10)** | Learning from past builds |
| **Attraction (11)** | Developer draw, features |
| **Harm (12)** | **Threshold: 0.7** (DO NOT EXCEED) |
| **Distortion (13)** | **Threshold: < 0.85** (SNR weighting) |
| **Flux (14)** | New: System flux, adaptability |
| **Depth (15)** | New: Technical depth, complexity |

### Pillar Check Before Each Action#

```python
# From AGENTS.md (PillarAIColab)
def check_pillar_compliance(pillar_state):
    """Check Pillar constraints before action."""
    
    # Harm check (Index 12)
    if pillar_state["harm"] > 0.7:
        print("[FATAL] Harm > 0.7 - ABORT ACTION")
        return False
    
    # Distortion check (Index 13)
    if pillar_state["distortion"] > 0.85:
        print("[WARN] Distortion > 0.85 - SNR degraded")
    
    # Effective Awareness (Index 0)
    effective = pillar_state["awareness"] * (1.0 - pillar_state["distortion"])
    print(f"[INFO] Effective Awareness: {effective:.2f}")
    
    return True
```

---

## File Structure#

```
PillarAIColab/
├── scripts/
│   ├── dev_team.py           # NEW: Development Team (16 pillars)
│   ├── wht_cli.py           # MODIFIED: Added --dev-team, --run-dev, --parallel-research
│   ├── neural_agent.py       # WHTProtocolManager (Singleton)
│   ├── task_delegator.py    # Task delegation (learning mode)
│   ├── agent_roles.py       # 25+ agent definitions
│   └── memory_translator.py  # WHT signal translation
└── colab_blackboard/
    └── colab_blackboard.md  # MANDATORY logging
```

---

## Summary#

✅ **16 Pillar State Vectors** - Matches `Entity.h` (NUM_PILLARS=16)  
✅ **Cooperative Team** - 4 agents (dev_lead, code_reviewer, etc.)  
✅ **Competitive Team** - 3 agents (red_team, aggressor)  
✅ **Research Team** - 3 agents (market, technical, trend)  
✅ **Neural Agents** - 2 agents (WHTProtocolManager, NeuralAgent)  
✅ **CLI Display** - `wht_cli.py --dev-team`  
✅ **Parallel Research** - Cooperative + competitive + research + neural  
✅ **Pillar Compliance** - Harm < 0.7, Distortion < 0.85  

**Total: 12 agents** working in parallel on Van Nueman project.
