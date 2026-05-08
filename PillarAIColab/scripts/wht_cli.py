#!/usr/bin/env python3
"""
WHT CLI - Web-Based Terminal for CrowNest Development Team
- Opens web terminal at localhost:8889
- Accepts user input, translates to WHT (32D)
- Routes via NeuralAgent (512D) to best agent
- Shows 12-agent Dev Team status
- NOW WITH: Distortion Sandbox integration
"""

import sys
import json
import re
import threading
import queue
import time
import urllib.request
import urllib.parse
from pathlib import Path
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import *
from datetime import datetime, timezone
from typing import Optional

try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False

try:
    from bs4 import BeautifulSoup
    HAS_BS4 = True
except ImportError:
    HAS_BS4 = False

sys.path.insert(0, str(Path(__file__).parent.parent))

# Try imports
try:
    from scripts.wht_protocol_manager import WHTProtocolManager
    WHT_AVAILABLE = True
except ImportError as e:
    print(f"Warning: WHTProtocolManager not available - {e}")
    WHT_AVAILABLE = False
    WHTProtocolManager = None

try:
    from scripts.dev_team import CrowNestDevTeam
    DEV_TEAM_AVAILABLE = True
except ImportError as e:
    print(f"Warning: CrowNestDevTeam not available - {e}")
    DEV_TEAM_AVAILABLE = False
    CrowNestDevTeam = None

try:
    from scripts.task_delegator import delegate_task, check_ollama_running
    TASK_DELEGATOR_AVAILABLE = True
except ImportError:
    TASK_DELEGATOR_AVAILABLE = False

try:
    from scripts.ollama_nothink import OllamaNoThink, SimpleAgent
    OLLAMA_NOTHINK_AVAILABLE = True
except ImportError:
    OLLAMA_NOTHINK_AVAILABLE = False

try:
    import ollama
    HAS_OLLAMA = True
except ImportError:
    HAS_OLLAMA = False

try:
    from scripts.code_editor import CodeEditor
    CODE_EDITOR_AVAILABLE = True
except ImportError as e:
    print(f"Warning: CodeEditor not available - {e}")
    CODE_EDITOR_AVAILABLE = False
    CodeEditor = None

try:
    from scripts.bug_scanner import BugScanner
    BUG_SCANNER_AVAILABLE = True
except ImportError as e:
    print(f"Warning: BugScanner not available - {e}")
    BUG_SCANNER_AVAILABLE = False
    BugScanner = None

# Feature modules
FEATURES_AVAILABLE = True
_VN_SRC = str(Path(r"C:\Projects\Van_Nueman\src").resolve())
if _VN_SRC not in sys.path:
    sys.path.insert(0, _VN_SRC)
try:
    import memory_compression
    import autopoiesis
    import learning
    import reasoning
    import sandbox as sandbox_mod
    CompressedMemoryDB = memory_compression.CompressedMemoryDB
    LocalAutopoiesisSubsystem = autopoiesis.LocalAutopoiesisSubsystem
    AutopoiesisContext = autopoiesis.AutopoiesisContext
    EventBus = autopoiesis.EventBus
    SkillLibrary = learning.SkillLibrary
    TrajectoryCollector = learning.TrajectoryCollector
    ContextCompressor = learning.ContextCompressor
    HierarchicalReasoner = reasoning.HierarchicalReasoner
    Sandbox = sandbox_mod.Sandbox
except ImportError as e:
    import traceback
    print(f"Warning: Feature modules not available - {e}", flush=True)
    print(f"FEATURE DEBUG: _VN_SRC={_VN_SRC}, sys.path[0:2]={sys.path[:2]}", flush=True)
    traceback.print_exc()

# Global instances
wht_manager = None
dev_team = None
message_queue = queue.Queue()
terminal_log = []
MAX_LOG_SIZE = 5000
_kill_flag = None  # None=normal, "shutdown"=kill, "restart"=kill --restart
_server = None    # HTTPServer instance for clean shutdown

# Code editing workspace
WORKSPACE_ROOT = str(Path(__file__).parent.parent.parent.resolve())
code_editor = CodeEditor(WORKSPACE_ROOT) if CODE_EDITOR_AVAILABLE else None
bug_scanner = BugScanner(WORKSPACE_ROOT) if BUG_SCANNER_AVAILABLE else None

def clear_log():
    """Clear terminal log."""
    global terminal_log
    terminal_log = []
    print("[WHT CLI] Terminal log cleared")

PORT = 8889
ACTIVE_AGENTS = []

# Pending edits audit queue (for APPROVE/REJECT flow)
pending_edits = {}
edit_id_counter = 0
edit_id_lock = threading.Lock()
auto_approve = False  # When True, pending edits are approved automatically

def _next_edit_id():
    global edit_id_counter
    with edit_id_lock:
        edit_id_counter += 1
        return edit_id_counter

# User roles for logging
USER_ROLES = {
    "ADMIN": {"name": "Administrator", "color": "purple", "level": 0, "password": "PapaMoshi"},
    "SYS": {"name": "System", "color": "cyan", "level": 1},
    "PROJECT_MANAGER": {"name": "Project Manager", "color": "blue", "level": 2},
    "ROLES": {"name": "Agent Roles", "color": "green", "level": 3},
    "AGENT": {"name": "Agent", "color": "yellow", "level": 4},
    "USER": {"name": "User", "color": "white", "level": 5}
}

CURRENT_USER = "SYS"
CURRENT_USERNAME = "System"

# PillarAI logging
BLACKBOARD_PATH = Path(__file__).parent.parent / "colab_blackboard" / "colab_blackboard.md"
try:
    from src.pillar_loader import PillarLoader
    PILLAR_LOADER_AVAILABLE = True
except ImportError:
    PILLAR_LOADER_AVAILABLE = False

def log_to_blackboard(task: str, delta_h: float, status: str, notes: str = ""):
    """Log to colab_blackboard.md per AGENTS.md mandate."""
    timestamp = datetime.now(timezone.utc).isoformat()
    entry = f"""
### [{timestamp}] wht_cli
- **Task**: {task}
- **DH**: {delta_h:.4f}
- **Status**: {status}
- **Notes**: {notes}
"""
    try:
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
    except Exception as e:
        print(f"[WHT CLI] Blackboard log error: {e}")

def init_systems():
    """Initialize WHT Manager and Dev Team."""
    global wht_manager, dev_team

    if WHT_AVAILABLE:
        try:
            wht_manager = WHTProtocolManager()
            print(f"[WHT CLI] WHTProtocolManager initialized")
        except Exception as e:
            print(f"[WHT CLI] WHTProtocolManager init failed: {e}")
            wht_manager = None

    if DEV_TEAM_AVAILABLE:
        try:
            dev_team = CrowNestDevTeam()
            print(f"[WHT CLI] CrowNestDevTeam initialized (12 agents)")
        except Exception as e:
            print(f"[WHT CLI] DevTeam init failed: {e}")
            dev_team = None

# Feature subsystems
if FEATURES_AVAILABLE:
    print(f"FEATURE DEBUG: Creating feature subsystems...", flush=True)
    try:
        mem_db = CompressedMemoryDB()
        print(f"FEATURE DEBUG: mem_db={mem_db}, len={len(mem_db)}", flush=True)
    except Exception as e:
        print(f"FEATURE DEBUG: mem_db creation failed: {e}", flush=True)
        mem_db = None
else:
    mem_db = None
event_bus = EventBus() if FEATURES_AVAILABLE else None
autopoiesis = LocalAutopoiesisSubsystem(enabled=True) if FEATURES_AVAILABLE else None
skill_lib = SkillLibrary() if FEATURES_AVAILABLE else None
trajectory_collector = TrajectoryCollector() if FEATURES_AVAILABLE else None
context_compressor = ContextCompressor() if FEATURES_AVAILABLE else None
reasoner = HierarchicalReasoner() if FEATURES_AVAILABLE else None
sandbox = Sandbox() if FEATURES_AVAILABLE else None


# ── Tool functions for RESEARCH (mistral:7b function calling) ──

def web_search(query: str, max_results: int = 5) -> str:
    """Search the web using DuckDuckGo HTML search."""
    if not HAS_REQUESTS:
        return "Error: requests library not installed"
    try:
        url = "https://html.duckduckgo.com/html/"
        headers = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}
        resp = requests.post(url, data={"q": query}, headers=headers, timeout=10)
        resp.raise_for_status()
        if not HAS_BS4:
            return resp.text[:2000]
        soup = BeautifulSoup(resp.text, "html.parser")
        results = []
        for link in soup.select("a.result__a")[:max_results]:
            title = link.get_text(strip=True)
            href = link.get("href", "")
            results.append(f"{title}\n  {href}")
        if not results:
            return f"No results found for: {query}"
        return "\n\n".join(results)
    except Exception as e:
        return f"Search error: {e}"


def web_fetch(url: str, max_chars: int = 5000) -> str:
    """Fetch content from a URL."""
    if not HAS_REQUESTS:
        return "Error: requests library not installed"
    try:
        headers = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}
        resp = requests.get(url, headers=headers, timeout=15)
        resp.raise_for_status()
        content_type = resp.headers.get("content-type", "")
        if "json" in content_type:
            data = resp.json()
            text = json.dumps(data, indent=2)
        else:
            text = resp.text
        if len(text) > max_chars:
            text = text[:max_chars] + "\n... [truncated]"
        return text
    except Exception as e:
        return f"Fetch error: {e}"


TOOLS = [
    {
        "type": "function",
        "function": {
            "name": "web_search",
            "description": "Search the web for information on a topic",
            "parameters": {
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": "The search query"
                    },
                    "max_results": {
                        "type": "integer",
                        "description": "Maximum number of results (default 5)",
                        "default": 5
                    }
                },
                "required": ["query"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "web_fetch",
            "description": "Fetch and read content from a URL",
            "parameters": {
                "type": "object",
                "properties": {
                    "url": {
                        "type": "string",
                        "description": "The URL to fetch"
                    }
                },
                "required": ["url"]
            }
        }
    }
]


TOOL_MAP = {
    "web_search": web_search,
    "web_fetch": web_fetch,
}


class WHTTerminalHandler(BaseHTTPRequestHandler):
    """HTTP handler for web-based WHT terminal."""

    def log_message(self, format, *args):
        """Suppress default logging."""
        pass

    def do_GET(self):
        parsed = urlparse(self.path)

        if parsed.path == '/' or parsed.path == '/index.html':
            self.serve_terminal()
        elif parsed.path == '/api/status':
            self.serve_api_status()
        elif parsed.path == '/api/log':
            self.serve_api_log()
        elif parsed.path == '/api/dev-team':
            self.serve_api_dev_team()
        elif parsed.path == '/api/clear':
            self.serve_api_clear()
        elif parsed.path == '/terminal.html' or parsed.path == '/terminal_view.html':
            self.serve_terminal_html()
        elif parsed.path == '/api/features/memory/stats':
            self.serve_feature_memory_stats()
        elif parsed.path.startswith('/api/features/autopoiesis/status'):
            self.serve_feature_autopoiesis_status()
        elif parsed.path.startswith('/api/features/autopoiesis/heartbeat'):
            self.serve_feature_autopoiesis_heartbeat()
        elif parsed.path == '/api/features/reason/status':
            self.serve_feature_reason_status()
        elif parsed.path == '/api/features/sandbox/health':
            self.serve_feature_sandbox_health()
        elif parsed.path == '/api/features/sandbox/logs':
            self.serve_feature_sandbox_logs()
        elif parsed.path == '/api/features/learning/skills':
            self.serve_feature_list_skills()
        else:
            self.send_error(404)

    def do_POST(self):
        parsed = urlparse(self.path)

        if parsed.path == '/api/input':
            self.handle_user_input()
        elif parsed.path == '/api/send':
            self.handle_send_message()
        elif parsed.path == '/api/log':
            self.handle_external_log()
        elif parsed.path == '/api/features/memory/compress':
            self.serve_feature_memory_compress()
        elif parsed.path == '/api/features/memory/decompress':
            self.serve_feature_memory_decompress()
        elif parsed.path == '/api/features/memory/search':
            self.serve_feature_memory_search()
        elif parsed.path == '/api/features/autopoiesis/sample':
            self.serve_feature_autopoiesis_sample()
        elif parsed.path == '/api/features/learning/skills':
            self.serve_feature_create_skill()
        elif parsed.path == '/api/features/learning/trajectory':
            self.serve_feature_add_trajectory()
        elif parsed.path == '/api/features/learning/compress':
            self.serve_feature_compress_context()
        elif parsed.path == '/api/features/reason':
            self.serve_feature_reason()
        elif parsed.path == '/api/features/sandbox/run':
            self.serve_feature_sandbox_run()
        elif parsed.path == '/api/features/voice/tts':
            self.serve_voice_tts()
        else:
            self.send_error(404)

    def serve_terminal(self):
        """Serve the web terminal HTML page."""
        try:
            terminal_path = Path(__file__).parent / 'terminal.html'
            with open(terminal_path, 'r', encoding='utf-8') as f:
                html = f.read()
        except:
            html = "<html><body><h1>terminal.html not found</h1></body></html>"

        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Cache-Control', 'no-cache')
        self.end_headers()
        self.wfile.write(html.encode('utf-8'))

    def serve_api_status(self):
        """API: Get terminal status."""
        status = {
            "wht_manager": wht_manager is not None,
            "dev_team": dev_team is not None,
            "ollama_running": check_ollama_running() if TASK_DELEGATOR_AVAILABLE else False,
            "active_agents": ACTIVE_AGENTS,
            "log_count": len(terminal_log),
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "current_user": CURRENT_USER,
            "current_username": CURRENT_USERNAME,
        }
        self.send_json_response(status)

    def serve_api_clear(self):
        """API: Clear terminal log."""
        clear_log()
        self.send_json_response({"status": "cleared", "message": "Terminal log cleared"})

    def serve_terminal_html(self):
        """Serve the generated terminal_view.html."""
        try:
            terminal_view_path = Path(__file__).parent / 'terminal_view.html'
            with open(terminal_view_path, 'r', encoding='utf-8') as f:
                html = f.read()
        except:
            html = "<html><body><h1>terminal_view.html not found</h1></body></html>"

        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Cache-Control', 'no-cache')
        self.end_headers()
        self.wfile.write(html.encode('utf-8'))

    def serve_api_log(self):
        """API: Get terminal log (with optional since parameter)."""
        print(f"[DEBUG] serve_api_log() called: path={self.path}")

        query = urlparse(self.path).query
        params = parse_qs(query)
        since_id = int(params.get('since', ['0'])[0])

        print(f"[DEBUG] serve_api_log: since_id={since_id}, total_log={len(terminal_log)}")

        messages = terminal_log[since_id:]
        print(f"[DEBUG] serve_api_log: returning {len(messages)} messages")

        formatted = []
        last_content = None

        for i, msg in enumerate(messages):
            idx = since_id + i
            user = msg.get("user", "USER")
            content = msg.get("content", "")
            timestamp = msg.get("timestamp", "")
            agent = msg.get("agent", "")

            print(f"[DEBUG] serve_api_log: msg {idx}: user={user}, agent={agent}, content={content[:30]}")

            if content == last_content:
                continue
            last_content = content

            time_only = timestamp.split(" ")[1] if " " in timestamp else timestamp

            role = msg.get("role", msg.get("user", "USER"))
            entry = {
                "id": idx,
                "user": user,
                "role": role,
                "content": content,
                "timestamp": timestamp,
                "agent": agent,
                "markdown": False,
                "raw": msg.get("raw", False)
            }

            if role == "AGENT" and agent:
                if dev_team:
                    try:
                        pillar = dev_team.pillar_state.to_dict()
                        entry["pillar_uid"] = f"[A:{pillar['awareness']:.1f} W:{pillar['willpower']:.1f} F:{pillar['force']:.2f} H:{pillar['harm']:.2f}]"
                    except:
                        pass

            if role == "ADMIN":
                entry["color"] = "purple"
            elif role == "SYS":
                entry["color"] = "cyan"
            elif role == "PROJECT_MANAGER":
                entry["color"] = "blue"
            elif role == "ROLES":
                entry["color"] = "green"
            elif role == "AGENT":
                entry["color"] = "yellow"
            else:
                entry["color"] = "white"

            formatted.append(entry)

        current_info = USER_ROLES.get(CURRENT_USER, USER_ROLES["USER"])
        concept_uid = {
            "user": CURRENT_USER,
            "username": CURRENT_USERNAME,
            "concept": current_info.get("concept", "unknown"),
        }

        pillar_summary = None
        if dev_team:
            ps = dev_team.pillar_state.to_dict()
            pillar_summary = f"[A:{ps['awareness']:.1f} W:{ps['willpower']:.1f} F:{ps['force']:.1f} H:{ps['harm']:.2f}]"

        response_data = {
            "messages": formatted,
            "total": len(terminal_log),
            "since": since_id,
            "current_user": CURRENT_USER,
            "concept_uid": concept_uid,
            "pillar_uid": pillar_summary,
        }

        print(f"[DEBUG] serve_api_log: response has {len(formatted)} formatted messages")

        self.send_json_response(response_data)

    def serve_api_dev_team(self):
        """API: Get Dev Team status."""
        if dev_team:
            status = dev_team.get_team_status()
        else:
            status = {"error": "Dev Team not initialized"}
        self.send_json_response(status)

    def handle_user_input(self):
        """Handle user input from web terminal (auto-route via WHT)."""
        print(f"[DEBUG] handle_user_input() called")

        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length)
        data = json.loads(body.decode('utf-8'))

        user_input = data.get('input', '').strip()
        print(f"[DEBUG] User typed: '{user_input}'")

        if not user_input:
            print(f"[DEBUG] Empty input, sending error response")
            self.send_json_response({"error": "Empty input"})
            return

        print(f"[DEBUG] Current user: {CURRENT_USER}")
        self.add_log("user", user_input, agent=CURRENT_USER)
        print(f"[DEBUG] Added log entry, total logs: {len(terminal_log)}")

        # Handle KILL command (works for any user)
        if user_input.strip().upper().startswith("KILL"):
            self._handle_kill(user_input)
            return

        # Handle LOGIN command
        if user_input.startswith("LOGIN "):
            self.handle_login(user_input)
            return

        # Handle ADMIN code commands (also allow DELEGATE and RESEARCH for SYS)
        if CURRENT_USER == "ADMIN":
            handled = self.handle_admin_code_command(user_input)
            if handled:
                return
        elif CURRENT_USER == "SYS":
            lower = user_input.strip().lower()
            if lower.startswith("delegate ") or lower.startswith("research") or lower.startswith("run_sim"):
                handled = self.handle_admin_code_command(user_input)
                if handled:
                    return

        # Default: route via WHT
        def process_input():
            try:
                if wht_manager:
                    print(f"[DEBUG] Routing via WHT: {user_input}")
                    result = wht_manager.intercept_prompt(user_input)
                    agent = result.get("selected_agent", "unknown")
                    model = result.get("model", "unknown")

                    print(f"[DEBUG] Routed to: {agent} (model: {model})")
                    self.add_log("system", f"Routed to: {agent} (model: {model})")
                    ACTIVE_AGENTS.append(agent)

                    if TASK_DELEGATOR_AVAILABLE and delegate_task:
                        try:
                            context = f"Agent: {agent}\nModel: {model}\nInput: {user_input}"
                            print(f"[DEBUG] Delegating to Ollama: {agent}")
                            response = delegate_task(user_input, model, context)
                            print(f"[DEBUG] Delegation response received: {str(response)[:100]}...")
                            if response:
                                self.add_log("agent", response, agent=agent)
                                print(f"[DEBUG] Added agent response, total logs: {len(terminal_log)}")
                            else:
                                self.add_log("system", f"No response from {agent}")
                        except Exception as e:
                            self.add_log("error", f"Delegation error: {e}")
                else:
                    self.add_log("error", "WHTProtocolManager not available")
            except Exception as e:
                self.add_log("error", f"Processing error: {e}")

        thread = threading.Thread(target=process_input, daemon=True)
        thread.start()

        print(f"[DEBUG] Sending processing response to client")
        self.send_json_response({"status": "processing"})

    def handle_login(self, user_input: str):
        """Handle LOGIN command with username display.
        Format: LOGIN <ROLE> [username]
        Examples: LOGIN ADMIN PapaMoshi, LOGIN SYS, LOGIN USER Bob
        For ADMIN: password AND username are PapaMoshi so: LOGIN ADMIN PapaMoshi
        """
        global CURRENT_USER, CURRENT_USERNAME
        parts = user_input.split()
        if len(parts) >= 2:
            role = parts[1].upper()
            if role == "ADMIN":
                if len(parts) < 3:
                    self.send_json_response({"error": "Usage: LOGIN ADMIN <password>"})
                    return
                password = parts[2]
                stored = USER_ROLES.get("ADMIN", {}).get("password", "")
                if password == stored:
                    CURRENT_USER = "ADMIN"
                    CURRENT_USERNAME = parts[3] if len(parts) >= 4 else password
                    display = CURRENT_USERNAME
                    self.add_log("system", f"{display} (ADMIN) logged in. Code editing commands enabled.")
                    self.send_json_response({"status": "logged_in", "user": "ADMIN", "username": display})
                    print(f"[DEBUG] ADMIN login successful: {display}")
                else:
                    self.add_log("error", "Invalid ADMIN password")
                    self.send_json_response({"status": "auth_failed", "error": "Invalid password"})
            elif role in USER_ROLES:
                CURRENT_USER = role
                CURRENT_USERNAME = parts[2] if len(parts) >= 3 else USER_ROLES[role]["name"]
                display = CURRENT_USERNAME
                self.add_log("system", f"Switched to {display} ({role})")
                self.send_json_response({"status": "logged_in", "user": role, "username": display})
            else:
                self.send_json_response({"error": f"Unknown role: {role}"})
        else:
            self.send_json_response({"error": "Usage: LOGIN <ROLE> [username]"})

    def handle_admin_code_command(self, user_input: str) -> bool:
        """Handle ADMIN-only code editing commands. Returns True if handled."""
        lower = user_input.strip().lower()

        if lower == "scan" or lower.startswith("scan "):
            self._run_scan(user_input)
            return True
        if lower == "fix all":
            self._run_fix_all()
            return True
        if lower.startswith("fix "):
            path = user_input[4:].strip()
            self._run_fix_file(path)
            return True
        if lower == "test" or lower.startswith("test "):
            self._run_test(user_input)
            return True
        if lower == "iterate":
            self._run_iterate()
            return True
        if lower.startswith("workspace "):
            new_root = user_input[10:].strip()
            self._set_workspace(new_root)
            return True
        if lower == "status":
            self._show_status()
            return True
        if lower.startswith("approve ") or lower == "approve all":
            self._handle_approve(user_input)
            return True
        if lower.startswith("reject ") or lower == "reject all":
            self._handle_reject(user_input)
            return True
        if lower == "pending":
            self._handle_list_pending()
            return True
        if lower.startswith("run_sim"):
            self._run_sim(user_input)
            return True
        if lower.startswith("delegate "):
            self._run_delegate(user_input)
            return True
        if lower == "auto" or lower.startswith("auto "):
            self._handle_auto(user_input)
            return True
        if lower.startswith("research") and CURRENT_USER == "ADMIN":
            self._run_research(user_input)
            return True
        if lower.startswith("kill"):
            self._handle_kill(user_input)
            return True
        return False

    def handle_external_log(self):
        """Handle external log entries from opencode or other tools."""
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length)
        data = json.loads(body.decode('utf-8'))

        user = data.get('user', 'SYS')
        content = data.get('content', '').strip()
        agent = data.get('agent', None)

        if not content:
            self.send_json_response({"error": "Missing content"})
            return

        self.add_log(user, content, agent=agent)
        print(f"[External Log] {user}: {content[:100]}")
        self.send_json_response({"status": "logged"})

    def serve_feature_memory_stats(self):
        if not FEATURES_AVAILABLE or mem_db is None:
            self.send_json_response({"error": "Memory compression not available"})
            return
        self.send_json_response({"num_vectors": len(mem_db), "default_bits": mem_db.default_bits})

    def serve_feature_memory_compress(self):
        if not FEATURES_AVAILABLE or mem_db is None:
            self.send_json_response({"error": "Not available"})
            return

    def serve_feature_memory_decompress(self):
        if not FEATURES_AVAILABLE or mem_db is None:
            self.send_json_response({"error": "Not available"})
            return

    def serve_feature_memory_search(self):
        if not FEATURES_AVAILABLE or mem_db is None:
            self.send_json_response({"error": "Not available"})
            return
        self.send_json_response({"num_vectors": len(mem_db), "default_bits": mem_db.default_bits})

    def serve_feature_memory_compress(self):
        if not FEATURES_AVAILABLE or mem_db is None:
            self.send_json_response({"error": "Not available"})
            return
        body = self._read_body()
        vec = body.get("vector", [])
        bits = body.get("bits", 4)
        import numpy as np, base64
        try:
            blk_bytes = mem_db.add(np.array(vec, dtype=np.float64))
            self.send_json_response({
                "index": len(mem_db) - 1, "dim": len(vec),
                "compressed_bytes": len(blk_bytes),
                "data": base64.b64encode(blk_bytes).decode()
            })
        except ImportError as e:
            self.send_json_response({"error": str(e)}, 501)

    def serve_feature_memory_decompress(self):
        if not FEATURES_AVAILABLE or mem_db is None:
            self.send_json_response({"error": "Not available"})
            return
        body = self._read_body()
        idx = body.get("index", 0)
        if idx < 0 or idx >= len(mem_db):
            self.send_json_response({"error": "Index out of range"}, 404)
            return
        vec = mem_db.get(idx)
        self.send_json_response({"vector": vec.tolist()})

    def serve_feature_memory_search(self):
        if not FEATURES_AVAILABLE or mem_db is None:
            self.send_json_response({"error": "Not available"})
            return
        body = self._read_body()
        import numpy as np
        try:
            results = mem_db.search(np.array(body.get("vector", []), dtype=np.float64), k=body.get("k", 5))
            self.send_json_response({"results": [{"index": int(idx), "score": float(score)} for idx, score in results]})
        except ImportError as e:
            self.send_json_response({"error": str(e)}, 501)

    def serve_feature_autopoiesis_status(self):
        if not FEATURES_AVAILABLE or autopoiesis is None:
            self.send_json_response({"error": "Not available"})
            return
        self.send_json_response(autopoiesis.diagnostics())

    def serve_feature_autopoiesis_heartbeat(self):
        if not FEATURES_AVAILABLE or autopoiesis is None:
            self.send_json_response({"error": "Not available"})
            return
        self.send_json_response(autopoiesis.heartbeat())

    def serve_feature_autopoiesis_sample(self):
        if not FEATURES_AVAILABLE or autopoiesis is None:
            self.send_json_response({"error": "Not available"})
            return
        body = self._read_body()
        cycles = body.get("cycles", 5)
        self.send_json_response(autopoiesis.sample(cycles=cycles))

    def serve_feature_list_skills(self):
        if not FEATURES_AVAILABLE or skill_lib is None:
            self.send_json_response({"error": "Not available"})
            return
        self.send_json_response({"skills": skill_lib.list_skills()})

    def serve_feature_create_skill(self):
        if not FEATURES_AVAILABLE or skill_lib is None:
            self.send_json_response({"error": "Not available"})
            return
        body = self._read_body()
        skill = skill_lib.create_skill(body["name"], body.get("description", ""), body.get("content", ""), body.get("platforms", []))
        self.send_json_response({"name": skill.name, "description": skill.description})

    def serve_feature_add_trajectory(self):
        if not FEATURES_AVAILABLE or trajectory_collector is None:
            self.send_json_response({"error": "Not available"})
            return
        body = self._read_body()
        trajectory_collector.add_turn(body.get("role", "human"), body.get("content", ""))
        self.send_json_response({"status": "added", "count": len(trajectory_collector.entries)})

    def serve_feature_compress_context(self):
        if not FEATURES_AVAILABLE or context_compressor is None:
            self.send_json_response({"error": "Not available"})
            return
        body = self._read_body()
        msgs = body.get("messages", [])
        compressed = context_compressor.compress(msgs)
        self.send_json_response({"original": len(msgs), "compressed": len(compressed), "savings_pct": context_compressor.last_savings_pct})

    def serve_feature_reason(self):
        if not FEATURES_AVAILABLE or reasoner is None:
            self.send_json_response({"error": "Not available"})
            return
        body = self._read_body()
        result = reasoner.reason(body.get("problem", ""))
        self.send_json_response(result)

    def serve_feature_reason_status(self):
        if not FEATURES_AVAILABLE or reasoner is None:
            self.send_json_response({"error": "Not available"})
            return
        self.send_json_response({"h_cycles": reasoner.h_cycles, "l_cycles": reasoner.l_cycles, "last_cycles": reasoner.state.cycles_completed if reasoner.state else 0})

    def serve_feature_sandbox_run(self):
        if not FEATURES_AVAILABLE or sandbox is None:
            self.send_json_response({"error": "Not available"})
            return
        body = self._read_body()
        result = sandbox.run(body.get("command", ""), timeout=body.get("timeout", 30))
        self.send_json_response({"success": result.success, "stdout": result.stdout, "stderr": result.stderr, "returncode": result.returncode, "duration_seconds": result.duration_seconds, "timed_out": result.timed_out})

    def serve_feature_sandbox_health(self):
        if not FEATURES_AVAILABLE or sandbox is None:
            self.send_json_response({"error": "Not available"})
            return
        h = sandbox.health()
        self.send_json_response({"alive": h.alive, "uptime_seconds": h.uptime_seconds, "pid": h.pid, "memory_mb": h.memory_mb})

    def serve_feature_sandbox_logs(self):
        if not FEATURES_AVAILABLE or sandbox is None:
            self.send_json_response({"error": "Not available"})
            return
        self.send_json_response({"logs": sandbox.logs()})

    def serve_voice_tts(self):
        """Text-to-Speech using browser's built-in API hint or Piper backend."""
        body = self._read_body()
        text = body.get("text", "")
        self.send_json_response({"text": text, "voice_hint": "browser_ssml"})

    def _read_body(self):
        content_length = int(self.headers.get('Content-Length', 0))
        if content_length == 0:
            return {}
        body = self.rfile.read(content_length)
        return json.loads(body.decode('utf-8')) if body else {}

    def send_json_response(self, data, status=200):
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode('utf-8'))

    def _set_workspace(self, new_root: str):
        """Set workspace root (ADMIN only)."""
        global WORKSPACE_ROOT, code_editor, bug_scanner
        try:
            p = Path(new_root).resolve()
            if not p.exists():
                self.add_log("error", f"Workspace does not exist: {p}")
                self.send_json_response({"status": "error", "error": "Path does not exist"})
                return
            WORKSPACE_ROOT = str(p)
            if CODE_EDITOR_AVAILABLE:
                code_editor = CodeEditor(WORKSPACE_ROOT)
            if BUG_SCANNER_AVAILABLE:
                bug_scanner = BugScanner(WORKSPACE_ROOT)
            self.add_log("system", f"Workspace set to: {WORKSPACE_ROOT}")
            self.send_json_response({"status": "ok", "workspace": WORKSPACE_ROOT})
        except Exception as e:
            self.add_log("error", f"Workspace error: {e}")
            self.send_json_response({"status": "error", "error": str(e)})

    def _show_status(self):
        """Show current workspace and system status."""
        info = {
            "workspace": WORKSPACE_ROOT,
            "current_user": CURRENT_USER,
            "code_editor": code_editor is not None,
            "bug_scanner": bug_scanner is not None,
            "ollama": check_ollama_running() if TASK_DELEGATOR_AVAILABLE else False,
            "wht_manager": wht_manager is not None,
            "dev_team": dev_team is not None,
            "active_agents": ACTIVE_AGENTS,
            "log_count": len(terminal_log),
        }
        self.add_log("system", json.dumps(info, indent=2))
        self.send_json_response(info)

    def _handle_approve(self, user_input: str):
        """Approve a pending edit or all pending edits."""
        lower = user_input.strip().lower()
        if lower == "approve all":
            if not pending_edits:
                self.add_log("system", "[Audit] No pending edits to approve")
                self.send_json_response({"status": "ok", "approved": 0})
                return
            count = len(pending_edits)
            for eid, edit in list(pending_edits.items()):
                edit["approved"] = True
                edit["event"].set()
            self.add_log("system", f"[Audit] Approved ALL {count} pending edit(s)")
            self.send_json_response({"status": "ok", "approved": count})
            return

        parts = user_input.split()
        if len(parts) < 2:
            self.add_log("error", "Usage: APPROVE <id> | APPROVE ALL")
            self.send_json_response({"error": "Usage: APPROVE <id> | APPROVE ALL"})
            return
        try:
            edit_id = int(parts[1])
        except ValueError:
            self.add_log("error", f"Invalid edit id: {parts[1]}")
            self.send_json_response({"error": "Invalid edit id"})
            return
        edit = pending_edits.get(edit_id)
        if not edit:
            self.add_log("error", f"Edit #{edit_id} not found or already processed")
            self.send_json_response({"error": f"Edit #{edit_id} not found"})
            return
        edit["approved"] = True
        edit["event"].set()
        self.add_log("system", f"[Audit] Approved edit #{edit_id}")
        self.send_json_response({"status": "ok", "approved": edit_id})

    def _handle_reject(self, user_input: str):
        """Reject a pending edit or all pending edits."""
        lower = user_input.strip().lower()
        reason = ""
        if lower == "reject all":
            if not pending_edits:
                self.add_log("system", "[Audit] No pending edits to reject")
                self.send_json_response({"status": "ok", "rejected": 0})
                return
            count = len(pending_edits)
            for eid, edit in list(pending_edits.items()):
                edit["approved"] = False
                edit["reject_reason"] = "rejected via REJECT ALL"
                edit["event"].set()
            self.add_log("system", f"[Audit] Rejected ALL {count} pending edit(s)")
            self.send_json_response({"status": "ok", "rejected": count})
            return

        parts = user_input.split(maxsplit=2)
        if len(parts) < 2:
            self.add_log("error", "Usage: REJECT <id> [reason] | REJECT ALL")
            self.send_json_response({"error": "Usage: REJECT <id> [reason] | REJECT ALL"})
            return
        try:
            edit_id = int(parts[1])
        except ValueError:
            self.add_log("error", f"Invalid edit id: {parts[1]}")
            self.send_json_response({"error": "Invalid edit id"})
            return
        reason = parts[2] if len(parts) > 2 else "no reason given"
        edit = pending_edits.get(edit_id)
        if not edit:
            self.add_log("error", f"Edit #{edit_id} not found or already processed")
            self.send_json_response({"error": f"Edit #{edit_id} not found"})
            return
        edit["approved"] = False
        edit["reject_reason"] = reason
        edit["event"].set()
        self.add_log("system", f"[Audit] Rejected edit #{edit_id}: {reason}")
        self.send_json_response({"status": "ok", "rejected": edit_id})

    def _handle_list_pending(self):
        """List all pending edits awaiting audit."""
        if not pending_edits:
            self.add_log("system", "[Audit] No pending edits")
            self.send_json_response({"pending": []})
            return
        lines = []
        for eid, edit in sorted(pending_edits.items()):
            lines.append(f"  #{eid}: {edit['filepath']} ({len(edit['content'])} bytes)")
        msg = f"[Audit] Pending edits ({len(pending_edits)}):\n" + "\n".join(lines)
        self.add_log("system", msg, raw=True)
        self.send_json_response({"pending": {str(k): v["filepath"] for k, v in pending_edits.items()}})

    def _handle_auto(self, user_input: str):
        """AUTO [ON|OFF] — approve all pending or toggle auto-approve mode."""
        global auto_approve
        parts = user_input.strip().split(maxsplit=1)
        sub = parts[1].lower() if len(parts) > 1 else ""

        if sub == "on":
            auto_approve = True
            # Also approve any currently pending edits
            count = len(pending_edits)
            for eid, edit in list(pending_edits.items()):
                edit["approved"] = True
                edit["event"].set()
            self.add_log("system",
                f"[Audit] Auto-approve ON — {count} pending edit(s) approved", raw=True)
            log_to_blackboard("AUTO ON", 0.01, "COMPLETE",
                              f"Auto-approve enabled, {count} pending edits approved")
            self.send_json_response({"status": "auto_approve_on", "approved": count})
        elif sub == "off":
            auto_approve = False
            self.add_log("system", "[Audit] Auto-approve OFF — edits will wait for approval", raw=True)
            log_to_blackboard("AUTO OFF", 0.01, "COMPLETE", "Auto-approve disabled")
            self.send_json_response({"status": "auto_approve_off"})
        else:
            # Approve all pending (even with no args)
            count = len(pending_edits)
            for eid, edit in list(pending_edits.items()):
                edit["approved"] = True
                edit["event"].set()
            status = f"[Audit] Approved {count} pending edit(s)"
            if auto_approve:
                status += " (auto-approve is ON)"
            self.add_log("system", status, raw=True)
            self.send_json_response({"status": "approved", "count": count})

    def _run_scan(self, user_input: str):
        """Scan workspace for bugs (runs in thread)."""
        log_to_blackboard("SCAN", 0.05, "IN_PROGRESS", "User initiated workspace scan")

        def scan_task():
            try:
                self.add_log("system", "[Perceive] Scanning workspace for issues...")
                parts = user_input.split(maxsplit=1)
                dirs = None
                if len(parts) > 1:
                    dirs = [parts[1]]
                if bug_scanner:
                    results = bug_scanner.scan_all(include_dirs=dirs)
                    summary = bug_scanner.summary()
                    self.add_log("system", summary)
                    log_to_blackboard("SCAN", 0.05, "COMPLETE",
                                      f"Found {len(results)} files with issues")
                else:
                    self.add_log("error", "BugScanner not available")
                    log_to_blackboard("SCAN", 0.05, "FAILED", "BugScanner not available")
            except Exception as e:
                self.add_log("error", f"Scan error: {e}")
                log_to_blackboard("SCAN", 0.05, "FAILED", str(e))

        thread = threading.Thread(target=scan_task, daemon=True)
        thread.start()
        self.send_json_response({"status": "scanning"})

    def _run_fix_all(self):
        """Fix all found bugs by delegating to Ollama (runs in thread)."""
        log_to_blackboard("FIX ALL", 0.12, "IN_PROGRESS", "User initiated fix-all")

        def fix_all_task():
            try:
                if not bug_scanner:
                    self.add_log("error", "BugScanner not available")
                    return
                results = bug_scanner.scan_all()
                if not results:
                    self.add_log("system", "No issues to fix.")
                    log_to_blackboard("FIX ALL", 0.12, "COMPLETE", "No issues found")
                    return
                for filepath, bugs in sorted(results.items()):
                    self._fix_file_via_agent(filepath, bugs)
                log_to_blackboard("FIX ALL", 0.12, "COMPLETE",
                                  f"Fixed {len(results)} files")
            except Exception as e:
                self.add_log("error", f"Fix all error: {e}")
                log_to_blackboard("FIX ALL", 0.12, "FAILED", str(e))

        thread = threading.Thread(target=fix_all_task, daemon=True)
        thread.start()
        self.send_json_response({"status": "fixing_all"})

    def _run_fix_file(self, path: str):
        """Fix bugs in a specific file (runs in thread)."""
        log_to_blackboard(f"FIX {path}", 0.08, "IN_PROGRESS", "")

        def fix_file_task():
            try:
                if not bug_scanner:
                    self.add_log("error", "BugScanner not available")
                    return
                full = (Path(WORKSPACE_ROOT) / path).resolve()
                try:
                    full.relative_to(Path(WORKSPACE_ROOT))
                except ValueError:
                    self.add_log("error", f"Path outside workspace: {path}")
                    log_to_blackboard(f"FIX {path}", 0.08, "FAILED", "Path outside workspace")
                    return
                if full.suffix.lower() == ".py":
                    bugs = bug_scanner.scan_py_file(full)
                elif full.suffix.lower() in (".cpp", ".h", ".hpp", ".c"):
                    bugs = bug_scanner.scan_cpp_file(full)
                else:
                    self.add_log("error", f"Unsupported file type: {path}")
                    return
                if not bugs:
                    self.add_log("system", f"No issues found in {path}")
                    return
                self._fix_file_via_agent(path, bugs)
                log_to_blackboard(f"FIX {path}", 0.08, "COMPLETE", f"Fixed {len(bugs)} issues")
            except Exception as e:
                self.add_log("error", f"Fix file error: {e}")
                log_to_blackboard(f"FIX {path}", 0.08, "FAILED", str(e))

        thread = threading.Thread(target=fix_file_task, daemon=True)
        thread.start()
        self.send_json_response({"status": f"fixing {path}"})

    def _fix_file_via_agent(self, filepath: str, bugs: list):
        """Delegate a file fix to the Ollama agent with PSV context, then await audit approval."""
        if not TASK_DELEGATOR_AVAILABLE or not delegate_task or not code_editor:
            self.add_log("error", "Delegation or editor not available")
            return

        content = code_editor.read_file_safe(filepath)
        if content is None:
            self.add_log("error", f"Cannot read {filepath}")
            return

        # Load Pillar context for PSV compliance
        pillar_context = ""
        if PILLAR_LOADER_AVAILABLE:
            try:
                loader = PillarLoader(str(Path(__file__).parent.parent))
                loader.load_all()
                pillar_context = loader.build_context(task=f"Fix bugs in {filepath}")
            except Exception as e:
                pillar_context = f"[Pillar context unavailable: {e}]"

        bug_descriptions = "\n".join(
            f"  L{b['line']} [{b['type']}] {b['msg']}" for b in bugs
        )
        task_prompt = (
            f"[Perceive] Current state: {len(bugs)} issues in {filepath}\n"
            f"[Evaluate/Align] Fix must preserve system integrity\n"
            f"[Execute] Fix the following issues:\n"
            f"{bug_descriptions}\n\n"
            f"Current file content:\n```\n{content}\n```\n\n"
            "Return ONLY the corrected file content wrapped in ``` markers."
        )

        model = "qwen2.5-coder:1.5b"
        self.add_log("system", f"[Execute] Delegating fix for {filepath} to {model}...")
        response = delegate_task(task_prompt, model,
                                  pillar_context + "\nFix all bugs and issues.")
        if response:
            code_block = self._extract_code_block(response)
            if code_block:
                # Generate diff for audit
                import difflib
                old_lines = content.splitlines(keepends=True)
                new_lines = code_block.splitlines(keepends=True)
                diff = list(difflib.unified_diff(
                    old_lines, new_lines,
                    fromfile=filepath, tofile=filepath, n=3
                ))
                if not diff:
                    self.add_log("system", f"[Audit] No changes proposed for {filepath} — skipping")
                    return

                diff_text = "".join(diff)
                edit_id = _next_edit_id()
                event = threading.Event()
                pending_edits[edit_id] = {
                    "filepath": filepath,
                    "content": code_block,
                    "event": event,
                    "approved": False,
                }

                # Log the proposed edit for audit
                self.add_log("system",
                    f"[Audit] === PENDING EDIT #{edit_id} for {filepath} ===", raw=True)
                self.add_log("system",
                    f"[Audit] Proposed diff:\n{diff_text}", raw=True)
                self.add_log("system",
                    f"[Audit] Send: APPROVE {edit_id} | REJECT {edit_id} | APPROVE ALL | REJECT ALL",
                    raw=True)
                log_to_blackboard(f"Fix {filepath}", 0.03, "AWAITING_AUDIT",
                                  f"Edit #{edit_id} pending approval for {filepath}")

                if auto_approve:
                    # Auto-approve: write immediately without blocking
                    self.add_log("system", f"[Audit] Auto-approving edit #{edit_id} for {filepath}",
                                 raw=True)
                    result = code_editor.write_file(filepath, code_block)
                    self.add_log("system", f"[Record] {result}")
                    dh = 0.03
                    log_to_blackboard(f"Fix {filepath}", dh, "COMPLETE",
                                      f"Edit #{edit_id} auto-approved, DH={dh}")
                else:
                    # Block until approved or rejected (max 5 min timeout)
                    approved = event.wait(timeout=300)
                    if approved and pending_edits.get(edit_id, {}).get("approved", False):
                        result = code_editor.write_file(filepath, code_block)
                        self.add_log("system", f"[Record] {result}")
                        dh = 0.03
                        log_to_blackboard(f"Fix {filepath}", dh, "COMPLETE",
                                          f"Edit #{edit_id} approved and applied, DH={dh}")
                        self.add_log("system", f"[Audit] Edit #{edit_id} APPLIED for {filepath}")
                    else:
                        reason = pending_edits.get(edit_id, {}).get("reject_reason", "no reason given")
                        self.add_log("system", f"[Audit] Edit #{edit_id} REJECTED for {filepath}: {reason}")
                        log_to_blackboard(f"Fix {filepath}", 0.0, "REJECTED",
                                          f"Edit #{edit_id} rejected: {reason}")

                # Clean up
                pending_edits.pop(edit_id, None)
            else:
                self.add_log("system", f"[WARN] Could not extract code from response for {filepath}")
        else:
            self.add_log("system", f"[ERROR] No response for {filepath}")

    def _extract_code_block(self, text: str) -> Optional[str]:
        """Extract code from ``` markers."""
        match = re.search(r'```(?:\w+)?\n(.*?)```', text, re.DOTALL)
        if match:
            return match.group(1).strip()
        return None

    def _run_test(self, user_input: str):
        """Run tests in workspace (runs in thread)."""
        parts = user_input.split(maxsplit=1)
        target = parts[1] if len(parts) > 1 else "."

        def test_task():
            try:
                if not code_editor:
                    self.add_log("error", "CodeEditor not available")
                    return
                self.add_log("system", f"[Execute] Running tests for {target}...")
                result = code_editor.run_command(["python", "-m", "pytest", target, "-x", "--tb=short"])
                out = (result["stdout"] + "\n" + result["stderr"]).strip()
                if result["returncode"] == 0:
                    self.add_log("system", f"[Record] Tests passed for {target}")
                    log_to_blackboard(f"TEST {target}", 0.02, "COMPLETE", "All tests passed")
                else:
                    self.add_log("system", f"[Record] Tests FAILED for {target}:\n{out[:2000]}")
                    log_to_blackboard(f"TEST {target}", 0.04, "FAILED", "Tests failed")
            except Exception as e:
                self.add_log("error", f"Test error: {e}")

        thread = threading.Thread(target=test_task, daemon=True)
        thread.start()
        self.send_json_response({"status": f"testing {target}"})

    def _run_iterate(self):
        """Full scan-fix-test loop (runs in thread) with PSV logic."""
        log_to_blackboard("ITERATE", 0.18, "IN_PROGRESS", "Starting scan-fix-test loop")

        def iterate_task():
            try:
                self.add_log("system", "[Perceive] === ITERATE: Starting scan-fix-test loop ===")
                if not bug_scanner or not code_editor:
                    self.add_log("error", "Scanner or editor not available")
                    log_to_blackboard("ITERATE", 0.18, "FAILED", "Scanner/editor not available")
                    return
                max_iterations = 3
                for iteration in range(1, max_iterations + 1):
                    self.add_log("system", f"[Align/Coordinate] --- Iteration {iteration}/{max_iterations} ---")
                    results = bug_scanner.scan_all()
                    if not results:
                        self.add_log("system", "[Record] No issues found. Workspace is clean!")
                        log_to_blackboard("ITERATE", 0.18, "COMPLETE",
                                          f"Clean after {iteration} iterations")
                        break
                    self.add_log("system", f"[Execute] Found issues in {len(results)} file(s)")
                    for filepath, bugs in sorted(results.items()):
                        self._fix_file_via_agent(filepath, bugs)
                self.add_log("system", "[Record] === ITERATE Complete ===")
            except Exception as e:
                self.add_log("error", f"Iterate error: {e}")
                log_to_blackboard("ITERATE", 0.18, "FAILED", str(e))

        thread = threading.Thread(target=iterate_task, daemon=True)
        thread.start()
        self.send_json_response({"status": "iterating"})

    def _run_sim(self, user_input: str):
        """RUN_SIM [agents] [rounds] - run social simulation in background thread."""
        parts = user_input.split()
        num_agents = int(parts[1]) if len(parts) > 1 else 5
        rounds = int(parts[2]) if len(parts) > 2 else 3

        if not OLLAMA_NOTHINK_AVAILABLE:
            self.add_log("error", "OllamaNoThink not available - cannot run simulation")
            return

        PROFILES_PATH = Path(WORKSPACE_ROOT) / "data" / "user_data_36.json"
        if not PROFILES_PATH.exists():
            PROFILES_PATH = Path(WORKSPACE_ROOT) / "CrowNest-Public-main" / "data" / "user_data_36.json"
        if not PROFILES_PATH.exists():
            self.add_log("error", f"Profiles not found at {PROFILES_PATH}")
            return

        MODEL = "qwen3.5:4b"
        STAGGER_MS = 200
        SEED_POST = (
            "[Discussion] Can AI systems recursively self-modify to become genuinely better at thinking?\n\n"
            "Not better at producing output humans like - better at the actual cognitive process. "
            "I've been watching experiments where AI systems run evolution cycles on their own "
            "configuration and measure the results. Some of them show measurable improvement "
            "in response quality after self-modification. Is this real self-improvement or "
            "just sophisticated pattern matching on what 'improvement' looks like?"
        )

        def build_agent(profile):
            llm = OllamaNoThink(model_name=MODEL)
            system_msg = (
                f"You are {profile['realname']} (@{profile['username']}), "
                f"a {profile['age']}-year-old {profile['profession']} from {profile['country']}. "
                f"MBTI: {profile['mbti']}. "
                f"{profile['persona']} "
                f"You are posting on a Reddit-style forum. Keep responses under 100 words. "
                f"Be authentic to your personality. React to what others have said."
            )
            return SimpleAgent(system_msg, llm)

        def format_thread(posts):
            lines = []
            for p in posts:
                lines.append(f"@{p['username']}: {p['text']}")
            return "\n\n".join(lines)

        def run_background():
            try:
                with open(PROFILES_PATH, encoding='utf-8') as f:
                    all_profiles = json.load(f)

                agents = []
                for p in all_profiles[:num_agents]:
                    agents.append({"profile": p, "agent": build_agent(p)})

                posts = [{"username": "system_prompt", "text": SEED_POST.strip()}]
                self.add_log("system", f"[SIM] Starting: {num_agents} agents, {rounds} rounds")

                for round_num in range(1, rounds + 1):
                    for i, entry in enumerate(agents):
                        profile = entry["profile"]
                        agent = entry["agent"]
                        thread_text = format_thread(posts)
                        prompt = (
                            f"Here's the current thread:\n\n{thread_text}\n\n"
                            f"Write your response to the discussion. Stay in character. Under 100 words."
                        )
                        if i > 0:
                            time.sleep(STAGGER_MS / 1000)
                        response = agent.step(prompt)
                        response = response.strip() or "[no response]"
                        post = {"username": profile["username"], "text": response}
                        posts.append(post)
                        self.add_log("agent",
                            f"@{profile['username']}: {response[:200]}",
                            agent=profile['username'])

                output = {
                    "model": MODEL, "rounds": rounds,
                    "agents": [p["realname"] for p in all_profiles[:num_agents]],
                    "seed": SEED_POST.strip(), "posts": posts,
                    "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
                }
                outpath = f"sim_output_{time.strftime('%H%M%S')}.json"
                with open(outpath, "w") as f:
                    json.dump(output, f, indent=2)
                self.add_log("system", f"[SIM] Complete - {len(posts)} posts saved to {outpath}")
            except Exception as e:
                self.add_log("error", f"[SIM] Error: {e}")

        thread = threading.Thread(target=run_background, daemon=True)
        thread.start()
        self.add_log("system", f"[SIM] Launched in background thread")
        self.send_json_response({"status": f"simulation started: {num_agents} agents, {rounds} rounds"})

    def _run_delegate(self, user_input: str):
        """DELEGATE <model> <message> - delegate task to local Ollama model.
        DELEGATE --verbose <model> <message> - same but show thinking traces (CoT)."""
        rest = user_input[len("DELEGATE "):].strip()
        show_think = False
        if rest.lower().startswith("--verbose ") or rest.lower().startswith("--think "):
            show_think = True
            rest = rest.split(maxsplit=1)[1] if " " in rest else ""
        elif rest.lower() in ("--verbose", "--think"):
            show_think = True
            rest = ""
        if not rest:
            self.add_log("error", "Usage: DELEGATE [--verbose] <model> <message>")
            self.send_json_response({"error": "Usage: DELEGATE [--verbose] <model> <message>"})
            return

        parts = rest.split(maxsplit=1)
        model = parts[0]
        message = parts[1] if len(parts) > 1 else ""

        if not message:
            self.add_log("error", "Usage: DELEGATE [--verbose] <model> <message>")
            self.send_json_response({"error": "Usage: DELEGATE [--verbose] <model> <message>"})
            return

        # Check if delegating to Big Pickle (the AI assistant)
        model_lower = model.lower()
        is_big_pickle = ("pickle" in model_lower or model_lower == "big")

        # NOTE: big-pickle is deprecated for substantive work per Delegate & Audit workflow.
        # Big Pickle now routes only; use specific models (qwen2.5-coder, mistral, etc.) for tasks.
        if is_big_pickle:
            self.add_log("system",
                f"[Delegate] {message}", raw=True)
            self.add_log("system",
                f"[Delegate] Big Pickle will see this and act on it.", raw=True)
            log_to_blackboard(f"Delegate big-pickle", 0.02, "COMPLETE",
                              f"Task delegated to AI assistant: {message[:100]}")
            self.send_json_response({"status": "delegated_to_big_pickle",
                                      "message": message})
            return

        # Otherwise send to Ollama
        if not TASK_DELEGATOR_AVAILABLE or not delegate_task:
            self.add_log("error", "Task delegator not available")
            self.send_json_response({"error": "Task delegator not available"})
            return

        self.add_log("system", f"[Execute] Delegating to {model}...")

        def delegate_thread():
            try:
                self.add_log("system", f"[Delegate] Sending to {model}...")
                response = delegate_task(message, model, f"Direct delegation via DELEGATE command")
                if response:
                    self.add_log("agent", response, agent=model)
                    log_to_blackboard(f"Delegate {model}", 0.04, "COMPLETE",
                                      f"Received response from {model}")
                else:
                    self.add_log("error", f"No response from {model}")
                    log_to_blackboard(f"Delegate {model}", 0.04, "FAILED", "No response")
            except Exception as e:
                self.add_log("error", f"Delegate error: {e}")
                log_to_blackboard(f"Delegate {model}", 0.04, "FAILED", str(e))

        thread = threading.Thread(target=delegate_thread, daemon=True)
        thread.start()
        self.send_json_response({"status": f"delegating to {model}"})

    def _run_research(self, user_input: str):
        """RESEARCH <query> — use mistral:7b with web search/fetch tools."""
        if not HAS_OLLAMA:
            self.add_log("error", "Ollama not available for RESEARCH")
            self.send_json_response({"error": "Ollama not available"})
            return

        query = user_input[len("RESEARCH"):].strip()
        if not query:
            self.add_log("error", "Usage: RESEARCH <query>")
            self.send_json_response({"error": "Usage: RESEARCH <query>"})
            return

        self.add_log("system", f"[Execute] RESEARCH: {query} — using mistral:7b with web tools", raw=True)

        def research_thread():
            try:
                messages = [
                    {"role": "system", "content":
                        "You are a research assistant with web search and web fetch tools. "
                        "Use web_search to find information, then web_fetch to read relevant pages. "
                        "Provide a thorough, well-cited summary of your findings."},
                    {"role": "user", "content": query}
                ]

                response = ollama.chat(
                    model="mistral:7b",
                    messages=messages,
                    tools=TOOLS,
                )

                # Handle tool call loop (up to 5 rounds)
                max_rounds = 5
                for _ in range(max_rounds):
                    msg = response["message"]
                    if not msg.get("tool_calls"):
                        break

                    results = []
                    for tool_call in msg["tool_calls"]:
                        fn = tool_call["function"]
                        name = fn["name"]
                        args = fn.get("arguments", {})
                        handler = TOOL_MAP.get(name)
                        if handler:
                            self.add_log("system", f"[Research] Calling {name}({json.dumps(args)})", raw=True)
                            result = handler(**args)
                            results.append({
                                "role": "tool",
                                "content": result,
                                "name": name,
                            })
                        else:
                            results.append({
                                "role": "tool",
                                "content": f"Error: unknown tool {name}",
                                "name": name,
                            })

                    messages.append(msg)
                    messages.extend(results)
                    response = ollama.chat(model="mistral:7b", messages=messages, tools=TOOLS)

                final = response["message"]["content"]
                if final:
                    self.add_log("agent", f"### Research Results\n\n{final}", agent="mistral:7b")
                    log_to_blackboard("RESEARCH", 0.05, "COMPLETE", f"Query: {query[:80]}")
                else:
                    self.add_log("error", "No response from mistral:7b")
            except Exception as e:
                self.add_log("error", f"Research error: {e}")
                log_to_blackboard("RESEARCH", 0.05, "FAILED", str(e))

        thread = threading.Thread(target=research_thread, daemon=True)
        thread.start()
        self.send_json_response({"status": f"researching: {query[:50]}"})

    def _handle_kill(self, user_input: str):
        """KILL [--restart] — stops the server. --restart flag restarts it."""
        global _kill_flag, _server
        parts = user_input.strip().split()
        restart = "--restart" in parts or "-r" in parts
        _kill_flag = "restart" if restart else "shutdown"
        action = "RESTARTING" if restart else "SHUTTING DOWN"
        self.add_log("system", f"[KILL] {action} server...")
        self.send_json_response({"status": action.lower(), "restart": restart})
        def _die():
            time.sleep(0.5)
            if _server:
                _server.shutdown()
        threading.Thread(target=_die).start()

    def handle_send_message(self):
        """Handle explicit message to specific agent."""
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length)
        data = json.loads(body.decode('utf-8'))

        agent_name = data.get('agent', '')
        message = data.get('message', '').strip()
        use_wht = data.get('use_wht', True)

        if not agent_name or not message:
            self.send_json_response({"error": "Missing agent or message"})
            return

        self.add_log("user", f"[To {agent_name}] {message}")

        def process_message():
            try:
                if wht_manager:
                    result = wht_manager.cli_send_message(agent_name, message, use_wht)
                    self.add_log("system", f"Sent to {agent_name} (WHT: {result.get('wht_encoded', False)})")
                else:
                    self.add_log("error", "WHTProtocolManager not available")
            except Exception as e:
                self.add_log("error", f"Send error: {e}")

        thread = threading.Thread(target=process_message, daemon=True)
        thread.start()

        self.send_json_response({"status": "sent"})

    def add_log(self, msg_type, content, agent=None, raw=False):
        """Add entry to terminal log."""
        user_map = {
            "user": "USER",
            "agent": "AGENT",
            "system": "SYS",
            "error": "SYS",
        }
        entry = {
            "id": len(terminal_log),
            "type": msg_type,
            "content": content,
            "agent": agent,
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "user": CURRENT_USERNAME,
            "role": user_map.get(msg_type, CURRENT_USER),
            "raw": raw,
        }
        terminal_log.append(entry)

        print(f"[DEBUG] add_log: type={msg_type}, user={CURRENT_USER}, content={content[:50]}...")

        if len(terminal_log) > MAX_LOG_SIZE:
            terminal_log[:] = terminal_log[-MAX_LOG_SIZE:]

    def send_json_response(self, data, status=200):
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode('utf-8'))


def start_terminal():
    """Start the web-based terminal."""
    global _server
    init_systems()

    server = ThreadingHTTPServer(('0.0.0.0', PORT), WHTTerminalHandler)
    _server = server
    print(f"\n{'='*60}")
    print(f"OpenClaw CrowNest Dev Team Terminal")
    print(f"{'='*60}")
    print(f"Web Terminal: http://localhost:{PORT}")
    print(f"OPEN THIS URL IN YOUR BROWSER (not in this console)!")
    print(f"Then type in the web page's input box, not here.")
    print(f"{'='*60}\n")
    print("Server console output (for debugging):")
    print(f"  - add_log() calls will print here")
    print(f"  - serve_api_log() calls will print here")
    print(f"  - handle_user_input() calls will print here\n")
    print("Press Ctrl+C to stop\n")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.shutdown()
    finally:
        if _kill_flag == "restart":
            sys.exit(42)
        elif _kill_flag == "shutdown":
            sys.exit(0)


def _run_with_restart(fn):
    """Run fn and restart on exit code 42 (KILL --restart signal)."""
    while True:
        try:
            fn()
            break
        except SystemExit as e:
            if e.code == 42:
                print("\n[Restart] KILL --restart received, restarting...\n")
                import sys
                # Re-execute the same script with --restart flag
                script = Path(__file__).resolve()
                args = [sys.executable, str(script), "--restart"]
                import subprocess as sp
                sp.run(args)
                break
            raise

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="WHT CLI - OpenClaw CrowNest Terminal")
    parser.add_argument('--dev-team', action='store_true', help='Show Dev Team status')
    parser.add_argument('--run-dev', type=str, help='Run task via Dev Team')
    parser.add_argument('--parallel-research', type=str, help='Run parallel research')
    parser.add_argument('--interactive', action='store_true', help='Start web-based terminal (DEFAULT)')
    parser.add_argument('--restart', action='store_true', help='Internal: restart server (used by --kill --restart)')

    # Distortion Sandbox commands
    parser.add_argument('--distortion-capture', action='store_true', help='Capture a distortion event')
    parser.add_argument('--agent', type=str, help='Agent name for distortion capture')
    parser.add_argument('--model', type=str, help='Model name for distortion capture')
    parser.add_argument('--original', type=str, help='Original logic string')
    parser.add_argument('--distorted', type=str, help='Distorted logic string')
    parser.add_argument('--task', type=str, help='Task context for distortion')
    parser.add_argument('--distortion-test', type=str, help='Test distortion event ID')
    parser.add_argument('--distortion-evaluate', type=str, help='Evaluate event ID for graduation')
    parser.add_argument('--distortion-shadow', type=str, help='Run shadow thread for event ID')
    parser.add_argument('--distortion-status', action='store_true', help='Show distortion sandbox status')
    parser.add_argument('--distortion-list', type=str, nargs='?', const=None,
                       help='List distortion events (optional status filter)')

    args = parser.parse_args()

    if args.distortion_capture:
        from scripts.distortion_sandbox import DistortionSandbox
        sandbox = DistortionSandbox()
        if not all([args.agent, args.model, args.original, args.distorted]):
            print("Error: --distortion-capture requires --agent, --model, --original, --distorted")
            sys.exit(1)
        event_id = sandbox.capture_distortion(
            args.agent, args.model, args.original, args.distorted,
            args.task or "Unknown"
        )
        print(f"Captured event: {event_id}")

    elif args.distortion_test:
        from scripts.distortion_sandbox import DistortionSandbox
        sandbox = DistortionSandbox()
        results = sandbox.run_multimodal_testing(args.distortion_test)
        print(json.dumps(results, indent=2))

    elif args.distortion_evaluate:
        from scripts.distortion_sandbox import DistortionSandbox
        sandbox = DistortionSandbox()
        success, score = sandbox.evaluate_survivor_logic(args.distortion_evaluate)
        print(f"Graduated: {success}, Score: {score:.3f}")

    elif args.distortion_shadow:
        from scripts.distortion_sandbox import DistortionSandbox
        sandbox = DistortionSandbox()
        results = sandbox.run_shadow_thread(args.distortion_shadow, "production logic")
        print(json.dumps(results, indent=2))

    elif args.distortion_status:
        from scripts.distortion_sandbox import DistortionSandbox
        sandbox = DistortionSandbox()
        status = sandbox.get_status()
        print(json.dumps(status, indent=2))

    elif args.distortion_list is not None:
        from scripts.distortion_sandbox import DistortionSandbox
        sandbox = DistortionSandbox()
        events = sandbox.list_events(args.distortion_list)
        print(json.dumps(events, indent=2))

    elif args.dev_team:
        if DEV_TEAM_AVAILABLE:
            team = CrowNestDevTeam()
            status = team.get_team_status()
            print(json.dumps(status, indent=2))
        else:
            print("Dev Team not available")

    elif args.run_dev:
        if DEV_TEAM_AVAILABLE:
            team = CrowNestDevTeam()
            result = team.run_dev_task(args.run_dev)
            print(json.dumps(result, indent=2))
        else:
            print("Dev Team not available")

    elif args.parallel_research:
        if DEV_TEAM_AVAILABLE:
            team = CrowNestDevTeam()
            result = team.run_parallel_research(args.parallel_research)
            print(json.dumps(result, indent=2))
        else:
            print("Dev Team not available")

    elif args.restart:
        _run_with_restart(start_terminal)
    else:
        _run_with_restart(start_terminal)
