from __future__ import annotations

from dataclasses import dataclass
from functools import cached_property
import linecache
import logging
import sys
from types import FrameType


@dataclass()
class ComponentFrame:
    """Component frame container."""

    custom_component: bool
    component: str
    module: str | None
    relative_filename: str
    frame: FrameType

    @cached_property
    def line_number(self) -> int:
        """Return the line number of the frame."""
        return self.frame.f_lineno

    @cached_property
    def filename(self) -> str:
        """Return the filename of the frame."""
        return self.frame.f_code.co_filename

    @cached_property
    def line(self) -> str:
        """Return the line of the frame."""
        return (linecache.getline(self.filename, self.line_number) or "?").strip()


def get_current_frame(depth: int = 0) -> FrameType:
    """Return the current frame."""
    # Add one to depth since get_current_frame is included
    return sys._getframe(depth + 1)  # pylint: disable=protected-access


class MissingComponentFrame(Exception):
    """Raised when no component is found in the frame."""


def get_component_frame(exclude_components: set | None = None) -> ComponentFrame:
    """Return the frame, component and component path of the current stack frame."""
    found_frame = None
    if not exclude_components:
        exclude_components = set()

    frame: FrameType | None = get_current_frame()
    while frame is not None:
        filename = frame.f_code.co_filename

        for path in ("custom_components/", "esphome/components/"):
            try:
                index = filename.index(path)
                start = index + len(path)
                end = filename.index("/", start)
                component = filename[start:end]
                if component not in exclude_components:
                    found_frame = frame

                break
            except ValueError:
                continue

        if found_frame is not None:
            break

        frame = frame.f_back

    if found_frame is None:
        raise MissingComponentFrame

    found_module: str | None = None
    for module, module_obj in dict(sys.modules).items():
        if not hasattr(module_obj, "__file__"):
            continue
        if module_obj.__file__ == found_frame.f_code.co_filename:
            found_module = module
            break

    return ComponentFrame(
        custom_component=path == "custom_components/",
        component=component,
        module=found_module,
        relative_filename=found_frame.f_code.co_filename[index:],
        frame=found_frame,
    )


def get_component_logger(fallback_name: str) -> logging.Logger:
    """Return a logger by checking the current component frame.

    If Python is unable to access the sources files, the call stack frame
    will be missing information, so let's guard by requiring a fallback name.
    """
    try:
        component_frame = get_component_frame()
    except MissingComponentFrame:
        return logging.getLogger(fallback_name)

    if component_frame.custom_component:
        logger_name = f"custom_components.{component_frame.component}"
    else:
        logger_name = f"esphome.components.{component_frame.component}"

    return logging.getLogger(logger_name)
