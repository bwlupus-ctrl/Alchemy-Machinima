#!/usr/bin/env python3
"""Compatibility entry point for the Director manifest generator and mode counts."""
import runpy
from pathlib import Path

if __name__ == "__main__":
    runpy.run_path(str(Path(__file__).resolve().parents[1] / "code_tools" /
                       "director_functional_manifest.py"), run_name="__main__")
