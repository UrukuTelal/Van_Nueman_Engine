#!/usr/bin/env python3
"""
Iterative timeout test for large models.
Starts at 60s (1 min), adds 60s each iteration until response received.
Logs response time to blackboard.
Uses threading instead of signal for Windows compatibility.
"""
import sys
import time
import threading
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))
from scripts.task_delegator import delegate_task, MODEL_CONFIGS, log_to_blackboard
import ollama

class TimeoutError(Exception):
    """Custom timeout exception."""
    pass

def test_with_iterative_timeout(model: str, task: str, max_timeout: int = 600000):
    """
    Test a model with iterative timeout increase.
    Start at 60000ms (1 min), add 60000ms each iteration.
    Uses threading to enforce timeout on Python side (Windows compatible).
    """
    timeout_ms = 60000  # Start at 1 minute
    iteration = 1
    
    print(f"\n{'='*60}")
    print(f"Testing {model} with iterative timeout")
    print(f"Task: {task}")
    print(f"{'='*60}\n")
    
    while timeout_ms <= max_timeout:
        timeout_s = timeout_ms / 1000
        print(f"\n[Iteration {iteration}] Timeout: {timeout_s:.0f}s")
        
        start_time = time.time()
        result = {'response': None, 'error': None, 'done': False}
        
        def run_model():
            try:
                result['response'] = ollama.chat(
                    model=model,
                    messages=[{'role': 'user', 'content': task}],
                    stream=False
                )
            except Exception as e:
                result['error'] = str(e)
            finally:
                result['done'] = True
        
        # Run model in separate thread
        thread = threading.Thread(target=run_model)
        thread.daemon = True
        thread.start()
        
        # Wait with timeout
        thread.join(timeout_s)
        
        end_time = time.time()
        elapsed = end_time - start_time
        
        if result['done'] and result['response']:
            response_time = elapsed
            print(f"\n✓ Response received in {response_time:.1f}s (timeout was {timeout_s:.0f}s)")
            print(f"\nResponse:\n{result['response']['message']['content'][:500]}...")
            
            # Log to blackboard
            delta_h = MODEL_CONFIGS.get(model, {}).get("delta_h", 0.05)
            log_to_blackboard(
                task=f"Large model test: {task}",
                delta_h=delta_h,
                status="APPROVED",
                notes=f"Model: {model}, Response time: {response_time:.1f}s, Timeout used: {timeout_s:.0f}s, Iteration: {iteration}"
            )
            
            return True, response_time, timeout_ms
            
        elif result['done'] and result['error']:
            print(f"✗ Error: {result['error']}")
            log_to_blackboard(
                task=f"Large model test FAILED: {task}",
                delta_h=MODEL_CONFIGS.get(model, {}).get("delta_h", 0.05),
                status="FAILED",
                notes=f"Model: {model}, Error: {result['error'][:100]}, Timeout used: {timeout_s:.0f}s"
            )
            return False, elapsed, timeout_ms
            
        else:
            # Timeout - thread is still running
            print(f"✗ Timeout after {elapsed:.1f}s (iteration {iteration})")
            timeout_ms += 60000  # Add 1 minute
            iteration += 1
            continue
    
    print(f"\n✗ Max timeout {max_timeout/1000:.0f}s reached without response")
    return False, 0, timeout_ms

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python scripts/test_large_model.py <model> <task>")
        sys.exit(1)
    
    model = sys.argv[1]
    task = " ".join(sys.argv[2:])
    
    success, response_time, timeout_used = test_with_iterative_timeout(model, task)
    
    if success:
        print(f"\n✓ Test PASSED")
        print(f"Response time: {response_time:.1f}s")
        print(f"Timeout used: {timeout_used/1000:.0f}s")
    else:
        print(f"\n✗ Test FAILED")
