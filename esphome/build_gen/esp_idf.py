from esphome.core import CORE

from . import cmake


class Formatter(cmake.Formatter):
    def include_espidf_project(self) -> str:
        return "include($ENV{IDF_PATH}/tools/cmake/project.cmake)"

    def compile_definitions(self, definitions: list[str]) -> str:
        return f"idf_build_set_property(COMPILE_DEFINITIONS \n\t{'\n\t'.join(definitions)}\n\tAPPEND\n)"

    def compile_options(self, flags: list[str]) -> str:
        return f"idf_build_set_property(COMPILE_OPTIONS \n\t{'\n\t'.join(flags)}\n\tAPPEND\n)"


class Generator(cmake.Generator):
    def get_content(self, f: Formatter = Formatter()) -> str:
        # CORE.add_platformio_option(
        #    "lib_deps",
        #    [x.as_lib_dep for x in CORE.platformio_libraries] + ["${common.lib_deps}"],
        # )
        # Sort to avoid changing build flags order
        content = [
            f.project(CORE.name),
            f.include_espidf_project(),
            f.compile_definitions(cmake.get_compile_defines()),
            f.compile_options(cmake.get_compile_options()),
        ]

        return "\n\n".join(content) + "\n"
