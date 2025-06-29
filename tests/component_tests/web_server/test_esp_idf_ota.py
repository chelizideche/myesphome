import asyncio
import os
import tempfile

import aiohttp
import pytest


@pytest.fixture
async def web_server_fixture(event_loop):
    """Start the test device with web server"""
    # This would be replaced with actual device setup in a real test environment
    # For now, we'll assume the device is running at a specific address
    base_url = "http://localhost:8080"

    # Wait a bit for server to be ready
    await asyncio.sleep(2)

    yield base_url


async def create_test_firmware():
    """Create a dummy firmware file for testing"""
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        # Write some dummy data that looks like a firmware file
        # ESP32 firmware files typically start with these magic bytes
        f.write(b"\xe9\x08\x02\x20")  # ESP32 magic bytes
        # Add some padding to make it look like a real firmware
        f.write(b"\x00" * 1024)  # 1KB of zeros
        f.write(b"TEST_FIRMWARE_CONTENT")
        f.write(b"\x00" * 1024)  # More padding
        return f.name


@pytest.mark.asyncio
async def test_ota_upload_multipart(web_server_fixture):
    """Test OTA firmware upload using multipart/form-data"""
    base_url = web_server_fixture
    firmware_path = await create_test_firmware()

    try:
        # Create multipart form data
        async with aiohttp.ClientSession() as session:
            # First, check if OTA endpoint is available
            async with session.get(f"{base_url}/") as resp:
                assert resp.status == 200
                content = await resp.text()
                assert "ota" in content or "OTA" in content

            # Prepare multipart upload
            with open(firmware_path, "rb") as f:
                data = aiohttp.FormData()
                data.add_field(
                    "firmware",
                    f,
                    filename="firmware.bin",
                    content_type="application/octet-stream",
                )

                # Send OTA update request
                async with session.post(f"{base_url}/ota/upload", data=data) as resp:
                    assert resp.status in [200, 201, 204], (
                        f"OTA upload failed with status {resp.status}"
                    )

                    # Check response
                    if resp.status == 200:
                        response_text = await resp.text()
                        # The response might be JSON or plain text depending on implementation
                        assert (
                            "success" in response_text.lower()
                            or "ok" in response_text.lower()
                        )

    finally:
        # Clean up
        os.unlink(firmware_path)


@pytest.mark.asyncio
async def test_ota_upload_wrong_content_type(web_server_fixture):
    """Test that OTA upload fails with wrong content type"""
    base_url = web_server_fixture

    async with aiohttp.ClientSession() as session:
        # Try to upload with wrong content type
        data = b"not a firmware file"
        headers = {"Content-Type": "text/plain"}

        async with session.post(
            f"{base_url}/ota/upload", data=data, headers=headers
        ) as resp:
            # Should fail with bad request or similar
            assert resp.status >= 400, f"Expected error status, got {resp.status}"


@pytest.mark.asyncio
async def test_ota_upload_empty_file(web_server_fixture):
    """Test that OTA upload fails with empty file"""
    base_url = web_server_fixture

    async with aiohttp.ClientSession() as session:
        # Create empty multipart upload
        data = aiohttp.FormData()
        data.add_field(
            "firmware",
            b"",
            filename="empty.bin",
            content_type="application/octet-stream",
        )

        async with session.post(f"{base_url}/ota/upload", data=data) as resp:
            # Should fail with bad request
            assert resp.status >= 400, (
                f"Expected error status for empty file, got {resp.status}"
            )


@pytest.mark.asyncio
async def test_ota_multipart_boundary_parsing(web_server_fixture):
    """Test multipart boundary parsing edge cases"""
    base_url = web_server_fixture
    firmware_path = await create_test_firmware()

    try:
        async with aiohttp.ClientSession() as session:
            # Test with custom boundary
            with open(firmware_path, "rb") as f:
                # Create multipart manually with specific boundary
                boundary = "----WebKitFormBoundaryCustomTest123"
                body = (
                    f"--{boundary}\r\n"
                    f'Content-Disposition: form-data; name="firmware"; filename="test.bin"\r\n'
                    f"Content-Type: application/octet-stream\r\n"
                    f"\r\n"
                ).encode()
                body += f.read()
                body += f"\r\n--{boundary}--\r\n".encode()

                headers = {
                    "Content-Type": f"multipart/form-data; boundary={boundary}",
                    "Content-Length": str(len(body)),
                }

                async with session.post(
                    f"{base_url}/ota/upload", data=body, headers=headers
                ) as resp:
                    assert resp.status in [200, 201, 204], (
                        f"Custom boundary upload failed with status {resp.status}"
                    )

    finally:
        os.unlink(firmware_path)


@pytest.mark.asyncio
async def test_ota_concurrent_uploads(web_server_fixture):
    """Test that concurrent OTA uploads are properly handled"""
    base_url = web_server_fixture
    firmware_path = await create_test_firmware()

    try:
        async with aiohttp.ClientSession() as session:
            # Create two concurrent upload tasks
            async def upload_firmware():
                with open(firmware_path, "rb") as f:
                    data = aiohttp.FormData()
                    data.add_field(
                        "firmware",
                        f.read(),  # Read to bytes to avoid file conflicts
                        filename="firmware.bin",
                        content_type="application/octet-stream",
                    )

                    async with session.post(
                        f"{base_url}/ota/upload", data=data
                    ) as resp:
                        return resp.status

            # Start two uploads concurrently
            results = await asyncio.gather(
                upload_firmware(), upload_firmware(), return_exceptions=True
            )

            # One should succeed, the other should fail with conflict
            statuses = [r for r in results if isinstance(r, int)]
            assert len(statuses) == 2
            assert 200 in statuses or 201 in statuses or 204 in statuses
            # The other might be 409 Conflict or similar

    finally:
        os.unlink(firmware_path)


@pytest.mark.asyncio
async def test_ota_large_file_upload(web_server_fixture):
    """Test OTA upload with a larger file to test chunked processing"""
    base_url = web_server_fixture

    # Create a larger test firmware (1MB)
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        # ESP32 magic bytes
        f.write(b"\xe9\x08\x02\x20")
        # Write 1MB of data in chunks
        chunk_size = 4096
        for _ in range(256):  # 256 * 4KB = 1MB
            f.write(b"A" * chunk_size)
        firmware_path = f.name

    try:
        async with aiohttp.ClientSession() as session:
            with open(firmware_path, "rb") as f:
                data = aiohttp.FormData()
                data.add_field(
                    "firmware",
                    f,
                    filename="large_firmware.bin",
                    content_type="application/octet-stream",
                )

                # Use a longer timeout for large file
                timeout = aiohttp.ClientTimeout(total=60)
                async with session.post(
                    f"{base_url}/ota/upload", data=data, timeout=timeout
                ) as resp:
                    assert resp.status in [200, 201, 204], (
                        f"Large file OTA upload failed with status {resp.status}"
                    )

    finally:
        os.unlink(firmware_path)


if __name__ == "__main__":
    # For manual testing
    asyncio.run(test_ota_upload_multipart(asyncio.Event()))
