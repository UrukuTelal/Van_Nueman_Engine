#!/usr/bin/env python3
"""
Ollama Integration Example
Shows how to use Pillar configs with local LLMs via Ollama.
"""

import sys
if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8')

from src.pillar_loader import PillarLoader

try:
    import ollama
    HAS_OLLAMA = True
except ImportError:
    HAS_OLLAMA = False
    print("Note: ollama package not installed. Install with: pip install ollama")


def analyze_with_pillars(task: str, model: str = "qwen2.5:0.5b"):
    """
    Analyze a task using pillar-aware reasoning.
    
    Args:
        task: The task or scenario to analyze
        model: Ollama model to use (qwen2.5:0.5b, qwen2.5:1.5b, etc.)
    """
    if not HAS_OLLAMA:
        print("Cannot run: ollama package not installed")
        return
    
    loader = PillarLoader()
    loader.load_all()
    
    # Build context with all pillars
    system_context = loader.build_context(task=task)
    
    # Craft prompt that leverages pillar framework
    prompt = f"""
Using the Pillar Framework provided in the system context:

TASK: {task}

Please analyze:
1. Which pillars are most relevant to this task?
2. What pillar interactions should I be aware of?
3. Are there any failure modes to watch for?
4. What pillar adjustments would optimize this outcome?

Format your response with clear pillar references (e.g., "Pillar 02 (Force) should be balanced with Pillar 09 (Warmth)").
"""
    
    print(f"Sending to {model}...")
    print("="*60)
    
    try:
        response = ollama.chat(model=model, messages=[
            {'role': 'system', 'content': system_context},
            {'role': 'user', 'content': prompt}
        ])
        
        print(response['message']['content'])
        
    except Exception as e:
        print(f"Error: {e}")
        print("\nTroubleshooting:")
        print("1. Ensure Ollama is running: ollama serve")
        print("2. Pull the model: ollama pull qwen2.5:0.5b")
        print("3. Check model name is correct")


def quick_chat(query: str, model: str = "qwen2.5:0.5b"):
    """
    Quick chat with pillar context injected.
    
    Args:
        query: User question
        model: Ollama model to use
    """
    if not HAS_OLLAMA:
        print("Cannot run: ollama package not installed")
        return
    
    loader = PillarLoader()
    loader.load_all()
    
    # Build minimal context (just blackboard + key pillars)
    context = loader.build_context(
        task=query,
        include_pillars=[0, 2, 5, 12]  # Awareness, Force, Integrity, Harm
    )
    
    print(f"Querying {model}...")
    print("="*60)
    
    try:
        response = ollama.chat(model=model, messages=[
            {'role': 'system', 'content': context},
            {'role': 'user', 'content': query}
        ])
        
        print(response['message']['content'])
        
    except Exception as e:
        print(f"Error: {e}")


def interactive_mode(model: str = "qwen2.5:0.5b"):
    """
    Interactive REPL with pillar-aware AI.
    """
    if not HAS_OLLAMA:
        print("Cannot run: ollama package not installed")
        return
    
    loader = PillarLoader()
    loader.load_all()
    
    print(f"Pillar AI Chat (Model: {model})")
    print("Type 'quit' or 'exit' to stop")
    print("Type 'pillars' to list loaded pillars")
    print("="*60)
    
    # Pre-load context
    system_context = loader.build_context()
    
    while True:
        try:
            user_input = input("\nYou: ").strip()
            
            if user_input.lower() in ('quit', 'exit'):
                break
            
            if user_input.lower() == 'pillars':
                print(loader.summary())
                continue
            
            if not user_input:
                continue
            
            response = ollama.chat(model=model, messages=[
                {'role': 'system', 'content': system_context},
                {'role': 'user', 'content': user_input}
            ])
            
            print(f"\nAI: {response['message']['content']}")
            
        except KeyboardInterrupt:
            print("\n\nExiting...")
            break
        except Exception as e:
            print(f"\nError: {e}")
            print("Make sure Ollama is running: ollama serve")


if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Pillar AI + Ollama Integration")
    parser.add_argument(
        '--mode', 
        choices=['analyze', 'chat', 'interactive'],
        default='analyze',
        help='Operation mode'
    )
    parser.add_argument(
        '--model',
        default='qwen2.5:0.5b',
        help='Ollama model to use'
    )
    parser.add_argument(
        '--query',
        default='How do I balance team productivity with employee wellbeing?',
        help='Query to analyze'
    )
    
    args = parser.parse_args()
    
    if args.mode == 'analyze':
        analyze_with_pillars(args.query, args.model)
    elif args.mode == 'chat':
        quick_chat(args.query, args.model)
    elif args.mode == 'interactive':
        interactive_mode(args.model)
