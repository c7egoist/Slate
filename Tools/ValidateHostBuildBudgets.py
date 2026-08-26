#!/usr/bin/env python3
"""Validation for host build-budget regressions found by the Windows build."""
from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    motion = read("Engine/SlateUI/Interface/MotionIntegrator/Api/MotionIntegrator.h")
    match = re.search(r"EaseCapacity\s*=\s*(\d+)u", motion)
    require(match is not None, "MotionIntegrator must declare EaseCapacity")
    require(int(match.group(1)) >= 8192, "MotionIntegrator eased capacity must cover runtime editor startup registrations")
    require("static storage in the windowed hosts" in motion, "MotionIntegrator comment must record why the reserve is safe")

    editor = read("Engine/Application/EditorHost/Source/EditorHost.cpp")
    require("constexpr std::size_t AutomaticUiBytes = sizeof(ShaderCodec) + sizeof(WorkspaceOverlayPass);" in editor,
            "EditorHost stack assertion must only count members still on automatic storage")
    for needle in [
        "static ViewportSequence Viewport;",
        "static WorkspaceIndex          Workspaces;",
        "static WorkspacePanel          Workspace;",
        "static EditorPanel             WorkspacePanels;",
        "static ControlCentrePanel      ControlCentre;",
        "static SceneDirectoryPanel     SceneDirectory;",
        "static TexturePaintPanel        TexturePaint;",
        "static ParametricWorkspacePanel SketchDirectory;",
        "static ParametricToolsPanel    ParametricTools;",
    ]:
        require(needle in editor, f"EditorHost missing static storage move {needle!r}")
    require("_dupenv_s(&Home" in editor, "EditorHost must avoid MSVC getenv warning on Windows")
    require("std::strncpy" not in editor, "EditorHost must avoid MSVC strncpy warning")
    require("ResizedGeometryOffering" in editor and "ResizedGeometryDelivery" in editor,
            "EditorHost resize path should not shadow geometry construction locals")
    require("ConsumeSharedCodexActivation" in editor, "EditorHost activation must use the shared codex activation helper")

    paint = read("Engine/Application/PaintHost/Source/PaintHost.cpp")
    require("static ViewportSequence Viewport;" in paint, "PaintHost must keep the motion-heavy viewport sequence off the stack")
    require("EditorCameraComponent" in paint and "CameraInput" in paint,
            "PaintHost viewport must use the shared editor camera component and hotkey path")
    require("ConsumeSharedCodexActivation" in paint,
            "PaintHost activation must use the shared codex activation helper")
    require("std::strncpy" not in paint, "PaintHost must avoid MSVC strncpy warning")

    validation = read("Engine/Application/InterfaceValidationHost/Source/InterfaceValidationHost.cpp")
    require("std::strncpy" not in validation, "InterfaceValidationHost must avoid MSVC strncpy warning")

    parametric = read("Engine/Application/ParametricSketchHost/Source/ParametricSketchHost.cpp")
    require("static ViewportSequence Viewport;" in parametric,
            "ParametricSketchHost must keep the motion-heavy viewport sequence off the stack")
    require("_dupenv_s(&Home" in parametric, "ParametricSketchHost must avoid MSVC getenv warning on Windows")
    require("std::strncpy" not in parametric, "ParametricSketchHost must avoid MSVC strncpy warning")
    require("ConsumeSharedCodexActivation" in parametric,
            "ParametricSketchHost activation must use the shared codex activation helper")

    shared_cad = read("Engine/Application/Api/SharedCadDrawingController.h")
    require("ResolveSharedCadDraftSubject" in shared_cad and "SharedCadDraftRequiredAnchors" in shared_cad,
            "shared CAD drawing controller must own the tool-to-draft dispatch")
    require("SharedCadDrawingController.h" in editor and "ResolveSharedCadDraftSubject" in editor,
            "EditorHost must consume the shared CAD drawing controller dispatch")
    require("SharedCadDrawingController.h" in parametric and "ResolveSharedCadDraftSubject" in parametric,
            "ParametricSketchHost must consume the shared CAD drawing controller dispatch")
    require("ResolveGizmoHandle" in parametric and "StartTransformSession" in parametric and "UpdateTransformSession" in parametric,
            "ParametricSketchHost transform gizmo handles must remain selectable and movable")
    require("Panel leaves must sample pointer/contact before they record" in parametric and
            parametric.find("ToolPanel.Advance") < parametric.find("WorkspacePanels.Record"),
            "ParametricSketchHost must advance CAD tool interactions before recording/drawing the viewport")

    shared_viewport = read("Engine/Application/Api/SharedViewportHostBridge.h")
    require("RecordSharedViewportGizmo" in shared_viewport and "HitSharedViewportGizmo" in shared_viewport,
            "shared viewport bridge must own the one-at-a-time gizmo dispatch")
    require("Extent.MinimumX + 52.0f" not in shared_viewport and "Extent.MaximumX - 70.0f" in shared_viewport,
            "both Blender and CAD viewport gizmos must be anchored at the top-right of the viewport")
    require("SharedViewportCameraDepth" in shared_viewport,
            "shared viewport gizmos must use the HTML-reference camera-forward depth ordering")
    require("DrawFaceLabel" in shared_viewport and "TextRun(Face" not in shared_viewport,
            "CAD cube face labels must be projected face strokes, not hovering screen-space text")
    require("CenterActivatedSceneAtWorldOrigin" in shared_viewport and "CenterActivatedSceneAtWorldOrigin(Loaded)" in shared_viewport,
            "codex scene activation must recenter loaded geometry at the world origin")

    content_browser = read("Engine/SlateUI/Interface/ContentBrowserPanel/Source/ContentBrowserPanel.cpp")
    require("ActivationRequested = Library.Taken" in content_browser and "ActivationRequested = Index" in content_browser,
            "Content Browser card and Import button must request codex activation")
    require("ImportPressed" in content_browser and "Sampled.ContactReleased" in content_browser,
            "Content Browser import button must visibly press and activate on click release")

    tools = read("Engine/SlateUI/Interface/ParametricTools/Source/ParametricToolsPanel.cpp")
    require("static_cast<void>(Elapsed);" in tools, "ParametricToolsPanel must mark Elapsed as intentionally unused")
    require("Bezier" in tools and "Hermite" in tools and "NURBS Curve" in tools,
            "Sketch Draw must expose the full primary curve set")

    parametric_spec = read("Engine/SlateUI/Interface/ParametricWorkspace/Api/ParametricWorkspaceSpecification.h")
    parametric_panel = read("Engine/SlateUI/Interface/ParametricWorkspace/Source/ParametricWorkspacePanel.cpp")
    parametric_bridge = read("Engine/Application/Api/ParametricWorkspaceBridge.h")
    require("ExtrusionCapToggleDemand" in parametric_spec and "Extrude Caps" in parametric_panel,
            "closed profile properties must expose a capped/wall extrusion toggle")
    require("Curve Closure" in parametric_bridge and "Extrude Result" in parametric_bridge and "Extrude Caps" in parametric_bridge,
            "closed profile inspector data must distinguish closed-loop rendering from capped solid extrusion")

    theme = read("Engine/SlateUI/Interface/ThemeInterchange/Source/ThemeInterchange.cpp")
    require("std::strncpy" not in theme, "ThemeInterchange must avoid MSVC strncpy warning")

    print("[HostBuildBudgets] editor build budgets and warning fixes hold")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"[HostBuildBudgets] failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
