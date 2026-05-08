#!/usr/bin/env python3
"""
Distortion Sandbox Workflow - Hallucination Management System

Implements the Distortion Sandbox for managing agent hallucinations as innovation hypotheses.

Features:
1. Capture: Tag hallucinations as Distortion Events
2. Multimodal Testing: Bit-Shift Parity, WHT Resonance, Thermal Efficiency
3. Survivor Logic: Graduate to Candidate if outperforms
4. 16-Pillar Integration: All pillars manage the innovation process
5. Shadow Thread: Parallel testing with production code using real subprocess execution
"""

import re
import sys
import json
import time
import hashlib
import subprocess
from pathlib import Path
from datetime import datetime, timezone
from typing import Optional, Dict, List, Tuple
from dataclasses import dataclass, field

sys.path.insert(0, str(Path(__file__).parent.parent))

try:
    from src.pillar_loader import PillarLoader
    PILLAR_LOADER_AVAILABLE = True
except ImportError:
    PILLAR_LOADER_AVAILABLE = False
    print("Warning: PillarLoader not available")

try:
    import numpy as np
    NUMPY_AVAILABLE = True
except ImportError:
    NUMPY_AVAILABLE = False
    print("Warning: numpy not available for WHT analysis")

# Distortion Event storage
DISTORTION_EVENTS_PATH = Path(__file__).parent / "distortion_events.json"
CANDIDATES_PATH = Path(__file__).parent / "distortion_candidates.json"


@dataclass
class DistortionEvent:
    """
    Represents a hallucination tagged as a Distortion Event.
    
    Attributes:
        id: Unique event identifier (12-char hex)
        timestamp: ISO 8601 timestamp of capture
        agent_name: Name of agent that generated the hallucination
        model: Ollama model used (e.g., qwen2.5:7b)
        original_logic: The standard logic that was expected
        distorted_logic: The hallucinated logic (potential innovation)
        task_context: Description of the task context
        pillar_scores: Dict mapping pillar names to scores (0.0-1.0)
        test_results: Dict mapping test names to scores (0.0-1.0)
        status: Current status (CAPTURED, TESTING, CANDIDATE, GRADUATED, REJECTED)
        performance_delta: Overall performance score
    """
    id: str
    timestamp: str
    agent_name: str
    model: str
    original_logic: str
    distorted_logic: str
    task_context: str
    pillar_scores: Dict[str, float] = field(default_factory=dict)
    test_results: Dict[str, float] = field(default_factory=dict)
    status: str = "CAPTURED"
    performance_delta: float = 0.0
    
    def to_dict(self) -> Dict:
        """Convert event to JSON-serializable dictionary."""
        return {
            "id": self.id,
            "timestamp": self.timestamp,
            "agent_name": self.agent_name,
            "model": self.model,
            "original_logic": self.original_logic[:500],
            "distorted_logic": self.distorted_logic[:500],
            "task_context": self.task_context[:200],
            "pillar_scores": self.pillar_scores,
            "test_results": self.test_results,
            "status": self.status,
            "performance_delta": self.performance_delta
        }
    
    @classmethod
    def from_dict(cls, data: Dict) -> 'DistortionEvent':
        """Create event from dictionary."""
        return cls(**data)


class DistortionSandbox:
    """
    Main sandbox for managing distortion events and testing.
    
    Handles the complete workflow:
    1. Capturing hallucinations as Distortion Events
    2. Running multimodal testing (Bit-Shift, WHT, Thermal)
    3. Evaluating survivor logic for graduation
    4. Running Shadow Thread comparison with real execution
    5. 16-Pillar integration for innovation management
    """
    
    def __init__(self):
        """Initialize the sandbox with pillar loader and data loading."""
        self.pillar_loader = None
        self.events: List[DistortionEvent] = []
        self.candidates: List[DistortionEvent] = []
        
        # TODO[MEMORY]: Memory leak risk - events and candidates lists grow forever
        # Fix: Add max size limit or periodic cleanup
        # TODO: Add self.max_events = 1000 and truncate in _save_data()
        
        self._load_data()
        
        if PILLAR_LOADER_AVAILABLE:
            try:
                self.pillar_loader = PillarLoader(str(Path(__file__).parent.parent))
                self.pillar_loader.load_all()
            except Exception as e:
                print(f"Warning: Could not load pillars: {e}")
    
    def _load_data(self):
        """
        Load existing events and candidates from JSON files.
        
        TODO[MEMORY]: Loading all events into memory - could be slow with 1000s of events
        Fix: Consider lazy loading or database backend
        """
        if DISTORTION_EVENTS_PATH.exists():
            try:
                data = json.loads(DISTORTION_EVENTS_PATH.read_text(encoding='utf-8'))
                self.events = [DistortionEvent.from_dict(e) for e in data]
            except Exception as e:
                print(f"Error loading events: {e}")
        
        if CANDIDATES_PATH.exists():
            try:
                data = json.loads(CANDIDATES_PATH.read_text(encoding='utf-8'))
                self.candidates = [DistortionEvent.from_dict(e) for e in data]
            except Exception as e:
                print(f"Error loading candidates: {e}")
    
    def _save_data(self):
        """Save events and candidates to JSON files."""
        try:
            events_data = [e.to_dict() for e in self.events]
            DISTORTION_EVENTS_PATH.write_text(
                json.dumps(events_data, indent=2), encoding='utf-8'
            )
            
            candidates_data = [c.to_dict() for c in self.candidates]
            CANDIDATES_PATH.write_text(
                json.dumps(candidates_data, indent=2), encoding='utf-8'
            )
        except Exception as e:
            print(f"Error saving data: {e}")
    
    def capture_distortion(self, agent_name: str, model: str,
                          original: str, distorted: str, 
                          task_context: str) -> str:
        """
        Step 1: Capture a hallucination as a Distortion Event.
        
        Args:
            agent_name: Name of the agent that generated the hallucination
            model: Ollama model used (e.g., qwen2.5:7b)
            original: The standard logic that was expected
            distorted: The hallucinated logic (potential innovation)
            task_context: Description of the task context
            
        Returns:
            event_id: Unique identifier for the captured event
        """
        event_id = hashlib.md5(
            f"{agent_name}{distorted}{datetime.now().isoformat()}".encode()
        ).hexdigest()[:12]
        
        event = DistortionEvent(
            id=event_id,
            timestamp=datetime.now(timezone.utc).isoformat(),
            agent_name=agent_name,
            model=model,
            original_logic=original,
            distorted_logic=distorted,
            task_context=task_context,
            status="CAPTURED"
        )
        
        # Calculate initial pillar scores
        if self.pillar_loader:
            event.pillar_scores = self._calculate_pillar_scores(distorted)
        
        self.events.append(event)
        self._save_data()
        
        print(f"[CAPTURED] Distortion Event {event_id}")
        print(f"  Agent: {agent_name} ({model})")
        print(f"  Task: {task_context[:50]}...")
        
        return event_id
    
    def _calculate_pillar_scores(self, logic: str) -> Dict[str, float]:
        """
        Calculate how the logic affects each of the 16 pillars.
        
        TODO[BUG]: Pillar 12 (Harm) scoring is inverted - lower should be better
        Currently adds harm_keywords which INCREASES harm score
        Fix: Harm should decrease for "safe" keywords, increase for dangerous ones
        
        TODO[OPTIMIZE]: Current method uses simple keyword counting
        Should use semantic analysis or at least word boundary checking
        e.g., "destroy" vs "destroyed" vs "destroys" - currently only exact match
        
        Args:
            logic: The logic string to analyze
            
        Returns:
            scores: Dictionary mapping pillar names to scores (0.0-1.0)
        """
        scores = {}
        
        def wc(word): return len(re.findall(r'\b' + re.escape(word) + r'\b', logic))
        
        # Pillar 0: Awareness - Does this logic improve perception?
        scores["awareness"] = 0.5 + (wc("detect") + wc("sense")) * 0.05
        
        # Pillar 1: Willpower - Does this persist in strategy?
        scores["willpower"] = 0.5 + (wc("persist") + wc("retry")) * 0.05
        
        # Pillar 2: Force - Execution capability
        scores["force"] = 0.5 + (wc("execute") + wc("apply")) * 0.05
        
        # Pillar 3: Influence - Spread potential
        scores["influence"] = 0.5 + (wc("propagate") + wc("broadcast")) * 0.05
        
        # Pillar 4: Resistance - Anti-fragility
        scores["resistance"] = 0.5 + (wc("protect") + wc("defend")) * 0.05
        
        # Pillar 5: Integrity - Alignment with standards
        scores["integrity"] = 0.5 + (wc("validate") + wc("check")) * 0.05
        
        # Pillar 6: Cohesion - Binding capability
        scores["cohesion"] = 0.5 + (wc("bind") + wc("group")) * 0.05
        
        # Pillar 7: Relation - Network connectivity
        scores["relation"] = 0.5 + (wc("connect") + wc("link")) * 0.05
        
        # Pillar 8: Presence - Visibility
        scores["presence"] = 0.5 + (wc("show") + wc("display")) * 0.05
        
        # Pillar 9: Warmth - Energy transfer efficiency
        scores["warmth"] = 0.5 + (wc("transfer") + wc("share")) * 0.05
        
        # Pillar 10: Memory - Historical learning
        scores["memory"] = 0.5 + (wc("remember") + wc("history")) * 0.05
        
        # Pillar 11: Attraction - Goal alignment
        scores["attraction"] = 0.5 + (wc("attract") + wc("pull")) * 0.05
        
        # Pillar 12: Harm - higher score = safer (less harmful)
        harm_keywords = ["destroy", "damage", "break", "crash"]
        harm_penalty = sum(1 for k in harm_keywords if re.search(r'\b' + re.escape(k) + r'\b', logic))
        scores["harm"] = 0.7 - harm_penalty * 0.1
        
        # Pillar 13: Distortion - Information quality (this IS a distortion)
        scores["distortion"] = 0.7  # Base high, as it's already tagged
        
        # Pillar 14: Flux - Change capacity (holds the distortion)
        scores["flux"] = 0.6 + (wc("change") + wc("update")) * 0.05
        
        # Pillar 15: Depth - Historical comparison
        scores["depth"] = 0.5 + (wc("deep") + wc("complex")) * 0.05
        
        # Clamp all scores to [0.0, 1.0]
        for key in scores:
            scores[key] = max(0.0, min(1.0, scores[key]))
        
        return scores
    
    def run_multimodal_testing(self, event_id: str) -> Dict[str, float]:
        """
        Step 2: Run multimodal testing on a distortion event.
        
        Tests:
        1. Bit-Shift Parity: Does the distorted logic break fixed-point arithmetic?
        2. WHT Resonance: Does the distorted logic produce cleaner WHT signals?
        3. Thermal Efficiency: Does the code run cooler (lower TDP)?
        
        Args:
            event_id: ID of the event to test
            
        Returns:
            results: Dictionary mapping test names to scores (0.0-1.0)
        """
        event = next((e for e in self.events if e.id == event_id), None)
        if not event:
            print(f"Event {event_id} not found")
            return {}
        
        event.status = "TESTING"
        results = {}
        
        # Test 1: Bit-Shift Parity
        results["bit_shift_parity"] = self._test_bit_shift_parity(
            event.original_logic, event.distorted_logic
        )
        
        # Test 2: WHT Resonance
        if NUMPY_AVAILABLE:
            results["wht_resonance"] = self._test_wht_resonance(
                event.distorted_logic
            )
        else:
            results["wht_resonance"] = 0.5  # Neutral if can't test
        
        # Test 3: Thermal Efficiency (simulated)
        results["thermal_efficiency"] = self._test_thermal_efficiency(
            event.original_logic, event.distorted_logic
        )
        
        event.test_results = results
        self._save_data()
        
        print(f"[TESTING] Event {event_id} results:")
        for test, score in results.items():
            print(f"  {test}: {score:.3f}")
        
        return results
    
    def _test_bit_shift_parity(self, original: str, distorted: str) -> float:
        """
        Bit-Shift Parity: Does the distorted logic break fixed-point arithmetic?
        
        Returns score (higher = better compatibility).
        Score meanings:
        - 1.0: No fixed-point operations, fully compatible
        - 0.8: Uses bit shifts, potentially more efficient
        - 0.7: Neutral
        - 0.5: Simplified things
        - 0.3: Penalize if distortion removes necessary bit operations
        """
        # Count operations that affect fixed-point math
        original_ops = sum(original.count(op) for op in ["<<", ">>", "*", "/", "int32_t"])
        distorted_ops = sum(distorted.count(op) for op in ["<<", ">>", "*", "/", "int32_t"])
        
        if original_ops == 0 and distorted_ops == 0:
            return 1.0  # No fixed-point operations, fully compatible
        
        if distorted_ops == 0:
            return 0.9  # Distortion simplified things
        
        # Check for bit-shift shortcuts
        if "<<" in distorted and ">>" in distorted:
            return 0.8  # Uses bit shifts, potentially more efficient
        
        # Penalize if distortion removes necessary bit operations
        if original_ops > 0 and distorted_ops == 0:
            return 0.3
        
        return 0.7  # Neutral
    
    def _test_wht_resonance(self, logic: str) -> float:
        """
        WHT Resonance: Does the distorted logic produce cleaner WHT signals?
        
        Returns resonance score (higher = cleaner signal).
        
        TODO[BUG]: Current SNR calculation is incorrect.
        - signal_power = np.mean(wht_result ** 2) is not true signal power
        - noise_power = np.var(wht_result) is not true noise power
        - WHT coefficients: larger = more signal, smaller = more noise
        FIX: Calculate signal as sum of top-k coefficients, noise as remainder.
        """
        if not NUMPY_AVAILABLE:
            return 0.5
        
        try:
            # Convert logic to signal (character frequency spectrum)
            chars = [ord(c) for c in logic[:32]]
            if len(chars) < 32:
                chars.extend([0] * (32 - len(chars)))
            
            signal = np.array(chars, dtype=np.float32)
            
            # Simple WHT (Walsh-Hadamard Transform)
            n = len(signal)
            wht_result = signal.copy()
            step = 1
            
            while step < n:
                for i in range(0, n, 2 * step):
                    for j in range(i, i + step):
                        u = wht_result[j]
                        v = wht_result[j + step]
                        wht_result[j] = u + v
                        wht_result[j + step] = u - v
                step *= 2
            
            wht_result = wht_result / np.sqrt(n)
            
            # SNR via top-25% WHT coefficients as signal, bottom-75% as noise
            coeffs = wht_result.ravel()
            abs_coeffs = np.abs(coeffs)
            sorted_idx = np.argsort(abs_coeffs)
            n_total = coeffs.size
            n_signal = int(np.ceil(0.25 * n_total))
            n_noise = n_total - n_signal
            signal_idx = sorted_idx[-n_signal:]
            noise_idx = sorted_idx[:n_noise]
            signal_power = np.mean(coeffs[signal_idx] ** 2)
            noise_power = np.mean(coeffs[noise_idx] ** 2)
            if noise_power == 0:
                return 1.0
            snr = 10 * np.log10(signal_power / noise_power)
            return min(1.0, max(0.0, 0.5 + snr / 20.0))
            
        except Exception as e:
            print(f"WHT test error: {e}")
            return 0.5
    
    def _test_thermal_efficiency(self, original: str, distorted: str) -> float:
        """
        Thermal Efficiency: Does the code run cooler (lower TDP)?
        
        Simulated by counting operations and complexity.
        Returns efficiency score (higher = cooler).
        """
        # Count operations (proxy for compute intensity)
        original_ops = len(original.split()) + original.count("for") * 5
        distorted_ops = len(distorted.split()) + distorted.count("for") * 5
        
        if original_ops == 0:
            return 0.5
        
        # Calculate efficiency ratio
        efficiency = original_ops / max(distorted_ops, 1)
        
        # Normalize to [0, 1]
        return min(1.0, max(0.0, efficiency / 2.0))
    
    def evaluate_survivor_logic(self, event_id: str) -> Tuple[bool, float]:
        """
        Step 3: Survivor Logic - Does the distortion outperform standard?
        
        TODO[LOGIC]: Harm pillar not properly accounted for in graduation
        - Currently: avg_score > 0.6 graduates (ignores Harm)
        - Harm should be weighted: lower Harm = better (more likely to graduate)
        - Fix: If Harm > 0.7 (threshold), automatically REJECT regardless of other scores
        - Or: Subtract Harm score from avg_score before comparing to threshold
        
        Args:
            event_id: ID of the event to evaluate
            
        Returns:
            (should_graduate, performance_delta)
        """
        event = next((e for e in self.events if e.id == event_id), None)
        if not event or not event.test_results:
            print(f"Event {event_id} not found or not tested")
            return False, 0.0
        
        # TODO[BUG]: event.performance_delta set but never used elsewhere
        # Should be used in candidation comparison or reporting
        
        # Calculate weighted performance score
        weights = {
            "bit_shift_parity": 0.3,
            "wht_resonance": 0.4,
            "thermal_efficiency": 0.3
        }
        
        total_score = 0.0
        total_weight = 0.0
        
        for test, score in event.test_results.items():
            if test in weights:
                total_score += score * weights[test]
                total_weight += weights[test]
        
        if total_weight == 0:
            return False, 0.0
        
        avg_score = total_score / total_weight
        event.performance_delta = avg_score
        
        # TODO[LOGIC]: Apply Harm penalty before graduation check
        # Pillar Harm: if event.pillar_scores["harm"] < 0.3 (unsafe), should reject
        harm_score = event.pillar_scores.get("harm", 0.5)
        if harm_score < 0.3:
            print(f"[REJECTED] Event {event_id} - Harm score {harm_score:.2f} below 0.3 safety threshold")
            event.status = "REJECTED"
            self._save_data()
            return False, avg_score
        
        # Graduate if average score > 0.6 (outperforms in any metric)
        should_graduate = avg_score > 0.6
        
        if should_graduate:
            event.status = "CANDIDATE"
            self.candidates.append(event)
            print(f"[GRADUATED] Event {event_id} -> CANDIDATE (score: {avg_score:.3f})")
        else:
            event.status = "REJECTED"
            print(f"[REJECTED] Event {event_id} (score: {avg_score:.3f})")
        
        self._save_data()
        return should_graduate, avg_score
    
    def run_shadow_thread(self, event_id: str, production_logic: str) -> Dict[str, any]:
        """
        Step 5: Shadow Thread - Run distorted logic in parallel with production.
        
        Compares outputs in real-time using actual subprocess execution.
        
        Args:
            event_id: ID of the event to test
            production_logic: The standard logic to run in production thread
            
        Returns:
            Dictionary with production_output, distorted_output, comparison results
        """
        event = next((e for e in self.events if e.id == event_id), None)
        if not event:
            return {"error": "Event not found"}
        
        print(f"[SHADOW THREAD] Starting parallel execution...")
        print(f"  Production: {production_logic[:50]}...")
        print(f"  Distorted:   {event.distorted_logic[:50]}...")
        
        # Actually run the logic in subprocesses (not just simulate)
        try:
            # Write logic to temporary files
            prod_file = Path("/tmp/production_logic.py")
            dist_file = Path("/tmp/distorted_logic.py")
            
            prod_file.write_text(f"# Production logic\n{production_logic}", encoding='utf-8')
            dist_file.write_text(f"# Distorted logic\n{event.distorted_logic}", encoding='utf-8')
            
            # Execute in subprocesses with timeout
            prod_start = time.time()
            prod_result = subprocess.run(
                ["python", str(prod_file)],
                capture_output=True, text=True, timeout=30
            )
            prod_time = time.time() - prod_start
            
            dist_start = time.time()
            dist_result = subprocess.run(
                ["python", str(dist_file)],
                capture_output=True, text=True, timeout=30
            )
            dist_time = time.time() - dist_start
            
            results = {
                "production_output": {
                    "stdout": prod_result.stdout[:200],
                    "stderr": prod_result.stderr[:200],
                    "returncode": prod_result.returncode,
                    "execution_time": prod_time
                },
                "distorted_output": {
                    "stdout": dist_result.stdout[:200],
                    "stderr": dist_result.stderr[:200],
                    "returncode": dist_result.returncode,
                    "execution_time": dist_time
                },
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "comparison": {}
            }
            
            # Compare outputs
            results["comparison"] = {
                "output_match": prod_result.stdout == dist_result.stdout,
                "distorted_faster": dist_time < prod_time,
                "distorted_cooler": dist_time < prod_time,  # Proxy for thermal
                "prod_returncode": prod_result.returncode,
                "dist_returncode": dist_result.returncode
            }
            
            print(f"[COMPARISON] Shadow thread results:")
            for key, value in results["comparison"].items():
                print(f"  {key}: {value}")
            
            # Cleanup temp files
            prod_file.unlink(missing_ok=True)
            dist_file.unlink(missing_ok=True)
            
            return results
            
        except subprocess.TimeoutExpired:
            print(f"[SHADOW THREAD] Execution timeout")
            return {"error": "Execution timeout"}
        except Exception as e:
            print(f"[SHADOW THREAD] Error: {e}")
            return {"error": str(e)}
    
    def _simulate_execution(self, logic: str) -> Dict[str, float]:
        """
        Simulate execution of logic and return metrics.
        
        Note: This is a fallback if subprocess execution fails.
        For real execution, use run_shadow_thread() instead.
        """
        # Simulate based on logic complexity
        complexity = len(logic.split()) + logic.count("for") * 10
        
        return {
            "execution_time": complexity * 0.001,  # seconds
            "thermal_cost": complexity * 0.01,  # arbitrary thermal units
            "output_hash": hashlib.md5(logic.encode()).hexdigest()[:8]
        }
    
    def get_status(self) -> Dict:
        """Get sandbox status."""
        return {
            "total_events": len(self.events),
            "candidates": len(self.candidates),
            "status_counts": {
                "CAPTURED": sum(1 for e in self.events if e.status == "CAPTURED"),
                "TESTING": sum(1 for e in self.events if e.status == "TESTING"),
                "CANDIDATE": sum(1 for e in self.events if e.status == "CANDIDATE"),
                "GRADUATED": sum(1 for e in self.events if e.status == "GRADUATED"),
                "REJECTED": sum(1 for e in self.events if e.status == "REJECTED"),
            },
            "pillar_integration": PILLAR_LOADER_AVAILABLE
        }
    
    def list_events(self, status_filter: Optional[str] = None) -> List[Dict]:
        """List all events, optionally filtered by status."""
        events = self.events
        if status_filter:
            events = [e for e in events if e.status == status_filter]
        return [e.to_dict() for e in events]


def main():
    """Main entry point for command-line usage."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="Distortion Sandbox - Hallucination Management"
    )
    parser.add_argument('--capture', action='store_true', 
                       help='Capture a new distortion event')
    parser.add_argument('--agent', type=str, help='Agent name')
    parser.add_argument('--model', type=str, help='Model name')
    parser.add_argument('--original', type=str, help='Original logic')
    parser.add_argument('--distorted', type=str, help='Distorted logic')
    parser.add_argument('--task', type=str, help='Task context')
    parser.add_argument('--test', type=str, help='Run tests on event ID')
    parser.add_argument('--evaluate', type=str, help='Evaluate event ID for graduation')
    parser.add_argument('--shadow', type=str, help='Run shadow thread for event ID')
    parser.add_argument('--status', action='store_true', help='Show sandbox status')
    parser.add_argument('--list', type=str, nargs='?', const=None, 
                       help='List events (optional status filter)')
    
    args = parser.parse_args()
    
    sandbox = DistortionSandbox()
    
    if args.status:
        status = sandbox.get_status()
        print(json.dumps(status, indent=2))
    
    elif args.capture:
        if not all([args.agent, args.model, args.original, args.distorted]):
            print("Error: --capture requires --agent, --model, --original, --distorted")
            return
        event_id = sandbox.capture_distortion(
            args.agent, args.model, args.original, args.distorted, 
            args.task or "Unknown task"
        )
        print(f"Event captured: {event_id}")
    
    elif args.test:
        results = sandbox.run_multimodal_testing(args.test)
        print(json.dumps(results, indent=2))
    
    elif args.evaluate:
        success, score = sandbox.evaluate_survivor_logic(args.evaluate)
        print(f"Graduated: {success}, Score: {score:.3f}")
    
    elif args.shadow:
        results = sandbox.run_shadow_thread(args.shadow, "production logic here")
        print(json.dumps(results, indent=2))
    
    elif args.list is not None:
        events = sandbox.list_events(args.list)
        print(json.dumps(events, indent=2))
    
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
