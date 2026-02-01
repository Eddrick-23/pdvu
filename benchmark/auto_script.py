# /// script
# requires-python = ">=3.12"
# dependencies = [
#   'psutil',
#   'pyautogui'
# ]
# ///

import psutil
from pathlib import Path
import signal
import time
import argparse
import subprocess
import sys
from actions import (
    SCROLL_LEFT,
    SCROLL_RIGHT,
    FAST_SCROLL_LEFT,
    FAST_SCROLL_RIGHT,
    ZOOM_IN,
    ZOOM_OUT,
    GO_TO_PAGE,
    SET_TOOL,
    SET_WINDOW_AND_MODE,
)
from json_parser import JsonParser

parser = argparse.ArgumentParser(
    description="Automated script to benchmark tdf file by simulating keypresses"
)

parser.add_argument(
    "tool",
    type=str,
    choices=["pdvu", "tdf", "Preview"],
    help="which pdf tool you are benchmarking",
)

parser.add_argument("--trackmem", action="store_true", help="Enable memory tracking")
args = parser.parse_args()
SET_TOOL(args.tool)

result = subprocess.run(["pgrep", args.tool], capture_output=True, text=True)

pids = [int(p) for p in result.stdout.split()]
if not pids:
    raise RuntimeError(f"{args.tool} is not running")

#Note tools like preview usually have multiple such processes under the same name
#Usually taking the last one is the correct one
pid = pids[-1]
print(f"pid found: {pid}")
proc = None
json_output_path = (
    Path(__file__).resolve().parent / "output" / "json" / f"{args.tool}_footprint.json"
)
csv_output_path = (
    Path(__file__).resolve().parent / "output" / "csv" / f"{args.tool}_footprint.csv"
)
json_output_path.parent.mkdir(parents=True, exist_ok=True)
csv_output_path.parent.mkdir(parents=True, exist_ok=True)


if args.trackmem:
    print("Tracking memory using native footprint tool...")
    proc = subprocess.Popen(
        [
            "footprint",
            "-p",
            str(pid),
            "--sample",
            "0.5",
            "--json",
            str(json_output_path),
            "--noCategories",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def stop_tracker():
    if (
        args.trackmem and proc and proc.poll() is None
    ):  # check if process is still running
        print("Stopping footprint tracker...")
        try:
            proc.send_signal(signal.SIGINT)
            proc.wait(timeout=5)
        except (psutil.NoSuchProcess, subprocess.TimeoutExpired):
            proc.kill()


interrupted = False


try:
    print("Starting in:")
    for i in range(5, 0, -1):
        print(i)
        time.sleep(1)

    SET_WINDOW_AND_MODE(args.tool)

    SCROLL_RIGHT(10)

    ZOOM_IN(3, 0.5)
    ZOOM_OUT(3, 0.5)

    FAST_SCROLL_RIGHT(30)

    GO_TO_PAGE(
        200 if args.tool == "Preview" else 223, 1
    )  # 223 is a heavy page in the image heavy pdf

    FAST_SCROLL_LEFT(50)

    GO_TO_PAGE(700, 0.5)

    ZOOM_IN(3, 0.5)
    SCROLL_LEFT(5)
    ZOOM_OUT(3, 0.5)

    FAST_SCROLL_RIGHT(40)

    ZOOM_IN(3, 0.5)
    GO_TO_PAGE(
        320 - 23 if args.tool == "Preview" else 320, 1
    )  # 320 is a heavy page in the image heavy pdf
    ZOOM_OUT(3, 0.5)

    SCROLL_LEFT(10)

    FAST_SCROLL_LEFT(30)

    GO_TO_PAGE(500, 0.5)
    GO_TO_PAGE(540, 0.5)
    GO_TO_PAGE(73, 0.5)
    FAST_SCROLL_LEFT(10)
    GO_TO_PAGE(1)
except KeyboardInterrupt:
    print("Benchmark interrupted")
    stop_tracker()

    if args.trackmem:
        print("Parsing data to csv")
        parser = JsonParser(json_output_path, csv_output_path)
        parser.parse_and_write()

    interrupted = True
    print("TEST END")
finally:
    if not interrupted:
        stop_tracker()

        if args.trackmem:
            print("Parsing data to csv")
            parser = JsonParser(json_output_path, csv_output_path)
            parser.parse_and_write()

        print("Ending in")
        for i in range(5, 0, -1):
            print(i)
            time.sleep(1)
        print("TEST END")
