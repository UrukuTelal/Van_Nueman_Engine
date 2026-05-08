#!/usr/bin/env python3
"""
Workflow Engine: Translates user prompts into multi-agent workflows.
Uses LLM to decompose complex prompts into multiple agent steps.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent))

from scripts.agent_roles import route_task_to_role, get_model_for_role
from scripts.memory_translator import MemoryTranslator
import json
import uuid
import threading
import time

class WorkflowStep:
    """A single step in a workflow."""
    def __init__(self, agent_role, task, step_id=None, dependencies=None):
        self.agent_role = agent_role
        self.task = task
        self.step_id = step_id
        self.dependencies = dependencies or []
        self.result = None
        self.status = "pending"  # pending, running, completed, failed
    
    def to_dict(self):
        return {
            "step_id": self.step_id,
            "agent": self.agent_role,
            "task": self.task,
            "dependencies": self.dependencies,
            "status": self.status,
            "result": self.result
        }


class WorkflowEngine:
    """
    Translates a user prompt into a workflow of agent tasks.
    Uses LLM to decompose complex prompts into multiple steps.
    """
    def __init__(self):
        self.translator = MemoryTranslator()
        self.workflows = {}  # workflow_id -> workflow steps
        self.workflow_status = {}  # workflow_id -> status
    
    def parse_prompt_to_workflow(self, prompt: str) -> list:
        """
        Use LLM to decompose prompt into multiple agent steps.
        Returns list of WorkflowStep objects.
        """
        # Try to use LLM to decompose the prompt
        try:
            import ollama
            
            decomposition_prompt = f"""
Analyze this task and decompose it into steps, where each step should be handled by a specific type of agent.

Task: {prompt}

Available agent types: quick_chat, code_assistant, general_assistant, deep_reasoning, thinking_agent, project_manager, experimental, messenger

For each step, output in this exact format:
STEP: <step_number>
AGENT: <agent_type>
TASK: <specific_task_for_this_step>

Example:
STEP: 1
AGENT: project_manager
TASK: Plan the architecture for auth system

STEP: 2
AGENT: code_assistant
TASK: Implement JWT token generation endpoint

STEP: 3
AGENT: general_assistant
TASK: Document the API endpoints

Now decompose the task above:
"""

            response = ollama.chat(
                model='qwen2.5:0.5b',
                messages=[{'role': 'user', 'content': decomposition_prompt}]
            )
            
            response_text = response['message']['content']
            steps = self._parse_llm_response(response_text, prompt)
            
            if len(steps) > 1:
                print(f"[Workflow] LLM decomposed into {len(steps)} steps")
                return steps
            else:
                print("[Workflow] LLM decomposition failed, using neural routing")
                
        except Exception as e:
            print(f"[Workflow] LLM decomposition error: {e}")
        
        # Fallback: Use neural routing for single step
        return self._create_single_step(prompt)
    
    def _parse_llm_response(self, response_text: str, original_prompt: str) -> list:
        """Parse LLM response into workflow steps."""
        steps = []
        current_step = None
        
        for line in response_text.split('\n'):
            line = line.strip()
            
            if line.startswith('STEP:') or line.startswith('Step') or 'step' in line.lower():
                if current_step and current_step.agent_role:
                    steps.append(current_step)
                step_id = f"step_{len(steps) + 1}"
                current_step = WorkflowStep(agent_role="", task="", step_id=step_id)
                
            elif line.startswith('AGENT:') or line.startswith('Agent'):
                if current_step:
                    agent = line.split(':', 1)[1].strip().lower()
                    # Map to valid agent role
                    valid_agents = ['quick_chat', 'code_assistant', 'general_assistant', 
                                  'deep_reasoning', 'thinking_agent', 'project_manager',
                                  'experimental', 'messenger']
                    if agent in valid_agents:
                        current_step.agent_role = agent
                    else:
                        # Try to match
                        for valid in valid_agents:
                            if valid in agent or agent in valid:
                                current_step.agent_role = valid
                                break
            
            elif line.startswith('TASK:') or line.startswith('Task'):
                if current_step:
                    task = line.split(':', 1)[1].strip()
                    current_step.task = task
        
        # Don't forget the last step
        if current_step and current_step.agent_role:
            steps.append(current_step)
        
        # Validate steps
        valid_steps = []
        for step in steps:
            if step.agent_role and step.task:
                valid_steps.append(step)
        
        return valid_steps if valid_steps else []
    
    def _create_single_step(self, prompt: str) -> list:
        """Create a single step using neural routing."""
        try:
            result = route_task_to_role(prompt)
            if isinstance(result, tuple) and len(result) >= 1:
                agent_role = result[0]  # (agent_role, method)
            else:
                agent_role = result if isinstance(result, str) else "general_assistant"
        except Exception as e:
            print(f"[_create_single_step] Error: {e}")
            agent_role = "general_assistant"
        
        step = WorkflowStep(
            agent_role=agent_role,
            task=prompt,
            step_id="step_1"
        )
        
        return [step]
    
    def execute_workflow(self, workflow_steps: list, workflow_id: str = None) -> dict:
        """Execute workflow steps (call agents via Ollama)."""
        if workflow_id is None:
            workflow_id = str(uuid.uuid4())
        
        self.workflow_status[workflow_id] = {
            "status": "running",
            "total_steps": len(workflow_steps),
            "completed_steps": 0,
            "results": []
        }
        
        try:
            import ollama
        except ImportError:
            self.workflow_status[workflow_id]["status"] = "failed"
            return {"error": "Ollama not available", "workflow_id": workflow_id}
        
        results = []
        for i, step in enumerate(workflow_steps):
            step.status = "running"
            model = get_model_for_role(step.agent_role)
            
            if not model:
                step.status = "failed"
                error_result = {
                    "step_id": step.step_id,
                    "agent": step.agent_role,
                    "error": f"No model found for agent: {step.agent_role}"
                }
                results.append(error_result)
                self.workflow_status[workflow_id]["results"].append(error_result)
                continue
            
            try:
                # Build context for this step
                from src.pillar_loader import PillarLoader
                loader = PillarLoader()
                loader.load_all()
                context = loader.build_context(task=step.task)
                
                # Call Ollama with agent's model (with timeout)
                import sys
                if sys.platform == 'win32':
                    # Windows: use timeout
                    response = ollama.chat(
                        model=model,
                        messages=[
                            {'role': 'system', 'content': context},
                            {'role': 'user', 'content': step.task}
                        ],
                        options={'timeout': 30}  # 30 second timeout
                    )
                else:
                    response = ollama.chat(
                        model=model,
                        messages=[
                            {'role': 'system', 'content': context},
                            {'role': 'user', 'content': step.task}
                        ]
                    )
                
                step.result = response['message']['content']
                step.status = "completed"
                
                result_dict = step.to_dict()
                results.append(result_dict)
                self.workflow_status[workflow_id]["results"].append(result_dict)
                self.workflow_status[workflow_id]["completed_steps"] = i + 1
                
            except Exception as e:
                step.status = "failed"
                error_result = {
                    "step_id": step.step_id,
                    "agent": step.agent_role,
                    "model": model,
                    "task": step.task,
                    "error": str(e)
                }
                results.append(error_result)
                self.workflow_status[workflow_id]["results"].append(error_result)
        
        self.workflow_status[workflow_id]["status"] = "completed"
        self.workflows[workflow_id] = results
        
        return {"workflow_id": workflow_id, "status": "completed", "results": results}
    
    def execute_workflow_async(self, prompt: str, workflow_id: str = None):
        """Execute workflow asynchronously."""
        if workflow_id is None:
            workflow_id = str(uuid.uuid4())

        # Initialize status
        self.workflow_status[workflow_id] = {
            'status': 'decomposing',
            'current_step': 0,
            'total_steps': 0,
            'results': [],
            'error': None
        }

        # Execute in background thread (includes decomposition)
        def run_workflow():
            try:
                steps = self.parse_prompt_to_workflow(prompt)
                self.execute_workflow(steps, workflow_id)
            except Exception as e:
                self.workflow_status[workflow_id]['status'] = 'error'
                self.workflow_status[workflow_id]['error'] = str(e)

        thread = threading.Thread(target=run_workflow)
        thread.start()

        return workflow_id
    
    def get_workflow_status(self, workflow_id: str) -> dict:
        """Get workflow execution status."""
        if workflow_id in self.workflow_status:
            return self.workflow_status[workflow_id]
        return {"error": "Workflow not found", "workflow_id": workflow_id}
    
    def save_workflow(self, workflow_id: str, steps: list):
        """Save workflow to disk."""
        import os
        workflow_dir = Path("workflows")
        workflow_dir.mkdir(exist_ok=True)
        
        data = [step.to_dict() for step in steps]
        with open(workflow_dir / f"{workflow_id}.json", 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2)
    
    def load_workflow(self, workflow_id: str) -> list:
        """Load workflow from disk."""
        workflow_path = Path("workflows") / f"{workflow_id}.json"
        if not workflow_path.exists():
            return []
        
        with open(workflow_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        steps = []
        for item in data:
            step = WorkflowStep(
                agent_role=item['agent'],
                task=item['task'],
                step_id=item.get('step_id'),
                dependencies=item.get('dependencies', [])
            )
            step.result = item.get('result')
            step.status = item.get('status', 'pending')
            steps.append(step)
        
        return steps


if __name__ == "__main__":
    print("Testing Workflow Engine...")
    
    engine = WorkflowEngine()
    
    # Test 1: Simple prompt
    print("\nTest 1: Single prompt routing")
    prompt = "Write a Python function to calculate fibonacci numbers"
    steps = engine.parse_prompt_to_workflow(prompt)
    print(f"Routed to: {steps[0].agent_role}")
    print(f"Task: {steps[0].task[:50]}...")
    
    # Test 2: Execute (if Ollama available)
    print("\nTest 2: Execute workflow")
    try:
        import ollama
        results = engine.execute_workflow(steps)
        print(f"Results: {json.dumps(results, indent=2)[:200]}...")
    except ImportError:
        print("Ollama not available - skipping execution")
    
    print("\nWorkflow Engine test complete!")
