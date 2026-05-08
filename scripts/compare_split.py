"""Quick check: which monorepo files appear in zero repos?"""
from pathlib import Path

T = Path("C:/Users/aobie/AppData/Local/Temp")

def load(name):
    p = T / f"{name}_files.txt"
    return set(l.strip().replace("\\", "/") for l in p.read_text().splitlines() if l.strip())

mono = load("monorepo")
all_repo = set()
for name in ["engine","services","toolchain","docs","socialsim","pillarai","whisper","piper"]:
    all_repo |= load(name)

# Submodule content not tracked in monorepo git
submods = ("PillarAIColab/", "whisper.cpp/", "piper/")
mono_filtered = {f for f in mono if not any(f.startswith(s) for s in submods)}

missing = mono_filtered - all_repo
print(f"Monorepo (excl submodules): {len(mono_filtered)}")
print(f"In at least one repo:        {len(mono_filtered & all_repo)}")
print(f"Truly missing (zero repos):  {len(missing)}")
if missing:
    print()
    for f in sorted(missing):
        print(f"  {f}")
