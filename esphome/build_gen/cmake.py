import os
from pathlib import Path

from esphome.const import ENV_NOGITIGNORE
from esphome.core import CORE
from esphome.helpers import get_bool_env, mkdir_p, read_file, write_file_if_changed
from esphome.writer import find_begin_end, update_storage_json, write_gitignore

AUTO_GENERATE_BEGIN = "# ========== AUTO GENERATED CODE BEGIN ==========="
AUTO_GENERATE_END = "# =========== AUTO GENERATED CODE END ============"

REQUIRED_VERSION_MIN = "3.26"

BASE_FORMAT = (
    f"""# Auto generated code by esphome
cmake_minimum_required(VERSION {REQUIRED_VERSION_MIN})

""",
    """

""",
)


def get_compile_defines() -> set[str]:
    return {x[2:] for x in sorted(CORE.build_flags) if x.startswith("-D")}


def get_compile_options() -> set[str]:
    return {x for x in sorted(CORE.build_flags) if not x.startswith("-D")}


class Formatter:
    def project(self, name: str, description=None) -> str:
        return (
            f"project({name}"
            + (f"\n DESCRIPTION {description}" if description else "")
            + ")"
        )

    def target(self, name: str, type: str = "library") -> str:
        return f"add_{type}({name})"

    def target_sources(self, target: str, sources: list[str]) -> str:
        return f"target_sources({target} PRIVATE \n\t{'\n\t'.join(sources)}\n)"

    def target_compile_options(self, target: str, flags: list[str]) -> str:
        return f"target_compile_options({target} PRIVATE \n\t{'\n\t'.join(flags)}\n)"

    def target_compile_definitions(
        self, target: str, definitions: dict[str, str]
    ) -> str:
        return f"target_compile_definitions({target} PRIVATE \n\t{'\n\t'.join(definitions)}\n)"

    def compile_options(self, flags: list[str]) -> str:
        return f"add_compile_options(\n\t{'\n\t'.join(flags)}\n)"

    def compile_definitions(self, definitions: dict[str, str]) -> str:
        return f"add_compile_definitions(\n\t{'\n\t'.join(definitions)}\n)"

    def fetchcontent_git(self, name: str, repo: str, tag: str) -> str:
        self.fetchcontent_included = True
        prelude = "include(FetchContent)" if not self.fetchcontent_included else ""
        return (
            prelude
            + f"""FetchContent_Declare({name}
    GIT_REPOSITORY {repo}
    GIT_TAG {tag}
    GIT_SHALLOW YES
)
FetchContent_MakeAvailable({name})"""
        )


class Generator:
    def get_content(self, f: Formatter = Formatter()) -> str:
        # CORE.add_platformio_option(
        #    "lib_deps",
        #    [x.as_lib_dep for x in CORE.platformio_libraries] + ["${common.lib_deps}"],
        # )
        # Sort to avoid changing build flags order

        src_path = Path(CORE.build_path)  # / "src"

        sources = [
            str(cpp_path.relative_to(src_path).as_posix())
            for cpp_path in src_path.rglob("*.cpp")
        ]

        content = [
            f.project(CORE.name),
            *[
                f.fetchcontent_git(lib.name, lib.repository, lib.version)
                for lib in CORE.platformio_libraries
            ],
            f.compile_definitions(get_compile_defines()),
            f.compile_options(get_compile_options()),
            f.target(CORE.name, "executable"),
            f.target_sources(CORE.name, sources),
        ]

        return "\n\n".join(content) + "\n"

    def write_cmakelists(self, content: str):
        update_storage_json()
        path = CORE.relative_build_path("CMakeLists.txt")

        if os.path.isfile(path):
            text = read_file(path)
            content_format = find_begin_end(
                text, AUTO_GENERATE_BEGIN, AUTO_GENERATE_END
            )
        else:
            content_format = BASE_FORMAT
        full_file = f"{content_format[0] + AUTO_GENERATE_BEGIN}\n{content}"
        full_file += AUTO_GENERATE_END + content_format[1]
        write_file_if_changed(path, full_file)

    def write_project(self):
        mkdir_p(CORE.build_path)

        content = self.get_content()
        if not get_bool_env(ENV_NOGITIGNORE):
            write_gitignore()
        self.write_cmakelists(content)
