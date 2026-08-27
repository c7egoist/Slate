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


def offstack(host: str) -> bool:
    """Whether a host keeps ViewportSequence's ~406 KB out of its automatic frame.

    A Windows thread is handed one megabyte and the budget these hosts assert against is a quarter of
    it. ViewportSequence alone is over four hundred kilobytes, so it has to live in static storage or
    the prologue's stack probe faults before main runs a statement -- no window, no log line.

    Two spellings satisfy that. A host may hold the sequence directly, or hold a SessionSequence that
    holds it; SlateRuntime owns the bring-up from step 7 onward, so the second is what a lifted host
    looks like. This tests the MECHANISM -- 406 KB off the stack -- rather than one of its spellings,
    because the earlier literal check reported a regression when a host was lifted correctly.
    """
    return ("static ViewportSequence Viewport;" in host or
            "static SessionSequence Session;" in host)


def main() -> int:
    motion = read("Engine/SlateUI/Interface/MotionIntegrator/Api/MotionIntegrator.h")
    match = re.search(r"EaseCapacity\s*=\s*(\d+)u", motion)
    require(match is not None, "MotionIntegrator must declare EaseCapacity")
    require(int(match.group(1)) >= 8192, "MotionIntegrator eased capacity must cover runtime editor startup registrations")
    require("static storage in the windowed hosts" in motion, "MotionIntegrator comment must record why the reserve is safe")

    editor = read("Engine/Application/EditorHost/Source/EditorHost.cpp")
    require("constexpr std::size_t AutomaticUiBytes = sizeof(ShaderCodec) + sizeof(WorkspaceOverlayPass);" in editor,
            "EditorHost stack assertion must only count members still on automatic storage")
    require(offstack(editor), "EditorHost must keep the motion-heavy viewport sequence off the stack")
    for needle in [
        "static WorkspaceIndex          Workspaces;",
        "static WorkspacePanel          Workspace;",
        "static EditorPanel             WorkspacePanels;",
        "static ControlCentrePanel      ControlCentre;",
        "static SceneDirectoryPanel     SceneDirectory;",
        "static TexturingPanel        Texturing;",
        "static ParametricWorkspacePanel SketchDirectory;",
        "static ParametricToolsPanel    ParametricTools;",
    ]:
        require(needle in editor, f"EditorHost missing static storage move {needle!r}")
    # 📝 `HomeProfilePath` was defined identically in two hosts and now lives in
    #    `SlateRuntime/Session/HostEnvironment`, so the MSVC guard is checked there — one place, both hosts.
    Environment = read("Engine/SlateRuntime/Session/HostEnvironment/Source/HostEnvironment.cpp")
    require("_dupenv_s(&Home" in Environment,
            "HostEnvironment must avoid MSVC getenv warning on Windows")
    require("_dupenv_s(&Home" not in editor,
            "EditorHost must not carry its own copy of the home profile lookup")
    require("std::strncpy" not in editor, "EditorHost must avoid MSVC strncpy warning")
    require("ResizedGeometryOffering" in editor and "ResizedGeometryDelivery" in editor,
            "EditorHost resize path should not shadow geometry construction locals")
    # 📝 `ConsumeSharedCodexActivation` was an `inline` in `Application/Api/SharedViewportHostBridge.h`,
    #    which is deleted. It is now `ConsumeCodexActivation` in `SlateWorkspace/Discipline/CodexActivation`.
    require("ConsumeCodexActivation" in editor,
            "EditorHost activation must use the shared codex activation unit")

    paint = read("Engine/Application/PaintHost/Source/PaintHost.cpp")
    require(offstack(paint), "PaintHost must keep the motion-heavy viewport sequence off the stack")
    require("EditorCameraComponent" in paint and "CameraInput" in paint,
            "PaintHost viewport must use the shared editor camera component and hotkey path")
    require("ConsumeCodexActivation" in paint,
            "PaintHost activation must use the shared codex activation unit")
    require("std::strncpy" not in paint, "PaintHost must avoid MSVC strncpy warning")

    validation = read("Engine/Application/InterfaceValidationHost/Source/InterfaceValidationHost.cpp")
    require("std::strncpy" not in validation, "InterfaceValidationHost must avoid MSVC strncpy warning")

    parametric = read("Engine/Application/ParametricSketchHost/Source/ParametricSketchHost.cpp")
    require(offstack(parametric),
            "ParametricSketchHost must keep the motion-heavy viewport sequence off the stack")
    require("_dupenv_s(&Home" not in parametric,
            "ParametricSketchHost must not carry its own copy of the home profile lookup")
    require("std::strncpy" not in parametric, "ParametricSketchHost must avoid MSVC strncpy warning")
    require("ConsumeCodexActivation" in parametric,
            "ParametricSketchHost activation must use the shared codex activation unit")

    # 🔴 THE BRIDGE HEADER IS GONE AND MUST STAY GONE. 675 `inline` lines under `Application/` that three
    #    hosts included — the orientation widget, the camera seed, the content root and the codex
    #    activation, none of which were bridges and none of which could be tested where they sat.
    require(not (ROOT / "Engine" / "Application" / "Api" / "SharedViewportHostBridge.h").exists(),
            "SharedViewportHostBridge.h must not come back")
    for Host, Body in (("EditorHost", editor), ("PaintHost", paint), ("ParametricSketchHost", parametric)):
        require("SharedViewportHostBridge" not in Body,
                f"{Host} must not include the deleted bridge header")

    # 🔴 This block used to demand the literal spelling `SharedCadDrawingController.h` in both hosts. That
    #    asserted a FILE NAME, not a property, so it reported a regression the moment the duplicated
    #    dispatch was correctly unified into `SlateToolset` — the same defect that made this validator
    #    demand `static ViewportSequence Viewport;` after the session was lifted. What actually matters is
    #    that the draft vocabulary is declared ONCE, in a unit, and that no host re-declares it.
    toolset = read("Engine/SketchToolset/SketchTool/SketchPlacement/Api/SketchPlacement.h")
    require("enum class SketchSubject" in toolset and "DeclaredPlacement" in toolset,
            "SketchToolset must own the shape-and-method dispatch")
    for host_name, host in (("EditorHost", editor), ("ParametricSketchHost", parametric)):
        require("enum class ParametricDraftSubject" not in host and
                "enum class SharedCadDraftSubject" not in host and
                "enum class SketchSubject" not in host,
                f"{host_name} must not re-declare the sketch subject vocabulary")
    # 🔴 The cast bridge is the specific defect this replaces: two identical enumerations reconciled by
    #    casting through the underlying integer, correct only while both stayed in the same order.
    require("static_cast<SharedCadDraftSubject>" not in parametric and
            "static_cast<ParametricDraftSubject>" not in parametric,
            "ParametricSketchHost must not cast between duplicate draft enumerations")
    require("SketchPlacement" in parametric,
            "ParametricSketchHost must drive drawing through the SketchToolset placement")
    # 🔴 The banned words. `Draft`/`Draught` name a provisional state rather than a mechanism, and
    #    `Paint` names the artist's gesture rather than the texels written.
    #
    # 🔴 THE SWEEP IS DONE, SO THE GATE NOW COVERS THE SWEPT GROUND. It used to check only the two
    #    toolsets, because 690 uses stood in SlateUI/SlateCompute/SlateDocument and a gate that fails
    #    on day one is a gate someone deletes. 610 identifier occurrences were renamed, the two
    #    directories moved, and these units are gated from here on.
    #
    # ⚠️ `PaintHost` is EXCLUDED BY NAME, not overlooked. It is deleted at step 11, and until the
    #    parametric wiring it holds is moved into `EditorHost` that deletion would take drawing with
    #    it — see References/RemainingWorkPlan.md. Excluding a known exception by name is honest;
    #    quietly widening the pattern until it passes is not.
    SweptUnits = ("SketchToolset", "TextureToolset", "SlateWorkspace", "SlateCompute", "SlateUI")
    for Unit in SweptUnits:
        for Source in (ROOT / "Engine" / Unit).rglob("*.*"):
            if Source.suffix not in (".h", ".cpp"):
                continue
            Body = Source.read_text(encoding="utf-8", errors="ignore")
            # ⚠️ TWO USES OF "Draft" ARE NOT THE BANNED WORD AND MUST SURVIVE.
            #    ① `"Draft", "0°"` on Extrude and Revolve is the CAD DRAFT ANGLE — the taper a moulded
            #       part needs to leave its die. It is standard manufacturing vocabulary with no
            #       synonym, and renaming it would make the property meaningless to the artist.
            #    ② `CodexProfile::Drafting = 10u` is bound to the `.draft` FILE EXTENSION. That token
            #       is matched against documents already on disk, so renaming it is a format break of
            #       exactly the kind the `DatumPlane` enumerator note forbids.
            #    A banned-word gate that cannot tell a vocabulary choice from a domain term or a
            #    persisted token would force a real defect to be introduced in order to pass.
            for Line in Body.splitlines():
                Code = Line.split("//", 1)[0]
                Stripped = Code.strip()
                if not Stripped:
                    continue
                if '"Draft", "0' in Code or ".draft" in Code or "CodexProfile::Drafting" in Code:
                    continue
                for Banned in ("Draught", "Draft"):
                    require(Banned not in Code,
                            f"{Source.name} carries the banned word {Banned}: {Stripped[:70]}")
            # 📝 `Paint` survives only inside prose that explains why it was removed, and in the
            #    persisted revision label `"PaintStroke"`, which is compared by string against
            #    documents already written. Renaming that is a format change, not a rename.
            for Line in Body.splitlines():
                # 📝 A trailing comment is prose too. Checking the whole line flagged
                #    `TextureFacetCount = 8u;  // [-] - Paint … Filter`, where the code is clean and
                #    only the note carries the word — a false positive that would have been "fixed"
                #    by mangling a perfectly good declaration.
                Code = Line.split("//", 1)[0]
                Stripped = Code.strip()
                if not Stripped or "PaintStroke" in Code:
                    continue
                require("Paint" not in Code,
                        f"{Source.name} carries the banned word Paint in code: {Stripped[:70]}")
    # 📝 Step 10 moved the interaction out of the host and into
    #    `SlateWorkspace/Discipline/SketchInteraction`, so the guarantee is checked where it now lives.
    #    ⚠️ The host is checked NOT to define it: a host that grew its own copy back is exactly the
    #    regression this file exists to catch, and looking only in the host could not tell the two apart.
    Interaction = (ROOT / "Engine" / "SlateWorkspace" / "Discipline" / "SketchInteraction"
                        / "Source" / "SketchInteraction.cpp").read_text(encoding="utf-8")
    require("ResolveGizmoHandle" in Interaction
            and "StartTransformSession" in Interaction
            and "UpdateTransformSession" in Interaction,
            "transform gizmo handles must remain selectable and movable")
    require("void DriveViewportSelectionAndTransform(" not in parametric,
            "ParametricSketchHost must not define the interaction it now calls")
    require("Panel leaves must sample pointer/contact before they record" in parametric and
            parametric.find("ToolPanel.Advance") < parametric.find("WorkspacePanels.Record"),
            "ParametricSketchHost must advance CAD tool interactions before recording/drawing the viewport")

    # 📝 These five claims outlived the file they were written against. The orientation widget now lives in
    #    `SlateWorkspace/Discipline/OrientationCube` and the codex activation in
    #    `SlateWorkspace/Discipline/CodexActivation`; the guarantees are unchanged, only their address is.
    orientation = read("Engine/SlateWorkspace/Discipline/OrientationCube/Source/OrientationCube.cpp")
    require("RecordOrientationWidget" in orientation and "HitOrientationWidget" in orientation,
            "the orientation unit must own the one-at-a-time gizmo dispatch")
    require("Extent.MinimumX + 52.0f" not in orientation and "Extent.MaximumX - 70.0f" in orientation,
            "both Blender and CAD viewport gizmos must be anchored at the top-right of the viewport")
    require("CubeAxisDepth" in orientation,
            "viewport gizmos must use the HTML-reference camera-forward depth ordering")
    require("DrawFaceLabel" in orientation and "TextRun(Face" not in orientation,
            "CAD cube face labels must be projected face strokes, not hovering screen-space text")

    activation = read("Engine/SlateWorkspace/Discipline/CodexActivation/Source/CodexActivation.cpp")
    require("CenterActivatedSceneAtWorldOrigin" in activation
            and "CenterActivatedSceneAtWorldOrigin(Loaded)" in activation,
            "codex scene activation must recenter loaded geometry at the world origin")

    # 🔴 THE WORKPLANE TOOL MUST BE DISPATCHED, NOT MERELY DEFINED. `ApplyWorkplaneTool` was written in
    #    full — screen-space placement, catalogue write, directory record, sealed revision — and called
    #    from nowhere. It compiled, two proofs covered its arithmetic, and picking the tool in the
    #    viewport did nothing at all, because a function nothing calls is indistinguishable from a
    #    function that does not work. No gate noticed: they all asserted the behaviour EXISTS.
    #
    # ⚠️ The ordering matters as much as the call. The workplane tool must run BEFORE the drawing tools
    #    and consume the press, or the click that places a plane is also read as the first point of a
    #    curve — drawn onto the plane that press just replaced.
    sketch_host = read("Engine/Application/ParametricSketchHost/Source/ParametricSketchHost.cpp")
    require("ApplyWorkplaneTool(" in sketch_host,
            "the parametric host must dispatch the workplane tool, not just link it")
    require(sketch_host.index("ApplyWorkplaneTool(") < sketch_host.index("DriveDrawingWithModifiers("),
            "the workplane tool must be offered the press before the drawing tools")
    require("PointerTaken = ApplyWorkplaneTool(" in sketch_host,
            "placing a workplane must consume the press so no curve starts on the replaced plane")

    interaction = read("Engine/SlateWorkspace/Discipline/SketchInteraction/Source/SketchInteraction.cpp")
    require("Workplanes.Declare(Placed" in interaction and "Sketch.DeclarePlane({ Workplanes.Active()" in interaction,
            "a placed workplane must join the catalogue and be adopted, never overwrite the sketch plane blind")

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
    # 🔴 These three strings were asserted against `Engine/Application/Api/ParametricWorkspaceBridge.h`,
    #    which no longer exists: it was 347 `inline` lines in the APPLICATION layer that `SlateWorkspace`
    #    had to reach UP into, inverting the dependency arrow. The behaviour now lives in the unit that
    #    owns the transformation. The claim is about the DATA the inspector shows, so it survives the move
    #    unchanged — only its address changes.
    parametric_bridge = read("Engine/SlateWorkspace/Discipline/SketchDirectoryPresentation/Source/SketchDirectoryPresentation.cpp")
    require("ExtrusionCapToggleDemand" in parametric_spec and "Extrude Caps" in parametric_panel,
            "closed profile properties must expose a capped/wall extrusion toggle")
    require("Curve Closure" in parametric_bridge and "Extrude Result" in parametric_bridge and "Extrude Caps" in parametric_bridge,
            "closed profile inspector data must distinguish closed-loop rendering from capped solid extrusion")

    # 🔴 Pins the deletion: a unit must never again depend on the application layer above it. If either
    #    file returns, this refuses instead of silently re-inverting the graph — the failure `make sequence`
    #    reported as "the unit graph holds a cycle".
    require(not (ROOT / "Engine/Application/Api/ParametricWorkspaceBridge.h").exists(),
            "ParametricWorkspaceBridge.h is deleted; SlateWorkspace must not reach up into Application/")
    require(not (ROOT / "Engine/Application/Api/SharedViewportHostBridge.h").exists(),
            "SharedViewportHostBridge.h is deleted; its four concerns live in the units that own them")

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
