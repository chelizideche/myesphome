#!/usr/bin/env python3
import os
import glob
import subprocess
import sys
from pathlib import Path
import pytest


def gather_test_entries():
    components_dir = os.path.abspath(Path(__file__).parent / "components")
    if not os.path.isdir(components_dir):
        pytest.skip(f"Components directory not found: {components_dir}")

    entry_list = []
    for entry in os.scandir(components_dir):
        if not entry.is_dir():
            continue
        files = sorted(glob.glob(os.path.join(entry.path, "test_*.yaml")))
        for f in files:
            entry_list.append((os.path.basename(entry.path), f))
    if not entry_list:
        pytest.skip("No test_*.yaml files found in any component folders.")
    return entry_list


# Generate human-friendly IDs from the YAML filenames
entries = gather_test_entries()
ids = [Path(f).stem for _, f in entries]


@pytest.mark.parametrize("folder_name,test_file", entries, ids=ids)
def test_esphome_yaml(folder_name, test_file):
    """
    Pytest subtest for each YAML: first compile, then run.
    """

    # Skip tests on Windows
    if os.name == "nt":
        print("⚠️ Skipping esphome tests on Windows", file=sys.stderr)
        return

    # Compile step (capture output)
    compile_cmd = ["esphome", "compile", test_file]
    compile_proc = subprocess.run(
        compile_cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    assert compile_proc.returncode == 0, (
        f"Compile failed for {test_file} (in {folder_name}):\n{compile_proc.stdout}"
    )

    # Run step (live output)
    run_cmd = ["esphome", "run", test_file]
    run_proc = subprocess.run(run_cmd, check=False)
    assert run_proc.returncode == 0, f"Run failed for {test_file} (in {folder_name})"


if __name__ == "__main__":
    # Allow running via pytest discovery
    sys.exit(pytest.main([__file__]))
