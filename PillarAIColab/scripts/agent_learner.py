#!/usr/bin/env python3
"""
Agent Routing Neural Network.
Learns to route tasks to optimal agents based on WHT signals.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent))

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader
import numpy as np
from typing import Dict, List, Tuple
import os

class AgentRoutingNetwork(nn.Module):
    """
    Neural network for optimal agent routing.
    Input: WHT signal (512D)
    Output: Probability distribution over 8 agents
    """
    def __init__(self, input_size=512, hidden_sizes=[256, 128, 64], num_agents=8, dropout_rate=0.2):
        super().__init__()
        
        layers = []
        prev_size = input_size
        
        # Build hidden layers
        for hidden in hidden_sizes:
            layers.extend([
                nn.Linear(prev_size, hidden),
                nn.ReLU(),
                nn.Dropout(dropout_rate)
            ])
            prev_size = hidden
        
        # Output layer (logits for each agent)
        layers.append(nn.Linear(prev_size, num_agents))
        
        self.network = nn.Sequential(*layers)
        self.softmax = nn.Softmax(dim=-1)
        
        # Move to GPU if available
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.to(self.device)
    
    def forward(self, x):
        """Forward pass. Returns raw logits."""
        x = x.to(self.device)
        return self.network(x)
    
    def predict(self, x):
        """Return probability distribution over agents."""
        with torch.no_grad():
            logits = self.forward(x)
            return self.softmax(logits)
    
    def get_best_agent(self, x):
        """Return agent index with highest probability."""
        probs = self.predict(x)
        return torch.argmax(probs, dim=-1)
    
    def get_agent_confidence(self, x):
        """Return confidence (probability) for best agent."""
        probs = self.predict(x)
        max_probs, _ = torch.max(probs, dim=-1)
        return max_probs


class RoutingTrainer:
    """Trainer for AgentRoutingNetwork."""
    
    def __init__(self, model: AgentRoutingNetwork, learning_rate=0.001):
        self.model = model
        self.criterion = nn.CrossEntropyLoss()
        self.optimizer = optim.Adam(model.parameters(), lr=learning_rate)
        self.train_losses = []
        self.val_accuracies = []
    
    def train_epoch(self, dataloader: DataLoader, epoch: int) -> float:
        """Train for one epoch. Returns average loss."""
        self.model.train()
        total_loss = 0.0
        correct = 0
        total = 0
        device = self.model.device
        
        for batch_idx, batch in enumerate(dataloader):
            features = batch["features"].to(device)
            labels = batch["label"].to(device)
            
            # Forward pass
            logits = self.model(features)
            loss = self.criterion(logits, labels)
            
            # Backward pass
            self.optimizer.zero_grad()
            loss.backward()
            self.optimizer.step()
            
            # Statistics
            total_loss += loss.item()
            _, predicted = torch.max(logits, 1)
            total += labels.size(0)
            correct += (predicted == labels).sum().item()
        
        avg_loss = total_loss / len(dataloader)
        accuracy = 100 * correct / total
        
        self.train_losses.append(avg_loss)
        print(f"Epoch {epoch+1}: Loss={avg_loss:.4f}, Accuracy={accuracy:.2f}%")
        
        return avg_loss
    
    def validate(self, dataloader: DataLoader) -> Tuple[float, float]:
        """Validate model. Returns (accuracy, avg_loss)."""
        self.model.eval()
        total_loss = 0.0
        correct = 0
        total = 0
        device = self.model.device
        
        with torch.no_grad():
            for batch in dataloader:
                features = batch["features"].to(device)
                labels = batch["label"].to(device)
                
                logits = self.model(features)
                loss = self.criterion(logits, labels)
                
                total_loss += loss.item()
                _, predicted = torch.max(logits, 1)
                total += labels.size(0)
                correct += (predicted == labels).sum().item()
        
        avg_loss = total_loss / len(dataloader)
        accuracy = 100 * correct / total
        
        self.val_accuracies.append(accuracy)
        print(f"Validation: Loss={avg_loss:.4f}, Accuracy={accuracy:.2f}%")
        
        return accuracy, avg_loss
    
    def save_model(self, path: str):
        """Save model state dict."""
        os.makedirs(os.path.dirname(path), exist_ok=True)
        torch.save({
            'model_state_dict': self.model.state_dict(),
            'optimizer_state_dict': self.optimizer.state_dict(),
            'train_losses': self.train_losses,
            'val_accuracies': self.val_accuracies
        }, path)
        print(f"Model saved to {path}")
    
    def load_model(self, path: str):
        """Load model state dict."""
        if os.path.exists(path):
            checkpoint = torch.load(path, map_location=self.model.device)
            self.model.load_state_dict(checkpoint['model_state_dict'])
            self.optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
            self.train_losses = checkpoint.get('train_losses', [])
            self.val_accuracies = checkpoint.get('val_accuracies', [])
            print(f"Model loaded from {path}")
            return True
        return False


def train_routing_network(
    model_path="models/routing_network.pth",
    signals_dir="Pillar_10_Memory/signals",
    epochs=100,
    batch_size=32,
    learning_rate=0.001
):
    """
    Full training pipeline for agent routing network.
    """
    from scripts.agent_dataset import AgentRoutingDataset, create_dataloaders
    
    # Load dataset
    print("Loading dataset...")
    dataset = AgentRoutingDataset(signals_dir=signals_dir)
    
    if len(dataset) == 0:
        print("[ERROR] No training data found. Run memory_translator.py first.")
        return None
    
    print(f"Dataset size: {len(dataset)}")
    print(f"Class distribution: {dataset.get_class_distribution()}")
    
    # Create dataloaders
    train_loader, val_loader, test_loader = create_dataloaders(
        dataset, batch_size=batch_size
    )
    
    print(f"\nDataLoaders:")
    print(f"  Train: {len(train_loader.dataset)} samples")
    print(f"  Val: {len(val_loader.dataset)} samples")
    print(f"  Test: {len(test_loader.dataset)} samples")
    
    # Initialize model
    print(f"\nInitializing AgentRoutingNetwork...")
    model = AgentRoutingNetwork(input_size=512, num_agents=8)
    print(f"Model device: {model.device}")
    print(f"Model parameters: {sum(p.numel() for p in model.parameters())}")
    
    # Trainer
    trainer = RoutingTrainer(model, learning_rate=learning_rate)
    
    # Load pre-trained if exists
    if os.path.exists(model_path):
        trainer.load_model(model_path)
    
    # Training loop
    print(f"\n=== Training for {epochs} epochs ===")
    best_val_accuracy = 0.0
    
    for epoch in range(epochs):
        # Train
        train_loss = trainer.train_epoch(train_loader, epoch)
        
        # Validate
        val_accuracy, val_loss = trainer.validate(val_loader)
        
        # Save best model
        if val_accuracy > best_val_accuracy:
            best_val_accuracy = val_accuracy
            trainer.save_model(model_path)
        
        print()
    
    # Final test
    print("=== Final Test ===")
    test_accuracy, test_loss = trainer.validate(test_loader)
    print(f"Test Accuracy: {test_accuracy:.2f}%")
    
    # Save final model
    trainer.save_model(model_path)
    
    return model


def predict_agent_for_task(
    task: str,
    model_path="models/routing_network.pth",
    confidence_threshold=0.7
) -> Tuple[str, float]:
    """
    Use trained model to predict best agent for a task.
    Returns (agent_name, confidence).
    """
    from scripts.agent_dataset import AgentRoutingDataset
    from scripts.memory_translator import MemoryTranslator
    
    # Load model
    if not os.path.exists(model_path):
        print(f"[ERROR] Model not found: {model_path}")
        return None, 0.0
    
    model = AgentRoutingNetwork(input_size=512, num_agents=8)
    checkpoint = torch.load(model_path, map_location=model.device)
    model.load_state_dict(checkpoint['model_state_dict'])
    model.eval()
    
    # Convert task to WHT signal
    translator = MemoryTranslator()
    signal, _ = translator.translate_memory_entry(task)
    
    if signal is None:
        print("[ERROR] Failed to translate task to signal")
        return None, 0.0
    
    # Ensure 512D
    if len(signal) < 512:
        signal = np.pad(signal, (0, 512 - len(signal)))
    else:
        signal = signal[:512]
    
    # Predict
    signal_tensor = torch.tensor(signal, dtype=torch.float32).unsqueeze(0).to(model.device)
    agent_idx = model.get_best_agent(signal_tensor).item()
    confidence = model.get_agent_confidence(signal_tensor).item()
    
    # Map to agent name
    dataset = AgentRoutingDataset()
    agent_name = dataset.get_agent_name(agent_idx)
    
    return agent_name, confidence


if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Train Agent Routing Network")
    parser.add_argument("--epochs", type=int, default=100, help="Number of training epochs")
    parser.add_argument("--batch-size", type=int, default=32, help="Batch size")
    parser.add_argument("--lr", type=float, default=0.001, help="Learning rate")
    parser.add_argument("--model-path", type=str, default="models/routing_network.pth", help="Model save path")
    parser.add_argument("--signals-dir", type=str, default="Pillar_10_Memory/signals", help="WHT signals directory")
    
    args = parser.parse_args()
    
    # Train
    model = train_routing_network(
        model_path=args.model_path,
        signals_dir=args.signals_dir,
        epochs=args.epochs,
        batch_size=args.batch_size,
        learning_rate=args.lr
    )
    
    # Test prediction
    if model is not None:
        print("\n=== Test Predictions ===")
        test_tasks = [
            "Write a binary search function",
            "What is 2+2?",
            "Create a project plan for auth system",
            "Think step by step: optimize quicksort"
        ]
        
        for task in test_tasks:
            agent, confidence = predict_agent_for_task(task, model_path=args.model_path)
            if agent:
                print(f"Task: {task}")
                print(f"  Predicted Agent: {agent}")
                print(f"  Confidence: {confidence:.2%}")
                print()
