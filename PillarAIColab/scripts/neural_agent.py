#!/usr/bin/env python3
"""
Custom Neural Agent that learns from other agents via reinforcement learning.
Uses Deep Q-Learning with experience replay.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent))

import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np
import random
from collections import deque
from typing import Dict, List, Tuple, Optional

class NeuralAgentNetwork(nn.Module):
    """
    Q-Network for the neural agent.
    Input: WHT signal (512D)
    Output: Q-values for each possible action (which agent to route to)
    """
    def __init__(self, input_size=512, hidden_sizes=[256, 128], num_actions=8):
        super().__init__()
        
        layers = []
        prev_size = input_size
        
        for hidden in hidden_sizes:
            layers.extend([
                nn.Linear(prev_size, hidden),
                nn.ReLU(),
                nn.Dropout(0.2)
            ])
            prev_size = hidden
        
        layers.append(nn.Linear(prev_size, num_actions))
        
        self.network = nn.Sequential(*layers)
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.to(self.device)
    
    def forward(self, x):
        return self.network(x.to(self.device))


class NeuralAgent:
    """
    Custom agent that learns from other agents via neural networking.
    Uses reinforcement learning (Deep Q-Learning) to optimize routing decisions.
    """
    def __init__(self, input_size=512, hidden_sizes=[256, 128], num_actions=8):
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        
        # Q-Network and target network
        self.q_network = NeuralAgentNetwork(
            input_size=input_size,
            hidden_sizes=hidden_sizes,
            num_actions=num_actions
        )
        self.target_network = NeuralAgentNetwork(
            input_size=input_size,
            hidden_sizes=hidden_sizes,
            num_actions=num_actions
        )
        self.target_network.load_state_dict(self.q_network.state_dict())
        
        # Optimizer
        self.optimizer = optim.Adam(self.q_network.parameters(), lr=0.001)
        self.criterion = nn.MSELoss()
        
        # Experience replay buffer
        self.experience_buffer = deque(maxlen=10000)
        
        # RL hyperparameters
        self.epsilon = 1.0  # Exploration rate
        self.epsilon_decay = 0.995
        self.epsilon_min = 0.01
        self.gamma = 0.95  # Discount factor
        self.target_update_freq = 100  # Update target network every N steps
        
        # Tracking
        self.training_steps = 0
        self.total_rewards = 0.0
        self.episode_rewards = []
        
        # Agent name mapping
        self.idx_to_agent = {}
        self.agent_names = [
            "quick_chat", "code_assistant", "general_assistant",
            "deep_reasoning", "thinking_agent", "project_manager",
            "experimental", "messenger"
        ]
        for idx, name in enumerate(self.agent_names):
            self.idx_to_agent[idx] = name
    
    def _calculate_reward(self, delta_h: float, success: bool, agent_name: str) -> float:
        """
        Calculate reward based on agent performance.
        Lower ΔH = better → higher reward.
        """
        # Base reward from delta_h (ΔH=0 → reward=1.0, ΔH=0.1 → reward=0.9)
        reward = 1.0 - delta_h
        
        # Bonus for success
        if success:
            reward += 0.5
        
        # Penalty for high ΔH
        if delta_h > 0.07:  # Harm threshold
            reward -= 1.0
        
        return reward
    
    def observe(self, task_signal: np.ndarray, selected_agent_idx: int, 
                delta_h: float, success: bool, agent_name: str):
        """
        Store experience from observing other agents.
        
        Args:
            task_signal: WHT signal (512D)
            selected_agent_idx: Which agent was chosen (index)
            delta_h: Harm metric
            success: Whether task was successful
            agent_name: Name of the agent that handled the task
        """
        reward = self._calculate_reward(delta_h, success, agent_name)
        
        # Store experience
        self.experience_buffer.append({
            "state": task_signal.copy(),
            "action": selected_agent_idx,
            "reward": reward,
            "next_state": task_signal.copy(),  # Simplified (same state for now)
            "done": False
        })
        
        self.total_rewards += reward
        
        # Periodic training
        if len(self.experience_buffer) >= 32:
            self.train_on_experiences()
    
    def select_action(self, task_signal: np.ndarray, explore: bool = True) -> Tuple[int, str]:
        """
        Select agent to route to using epsilon-greedy policy.
        Returns (agent_index, agent_name).
        """
        if explore and random.random() < self.epsilon:
            # Explore: random agent
            agent_idx = random.randint(0, len(self.agent_names) - 1)
        else:
            # Exploit: best predicted agent
            with torch.no_grad():
                state = torch.tensor(task_signal, dtype=torch.float32).unsqueeze(0).to(self.device)
                q_values = self.q_network(state)
                agent_idx = q_values.argmax().item()
        
        agent_name = self.idx_to_agent.get(agent_idx, "general_assistant")
        return agent_idx, agent_name
    
    def train_on_experiences(self, batch_size: int = 64) -> Optional[float]:
        """Train Q-network on collected experiences."""
        if len(self.experience_buffer) < batch_size:
            return None
        
        # Sample batch
        batch = random.sample(self.experience_buffer, batch_size)
        
        # Convert to numpy arrays first (faster than list comprehension)
        states = torch.tensor(np.array([e["state"] for e in batch]), dtype=torch.float32).to(self.device)
        actions = torch.tensor([e["action"] for e in batch], dtype=torch.long).to(self.device)
        rewards = torch.tensor([e["reward"] for e in batch], dtype=torch.float32).to(self.device)
        next_states = torch.tensor(np.array([e["next_state"] for e in batch]), dtype=torch.float32).to(self.device)
        dones = torch.tensor([e["done"] for e in batch], dtype=torch.bool).to(self.device)
        
        # Current Q-values
        current_q = self.q_network(states).gather(1, actions.unsqueeze(1))
        
        # Target Q-values (Bellman equation)
        with torch.no_grad():
            next_q = self.target_network(next_states).max(1)[0]
            target_q = rewards + self.gamma * next_q * ~dones
        
        # Compute loss
        loss = self.criterion(current_q.squeeze(), target_q)
        
        # Backprop
        self.optimizer.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(self.q_network.parameters(), max_norm=1.0)
        self.optimizer.step()
        
        # Decay epsilon
        self.epsilon = max(self.epsilon_min, self.epsilon * self.epsilon_decay)
        
        # Update target network
        self.training_steps += 1
        if self.training_steps % self.target_update_freq == 0:
            self.target_network.load_state_dict(self.q_network.state_dict())
            print(f"[Neural Agent] Target network updated at step {self.training_steps}")
        
        return loss.item()
    
    def save_model(self, path: str):
        """Save Q-network state."""
        import os
        os.makedirs(os.path.dirname(path), exist_ok=True)
        torch.save({
            'q_network_state_dict': self.q_network.state_dict(),
            'target_network_state_dict': self.target_network.state_dict(),
            'optimizer_state_dict': self.optimizer.state_dict(),
            'epsilon': self.epsilon,
            'training_steps': self.training_steps,
            'total_rewards': self.total_rewards
        }, path)
        print(f"[Neural Agent] Model saved to {path}")
    
    def load_model(self, path: str):
        """Load Q-network state."""
        if os.path.exists(path):
            checkpoint = torch.load(path, map_location=self.device)
            self.q_network.load_state_dict(checkpoint['q_network_state_dict'])
            self.target_network.load_state_dict(checkpoint['target_network_state_dict'])
            self.optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
            self.epsilon = checkpoint.get('epsilon', 0.01)
            self.training_steps = checkpoint.get('training_steps', 0)
            self.total_rewards = checkpoint.get('total_rewards', 0.0)
            print(f"[Neural Agent] Model loaded from {path}")
            return True
        return False
    
    def get_stats(self) -> Dict:
        """Get learning statistics."""
        return {
            "epsilon": self.epsilon,
            "training_steps": self.training_steps,
            "buffer_size": len(self.experience_buffer),
            "total_rewards": self.total_rewards,
            "avg_reward": self.total_rewards / max(self.training_steps, 1)
        }


if __name__ == "__main__":
    # Test neural agent
    print("Testing Neural Agent...")
    
    agent = NeuralAgent()
    print(f"Device: {agent.device}")
    print(f"Q-Network parameters: {sum(p.numel() for p in agent.q_network.parameters())}")
    
    # Simulate some experiences
    print("\nSimulating experiences...")
    for i in range(100):
        # Fake WHT signal
        signal = np.random.randn(512).astype(np.float32)
        agent_idx = random.randint(0, 7)
        delta_h = random.uniform(0, 0.1)
        success = delta_h < 0.05
        
        agent.observe(signal, agent_idx, delta_h, success, agent.idx_to_agent[agent_idx])
    
    print(f"\nStats: {agent.get_stats()}")
    
    # Test action selection
    test_signal = np.random.randn(512).astype(np.float32)
    agent_idx, agent_name = agent.select_action(test_signal, explore=False)
    print(f"\nTest action selection:")
    print(f"  Selected agent: {agent_name} (idx={agent_idx})")
    
    # Save model
    agent.save_model("models/neural_agent.pth")
    print("\nNeural agent test complete!")
