import httpx
import pytest

BASE_URL = "http://localhost:8080"


def test_api_root():
    """Test that the API root endpoint is accessible."""
    response = httpx.get(f"{BASE_URL}/")
    assert response.status_code == 200


def test_get_world_state():
    """Test GET /api/world/state endpoint."""
    response = httpx.get(f"{BASE_URL}/api/world/state")
    assert response.status_code == 200
    data = response.json()
    assert "temperature" in data
    assert "hazard_level" in data
    assert "resource_density" in data
    assert "tick_count" in data


def test_get_agents_list():
    """Test GET /api/agents endpoint."""
    response = httpx.get(f"{BASE_URL}/api/agents")
    assert response.status_code == 200
    data = response.json()
    assert "agents" in data
    assert isinstance(data["agents"], list)


def test_get_nonexistent_agent():
    """Test GET /api/agents/{id} with invalid ID."""
    response = httpx.get(f"{BASE_URL}/api/agents/999")
    assert response.status_code == 404


def test_post_simulation_tick():
    """Test POST /api/simulation/tick endpoint."""
    response = httpx.post(f"{BASE_URL}/api/simulation/tick", json={"ticks": 1})
    assert response.status_code == 200
    data = response.json()
    assert "ticks_processed" in data
    assert "tick_count" in data


def test_get_chunk_not_implemented():
    """Test GET /api/chunks/{x}/{y}/{z} endpoint (may return 501)."""
    response = httpx.get(f"{BASE_URL}/api/chunks/0/0/0")
    assert response.status_code in [200, 501]
