#!/usr/bin/env python3
"""Runtime GPU/CPU detection and offloading advisory for Ollama models.

Detects available hardware (NVIDIA, AMD, CPU-only), measures free VRAM/system RAM,
and advises which models can run fully on GPU vs which require CPU offload.

Usage:
    python gpu_check.py              # Print full report
    python gpu_check.py --json       # JSON output for programmatic use
    python gpu_check.py --recommend "qwen2.5:7b"  # Check a specific model
"""

import json
import os
import platform
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field, asdict
from typing import Any, Optional


RTX_3070_LAPTOP_VRAM_TOTAL_MB = 8192
RTX_3070_LAPTOP_VRAM_USABLE_MB = 6973
MODEL_OVERHEAD_MB = 500


GPU_MODEL_FITS = {
    "qwen2.5:0.5b": {"size_mb": 400, "fits": True, "offload_mb": 0},
    "qwen2.5:1.5b": {"size_mb": 1000, "fits": True, "offload_mb": 0},
    "qwen2.5-coder:1.5b": {"size_mb": 1100, "fits": True, "offload_mb": 0},
    "llama3.2:1b": {"size_mb": 1400, "fits": True, "offload_mb": 0},
    "qwen2.5:3b": {"size_mb": 2000, "fits": True, "offload_mb": 0},
    "mistral:7b": {"size_mb": 4500, "fits": True, "offload_mb": 0},
    "qwen2.5:7b": {"size_mb": 4700, "fits": True, "offload_mb": 1200},
    "qwen3.5:latest": {"size_mb": 6600, "fits": False, "offload_mb": 400},
    "gemma4:latest": {"size_mb": 9600, "fits": False, "offload_mb": 5000},
    "qwen3.6:latest": {"size_mb": 23000, "fits": False, "offload_mb": 17000},
    "gpt-oss:20b": {"size_mb": 18000, "fits": False, "offload_mb": 12000},
    "qwen3:30b-a3b": {"size_mb": 20000, "fits": False, "offload_mb": 14000},
}


def _run(cmd: list[str], timeout: int = 8) -> str:
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, check=False)
        return (out.stdout or "") + (out.stderr or "")
    except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
        return ""


def is_wsl() -> bool:
    if platform.system() not in ("Linux", "Windows"):
        return False
    if platform.system() == "Windows":
        try:
            out = _run(["powershell", "-NoProfile", "(Get-CimInstance Win32_ComputerSystem).Model"])
            return "wsl" in out.lower()
        except Exception:
            return False
    if "microsoft" in platform.release().lower() or "wsl" in platform.release().lower():
        return True
    try:
        with open("/proc/version") as f:
            return "microsoft" in f.read().lower()
    except OSError:
        return False


def detect_nvidia() -> Optional[dict]:
    if not shutil.which("nvidia-smi"):
        return None
    out = _run([
        "nvidia-smi",
        "--query-gpu=index,name,memory.total,memory.free,memory.used,driver_version,temperature.gpu",
        "--format=csv,noheader,nounits",
    ])
    if not out.strip():
        return None
    gpus = []
    for line in out.strip().splitlines():
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 4:
            continue
        try:
            gpus.append({
                "index": int(parts[0]),
                "name": parts[1],
                "vram_total_mb": int(parts[2]),
                "vram_free_mb": int(parts[3]),
                "vram_used_mb": int(parts[4]),
                "driver": parts[5] if len(parts) > 5 else "",
                "temp_c": int(parts[6]) if len(parts) > 6 and parts[6] else 0,
            })
        except (ValueError, IndexError):
            continue
    if not gpus:
        return None
    best = max(gpus, key=lambda g: g["vram_free_mb"])
    best["all_gpus"] = gpus
    return best


def detect_rocm() -> Optional[dict]:
    if not shutil.which("rocm-smi"):
        return None
    out = _run(["rocm-smi", "--showproductname", "--showmeminfo", "vram", "--json"])
    if out.strip().startswith("{"):
        try:
            data = json.loads(out)
            cards = []
            for card_id, info in data.items():
                if not card_id.startswith("card"):
                    continue
                name = (
                    info.get("Card series") or info.get("Card model")
                    or info.get("Marketing Name") or "AMD GPU"
                )
                vram_b = info.get("VRAM Total Memory (B)") or info.get("vram_total_memory_b") or 0
                vram_free_b = info.get("VRAM Free Memory (B)") or info.get("vram_free_memory_b") or 0
                try:
                    vram_b = int(vram_b)
                    vram_free_b = int(vram_free_b)
                except (ValueError, TypeError):
                    vram_b = 0
                    vram_free_b = 0
                cards.append({
                    "vendor": "amd",
                    "name": str(name).strip(),
                    "vram_total_mb": round(vram_b / (1024**2), 1),
                    "vram_free_mb": round(vram_free_b / (1024**2), 1),
                    "driver": "rocm",
                })
            if cards:
                best = max(cards, key=lambda c: c["vram_free_mb"])
                best["all_gpus"] = cards
                return best
        except json.JSONDecodeError:
            pass
    out = _run(["rocm-smi", "--showproductname", "--showmeminfo", "vram"])
    if not out.strip():
        return None
    name_m = re.search(r"Card (?:series|model|Marketing Name):\s*(.+)", out)
    vram_m = re.search(r"VRAM Total Memory \(B\):\s*(\d+)", out)
    vram_total = round(int(vram_m.group(1)) / (1024**2), 1) if vram_m else 0.0
    return {
        "vendor": "amd",
        "name": name_m.group(1).strip() if name_m else "AMD GPU",
        "vram_total_mb": vram_total,
        "vram_free_mb": vram_total,
        "driver": "rocm",
    }


def detect_apple_silicon() -> Optional[dict]:
    if platform.system() != "Darwin":
        return None
    if platform.machine() != "arm64":
        return None
    chip = _run(["sysctl", "-n", "machdep.cpu.brand_string"]).strip()
    m = re.search(r"Apple M(\d+)", chip)
    generation = int(m.group(1)) if m else None
    mem_bytes = 0
    try:
        mem_bytes = int(_run(["sysctl", "-n", "hw.memsize"]).strip() or 0)
    except ValueError:
        pass
    ram_gb = round(mem_bytes / (1024**3), 1) if mem_bytes else 0.0
    return {
        "vendor": "apple",
        "name": chip or "Apple Silicon",
        "generation": generation,
        "unified_memory_gb": ram_gb,
    }


def total_system_ram_gb() -> float:
    sysname = platform.system()
    if sysname in ("Darwin", "Linux"):
        try:
            if sysname == "Darwin":
                return round(int(_run(["sysctl", "-n", "hw.memsize"]).strip() or 0) / (1024**3), 1)
            with open("/proc/meminfo") as f:
                for line in f:
                    if line.startswith("MemTotal:"):
                        kb = int(line.split()[1])
                        return round(kb / (1024**2), 1)
        except (OSError, ValueError):
            return 0.0
    if sysname == "Windows":
        if shutil.which("powershell"):
            out = _run([
                "powershell", "-NoProfile",
                "(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory",
            ])
            m = re.search(r"(\d{8,})", out)
            if m:
                return round(int(m.group(1)) / (1024**3), 1)
        out = _run(["wmic", "ComputerSystem", "get", "TotalPhysicalMemory"])
        m = re.search(r"(\d{6,})", out)
        if m:
            return round(int(m.group(1)) / (1024**3), 1)
    return 0.0


def check_ollama_gpu_mode() -> Optional[bool]:
    """Check if Ollama is using GPU acceleration."""
    if not shutil.which("ollama"):
        return None
    out = _run(["ollama", "serve", "--help"], timeout=5)
    if "nvidia" in out.lower():
        return True
    out = _run(["nvidia-smi"], timeout=5)
    if "ollama" in out:
        return True
    return None


def check_pytorch_gpu() -> dict:
    try:
        import torch
        info = {"available": True, "version": torch.__version__}
        info["cuda"] = torch.cuda.is_available()
        if info["cuda"]:
            info["cuda_devices"] = torch.cuda.device_count()
            info["cuda_device_0"] = torch.cuda.get_device_name(0)
            info["cuda_vram_total"] = round(torch.cuda.get_device_properties(0).total_mem / (1024**3), 1)
        info["mps"] = torch.backends.mps.is_available() if hasattr(torch.backends, "mps") else False
        return info
    except Exception as e:
        return {"available": False, "error": str(e)}


def recommend_models(gpu_info: Optional[dict], ram_gb: float) -> dict:
    recommendations = {}
    for model, info in GPU_MODEL_FITS.items():
        if gpu_info is None:
            recommendations[model] = {
                "verdict": "CPU_ONLY",
                "note": "No GPU detected - will run entirely on CPU (very slow)"
            }
        elif gpu_info.get("vendor") == "apple":
            unified = gpu_info.get("unified_memory_gb", 0)
            if info["size_mb"] / 1024 < unified * 0.7:
                recommendations[model] = {"verdict": "FULL_GPU", "note": "Fits in unified memory"}
            else:
                recommendations[model] = {"verdict": "OFFLOAD", "note": "Will use swap"}
        else:
            vram_free = gpu_info.get("vram_free_mb", 0)
            required = info["size_mb"] + MODEL_OVERHEAD_MB
            if vram_free >= required:
                recommendations[model] = {"verdict": "FULL_GPU", "note": f"Fits in VRAM (needs {required}MB, has {vram_free}MB)"}
            elif vram_free >= info["size_mb"] * 0.3:
                offload = info["size_mb"] - vram_free + MODEL_OVERHEAD_MB
                recommendations[model] = {"verdict": "PARTIAL_OFFLOAD", "offload_mb": offload, "note": f"Partial CPU offload: ~{offload}MB in system RAM"}
            else:
                recommendations[model] = {"verdict": "FULL_OFFLOAD", "note": f"Mostly CPU offload (only {vram_free}MB VRAM free)"}
    return recommendations


def build_report() -> dict:
    sysname = platform.system()
    arch = platform.machine()
    wsl = is_wsl()
    ram_gb = total_system_ram_gb()
    gpu = detect_nvidia() or detect_rocm() or detect_apple_silicon()
    ollama_gpu = check_ollama_gpu_mode() if gpu else False
    pytorch = check_pytorch_gpu()
    recommendations = recommend_models(gpu, ram_gb)

    return {
        "os": sysname,
        "arch": arch,
        "wsl": wsl,
        "system_ram_gb": ram_gb,
        "gpu": gpu,
        "ollama_gpu_mode": ollama_gpu,
        "pytorch": pytorch,
        "recommendations": recommendations,
    }


def print_report(report: dict):
    print("=" * 70)
    print("  GPU/CPU HARDWARE DETECTION REPORT")
    print("=" * 70)
    print(f"  OS:          {report['os']} ({report['arch']})")
    if report.get("wsl"):
        print(f"  Environment: WSL2")
    print(f"  System RAM:  {report['system_ram_gb']} GB")

    gpu = report.get("gpu")
    if gpu:
        vendor = gpu.get("vendor", "unknown")
        name = gpu.get("name", "Unknown GPU")
        if vendor == "nvidia":
            print(f"  GPU:         {name}  ({gpu['vram_free_mb']}MB free / {gpu['vram_total_mb']}MB total)")
            print(f"  Driver:      {gpu.get('driver', '')}")
            if len(gpu.get("all_gpus", [])) > 1:
                print(f"  GPUs:        {len(gpu['all_gpus'])} total (using best by free VRAM)")
        elif vendor == "apple":
            print(f"  GPU:         {name}  ({gpu.get('unified_memory_gb', 0)} GB unified)")
        elif vendor == "amd":
            print(f"  GPU:         {name}  ({gpu.get('vram_free_mb', 0)}MB free)")
    else:
        print(f"  GPU:         NONE DETECTED (CPU-only mode)")

    if report.get("pytorch", {}).get("available"):
        pt = report["pytorch"]
        line = f"  PyTorch:     {pt.get('version')}"
        if pt.get("cuda"):
            line += f" + CUDA ({pt.get('cuda_device_0', '?')})"
        if pt.get("mps"):
            line += " + MPS"
        print(line)

    print()
    print("-" * 70)
    print("  MODEL OFFLOAD ADVISORY")
    print("-" * 70)
    for model, rec in report.get("recommendations", {}).items():
        verdict = rec["verdict"]
        note = rec["note"]
        if verdict == "FULL_GPU":
            print(f"  [OK] {model:<25s} FULL GPU  - {note}")
        elif verdict == "PARTIAL_OFFLOAD":
            print(f"  [!!] {model:<25s} OFFLOAD  - {note}")
        elif verdict == "FULL_OFFLOAD":
            print(f"  [XX] {model:<25s} OFFLOAD  - {note}")
        elif verdict == "CPU_ONLY":
            print(f"  [--] {model:<25s} CPU ONLY - {note}")

    print()
    print("=" * 70)
    verdicts = [r["verdict"] for r in report.get("recommendations", {}).values()]
    full_gpu = sum(1 for v in verdicts if v == "FULL_GPU")
    partial = sum(1 for v in verdicts if v in ("PARTIAL_OFFLOAD",))
    full_off = sum(1 for v in verdicts if v == "FULL_OFFLOAD")
    cpu_only = sum(1 for v in verdicts if v == "CPU_ONLY")
    print(f"  Summary:  {full_gpu} full-GPU  |  {partial} partial-offload  |  {full_off} full-offload  |  {cpu_only} CPU-only")


def main():
    import argparse
    p = argparse.ArgumentParser(description="GPU/CPU detection and offload advisory")
    p.add_argument("--json", action="store_true", help="JSON output")
    p.add_argument("--recommend", help="Check a specific model")
    args = p.parse_args()

    if args.recommend:
        report = build_report()
        gpu = report.get("gpu")
        ram = report.get("system_ram_gb", 0)
        recs = recommend_models(gpu, ram)
        if args.recommend in recs:
            print(json.dumps(recs[args.recommend], indent=2))
        else:
            print(json.dumps({"error": f"Unknown model: {args.recommend}"}))
        return

    report = build_report()
    if args.json:
        print(json.dumps(report, indent=2, default=str))
    else:
        print_report(report)


if __name__ == "__main__":
    if sys.platform == 'win32':
        sys.stdout.reconfigure(encoding='utf-8')
    main()
