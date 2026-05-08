#!/usr/bin/env python3
"""
Pillar AI Web UI - Lightweight version using http.server.

No external dependencies beyond PillarLoader.
Serves a simple HTML interface for viewing pillars and running queries.

Usage:
    python src/web_ui_simple.py
    Then open http://localhost:8000
"""

import json
import sys
from pathlib import Path
from http.server import HTTPServer, BaseHTTPRequestHandler
import urllib.parse

if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8')

sys.path.insert(0, str(Path(__file__).parent.parent))

from src.pillar_loader import PillarLoader
from scripts.dependency_graph import build_graph, PILLAR_NAMES

# Global loader
loader = PillarLoader()
loader.load_all()


class WebUIHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)

        if parsed.path == '/' or parsed.path == '/index.html':
            self.serve_index()
        elif parsed.path.startswith('/pillar/'):
            idx = int(parsed.path.split('/')[-1])
            self.serve_pillar_detail(idx)
        elif parsed.path == '/graph':
            self.serve_graph()
        elif parsed.path == '/api/pillars':
            self.serve_api_pillars()
        else:
            self.send_error(404)

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)

        if parsed.path == '/api/query':
            self.handle_query()
        elif parsed.path == '/api/simulate':
            self.handle_simulate()
        else:
            self.send_error(404)

    def serve_index(self):
        """Serve main dashboard."""
        from scripts.dependency_graph import build_graph
        import os

        base_dir = Path(__file__).parent.parent
        interactions_path = base_dir / "Interactions" / "pillar_interactions.md"
        configs = {idx: p.to_dict() for idx, p in loader.pillars.items()}
        graph = build_graph(configs, interactions_path)

        graph_lines = []
        for i in range(16):
            deps = sorted(graph[i])
            dep_names = [PILLAR_NAMES[d] for d in deps]
            graph_lines.append(f"{PILLAR_NAMES[i]:12s} -> {', '.join(dep_names)}")

        graph_text = '\n'.join(graph_lines)

        html = f"""
<!DOCTYPE html>
<html>
<head>
    <title>Pillar AI Colab</title>
    <meta charset="utf-8">
    <style>
        :root {{
            --bg-body: #f5f5f5;
            --bg-section: white;
            --bg-response: #f8f9fa;
            --bg-graph: #f8f9fa;
            --color-text: #333;
            --color-primary: #007bff;
            --color-primary-hover: #0056b3;
            --color-border: #ddd;
            --shadow: 0 2px 4px rgba(0,0,0,0.1);
        }}

        @media (prefers-color-scheme: dark) {{
            :root:not([data-theme]) {{
                --bg-body: #1a1a1a;
                --bg-section: #2d2d2d;
                --bg-response: #3d3d3d;
                --bg-graph: #3d3d3d;
                --color-text: #e0e0e0;
                --color-primary: #4da6ff;
                --color-primary-hover: #3399ff;
                --color-border: #444;
                --shadow: 0 2px 4px rgba(0,0,0,0.3);
            }}
        }}

        [data-theme="dark"] {{
            --bg-body: #1a1a1a;
            --bg-section: #2d2d2d;
            --bg-response: #3d3d3d;
            --bg-graph: #3d3d3d;
            --color-text: #e0e0e0;
            --color-primary: #4da6ff;
            --color-primary-hover: #3399ff;
            --color-border: #444;
            --shadow: 0 2px 4px rgba(0,0,0,0.3);
        }}

        body {{ font-family: Arial, sans-serif; margin: 20px; background: var(--bg-body); color: var(--color-text); }}
        h1, h2 {{ color: var(--color-text); }}
        .container {{ max-width: 1200px; margin: 0 auto; }}
        .section {{ background: var(--bg-section); padding: 20px; margin: 20px 0; border-radius: 8px; box-shadow: var(--shadow); }}
        .pillar-grid {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(250px, 1fr)); gap: 15px; }}
        .pillar-card {{ border: 1px solid var(--color-border); padding: 15px; border-radius: 5px; }}
        .pillar-card h3 {{ margin-top: 0; color: var(--color-primary); }}
        .query-box {{ width: 100%; height: 100px; margin: 10px 0; padding: 10px; box-sizing: border-box; background: var(--bg-section); color: var(--color-text); border: 1px solid var(--color-border); }}
        .btn {{ background: var(--color-primary); color: white; border: none; padding: 10px 20px; cursor: pointer; border-radius: 5px; margin: 5px; }}
        .btn:hover {{ background: var(--color-primary-hover); }}
        .response {{ background: var(--bg-response); padding: 15px; margin-top: 15px; border-left: 4px solid var(--color-primary); white-space: pre-wrap; }}
        .graph {{ font-family: monospace; white-space: pre; background: var(--bg-graph); padding: 15px; }}
        .theme-toggle {{ position: fixed; top: 20px; right: 20px; z-index: 1000; }}
        .theme-toggle button {{ background: var(--bg-section); color: var(--color-text); border: 1px solid var(--color-border); padding: 8px 12px; margin: 0 2px; cursor: pointer; border-radius: 4px; }}
        .theme-toggle button.active {{ background: var(--color-primary); color: white; }}
    </style>
</head>
<body>
    <script>
        function setTheme(theme) {{
            if (theme === 'system') {{
                document.documentElement.removeAttribute('data-theme');
            }} else {{
                document.documentElement.setAttribute('data-theme', theme);
            }}
            localStorage.setItem('theme', theme);
            updateThemeButtons();
        }}

        function updateThemeButtons() {{
            const theme = localStorage.getItem('theme') || 'system';
            document.getElementById('theme-system').classList.toggle('active', theme === 'system');
            document.getElementById('theme-light').classList.toggle('active', theme === 'light');
            document.getElementById('theme-dark').classList.toggle('active', theme === 'dark');
        }}

        updateThemeButtons();
    </script>
    <div class="theme-toggle">
        <button onclick="setTheme('system')" id="theme-system">System</button>
        <button onclick="setTheme('light')" id="theme-light">Light</button>
        <button onclick="setTheme('dark')" id="theme-dark">Dark</button>
    </div>
    <div class="container">
        <h1>Pillar AI Colab</h1>

        <div class="section">
            <h2>Pillar Configurations ({len(loader.pillars)} loaded)</h2>
            <div class="pillar-grid">
"""

        for idx, pillar in loader.pillars.items():
            html += f"""
                <div class="pillar-card">
                    <h3>{pillar.name} ({idx})</h3>
                    <p><strong>{pillar.core_function}</strong></p>
                    <p>{pillar.description[:100]}...</p>
                    <a href="/pillar/{idx}">View Details</a>
                </div>
"""

        html += f"""
            </div>
        </div>

        <div class="section">
            <h2>Query with Ollama</h2>
            <textarea class="query-box" id="queryInput" placeholder="How do I balance team productivity with wellbeing?"></textarea>
            <br>
            <button class="btn" onclick="submitQuery()">Submit Query</button>
            <div id="queryResponse" class="response" style="display:none;"></div>
        </div>

        <div class="section">
            <h2>Dependency Graph</h2>
            <a href="/graph">View Full Graph</a>
            <div class="graph">{graph_text}</div>
        </div>
    </div>

    <script>
        function submitQuery() {{
            const query = document.getElementById('queryInput').value;
            if (!query) return;

            const resp = document.getElementById('queryResponse');
            resp.style.display = 'block';
            resp.innerHTML = 'Processing...';

            fetch('/api/query', {{
                method: 'POST',
                headers: {{'Content-Type': 'application/json'}},
                body: JSON.stringify({{query: query}})
            }})
            .then(r => r.json())
            .then(data => {{
                resp.innerHTML = data.response || data.error || 'No response';
            }})
            .catch(e => {{
                resp.innerHTML = 'Error: ' + e;
            }});
        }}
    </script>
</body>
</html>
"""
        self.send_response(200)
        self.send_header('Content-type', 'text/html; charset=utf-8')
        self.end_headers()
        self.wfile.write(html.encode('utf-8'))

    def serve_pillar_detail(self, idx):
        """Serve pillar detail page."""
        if idx not in loader.pillars:
            self.send_error(404, f"Pillar {idx} not found")
            return

        pillar = loader.pillars[idx]
        pdict = pillar.to_dict()

        html = f"""
<!DOCTYPE html>
<html>
<head>
    <title>{pillar.name} - Pillar {idx}</title>
    <meta charset="utf-8">
    <style>
        :root {{
            --bg-body: #f5f5f5;
            --bg-container: white;
            --bg-pre: #f8f9fa;
            --color-text: #333;
            --color-primary: #007bff;
        }}

        @media (prefers-color-scheme: dark) {{
            :root:not([data-theme]) {{
                --bg-body: #1a1a1a;
                --bg-container: #2d2d2d;
                --bg-pre: #3d3d3d;
                --color-text: #e0e0e0;
                --color-primary: #4da6ff;
            }}
        }}

        [data-theme="dark"] {{
            --bg-body: #1a1a1a;
            --bg-container: #2d2d2d;
            --bg-pre: #3d3d3d;
            --color-text: #e0e0e0;
            --color-primary: #4da6ff;
        }}

        body {{ font-family: Arial, sans-serif; margin: 20px; background: var(--bg-body); color: var(--color-text); }}
        .container {{ max-width: 800px; margin: 0 auto; background: var(--bg-container); padding: 30px; border-radius: 8px; }}
        .back {{ color: var(--color-primary); text-decoration: none; }}
        pre {{ background: var(--bg-pre); padding: 15px; overflow-x: auto; color: var(--color-text); }}
        .theme-toggle {{ position: fixed; top: 20px; right: 20px; z-index: 1000; }}
        .theme-toggle button {{ background: var(--bg-container); color: var(--color-text); border: 1px solid var(--color-primary); padding: 8px 12px; margin: 0 2px; cursor: pointer; border-radius: 4px; }}
        .theme-toggle button.active {{ background: var(--color-primary); color: white; }}
    </style>
</head>
<body>
    <script>
        function setTheme(theme) {{
            if (theme === 'system') {{
                document.documentElement.removeAttribute('data-theme');
            }} else {{
                document.documentElement.setAttribute('data-theme', theme);
            }}
            localStorage.setItem('theme', theme);
            updateThemeButtons();
        }}

        function updateThemeButtons() {{
            const theme = localStorage.getItem('theme') || 'system';
            document.getElementById('theme-system').classList.toggle('active', theme === 'system');
            document.getElementById('theme-light').classList.toggle('active', theme === 'light');
            document.getElementById('theme-dark').classList.toggle('active', theme === 'dark');
        }}

        updateThemeButtons();
    </script>
    <div class="theme-toggle">
        <button onclick="setTheme('system')" id="theme-system">System</button>
        <button onclick="setTheme('light')" id="theme-light">Light</button>
        <button onclick="setTheme('dark')" id="theme-dark">Dark</button>
    </div>
    <div class="container">
        <a href="/" class="back">← Back to Dashboard</a>
        <h1>Pillar {idx}: {pillar.name}</h1>
        <p><strong>Function:</strong> {pillar.core_function}</p>
        <p>{pillar.description}</p>

        <h2>Operational Constants</h2>
        <pre>
"""
        for k, v in pdict['operational_constants'].items():
            html += f"{k}: {v}\n"

        html += "</pre><h2>Logic Mechanics</h2><pre>"
        for k, v in pdict['logic_mechanics'].items():
            html += f"{k}:\n  formula: {v.get('formula', 'N/A')}\n  impact: {v.get('impact', 'N/A')}\n"

        html += "</pre><h2>Failure Modes</h2><pre>"
        for k, v in pdict['failure_modes'].items():
            html += f"{k}:\n  condition: {v.get('condition', 'N/A')}\n  result: {v.get('result', 'N/A')}\n"

        html += """
        </pre>
    </div>
</body>
</html>
"""
        self.send_response(200)
        self.send_header('Content-type', 'text/html; charset=utf-8')
        self.end_headers()
        self.wfile.write(html.encode('utf-8'))

    def serve_graph(self):
        """Serve DOT graph."""
        from scripts.dependency_graph import generate_dot
        import os

        base_dir = Path(__file__).parent.parent
        interactions_path = base_dir / "Interactions" / "pillar_interactions.md"
        configs = {idx: p.to_dict() for idx, p in loader.pillars.items()}
        graph = build_graph(configs, interactions_path)
        dot = generate_dot(graph, configs)

        html = f"""
<!DOCTYPE html>
<html>
<head>
    <title>Dependency Graph - Pillar AI Colab</title>
    <meta charset="utf-8">
    <style>
        :root {{
            --bg-body: #f5f5f5;
            --bg-pre: #f8f9fa;
            --color-text: #333;
            --color-primary: #007bff;
        }}

        @media (prefers-color-scheme: dark) {{
            :root:not([data-theme]) {{
                --bg-body: #1a1a1a;
                --bg-pre: #3d3d3d;
                --color-text: #e0e0e0;
                --color-primary: #4da6ff;
            }}
        }}

        [data-theme="dark"] {{
            --bg-body: #1a1a1a;
            --bg-pre: #3d3d3d;
            --color-text: #e0e0e0;
            --color-primary: #4da6ff;
        }}

        body {{ font-family: Arial, sans-serif; margin: 20px; background: var(--bg-body); color: var(--color-text); }}
        h1 {{ color: var(--color-text); }}
        pre {{ background: var(--bg-pre); padding: 15px; overflow-x: auto; color: var(--color-text); white-space: pre; }}
        .back {{ color: var(--color-primary); text-decoration: none; }}
        .theme-toggle {{ position: fixed; top: 20px; right: 20px; z-index: 1000; }}
        .theme-toggle button {{ background: var(--bg-pre); color: var(--color-text); border: 1px solid var(--color-primary); padding: 8px 12px; margin: 0 2px; cursor: pointer; border-radius: 4px; }}
        .theme-toggle button.active {{ background: var(--color-primary); color: white; }}
    </style>
</head>
<body>
    <script>
        function setTheme(theme) {{
            if (theme === 'system') {{
                document.documentElement.removeAttribute('data-theme');
            }} else {{
                document.documentElement.setAttribute('data-theme', theme);
            }}
            localStorage.setItem('theme', theme);
            updateThemeButtons();
        }}

        function updateThemeButtons() {{
            const theme = localStorage.getItem('theme') || 'system';
            document.getElementById('theme-system').classList.toggle('active', theme === 'system');
            document.getElementById('theme-light').classList.toggle('active', theme === 'light');
            document.getElementById('theme-dark').classList.toggle('active', theme === 'dark');
        }}

        updateThemeButtons();
    </script>
    <div class="theme-toggle">
        <button onclick="setTheme('system')" id="theme-system">System</button>
        <button onclick="setTheme('light')" id="theme-light">Light</button>
        <button onclick="setTheme('dark')" id="theme-dark">Dark</button>
    </div>
    <a href="/" class="back">← Back to Dashboard</a>
    <h1>Pillar Dependency Graph (DOT)</h1>
    <pre>{dot}</pre>
</body>
</html>
"""
        self.send_response(200)
        self.send_header('Content-type', 'text/html; charset=utf-8')
        self.end_headers()
        self.wfile.write(html.encode('utf-8'))

    def serve_api_pillars(self):
        """API: List all pillars."""
        data = {idx: p.to_dict() for idx, p in loader.pillars.items()}
        self.send_json_response(data)

    def handle_query(self):
        """Handle query API."""
        content_length = int(self.headers['Content-Length'])
        body = self.rfile.read(content_length)
        data = json.loads(body.decode('utf-8'))

        query = data.get('query', '')

        try:
            context = loader.build_context(task=query)
            response = f"Context built ({len(context)} chars).\\n\\n"
            response += context[:1000] + "..." if len(context) > 1000 else context

            # Try Ollama
            try:
                import ollama
                result = ollama.chat(
                    model='qwen2.5:0.5b',
                    messages=[
                        {'role': 'system', 'content': context},
                        {'role': 'user', 'content': query}
                    ]
                )
                response = result['message']['content']
            except ImportError:
                pass
            except Exception as e:
                response = f"Ollama error: {e}"

            self.send_json_response({'response': response})

        except Exception as e:
            self.send_json_response({'error': str(e)})

    def handle_simulate(self):
        """Handle simulation API."""
        content_length = int(self.headers['Content-Length'])
        body = self.rfile.read(content_length)
        data = json.loads(body.decode('utf-8'))

        scenario = data.get('scenario', '')

        try:
            from scripts.simulate import PillarSimulation, SCENARIOS

            sim = PillarSimulation()
            sim.initialize_from_loader(loader)

            if scenario in SCENARIOS:
                SCENARIOS[scenario](sim)
                sim.run(steps=5)
                self.send_json_response(json.loads(sim.export_json()))
            else:
                self.send_json_response({'error': 'Unknown scenario'})

        except Exception as e:
            self.send_json_response({'error': str(e)})

    def send_json_response(self, data):
        """Send JSON response."""
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(data, indent=2).encode('utf-8'))

    def log_message(self, format, *args):
        """Suppress log messages."""
        pass


def main():
    port = 8080
    try:
        server = HTTPServer(('localhost', port), WebUIHandler)
        print(f"Starting Pillar AI Web UI...")
        print(f"Open http://localhost:{port} in your browser")
        print("Press Ctrl+C to stop")

        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.shutdown()
    except Exception as e:
        print(f"Error: {e}")


if __name__ == "__main__":
    main()
