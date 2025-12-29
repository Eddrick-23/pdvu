# /// script
# requires-python = ">=3.12"
# dependencies = [
#   'pyautogui'
# ]
# ///

import os
from pathlib import Path
import signal
import pyautogui
import time
import argparse
import subprocess

parser = argparse.ArgumentParser(
    description="Automated script to benchmark tdf file by simulating keypresses"
)

parser.add_argument("tool", type=str, choices=["pdvu", "tdf", "Preview"], help="which pdf tool you are benchmarking")
args = parser.parse_args()

NEXT_PAGE_KEY = {"pdvu" : "right", "tdf" : "right", "Preview" : "right"}
PREV_PAGE_KEY = {"pdvu" : "left", "tdf" : "left", "Preview" : "left"}
GO_TO_PAGE_INPUT_KEY = {"pdvu" : lambda : pyautogui.press('g'),
                        "tdf" : lambda:pyautogui.press('g'), 
                        "Preview" : lambda: pyautogui.hotkey('option', 'command', 'g')}

NEXT_PAGE = lambda:pyautogui.press(NEXT_PAGE_KEY[args.tool])
PREV_PAGE = lambda:pyautogui.press(PREV_PAGE_KEY[args.tool])
GO_TO_PAGE_INPUT = GO_TO_PAGE_INPUT_KEY[args.tool]
ENTER = lambda:pyautogui.press('enter')


def ENTER_VALUE(val : str):
    pyautogui.write(val, interval=0.25)

def SCROLL_RIGHT(n : int):
    for i in range(n):
        NEXT_PAGE()
        pyautogui.sleep(0.5)

def FAST_SCROLL_RIGHT(n: int):
    for i in range(n):
        NEXT_PAGE()
        pyautogui.sleep(0.05)

def SCROLL_LEFT(n : int):
    for i in range(n):
        PREV_PAGE()
        pyautogui.sleep(0.5)

def FAST_SCROLL_LEFT(n: int):
    for i in range(n):
        PREV_PAGE()
        pyautogui.sleep(0.05)

def GO_TO_PAGE(n : int, sleep : float = 0.0):
    RESET_MODIFIERS()
    GO_TO_PAGE_INPUT()
    ENTER_VALUE(str(n))
    ENTER()
    pyautogui.sleep(sleep)

def RESET_MODIFIERS():
    for key in ('shift', 'command', 'option', 'ctrl', 'fn'):
        pyautogui.keyUp(key)
    time.sleep(0.05)

result = subprocess.run(
    ["pgrep", args.tool],
    capture_output=True,
    text=True
)

pids = [int(p) for p in result.stdout.split()]
if not pids:
    raise RuntimeError(f"{args.tool} is not running")

pid = pids[0]

print(f"pid found: {pid}")
print(f"starting memory tracker")
script_dir = Path(__file__).resolve().parent
tracker = script_dir / "memory_tracker.py"
proc = subprocess.Popen(
    [
        "uv", "run", str(tracker),
        "--filename", f"{args.tool}_auto_bench",
        str(pid)
    ],
    preexec_fn=os.setsid
)

interrupted = False


try :
    print("Starting in:")
    for i in range(5,0, -1):
        print(i)
        time.sleep(1)

    SCROLL_RIGHT(10)

    FAST_SCROLL_RIGHT(30)

    GO_TO_PAGE(200 if args.tool == "Preview" else 223, 1)  # 223 is a heavy page in one of the pdfs

    FAST_SCROLL_LEFT(50)

    GO_TO_PAGE(700, 0.5)

    SCROLL_LEFT(5)

    FAST_SCROLL_RIGHT(40)

    GO_TO_PAGE(320 - 23 if args.tool == "Preview" else 320, 1) #320 is a heavy page in one of the pdfs

    SCROLL_LEFT(10)

    FAST_SCROLL_LEFT(30)

    GO_TO_PAGE(500, 0.5)
    GO_TO_PAGE(540, 0.5)
    GO_TO_PAGE(73, 0.5)
    FAST_SCROLL_LEFT(10)
    GO_TO_PAGE(1)
except KeyboardInterrupt:
    os.killpg(proc.pid, signal.SIGINT)
    proc.wait()
    interrupted = True
    print("TEST END")
finally:
    if (not interrupted):
        print("Ending in")
        for i in range(5, 0, -1):
            print(i)
            time.sleep(1)

        os.killpg(proc.pid, signal.SIGINT)
        proc.wait()
        print("TEST END")

