# Testing ESP-IDF Web Server OTA Functionality

This directory contains tests for the ESP-IDF web server OTA (Over-The-Air) update functionality using multipart form uploads.

## Test Files

- `test_ota.esp32-idf.yaml` - ESPHome configuration with OTA enabled for ESP-IDF
- `test_no_ota.esp32-idf.yaml` - ESPHome configuration with OTA disabled
- `test_ota_disabled.esp32-idf.yaml` - ESPHome configuration with web_server ota: false
- `test_multipart_ota.py` - Manual test script for OTA functionality
- `test_esp_idf_ota.py` - Automated pytest for OTA functionality

## Running the Tests

### 1. Compile and Flash Test Device

```bash
# Compile the OTA-enabled configuration
esphome compile tests/components/web_server/test_ota.esp32-idf.yaml

# Flash to device
esphome upload tests/components/web_server/test_ota.esp32-idf.yaml
```

### 2. Run Manual Tests

Once the device is running, you can test OTA functionality:

```bash
# Test with default settings (creates test firmware)
python tests/components/web_server/test_multipart_ota.py --host <device-ip>

# Test with real firmware file
python tests/components/web_server/test_multipart_ota.py --host <device-ip> --firmware <path-to-firmware.bin>

# Skip error condition tests (useful for production devices)
python tests/components/web_server/test_multipart_ota.py --host <device-ip> --skip-error-tests
```

### 3. Run Automated Tests

```bash
# Run pytest suite
pytest tests/component_tests/web_server/test_esp_idf_ota.py
```

## What's Being Tested

1. **Multipart Upload**: Tests that firmware can be uploaded using multipart/form-data
2. **Error Handling**: 
   - Wrong content type rejection
   - Empty file rejection
   - Concurrent upload handling
3. **Large Files**: Tests chunked processing of larger firmware files
4. **Boundary Parsing**: Tests various multipart boundary formats

## Implementation Details

The ESP-IDF web server uses the `multipart-parser` library to handle multipart uploads. Key components:

- `MultipartReader` class for parsing multipart data
- Chunked processing to handle large files without excessive memory use
- Integration with ESPHome's OTA component for actual firmware updates

## Troubleshooting

1. **Connection Refused**: Make sure the device is on the network and the IP is correct
2. **404 Not Found**: Ensure OTA is enabled in the configuration (`ota: true` in web_server)
3. **Upload Fails**: Check device logs for detailed error messages
4. **Timeout**: Large firmware files may take time, increase timeout if needed