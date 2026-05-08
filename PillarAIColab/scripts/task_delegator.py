#!/usr/bin/env python3
"""
Task Delegator: Delegate tasks to local Ollama models with Pillar AI context.
Part of Delegate & Audit workflow: Big Pickle routes, local models execute, Big Pickle audits.
Aligns with AGENTS.md mandates and PillarAIColab constraints.
NOW WITH: Dynamic GPU/CPU offload detection via gpu_check.py
"""

import sys
import json
import argparse
import subprocess
import re
from pathlib import Path
from datetime import datetime, timezone
from typing import Optional

if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8')

# Add parent directory to path to import PillarLoader and gpu_check
sys.path.insert(0, str(Path(__file__).parent.parent))
from src.pillar_loader import PillarLoader
from scripts.gpu_check import build_report as gpu_build_report, recommend_models as gpu_recommend

try:
    import ollama
    HAS_OLLAMA = True
except ImportError:
    HAS_OLLAMA = False
    print("Warning: ollama package not installed. Install with: pip install ollama")

# Model configurations
# GPU estimates account for CPU offload: Ollama offloads layers to system RAM when VRAM insufficient
# RTX 3070 Laptop = 8192MB total, ~6973MB usable to Ollama
MODEL_CONFIGS = {
    "qwen2.5:0.5b": {
        "gpu_estimate_mb": 400,   # ~397MB model, fits entirely in GPU
        "task_types": ["simple", "classification", "extraction", "quick_chat"],
        "delta_h": 0.01
    },
    "qwen2.5:1.5b": {
        "gpu_estimate_mb": 1000,  # ~986MB model, fits entirely in GPU
        "task_types": ["chat", "summarization", "basic_analysis"],
        "delta_h": 0.03
    },
    "qwen2.5:3b": {
        "gpu_estimate_mb": 2000,  # ~1.9GB model, fits entirely in GPU
        "task_types": ["code_review", "moderate_analysis", "content_generation"],
        "delta_h": 0.03
    },
    "qwen2.5:7b": {
        "gpu_estimate_mb": 3500,  # ~4.7GB model, partial CPU offload (~1.2GB in system RAM)
        "task_types": ["complex_reasoning", "deep_analysis", "cpp_code_generation"],
        "delta_h": 0.05
    },
    "qwen3.5:latest": {
        "gpu_estimate_mb": 6000,  # ~6.6GB model, minimal CPU offload (~400MB in system RAM)
        "task_types": ["complex_reasoning", "deep_analysis", "cpp_code_generation"],
        "delta_h": 0.07
    },
    # New models from user downloads
    "qwen2.5-coder:1.5b": {
        "gpu_estimate_mb": 1100,
        "task_types": ["code_review", "code_generation", "programming"],
        "delta_h": 0.03
    },
    "mistral:7b": {
        "gpu_estimate_mb": 4500,
        "task_types": ["complex_reasoning", "deep_analysis", "chat"],
        "delta_h": 0.05
    },
    "llama3.2:1b": {
        "gpu_estimate_mb": 1400,
        "task_types": ["simple", "quick_chat", "classification"],
        "delta_h": 0.02
    },
    "gemma4:latest": {
        "gpu_estimate_mb": 10000,  # Exceeds VRAM, requires --model flag
        "task_types": ["complex_reasoning", "thinking", "deep_analysis"],
        "delta_h": 0.06
    },
    "qwen3.6:latest": {
        "gpu_estimate_mb": 24000,  # ~17GB system RAM, ~6GB VRAM
        "task_types": ["complex_reasoning", "thinking", "project_management"],
        "delta_h": 0.09,
        "timeout_ms": 120000,  # Needs 2 min with CPU offload (80% CPU/20% GPU)
        "notes": "FALLBACK PM MODEL - CPU offload, 120s timeout"
    },
    # Recommended models (tested)
    "gpt-oss:20b": {
        "gpu_estimate_mb": 18000,  # MoE model, designed for CPU offload (MXFP4)
        "task_types": ["complex_reasoning", "thinking", "agentic", "project_management"],
        "delta_h": 0.08,
        "timeout_ms": 120000,  # 120s response time (MoE, CPU offload)
        "notes": "OpenAI MoE model - 120s timeout, efficient CPU offload"
    },
    "qwen3:30b-a3b": {
        "gpu_estimate_mb": 20000,  # MoE with 3B active params, long context
        "task_types": ["complex_reasoning", "thinking", "long_context", "project_management"],
        "delta_h": 0.08,
        "timeout_ms": 120000,  # 55s response time (MoE, 3B active params)
        "notes": "Qwen3 MoE - 55s response, 3B active params, excellent for long context"
    }
}

AVAILABLE_MODELS = list(MODEL_CONFIGS.keys())

# qwen3.5:latest is now included - highest capability but uses ~90% VRAM

# C++ generation quality warnings by model (now that ANSI parsing is fixed)
# Smaller models produce working but lower-quality code
CPP_QUALITY_WARNINGS = {
    "qwen2.5:0.5b": "⚠ Basic C++ possible, expect syntax errors & hallucinations (85% fail rate on complex tasks)",
    "qwen2.5:1.5b": "⚠ Simple C++ works, complex tasks may have issues (70% fail rate)",
    "qwen2.5:3b": "✓ Moderate C++ capability, review output carefully (50% fail rate)",
    "qwen2.5:7b": "✓ Good C++ capability with partial CPU offload",
    "qwen3.5:latest": "✓ Best C++ capability but uses ~90% VRAM",
    "qwen2.5-coder:1.5b": "✓ Good C++ capability, code-specialized model",
    "mistral:7b": "✓ Moderate C++ capability",
    "llama3.2:1b": "⚠ Limited C++ capability (small model)",
    "gemma4:latest": "✓ Good C++ capability with thinking mode",
    "qwen3.6:latest": "✓ Best C++ capability with advanced reasoning (PROJECT MANAGER)"
}

# Task type detection keywords
TASK_KEYWORDS = {
    "simple": ["what", "list", "define", "explain simple", "quick"],
    "code_review": ["review", "code", "function", "class", "bug", "fix"],
    "analysis": ["analyze", "evaluate", "assess", "compare", "why"],
    "chat": ["chat", "talk", "tell me", "how to", "what is"],
    "complex": ["generate code", "design", "architect", "optimize", "deep dive"],
    "thinking": ["think", "reason", "step by step", "think through", "reasoning"],
    "programming": ["write code", "implement", "programming", "function", "code"]
}


def should_delegate(task: str, model: str) -> tuple[bool, str]:
    """
    Check if task should be delegated to Ollama based on task type and model.
    Returns (should_delegate, reason_if_not).
    Shows quality warnings for C++ tasks but allows delegation.
    """
    task_lower = task.lower()
    
    # Check for C++ generation tasks (warn but allow)
    generation_indicators = ["write", "create", "implement", "generate"]
    cpp_indicators = [".h", ".cpp", "c++ class", "c++ struct", "c++ function"]
    
    is_generation_task = any(gi in task_lower for gi in generation_indicators)
    has_cpp_indicator = any(ci in task_lower for ci in cpp_indicators)
    
    if is_generation_task and has_cpp_indicator:
        warning = CPP_QUALITY_WARNINGS.get(model, "Unknown model quality")
        print(f"[INFO] C++ Generation Warning for {model}: {warning}")
        return True, warning
    
    return True, ""


def read_file_context(file_path: str, max_lines: int = 100) -> str:
    """Read file content to provide context for delegation."""
    try:
        path = Path(file_path)
        if not path.exists():
            return f"[File not found: {file_path}]"
        lines = path.read_text(encoding='utf-8').split('\n')[:max_lines]
        return f"\n--- Context from {file_path} ---\n" + '\n'.join(lines) + "\n--- End Context ---\n"
    except Exception as e:
        return f"[Error reading {file_path}: {e}]"


def get_gpu_free_memory() -> Optional[int]:
    """Get free GPU memory in MB. Returns None if error."""
    try:
        result = subprocess.run(
            ["nvidia-smi", "--query-gpu=memory.free", "--format=csv,noheader,nounits"],
            capture_output=True, text=True, check=True
        )
        return int(result.stdout.strip().split('\n')[0])
    except Exception as e:
        print(f"Warning: Could not query GPU memory: {e}")
        return None


def detect_task_type(task: str) -> str:
    """Auto-detect task type from keywords."""
    task_lower = task.lower()
    scores = {t: 0 for t in TASK_KEYWORDS}
    for task_type, keywords in TASK_KEYWORDS.items():
        for kw in keywords:
            if kw in task_lower:
                scores[task_type] += 1
    return max(scores.items(), key=lambda x: x[1])[0] if max(scores.values()) > 0 else "chat"


def select_model(task_type: str, gpu_free_mb: Optional[int], override_model: Optional[str] = None, use_dynamic: bool = True) -> str:
    """Select best model based on task type and GPU memory.

    Uses gpu_check.py for dynamic offload detection when use_dynamic=True.
    Falls back to static MODEL_CONFIGS estimates when gpu_check unavailable.
    """
    if override_model:
        if override_model not in AVAILABLE_MODELS:
            raise ValueError(f"Model {override_model} not available. Choose from {AVAILABLE_MODELS}")
        return override_model

    suited_models = [m for m, cfg in MODEL_CONFIGS.items() if task_type in cfg["task_types"]]
    if not suited_models:
        suited_models = AVAILABLE_MODELS

    suited_models.sort(key=lambda m: MODEL_CONFIGS[m]["gpu_estimate_mb"], reverse=True)

    # Dynamic GPU/CPU check
    if use_dynamic and gpu_free_mb is not None:
        try:
            report = gpu_build_report()
            gpu = report.get("gpu")
            recs = gpu_recommend(gpu, report.get("system_ram_gb", 0))
            # Prefer full-GPU models first, then partial-offload
            for model in suited_models:
                rec = recs.get(model, {})
                if rec.get("verdict") == "FULL_GPU":
                    return model
            for model in suited_models:
                rec = recs.get(model, {})
                if rec.get("verdict") == "PARTIAL_OFFLOAD":
                    return model
            return suited_models[-1]
        except Exception:
            pass

    # Fallback: static model config estimates
    if gpu_free_mb is None:
        return suited_models[-1]

    for model in suited_models:
        required = MODEL_CONFIGS[model]["gpu_estimate_mb"] + 500
        if gpu_free_mb >= required:
            return model

    return sorted(MODEL_CONFIGS.items(), key=lambda x: x[1]["gpu_estimate_mb"])[0][0]


def log_to_blackboard(task: str, delta_h: float, status: str, notes: str = ""):
    """Log task to colab_blackboard.md per AGENTS.md mandate."""
    timestamp = datetime.now(timezone.utc).isoformat()
    entry = f"""
### [{timestamp}] task_delegator
- **Task**: {task}
- **ΔH**: {delta_h:.4f}
- **Status**: {status}
- **Notes**: {notes}
"""
    with open(BLACKBOARD_PATH, 'r', encoding='utf-8') as f:
        content = f.read()
    
    log_start = "<!-- LOG_START -->"
    if log_start in content:
        parts = content.split(log_start)
        new_content = parts[0] + log_start + entry + parts[1]
    else:
        new_content = content + entry
    
    with open(BLACKBOARD_PATH, 'w', encoding='utf-8') as f:
        f.write(new_content)


def check_ollama_running() -> bool:
    """Check if Ollama is running."""
    if not HAS_OLLAMA:
        return False
    try:
        ollama.list()
        return True
    except Exception:
        return False


def ensure_model(model: str) -> bool:
    """Ensure model is available, pull if missing."""
    if not HAS_OLLAMA:
        return False
    try:
        response = ollama.list()
        model_names = [m.model for m in response.models]
        if model not in model_names:
            print(f"Model {model} not found. Pulling...")
            ollama.pull(model)
        return True
    except Exception as e:
        print(f"Error with model {model}: {e}")
        return False


def strip_ansi_codes(text: str) -> str:
    """Remove ANSI escape codes from text."""
    return re.sub(r'\x1b\[[?0-9;]*[a-zA-Z]', '', text)


def delegate_task(task: str, model: str, pillar_context: str) -> Optional[str]:
    """Delegate task to Ollama with pillar context, stream response."""
    if not check_ollama_running():
        print("Error: Ollama not running. Start with: ollama serve")
        return None
    if not ensure_model(model):
        return None
    
    messages = [
        {'role': 'system', 'content': pillar_context},
        {'role': 'user', 'content': task}
    ]
    
    print(f"\nDelegating to {model}...\n{'='*60}")
    response_text = ""
    try:
        show_think = False
        stream = ollama.chat(model=model, messages=messages, stream=True)
        in_think = False
        for chunk in stream:
            msg = chunk.get('message', {})
            # Capture thinking trace if toggle is on
            if show_think and msg.get('thinking'):
                if not in_think:
                    sys.stderr.write("\n[THINKING]\n")
                    sys.stderr.flush()
                    in_think = True
                sys.stderr.write(msg['thinking'])
                sys.stderr.flush()
            # Capture content
            content = msg.get('content', '')
            if content:
                if in_think:
                    sys.stderr.write("\n[RESPONSE]\n")
                    sys.stderr.flush()
                    in_think = False
                clean_content = strip_ansi_codes(content)
                print(clean_content, end='', flush=True)
                response_text += clean_content
        print(f"\n{'='*60}")
        return response_text
    except Exception as e:
        print(f"\nError delegating task: {e}")
        return None


def main():
    parser = argparse.ArgumentParser(description="Task Delegator: Pillar AI + Ollama")
    parser.add_argument('--task', required=True, help='Task to delegate to Ollama')
    parser.add_argument('--model', help='Override model selection (e.g., qwen2.5:7b)')
    parser.add_argument('--type', help='Task type (simple, code_review, analysis, chat, complex)')
    parser.add_argument('--file-context', action='append', help='File(s) to include as context (can be repeated)')
    args = parser.parse_args()

    # Pillar Navigation: Perceive
    print("[Perceive] Checking system state (GPU/CPU)...")
    gpu_free = get_gpu_free_memory()

    # Use gpu_check for richer diagnostics
    try:
        report = gpu_build_report()
        gpu = report.get("gpu")
        if gpu and gpu.get("vendor") == "nvidia":
            print(f"  GPU: {gpu['name']} | VRAM: {gpu['vram_free_mb']}MB free / {gpu['vram_total_mb']}MB total")
        elif gpu:
            print(f"  GPU: {gpu.get('name', 'Unknown')}")
        else:
            print(f"  GPU: NONE DETECTED (CPU-only mode)")
        print(f"  System RAM: {report.get('system_ram_gb', 0)} GB")
    except Exception as e:
        print(f"  (gpu_check unavailable: {e})")
        if gpu_free:
            print(f"  GPU free memory (nvidia-smi): {gpu_free} MB")

    # Pillar Navigation: Evaluate
    print("[Evaluate] Checking Ollama availability...")
    if not HAS_OLLAMA:
        print("Error: ollama package not installed.")
        return
    if not check_ollama_running():
        print("Error: Ollama not running. Start with: ollama serve")
        return

    # Pillar Navigation: Align
    print("[Align] Determining task type and model...")
    task_type = args.type or detect_task_type(args.task)
    print(f"Detected task type: {task_type}")
    try:
        model = select_model(task_type, gpu_free, args.model)
    except ValueError as e:
        print(f"Error: {e}")
        return
    print(f"Selected model: {model}")

    # Check if task should be delegated to this model
    can_delegate, reason = should_delegate(args.task, model)
    if not can_delegate:
        print(f"[WARN] Task will NOT be delegated to {model}: {reason}")
        print("[INFO] Implementing directly is more efficient for this task type.")
        log_to_blackboard(
            task=f"Task delegation SKIPPED: {args.task}",
            delta_h=0.0,
            status="SKIPPED",
            notes=f"{reason} (model: {model})"
        )
        return

    # Pillar Navigation: Coordinate
    print("[Coordinate] Loading Pillar AI context...")
    loader = PillarLoader(str(Path(__file__).parent.parent))
    loader.load_all()
    pillar_context = loader.build_context(task=args.task)

    # Add file context if provided
    if args.file_context:
        for file_path in args.file_context:
            pillar_context += read_file_context(file_path)

    # Pillar Navigation: Execute
    print("[Execute] Delegating task...")
    delta_h = MODEL_CONFIGS[model]["delta_h"]
    response = delegate_task(args.task, model, pillar_context)

    # Log to blackboard
    if response:
        log_to_blackboard(
            task=f"Task delegation: {args.task}",
            delta_h=delta_h,
            status="APPROVED",
            notes=f"Model: {model}, Task type: {task_type}"
        )
        print("\nTask delegation complete.")
    else:
        log_to_blackboard(
            task=f"Task delegation failed: {args.task}",
            delta_h=delta_h,
            status="FAILED",
            notes=f"Model: {model}, Task type: {task_type}"
        )
        print("\nTask delegation failed.")


if __name__ == "__main__":
    main()
