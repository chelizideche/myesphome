"""Simple test that defer() maintains FIFO order."""

import asyncio

from aioesphomeapi import BinarySensorInfo, BinarySensorState, EntityState
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
        # Verify we can connect
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "defer-fifo-simple"

        # List entities to get the keys
        entity_info, _ = await asyncio.wait_for(
            client.list_entities_services(), timeout=5.0
        )

        # Find our test entities
        test_complete_entity: BinarySensorInfo | None = None
        test_passed_entity: BinarySensorInfo | None = None

        for entity in entity_info:
            if isinstance(entity, BinarySensorInfo):
                if entity.object_id == "test_complete":
                    test_complete_entity = entity
                elif entity.object_id == "test_passed":
                    test_passed_entity = entity

        assert test_complete_entity is not None, "test_complete sensor not found"
        assert test_passed_entity is not None, "test_passed sensor not found"

        # Get the event loop
        loop = asyncio.get_running_loop()

        # Subscribe to state changes
        states: dict[int, EntityState] = {}
        test_complete_future: asyncio.Future[BinarySensorState] = loop.create_future()
        test_passed_future: asyncio.Future[BinarySensorState] = loop.create_future()

        def on_state(state: EntityState) -> None:
            states[state.key] = state
            # Check if this is our test_complete binary sensor
            if isinstance(state, BinarySensorState):
                if state.key == test_complete_entity.key:
                    if state.state and not test_complete_future.done():
                        test_complete_future.set_result(state)
                elif state.key == test_passed_entity.key:
                    if not test_passed_future.done():
                        test_passed_future.set_result(state)

        client.subscribe_states(on_state)

        # Wait for test completion with timeout
        try:
            await asyncio.wait_for(test_complete_future, timeout=10.0)
            test_passed_state = await asyncio.wait_for(test_passed_future, timeout=1.0)
        except asyncio.TimeoutError:
            pytest.fail(
                f"Test did not complete within 10 seconds. "
                f"Received states: {list(states.values())}"
            )

        # Verify the test passed
        assert test_passed_state.state is True, (
            "FIFO test failed - items executed out of order"
        )
