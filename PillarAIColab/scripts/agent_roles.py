#!/usr/bin/env python3
"""
Agent Roles - Defines which models play which roles.
Based on test findings and model capabilities.
Includes busy status tracking and message logging.
"""
from typing import Dict, List, Any, Tuple
from datetime import datetime, timezone
import sys
import os
import numpy as np
from pathlib import Path

# Add parent directory to path for blackboard logging
sys.path.insert(0, str(Path(__file__).parent.parent))
try:
    from api.pillar_ai_bridge import log_to_blackboard
except ImportError:
    # Fallback if bridge not available
    def log_to_blackboard(entry_dict):
        print(f"[Blackboard] {entry_dict}")

# Agent roles based on test findings
AGENT_ROLES = {
    "quick_chat": {
        "role": "Quick Chat Agent",
        "description": "Handles simple, low-latency chat requests",
        "model": "llama3.2:1b",  # Ultra-fast (3s)
        "fallback_models": ["qwen2.5:0.5b", "qwen2.5:1.5b"],
        "delta_h": 0.02,
        "max_timeout_ms": 30000,  # 30s
        "when_to_use": "User wants quick responses, simple questions, greetings",
        "example_tasks": ["What is 2+2?", "Hello!", "List 3 benefits of AI"],
        "busy": False,  # Track if agent is busy
        "message_log": [],  # Log of messages/requests
        "last_active": None  # Last activity timestamp
    },
    "code_assistant": {
        "role": "Code Assistant",
        "description": "Handles programming tasks, code review, and generation",
        "model": "qwen2.5-coder:1.5b",  # Code-specialized
        "fallback_models": ["qwen2.5:3b", "qwen2.5:7b"],
        "delta_h": 0.03,
        "max_timeout_ms": 60000,  # 1min
        "when_to_use": "Code generation, review, debugging, programming questions",
        "example_tasks": ["Write a binary search function", "Review this Python code", "Explain decorators"],
        "busy": False,
        "message_log": [],
        "last_active": None
    },
    "general_assistant": {
        "role": "General Assistant",
        "description": "General-purpose chat and analysis",
        "model": "mistral:7b",  # Balanced, fits in GPU
        "fallback_models": ["qwen2.5:7b", "qwen2.5:3b"],
        "delta_h": 0.05,
        "max_timeout_ms": 60000,  # 1min
        "when_to_use": "General chat, analysis, moderate complexity tasks",
        "example_tasks": ["What is quantum computing?", "Analyze microservices pros/cons", "Summarize local LLMs"],
        "busy": False,
        "message_log": [],
        "last_active": None
    },
    "deep_reasoning": {
        "role": "Deep Reasoning Agent",
        "description": "Handles complex reasoning and analysis tasks",
        "model": "qwen2.5:7b",  # Strong reasoning, minimal offload
        "fallback_models": ["qwen3.5:latest", "mistral:7b"],
        "delta_h": 0.05,
        "max_timeout_ms": 60000,  # 1min
        "when_to_use": "Complex reasoning, deep analysis, C++ code generation",
        "example_tasks": ["Optimize this algorithm", "Deep dive: AI ethics", "Generate C++ class"],
        "busy": False,
        "message_log": [],
        "last_active": None
    },
    "thinking_agent": {
        "role": "Thinking Agent",
        "description": "Uses thinking/reasoning models for step-by-step tasks",
        "model": "gemma4:latest",  # Supports thinking mode
        "fallback_models": ["qwen3.5:latest", "qwen2.5:7b"],
        "delta_h": 0.06,
        "max_timeout_ms": 300000,  # 5min (may offload to CPU)
        "when_to_use": "Problems requiring step-by-step thinking, math, complex logic",
        "example_tasks": ["Solve: Two trains 300 miles apart...", "Think step by step: optimize quicksort"],
        "busy": False,
        "message_log": [],
        "last_active": None
    },
    "project_manager": {
        "role": "Project Manager",
        "description": "Large model for project planning and management tasks",
        "model": "qwen3:30b-a3b",  # 55s response (FASTEST MoE, 3B active params)
        "fallback_models": ["qwen3.6:latest", "gpt-oss:20b"],  # Alternatives
        "delta_h": 0.08,
        "max_timeout_ms": 120000,  # 2min (55s actual response time)
        "when_to_use": "Project planning, system architecture, high-level decision making",
        "example_tasks": ["Create project plan for auth system", "Design microservices architecture"],
        "notes": "DEFAULT PM: 55s response (fastest MoE), 3B active params, excellent for long context",
        "busy": False,
        "message_log": [],
        "last_active": None
    },
    "experimental": {
        "role": "Experimental Agent",
        "description": "Test new MoE models (still downloading at test time)",
        "model": "gpt-oss:20b",  # OpenAI MoE (MXFP4)
        "fallback_models": ["qwen3:30b-a3b", "qwen3.6:latest"],
        "delta_h": 0.08,
        "max_timeout_ms": 300000,  # 5min (unknown performance)
        "when_to_use": "Testing MoE models, agentic workflows, when other models fail",
        "example_tasks": ["Agentic task planning", "Multi-step reasoning"],
        "notes": "MoE model designed for CPU offload, may be faster than dense models",
        "busy": False,
        "message_log": [],
        "last_active": None
    },
    "vision_agent": {
        "role": "Vision Agent",
        "description": "Handles visual understanding tasks using vision-language models",
        "model": "qwen2.5:7b",  # Multimodal vision model
        "fallback_models": ["qwen2.5:3b", "llama3.2:1b"],
        "delta_h": 0.04,
        "max_timeout_ms": 60000,  # 1min
        "when_to_use": "Visual analysis, image understanding, creature visualization, scene description",
        "example_tasks": ["Describe this creature image", "Analyze voxel structure visually", "Count objects in scene"],
        "busy": False,
        "message_log": [],
        "last_active": None
    },
    "messenger": {
        "role": "Messenger Agent",
        "description": "Handles message propagations between agents",
        "model": None,  # No model needed - pure coordination
        "fallback_models": [],
        "delta_h": 0.02,
        "max_timeout_ms": 30000,  # 30s (coordination only)
        "when_to_use": "Routing messages between agents, task delegation, inter-agent communication",
        "example_tasks": ["Route task to Code Assistant", "Propagate message to Project Manager"],
        "busy": False,
        "message_log": [],
        "last_active": None,
        "message_queue": [],  # Pending messages for busy agents
        "routing_table": {}  # agent_name -> [message_history]
    }
}

# Neural routing components (lazy loaded)
_ROUTING_NETWORK = None
_ROUTING_TRAINER = None

# Neural agent (learns from other agents)
_NEURAL_AGENT = None

def init_neural_routing():
    """Initialize neural routing network if available."""
    global _ROUTING_NETWORK, _ROUTING_TRAINER
    
    try:
        import torch
        from scripts.agent_learner import AgentRoutingNetwork, RoutingTrainer
        
        model_path = "models/routing_network.pth"
        if os.path.exists(model_path):
            _ROUTING_NETWORK = AgentRoutingNetwork(input_size=512, num_agents=8)
            checkpoint = torch.load(model_path, map_location=_ROUTING_NETWORK.device)
            _ROUTING_NETWORK.load_state_dict(checkpoint['model_state_dict'])
            _ROUTING_NETWORK.eval()
            print("[Neural] Loaded pre-trained routing network")
            return True
        else:
            print(f"[Neural] Model not found: {model_path}")
    except Exception as e:
        print(f"[Neural] Failed to load routing network: {e}")
    
    return False


def init_neural_agent():
    """Initialize the custom neural agent that learns from other agents."""
    global _NEURAL_AGENT
    
    try:
        import os
        from scripts.neural_agent import NeuralAgent
        
        _NEURAL_AGENT = NeuralAgent(input_size=512, num_actions=8)
        
        # Try to load pre-trained model
        model_path = "models/neural_agent.pth"
        if os.path.exists(model_path):
            _NEURAL_AGENT.load_model(model_path)
        
        print("[Neural Agent] Initialized (learns from other agents via RL)")
        return True
    except Exception as e:
        print(f"[Neural Agent] Failed to initialize: {e}")
        return False


# Status management functions
def set_agent_busy(role_name: str, busy: bool = True):
    """Set an agent's busy status."""
    if role_name in AGENT_ROLES:
        AGENT_ROLES[role_name]["busy"] = busy
        if busy:
            AGENT_ROLES[role_name]["last_active"] = datetime.now(timezone.utc).isoformat()
        status = "BUSY" if busy else "IDLE"
        print(f"[{status}] {role_name} -> {AGENT_ROLES[role_name]['model']}")
        return True
    return False


def is_agent_busy(role_name: str) -> bool:
    """Check if an agent is busy."""
    return AGENT_ROLES.get(role_name, {}).get("busy", False)


def log_message(role_name: str, message: str, task: str = None):
    """
    Log a message to an agent's message log.
    Also logs to blackboard if available.
    """
    if role_name not in AGENT_ROLES:
        print(f"[ERR] Unknown role: {role_name}")
        return False
    
    timestamp = datetime.now(timezone.utc).isoformat()
    log_entry = {
        "timestamp": timestamp,
        "message": message,
        "task": task or "No task specified"
    }
    
    AGENT_ROLES[role_name]["message_log"].append(log_entry)
    AGENT_ROLES[role_name]["last_active"] = timestamp
    
    # Log to blackboard
    try:
        log_to_blackboard({
            "agent_id": f"{role_name}_{AGENT_ROLES[role_name]['role'].replace(' ', '_')}",
            "task": f"Message logged: {task or message[:50]}",
            "delta_h": AGENT_ROLES[role_name].get("delta_h", 0.05),
            "approved": True,
            "notes": f"Role: {role_name}, Message: {message[:100]}"
        })
    except Exception as e:
        print(f"[WARN] Could not log to blackboard: {e}")
    
    print(f"[LOG] {role_name}: {message[:50]}...")
    return True


def get_agent_logs(role_name: str, limit: int = 10) -> List[Dict]:
    """Get recent message logs for an agent."""
    if role_name not in AGENT_ROLES:
        return []
    logs = AGENT_ROLES[role_name]["message_log"]
    return logs[-limit:] if limit else logs


def clear_agent_logs(role_name: str):
    """Clear an agent's message log."""
    if role_name in AGENT_ROLES:
        AGENT_ROLES[role_name]["message_log"] = []
        print(f"[CLEAR] Cleared logs for {role_name}")
        return True
    return False


def print_agent_status():
    """Print status of all agents (busy/idle, last active, log count)."""
    print("=" * 80)
    print("Agent Status Dashboard")
    print("=" * 80)
    print()
    
    for role_name, config in AGENT_ROLES.items():
        status = "🔴 BUSY" if config.get("busy") else "🟢 IDLE"
        last_active = config.get("last_active") or "Never"
        log_count = len(config.get("message_log", []))
        
        print(f"\n{config['role']} ({role_name})")
        print("-" * 40)
        print(f"  Status: {status}")
        print(f"  Model: {config['model']}")
        print(f"  Last Active: {last_active}")
        print(f"  Messages Logged: {log_count}")
    
    print()
    print("=" * 80)


def simulate_agent_work(role_name: str, task: str):
    """
    Simulate an agent working on a task.
    Sets busy -> logs message -> completes -> sets idle.
    """
    if role_name not in AGENT_ROLES:
        print(f"[ERR] Unknown role: {role_name}")
        return False
    
    # Set busy
    set_agent_busy(role_name, True)
    
    # Log start message
    log_message(role_name, f"Started task: {task}", task)
    
    print(f"\n[WORKING] {AGENT_ROLES[role_name]['role']} processing...")
    print(f"  Task: {task}")
    print(f"  Model: {AGENT_ROLES[role_name]['model']}")
    
    # Simulate work (in real use, this would call the model)
    import time
    timeout_s = AGENT_ROLES[role_name].get("max_timeout_ms", 60000) / 1000
    print(f"  Estimated time: {timeout_s}s")
    
    # Log completion
    log_message(role_name, f"Completed task: {task}", task)
    
    # Set idle
    set_agent_busy(role_name, False)
    
    # Process message queue for this agent (deliver queued messages)
    if "messenger" in AGENT_ROLES:
        delivered = process_message_queue(role_name)
        if delivered:
            print(f"[Messenger] Delivered {len(delivered)} queued message(s)")
    
    print(f"[DONE] Task completed!")
    return True


# Messenger functions for inter-agent communication
def route_signal_via_messenger(sender: str, recipient: str, signal: np.ndarray, metadata: Dict, task: str = None):
    """
    Route a WHT signal through messenger to another agent.
    Signals are stored and can be retrieved by the recipient.
    """
    if "messenger" not in AGENT_ROLES:
        print("[ERR] Messenger agent not configured")
        return {"status": "error", "message": "Messenger not available"}
    
    # Store signal temporarily
    from scripts.memory_translator import MemoryTranslator
    translator = MemoryTranslator()
    signal_filename = f"signal_{sender}_to_{recipient}_{datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')}.npy"
    signal_path = translator.store_signal(signal, metadata, filename=signal_filename)
    
    # Create signal message
    signal_message = f"[WHT Signal] {signal_filename} - Energy: {metadata.get('signal_energy', 0):.2f}"
    
    # Route via messenger
    result = route_message(sender, recipient, signal_message, task or "Signal transmission")
    
    # Track in routing table
    if recipient not in AGENT_ROLES["messenger"]["routing_table"]:
        AGENT_ROLES["messenger"]["routing_table"][recipient] = []
    AGENT_ROLES["messenger"]["routing_table"][recipient].append({
        "sender": sender,
        "message": signal_message,
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "signal_path": str(signal_path)
    })
    
    return {**result, "signal_path": str(signal_path)}


def route_message(sender: str, recipient: str, message: str, task: str = None):
    """
    Route a message from sender to recipient via messenger.
    If recipient is busy, queue the message for later delivery.
    """
    if "messenger" not in AGENT_ROLES:
        print("[ERR] Messenger agent not configured")
        return {"status": "error", "message": "Messenger not available"}
    
    # Check if recipient exists
    if recipient not in AGENT_ROLES:
        print(f"[ERR] Unknown recipient: {recipient}")
        return {"status": "error", "message": f"Unknown recipient: {recipient}"}
    
    timestamp = datetime.now(timezone.utc).isoformat()
    
    # Check if recipient is busy
    if is_agent_busy(recipient):
        # Queue message for later
        AGENT_ROLES["messenger"]["message_queue"].append({
            "sender": sender,
            "recipient": recipient,
            "message": message,
            "task": task or "No task specified",
            "timestamp": timestamp
        })
        log_message("messenger", f"Queued message for {recipient}: {message[:50]}", task)
        print(f"[Messenger] {sender} → {recipient}: QUEUED (agent busy)")
        return {"status": "queued", "recipient": recipient, "queue_length": len(AGENT_ROLES["messenger"]["message_queue"])}
    
    # Deliver immediately
    log_message(recipient, message, task)
    log_message("messenger", f"Routed: {sender} → {recipient}: {message[:50]}", task)
    
    # Update routing table
    if recipient not in AGENT_ROLES["messenger"]["routing_table"]:
        AGENT_ROLES["messenger"]["routing_table"][recipient] = []
    AGENT_ROLES["messenger"]["routing_table"][recipient].append({
        "sender": sender,
        "message": message,
        "timestamp": timestamp
    })
    
    print(f"[Messenger] {sender} → {recipient}: DELIVERED")
    return {"status": "delivered", "recipient": recipient}


def process_message_queue(agent_name: str):
    """
    Process queued messages for an agent when they become idle.
    Returns list of delivered messages.
    """
    if "messenger" not in AGENT_ROLES:
        return []
    
    queue = AGENT_ROLES["messenger"]["message_queue"]
    delivered = []
    
    for msg in queue[:]:
        if msg["recipient"] == agent_name and not is_agent_busy(agent_name):
            # Deliver queued message
            log_message(agent_name, msg["message"], msg.get("task"))
            log_message("messenger", f"Delivered queued: {msg['sender']} → {agent_name}: {msg['message'][:50]}", msg.get("task"))
            
            delivered.append(msg)
            queue.remove(msg)
    
    return delivered


def get_messenger_stats():
    """Get messenger statistics (queue length, routing table size)."""
    if "messenger" not in AGENT_ROLES:
        return {}
    
    messenger = AGENT_ROLES["messenger"]
    return {
        "queue_length": len(messenger["message_queue"]),
        "routing_table_entries": sum(len(v) for v in messenger["routing_table"].values()),
        "agents_with_history": len(messenger["routing_table"]),
        "messenger_logs": len(messenger["message_log"])
    }


def print_messenger_dashboard():
    """Print messenger dashboard (queue, routing table, stats)."""
    print("=" * 80)
    print("Messenger Agent Dashboard")
    print("=" * 80)
    print()
    
    if "messenger" not in AGENT_ROLES:
        print("[ERR] Messenger agent not configured")
        return
    
    stats = get_messenger_stats()
    
    print(f"Message Queue: {stats['queue_length']} pending")
    print(f"Routing Table: {stats['routing_table_entries']} total entries across {stats['agents_with_history']} agents")
    print(f"Messenger Logs: {stats['messenger_logs']} entries")
    
    # Show queue
    if stats['queue_length'] > 0:
        print("\nPending Messages:")
        print("-" * 40)
        for i, msg in enumerate(AGENT_ROLES["messenger"]["message_queue"]):
            print(f"{i+1}. {msg['sender']} → {msg['recipient']}: {msg['message'][:30]}...")
    
    # Show routing table summary
    if stats['routing_table_entries'] > 0:
        print("\nRouting History (last 3 per agent):")
        print("-" * 40)
        for agent, history in AGENT_ROLES["messenger"]["routing_table"].items():
            print(f"\n{agent}:")
            for entry in history[-3:]:  # Last 3 entries
                print(f"  {entry['sender']}: {entry['message'][:40]}...")
    
    print()
    print("=" * 80)


# Update route_task_to_role to use messenger
def route_task_via_messenger(task: str, sender: str = "user"):
    """
    Route a task to the appropriate agent using messenger.
    Returns (role_name, delivery_status).
    """
    role_name = route_task_to_role(task)
    model = get_model_for_role(role_name)
    
    # Route via messenger
    result = route_message(sender, role_name, f"Task: {task}", task)
    
    return role_name, result.get("status", "unknown")


def get_role(role_name: str) -> Dict[str, Any]:
    """Get configuration for a specific role."""
    return AGENT_ROLES.get(role_name, {})


def get_model_for_role(role_name: str) -> str:
    """Get the assigned model for a role."""
    role = AGENT_ROLES.get(role_name)
    return role["model"] if role else None


def log_role_assignment(role_name: str, task: str, model: str = None):
    """Log role assignment to blackboard (placeholder for actual implementation)."""
    if model is None:
        model = get_model_for_role(role_name)
    
    timestamp = datetime.now(timezone.utc).isoformat()
    log_entry = {
        "timestamp": timestamp,
        "role": role_name,
        "model": model,
        "task": task,
        "agent_id": f"role_assigner_{role_name}"
    }
    
    # This would integrate with api/pillar_ai_bridge.py log_to_blackboard()
    print(f"[Role Assignment] {role_name} -> {model}")
    print(f"  Task: {task}")
    print(f"  Timestamp: {timestamp}")
    
    return log_entry


def print_roles():
    """Print all agent roles in a formatted table."""
    print("=" * 80)
    print("Agent Roles (Based on Test Findings)")
    print("=" * 80)
    print()
    
    for role_name, config in AGENT_ROLES.items():
        print(f"\n{config['role']} ({role_name})")
        print("-" * 40)
        print(f"  Model: {config['model']}")
        print(f"  Delta H: {config['delta_h']}")
        print(f"  Timeout: {config['max_timeout_ms']/1000:.0f}s")
        print(f"  Description: {config['description']}")
        print(f"  When to Use: {config['when_to_use']}")
        print(f"  Fallback Models: {', '.join(config['fallback_models'])}")
        if 'notes' in config:
            print(f"  Notes: {config['notes']}")
        print(f"  Example Tasks:")
        for task in config['example_tasks']:
            print(f"    - {task}")
    
    print()
    print("=" * 80)


def route_task_to_role(task: str, use_neural=True, use_neural_agent=True) -> Tuple[str, str]:
    """
    Route task using neural networks if available, else rule-based.
    Returns (role_name, routing_method) where method is 'neural_agent', 'neural_routing', or 'rule_based'.
    """
    # Try neural agent first (learns from experience)
    if use_neural_agent and _NEURAL_AGENT is not None:
        try:
            from scripts.memory_translator import MemoryTranslator
            translator = MemoryTranslator()
            signal, _ = translator.translate_memory_entry(task)
            
            if signal is not None:
                # Ensure 512D
                if len(signal) < 512:
                    signal = np.pad(signal, (0, 512 - len(signal)))
                else:
                    signal = signal[:512]
                
                # Neural agent selects action (with exploration)
                agent_idx, agent_name = _NEURAL_AGENT.select_action(signal, explore=True)
                print(f"[Neural Agent] Selected: {agent_name} (exploring: {_NEURAL_AGENT.epsilon:.2%})")
                return agent_name, 'neural_agent'
        except Exception as e:
            print(f"[Neural Agent] Failed: {e}")
    
    # Try neural routing (pre-trained)
    if use_neural and _ROUTING_NETWORK is not None:
        try:
            from scripts.agent_learner import predict_agent_for_task
            agent_name, confidence = predict_agent_for_task(task)
            if agent_name and confidence >0.6:
                print(f"[Neural Routing] Predicted: {agent_name} (confidence: {confidence:.2%})")
                return agent_name, 'neural_routing'
        except Exception as e:
            print(f"[Neural Routing] Failed: {e}")
    
    # Fall back to rule-based routing
    task_lower = task.lower()
    
    # Code-related keywords
    if any(kw in task_lower for kw in ["code", "function", "class", "programming", "debug", "review"]):
        return "code_assistant", 'rule_based'
    
    # Thinking/reasoning keywords
    if any(kw in task_lower for kw in ["think", "step by step", "reason", "solve", "calculate"]):
        return "thinking_agent", 'rule_based'
    
    # Project management keywords
    if any(kw in task_lower for kw in ["project", "plan", "architecture", "design", "system"]):
        return "project_manager", 'rule_based'
    
    # Complex analysis keywords
    if any(kw in task_lower for kw in ["analyze", "deep dive", "complex", "evaluate"]):
        return "deep_reasoning", 'rule_based'
    
    # Default to general assistant
    return "general_assistant", 'rule_based'


if __name__ == "__main__":
    print_roles()
    
    print("\n\nTask Routing Examples:")
    print("-" * 40)
    test_tasks = [
        "Write a binary search function",
        "What is 2+2?",
        "Create a project plan for auth system",
        "Think step by step: optimize quicksort",
        "Analyze microservices architecture"
    ]
    
    for task in test_tasks:
        result = route_task_to_role(task)
        if isinstance(result, tuple):
            role, method = result
        else:
            role, method = result, "unknown"
        model = get_model_for_role(role)
        print(f"Task: {task}")
        print(f"  -> Routed to: {role} ({model}) via {method}")
        print()
