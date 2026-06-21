#!/usr/bin/env python3
"""
Memory Translator: Project Memory → WHT Signals → Storage
Converts text-based project memory (blackboard, RAG store) into Walsh-Hadamard Transform signals,
applies threshold filtering based on pillar values, and stores the results.

Supports fp20_t (20-bit fixed-point) WHT encoding/decoding for deterministic
agent communication compatible with the C++ vn::fp20_t type.
"""
import numpy as np
from pathlib import Path
from typing import Dict, List, Tuple, Optional, Union
import json
from datetime import datetime, timezone
import re
import struct

class MemoryTranslator:
    """Translate project memory to WHT signals with threshold filtering."""
    
    def __init__(self, memory_dir: str = "colab_blackboard", signal_dir: str = "Pillar_10_Memory/signals"):
        self.memory_dir = Path(memory_dir)
        self.signal_dir = Path(signal_dir)
        self.signal_dir.mkdir(parents=True, exist_ok=True)
        
        # Load memory config for thresholds
        self.memory_config = self._load_memory_config()
        
        # Thresholds from pillar configs and memory config
        self.thresholds = {
            "memory_min": 0.1,  # Avoid Amnesia (Memory < 0.1)
            "memory_max": 0.9,  # Avoid Memory Lock-in (Memory > 0.9 && Willpower > 0.8)
            "wht_threshold": self.memory_config.get("operational_constants", {}).get("wht_threshold", 0.7),
            "blending_factor": self.memory_config.get("operational_constants", {}).get("blending_factor", 0.15)
        }
    
    def _load_memory_config(self) -> Dict:
        """Load Pillar_10_Memory config."""
        config_path = Path("Pillar_10_Memory/memory_config.json")
        if config_path.exists():
            with open(config_path, 'r', encoding='utf-8') as f:
                return json.load(f)
        return {}
    
    def _pad_to_power_of_2(self, arr: np.ndarray) -> np.ndarray:
        """Pad array to next power of 2 length."""
        n = len(arr)
        next_pow2 = 1
        while next_pow2 < n:
            next_pow2 *= 2
        if next_pow2 > n:
            return np.pad(arr, (0, next_pow2 - n), 'constant')
        return arr
    
    def _fwht(self, x: np.ndarray) -> np.ndarray:
        """Fast Walsh-Hadamard Transform (FWHT) using NumPy or pyfwht if available."""
        x = x.copy().astype(np.float32)
        n = len(x)
        if n & (n - 1) != 0:
            raise ValueError(f"Length {n} must be power of 2")
        
        # Try pyfwht first (GPU-accelerated)
        try:
            import pyfwht
            return pyfwht.transform(x) / np.sqrt(n)  # Normalized
        except ImportError:
            pass
        
        # Fallback to NumPy implementation
        step = 1
        while step < n:
            for i in range(0, n, 2 * step):
                for j in range(i, i + step):
                    u = x[j]
                    v = x[j + step]
                    x[j] = u + v
                    x[j + step] = u - v
            step *= 2
        return x / np.sqrt(n)  # Normalized
    
    def _ifwht(self, x: np.ndarray) -> np.ndarray:
        """
        Inverse FWHT. Since FWHT is its own inverse (H*H = n*I),
        applying forward transform again gives: H*(H*x/sqrt(n))/sqrt(n) = n*x/n = x
        So inverse is same as forward: just apply _fwht again.
        """
        # Apply forward transform (same as inverse for FWHT)
        return self._fwht(x.copy())
    
    def reconstruct_text_from_signal(self, signal: np.ndarray, original_length: int = None) -> str:
        """
        Approximate text reconstruction from WHT signal.
        Note: Due to thresholding, reconstruction will be lossy.
        """
        # Inverse transform
        reconstructed = self._ifwht(signal.copy())
        
        # Denormalize (undo the *100 scaling from encode_text_to_signal)
        reconstructed = reconstructed / 100.0
        
        # Convert back to ASCII (take real part, round to nearest int)
        ascii_vals = np.round(np.real(reconstructed)).astype(int)
        
        # Filter valid ASCII range (32-126 for printable characters)
        ascii_vals = np.clip(ascii_vals, 32, 126)
        
        # Convert to text
        try:
            # Use original_length if provided to avoid padding
            if original_length:
                ascii_vals = ascii_vals[:original_length]
            text_chars = [chr(c) for c in ascii_vals if 32 <= c <= 126]
            return ''.join(text_chars).strip()
        except Exception:
            return "[Reconstruction failed]"
    
    def apply_pillar_thresholds(self, coefficients: np.ndarray) -> np.ndarray:
        """
        Apply pillar-specific thresholds to WHT coefficients.
        Based on Harm (0.7), Distortion (0.85), Awareness SNR (0.5) thresholds.
        """
        filtered = coefficients.copy()
        max_val = np.max(np.abs(filtered))
        
        if max_val == 0:
            return filtered
        
        # Pillar-based thresholding
        thresholds = {
            "harm": 0.7,          # Pillar_12_Harm: transformation threshold
            "distortion": 0.85,  # Pillar_13_Distortion: saturation threshold
            "awareness": 0.5     # Pillar_00_Awareness: SNR threshold
        }
        
        for name, threshold_pct in thresholds.items():
            threshold_value = max_val * threshold_pct
            # Attenuate (not zero out) based on pillar thresholds
            mask = np.abs(filtered) < threshold_value
            filtered[mask] *= 0.5  # Reduce by 50% instead of zeroing
        
        return filtered
    
    def encode_text_to_signal(self, text: str) -> np.ndarray:
        """Convert text to numerical signal (ASCII values, scaled and padded to power of 2)."""
        if not text:
            return np.array([0.0], dtype=np.float32)
        
        # Convert to ASCII values and scale to increase signal strength
        ascii_vals = [ord(c) for c in text[:1024]]  # Limit to 1024 chars
        signal = np.array(ascii_vals, dtype=np.float32)
        
        # Scale signal to have larger magnitude (multiply by 100 to avoid tiny values)
        signal = signal * 100.0
        
        # Pad to power of 2
        signal = self._pad_to_power_of_2(signal)
        return signal
    
    def translate_memory_entry(self, text: str, use_pillar_thresholds: bool = False) -> Tuple[Optional[np.ndarray], Dict]:
        """
        Translate a single memory entry (text) to WHT signal.
        Returns (transformed_signal, metadata) or (None, {}) if thresholds not met.
        """
        # Encode text to signal
        signal = self.encode_text_to_signal(text)
        
        # Apply FWHT
        transformed = self._fwht(signal)
        
        # Apply threshold (attenuate, don't zero out completely)
        max_val = np.max(np.abs(transformed))
        if max_val == 0:
            return None, {"error": "Zero signal"}
        
        if use_pillar_thresholds:
            # Use pillar-based thresholding (attenuation)
            filtered = self.apply_pillar_thresholds(transformed)
        else:
            # Simple thresholding (keep top coefficients)
            threshold_value = max_val * self.thresholds["wht_threshold"]
            filtered = transformed.copy()
            # Attenuate instead of zero - preserve some signal
            mask = np.abs(filtered) < threshold_value
            filtered[mask] *= 0.3  # Reduce by 70%, keep 30%
        
        # Check if we have enough signal energy (avoid Amnesia)
        signal_energy = np.sum(filtered ** 2)
        if signal_energy < self.thresholds["memory_min"]:
            return None, {"error": "Insufficient signal energy (Amnesia threshold)"}
        
        # Metadata
        non_zero = np.sum(filtered != 0)
        metadata = {
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "original_length": len(text),
            "signal_length": len(signal),
            "non_zero_coefficients": int(non_zero),
            "sparsity": float(np.sum(filtered == 0)) / len(filtered),
            "threshold_used": self.thresholds["wht_threshold"],
            "max_coefficient": float(max_val),
            "signal_energy": float(signal_energy),
            "pillar_thresholds_applied": use_pillar_thresholds
        }
        
        return filtered, metadata
    
    def store_signal(self, signal: np.ndarray, metadata: Dict, filename: str = None) -> Path:
        """Store transformed signal and metadata to disk."""
        if filename is None:
            timestamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
            filename = f"wht_signal_{timestamp}.npy"
        
        signal_path = self.signal_dir / filename
        np.save(signal_path, signal)
        
        # Save metadata as JSON
        meta_path = signal_path.with_suffix('.json')
        with open(meta_path, 'w', encoding='utf-8') as f:
            json.dump(metadata, f, indent=2)
        
        return signal_path
    
    def retrieve_signal(self, filename: str) -> Tuple[np.ndarray, Dict]:
        """Retrieve signal and metadata from storage."""
        signal_path = self.signal_dir / filename
        if not signal_path.exists():
            raise FileNotFoundError(f"Signal file not found: {signal_path}")
        
        signal = np.load(signal_path)
        meta_path = signal_path.with_suffix('.json')
        with open(meta_path, 'r', encoding='utf-8') as f:
            metadata = json.load(f)
        
        return signal, metadata
    
    # ── fp20 Fixed-Point WHT Encoding/Decoding ────────────────────────
    # These methods provide compatibility with the C++ vn::fp20_t type
    # (ScaledInt<int64_t, 20>) for deterministic agent communication.
    
    FP20_SCALE = 1 << 20  # 2^20 = 1048576
    FP20_MAX = (1 << 43) - 1  # max int64_t that fits in fp20 precision space
    
    @staticmethod
    def float_to_fp20(value: float) -> int:
        """Convert float to fp20_t raw int (20-bit fractional precision).
        
        Matches vn::ScaledInt<int64_t, 20> encoding:
            raw = round(value * 2^20)
        """
        raw = int(round(value * MemoryTranslator.FP20_SCALE))
        # Clamp to int64_t range
        return max(-(1 << 63), min((1 << 63) - 1, raw))
    
    @staticmethod
    def fp20_to_float(raw: int) -> float:
        """Convert fp20_t raw int back to float.
        
        Matches vn::ScaledInt::to_float():
            float = raw / 2^20
        """
        return raw / MemoryTranslator.FP20_SCALE
    
    def encode_pillars_to_fp20_signal(self, pillar_values: np.ndarray) -> np.ndarray:
        """Encode 16 pillar state values to fp20-precision WHT signal.
        
        Args:
            pillar_values: 16-element float array of pillar states (range [0, 1])
        
        Returns:
            32-element float array (fp20 quantized, then WHT transformed)
        """
        assert len(pillar_values) == 16, "Must have exactly 16 pillar values"
        
        # Quantize to fp20 precision
        fp20_raw = np.array([self.float_to_fp20(v) for v in pillar_values], dtype=np.int64)
        signal_32 = np.zeros(32, dtype=np.float64)
        
        # Elements 0-15: fp20-quantized pillar values
        for i in range(16):
            signal_32[i] = self.fp20_to_float(fp20_raw[i])
        
        # Elements 16-31: harmonic coefficients (matching wht_packet.cpp)
        pillar_rms = np.sqrt(np.mean(np.array(pillar_values) ** 2))
        for i in range(8):
            # H_n = sin(n * pi / 8) / (n + 1), fp20 quantized
            h = np.sin(i * np.pi / 8) / (i + 1) if i > 0 else 0.0
            fp20_h = self.float_to_fp20(h * pillar_rms)
            signal_32[16 + i] = self.fp20_to_float(fp20_h)
        
        # Elements 24-31: resonance cache
        for i in range(8):
            signal_32[24 + i] = self.fp20_to_float(self.float_to_fp20(0.1 * (1.0 - i * 0.1)))
        
        # Apply WHT
        wht_signal = self._fwht(signal_32.astype(np.float32))
        return wht_signal
    
    def decode_fp20_signal_to_pillars(self, wht_signal: np.ndarray) -> np.ndarray:
        """Decode fp20-precision WHT signal back to 16 pillar values.
        
        Args:
            wht_signal: 32-element float array (WHT domain)
        
        Returns:
            16-element float array of decoded pillar states
        """
        assert len(wht_signal) == 32, "WHT signal must be 32 elements"
        
        # Inverse WHT
        temp = self._ifwht(wht_signal.copy())
        
        # Extract and de-quantize first 16 elements from fp20
        pillars = np.zeros(16, dtype=np.float32)
        for i in range(16):
            raw = self.float_to_fp20(temp[i])
            pillars[i] = self.fp20_to_float(raw)
            # Clamp to valid pillar range [0, 1]
            pillars[i] = np.clip(pillars[i], 0.0, 1.0)
        
        return pillars
    
    def translate_blackboard_entries(self) -> List[Path]:
        """Translate all entries in the blackboard to WHT signals."""
        blackboard_path = self.memory_dir / "colab_blackboard.md"
        if not blackboard_path.exists():
            print(f"Blackboard not found: {blackboard_path}")
            return []
        
        with open(blackboard_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Split into entries (each starts with ### [timestamp])
        entries = re.split(r'### \[', content)
        entries = [f"### [{e}" for e in entries if e.strip()]  # Re-add the ### [
        
        stored_paths = []
        for i, entry in enumerate(entries):
            # Extract text from entry (simplified)
            text = entry[:500]  # Take first 500 chars per entry
            signal, metadata = self.translate_memory_entry(text)
            if signal is not None:
                path = self.store_signal(signal, metadata, filename=f"blackboard_entry_{i}_{datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')}.npy")
                stored_paths.append(path)
                print(f"Stored blackboard entry {i}: {metadata['non_zero_coefficients']} non-zero coeffs")
        
        return stored_paths

if __name__ == "__main__":
    # Test the translator
    translator = MemoryTranslator()
    print("Testing Memory Translator...")
    
    # Test with a sample text
    test_text = "Task delegation: Summarize benefits of local LLMs. ΔH: 0.05, Status: Completed."
    signal, metadata = translator.translate_memory_entry(test_text)
    if signal is not None:
        print(f"Translated test text: {metadata['non_zero_coefficients']} non-zero coefficients")
        path = translator.store_signal(signal, metadata)
        print(f"Stored test signal to: {path}")
    else:
        print(f"Translation failed: {metadata.get('error', 'Unknown error')}")
    
    # Test blackboard translation
    print("\nTranslating blackboard entries...")
    stored = translator.translate_blackboard_entries()
    print(f"Stored {len(stored)} blackboard entries as WHT signals")
