#!/usr/bin/env python3
"""
WHT Protocol Manager (Singleton)
- Manages WHT (Walsh-Hadamard Transform) 32D signals
- Integrates with NeuralAgent (512D input)
- Intercept mode: Auto-route user prompts to best agent
- CLI mode: Explicit WHT messaging to agents
- Reuses memory_translator.py for text ↔ WHT translation
"""

import sys
from pathlib import Path
from typing import Dict, Optional, Tuple
import numpy as np

sys.path.insert(0, str(Path(__file__).parent.parent))

try:
    from scripts.memory_translator import MemoryTranslator
except ImportError:
    MemoryTranslator = None
    print("[WHTManager] memory_translator not available")

try:
    from scripts.neural_agent import NeuralAgent
except ImportError:
    NeuralAgent = None
    print("[WHTManager] neural_agent not available")

try:
    from scripts.agent_roles import AGENT_ROLES, get_model_for_role
except ImportError:
    AGENT_ROLES = {}
    get_model_for_role = None


class WHTProtocolManager:
    """
    Singleton WHT Protocol Manager.
    Handles 32D WHT signals and 512D NeuralAgent integration.
    """
    
    _instance = None  # Singleton instance
    
    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance
    
    def __init__(self, wht_n: int = 32, neural_input: int = 512):
        # Prevent re-initialization on singleton pattern
        if hasattr(self, '_initialized') and self._initialized:
            return
        
        self.wht_n = wht_n  # 32D WHT signals
        self.neural_input = neural_input  # 512D NeuralAgent input
        
        # Initialize translator
        if MemoryTranslator:
            self.translator = MemoryTranslator()
        else:
            self.translator = None
            print("[WHTManager] MemoryTranslator not available")
        
        # Initialize NeuralAgent
        self.neural_agent = None
        self._init_neural_agent()
        
        # Agent name mapping (for 32D ↔ 512D translation)
        self.agent_names = list(AGENT_ROLES.keys()) if AGENT_ROLES else []
        
        # Message log (for terminal display)
        self.message_log = []
        
        self._initialized = True
        print(f"[WHTManager] Initialized (WHT_N={wht_n}, NeuralInput={neural_input})")
    
    def _init_neural_agent(self):
        """Initialize NeuralAgent if available."""
        if NeuralAgent:
            try:
                self.neural_agent = NeuralAgent()
                print("[WHTManager] NeuralAgent loaded (512D Q-Network)")
            except Exception as e:
                print(f"[WHTManager] NeuralAgent init failed: {e}")
                self.neural_agent = None
        else:
            print("[WHTManager] NeuralAgent class not available")
    
    def encode_text_to_wht(self, text: str) -> Tuple[np.ndarray, Dict]:
        """
        Encode text to 32D WHT signal.
        
        Args:
            text: Input text
            
        Returns:
            (wht_signal_32d, metadata)
        """
        if self.translator is None:
            print("[WHTManager] No translator available")
            return np.zeros(self.wht_n, dtype=np.float32), {}
        
        try:
            signal = self.translator.encode_text_to_signal(text)
            transformed = self.translator._fwht(signal)
            
            # Truncate/pad to WHT_N (32D)
            if len(transformed) >= self.wht_n:
                wht_32d = transformed[:self.wht_n]
            else:
                wht_32d = np.pad(transformed, ((0, self.wht_n - len(transformed)),))
            
            metadata = {
                "original_length": len(text),
                "signal_length": len(signal),
                "wht_coefficients": self.wht_n
            }
            
            return wht_32d, metadata
            
        except Exception as e:
            print(f"[WHTManager] WHT encoding error: {e}")
            return np.zeros(self.wht_n, dtype=np.float32), {}
    
    def decode_wht_to_text(self, wht_signal: np.ndarray) -> str:
        """
        Approximate text reconstruction from WHT signal.
        Note: Lossy due to thresholding.
        """
        if self.translator is None:
            return "[No translator available]"
        
        try:
            text = self.translator.reconstruct_text_from_signal(wht_signal, self.wht_n)
            return text
        except Exception as e:
            return f"[Decode error: {e}]"
    
    def _expand_32_to_512(self, wht_32d: np.ndarray) -> np.ndarray:
        """
        Expand 32D WHT signal to 512D for NeuralAgent.
        
        Args:
            wht_32d: 32D WHT signal
            
        Returns:
            512D signal (zeros padded)
        """
        result = np.zeros(self.neural_input, dtype=np.float32)
        result[:self.wht_n] = wht_32d
        return result
    
    def _compress_512_to_32(self, signal_512d: np.ndarray) -> np.ndarray:
        """
        Compress 512D NeuralAgent signal to 32D WHT.
        
        Args:
            signal_512d: 512D signal from NeuralAgent
            
        Returns:
            32D WHT signal
        """
        return signal_512d[:self.wht_n].copy()
    
    def intercept_prompt(self, user_prompt: str) -> Dict:
        """
        Intercept mode: Auto-route user prompt to best agent via NeuralAgent.
        
        Args:
            user_prompt: User's input text
            
        Returns:
            Dict with selected agent info
        """
        print(f"\n[WHTManager] Intercepting prompt: '{user_prompt[:50]}...'")
        
        # Encode to WHT 32D (direct input to NeuralAgent)
        wht_signal, metadata = self.encode_text_to_wht(user_prompt)
        
        # Route via NeuralAgent using 32D WHT signal directly
        if self.neural_agent is not None:
            neural_input = self._expand_32_to_512(wht_signal)
            agent_idx, agent_name = self.neural_agent.select_action(neural_input, explore=False)
            routing_method = "neural_agent"
        else:
            # Fallback: rule-based routing to Dev Team agents
            agent_name = "dev_lead"  # Default to Dev Lead
            routing_method = "rule_based"
            prompt_lower = user_prompt.lower()
            
            # Greetings -> quick_chat (llama3.2:1b for fast responses)
            if any(g in prompt_lower for g in ["hi", "hello", "hey", "greetings"]):
                agent_name = "quick_chat"
            elif "code" in prompt_lower or "programming" in prompt_lower:
                agent_name = "code_reviewer"
            elif "think" in prompt_lower or "step" in prompt_lower:
                agent_name = "system_architect"
            elif "project" in prompt_lower or "plan" in prompt_lower:
                agent_name = "dev_lead"
            elif "research" in prompt_lower or "market" in prompt_lower:
                agent_name = "market_researcher"
            elif "test" in prompt_lower or "aggressive" in prompt_lower:
                agent_name = "aggressor"
        
        # Validate model exists for selected agent
        model = get_model_for_role(agent_name) if get_model_for_role else None
        if model is None:
            # Fallback to dev_lead (qwen3:30b-a3b) if original agent has no model
            print(f"[WHTManager] Warning: {agent_name} has no model, falling back to dev_lead")
            agent_name = "dev_lead"
            model = get_model_for_role("dev_lead") if get_model_for_role else "qwen3:30b-a3b"
        
        result = {
            "mode": "intercept",
            "selected_agent": agent_name,
            "model": model,
            "routing_method": routing_method,
            "wht_metadata": metadata
        }
        
        print(f"[WHTManager] Routed to: {agent_name} (model: {model})")
        
        # Log message
        self.message_log.append({
            "direction": "user→agent",
            "prompt": user_prompt,
            "agent": agent_name,
            "wht_signal": wht_signal.tolist()
        })
        
        return result
    
    def cli_send_message(self, agent_name: str, message: str, use_wht: bool = True) -> Dict:
        """
        CLI mode: Explicitly send message to agent via WHT.
        
        Args:
            agent_name: Target agent name
            message: Message text
            use_wht: Whether to encode as WHT signal
            
        Returns:
            Dict with send status
        """
        print(f"\n[WHTManager] CLI send to {agent_name}: '{message[:50]}...'")
        
        if use_wht and self.translator:
            signal, metadata = self.encode_text_to_wht(message)
            transformed = self.translator._fwht(signal)
            
            # Log
            self.message_log.append({
                "direction": f"user→{agent_name}",
                "message": message,
                "wht_signal": signal.tolist(),
                "mode": "cli_wht"
            })
            
            result = {
                "mode": "cli_wht",
                "target_agent": agent_name,
                "message": message,
                "wht_encoded": True,
                "wht_metadata": metadata
            }
            print(f"[WHTManager] Message sent via WHT (32D signal)")
        else:
            self.message_log.append({
                "direction": f"user→{agent_name}",
                "message": message,
                "mode": "cli_plain"
            })
            
            result = {
                "mode": "cli_plain",
                "target_agent": agent_name,
                "message": message,
                "wht_encoded": False
            }
            print(f"[WHTManager] Message sent (plain text)")
        
        return result
    
    def get_agent_for_wht(self, wht_signal: np.ndarray) -> Tuple[str, str]:
        """
        Use NeuralAgent to select best agent for given WHT signal.
        Takes 32D WHT signal directly (matching WHT Protocol).
        
        Args:
            wht_signal: 32D WHT signal
            
        Returns:
            (agent_name, model_name)
        """
        if self.neural_agent:
            neural_input = self._expand_32_to_512(wht_signal)
            _, agent_name = self.neural_agent.select_action(neural_input, explore=False)
            model = get_model_for_role(agent_name) if get_model_for_role else None
            if model is None:
                agent_name = "dev_lead"
                model = get_model_for_role("dev_lead") if get_model_for_role else "qwen3:30b-a3b"
            return agent_name, model
        else:
            return "dev_lead", "qwen3:30b-a3b"
    
    def get_message_log(self) -> list:
        """Get all logged messages."""
        return self.message_log.copy()
    
    def get_status(self) -> Dict:
        """Get WHTProtocolManager status."""
        return {
            "singleton": True,
            "wht_n": self.wht_n,
            "neural_input": self.neural_input,
            "neural_agent_available": self.neural_agent is not None,
            "translator_available": self.translator is not None,
            "message_count": len(self.message_log),
            "agent_count": len(self.agent_names)
        }


if __name__ == "__main__":
    print("Testing WHTProtocolManager...\n")
    
    manager = WHTProtocolManager()
    
    # Test status
    status = manager.get_status()
    print(f"Status: {status}\n")
    
    # Test intercept mode
    result = manager.intercept_prompt("What is 2+2?")
    print(f"Intercept result: {result}\n")
    
    # Test CLI mode
    result = manager.cli_send_message("code_assistant", "Review this function")
    print(f"CLI result: {result}\n")
    
    # Test WHT encoding/decoding
    signal, meta = manager.encode_text_to_wht("Hello WHT")
    print(f"WHT Signal (32D): {signal[:5]}... (showing first 5)")
    print(f"Metadata: {meta}")
    
    print("\n[WHTManager] Test complete!")
