"""
Sandboxed Code Editor: file operations restricted to a designated workspace.
Only executable when an ADMIN is logged in and sends the command.
"""

import os
import re
import sys
import subprocess
from pathlib import Path
from typing import Optional


class CodeEditor:
    """Sandboxed file editor restricted to a designated workspace."""

    def __init__(self, workspace_root: str):
        self.workspace_root = Path(workspace_root).resolve()
        if not self.workspace_root.exists():
            raise ValueError(f"Workspace does not exist: {self.workspace_root}")

    def _resolve_path(self, path: str) -> Path:
        """Resolve a path relative to workspace and ensure it stays inside."""
        full = (self.workspace_root / path).resolve()
        full.relative_to(self.workspace_root)
        return full

    def read_file(self, path: str) -> str:
        """Read a file from workspace."""
        full = self._resolve_path(path)
        if not full.exists():
            return f"[ERROR] File not found: {path}"
        if not full.is_file():
            return f"[ERROR] Not a file: {path}"
        return full.read_text(encoding="utf-8")

    def write_file(self, path: str, content: str) -> str:
        """Write content to a file in workspace."""
        full = self._resolve_path(path)
        full.parent.mkdir(parents=True, exist_ok=True)
        full.write_text(content, encoding="utf-8")
        return f"[OK] Written {len(content)} bytes to {path}"

    def edit_file(self, path: str, old_string: str, new_string: str) -> str:
        """Find and replace within a file."""
        full = self._resolve_path(path)
        if not full.exists():
            return f"[ERROR] File not found: {path}"
        content = full.read_text(encoding="utf-8")
        if old_string not in content:
            return f"[ERROR] String not found in {path}"
        new_content = content.replace(old_string, new_string)
        full.write_text(new_content, encoding="utf-8")
        return f"[OK] Replaced string in {path}"

    def list_files(self, pattern: str = "**/*") -> list:
        """List files matching a glob pattern within workspace."""
        files = []
        for f in self.workspace_root.glob(pattern):
            if f.is_file():
                files.append(str(f.relative_to(self.workspace_root)))
        return sorted(files)

    def run_command(self, cmd: list, cwd: Optional[str] = None, timeout: int = 60) -> dict:
        """Run a shell command within the workspace."""
        if cwd:
            work_dir = self._resolve_path(cwd)
        else:
            work_dir = self.workspace_root
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=work_dir,
                timeout=timeout,
            )
            return {
                "stdout": result.stdout,
                "stderr": result.stderr,
                "returncode": result.returncode,
            }
        except subprocess.TimeoutExpired:
            return {"stdout": "", "stderr": "TIMEOUT", "returncode": -1}
        except FileNotFoundError:
            return {"stdout": "", "stderr": f"Command not found: {cmd[0]}", "returncode": -1}

    def list_py_files(self) -> list:
        """List all Python files in workspace."""
        return self.list_files("**/*.py")

    def read_file_safe(self, path: str) -> Optional[str]:
        """Read file, return None if error."""
        try:
            return self.read_file(path)
        except (ValueError, Exception):
            return None
