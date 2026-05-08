#!/usr/bin/env python3
"""
Model Qualifiers - Based on Test Findings.
Qualifies models by speed, resource usage, and suitability for tasks.
"""
from typing import Dict, List, Any

# Model qualifiers based on test findings (RTX 3070 Laptop: 8GB VRAM)
MODEL_QUALIFIERS = {
    # Small models (Full GPU, Fast)
    "qwen2.5:0.5b": {
        "tier": "small",
        "speed": "fast",  # ~5s response
        "gpu_usage": "100%",
        "cpu_offload": False,
        "response_time_s": 5,
        "best_for": ["simple", "quick_chat", "classification"],
        "avoid_for": ["complex_reasoning", "long_context"],
        "qualifier": "ULTRA-FAST - Fits entirely in GPU, ideal for quick tasks"
    },
    "qwen2.5:1.5b": {
        "tier": "small",
        "speed": "fast",  # ~5-8s response
        "gpu_usage": "100%",
        "cpu_offload": False,
        "response_time_s": 8,
        "best_for": ["chat", "summarization", "basic_analysis"],
        "avoid_for": ["complex_reasoning", "deep_analysis"],
        "qualifier": "FAST - Good balance of speed and capability"
    },
    "llama3.2:1b": {
        "tier": "small",
        "speed": "ultra-fast",  # ~3s response
        "gpu_usage": "100%",
        "cpu_offload": False,
        "response_time_s": 3,
        "best_for": ["simple", "quick_chat", "classification"],
        "avoid_for": ["code_generation", "complex_tasks"],
        "qualifier": "LIGHTNING FAST - Smallest model, ultra-low latency"
    },
    "qwen2.5-coder:1.5b": {
        "tier": "small",
        "speed": "fast",  # ~5-8s response
        "gpu_usage": "100%",
        "cpu_offload": False,
        "response_time_s": 8,
        "best_for": ["code_review", "code_generation", "programming"],
        "avoid_for": ["complex_reasoning", "deep_analysis"],
        "qualifier": "CODE SPECIALIST - Optimized for programming tasks"
    },
    
    # Medium models (Full GPU, Moderate speed)
    "mistral:7b": {
        "tier": "medium",
        "speed": "moderate",  # ~8s response
        "gpu_usage": "100%",
        "cpu_offload": False,
        "response_time_s": 8,
        "best_for": ["chat", "complex_reasoning", "deep_analysis"],
        "avoid_for": ["ultra_low_latency"],
        "qualifier": "BALANCED - Good general-purpose model, fits in GPU"
    },
    "qwen2.5:3b": {
        "tier": "medium",
        "speed": "moderate",  # ~10s response
        "gpu_usage": "100%",
        "cpu_offload": False,
        "response_time_s": 10,
        "best_for": ["code_review", "moderate_analysis", "content_generation"],
        "avoid_for": ["ultra_low_latency"],
        "qualifier": "MODERATE - Good for content generation and analysis"
    },
    "qwen2.5:7b": {
        "tier": "medium",
        "speed": "moderate",  # ~12s response
        "gpu_usage": "100%",
        "cpu_offload": False,  # Minimal (~1.2GB in system RAM)
        "response_time_s": 12,
        "best_for": ["complex_reasoning", "deep_analysis", "cpp_code_generation"],
        "avoid_for": ["ultra_low_latency"],
        "qualifier": "CAPABLE - Strong reasoning, minimal offload"
    },
    "qwen3.5:latest": {
        "tier": "medium",
        "speed": "moderate",  # ~15s response
        "gpu_usage": "~95%",
        "cpu_offload": True,  # ~400MB in system RAM
        "response_time_s": 15,
        "best_for": ["complex_reasoning", "deep_analysis", "cpp_code_generation"],
        "avoid_for": ["ultra_low_latency"],
        "qualifier": "HIGH-CAPABILITY - Best C++ capability, near-full GPU"
    },
    
    # Large models (CPU offload, Slow)
    "gemma4:latest": {
        "tier": "large",
        "speed": "slow",  # 20s (GPU) / 260s+ (offloaded)
        "gpu_usage": "Varies (100% if loaded, ~40% if offloaded)",
        "cpu_offload": True,  # Exceeds VRAM
        "response_time_s": 260,  # When offloaded
        "best_for": ["complex_reasoning", "thinking", "deep_analysis"],
        "avoid_for": ["quick_tasks", "low_latency"],
        "qualifier": "THINKING MODEL - Supports thinking mode, slow when offloaded"
    },
    "qwen3.6:latest": {
        "tier": "large",
        "speed": "very-slow",  # 120s response (80% CPU / 20% GPU)
        "gpu_usage": "20% (80% CPU offload)",
        "cpu_offload": True,  # ~17GB in system RAM
        "response_time_s": 120,
        "best_for": ["project_management", "complex_reasoning", "thinking"],
        "avoid_for": ["quick_tasks", "interactive_chat"],
        "qualifier": "PROJECT MANAGER - Large model, CPU offload, 120s timeout needed"
    },
    
    # Recommended models (Still downloading at test time)
    "gpt-oss:20b": {
        "tier": "large",
        "speed": "unknown",  # MoE model, designed for CPU offload
        "gpu_usage": "Partial (MoE architecture)",
        "cpu_offload": True,  # MXFP4 quantization
        "response_time_s": None,  # Not tested yet
        "best_for": ["complex_reasoning", "thinking", "agentic", "project_management"],
        "avoid_for": ["unknown until tested"],
        "qualifier": "MoE MODEL - Designed for efficient CPU offload (MXFP4)",
        "notes": "OpenAI MoE model, 20B parameters, active params vary"
    },
    "qwen3:30b-a3b": {
        "tier": "large",
        "speed": "unknown",  # MoE with 3B active params
        "gpu_usage": "Partial (MoE architecture)",
        "cpu_offload": True,  # 30B total, 3B active
        "response_time_s": None,  # Not tested yet
        "best_for": ["complex_reasoning", "thinking", "long_context", "project_management"],
        "avoid_for": ["unknown until tested"],
        "qualifier": "MoE EFFICIENT - 30B total but only 3B active params, great for long context",
        "notes": "Qwen3 MoE model, excellent for long context (48K+ tokens)"
    }
}


def get_qualifier(model: str) -> Dict[str, Any]:
    """Get qualifier for a specific model."""
    return MODEL_QUALIFIERS.get(model, {
        "tier": "unknown",
        "speed": "unknown",
        "qualifier": "No qualifier available"
    })


def get_models_by_tier(tier: str) -> List[str]:
    """Get all models matching a tier (small, medium, large)."""
    return [m for m, q in MODEL_QUALIFIERS.items() if q.get("tier") == tier]


def get_models_by_speed(speed: str) -> List[str]:
    """Get all models matching a speed (fast, moderate, slow, very-slow)."""
    return [m for m, q in MODEL_QUALIFIERS.items() if q.get("speed") == speed]


def print_qualifiers():
    """Print all model qualifiers in a formatted table."""
    print("=" * 80)
    print("Model Qualifiers (Based on Test Findings)")
    print("=" * 80)
    print()
    
    tiers = ["small", "medium", "large"]
    for tier in tiers:
        models = get_models_by_tier(tier)
        if models:
            print(f"\n{tier.upper()} Models:")
            print("-" * 40)
            for model in sorted(models):
                q = MODEL_QUALIFIERS[model]
                print(f"  {model}")
                print(f"    Qualifier: {q['qualifier']}")
                print(f"    Speed: {q['speed']}, Response: {q['response_time_s']}s")
                print(f"    GPU Usage: {q['gpu_usage']}")
                print(f"    CPU Offload: {q['cpu_offload']}")
                print(f"    Best For: {', '.join(q['best_for'])}")
                if 'notes' in q:
                    print(f"    Notes: {q['notes']}")
                print()
    
    print("=" * 80)


if __name__ == "__main__":
    print_qualifiers()
