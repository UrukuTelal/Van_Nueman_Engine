#!/usr/bin/env python3
"""
Ollama wrapper with advanced context injection for Pillar AI.

Features:
- Streaming responses
- Context window management (truncation for small models)
- Multiple prompt templates
- Session memory with pillar state tracking
- Automatic model fallback

Usage:
    python src/ollama_wrapper.py --mode chat
    python src/ollama_wrapper.py --query "task" --model qwen2.5:0.5b
"""

import sys
import json
import argparse
from pathlib import Path

if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8')

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from src.pillar_loader import PillarLoader

try:
    import ollama
    HAS_OLLAMA = True
except ImportError:
    HAS_OLLAMA = False
    print("Note: ollama package not installed. Install: pip install ollama")


# Prompt templates
TEMPLATES = {
    'analyze': """Using the Pillar Framework in system context:

TASK: {task}

Analyze:
1. Most relevant pillars and why
2. Key interactions to consider
3. Potential failure modes
4. Recommended pillar adjustments

Reference pillars by name and index (e.g., "Pillar 02 (Force)").""",

    'decompose': """Using the Pillar Framework in system context:

TASK: {task}

Decompose this task into pillar state vectors:
- List each pillar (0-15) with current state (0.0-1.0)
- Identify which pillars need activation/increase
- Identify which pillars need suppression/decrease
- Note critical interactions

Format as: Pillar X (Name): State -> Target State (Reason)""",

    'evaluate': """Using the Pillar Framework in system context:

SCENARIO: {task}

Evaluate:
1. Is this scenario harmful? (check Pillar 12)
2. Is there distortion risk? (check Pillar 13)
3. What pillars are out of balance?
4. Suggested corrective actions

Use the blackboard governance rules for evaluation."""
}


def check_ollama_running():
    """Check if Ollama is running."""
    if not HAS_OLLAMA:
        return False
    try:
        ollama.list()
        return True
    except Exception:
        return False


def ensure_model(model_name):
    """Ensure model is available, pull if needed."""
    if not HAS_OLLAMA:
        return False

    try:
        models = ollama.list()
        model_names = [m['name'] for m in models.get('models', [])]

        if model_name not in model_names:
            print(f"Model {model_name} not found. Pulling...")
            ollama.pull(model_name)
        return True
    except Exception as e:
        print(f"Error with model {model_name}: {e}")
        return False


def stream_response(model, messages):
    """Stream response from Ollama."""
    try:
        stream = ollama.chat(model=model, messages=messages, stream=True)
        response_text = ""
        for chunk in stream:
            content = chunk['message']['content']
            print(content, end='', flush=True)
            response_text += content
        print()
        return response_text
    except Exception as e:
        print(f"\nError: {e}")
        return None


def build_focused_context(loader, task, max_tokens=4000):
    """Build context with token limit for small models."""
    # Determine relevant pillars for the task
    task_lower = task.lower()

    # Keywords to identify relevant pillars
    keywords = {
        0: ['aware', 'detect', 'perceive', 'monitor'],
        1: ['will', 'persist', 'goal', 'strategy'],
        2: ['force', 'execute', 'action', 'implement'],
        3: ['influence', 'communicate', 'culture'],
        4: ['resist', 'risk', 'compliance', 'stabilize'],
        5: ['integrity', 'identity', 'standard', 'value'],
        6: ['cohesion', 'team', 'bind', 'structure'],
        7: ['relation', 'network', 'connect', 'depend'],
        8: ['presence', 'priority', 'visible', 'focus'],
        9: ['warmth', 'hr', 'support', 'human'],
        10: ['memory', 'knowledge', 'learn', 'history'],
        11: ['attract', 'incent', 'pull', 'direction'],
        12: ['harm', 'disrupt', 'transform', 'damage'],
        13: ['distort', 'noise', 'decay', 'perception'],
        14: ['flux', 'throughput', 'bandwidth', 'transfer'],
        15: ['depth', 'capacity', 'potential', 'latent']
    }

    # Score pillars by keyword match
    scores = {}
    for pillar_idx, words in keywords.items():
        score = sum(1 for w in words if w in task_lower)
        if score > 0:
            scores[pillar_idx] = score

    # Always include governance pillars (0, 12, 13)
    required = {0, 12, 13}
    selected = set(scores.keys()) | required

    # Limit to top N pillars if too many
    if len(selected) > 8:
        sorted_pillars = sorted(scores.items(), key=lambda x: x[1], reverse=True)
        selected = {p for p, _ in sorted_pillars[:5]} | required

    return loader.build_context(task=task, include_pillars=list(selected))


def chat_mode(model="qwen2.5:0.5b"):
    """Interactive chat with pillar context."""
    if not check_ollama_running():
        print("Ollama not running. Start with: ollama serve")
        return

    if not ensure_model(model):
        return

    loader = PillarLoader()
    loader.load_all()

    print(f"Pillar AI Chat - Model: {model}")
    print("Commands: /pillars, /reset, /quit")
    print("=" * 60)

    system_context = loader.build_context()
    conversation = [{'role': 'system', 'content': system_context}]

    while True:
        try:
            user_input = input("\nYou: ").strip()

            if user_input.lower() in ('/quit', '/exit', '/q'):
                break

            if user_input.lower() == '/pillars':
                print(loader.summary())
                continue

            if user_input.lower() == '/reset':
                conversation = [{'role': 'system', 'content': system_context}]
                print("Conversation reset.")
                continue

            if not user_input:
                continue

            # Build focused context for this query
            focused_context = build_focused_context(loader, user_input)
            conversation[0] = {'role': 'system', 'content': focused_context}

            conversation.append({'role': 'user', 'content': user_input})

            print("\nAI: ", end='', flush=True)
            response = stream_response(model, conversation)

            if response:
                conversation.append({'role': 'assistant', 'content': response})

                # Keep conversation manageable
                if len(conversation) > 10:
                    conversation = [conversation[0]] + conversation[-8:]

        except KeyboardInterrupt:
            print("\n\nExiting...")
            break
        except Exception as e:
            print(f"\nError: {e}")


def single_query(query, model="qwen2.5:0.5b", template='analyze'):
    """Process a single query with pillar context."""
    if not check_ollama_running():
        print("Ollama not running. Start with: ollama serve")
        return

    if not ensure_model(model):
        return

    loader = PillarLoader()
    loader.load_all()

    # Build context
    context = build_focused_context(loader, query)

    # Format prompt
    prompt = TEMPLATES.get(template, TEMPLATES['analyze']).format(task=query)

    messages = [
        {'role': 'system', 'content': context},
        {'role': 'user', 'content': prompt}
    ]

    print(f"Sending to {model}...")
    print("=" * 60)
    print("\nAI: ", end='', flush=True)
    stream_response(model, messages)


def main():
    parser = argparse.ArgumentParser(description="Pillar AI Ollama Wrapper")
    parser.add_argument('--mode', choices=['chat', 'query'], default='query')
    parser.add_argument('--model', default='qwen2.5:0.5b')
    parser.add_argument('--query', default='How do I balance team productivity with wellbeing?')
    parser.add_argument('--template', choices=['analyze', 'decompose', 'evaluate'], default='analyze')

    args = parser.parse_args()

    if args.mode == 'chat':
        chat_mode(args.model)
    else:
        single_query(args.query, args.model, args.template)


if __name__ == "__main__":
    main()
