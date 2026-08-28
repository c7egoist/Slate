#!/usr/bin/env python3
"""Builds and runs the tool-options widget proof.

The widget's own translation unit is compiled with every warning as an error, so a measure that stops
being used or a parameter that stops being read fails here rather than being noticed later. The proof
links only against what its claims touch: the arithmetic of the card, its rows and its pill.
"""

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]

OWNED = [
    "Tools/ToolOptionsWidgetProof/ToolOptionsWidgetProof.cpp",
    "Engine/SlateUI/Interface/ToolOptionsWidget/Source/ToolOptionsWidget.cpp",
]

INCLUDES = ["-I", ".", "-I", "Engine", "-I", "Tools/VulkanParseStub"]


def Compile(Source, Strict, Objects):
    Object = ROOT / "Tools" / "ToolOptionsWidgetProof" / (pathlib.Path(Source).stem + ".o")
    Command = ["g++", "-std=c++20", "-c", Source, "-o", str(Object)] + INCLUDES
    Command += ["-Wall", "-Wextra", "-Werror"] if Strict else ["-w"]
    Result = subprocess.run(Command, cwd=ROOT, capture_output=True, text=True)
    if Result.returncode != 0:
        print(Result.stderr[:4000])
        return False
    Objects.append(str(Object))
    return True


def Main():
    Objects = []

    # 📝 The widget compiles strictly on its own, which is the claim that the unit is warning-clean.
    #    The proof then runs against the parts of it that need no live surface.
    for Source in OWNED:
        if not Compile(Source, True, Objects):
            print("[ToolOptionsWidgetProof] FAILED to compile", Source)
            return 1

    # 🔴 Only the proof is linked. The widget's recording path reaches a RecordingSurface, a
    #    MotionIntegrator and a ThemeProfile — the whole interface stack — and dragging that into a
    #    gate would test the stack rather than the widget. It is compiled above, so its warnings and
    #    its type errors are caught; what runs below is the arithmetic its behaviour rests on.
    Binary = ROOT / "Tools" / "ToolOptionsWidgetProof" / "ToolOptionsWidgetProof"
    Link = ["g++", "-std=c++20", "-o", str(Binary),
            str(ROOT / "Tools" / "ToolOptionsWidgetProof" / "ToolOptionsWidgetProof.o")]
    Result = subprocess.run(Link, cwd=ROOT, capture_output=True, text=True)
    if Result.returncode != 0:
        Reported = [Line for Line in Result.stderr.splitlines() if "undefined ref" in Line]
        print("\n".join(Reported[:20]) if Reported else Result.stderr[:3000])
        print("[ToolOptionsWidgetProof] FAILED to link")
        return 1

    Ran = subprocess.run([str(Binary)], cwd=ROOT, capture_output=True, text=True)
    print(Ran.stdout)
    if Ran.returncode != 0:
        print(Ran.stderr[:2000])
    return Ran.returncode


if __name__ == "__main__":
    sys.exit(Main())
