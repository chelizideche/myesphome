"""Integration test for loop disable/enable functionality."""

from __future__ import annotations

import asyncio
import logging
from pathlib import Path
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

_LOGGER = logging.getLogger(__name__)


@pytest.mark.asyncio
async def test_loop_disable_enable(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that components can disable and enable their loop() method."""
    # Get the absolute path to the external components directory
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )

    # Replace the placeholder in the YAML config with the actual path
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    # Track log messages and events
    log_messages = []
    self_disable_10_disabled = asyncio.Event()
    normal_component_10_loops = asyncio.Event()
    redundant_enable_tested = asyncio.Event()
    redundant_disable_tested = asyncio.Event()
    self_disable_10_counts = []
    normal_component_counts = []

    def on_log_line(line: str) -> None:
        """Process each log line from the process output."""
        # Strip ANSI color codes
        clean_line = re.sub(r"\x1b\[[0-9;]*m", "", line)

        if "loop_test_component" not in clean_line:
            return

            log_messages.append(clean_line)

            # Track specific events using the cleaned line
            if "[self_disable_10]" in clean_line:
                if "Loop count:" in clean_line:
                    # Extract loop count
                    try:
                        count = int(clean_line.split("Loop count: ")[1])
                        self_disable_10_counts.append(count)
                    except (IndexError, ValueError):
                        pass
                elif "Disabling self after 10 loops" in clean_line:
                    self_disable_10_disabled.set()

            elif "[normal_component]" in clean_line and "Loop count:" in clean_line:
                try:
                    count = int(clean_line.split("Loop count: ")[1])
                    normal_component_counts.append(count)
                    if count >= 10:
                        normal_component_10_loops.set()
                except (IndexError, ValueError):
                    pass

            elif (
                "[redundant_enable]" in clean_line
                and "Testing enable when already enabled" in clean_line
            ):
                redundant_enable_tested.set()

            elif (
                "[redundant_disable]" in clean_line
                and "Testing disable when will be disabled" in clean_line
            ):
                redundant_disable_tested.set()

    # Write, compile and run the ESPHome device with log callback
    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        # Verify we can connect and get device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "loop-test"

        # Wait for self_disable_10 to disable itself
        try:
            await asyncio.wait_for(self_disable_10_disabled.wait(), timeout=10.0)
        except asyncio.TimeoutError:
            pytest.fail("self_disable_10 did not disable itself within 10 seconds")

        # Verify it ran exactly 10 times
        assert len(self_disable_10_counts) == 10, (
            f"Expected 10 loops for self_disable_10, got {len(self_disable_10_counts)}"
        )
        assert self_disable_10_counts == list(range(1, 11)), (
            f"Expected counts 1-10, got {self_disable_10_counts}"
        )

        # Wait for normal_component to run at least 10 times
        try:
            await asyncio.wait_for(normal_component_10_loops.wait(), timeout=10.0)
        except asyncio.TimeoutError:
            pytest.fail(
                f"normal_component did not reach 10 loops within timeout, got {len(normal_component_counts)}"
            )

        # Wait for redundant operation tests
        try:
            await asyncio.wait_for(redundant_enable_tested.wait(), timeout=10.0)
        except asyncio.TimeoutError:
            pytest.fail("redundant_enable did not test enabling when already enabled")

        try:
            await asyncio.wait_for(redundant_disable_tested.wait(), timeout=10.0)
        except asyncio.TimeoutError:
            pytest.fail(
                "redundant_disable did not test disabling when will be disabled"
            )

        # Wait a bit to see if self_disable_10 gets re-enabled
        await asyncio.sleep(3)

        # Check final counts
        later_self_disable_counts = [c for c in self_disable_10_counts if c > 10]
        if later_self_disable_counts:
            _LOGGER.info(
                f"self_disable_10 was successfully re-enabled and ran {len(later_self_disable_counts)} more times"
            )

        _LOGGER.info("Loop disable/enable test passed - all assertions verified!")
