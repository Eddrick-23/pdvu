# /// script
# requires-python = ">=3.12"
# dependencies = [
#   'pyautogui'
# ]
# ///

import pyautogui

TOOL = "pdvu"  # default


def SET_TOOL(tool: str):
    global TOOL
    TOOL = tool


NEXT_PAGE_KEY = {
    "pdvu": lambda: pyautogui.press("right"),
    "tdf": lambda: pyautogui.press("right"),
    "Preview": lambda: pyautogui.hotkey("option", "down"),
}

PREV_PAGE_KEY = {
    "pdvu": lambda: pyautogui.press("left"),
    "tdf": lambda: pyautogui.press("left"),
    "Preview": lambda: pyautogui.hotkey("option", "up"),
}

GO_TO_PAGE_INPUT_KEY = {
    "pdvu": lambda: pyautogui.press("g"),
    "tdf": lambda: pyautogui.press("g"),
    "Preview": lambda: pyautogui.hotkey("command", "option", "g"),
}

# NOTE to allow zooming in tdf, we need to press g to put into the correct mode
ZOOM_IN_KEY = {
    "pdvu": lambda: pyautogui.press("="),
    "tdf": lambda: pyautogui.press("o"),
    "Preview": lambda: pyautogui.hotkey("command", "="),
}
ZOOM_OUT_KEY = {
    "pdvu": lambda: pyautogui.press("-"),
    "tdf": lambda: pyautogui.press("O"),
    "Preview": lambda: pyautogui.hotkey("command", "-"),
}


def RESET_MODIFIERS():
    for key in ("shift", "command", "option", "ctrl", "fn"):
        pyautogui.keyUp(key)


def NEXT_PAGE():
    return NEXT_PAGE_KEY[TOOL]()


def PREV_PAGE():
    return PREV_PAGE_KEY[TOOL]()


def ENTER():
    return pyautogui.press("enter")


def ENTER_VALUE(val: str):
    pyautogui.write(val, interval=0.25)


def SCROLL_RIGHT(n: int):
    RESET_MODIFIERS()
    for i in range(n):
        NEXT_PAGE()
        pyautogui.sleep(0.5)


def FAST_SCROLL_RIGHT(n: int):
    RESET_MODIFIERS()
    for i in range(n):
        NEXT_PAGE()
        pyautogui.sleep(0.05)


def SCROLL_LEFT(n: int):
    RESET_MODIFIERS()
    for i in range(n):
        PREV_PAGE()
        pyautogui.sleep(0.5)


def FAST_SCROLL_LEFT(n: int):
    RESET_MODIFIERS()
    for i in range(n):
        PREV_PAGE()
        pyautogui.sleep(0.05)


def SET_WINDOW_AND_MODE(tool: str):
    if tool == "tdf":
        pyautogui.press("z")
    elif tool == "Preview":
        pyautogui.hotkey("command", "2")


def GO_TO_PAGE(n: int, sleep: float = 0.0):
    RESET_MODIFIERS()
    GO_TO_PAGE_INPUT_KEY[TOOL]()
    ENTER_VALUE(str(n))
    ENTER()
    pyautogui.sleep(sleep)


def ZOOM_IN(n: int, sleep: float = 0.0):
    RESET_MODIFIERS()
    for i in range(n):
        ZOOM_IN_KEY[TOOL]()
        pyautogui.sleep(sleep)


def ZOOM_OUT(n: int, sleep: float = 0.0):
    RESET_MODIFIERS()
    for i in range(n):
        ZOOM_OUT_KEY[TOOL]()
        pyautogui.sleep(sleep)
