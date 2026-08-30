#!/usr/bin/env python3
"""Builds and runs the world-draft placement-commit proof."""

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]

OWNED = [
    "Tools/WorldDraftPlacementCommitProof/WorldDraftPlacementCommitProof.cpp",
    "Engine/SlateWorkspace/Discipline/WorldDraftSketchBridge/Source/WorldDraftSketchBridge.cpp",
]

SUPPORTING = [
    "Engine/SketchToolset/SketchTool/SketchPlacement/Source/SketchPlacement.cpp",
    "Engine/SlateWorkspace/Discipline/PlacementCommit/Source/PlacementCommit.cpp",
    "Engine/SlateWorkspace/Discipline/RecordDeclaration/Source/RecordDeclaration.cpp",
    "Engine/SlateShape/Record/WorkspaceNameIndex/Source/WorkspaceNameIndex.cpp",
    "Engine/SlateShape/Record/WorkspaceRecordStructure/Source/WorkspaceRecordStructure.cpp",
    "Engine/SlateShape/Record/WorkspaceRevisionSequence/Source/WorkspaceRevisionSequence.cpp",
    "Engine/SlateShape/Record/WorkspaceDirectoryProjection/Source/WorkspaceDirectoryProjection.cpp",
    "Engine/SlateWorkspace/Discipline/TransformSequence/Source/TransformSequence.cpp",
    "Engine/SlateWorkspace/Discipline/ViewportProjection/Source/ViewportProjection.cpp",
    "Engine/SlateWorkspace/Discipline/ViewportProjection/Source/SketchBasis.cpp",
    "Engine/SlateShape/World/WorldDraftRenderingProjection/Source/WorldDraftRenderingProjection.cpp",
    "Engine/SlateShape/World/WorldDraftPicking/Source/WorldDraftPicking.cpp",
    "Engine/SlateShape/World/WorldDraftEditing/Source/WorldDraftEditing.cpp",
    "Engine/SlateShape/World/WorldDraftAnalysis/Source/WorldDraftAnalysis.cpp",
    "Engine/SlateShape/World/WorldDraftStructure/Source/WorldDraftStructure.cpp",
    "Engine/SlateShape/Sketch/SketchStructure/Source/SketchStructure.cpp",
    "Engine/SlateShape/Sketch/SketchSelection/Source/SketchSelection.cpp",
    "Engine/SlateShape/Sketch/SketchEditing/Source/SketchEditing.cpp",
    "Engine/SlateWorkspace/Discipline/SketchPicking/Source/SketchPicking.cpp",
    "Engine/SlateShape/Sketch/SketchPolyline/Source/SketchPolyline.cpp",
    "Engine/SlateShape/Sketch/SketchSnap/Source/SketchSnap.cpp",
    "Engine/SlateShape/Sketch/ConstraintSpecification/Source/ConstraintSpecification.cpp",
    "Engine/SlateShape/Sketch/DimensionSpecification/Source/DimensionSpecification.cpp",
    "Engine/SlateShape/Reference/ReferenceSpecification/Source/ReferenceSpecification.cpp",
    "Engine/SlateShape/Sketch/ConstraintSolver/Source/ConstraintSolver.cpp",
    "Engine/SlateShape/Sketch/DimensionSolver/Source/DimensionSolver.cpp",
    "Engine/SlateShape/Geometry/CurveSpecification/Source/CurveSpecification.cpp",
    "Engine/SlateShape/Geometry/ProfileSpecification/Source/ProfileSpecification.cpp",
    "Engine/SlateShape/Geometry/SurfaceSpecification/Source/SurfaceSpecification.cpp",
    "Engine/SlateShape/Operation/ExtrusionSpecification/Source/ExtrusionSpecification.cpp",
    "Engine/SlateShape/Topology/SolidStructure/Source/SolidStructure.cpp",
]

INCLUDES = ["-I", ".", "-I", "Engine", "-I", "Tools/VulkanParseStub"]


def compile_source(source: str, strict: bool, objects: list[str]) -> bool:
    obj = ROOT / "Tools" / "WorldDraftPlacementCommitProof" / (pathlib.Path(source).stem + ".o")
    command = ["g++", "-std=c++20", "-ffunction-sections", "-fdata-sections", "-c", source, "-o", str(obj)] + INCLUDES
    command += ["-Wall", "-Wextra", "-Werror"] if strict else ["-w"]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stderr[:4000])
        return False
    objects.append(str(obj))
    return True


def main() -> int:
    objects: list[str] = []
    for source in OWNED:
        if not compile_source(source, True, objects):
            print("FAILED to compile " + source)
            return 1
    for source in SUPPORTING:
        if not compile_source(source, False, objects):
            print("FAILED to compile " + source)
            return 1

    binary = ROOT / "Tools" / "WorldDraftPlacementCommitProof" / "WorldDraftPlacementCommitProof"
    binary.unlink(missing_ok=True)
    link = subprocess.run(["g++", "-Wl,--gc-sections", "-o", str(binary)] + objects,
                          cwd=ROOT, capture_output=True, text=True)
    if link.returncode != 0:
        print(link.stderr[:4000])
        return 1

    run = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True)
    print(run.stdout)
    if run.returncode != 0:
        print(run.stderr[:2000])

    for obj in objects:
        pathlib.Path(obj).unlink(missing_ok=True)
    binary.unlink(missing_ok=True)
    return run.returncode


if __name__ == "__main__":
    sys.exit(main())
