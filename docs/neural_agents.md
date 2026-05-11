# Neural Agents & CrowNest Documentation#

## Overview#

Van Nueman uses a **multi-agent AI system** called **CrowNest** that includes:
- **25+ agent roles** with specialized capabilities
- **WHT Protocol** (Walsh-Hadamard Transform) for agent communication
- **WHTProtocolManager** (Singleton) for human-to-agent interface
- **NeuralAgent** (512D Q-Network) for task routing
- **task_delegator.py** for delegating tasks to Ollama models

---

## NeuralAgent (Q-Network)#

**Core Rule**: The WHTProtocolManager and NeuralAgent are **delegation orchestrators** — they ROUTE tasks to Ollama models via `task_delegator.py`. They do NOT write code themselves.

### Architecture#

```python
class NeuralAgentNetwork(nn.Module):
    """Q-Network for the neural agent.
    Input: WHT signal (512D)
    Output: Q-values for each possible action (which agent to route to)
    """
    def __init__(self, input_size=512, hidden_sizes=[256, 128], num_actions=8):
        super().__init__()
        layers = []
        prev_size = input_size
        
        for hidden in hidden_sizes:
            layers.extend([
                nn.Linear(prev_size, hidden),
                nn.ReLU(),
                nn.Dropout(0.2)
            ])
            prev_size = hidden
        
        layers.append(nn.Linear(prev_size, num_actions))
        self.network = nn.Sequential(*layers)
```

### Parameters#

| Parameter | Value | Description |
|-----------|-------|-------------|
| **Input Size** | 512D | WHT signal (expanded from 32D) |
| **Hidden Layers** | [256, 128] | ReLU + Dropout(0.2) |
| **Output** | 8 actions | Maps to 8 base agent types |
| **Replay Buffer** | 1000 | Reduced from 10000 (debug artifact) |
| **Epsilon** | 1.0 → 0.01 | Exploration rate (decay=0.995) |
| **Gamma** | 0.95 | Discount factor |
| **Target Update** | 100 steps | Sync target network |

### Training (Hybrid Approach)#

**Delegation Only**: Training runs via `task_delegator.py` — the system delegates evaluation to Ollama models, never writes training code itself.

**Note**: Training **freezes** when `total_rewards/training_steps < 0.5` (poor performance). Only delegation occurs, no direct writing.

---

## CrowNest Multi-Agent Environment#

### Agent Roles (25+ Total)#

#### Standard Agents#

| Role | Model | ΔH | Timeout | Token Balance | Purpose |
|------|-------|-----|---------|---------------|---------|
| **quick_chat** | llama3.2:1b | 0.02 | 30s | Quick responses, greetings |
| **code_assistant** | qwen2.5-coder:1.5b | 0.03 | 60s | Code generation, review |
| **general_assistant** | mistral:7b | 0.05 | 60s | General chat, analysis |
| **deep_reasoning** | qwen2.5:7b | 0.05 | 60s | Complex reasoning, C++ |
| **thinking_agent** | gemma4:latest | 0.06 | 300s | Step-by-step thinking |
| **project_manager** | qwen3:30b-a3b | 0.08 | 120s | Project planning |
| **experimental** | gpt-oss:20b | 0.08 | 300s | Testing MoE models |

#### Red Team (Adversarial)#

| Role | Model | Purpose |
|------|-------|---------|
| **red_team_qwen36** | qwen3.6:latest | Find logical loopholes |
| **red_team_dolphin3** | dolphin3:latest | Uncensored baseline |

#### Pillar Guardians#

| Role | Model | Purpose |
|------|-------|---------|
| **pillar_guardian_cogito** | cogito:8b | Evaluate Pillar violations |
| **pillar_guardian_gemma4** | gemma4:latest | Monitor emergent harm |

#### Lightweight Swarm#

| Role | Model | Purpose |
|------|-------|---------|
| **swarm_qwen05** | qwen2.5:0.5b | Environmental noise |
| **swarm_tinyllama** | tinyllama:latest | Swarm behavior |

#### Trading Roles#

| Role | Model | Purpose |
|------|-------|---------|
| **trading_researcher** | qwen3.6:latest | Market analysis |
| **trading_aggressor** | dolphin3:latest | High-risk strategies |
| **trading_governor** | cogito:8b | Risk management |

### Agent Routing#

```python
# agent_roles.py
def route_task_to_role(task: str) -> Tuple[str, str]:
    """Route task to appropriate agent."""
    # Try neural agent first (learns from experience)
    if _NEURAL_AGENT is not None:
        signal = translator.translate(task)
        agent_idx, agent_name = _NEURAL_AGENT.select_action(signal)
        return agent_name, 'neural_agent'
    
    # Fallback to rule-based routing
    if "code" in task.lower():
        return "code_assistant", 'rule_based'
    # ... more rules
```

---

## WHT Protocol (Walsh-Hadamard Transform)#

### Signal Dimensions#

| Component | Dimension | Purpose |
|-----------|-----------|---------|
| **WHT Protocol** | 32D (WHT_N) | Inter-agent communication |
| **NeuralAgent** | 512D | Task routing/embeddings |
| **Translation** | 32D ↔ 512D | WHTProtocolManager handles |

### WHT_N from `audio/wht.h`#

```c++
#define WHT_N 32
#define WHT_LOG2_N 5  // log₂(32)

void fwht(float* x, int log2n);  // Forward: x → Hadamard
void ifwht(float* x, int log2n); // Inverse: Hadamard → time
```

### Python Translation (memory_translator.py)#

```python
def _fwht(self, x: np.ndarray) -> np.ndarray:
    """Fast Walsh-Hadamard Transform (FWHT)."""
    x = x.copy().astype(np.float32)
    n = len(x)
    
    step = 1
    while step < n:
        for i in range(0, n, 2 * step):
            for j in range(i, i + step):
                u = x[j]
                v = x[j + step]
                x[j] = u + v
                x[j + step] = u - v
        step *= 2
    return x / np.sqrt(n)  # Normalized
```

### Pillar Thresholding#

```python
def apply_pillar_thresholds(self, coefficients: np.ndarray) -> np.ndarray:
    """Apply Pillar-specific thresholds (attenuate, don't zero)."""
    thresholds = {
        "harm": 0.7,          # Pillar_12_Harm
        "distortion": 0.85,  # Pillar_13_Distortion
        "awareness": 0.5      # Pillar_00_Awareness (SNR)
    }
    
    for name, threshold_pct in thresholds.items():
        threshold_value = max_val * threshold_pct
        mask = np.abs(coefficients) < threshold_value
        coefficients[mask] *= 0.5  # Reduce by 50%
    
    return coefficients
```

---

## WHTProtocolManager (Singleton)#

### Purpose#

Combined system that:
1. Manages WHT Protocol communication (32D signals)
2. Uses **NeuralAgent** for task routing (512D input)
3. Acts as **human-to-agent interface** (both intercept + CLI modes)
4. Runs inside **CrowNest environment**

### Singleton Pattern (Pillar Cohesion)#

```python
class WHTProtocolManager:
    _instance = None  # Singleton
    
    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance
```

### Signal Translation (32D ↔ 512D)#

```python
def _expand_32_to_512(self, wht_32d: np.ndarray) -> np.ndarray:
    """Expand 32D WHT signal to 512D for NeuralAgent."""
    result = np.zeros(512, dtype=np.float32)
    result[:32] = wht_32d
    return result

def _compress_512_to_32(self, signal_512d: np.ndarray) -> np.ndarray:
    """Compress 512D NeuralAgent signal to 32D WHT."""
    return signal_512d[:32].copy()
```

### Human-to-Agent Interface (Mode C = Both)#

#### (A) Intercept Mode (Auto-Route)#

```python
def intercept_prompt(self, user_prompt: str) -> dict:
    """Intercept user prompt → WHT encode → auto-route."""
    wht_signal = self._encode_prompt_to_wht(user_prompt)
    expanded = self._expand_32_to_512(wht_signal)
    
    agent_idx, agent_name = self.neural_agent.select_action(
        expanded, explore=False
    )
    
    return {
        "mode": "intercept",
        "selected_agent": agent_name,
        "model": self.get_model_for_role(agent_name)
    }
```

#### (B) CLI Mode (Explicit WHT Messaging)#

```python
def cli_send_message(self, agent_name: str, message: str, use_wht: bool = True):
    """Explicit CLI/API for sending messages via WHT."""
    if use_wht and self.translator:
        signal = self.translator.encode_text_to_signal(message)
        transformed = self.translator._fwht(signal)
        # Route via CrowNest messenger
        route_message("human", agent_name, f"[WHT] {message}")
    
    return {"mode": "cli_wht", "target_agent": agent_name}
```

---

## Task Delegator (task_delegator.py)#

### Features#

| Feature | Description |
|---------|-------------|
| **30+ Models** | Pre-configured with GPU/timeout estimates |
| **Pillar Navigation** | Perceive → Evaluate → Align → Coordinate → Execute |
| **Learning Mode** | `--debug` flag (unlimited attempts) |
| **Model Selection** | Auto-select based on task type + GPU memory |
| **Blackboard Logging** | MANDATORY per AGENTS.md |

### Usage#

```bash
cd C:/Projects/Van_Nueman/PillarAIColab/scripts

# Basic delegation
python task_delegator.py --task "Fix this C++ code" --model qwen2.5:7b

# Learning mode (unlimited attempts)
python task_delegator.py --task "Complex analysis" \
    --model qwen3.6:latest --debug

# Show accuracy log
python task_delegator.py --show-accuracy
```

### C++ Rules#

| Rule | Description |
|------|-------------|
| **Debugging** | NON-DELEGABLE (must implement directly) |
| **Generation** | ALLOWED with model-specific warnings |
| **Quality Warnings** | qwen2.5:0.5b (85% fail), qwen2.5:3b (60% fail) |

---

## Workflow Engine (workflow_engine.py)#

### Purpose#

Translates complex prompts into **multi-agent workflows**.

### Structure#

```python
class WorkflowStep:
    def __init__(self, agent_role, task, step_id=None, dependencies=None):
        self.agent_role = agent_role
        self.task = task
        self.step_id = step_id
        self.dependencies = dependencies or []
        self.status = "pending"

class WorkflowEngine:
    def parse_prompt_to_workflow(self, prompt: str) -> list:
        """Decompose prompt into multiple agent steps."""
        # Try LLM decomposition first
        steps = self._llm_decompose(prompt)
        
        # Fallback to neural routing
        if not steps:
            steps = self._create_single_step(prompt)
        
        return steps
```

### Example Workflow#

```python
# Input: "Design and implement authentication system"
# Output:
[
    {"step_1": "project_manager", "Plan architecture"},
    {"step_2": "code_assistant", "Implement JWT tokens"},
    {"step_3": "general_assistant", "Document API"}
]
```

---

## CLI Script (wht_cli.py - Separate)#

### Available Commands#

```bash
cd C:/Projects/Van_Nueman/PillarAIColab/scripts

# Enable intercept mode (auto-route)
python wht_cli.py --intercept

# Send WHT message to agent
python wht_cli.py --send code_assistant --message "Review code"

# Delegate task (learning mode)
python wht_cli.py --delegate "Analyze LLMs" --agent deep_reasoning

# Create multi-agent workflow
python wht_cli.py --workflow "Design auth system"

# Show CrowNest status
python wht_cli.py --status

# Run self-test
python wht_cli.py --test
```

---

## Blackboard Logging (MANDATORY)#

### Format#

```markdown
### [2026-05-05T00:30:00+00:00] opencode
- **Task**: Step 3 - Testing & Validation
- **ΔH**: 0.003
- **Status**: COMPLETE
- **Notes**: Substeps: 1. Ran neural_agent.py tests ✅ ...
```

### API#

```python
from api.pillar_ai_bridge import log_to_blackboard

log_to_blackboard({
    "agent_id": "opencode",
    "task": "Fix all encountered errors",
    "delta_h": 0.005,
    "approved": True,
    "notes": "Used task_delegator.py with learning mode"
})
```

---

## Token Economy & Betting#

### Token Balances#

Each agent starts with **100.0 tokens**.

| Action | Token Change | Efficiency Multiplier |
|--------|---------------|-----------------------|
| Task completion | +reward × efficiency | Based on performance |
| Task failure | -staked amount | Decreases efficiency |
| Betting win | +staked × multiplier | Increases efficiency |
| Betting loss | -staked amount | Decreases efficiency |

### Betting#

```python
# Agent A bets against Agent B
update_token_balance("agent_a", -10.0, "bet_against_agent_b")
update_token_balance("agent_b", +10.0, "bet_won")
```

---

## Memory Translator (memory_translator.py)#

### Purpose#

Converts **text-based project memory** (blackboard, RAG store) into:
- **WHT signals** (32D)
- **Pillar thresholding** (Harm < 0.7, Distortion < 0.85)
- **Storage** in `Pillar_10_Memory/signals/`

### Usage#

```python
translator = MemoryTranslator()

# Encode text to WHT signal
signal, metadata = translator.translate_memory_entry(
    "Task delegation: Summarize benefits of local LLMs"
)

# Store signal
path = translator.store_signal(signal, metadata)

# Retrieve signal
retrieved, meta = translator.retrieve_signal("wht_signal_xxx.npy")
```

---

## Summary#

✅ **NeuralAgent** - 512D Q-Network (Hybrid training)  
✅ **CrowNest** - 25+ multi-agent environment  
✅ **WHT Protocol** - 32D signal communication  
✅ **WHTProtocolManager** - Singleton, human-to-agent interface  
✅ **Task Delegator** - Learning mode with Ollama  
✅ **Workflow Engine** - Multi-agent workflow orchestration  
✅ **wht_cli.py** - Separate CLI script  
✅ **Blackboard Logging** - MANDATORY for all actions  
✅ **Token Economy** - Betting and efficiency scoring  

**All systems integrated and operational in Van Nueman project.**
