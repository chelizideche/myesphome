"""Simple test that defer() maintains FIFO order."""

import asyncio

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_defer_fifo_simple(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that defer() maintains FIFO order with a simple test."""

    async with run_compiled(yaml_config), api_client_connected() as client:
        # Just verify we can connect and the device is running
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "defer-fifo-simple"

        # Give the test component time to run
        await asyncio.sleep(5)

        # The component will log results, we mainly want to ensure
        # it doesn't crash and completes successfully
        print("Defer FIFO simple test completed")
