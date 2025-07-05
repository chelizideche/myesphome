#!/usr/bin/env python3
import os
import glob
import subprocess
import sys
from pathlib import Path

import pytest
import yaml

# Ignore custom !include tags by returning the node value
yaml.SafeLoader.add_constructor("!include", lambda loader, node: node.value)


def gather_test_entries():
    components_dir = Path(__file__).parent / "components"
    if not components_dir.is_dir():
        pytest.skip(f"Components directory not found: {components_dir}")

    # Collect testNNN.yaml files directly in components_dir
    files = sorted(
        glob.glob(str(components_dir / "test*.yaml")),
        key=lambda p: int(Path(p).stem.replace("test", "")),
    )
    if not files:
        pytest.skip("No testNNN.yaml files found in components directory.")
    return files


# Gather entries and human-friendly IDs
entries = gather_test_entries()
ids = [Path(f).stem for f in entries]

# Track includes to enforce no overlap across tests
seen_includes = set()


@pytest.mark.parametrize("test_file", entries, ids=ids)
def test_esphome_yaml(test_file):
    # Skip tests on Windows
    if os.name == "nt":
        print("⚠️ Skipping esphome tests on Windows", file=sys.stderr)
        return

    # Load YAML and enforce name matches filename
    data = yaml.safe_load(Path(test_file).read_text())
    expected_name = Path(test_file).stem
    actual_name = data.get("esphome", {}).get("name")
    assert actual_name == expected_name, (
        f"esphome/name '{actual_name}' does not match filename '{expected_name}'"
    )

    # Enforce unique includes across files
    includes = data.get("esphome", {}).get("includes", []) or []
    overlap = set(includes) & seen_includes
    assert not overlap, f"Includes overlap found: {overlap}"
    seen_includes.update(includes)

    # Compile step
    compile_cmd = ["esphome", "compile", test_file]
    compile_proc = subprocess.run(
        compile_cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    assert compile_proc.returncode == 0, (
        f"Compile failed for {test_file}:\n{compile_proc.stdout}"
    )

    # Run step
    run_cmd = ["esphome", "run", test_file]
    run_proc = subprocess.run(run_cmd, check=False)
    assert run_proc.returncode == 0, f"Run failed for {test_file}"


if __name__ == "__main__":
    sys.exit(pytest.main([__file__]))
