# Pillar AI Configuration Suite

A multimodal configuration system for local AI agents (Ollama/qwen2.5, etc.) implementing the **Pillar-Constrained Multi-Scale Representative Model (PCMSRM)**.

**NEW**: Now with **Neural Agent Routing** and **Workflow Engine** - translates user prompts into optimized multi-agent workflows using WHT signals and deep reinforcement learning.

## Quick Start

### 1. Validate Configs
```bash
python scripts/validate_configs.py
```

### 2. Load Pillars in Python
```python
from src.pillar_loader import PillarLoader

loader = PillarLoader()
loader.load_all()

# Get specific pillar
awareness = loader.get_pillar(0)
print(awareness.to_prompt())

# Build context for LLM
context = loader.build_context(
    task="Optimize team productivity",
    include_pillars=[0, 2, 6]  # Awareness + Force + Cohesion
)
```

### 3. Start WebUI with Neural Routing
```bash
# Install Flask if needed
pip install flask torch

# Start WebUI (http://localhost:5000)
python src/web_ui.py
```
- User prompts are automatically routed to optimal agents via **Neural Routing**
- Creates **WHT signals** from text for neural agent learning
- Uses **AgentRoutingNetwork** (100% test accuracy) to select best agent

### 4. Use Workflow Engine
```python
from scripts.workflow_engine import WorkflowEngine

engine = WorkflowEngine()

# Parse prompt → workflow steps
steps = engine.parse_prompt_to_workflow("Build an auth system with JWT tokens")

# Execute with multi-agent workflow
results = engine.execute_workflow(steps)
print(results)
```

### 5. Use with Ollama
```python
import ollama

# Build context
context = loader.build_context(task="Your task here")

# Send to qwen2.5
response = ollama.chat(model='qwen2.5:0.5b', messages=[
    {'role': 'system', 'content': context},
    {'role': 'user', 'content': 'What pillars should I prioritize for conflict resolution?'}
])
print(response['message']['content'])
```

### 2. Load Pillars in Python
```python
from src.pillar_loader import PillarLoader

loader = PillarLoader()
loader.load_all()

# Get specific pillar
awareness = loader.get_pillar(0)
print(awareness.to_prompt())

# Build context for LLM
context = loader.build_context(
    task="Optimize team productivity",
    include_pillars=[0, 2, 6]  # Awareness + Force + Cohesion
)
```

### 3. Use with Ollama
```python
import ollama

# Build context
context = loader.build_context(task="Your task here")

# Send to qwen2.5
response = ollama.chat(model='qwen2.5:0.5b', messages=[
    {'role': 'system', 'content': context},
    {'role': 'user', 'content': 'What pillars should I prioritize for conflict resolution?'}
])

print(response['message']['content'])
```

## Architecture

```
PillarAIColab/
├── Pillar_00_Awareness/     # 16 pillar folders (00-15)
│   ├── awareness_config.json    # Machine-readable config
│   └── awareness_identity.md    # Human/LLM-readable docs
├── Interactions/
│   └── pillar_interactions.md   # Cross-pillar emergent behaviors
├── colab_blackboard/
│   └── colab_blackboard.md      # Agent governance protocol
├── schema/
│   └── pillar_config_schema.json  # JSON schema for validation
├── Pillar_10_Memory/
│   ├── memory_config.json       # Memory system config (WHT enabled)
│   └── signals/                 # WHT signals (58+ stored)
├── models/
│   ├── routing_network.pth     # Neural routing model (100% accuracy)
│   └── neural_agent.pth        # Custom RL agent (Q-Network)
├── workflows/                   # Stored workflow definitions
├── src/
│   ├── pillar_loader.py         # Main API for loading configs
│   ├── pillar_defaults.json     # Fallback defaults
│   ├── ollama_example.py        # Ollama integration examples
│   ├── ollama_wrapper.py       # Advanced wrapper with streaming
│   ├── web_ui.py                # Web UI with neural routing
│   └── web_ui_simple.py        # Lightweight web UI (no Flask)
└── scripts/
    ├── validate_configs.py      # Config validation script
    ├── dependency_graph.py      # Pillar dependency visualization
    ├── citation_db.py           # Citation extraction tool
    ├── build_rag.py             # RAG vector store builder
    ├── simulate.py              # Pillar interaction simulator
    ├── memory_translator.py     # Text → WHT signals (512D)
    ├── agent_dataset.py          # Training data from blackboard
    ├── agent_learner.py         # Neural routing network (PyTorch)
    ├── neural_agent.py           # Custom RL agent (Deep Q-Learning)
    ├── agent_roles.py           # 8 agent roles + neural routing
    ├── workflow_engine.py        # Prompt → Workflow translator
    ├── model_qualifiers.py     # 12 model speed/resource ratings
    ├── validate_models.py       # Validates all 12 Ollama models
    └── test_large_model.py      # Iterative timeout testing
```

### Neural Agent System Flow
```
User Prompt → WebUI → Neural Agent (RL) → Agent Routing Network
                                    ↓
                              Selects Optimal Agent Role
                                    ↓
                              Agent's Model (Ollama) → Response
                                    ↓
                              WHT Signal Storage → Neural Learning
```


## The 16 Pillars

| Index | Name | Core Function |
|-------|------|---------------|
| 0 | Awareness | Intelligence / Analytics |
| 1 | Willpower | Strategic Persistence |
| 2 | Force | Execution / Production |
| 3 | Influence | Communication / Culture |
| 4 | Resistance | Risk / Compliance / Stabilization |
| 5 | Integrity | Identity / Standards |
| 6 | Cohesion | Team Structure / Binding Force |
| 7 | Relation | Networking / Dependencies |
| 8 | Presence | Priority / Visibility |
| 9 | Warmth | HR / Support Systems |
| 10 | Memory | Knowledge Systems / Learning |
| 11 | Attraction | Incentives / Directional Pull |
| 12 | Harm | Disruption Detection / Transformation |
| 13 | Distortion | Information Decay / Perception Subversion |
| 14 | Flux | Throughput / Transfer Bandwidth |
| 15 | Depth | Latent Capacity / Resilience |

## How Local LLMs Use This

### Context Window Strategy
Small LLMs (qwen2.5:0.5b, 1.5b) have ~4K-8K token context. Use selectively:

```python
# Full context (all pillars) - ~3-4K tokens
full_ctx = loader.build_context()

# Targeted context (specific pillars) - ~500 tokens
targeted_ctx = loader.build_context(
    task="Resolve team conflict",
    include_pillars=[4, 5, 7, 12]  # Resistance + Integrity + Relation + Harm
)
```

### RAG Pattern
1. **Load** pillar configs at startup
2. **Build context** based on user task
3. **Inject** into system prompt
4. **Query** LLM with pillar-aware reasoning

### Example: Pillar-Aware Prompting
```python
system_prompt = loader.build_context(task="Analyze organizational health")

user_query = """
Using the pillar framework, analyze this scenario:
- Team has high Force but low Warmth
- What failure modes should I watch for?
- Which pillars need intervention?
"""

response = ollama.chat(model='qwen2.5:1.5b', messages=[
    {'role': 'system', 'content': system_prompt},
    {'role': 'user', 'content': user_query}
])
```

## Net Value Equation

The economic model underlying pillar interactions:

```
V_net = (P × S × W / C) - E - I - R - B - λH

Where:
- P = Profit (Force, Influence, Relation)
- S = Scale (Presence, Cohesion)
- W = Weight (Integrity, Memory, Relation)
- C = Cost (Resistance, Distortion)
- E = Employment (Warmth, Force)
- I = Investment (Attraction, Willpower, Memory)
- R = Risk (Resistance, Awareness)
- B = Buffer (Memory, Resistance)
- H = Harm (minimized via Awareness, Integrity)
- λ = Harm penalty coefficient
```

## Validation

Run validation before deploying:

```bash
# Validate all configs
python scripts/validate_configs.py

# Expected output:
# [OK] All 16 pillar configs are valid
# [OK] All 16 pillar indices (0-15) present
```

## Citation System

Identity markdown files contain `[cite: XX]` markers for:
- **RAG lookup**: Extract citations for LLM fact-checking
- **Human reference**: Trace design decisions
- **LLM reasoning**: Models can cite sources in responses

Citations are **preserved in markdown** (for context) but **stripped from JSON** (for parsing).

## API Reference

### PillarLoader

| Method | Description |
|--------|-------------|
| `load_all()` | Load all 16 pillars |
| `get_pillar(index)` | Get pillar by index |
| `get_pillar_by_name(name)` | Get pillar by name |
| `get_interaction(a, b)` | Get interaction between two pillars |
| `build_context(task, include_pillars)` | Build LLM context string |
| `build_state_vector()` | Get normalized pillar state (0.0-1.0) |
| `summary()` | Get text summary of loaded pillars |

### PillarConfig

| Method | Description |
|--------|-------------|
| `to_dict()` | Convert to dictionary |
| `to_prompt()` | Format as LLM-readable text |

## Troubleshooting

### "Invalid JSON" errors
```bash
# Check specific file
python -c "import json; json.load(open('Pillar_00_Awareness/awareness_config.json'))"
```

### Missing pillar indices
```bash
# Validator will report gaps
python scripts/validate_configs.py
```

### UTF-8 encoding on Windows
```python
import sys
if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8')
```

## Next Steps

1. **Install Ollama**: `https://ollama.com/download` then `ollama pull qwen2.5:0.5b`
2. **Install Dependencies**: `pip install torch flask numpy`
3. **Run RAG**: `python scripts/build_rag.py --build`
4. **Start Web UI with Neural Routing**: `python src/web_ui.py` (then open http://localhost:5000)
   - Prompts auto-routed to optimal agents (code_assistant, project_manager, etc.)
   - Watch routing decisions in real-time
5. **Try Workflow Engine**: Send complex tasks → multi-agent workflows
6. **Run Simulation**: `python scripts/simulate.py --scenario high_force_low_resistance`

## Neural Agent Features

| Feature | Description |
|---------|-------------|
| **AgentRoutingNetwork** | PyTorch neural network (512D input → 8 agent outputs) |
| **Training Data** | 53 blackboard entries → WHT signals (100% test accuracy) |
| **Neural Agent (RL)** | Custom agent using Deep Q-Learning with experience replay |
| **WHT Signals** | Text converted to 512D Walsh-Hadamard Transform signals |
| **Epsilon-Greedy** | Exploration (100% → 1%) with decay for balanced learning |
| **WebUI Integration** | Prompts routed via neural networks to optimal agents |
| **Workflow Engine** | Translates prompts into multi-agent workflows |
| **Ollama Models** | 8 specialized agents (llama3.2, qwen-coder, mistral, etc.) |

## Model Qualifiers (Tested)

| Tier | Model | Speed | Response Time | Role |
|------|-------|-------|--------------|
| Small | `llama3.2:1b` | Ultra-fast | ~3s | Quick Chat |
| Small | `qwen2.5-coder:1.5b` | Fast | ~8s | Code Assistant |
| Medium | `mistral:7b` | Moderate | ~8s | General Assistant |
| Medium | `qwen2.5:7b` | Moderate | ~12s | Deep Reasoning |
| Large | `qwen3:30b-a3b` | Slow (MoE) | ~55s | Project Manager (DEFAULT) |
| Large | `gemma4:latest` | Very Slow | ~20s | Thinking Agent |

## Completed Tools

| Script | Description |
|--------|-------------|
| `scripts/validate_configs.py` | Validates all 16 pillar configs (stdlib-only) |
| `scripts/dependency_graph.py` | Generates pillar dependency graph (text/DOT/Mermaid) |
| `scripts/citation_db.py` | Extracts [cite: XX] references from markdown |
| `scripts/build_rag.py` | Builds vector store for semantic search |
| `scripts/simulate.py` | Simulates pillar interactions over time |
| `src/ollama_example.py` | Basic Ollama integration (analyze/chat/interactive) |
| `src/ollama_wrapper.py` | Advanced wrapper with streaming + context management |
| `src/web_ui_simple.py` | Web UI using built-in http.server |

## License

Open for local AI experimentation.
