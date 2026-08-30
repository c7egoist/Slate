#!/usr/bin/env python3
"""Builds and runs the construction-catalogue proof.

Unlike the placement proof this one LINKS ENGINE CODE, because the thing under test is geometry: the
corner solver, the resolver that turns a selected curve and a click into a corner, and the reshape
entry points behind trim and cut. Compiling was never the question -- `ApplyProfileCorner` compiled
fine for as long as it had no caller.
"""

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]

OWNED = [
    "Tools/ConstructionToolProof/ConstructionToolProof.cpp",
    "Engine/SlateShape/Sketch/ProfileCorner/Source/ProfileCorner.cpp",
    "Engine/SlateShape/Sketch/ProfileReshape/Source/ProfileReshape.cpp",
    "Engine/SlateShape/Sketch/SketchStructure/Source/SketchStructure.cpp",
    "Engine/SlateShape/Geometry/CurveSpecification/Source/CurveSpecification.cpp",
    "Engine/SlateShape/Geometry/ProfileSpecification/Source/ProfileSpecification.cpp",
    "Engine/SlateShape/Sketch/SketchPolyline/Source/SketchPolyline.cpp",
    "Engine/SlateShape/Sketch/ConstraintSpecification/Source/ConstraintSpecification.cpp",
    "Engine/SlateShape/Sketch/DimensionSpecification/Source/DimensionSpecification.cpp",
    "Engine/SlateShape/Reference/ReferenceSpecification/Source/ReferenceSpecification.cpp",
]

INCLUDES = ["-I", ".", "-I", "Engine", "-I", "Tools/VulkanParseStub"]


def Main():
    Binary = ROOT / "Tools" / "ConstructionToolProof" / "ConstructionToolProof"
    Binary.unlink(missing_ok=True)

    Command = ["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
               "-o", str(Binary)] + OWNED + INCLUDES
    Built = subprocess.run(Command, cwd=ROOT, capture_output=True, text=True)
    if Built.returncode != 0:
        print(Built.stderr[:4000])
        return 1

    Run = subprocess.run([str(Binary)], cwd=ROOT, capture_output=True, text=True)
    print(Run.stdout)
    if Run.returncode != 0:
        print(Run.stderr[:2000])

    Binary.unlink(missing_ok=True)
    return Run.returncode


if __name__ == "__main__":
    sys.exit(Main())
