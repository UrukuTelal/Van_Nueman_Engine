"""
test_rest_api.py - Test the C++ REST API (port 8080)

These tests verify the REST API endpoints work correctly.
Requires the API server to be running on localhost:8080.
"""

import pytest
import httpx
import time
import subprocess
import sys
import os

BASE_URL = "http://localhost:8080"

# Check if server is running
def is_server_running():
    try:
        response = httpx.get(f"{BASE_URL}/", timeout=1.0)
        return True
    except (httpx.ConnectError, httpx.TimeoutException):
        return False

SERVER_RUNNING = is_server_running()


@pytest.fixture(scope="module")
def api_server():
    """
    Fixture to start/stop the API server if needed.
    For now, we assume the server is already running.
    """
    yield


class TestRestApi:
    """Test suite for REST API endpoints."""

    @pytest.mark.skipif(not SERVER_RUNNING, reason="API server not running on port 8080")
    def test_server_health(self, api_server):
        """Test that the server is responding."""
        response = httpx.get(f"{BASE_URL}/", timeout=2.0)
        assert response.status_code in [200, 404]  # 404 is ok if no root handler

    @pytest.mark.skipif(not SERVER_RUNNING, reason="API server not running on port 8080")
    def test_get_world_state(self, api_server):
        """Test GET /api/world/state endpoint."""
        response = httpx.get(f"{BASE_URL}/api/world/state", timeout=2.0)
        assert response.status_code == 200
        data = response.json()
        assert "temperature" in data
        assert "hazard_level" in data
        assert "resource_density" in data
        assert "tick_count" in data

    @pytest.mark.skipif(not SERVER_RUNNING, reason="API server not running on port 8080")
    def test_get_agents_list(self, api_server):
        """Test GET /api/agents endpoint."""
        response = httpx.get(f"{BASE_URL}/api/agents", timeout=2.0)
        assert response.status_code == 200
        data = response.json()
        assert "agents" in data
        assert isinstance(data["agents"], list)

    @pytest.mark.skipif(not SERVER_RUNNING, reason="API server not running on port 8080")
    def test_get_nonexistent_agent(self, api_server):
        """Test GET /api/agents/{id} with invalid ID."""
        response = httpx.get(f"{BASE_URL}/api/agents/99999", timeout=2.0)
        assert response.status_code == 404

    @pytest.mark.skipif(not SERVER_RUNNING, reason="API server not running on port 8080")
    def test_post_simulation_tick(self, api_server):
        """Test POST /api/simulation/tick endpoint."""
        response = httpx.post(
            f"{BASE_URL}/api/simulation/tick",
            json={"ticks": 1},
            timeout=2.0
        )
        assert response.status_code == 200
        data = response.json()
        assert "ticks_processed" in data
        assert "tick_count" in data

    @pytest.mark.skipif(not SERVER_RUNNING, reason="API server not running on port 8080")
    def test_chunk_endpoint_not_implemented(self, api_server):
        """Test GET /api/chunks/{x}/{y}/{z} returns 501 or 200."""
        response = httpx.get(f"{BASE_URL}/api/chunks/0/0/0", timeout=2.0)
        assert response.status_code in [200, 501]


def test_api_import():
    """Test that we can import the API module."""
    assert httpx is not None
    assert pytest is not None


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
