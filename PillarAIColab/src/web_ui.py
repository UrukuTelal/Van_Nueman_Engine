#!/usr/bin/env python3
"""
Pillar AI Web UI

Simple web interface for interacting with Pillar AI system.
Features:
- View pillar configs
- Query with Ollama + RAG
- Run simulations
- Visualize dependency graph

Usage:
    pip install flask
    python src/web_ui.py
    Then open http://localhost:5000
"""

import json
import sys
from pathlib import Path

if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8')

sys.path.insert(0, str(Path(__file__).parent.parent))

try:
    from flask import Flask, render_template_string, request, jsonify
    HAS_FLASK = True
except ImportError:
    HAS_FLASK = False
    print("Flask not installed. Install: pip install flask")

from src.pillar_loader import PillarLoader
from scripts.dependency_graph import build_graph, PILLAR_NAMES

# Theme CSS variables
THEME_CSS = """
    :root {
        --bg-body: #f5f5f5;
        --bg-section: white;
        --bg-response: #f8f9fa;
        --bg-pre: #f8f9fa;
        --color-text: #333;
        --color-primary: #007bff;
        --color-primary-hover: #0056b3;
        --color-border: #ddd;
        --shadow: 0 2px 4px rgba(0,0,0,0.1);
    }

    @media (prefers-color-scheme: dark) {
        :root:not([data-theme]) {
            --bg-body: #1a1a1a;
            --bg-section: #2d2d2d;
            --bg-response: #3d3d3d;
            --bg-pre: #3d3d3d;
            --color-text: #e0e0e0;
            --color-primary: #4da6ff;
            --color-primary-hover: #3399ff;
            --color-border: #444;
            --shadow: 0 2px 4px rgba(0,0,0,0.3);
        }
    }

    [data-theme="dark"] {
        --bg-body: #1a1a1a;
        --bg-section: #2d2d2d;
        --bg-response: #3d3d3d;
        --bg-pre: #3d3d3d;
        --color-text: #e0e0e0;
        --color-primary: #4da6ff;
        --color-primary-hover: #3399ff;
        --color-border: #444;
        --shadow: 0 2px 4px rgba(0,0,0,0.3);
    }

    body { font-family: Arial, sans-serif; margin: 20px; background: var(--bg-body); color: var(--color-text); }
    h1, h2 { color: var(--color-text); }
    .container { max-width: 1200px; margin: 0 auto; }
    .section { background: var(--bg-section); padding: 20px; margin: 20px 0; border-radius: 8px; box-shadow: var(--shadow); }
    .pillar-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(250px, 1fr)); gap: 15px; }
    .pillar-card { border: 1px solid var(--color-border); padding: 15px; border-radius: 5px; }
    .pillar-card h3 { margin-top: 0; color: var(--color-primary); }
    .query-box { width: 100%; height: 100px; margin: 10px 0; padding: 10px; background: var(--bg-section); color: var(--color-text); border: 1px solid var(--color-border); }
    .btn { background: var(--color-primary); color: white; border: none; padding: 10px 20px; cursor: pointer; border-radius: 5px; }
    .btn:hover { background: var(--color-primary-hover); }
    .response { background: var(--bg-response); padding: 15px; margin-top: 15px; border-left: 4px solid var(--color-primary); }
    .graph { font-family: monospace; white-space: pre; background: var(--bg-pre); padding: 15px; }
    .theme-toggle { position: fixed; top: 20px; right: 20px; z-index: 1000; }
    .theme-toggle button { background: var(--bg-section); color: var(--color-text); border: 1px solid var(--color-border); padding: 8px 12px; margin: 0 2px; cursor: pointer; border-radius: 4px; }
    .theme-toggle button.active { background: var(--color-primary); color: white; }
    pre { background: var(--bg-pre); padding: 15px; overflow-x: auto; color: var(--color-text); }
    .back { color: var(--color-primary); text-decoration: none; }
"""

# Theme toggle JS
THEME_JS = """
    <script>
        function setTheme(theme) {
            if (theme === 'system') {
                document.documentElement.removeAttribute('data-theme');
            } else {
                document.documentElement.setAttribute('data-theme', theme);
            }
            localStorage.setItem('theme', theme);
            updateThemeButtons();
        }
        
        function updateThemeButtons() {
            const theme = localStorage.getItem('theme') || 'system';
            document.getElementById('theme-system').classList.toggle('active', theme === 'system');
            document.getElementById('theme-light').classList.toggle('active', theme === 'light');
            document.getElementById('theme-dark').classList.toggle('active', theme === 'dark');
        }
        
        updateThemeButtons();
    </script>
    <div class="theme-toggle">
        <button onclick="setTheme('system')" id="theme-system">System</button>
        <button onclick="setTheme('light')" id="theme-light">Light</button>
        <button onclick="setTheme('dark')" id="theme-dark">Dark</button>
    </div>
"""

# Routing info HTML
ROUTING_INFO_HTML = """
    <div id="routingInfo" class="routing-info" style="display:none; background: var(--bg-response); padding:10px; margin:10px 0; border-radius: 5px;">
        <strong>Routed to:</strong> <span id="agentName"></span>
        <br>
        <strong>Model:</strong> <span id="modelName"></span>
        <br>
        <strong>Method:</strong> <span id="routingMethod"></span>
    </div>
"""

# HTML template for main dashboard
HTML_TEMPLATE = """
<!DOCTYPE html>
<html>
<head>
    <title>Pillar AI Colab - Web UI</title>
    <meta charset="utf-8">
    <style>""" + THEME_CSS + """</style>
</head>
<body>
""" + THEME_JS + """
    <div class="container">
        <h1>Pillar AI Colab - Web UI</h1>

        <div class="section">
            <h2>Pillar Configurations</h2>
            <div class="pillar-grid">
                {% for idx, pillar in pillars.items() %}
                <div class="pillar-card">
                    <h3>{{ pillar.name }} ({{ idx }})</h3>
                    <p><strong>Function:</strong> {{ pillar.core_function }}</p>
                    <p>{{ pillar.description[:100] }}...</p>
                    <a href="/pillar/{{ idx }}">View Details</a>
                </div>
                {% endfor %}
            </div>
        </div>

        <div class="section">
            <h2>Query with Ollama</h2>
            <p>Enter a task or question (requires Ollama running):</p>
            <textarea class="query-box" id="queryInput" placeholder="How do I balance team productivity with wellbeing?"></textarea>
            <br>
            <button class="btn" onclick="submitQuery()">Submit Query</button>
            <div id="routingInfo" class="routing-info" style="display:none; background: var(--bg-response); padding:10px; margin:10px 0; border-radius: 5px;">
                <strong>Routed to:</strong> <span id="agentName"></span>
                <br>
                <strong>Model:</strong> <span id="modelName"></span>
                <br>
                <strong>Method:</strong> <span id="routingMethod"></span>
            </div>
            <div id="queryResponse" class="response" style="display:none;"></div>
        </div>

        <div class="section">
            <h2>Dependency Graph</h2>
            <a href="/graph">View Full Graph</a>
            <div class="graph">{{ graph_text }}</div>
        </div>

         <div class="section">
             <h2>Simulation</h2>
             <p>Run predefined scenarios:</p>
             <button class="btn" onclick="runSimulation('high_force_low_resistance')">High Force / Low Resistance</button>
             <button class="btn" onclick="runSimulation('awareness_breakdown')">Awareness Breakdown</button>
             <div id="simResponse" class="response" style="display:none;"></div>
         </div>
         
        <div class="section">
            <h2>Workflow Engine</h2>
            <p>Enter a task to be decomposed into multi-agent workflow:</p>
            <textarea class="query-box" id="workflowInput" placeholder="Build an auth system with JWT tokens - Step 1: Plan, Step 2: Implement, Step 3: Document"></textarea>
            <br>
            <button class="btn" onclick="runWorkflow()" id="workflowBtn">Run Workflow</button>
            <div id="workflowProgress" style="display:none; background: var(--bg-response); padding:15px; margin:10px 0; border-radius: 5px;">
                <div id="statusIndicator" style="margin-bottom: 10px;">
                    <span id="runningIndicator" style="display: inline;">
                        <span style="animation: spin 1s linear infinite;">⚙</span> <strong>Workflow Running...</strong>
                    </span>
                    <span id="completedIndicator" style="display: none; color: green;">
                        ✓ <strong>Workflow Completed!</strong>
                    </span>
                    <span id="failedIndicator" style="display: none; color: red;">
                        ✗ <strong>Workflow Failed</strong>
                    </span>
                </div>
                <strong>Progress:</strong> <span id="workflowSteps">Processing...</span>
                <div style="width:100%; background-color: #e0e0e0; border-radius: 5px; margin-top: 10px;">
                    <div id="progressBar" style="width:0%; height: 20px; background-color: var(--color-primary); border-radius: 5px; transition: width 0.5s;"></div>
                </div>
                <p id="currentStep" style="margin-top: 10px; font-style: italic;"></p>
            </div>
            <div id="workflowResponse" class="response" style="display:none;"></div>
        </div>
             <div id="workflowResponse" class="response" style="display:none;"></div>
         </div>
     </div>

    <script>
        function submitQuery() {
            const query = document.getElementById('queryInput').value;
            if (!query) return;

            document.getElementById('queryResponse').style.display = 'block';
            document.getElementById('queryResponse').innerHTML = 'Processing...';

            fetch('/api/query', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({query: query})
            })
            .then(r => r.json())
            .then(data => {
                document.getElementById('queryResponse').innerHTML = data.response || data.error || 'No response';
                
                // Show routing info if available
                if (data.agent || data.model || data.routing_method) {
                    document.getElementById('routingInfo').style.display = 'block';
                    document.getElementById('agentName').textContent = data.agent || 'N/A';
                    document.getElementById('modelName').textContent = data.model || 'N/A';
                    document.getElementById('routingMethod').textContent = data.routing_method || 'N/A';
                }
            })
            .catch(e => {
                document.getElementById('queryResponse').innerHTML = 'Error: ' + e;
            });
        }

         function runSimulation(scenario) {
             document.getElementById('simResponse').style.display = 'block';
             document.getElementById('simResponse').innerHTML = 'Running simulation...';
             
             fetch('/api/simulate', {
                 method: 'POST',
                 headers: {'Content-Type': 'application/json'},
                 body: JSON.stringify({scenario: scenario})
             })
             .then(r => r.json())
             .then(data => {
                 let html = '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
                 document.getElementById('simResponse').innerHTML = html;
             })
             .catch(e => {
                 document.getElementById('simResponse').innerHTML = 'Error: ' + e;
             });
         }
         
        function runWorkflow() {
            const query = document.getElementById('workflowInput').value;
            if (!query) return;
            
            // Reset UI
            document.getElementById('workflowResponse').style.display = 'none';
            document.getElementById('workflowProgress').style.display = 'block';
            document.getElementById('runningIndicator').style.display = 'inline';
            document.getElementById('completedIndicator').style.display = 'none';
            document.getElementById('failedIndicator').style.display = 'none';
            document.getElementById('workflowSteps').textContent = 'Processing workflow...';
            document.getElementById('progressBar').style.width = '0%';
            document.getElementById('currentStep').textContent = 'Starting workflow...';
            document.getElementById('workflowBtn').disabled = true;
            
            fetch('/api/workflow', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({query: query})
            })
            .then(r => r.json())
            .then(data => {
                if (data.workflow_id) {
                    // Poll for status
                    checkWorkflowStatus(data.workflow_id, data.total_steps);
                } else {
                    document.getElementById('workflowProgress').style.display = 'none';
                    document.getElementById('workflowBtn').disabled = false;
                    let html = '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
                    document.getElementById('workflowResponse').innerHTML = html;
                    document.getElementById('workflowResponse').style.display = 'block';
                }
            })
            .catch(e => {
                document.getElementById('workflowProgress').style.display = 'none';
                document.getElementById('workflowBtn').disabled = false;
                document.getElementById('failedIndicator').style.display = 'inline';
                document.getElementById('runningIndicator').style.display = 'none';
                document.getElementById('workflowResponse').innerHTML = 'Error: ' + e;
                document.getElementById('workflowResponse').style.display = 'block';
            });
        }
         
        function checkWorkflowStatus(workflowId, totalSteps) {
            fetch(`/api/workflow/status/${workflowId}`)
            .then(r => r.json())
            .then(data => {
                // Update progress
                const completed = data.completed_steps || 0;
                const status = data.status || 'unknown';
                const total = totalSteps || data.total_steps || 0;
                
                document.getElementById('workflowSteps').textContent = `${completed}/${total} steps completed`;
                const progressPercent = total > 0 ? Math.round((completed / total) * 100) : 0;
                document.getElementById('progressBar').style.width = progressPercent + '%';
                
                // Update current step
                if (data.results && data.results.length > 0) {
                    const lastResult = data.results[data.results.length - 1];
                    const stepStatus = lastResult.status || 'processing';
                    const agent = lastResult.agent || 'unknown';
                    document.getElementById('currentStep').textContent = `Step ${completed}: ${agent} - ${stepStatus}`;
                }
                
                if (status === 'completed') {
                    // Show completion
                    document.getElementById('runningIndicator').style.display = 'none';
                    document.getElementById('completedIndicator').style.display = 'inline';
                    document.getElementById('workflowBtn').disabled = false;
                    
                    // Show results
                    let html = '<h3>Workflow Results:</h3>';
                    if (data.results) {
                        data.results.forEach((step, i) => {
                            html += `<p><strong>Step ${i+1} (${step.agent || 'unknown'}):</strong> ${step.status || 'unknown'}</p>`;
                            if (step.result) {
                                html += `<pre style="max-height: 200px; overflow-y: auto;">${step.result.substring(0, 500)}...</pre>`;
                            }
                        });
                    }
                    
                    setTimeout(() => {
                        document.getElementById('workflowProgress').style.display = 'none';
                        document.getElementById('workflowResponse').innerHTML = html;
                        document.getElementById('workflowResponse').style.display = 'block';
                    }, 1000);
                } else if (status === 'failed') {
                    document.getElementById('runningIndicator').style.display = 'none';
                    document.getElementById('failedIndicator').style.display = 'inline';
                    document.getElementById('workflowBtn').disabled = false;
                    document.getElementById('workflowResponse').innerHTML = 
                        '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
                    document.getElementById('workflowResponse').style.display = 'block';
                } else {
                    // Continue polling
                    setTimeout(() => checkWorkflowStatus(workflowId, total), 2000);
                }
            })
            .catch(e => {
                document.getElementById('runningIndicator').style.display = 'none';
                document.getElementById('failedIndicator').style.display = 'inline';
                document.getElementById('workflowBtn').disabled = false;
                document.getElementById('workflowResponse').innerHTML = 'Error checking status: ' + e;
                document.getElementById('workflowResponse').style.display = 'block';
            });
        }
            })
            .catch(e => {
                document.getElementById('workflowProgress').style.display = 'none';
                document.getElementById('workflowResponse').style.display = 'block';
                document.getElementById('workflowResponse').innerHTML = 'Error: ' + e;
            });
        }
        
        function checkWorkflowStatus(workflowId, totalSteps) {
            fetch(`/api/workflow/status/${workflowId}`)
            .then(r => r.json())
            .then(data => {
                const completed = data.completed_steps || 0;
                totalSteps = totalSteps || data.total_steps || 1;
                
                // Update progress
                document.getElementById('workflowSteps').textContent = `${completed}/${totalSteps} steps completed`;
                const progressPercent = Math.round((completed / totalSteps) * 100);
                document.getElementById('progressBar').style.width = progressPercent + '%';
                
                // Update current step
                if (data.results && data.results.length > 0) {
                    const lastResult = data.results[data.results.length - 1];
                    const status = lastResult.status || 'processing';
                    const agent = lastResult.agent || 'unknown';
                    document.getElementById('currentStep').textContent = `Step ${completed}: ${agent} - ${status}`;
                }
                
                if (data.status === 'completed') {
                    // Show results
                    document.getElementById('workflowProgress').style.display = 'none';
                    document.getElementById('workflowResponse').style.display = 'block';
                    
                    let html = '<h3>Workflow Results:</h3>';
                    if (data.results) {
                        data.results.forEach((step, i) => {
                            html += `<p><strong>Step ${i+1} (${step.agent || 'unknown'}):</strong> ${step.status || 'unknown'}</p>`;
                            if (step.result) {
                                html += `<pre style="max-height: 200px; overflow-y: auto;">${step.result.substring(0, 500)}...</pre>`;
                            }
                        });
                    }
                    document.getElementById('workflowResponse').innerHTML = html;
                } else if (data.status === 'failed') {
                    document.getElementById('workflowProgress').style.display = 'none';
                    document.getElementById('workflowResponse').style.display = 'block';
                    document.getElementById('workflowResponse').innerHTML = '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
                } else {
                    // Continue polling
                    setTimeout(() => checkWorkflowStatus(workflowId, totalSteps), 2000);
                }
            })
            .catch(e => {
                document.getElementById('workflowProgress').style.display = 'none';
                document.getElementById('workflowResponse').style.display = 'block';
                document.getElementById('workflowResponse').innerHTML = 'Error checking status: ' + e;
            });
        }
             })
             .catch(e => {
                 document.getElementById('workflowResponse').innerHTML = 'Error: ' + e;
             });
         }
         
         function checkWorkflowStatus(workflowId, totalSteps) {
             fetch(`/api/workflow/status/${workflowId}`)
             .then(r => r.json())
             .then(data => {
                 // Update progress
                 const completed = data.completed_steps || 0;
                 const status = data.status || 'unknown';
                 const total = totalSteps || data.total_steps || 0;
                 
                 document.getElementById('workflowSteps').innerHTML = 
                     `Status: ${status} (${completed}/${total} steps completed)`;
                 
                 if (status === 'completed') {
                     let html = '<h3>Workflow Results:</h3>';
                     if (data.results) {
                         data.results.forEach((step, i) => {
                             html += `<p><strong>Step ${i+1} (${step.agent || 'unknown'}):</strong> ${step.status || 'unknown'}</p>`;
                             if (step.result) {
                                 html += `<pre>${step.result.substring(0, 500)}...</pre>`;
                             }
                         });
                     }
                     document.getElementById('workflowResponse').innerHTML = html;
                 } else if (status === 'failed') {
                     document.getElementById('workflowResponse').innerHTML = 
                         '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
                 } else {
                     // Continue polling
                     setTimeout(() => checkWorkflowStatus(workflowId, total), 2000);
                 }
             })
             .catch(e => {
                 document.getElementById('workflowResponse').innerHTML = 'Error checking status: ' + e;
             });
         }
    </script>
</body>
</html>
"""

# HTML template for pillar detail page
PILLAR_DETAIL_TEMPLATE = """
<!DOCTYPE html>
<html>
<head>
    <title>{{ pillar.name }} - Pillar {{ idx }}</title>
    <meta charset="utf-8">
    <style>""" + THEME_CSS + """</style>
</head>
<body>
""" + THEME_JS + """
    <div class="container">
        <a href="/" class="back">← Back to Dashboard</a>
        <h1>Pillar {{ idx }}: {{ pillar.name }}</h1>
        <p><strong>Function:</strong> {{ pillar.core_function }}</p>
        <p>{{ pillar.description }}</p>

        <h2>Operational Constants</h2>
        <pre>{% for k, v in pillar.operational_constants.items() %}{{ k }}: {{ v }}
{% endfor %}</pre>

        <h2>Logic Mechanics</h2>
        <pre>{% for k, v in pillar.logic_mechanics.items() %}{{ k }}:
  formula: {{ v.formula }}
  impact: {{ v.impact }}
{% endfor %}</pre>

        <h2>Failure Modes</h2>
        <pre>{% for k, v in pillar.failure_modes.items() %}{{ k }}:
  condition: {{ v.condition }}
  result: {{ v.result }}
{% endfor %}</pre>
    </div>
</body>
</html>
"""

# HTML template for graph page
GRAPH_TEMPLATE = """
<!DOCTYPE html>
<html>
<head>
    <title>Dependency Graph - Pillar AI Colab</title>
    <meta charset="utf-8">
    <style>""" + THEME_CSS + """</style>
</head>
<body>
""" + THEME_JS + """
    <a href="/" class="back">← Back to Dashboard</a>
    <h1>Pillar Dependency Graph (DOT)</h1>
    <pre>{{ dot }}</pre>
</body>
</html>
"""


def create_app():
    if not HAS_FLASK:
        print("Flask required: pip install flask")
        return None

    app = Flask(__name__)

    # Load pillars
    loader = PillarLoader()
    loader.load_all()

    @app.route('/')
    def index():
        # Build graph text
        from scripts.dependency_graph import build_graph, PILLAR_NAMES
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

        return render_template_string(
            HTML_TEMPLATE,
            pillars={idx: p.to_dict() for idx, p in loader.pillars.items()},
            graph_text=graph_text
        )

    @app.route('/pillar/<int:idx>')
    def pillar_detail(idx):
        if idx not in loader.pillars:
            return "Pillar not found", 404

        pillar = loader.pillars[idx]
        return render_template_string(
            PILLAR_DETAIL_TEMPLATE,
            idx=idx,
            pillar=pillar.to_dict()
        )

    @app.route('/graph')
    def graph():
        from scripts.dependency_graph import generate_dot, build_graph, PILLAR_NAMES
        base_dir = Path(__file__).parent.parent
        interactions_path = base_dir / "Interactions" / "pillar_interactions.md"
        configs = {idx: p.to_dict() for idx, p in loader.pillars.items()}
        graph = build_graph(configs, interactions_path)

        dot = generate_dot(graph, configs)
        return render_template_string(GRAPH_TEMPLATE, dot=dot)

    @app.route('/api/query', methods=['POST'])
    def api_query():
        data = request.json
        query = data.get('query', '')
        
        try:
            # Build context
            context = loader.build_context(task=query)
            
            # Initialize neural routing if not already done
            try:
                from scripts.agent_roles import init_neural_routing, init_neural_agent, route_task_to_role, get_model_for_role
                init_neural_routing()
                init_neural_agent()
            except:
                pass
            
            # Route to optimal agent using neural network
            try:
                result = route_task_to_role(query)
                if isinstance(result, tuple):
                    agent_role, method = result
                else:
                    agent_role, method = result, 'rule_based'
                
                model = get_model_for_role(agent_role)
                if not model:
                    model = 'qwen2.5:0.5b'  # Fallback
            except Exception as e:
                print(f"Routing error: {e}")
                agent_role = 'general_assistant'
                method = 'fallback'
                model = 'qwen2.5:0.5b'
            
            # Try Ollama if available
            try:
                import ollama
                response = ollama.chat(
                    model=model,
                    messages=[
                        {'role': 'system', 'content': context},
                        {'role': 'user', 'content': query}
                    ]
                )
                response_text = response['message']['content']
                
                # Store interaction for neural agent learning
                try:
                    from scripts.agent_roles import _NEURAL_AGENT
                    from scripts.memory_translator import MemoryTranslator
                    
                    if _NEURAL_AGENT is not None:
                        translator = MemoryTranslator()
                        signal, _ = translator.translate_memory_entry(query)
                        
                        if signal is not None:
                            # Ensure 512D
                            if len(signal) < 512:
                                signal = np.pad(signal, (0, 512 - len(signal)))
                            else:
                                signal = signal[:512]
                            
                            # Get agent index
                            agent_names = list(AGENT_ROLES.keys())
                            if agent_role in agent_names:
                                agent_idx = agent_names.index(agent_role)
                                
                                # Observe (assume success for now)
                                _NEURAL_AGENT.observe(
                                    task_signal=signal,
                                    selected_agent_idx=agent_idx,
                                    delta_h=0.05,
                                    success=True,
                                    agent_name=agent_role
                                )
                except Exception as learn_error:
                    print(f"Neural agent learning error: {learn_error}")
                
                return jsonify({
                    'response': response_text,
                    'agent': agent_role,
                    'model': model,
                    'routing_method': method
                })
            except ImportError:
                return jsonify({
                    'response': f'Context built (Ollama not available):\n\n{context[:500]}...',
                    'agent': agent_role,
                    'model': model,
                    'routing_method': method
                })
            except Exception as e:
                return jsonify({'error': str(e)})
        
        except Exception as e:
            return jsonify({'error': str(e)})

    @app.route('/api/simulate', methods=['POST'])
    def api_simulate():
        data = request.json
        scenario = data.get('scenario', '')
        
        try:
            from scripts.simulate import PillarSimulation, SCENARIOS
            
            sim = PillarSimulation()
            sim.initialize_from_loader(loader)
            
            if scenario in SCENARIOS:
                SCENARIOS[scenario](sim)
                sim.run(steps=5)
                return jsonify(json.loads(sim.export_json()))
            else:
                return jsonify({'error': 'Unknown scenario'})
        
        except Exception as e:
            return jsonify({'error': str(e)})
    
    @app.route('/api/workflow', methods=['POST'])
    def api_workflow():
        """Handle workflow API - translates prompt to multi-agent workflow."""
        data = request.json
        prompt = data.get('query', '')
        if not prompt:
            return jsonify({'error': 'No query provided'}), 400

        try:
            from scripts.workflow_engine import WorkflowEngine
            engine = WorkflowEngine()

            # Start async workflow (decomposition happens in background)
            workflow_id = engine.execute_workflow_async(prompt)

            return jsonify({
                "workflow_id": workflow_id,
                "status": "processing",
                "message": "Workflow started, poll /api/workflow/status/<id> for updates"
            })

        except Exception as e:
            return jsonify({'error': str(e)}), 500
    
    @app.route('/api/workflow/status/<workflow_id>', methods=['GET'])
    def api_workflow_status(workflow_id):
        """Get workflow execution status."""
        try:
            from scripts.workflow_engine import WorkflowEngine
            engine = WorkflowEngine()
            
            status = engine.get_workflow_status(workflow_id)
            return jsonify(status)
        
        except Exception as e:
            return jsonify({'error': str(e)})
    
    return app


def main():
    if not HAS_FLASK:
        print("Install Flask: pip install flask")
        return

    app = create_app()
    if app:
        print("Starting Pillar AI Web UI...")
        print("Open http://localhost:5000 in your browser")
        app.run(debug=True, host='0.0.0.0', port=5000)


if __name__ == "__main__":
    main()
