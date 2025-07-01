"""Tests for web_server OTA migration validation."""

import pytest

from esphome import config_validation as cv


def test_web_server_ota_true_fails_validation():
    """Test that web_server with ota: true fails validation with helpful message."""
    from esphome.components.web_server import validate_ota_removed

    # Config with ota: true should fail
    config = {"ota": True}

    with pytest.raises(cv.Invalid) as exc_info:
        validate_ota_removed(config)

    # Check error message contains migration instructions
    error_msg = str(exc_info.value)
    assert "has been removed from 'web_server'" in error_msg
    assert "platform: web_server" in error_msg
    assert "ota:" in error_msg


def test_web_server_ota_false_passes_validation():
    """Test that web_server with ota: false passes validation."""
    from esphome.components.web_server import validate_ota_removed

    # Config with ota: false should pass
    config = {"ota": False}
    result = validate_ota_removed(config)
    assert result == config

    # Config without ota should also pass
    config = {}
    result = validate_ota_removed(config)
    assert result == config
