# Paying off the host debt — the 12 that remain

> Written at `12e9a68e`. This plan finishes step 10 and states honestly what step 10's exit actually is.

---

## 0. What I got wrong, and the correction

I told you **37 file-scope definitions remain across all hosts**. That number is wrong, and wrong in the
direction that makes the job look bigger and vaguer than it is. The gate's own accounting is:

```
[HostPartition] totals — 12 definitions, 2 engine-mechanism, 1 preprocessor-gated, 0 unreachable
```

`VerifyHostPartition` **already exempts** `ConsoleHost` and `InterfaceValidationHost` (`EXEMPT_SUBJECTS`,
line 41) because neither presents a workspace — `ConsoleHost` is the test runner and its 23 `Verify*`
functions are its whole reason to exist. I counted them anyway by walking the directory myself instead of
asking the gate. **I should have asked the tool that owns the definition of "done" rather than
re-deriving it.** That is the same mistake as reimplementing a gate's parser to query it, which is already
in my notes as a dead end.

The real remaining debt, in full:

| File | Lines | Defs | Mech | PP | What they are |
|------|------:|-----:|-----:|---:|---------------|
| `EditorHost.cpp` | 1 668 | 2 | 1 | 0 | `ProjectWorkspaceCodexPoint`, `RecordWorkspaceCodexProxy` |
| `EditorHost/SkyImage.cpp` | 257 | 1 | 0 | 0 | `GenerateSkyImage` |
| `PaintHost.cpp` | 693 | 3 | 1 | 0 | `ProjectPaintScenePoint`, `RecordPaintSceneProxy`, `RecordSharedViewportChrome` |
| `ParametricSketchHost.cpp` | 1 348 | 6 | 0 | 1 | `Attach`, `ClearInspectorBridge`, `SeedParametricWorkspace`, `SeatParametricContext`, `SynchroniseParametricPresentation`, `SynchroniseCadPacket` |

**Twelve.** Six of them are deleted outright by steps 11–12. The work below is what has to happen to the
other six, plus the two loose ends.

---

## 1. The root cause I found while surveying — and it is not what the names suggest

`ParametricSketchHost` had `RecordCodexSceneProxy`. `EditorHost` has `RecordWorkspaceCodexProxy` +
`ProjectWorkspaceCodexPoint`. `PaintHost` has `RecordPaintSceneProxy` + `ProjectPaintScenePoint`. I have
been describing these as "three copies of the same thing", and asking why anyone would write it three times.

They were not copied. **They were written three times because the shared projection could not express the
editor's camera.**

```
ViewportStanding  — Orientation, Focus, Distance, OrthoScale, OrbitYaw, OrbitPitch
                    an ORBIT: a focus, and an eye a fixed distance away from it

SkyViewCamera + CameraPosition[3]   (EditorHost)
EditorCameraComponent               (PaintHost)
                    a FREE EYE: a position, and a direction it points
```

`ResolveViewportFrame` only accepts the orbit. An editor camera flying under WASD has no orbit distance and
no focus, so neither host could call the shared function, so each grew its own — and once you are writing
the frame you may as well write the projection under it, and once you have the projection you may as well
write the proxy drawing on top. **One missing conversion at the bottom produced ~250 duplicated lines
above it.**

I checked the arithmetic rather than assuming. `/tmp/alg.py` evaluates both perspective formulae over 100
samples:

```
worst disagreement over 100 samples: 2.910e-11 px
IDENTICAL FORMULA
```

The CAD form computes `Focal = (Height/2)/tan(fovV/2)` and multiplies; the editor form divides by
`tan(fovV/2)*aspect` and rescales by width. They are the same expression rearranged. The three hosts have
been computing *the same number by three routes*, which is why nobody noticed — and why, if one of them is
ever corrected, the other two will silently stay wrong.

> 🔴 **The fix is not "delete two copies". It is to give `ViewFrame` a second way to be constructed, so
> that a free-flying camera is a first-class citizen of the shared projection.** Delete the copies
> afterwards, as a consequence. If I only deleted copies, the next host with a free camera would write a
> fourth.

---

## 2. The steps

Each step ends green: unit compiles under `-Werror`, both hosts compile, 33 gates pass, commit, push.

### **Step A — `ViewFrame` from a free-flying eye** *(the root-cause fix)*

Add to `ViewportProjection`:

```cpp
/// 🧩 The frame a free-flying camera resolves to — an eye position and the direction it looks.
ViewFrame ResolveFreeViewFrame(const SpatialPoint& Eye, double YawDegrees, double PitchDegrees);

/// 🧩 Where a spatial point lands, seen from an explicit frame at an explicit vertical field of view.
bool ProjectThroughFrame(const ViewFrame& Frame, const PlaneExtent& Extent,
                         double FieldOfViewDegrees, const SpatialPoint& Position,
                         float& ScreenX, float& ScreenY);
```

`ProjectSpatialPoint` becomes a thin call into `ProjectThroughFrame` so there is exactly one perspective
divide in the codebase. **Proof obligation:** extend `ViewportProjectionProof` with a section that builds
an orbit frame and a free frame *describing the same camera* and demands they project every sample point
to the same pixel. That claim is the thing that stops the split reopening.

⚠️ Sabotage list decided in advance: negate pitch; swap yaw sign; drop the aspect term; use `Height` where
`Width` belongs; move the `CameraZ <= 0.01` refusal to `< 0`. Any of these scoring zero means the fixture
is degenerate — as it was in §7, where the focus sat on the plane origin.

### **Step B — `EditorHost`'s two definitions**

Delete `ProjectWorkspaceCodexPoint` outright; its call sites take `ProjectThroughFrame` with a frame from
`ResolveFreeViewFrame`. Then `RecordWorkspaceCodexProxy` is `RecordCodexSceneProxy` with a different frame
source — extend the `CodexSceneProxy` entry point to accept a `ViewFrame` rather than a
`ViewportStanding`, and have the parametric path pass `ResolveViewportFrame(...)` into it.

**Leaves `EditorHost` at 0 definitions.**

### **Step C — `SkyImage.cpp`'s `GenerateSkyImage`**

257 lines of atmospheric integration sitting under `Application/`. It reads an `AtmosphereIntegrator` and
writes pixels; nothing about it is host-specific. It moves to `SlateWorld` beside the atmosphere code it
already depends on. This is a file move plus an include repoint, not a rewrite.

**Leaves `EditorHost` at 0 files with debt.**

### **Step D — `ParametricSketchHost`'s five small ones**

`Attach` (~14 L), `ClearInspectorBridge` (~11), `SeedParametricWorkspace` (~15), `SeatParametricContext`
(~48), `SynchroniseParametricPresentation` (~46). `Attach` is *also* defined in
`InterfaceValidationHost` — same duplication shape as 10t, so it goes to
`SlateRuntime/Session/HostEnvironment` where the other twice-defined host plumbing now lives. The other
four are parametric-workspace seating and belong in a `SlateWorkspace` unit.

### **Step E — `SynchroniseCadPacket`, ~1 000 lines, containing `main()`**

⚠️ **`VerifyHostPartition` mis-attributes this one.** It reports a definition's span as running to the
*next* definition's start, and `main()` is not counted as a definition — so `SynchroniseCadPacket` is
credited with everything up to end of file, `main()` included. Its true length must be measured by braces
before anything is planned around it, exactly as `SynchroniseToolContext` had to be (reported 1 098, actual
97).

This step gets re-planned once measured. The lift itself is mechanical; the risk is entirely in the
mis-measurement.

### **Step F — the two loose ends**

- `DatumPlane = 27u` duplicates the `Workplane` branch in `ParametricToolsSpecification.h`. Remove the
  enumerator and its branch.
- `EditorHost` and `PaintHost` still include `SharedViewportHostBridge` for the orientation widget; repoint
  both to `OrientationCube` and delete the 675-line header. This is a **standing user instruction** — the
  `*Bridge*` classes must go — and it should not wait on step 11.

### **Step G — turn `--strict` on**

Once A–F land, `VerifyHostPartition --strict` becomes the gate's default rather than a flag, so the debt
cannot silently return. **This is the real exit of step 10**, and it is the thing I should have been
measuring against from the start.

---

## 3. Order, and why

**A first**, because B and D both depend on it and because it is the only step that fixes a cause rather
than a symptom. **C** is independent and cheap. **E** last of the lifts, because it is the only one whose
size is currently unknown. **F** any time; **G** only when the rest is green.

`PaintHost`'s three definitions are deliberately **not** in this plan. It is deleted at step 11, and
lifting code out of a file that is about to be removed is work that gets thrown away — except for
`ProjectPaintScenePoint`, which step A retires by making the shared projection able to express its camera.

---

## 4. Outcome — all steps green at `--strict`

```
[HostPartition] 5 host translation units
  [    ] ConsoleHost.cpp              exempt — presents no workspace
  [ok  ] EditorHost.cpp               1 585 lines · 0 defs · 0 mech · 0 pp
  [    ] InterfaceValidationHost.cpp  exempt — presents no workspace
  [ok  ] PaintHost.cpp                  621 lines · 0 defs · 0 mech · 0 pp
  [ok  ] ParametricSketchHost.cpp     1 216 lines · 0 defs · 0 mech · 0 pp

[HostPartition] totals — 0 definitions, 0 engine-mechanism, 0 preprocessor-gated, 0 unreachable
[HostPartition] hosts are lifetime and tick only
```

**Refusing is now the DEFAULT**, not a flag. A host that grows a definition back fails the build — verified
by adding one and watching the gate exit 1. `--permissive` remains for anyone mid-lift.

### ⭐ The measurement that nearly cost a week

`SynchroniseCadPacket` was reported by the gate as **1 013 lines** and had been carried in the plan as "the
last big one, ~1 000 lines, contains `main()`". Measured by braces it is **six**:

```cpp
Deliver<bool> SynchroniseCadPacket(const SketchStructure& Sketch,
                                   const WorkspaceRecordStructure& Records,
                                   WorkspaceCadPacket& Delivered)
{
    return ProjectSketchRendering(Sketch, Records, Delivered, {});
}
```

A pass-through to a unit function that already defaults its fourth argument. It was deleted, not lifted.
`Attach` was likewise reported as 85 lines and is 14. **The gate attributes everything from a definition's
start to the next definition's start, and `main()` is not a definition** — so the last function before
`main()` absorbs the whole of it. This is the second time that fiction has driven planning; the first was
`SynchroniseToolContext`, reported 1 098, actual 97.

### What the four defects had in common

| Found | Because |
|-------|---------|
| Editor content browser could not see mesh or image imports | `PopulateImportDirectory` existed twice and only one copy knew |
| Sun intensity did nothing to the sky | `-Wunused-variable` never reached code under `Application/` |
| `DatumPlane` was a second name for `Workplane` | two enumerators, one behaviour, no test comparing them |
| Editor scene proxies would have been 1 000× too far away | the metre/millimetre difference was a `constexpr` in a `.cpp` |

None was found by looking for defects. All four surfaced because code moved somewhere it could be
compiled, warned about, or compared against its twin. **The duplication was not the risk; it was the
mechanism by which the two copies were allowed to disagree.**

---

## 5. What the end-to-end build found that no syntax check could

Everything in §4 was verified per-file. That was not enough. Running the real pipeline —
`make sequence`, the same order as `Build/Construct.ps1` — refused at step 3:

```
[FAILED]  the unit graph holds a cycle among: Application, SketchToolset,
          SlateCompute, SlateRuntime, SlateUI, SlateWorkspace, TextureToolset
```

Step C moved `SkyDomeImage` into `SlateCompute` and added `SlateUI` to its `requires`.
`SlateUI`'s own link list already named `SlateCompute`. **Every one of the 33 gates passed and
every file compiled, because a cycle is a property of the graph, not of any translation unit.**
No amount of `-fsyntax-only` would ever have shown it.

The cause was a misplaced struct. `EnvironmentConfiguration` lived in
`SlateUI/Interface/SceneDirectoryPanel/`, but its nineteen fields are Rayleigh density, Mie
asymmetry, ozone, scale heights — atmospheric physics, not a panel. The sky evaluator needs it;
the panel only writes it. It now lives in `Shared/`, which both layers may read.

`ParametricWorkspaceBridge.h` fell to the same rule that killed `SharedViewportHostBridge.h`: a
unit was reaching **up** into `Application/` to compile.

| Guard | Now refuses when |
|---|---|
| `make sequence` | the unit graph gains a cycle |
| `VerifyPartition` | a unit includes what its `Module.toml` does not declare |
| `ValidateHostBuildBudgets` | either deleted bridge header reappears |

**The lesson, and it is the same one as `SynchroniseCadPacket`: a check that never runs the real
thing measures the wrong quantity.** Line counts were attribution, not measurement; per-file
syntax checks were compilation, not building. Both were confidently green while wrong.

Baseline honesty: 36 of 246 translation units still fail in this sandbox on the stub Vulkan
header and the absent ImGui submodule — **the identical count at `12e9a68e`, before this effort
started.** No new translation failure was introduced.
