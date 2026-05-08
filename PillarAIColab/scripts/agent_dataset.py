#!/usr/bin/env python3
"""
Dataset preparation for neural agent routing.
Extracts training data from blackboard entries and WHT signals.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent))

import torch
from torch.utils.data import Dataset, DataLoader
import numpy as np
import json
import glob
import os
import re
from datetime import datetime

class AgentRoutingDataset(Dataset):
    """
    Dataset for training agent routing neural network.
    Uses WHT signals as input features and agent assignments as labels.
    """
    def __init__(self, signals_dir="Pillar_10_Memory/signals", blackboard_path="colab_blackboard/colab_blackboard.md"):
        self.samples = []
        self.agent_to_idx = {}
        self.idx_to_agent = {}
        
        # Define agent mapping (must match AGENT_ROLES keys)
        agent_names = [
            "quick_chat", "code_assistant", "general_assistant",
            "deep_reasoning", "thinking_agent", "project_manager",
            "experimental", "messenger"
        ]
        
        for idx, name in enumerate(agent_names):
            self.agent_to_idx[name] = idx
            self.idx_to_agent[idx] = name
        
        # Load data from blackboard + signals
        self._load_from_blackboard_and_signals(blackboard_path, signals_dir)
        
        print(f"Loaded {len(self.samples)} samples")
        if len(self.samples) == 0:
            print("[WARN] No training samples found. Check signals_dir and blackboard_path.")
    
    def _load_from_blackboard_and_signals(self, blackboard_path, signals_dir):
        """Load training data by parsing blackboard and generating WHT signals."""
        
        # Import here to avoid circular import
        from scripts.memory_translator import MemoryTranslator
        
        # Step1: Parse blackboard to get entries
        blackboard_entries = self._parse_blackboard(blackboard_path)
        print(f"Parsed {len(blackboard_entries)} blackboard entries")
        
        # Step2: Generate WHT signals for each entry and assign agent
        translator = MemoryTranslator()
        loaded = 0
        
        for entry in blackboard_entries:
            agent_id = entry["agent_id"]
            task = entry["task"]
            
            if not task or task.strip() == "":
                continue
            
            # Map agent_id to agent_name
            agent_name = self._map_agent_id_to_name(agent_id, task)
            
            if agent_name not in self.agent_to_idx:
                continue
            
            agent_idx = self.agent_to_idx[agent_name]
            
            # Generate WHT signal from task text
            try:
                signal, metadata = translator.translate_memory_entry(task)
                if signal is None:
                    continue
                
                # Get delta_h
                delta_h = min(entry.get("delta_h", 0.05), 1.0)
                
                # Store sample
                self.samples.append({
                    "signal": signal,
                    "agent_idx": agent_idx,
                    "delta_h": delta_h,
                    "task": task,
                    "original_length": len(task)
                })
                loaded += 1
            except Exception as e:
                print(f"[WARN] Failed to process entry: {e}")
                continue
        
        print(f"Loaded {loaded} samples with generated WHT signals")
    
    def _parse_blackboard(self, blackboard_path):
        """Parse blackboard markdown file to extract entries."""
        entries = []
        
        if not os.path.exists(blackboard_path):
            return entries
        
        with open(blackboard_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Split by entry marker
        entry_blocks = re.split(r'### \[([^\]]+)\]\s*(\S+)', content)
        
        # Reconstruct entries
        for i in range(1, len(entry_blocks), 3):
            if i + 1 >= len(entry_blocks):
                break
            
            timestamp = entry_blocks[i]
            agent_id = entry_blocks[i + 1]
            rest = entry_blocks[i + 2] if i + 2 < len(entry_blocks) else ""
            
            # Extract task and delta_h from rest
            task_match = re.search(r'\*\*Task\*\*:\s*(.+)', rest)
            delta_h_match = re.search(r'\*\*ΔH\*\*:\s*([\d.]+)', rest)
            
            entry = {
                "timestamp": timestamp,
                "agent_id": agent_id,
                "task": task_match.group(1).strip() if task_match else "",
                "delta_h": float(delta_h_match.group(1)) if delta_h_match else 0.05
            }
            entries.append(entry)
        
        return entries
    
    def _map_agent_id_to_name(self, agent_id, task=""):
        """Map blackboard agent_id to agent role name."""
        agent_id_lower = agent_id.lower()
        task_lower = task.lower()
        
        # Check notes field which contains model info like "Model: gemma4:latest"
        # This is the most reliable way to determine which agent handled the task
        
        # From task_delegator notes, we can see which model was used
        if "gemma" in agent_id_lower or "thinking" in task_lower or "step by step" in task_lower:
            return "thinking_agent"
        elif "qwen-coder" in agent_id_lower or "code" in task_lower or "binary search" in task_lower:
            return "code_assistant"
        elif "mistral" in agent_id_lower or "quantum" in task_lower or "what is" in task_lower:
            return "general_assistant"
        elif "qwen2.5-7b" in agent_id_lower or "deep" in task_lower or "optimize" in task_lower:
            return "deep_reasoning"
        elif "qwen3" in agent_id_lower or "project" in task_lower or "plan" in task_lower:
            return "project_manager"
        elif "llama" in agent_id_lower or "quick" in agent_id_lower or "2+2" in task_lower:
            return "quick_chat"
        elif "gpt-oss" in agent_id_lower or "experimental" in agent_id_lower:
            return "experimental"
        elif "messenger" in agent_id_lower:
            return "messenger"
        else:
            # Default based on task keywords
            if any(kw in task_lower for kw in ["code", "function", "programming", "debug"]):
                return "code_assistant"
            elif any(kw in task_lower for kw in ["think", "step by step", "solve", "calculate"]):
                return "thinking_agent"
            elif any(kw in task_lower for kw in ["project", "plan", "architecture", "design"]):
                return "project_manager"
            elif any(kw in task_lower for kw in ["analyze", "deep", "complex"]):
                return "deep_reasoning"
            elif any(kw in task_lower for kw in ["what is", "hello", "2+2", "simple"]):
                return "general_assistant"
            
            # Default
            return "general_assistant"
    
    def __len__(self):
        return len(self.samples)
    
    def __getitem__(self, idx):
        sample = self.samples[idx]
        
        # Get signal and ensure fixed size (512D)
        signal = sample["signal"]
        if len(signal) < 512:
            # Pad to 512
            padded = np.pad(signal, (0, 512 - len(signal)), 'constant')
        else:
            padded = signal[:512]
        
        return {
            "features": torch.tensor(padded, dtype=torch.float32),
            "label": torch.tensor(sample["agent_idx"], dtype=torch.long),
            "delta_h": torch.tensor(sample["delta_h"], dtype=torch.float32)
        }
    
    def get_agent_name(self, idx):
        """Get agent name from index."""
        return self.idx_to_agent.get(idx, "unknown")
    
    def get_class_distribution(self):
        """Get distribution of samples across agent classes."""
        dist = {}
        for sample in self.samples:
            agent_idx = sample["agent_idx"]
            agent_name = self.get_agent_name(agent_idx)
            dist[agent_name] = dist.get(agent_name, 0) + 1
        return dist


def create_dataloaders(dataset, batch_size=32, train_split=0.8, val_split=0.1):
    """
    Create train, validation, and test dataloaders.
    """
    dataset_size = len(dataset)
    train_size = int(train_split * dataset_size)
    val_size = int(val_split * dataset_size)
    test_size = dataset_size - train_size - val_size
    
    train_dataset, val_dataset, test_dataset = torch.utils.data.random_split(
        dataset, [train_size, val_size, test_size]
    )
    
    train_loader = DataLoader(train_dataset, batch_size=batch_size, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=batch_size, shuffle=False)
    test_loader = DataLoader(test_dataset, batch_size=batch_size, shuffle=False)
    
    return train_loader, val_loader, test_loader


if __name__ == "__main__":
    # Test dataset loading
    print("Loading Agent Routing Dataset...")
    dataset = AgentRoutingDataset()
    
    if len(dataset) > 0:
        print(f"\nDataset size: {len(dataset)}")
        print(f"Class distribution: {dataset.get_class_distribution()}")
        
        # Test loading a sample
        sample = dataset[0]
        print(f"\nSample 0:")
        print(f"  Features shape: {sample['features'].shape}")
        print(f"  Label: {sample['label'].item()} ({dataset.get_agent_name(sample['label'].item())})")
        print(f"  Delta H: {sample['delta_h'].item():.4f}")
        
        # Test dataloaders
        train_loader, val_loader, test_loader = create_dataloaders(dataset)
        print(f"\nDataLoaders:")
        print(f"  Train: {len(train_loader.dataset)} samples")
        print(f"  Val: {len(val_loader.dataset)} samples")
        print(f"  Test: {len(test_loader.dataset)} samples")
    else:
        print("No samples loaded. Check Pillar_10_Memory/signals/ directory.")
