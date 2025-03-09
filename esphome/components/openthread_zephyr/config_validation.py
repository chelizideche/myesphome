from esphome.config_validation import Version, Invalid
from esphome.core import CORE
from esphome.const import KEY_CORE, KEY_TARGET_FRAMEWORK, KEY_FRAMEWORK_VERSION

def require_framework_version(required_version):
    def validator(value):
        if not CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] == "zephyr":
            return value
            
        version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
        if version < required_version:
            raise Invalid(
                f"This feature requires at least Zephyr framework version {required_version}"
            )
        return value
    return validator 