"""
Bug Scanner: scans workspace files for syntax errors, TODOs, and issues.
Integrates with CodeEditor for safe file operations.
"""

import ast
import sys
import py_compile
import tempfile
import traceback
from pathlib import Path
from typing import List, Dict, Optional


class BugScanner:
    """Scans workspace files for bugs, TODOs, and potential issues."""

    def __init__(self, workspace_root: str):
        self.workspace_root = Path(workspace_root).resolve()
        self.results: Dict[str, List[dict]] = {}

    def scan_py_file(self, filepath: Path) -> List[dict]:
        """Scan a Python file for syntax errors, TODOs, and common issues."""
        rel = str(filepath.relative_to(self.workspace_root))
        bugs = []
        try:
            content = filepath.read_text(encoding="utf-8")
        except Exception as e:
            bugs.append({"type": "IO_ERROR", "file": rel, "line": 0, "msg": str(e)})
            return bugs

        # Syntax check
        try:
            ast.parse(content, filename=rel)
        except SyntaxError as e:
            bugs.append({
                "type": "SYNTAX_ERROR",
                "file": rel,
                "line": e.lineno or 0,
                "msg": str(e),
            })

        # Compile check (stricter)
        try:
            compile(content, rel, "exec")
        except Exception as e:
            bugs.append({
                "type": "COMPILE_ERROR",
                "file": rel,
                "line": 0,
                "msg": str(e),
            })

        # Scan for TODO/FIXME/HACK/XXX markers
        for i, line in enumerate(content.split("\n"), 1):
            lower = line.strip()
            for keyword, severity in [("todo", "INFO"), ("fixme", "WARN"), ("hack", "WARN"), ("xxx", "WARN")]:
                if lower.lower().startswith(f"// {keyword}") or lower.lower().startswith(f"# {keyword}"):
                    bugs.append({
                        "type": f"{severity}_{keyword.upper()}",
                        "file": rel,
                        "line": i,
                        "msg": line.strip(),
                    })
                    break

        return bugs

    def scan_cpp_file(self, filepath: Path) -> List[dict]:
        """Scan a C++ file for TODO markers."""
        rel = str(filepath.relative_to(self.workspace_root))
        bugs = []
        try:
            content = filepath.read_text(encoding="utf-8")
        except Exception as e:
            bugs.append({"type": "IO_ERROR", "file": rel, "line": 0, "msg": str(e)})
            return bugs

        for i, line in enumerate(content.split("\n"), 1):
            lower = line.strip().lower()
            for keyword, severity in [("todo", "INFO"), ("fixme", "WARN"), ("hack", "WARN"), ("xxx", "WARN")]:
                if lower.startswith(f"// {keyword}") or lower.startswith(f"/* {keyword}"):
                    bugs.append({
                        "type": f"{severity}_{keyword.upper()}",
                        "file": rel,
                        "line": i,
                        "msg": line.strip(),
                    })
                    break

        return bugs

    def scan_file(self, path_str: str) -> List[dict]:
        """Scan a single file by path string (relative to workspace)."""
        full = (self.workspace_root / path_str).resolve()
        try:
            full.relative_to(self.workspace_root)
        except ValueError:
            return [{"type": "SECURITY", "file": path_str, "line": 0, "msg": "Path outside workspace"}]
        if not full.exists() or not full.is_file():
            return [{"type": "IO_ERROR", "file": path_str, "line": 0, "msg": "File not found"}]
        suffix = full.suffix.lower()
        if suffix == ".py":
            return self.scan_py_file(full)
        elif suffix in (".cpp", ".h", ".hpp", ".c", ".hxx"):
            return self.scan_cpp_file(full)
        return []

    def scan_all(self, include_dirs: Optional[List[str]] = None,
                 exclude_dirs: Optional[list] = None) -> Dict[str, List[dict]]:
        """Scan all supported files in workspace (or specific subdirs)."""
        SKIP_DIRS = [
            "whisper.cpp", "CrowNest-Public-main", "llvm-project-release-17.x",
            "llvm-build-nvptx", "piper", "PillarAIColabImport",
            "llvm17-install", "llvm-config", "spirv-translator",
            ".git", ".venv", "build", "build-llvm-nvptx", "models",
            "gui/imgui", "gui/backends", "assets", "audio",
        ]
        if exclude_dirs is None:
            exclude_dirs = SKIP_DIRS

        def _is_skipped(path: Path) -> bool:
            try:
                rel = str(path.relative_to(self.workspace_root))
            except ValueError:
                return True
            rel_norm = rel.replace("\\", "/")
            for skip in exclude_dirs:
                skip_norm = skip.replace("\\", "/")
                if rel_norm == skip_norm or rel_norm.startswith(skip_norm + "/"):
                    return True
            return False

        self.results = {}
        if include_dirs:
            roots = [(self.workspace_root / d).resolve() for d in include_dirs]
        else:
            roots = [self.workspace_root]

        for root in roots:
            try:
                root.relative_to(self.workspace_root)
            except ValueError:
                continue
            if not root.exists():
                continue
            for f in root.rglob("*"):
                if not f.is_file():
                    continue
                if _is_skipped(f):
                    continue
                if f.suffix.lower() == ".py":
                    bugs = self.scan_py_file(f)
                elif f.suffix.lower() in (".cpp", ".h", ".hpp", ".c"):
                    bugs = self.scan_cpp_file(f)
                else:
                    continue
                if bugs:
                    rel = str(f.relative_to(self.workspace_root))
                    self.results[rel] = bugs
        return self.results

    def summary(self) -> str:
        """Return a human-readable bug summary."""
        if not self.results:
            return "No issues found."
        lines = [f"Found issues in {len(self.results)} file(s):"]
        for filepath, bugs in sorted(self.results.items()):
            lines.append(f"\n  {filepath}:")
            for b in bugs:
                lines.append(f"    L{b['line']:>5}  [{b['type']}]  {b['msg'][:100]}")
        return "\n".join(lines)
