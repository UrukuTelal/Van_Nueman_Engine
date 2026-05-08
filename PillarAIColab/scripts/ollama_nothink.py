"""Ollama native API wrapper with think=False support for qwen3.5 models.

Imported from CrowNest-Public and adapted for PillarAIColab.
Supports disabling thinking tokens (/no_think prefix) for faster responses.
"""
import requests
import json
from typing import Optional


class OllamaNoThink:
    """Wrapper around Ollama native API with think=False support."""

    def __init__(self, model_name: str = "qwen3.5:4b", base_url: str = "http://localhost:11434"):
        self.model_name = model_name
        self.base_url = base_url

    def chat(self, system_message: str = "", user_message: str = "", max_tokens: int = 150) -> str:
        messages = []
        if system_message:
            messages.append({"role": "system", "content": system_message})
        messages.append({"role": "user", "content": user_message})

        payload = {
            "model": self.model_name,
            "messages": messages,
            "stream": False,
            "options": {"num_predict": max_tokens},
        }

        if not user_message.startswith("/no_think"):
            payload["options"]["num_predict"] = max_tokens * 2

        resp = requests.post(
            f"{self.base_url}/api/chat",
            json=payload,
            timeout=120,
        )
        resp.raise_for_status()
        data = resp.json()
        return data.get("message", {}).get("content", "")


class SimpleAgent:
    """Minimal agent using OllamaNoThink."""

    def __init__(self, system_message: str, model: OllamaNoThink):
        self.system_message = system_message
        self.model = model

    def step(self, user_message: str) -> str:
        clean = user_message.replace("/no_think ", "").replace("/no_think", "")
        return self.model.chat(self.system_message, clean)
