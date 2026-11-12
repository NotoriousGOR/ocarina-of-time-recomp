#!/usr/bin/env python3
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


if len(sys.argv) != 2 or sys.platform != "darwin":
    raise SystemExit("Usage on macOS: python3 tests/shared_settings_test.py <app-executable>")

with tempfile.TemporaryDirectory(prefix="zelda-shared-settings-") as directory:
    config = Path(directory) / "general.json"
    config.write_text(json.dumps({"autosave_mode": "On", "rumble_strength": 25}))
    env = {**os.environ, "APP_FOLDER_PATH": directory}
    process = subprocess.Popen([sys.argv[1]], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f"Launcher exited early: {process.returncode}")
            result = subprocess.run(
                ["osascript", "-e", f'tell application "System Events" to count windows of '
                 f'(first application process whose unix id is {process.pid})'],
                capture_output=True, text=True,
            )
            if result.returncode == 0 and result.stdout.strip() != "0":
                break
            time.sleep(0.5)
        else:
            raise RuntimeError("Launcher window did not appear")

        config.write_text(json.dumps({"autosave_mode": "Off", "rumble_strength": 37}))
        script = f'''
tell application "System Events"
    set targetProcess to first application process whose unix id is {process.pid}
    set frontmost of targetProcess to true
    delay 0.5
    if not (frontmost of targetProcess) then error "Could not focus the launcher"
    key code 125
    delay 0.3
    key code 125
    delay 0.3
    key code 36
    delay 1
    key code 53
end tell
'''
        subprocess.run(["osascript", "-e", script], check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            saved = json.loads(config.read_text())
            if "background_input_mode" in saved:
                break
            time.sleep(0.2)
        else:
            raise RuntimeError("The settings menu did not save its configuration")
        assert saved["autosave_mode"] == "Off" and saved["rumble_strength"] == 37, saved
        print("Shared settings survive opening and closing the launcher menu")
    finally:
        if process.poll() is None:
            process.terminate()
        process.wait(timeout=10)
