"""
Helper to log actions to the running web terminal via /api/log POST.
Usage:
    python log_terminal.py "message"
    python log_terminal.py --user ADMIN "message"
    python log_terminal.py --agent project_manager "agent response"
"""

import sys
import json
import urllib.request
import urllib.error

SERVER = "http://localhost:8889"


def log(user: str = "SYS", content: str = "", agent: str = None):
    payload = {"user": user, "content": content}
    if agent:
        payload["agent"] = agent
    data = json.dumps(payload).encode("utf-8")
    try:
        req = urllib.request.Request(
            f"{SERVER}/api/log",
            data=data,
            headers={"Content-Type": "application/json"},
        )
        urllib.request.urlopen(req, timeout=5)
    except urllib.error.URLError as e:
        print(f"[log_terminal] Server unreachable: {e}", file=sys.stderr)
    except Exception as e:
        print(f"[log_terminal] Error: {e}", file=sys.stderr)


if __name__ == "__main__":
    args = sys.argv[1:]
    user = "SYS"
    agent = None

    if not args:
        print("Usage: python log_terminal.py [--user ROLE] [--agent NAME] <message>")
        sys.exit(1)

    if args[0] == "--user" and len(args) >= 2:
        user = args[1].upper()
        args = args[2:]
    elif args[0] == "--agent" and len(args) >= 2:
        agent = args[1]
        user = "AGENT"
        args = args[2:]

    log(user=user, content=" ".join(args), agent=agent)
