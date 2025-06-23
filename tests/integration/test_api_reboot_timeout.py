"""Test API server reboot timeout functionality."""

import asyncio
import re

import pytest

from .types import RunCompiledFunction


@pytest.mark.asyncio
async def test_api_reboot_timeout(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
) -> None:
    """Test that the device reboots when no API clients connect within the timeout."""
    reboot_detected = False
    reboot_pattern = re.compile(r"No client connected; rebooting")

    def check_output(line: str) -> None:
        """Check output for reboot message."""
        nonlocal reboot_detected
        if reboot_pattern.search(line):
            reboot_detected = True

    # Run the device without connecting any API client
    async with run_compiled(yaml_config, line_callback=check_output):
        # Wait for up to 3 seconds for the reboot to occur
        # (1s timeout + some margin for processing)
        start_time = asyncio.get_event_loop().time()
        while not reboot_detected:
            await asyncio.sleep(0.1)
            elapsed = asyncio.get_event_loop().time() - start_time
            if elapsed > 3.0:
                pytest.fail("Device did not reboot within expected timeout")

    # Verify that reboot was detected
    assert reboot_detected, "Reboot message was not detected in output"
