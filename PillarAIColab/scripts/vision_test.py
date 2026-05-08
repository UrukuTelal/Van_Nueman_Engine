#!/usr/bin/env python3
"""
Vision test using qwen2.5:7b model.
Tests visual understanding by analyzing creature renders or screenshots.
"""
import sys
from pathlib import Path
import os
import subprocess
import time

def check_vision_model():
    """Check if any vision-capable model is available."""
    result = subprocess.run(
        ["ollama", "list"],
        capture_output=True,
        text=True
    )
    
    models = result.stdout.lower()
    vision_keywords = ["v", "vision", "llava", "bakhshali", "phi3-v"]
    
    for kw in vision_keywords:
        if kw in models:
            print(f"[Vision] Found vision model containing: {kw}")
            return True
    
    print("[Vision] No dedicated vision model found")
    print("[Vision] qwen2.5:7b has some multimodal capabilities")
    return False

def query_vision_model(image_path: str, prompt: str, model: str = "qwen2.5:7b"):
    """
    Query a model with an image.
    Note: qwen2.5:7b has limited vision - may need base64 encoding.
    """
    if not os.path.exists(image_path):
        print(f"[Error] Image not found: {image_path}")
        return None
    
    print(f"[Vision] Querying {model} with image: {image_path}")
    print(f"[Vision] Prompt: {prompt}")
    
    try:
        # Ollama with image (if model supports it)
        result = subprocess.run(
            [
                "ollama", "run", model,
                prompt,
                "--image", image_path
            ],
            capture_output=True,
            text=True,
            timeout=60,
            encoding='utf-8',  # Fix encoding issue
            errors='ignore'  # Ignore decode errors
        )
        
        if result.returncode == 0:
            return result.stdout.strip()
        else:
            print(f"[Vision] Error: {result.stderr}")
            return None
            
    except subprocess.TimeoutExpired:
        print("[Vision] Timeout - model took too long")
        return None
    except Exception as e:
        print(f"[Vision] Exception: {e}")
        return None

def create_test_image():
    """Create a simple test image (simulated creature render)."""
    try:
        import numpy as np
        from PIL import Image, ImageDraw
        
        # Create a simple 512x512 RGBA image
        img = Image.new('RGBA', (512, 512), (0, 0, 0, 255))
        draw = ImageDraw.Draw(img)
        
        # Draw a simple "creature" (circle with limbs)
        # Body
        draw.ellipse([200, 200, 312, 312], fill=(255, 100, 100, 255))
        # Head
        draw.ellipse([240, 150, 272, 182], fill=(255, 120, 100, 255))
        # Limbs
        draw.line([250, 312, 200, 400], fill=(200, 80, 80, 255), width=10)
        draw.line([262, 312, 312, 400], fill=(200, 80, 80, 255), width=10)
        draw.line([230, 220, 150, 180], fill=(200, 80, 80, 255), width=8)
        draw.line([282, 220, 362, 180], fill=(200, 80, 80, 255), width=8)
        # Tail
        draw.line([312, 256, 400, 300], fill=(200, 80, 80, 255), width=8)
        
        # Save
        output_path = "assets/creature_test_render.png"
        Path("assets").mkdir(exist_ok=True)
        img.save(output_path)
        
        print(f"[Image] Created test image: {output_path}")
        return output_path
        
    except ImportError:
        print("[Image] PIL not available, skipping image creation")
        return None

def analyze_creature_structure(creature_json: str):
    """Analyze creature JSON using text model (since vision models unavailable)."""
    try:
        import json
        
        with open(creature_json, 'r') as f:
            creature = json.load(f)
        
        print(f"\n{'=' * 60}")
        print(f"Creature Analysis: {creature['name']}")
        print(f"{'=' * 60}")
        
        # Analyze pillar vector
        pillars = creature.get('pillars', {})
        print(f"\nPillar Analysis:")
        
        # High force -> strong creature
        force = pillars.get('Force', 0)
        if force > 0.7:
            print(f"  [HIGH] High Force ({force:.1f}) -> Strong attacker")
        elif force > 0.4:
            print(f"  [MED] Moderate Force ({force:.1f}) -> Balanced")
        else:
            print(f"  [LOW] Low Force ({force:.1f}) -> Weak")
        
        # High awareness -> good perception
        awareness = pillars.get('Awareness', 0)
        if awareness > 0.7:
            print(f"  [HIGH] High Awareness ({awareness:.1f}) -> Good senses")
        else:
            print(f"  [MED] Moderate Awareness ({awareness:.1f})")
        
        # Skelly analysis
        skelly = creature.get('skelly_config', {})
        if skelly:
            bones = skelly.get('bones', [])
            muscles = skelly.get('muscles', [])
            organs = skelly.get('organs', [])
            
            print(f"\nSkelly Structure:")
            print(f"  Bones: {len(bones)} (mobility)")
            print(f"  Muscles: {len(muscles)} (strength/agility)")
            print(f"  Organs: {len(organs)} (life support)")
            
            scale = skelly.get('scale', 1.0)
            print(f"  Scale: {scale:.1f}x (size multiplier)")
        
        print(f"\n{'=' * 60}")
        return True
        
    except Exception as e:
        print(f"[Error] Failed to analyze creature: {e}")
        return False

if __name__ == "__main__":
    print("=" * 60)
    print("Vision Test Suite (PillarAIColab)")
    print("=" * 60)
    
    # Check for vision models
    has_vision = check_vision_model()
    
    # Create test image
    image_path = create_test_image()
    
    if image_path and has_vision:
        # Try vision query
        prompt = "Describe this creature render. What do you see?"
        response = query_vision_model(image_path, prompt)
        if response:
            print(f"\n[Vision Response]\n{response}\n")
    
    # Analyze creature JSON (works without vision model)
    creature_path = "assets/creature_predator.json"
    if os.path.exists(creature_path):
        analyze_creature_structure(creature_path)
    else:
        print(f"\n[Warn] Creature JSON not found: {creature_path}")
    
    # Try to pull a vision model if none exists
    if not has_vision:
        print("\n[Vision] Attempting to pull qwen2.5-vl:7b...")
        result = subprocess.run(
            ["ollama", "pull", "qwen2.5:7b"],
            capture_output=True,
            text=True,
            timeout=300
        )
        if result.returncode == 0:
            print("[Vision] Successfully pulled qwen2.5:7b")
        else:
            print(f"[Vision] Pull failed: {result.stderr}")
    
    print(f"\n{'=' * 60}")
    print("Vision test complete!")
    print(f"{'=' * 60}")
