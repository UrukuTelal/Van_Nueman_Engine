#!/usr/bin/env python3
"""
Model Validation Script.
Validates all Ollama models against test findings and system constraints.
Exit codes: 0 = all valid, 1 = validation errors found
"""
import json
import sys
import subprocess
from pathlib import Path
from typing import Optional

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))
from scripts.task_delegator import MODEL_CONFIGS


class ModelValidator:
    """Validate Ollama models against system constraints and test findings."""
    
    def __init__(self, model_configs: dict):
        self.model_configs = model_configs
        self.errors = []
        self.warnings = []
    
    def validate_gpu_estimate(self, model: str, config: dict) -> list[str]:
        """Validate GPU estimate is reasonable for RTX 3070 (8GB VRAM)."""
        errors = []
        gpu_est_mb = config.get("gpu_estimate_mb", 0)
        
        # RTX 3070 usable VRAM ~7000MB
        if gpu_est_mb > 7000:
            self.warnings.append(
                f"{model}: GPU estimate {gpu_est_mb}MB exceeds usable VRAM (7000MB) - will offload to CPU"
            )
        
        if gpu_est_mb < 0:
            errors.append(f"{model}: Invalid GPU estimate {gpu_est_mb}MB")
        
        return errors
    
    def validate_task_types(self, model: str, config: dict) -> list[str]:
        """Validate task types are recognized."""
        errors = []
        valid_task_types = [
            "simple", "code_review", "analysis", "chat", "complex",
            "thinking", "programming", "complex_reasoning", "deep_analysis",
            "cpp_code_generation", "code_generation", "moderate_analysis", "content_generation",
            "basic_analysis", "summarization", "extraction", "quick_chat",
            "classification", "project_management", "agentic", "long_context"
        ]
        
        task_types = config.get("task_types", [])
        for tt in task_types:
            if tt not in valid_task_types:
                errors.append(f"{model}: Unknown task type '{tt}'")
        
        if not task_types:
            errors.append(f"{model}: No task types specified")
        
        return errors
    
    def validate_delta_h(self, model: str, config: dict) -> list[str]:
        """Validate delta_h is within acceptable range (0.0 - 1.0)."""
        errors = []
        delta_h = config.get("delta_h", None)
        
        if delta_h is None:
            errors.append(f"{model}: Missing delta_h")
        elif not isinstance(delta_h, (int, float)):
            errors.append(f"{model}: delta_h must be numeric, got {type(delta_h).__name__}")
        elif delta_h < 0.0 or delta_h > 1.0:
            errors.append(f"{model}: delta_h {delta_h} outside range [0.0, 1.0]")
        elif delta_h > 0.7:
            self.warnings.append(
                f"{model}: delta_h {delta_h} exceeds transformation threshold (0.7) - action may be rejected"
            )
        
        return errors
    
    def validate_timeout(self, model: str, config: dict) -> list[str]:
        """Validate timeout_ms is sufficient based on test findings."""
        errors = []
        timeout_ms = config.get("timeout_ms", None)
        
        if timeout_ms is None:
            # No timeout specified - model likely fits in GPU
            pass
        elif not isinstance(timeout_ms, int):
            errors.append(f"{model}: timeout_ms must be integer, got {type(timeout_ms).__name__}")
        elif timeout_ms < 60000:
            self.warnings.append(
                f"{model}: timeout_ms {timeout_ms} may be too low (minimum recommended: 60000ms = 1min)"
            )
        elif timeout_ms > 600000:
            self.warnings.append(
                f"{model}: timeout_ms {timeout_ms} is very high - model may be too slow for practical use"
            )
        
        return errors
    
    def validate_model_available(self, model: str, skip_downloading: bool = True) -> list[str]:
        """Check if model is available in Ollama.
        skip_downloading: If True, skip check for models that are still downloading.
        """
        errors = []
        
        # Skip availability check for models known to be downloading
        if skip_downloading and model in ["gpt-oss:20b", "qwen3:30b-a3b"]:
            self.warnings.append(f"{model}: Skipping availability check (known to be downloading)")
            return errors
        
        try:
            result = subprocess.run(
                ["ollama", "list"],
                capture_output=True,
                text=True,
                check=True
            )
            if model not in result.stdout:
                errors.append(f"{model}: Not found in Ollama (run: ollama pull {model})")
        except Exception as e:
            self.warnings.append(f"Could not check Ollama availability: {e}")
        
        return errors
    
    def validate_all(self) -> list[str]:
        """Validate all models in MODEL_CONFIGS."""
        all_errors = []
        
        print(f"Validating {len(self.model_configs)} models...")
        print()
        
        for model, config in self.model_configs.items():
            model_errors = []
            model_errors.extend(self.validate_gpu_estimate(model, config))
            model_errors.extend(self.validate_task_types(model, config))
            model_errors.extend(self.validate_delta_h(model, config))
            model_errors.extend(self.validate_timeout(model, config))
            model_errors.extend(self.validate_model_available(model))
            
            if model_errors:
                print(f"[ERR] {model}: {len(model_errors)} validation errors")
                for err in model_errors[:3]:
                    print(f"    - {err}")
                if len(model_errors) > 3:
                    print(f"    ... and {len(model_errors) - 3} more")
                all_errors.extend([f"{model}: {e}" for e in model_errors])
            else:
                gpu_est = config.get("gpu_estimate_mb", 0)
                timeout = config.get("timeout_ms", "N/A")
                delta_h = config.get("delta_h", 0)
                print(f"[OK] {model}: GPU~{gpu_est}MB, ΔH={delta_h:.2f}, timeout={timeout}ms")
        
        return all_errors


def main():
    """Run validation on all configured models."""
    print("=" * 60)
    print("Model Validation Script")
    print("=" * 60)
    print()
    
    validator = ModelValidator(MODEL_CONFIGS)
    errors = validator.validate_all()
    
    # Print warnings
    if validator.warnings:
        print()
        print("-" * 60)
        print(f"Warnings ({len(validator.warnings)}):")
        for warn in validator.warnings:
            print(f"  ⚠ {warn}")
    
    # Summary
    print()
    print("=" * 60)
    if errors:
        print(f"VALIDATION FAILED: {len(errors)} errors found")
        for err in errors:
            print(f"  - {err}")
        sys.exit(1)
    else:
        print(f"VALIDATION PASSED: All {len(MODEL_CONFIGS)} models are valid")
        sys.exit(0)


if __name__ == "__main__":
    main()
