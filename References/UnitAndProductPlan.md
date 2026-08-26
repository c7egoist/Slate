# Units And Products — Two Authoring Applications On One Runtime

Names adjudicated against `SKILL-Naming`, the unit stack settled, and the mess dismantled in order.

Written against `433ba66`. Supersedes `References/HostAndUnitCompositionPlan.md`.

---

## 1. The names, adjudicated

Every proposal passes the banned-word gate — `Framework`, `Workspace`, `Toolkit`, `Tooling`, `Toolset`,
`Runtime`, `World`, `Authoring`, `Texture`, `Parametric` are all absent from every banned list. Verified.
So the question is not legality but whether each name **states a boundary that excludes something**.

| Proposed              | Verdict | Reasoning                                                                                          |
|-----------------------|---------|------------------------------------------------------------------------------------------------------|
| `SlateWorld` ← `SlateScene` | ✓ accept | A scene is a view of a world. The 9 components (camera, atmosphere, light, transform) are world content, not a viewing arrangement |
| `SlateRuntime`        | ✓ accept | Names the tick — see §2, where the evidence for it is overwhelming                                   |
| `SlateWorkspace`      | ✓ accept | With the boundary in §2.1 stated. Without it, it collides with `SlateRuntime`                       |
| `SlateToolset`        | ✓ accept | Chosen over `Toolkit` and `Tooling`: a *set of tools* is countable and concrete; a *kit* is a marketing word and *tooling* is a process noun |
| `SlateShape` ← `SlateGeometry` | ✓ carry forward | Unchanged from the previous plan; `Geometry` excludes nothing              |
| `SlateFramework`      | ⚠️ **flagged** | See §1.1                                                                                       |

### 1.1 The one name I would push back on

`SlateFramework` is legal but sits in the same family as `Core`, `System` and `Module` — all three of which
your own rules ban as OO/AI tropes. The mechanism test asks what it *excludes*, and "framework" excludes
nothing; every unit here is arguably framework.

What is actually in there is two different things:

| Today               | Is                                                              | Better name  |
|---------------------|-------------------------------------------------------------------|--------------|
| `Engine/Foundation/`| 7 headers of declared guarantees — `DeliveryGuarantee`, `NumericTolerance`, `PrecisionGuarantee`, `Identity` | `Guarantee/` |
| `Engine/Shared/`    | 18 `.slang.h` predicates compiled by **both** toolchains          | `Shared/`    |

🔴 Neither is a link unit — no `Module.toml`, no archive. They are reached by every unit through the engine
root. Naming them `SlateFramework` would imply an archive that does not exist, and `VerifyPartition`
reasons about units by manifest. My recommendation: **rename `Foundation/` → `Guarantee/`** and leave
`Shared/` alone. Both stay header-only and outside the unit graph.

🔴 The CAD architecture document calls this folder `Contract/`, and that spelling is now **banned** — it
names a programming-language concept rather than a mechanism, since a contract is what every declaration
in this engine already is. `Guarantee` is the replacement and is already engine vocabulary:
`PrecisionGuarantee.h` carries it, as do 223 `tag guarantee` annotations. Where the CAD document says
`GeometryContract.h` / `TopologyContract.h` / `ReferenceContract.h`, read `…Guarantee.h`.

⚠️ If you want the `Slate*` prefix for consistency, `SlateGuarantee` as a real header-only unit works — but
it is a bigger change than it looks, because `Construct.ps1` special-cases `Foundation` and `Shared` by
name in its shader-freshness pass (lines 343, 705, 723). Your call; nothing else in this plan depends on it.

### 1.2 A collision worth knowing about

`Workspace` is already the busiest word in the tree — 10 existing modules use it:

```
SlateUI/Interface/WorkspacePanel          SlateVulkan/Device/WorkspaceOverlayPass
SlateVulkan/Device/WorkspaceCadPass       SlateVulkan/Device/WorkspaceScenePass
SlateDocument/Format/WorkspaceSceneActivation
SlateFeature/Feature/Workspace{Directory,Name,Property,Record,Revision}*
```

That is survivable — a unit named `SlateWorkspace` alongside a module named `WorkspacePanel` in `SlateUI`
is unambiguous in an include line. But it means the §2.1 boundary must be *written down*, or the next agent
will put a panel in `SlateWorkspace` and a workspace definition in `SlateUI`.

### 1.3 `Authoring` is now an application word

You have moved `Authoring` up to the product tier — `TextureAuthoring`, `ParametricAuthoring`. That
retires the previous plan's `SlateAuthoring` unit: a unit and a product sharing a word is precisely the
ambiguity this refactor exists to remove. `SlateFeature`'s contents redistribute per §2.2 instead.

---

## 2. The evidence: two products, one runtime

Measured at `433ba66`, and it settles the architecture on its own:

| Measurement                                                     | Result |
|---------------------------------------------------------------------|--------|
| References to sketch/constraint/profile in `PaintHost`               | **0**  |
| References to texture-paint/layer-stack/brush in `ParametricSketchHost` | **0**  |
| Identical `Lifetime.*` / `Viewport.*` seam calls shared by both      | **27** |

The two disciplines are **completely disjoint in features** and **completely identical in runtime**. That
is the whole argument: what differs is the workspace, what repeats is the tick. So the tick becomes a unit
and the workspaces become data.

### 2.1 The three new units, with boundaries stated

> **`SlateRuntime`** answers *"how does a product start, tick and stop?"*
> One bring-up, one tick, one teardown, one feature gate. This is the 27 identical calls, written once.
>
> **`SlateWorkspace`** answers *"what **is** a texture workspace / a parametric workspace?"*
> Which panels it seats, which tools it offers, which document it binds. Declaration, not execution.
>
> **`SlateToolset`** answers *"what is a brush / a line tool / an extrude tool?"*
> One tool's parameters, its pointer behaviour, and what it commits. Knows no panel and no tick.

🔴 The distinguishing test, to be applied when something is ambiguous: **if it runs every frame regardless
of discipline it is Runtime; if it differs between texture and parametric it is Workspace; if the artist
picks it up and puts it down it is Toolset.**

📝 `SlateWorkspace` is what makes "blank editor" free: a product offers workspaces, and offering zero
active workspaces is the empty shell that `InterfaceExchange::VacantPressed` already serves.

### 2.2 Where `SlateFeature` goes

`SlateFeature` is retired — the word now collides with build-time feature masks. Its 30 modules redistribute:

| Today                                    | Moves to                        | Because                                     |
|------------------------------------------|---------------------------------|---------------------------------------------|
| `Sketch/*` (20 modules)                  | `SlateShape/Sketch/`            | Exact 2D geometry and constraint solving — no device, no author, no undo |
| `Feature/{FeatureStructure,RecomputeScheduler,OccurrenceStructure}` | `SlateShape/History/` | Parametric feature history is shape provenance |
| `Reference/*` (5 modules)                | `SlateShape/Reference/`         | Provenance and pick classification are shape identity |
| `Feature/Workspace*` (5 modules)         | `SlateDocument/Document/`       | These are document projections, misfiled     |

⚠️ Note `Sketch/*` lands in `SlateShape`, **not** `SlateWorkspace`. A constraint solver is exact geometry;
it would still be correct in a file with no undo stack and no panels. The *sketching workspace* — which
panels, which tools, which snap configuration — is what goes in `SlateWorkspace`.

---

## 3. The stack

```
Application/     AuthoringHost  →  TextureAuthoring.exe, ParametricAuthoring.exe   (one source, two products)
      │          ConsoleHost, InterfaceValidationHost
      │
SlateWorkspace/  ←NEW  TextureWorkspace, ParametricWorkspace — panels, tools and document a discipline binds
      │
SlateToolset/    ←NEW  brush, line, arc, extrude, constrain — one tool's parameters and pointer behaviour
      │
SlateRuntime/    ←NEW  bring-up, tick, teardown, feature gate — the 27 calls, written once
      │
SlateUI/               panels, widgets, the one copy of ImGui
SlateWorld/      ←REN  (was SlateScene) camera, atmosphere, light, transform components
SlateCompute/          depots, rasters, integrators — device-side execution
SlateVulkan/           the device edge
SlateDocument/         occupants, revisions, selection, formats
SlateShape/      ←REN  (was SlateGeometry + SlateFeature) exact shape, topology, sketch, history
SlateMath/             numerics, exact predicates
      │
Guarantee/       ←REN  (was Foundation/) header-only declared guarantees   ⚠️ §1.1
Shared/                dual-toolchain predicates                       header-only, unchanged
```

**Products**

| Product                | Features                            | Links                                    |
|------------------------|-------------------------------------|------------------------------------------|
| `TextureAuthoring`     | World │ Codex │ Texture             | no `SlateShape/Sketch`, no parametric tools |
| `ParametricAuthoring`  | World │ Codex │ Parametric          | no texture panels, no impression path    |

📝 A combined build — both masks set — remains available as a third variant at **zero additional code**,
since it is one more line in `[variant]`. You said two products; the design does not foreclose the third.

---

## 4. The order of work

Explicitly what you asked for: one step at a time. Each step is a commit that leaves the tree building.

🔴 **Deletion is step 11, not step 1.** `ParametricSketchHost` holds 5 981 lines and 138 local function
definitions that exist nowhere else. Deleting before lifting destroys real work — that is the mistake this
whole exercise is recovering from.

| # | Step                                                                                                     | Risk | Leaves                                        |
|---|--------------------------------------------------------------------------------------------------------|------|-----------------------------------------------|
| 1 ✅| Publish the capability-ownership table into `AgenticInstuctions/` — camera, sky, grid, sketch already have owners | ✔️ | The next agent cannot claim ignorance     |
| 2 ✅| Wire the build seam: `[variant]` in `Module.toml`, `/D$Define` in `Construct.ps1`, `#error` on the missing-macro branch | ✔️ | The `#ifdef` mechanism is live for the first time |
| 3 ✅| Add `VerifyHostPartition.py` in **warn-only** mode                                                       | ✔️  | The debt measured, nothing changed            |
| 4 ✅| `git mv SlateScene SlateWorld` + include rewrite — smallest rename, proves the procedure                 | ✔️  | 9 modules moved                               |
| 5 ✅| `git mv SlateGeometry SlateShape` + include rewrite (**34 files**)                                        | ✔️  | Nothing depended on the name                  |
| 6 | Fold `SlateFeature` into `SlateShape` per §2.2; move the 5 `Workspace*` modules to `SlateDocument` (**75 files**) | 🚩 | `SlateFeature` gone; the word freed       |
| 7 | Create `SlateRuntime`. Lift the 27 identical seam calls out of `EditorHost` into one tick                | 🚩  | One tick loop exists                          |
| 8 | Create `SlateToolset`. Lift tool behaviour from both hosts, one tool per commit                          | 🔴  | Tools reachable from any product              |
| 9 | Create `SlateWorkspace`. Define `TextureWorkspace` and `ParametricWorkspace` as declarations             | 🚩  | Disciplines are data                          |
| 10| Lift what remains of `ParametricSketchHost`'s 138 locals — **one behaviour per commit**, `ConsoleHost` covering each | 🔴 | Nothing is left in the host but `main()` |
| 11| **Delete** `PaintHost/`, `ParametricSketchHost/`, the 5 `Application/Api/*Bridge*.h`, and dead `SkyImage.cpp` | ✔️ | The duplication is physically gone       |
| 12| Rename `EditorHost/` → `AuthoringHost/`; both products build from it                                     | ✔️  | Two products, one source                      |
| 13| Validators warn → **fail**                                                                                | ✔️  | Rules enforced, not documented                |

⚠️ **Steps 1–5 are worth doing immediately.** They are mechanical, reversible, and after step 2 the
codebase begins refusing the mistake instead of inviting it. Step 10 is the real work.

### 4.1 On deleting the `Bridge` headers

You are right that they must go — `Bridge` is a banned word, and all five are only reachable from the three
hosts, so deleting the hosts deletes them. But 675 lines of `SharedViewportHostBridge.h` is **not** all
disposable. Before step 11, its contents split three ways:

| Content                                                                    | Destination            |
|----------------------------------------------------------------------------|------------------------|
| Feature mask + `HasFeature` (lines 30–88)                                   | `SlateRuntime` — this is the seam being repaired |
| Orientation gizmo, CAD cube, axis projection, hit tests (lines 110–520)     | `SlateUI` — presentation with a hit test is a panel |
| `ResolveEngineContentRoot`, codex activation, scene centring (lines 528–675)| `SlateDocument`        |

🔴 Deleting the file before this split loses the orientation-gizmo hit testing, which nothing else
implements. Split first, delete second — same discipline as step 10.

---

## 5. Gates

| Gate                                                                              | Enforced by                     |
|-----------------------------------------------------------------------------------|---------------------------------|
| Every product macro is defined; an undefined one fails the compile                 | `#error` in `HostFeature.h`     |
| A host source declares no function beyond `main()`                                 | `VerifyHostPartition.py`        |
| `SlateShape` names no unit — `[requires] unit = []`                                | `VerifyPartition.ps1`           |
| `SlateRuntime` names no `SlateWorkspace` and no `SlateToolset` — the arrow points down | `VerifyPartition.ps1`       |
| `SlateToolset` names no panel                                                      | `VerifyPartition.ps1`           |
| Both products link in CI; a gated symbol absent from a product is a link error     | `Construct.ps1` with no `-Product` |
| `#ifdef` only around includes and members; logic uses `if constexpr`               | `VerifyHostPartition.py`        |
| Dead source — a `.cpp` no target compiles — fails the build                        | `Construct.ps1` reachability    |
| No banned word enters a new folder, file or type name                              | `VerifyNaming.ps1`              |

📝 The dead-source gate would have caught `SkyImage.cpp`, which sat unreferenced in `EditorHost/Source/`
long enough that an agent read it, assumed sky was unimplemented, and wrote a second one.

---

## 6. Open

| Question                                                                       | Blocks        |
|--------------------------------------------------------------------------------|---------------|
| `SlateFramework` vs `Guarantee/` + `Shared/` staying header-only — §1.1         | Not started   |
| Whether a combined both-disciplines variant ships as a third product            | Nothing       |
| Whether `InterfaceValidationHost` becomes a variant or stays a subject          | Nothing       |
| Whether direct polygon authoring gets its own workspace or extends parametric   | Step 9        |
