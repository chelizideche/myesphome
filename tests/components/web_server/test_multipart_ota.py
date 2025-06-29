#!/usr/bin/env python3
"""
Test script for ESP-IDF web server multipart OTA upload functionality.
This script can be run manually to test OTA uploads to a running device.
"""

import argparse
from pathlib import Path
import sys
import time

import requests


def test_multipart_ota_upload(host, port, firmware_path):
    """Test OTA firmware upload using multipart/form-data"""
    base_url = f"http://{host}:{port}"

    print(f"Testing OTA upload to {base_url}")

    # First check if server is reachable
    try:
        resp = requests.get(f"{base_url}/", timeout=5)
        if resp.status_code != 200:
            print(f"Error: Server returned status {resp.status_code}")
            return False
        print("✓ Server is reachable")
    except requests.exceptions.RequestException as e:
        print(f"Error: Cannot reach server - {e}")
        return False

    # Check if firmware file exists
    if not Path(firmware_path).exists():
        print(f"Error: Firmware file not found: {firmware_path}")
        return False

    # Prepare multipart upload
    print(f"Uploading firmware: {firmware_path}")
    print(f"File size: {Path(firmware_path).stat().st_size} bytes")

    try:
        with open(firmware_path, "rb") as f:
            files = {"firmware": ("firmware.bin", f, "application/octet-stream")}

            # Send OTA update request
            resp = requests.post(f"{base_url}/ota/upload", files=files, timeout=60)

            if resp.status_code in [200, 201, 204]:
                print(f"✓ OTA upload successful (status: {resp.status_code})")
                if resp.text:
                    print(f"Response: {resp.text}")
                return True
            else:
                print(f"✗ OTA upload failed with status {resp.status_code}")
                print(f"Response: {resp.text}")
                return False

    except requests.exceptions.RequestException as e:
        print(f"Error during upload: {e}")
        return False


def test_ota_with_wrong_content_type(host, port):
    """Test that OTA upload fails gracefully with wrong content type"""
    base_url = f"http://{host}:{port}"

    print("\nTesting OTA with wrong content type...")

    try:
        # Send plain text instead of multipart
        headers = {"Content-Type": "text/plain"}
        resp = requests.post(
            f"{base_url}/ota/upload",
            data="This is not a firmware file",
            headers=headers,
            timeout=10,
        )

        if resp.status_code >= 400:
            print(
                f"✓ Server correctly rejected wrong content type (status: {resp.status_code})"
            )
            return True
        else:
            print(f"✗ Server accepted wrong content type (status: {resp.status_code})")
            return False

    except requests.exceptions.RequestException as e:
        print(f"Error: {e}")
        return False


def test_ota_with_empty_file(host, port):
    """Test that OTA upload fails gracefully with empty file"""
    base_url = f"http://{host}:{port}"

    print("\nTesting OTA with empty file...")

    try:
        # Send empty file
        files = {"firmware": ("empty.bin", b"", "application/octet-stream")}
        resp = requests.post(f"{base_url}/ota/upload", files=files, timeout=10)

        if resp.status_code >= 400:
            print(
                f"✓ Server correctly rejected empty file (status: {resp.status_code})"
            )
            return True
        else:
            print(f"✗ Server accepted empty file (status: {resp.status_code})")
            return False

    except requests.exceptions.RequestException as e:
        print(f"Error: {e}")
        return False


def create_test_firmware(size_kb=10):
    """Create a dummy firmware file for testing"""
    import tempfile

    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        # ESP32 firmware magic bytes
        f.write(b"\xe9\x08\x02\x20")
        # Add padding
        f.write(b"\x00" * (size_kb * 1024 - 4))
        return f.name


def main():
    parser = argparse.ArgumentParser(
        description="Test ESP-IDF web server OTA functionality"
    )
    parser.add_argument("--host", default="localhost", help="Device hostname or IP")
    parser.add_argument("--port", type=int, default=8080, help="Web server port")
    parser.add_argument(
        "--firmware", help="Path to firmware file (if not specified, creates test file)"
    )
    parser.add_argument(
        "--skip-error-tests", action="store_true", help="Skip error condition tests"
    )

    args = parser.parse_args()

    # Create test firmware if not specified
    firmware_path = args.firmware
    if not firmware_path:
        print("Creating test firmware file...")
        firmware_path = create_test_firmware(100)  # 100KB test file
        print(f"Created test firmware: {firmware_path}")

    all_passed = True

    # Test successful OTA upload
    if not test_multipart_ota_upload(args.host, args.port, firmware_path):
        all_passed = False

    # Test error conditions
    if not args.skip_error_tests:
        time.sleep(1)  # Small delay between tests

        if not test_ota_with_wrong_content_type(args.host, args.port):
            all_passed = False

        time.sleep(1)

        if not test_ota_with_empty_file(args.host, args.port):
            all_passed = False

    # Clean up test firmware if we created it
    if not args.firmware:
        import os

        os.unlink(firmware_path)
        print("\nCleaned up test firmware")

    print(f"\n{'All tests passed!' if all_passed else 'Some tests failed!'}")
    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
