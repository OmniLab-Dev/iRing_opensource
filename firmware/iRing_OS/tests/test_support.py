"""Shared test sources and tool lookup."""
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
SUITES = ("gestures", "features", "analog", "health", "direction", "waveform")
SOURCES = (ROOT / "gesture_engine.cpp", ROOT / "analog_touch.cpp")

def tool(name, directory=None):
    if directory:
        for suffix in ("", ".exe"):
            candidate = Path(directory) / (name + suffix)
            if candidate.is_file():
                return str(candidate)
    else:
        found = shutil.which(name)
        if found:
            return found
    raise SystemExit(f"Cannot find {name}; check the toolchain path.")
