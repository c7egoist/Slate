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
| `Feature/{FeatureStructure,RecomputeScheduler,OccurrenceStructure}` | `SlateShape/Sequence/` | The feature DAG and its recompute order — an ordered workflow |
| `Reference/*` (5 modules)                | `SlateShape/Reference/`         | Provenance and pick classification are shape identity |
| `Feature/Workspace*` (5 modules)         | `SlateShape/Record/`            | 🔴 Corrected during step 6 — see below       |

🔴 **Two corrections made when step 6 was executed, both found by reading the source rather than the plan.**
First, `History` is a banned structural word, so the feature-order folder is `Sequence/` — an ordered
workflow, which is what the role means. Second, the `Workspace*` modules could **not** move to
`SlateDocument`: `WorkspaceRecordStructure` names `ConstraintSpecification`, `DimensionSpecification` and
`ReferenceSpecification`, so promoting it above `SlateShape` would point that edge backwards and make the
unit graph cyclic. They live in `SlateShape/Record/` instead. The three groups were mutually entangled —
Feature named Reference and Sketch, Reference named Feature, Sketch named both — so no cut through them
was acyclic, which is itself the evidence that the boundary they were separated by did not exist.

⚠️ Note `Sketch/*` lands in `SlateShape`, **not** `SlateWorkspace`. A constraint solver is exact geometry;
it would still be correct in a file with no undo stack and no panels. The *sketching workspace* — which
panels, which tools, which snap configuration — is what goes in `SlateWorkspace`.

---

## 3. The stack

```
Application/     AuthoringHost  →  TextureAuthoring.exe, ParametricAuthoring.exe   (one source, two products)
      │          ConsoleHost, InterfaceValidationHost
      │
SlateWorkspace/  ✅NEW  Texturing, Sketching, Combined, Vacant — the panels and tools a discipline declares
      │
SketchToolset/   ✅NEW  line, arc, rectangle, circle — one shape and how it is placed
TextureToolset/  ✅NEW  deposit, erase, smudge, clone — skeleton; the impression machinery already exists
      │
SlateRuntime/    ✅NEW  bring-up, tick, teardown, feature gate — SessionSequence, the 26 calls written once
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
| 6 ✅| Fold `SlateFeature` into `SlateShape` per §2.2 (**70 files**). The 5 `Workspace*` modules went to `SlateShape/Record/`, **not** `SlateDocument` — see §2.2 note | 🚩 | `SlateFeature` gone; the word freed |
| 7 ✅| Create `SlateRuntime`. Lift the shared seam out of `EditorHost` and `PaintHost` into one tick — **26** identical calls, not 27; see §4.2 | 🚩 | One tick loop exists; `PaintHost` holds **zero** `Lifetime.*` calls |
| 8 ✅| Create `SketchToolset` + `TextureToolset`. Placement machine lifted; duplicated enum and cast bridge deleted; 22 subjects split into 13 shapes × 5 methods — see §4.3 | 🔴 | One vocabulary, no redundant tools |
| 9 ✅| Create `SlateWorkspace`. Four declarations replace three arrangement procedures — see §4.4 | 🚩 | Disciplines are data |
| 10 ◐| Lift what remains of `ParametricSketchHost`'s 138 locals — **one behaviour per commit**, `ConsoleHost` covering each | 🔴 | Nothing is left in the host but `main()` |
| 11| **Delete** `PaintHost/`, `ParametricSketchHost/`, the **4** `Application/Api/*Bridge*.h`, and dead `SkyImage.cpp` | ✔️ | The duplication is physically gone       |
| 12| Rename `EditorHost/` → `AuthoringHost/`; both products build from it                                     | ✔️  | Two products, one source                      |
| 13| Validators warn → **fail**                                                                                | ✔️  | Rules enforced, not documented                |

⚠️ **Steps 1–5 are worth doing immediately.** They are mechanical, reversible, and after step 2 the
codebase begins refusing the mistake instead of inviting it. Step 10 is the real work.

### 4.2 What step 7 actually found

🔴 **The count was 26, not 27, and it was measured rather than trusted.** Re-derived across all three
hosts at step 7: `EditorHost` names 28 distinct seam methods, `PaintHost` 26, `ParametricSketchHost` 28,
and the intersection of all three is **26**. The plan's 27 was close enough to act on and wrong enough to
record.

`SlateRuntime/Session/SessionSequence` now owns that intersection — bring-up, the tick prologue, the seal,
the submit and the teardown, plus the appearance reconciliation all three hosts had copied. Measured after:

| Host         | Lines           | `Lifetime.*` calls |
|--------------|-----------------|--------------------|
| `PaintHost`  | 843 → **676**   | 13 → **0**         |
| `EditorHost` | 1895 → **1761** | 28 → **14**        |

`EditorHost` keeps 14 because it owns a device estate the session cannot know about — the atmosphere
surface, the overlay pass, the geometry exchange. Those are reached through `Session.Device()` and rebuilt
on the tick that reports `DeviceRebuilt`. That is the correct residue, not leftover debt.

🔴 **Two defects were found by verifying rather than by reading.**

① `Engine/SlateWorld/Module.toml` declared `subject = [ "Scene" ]`, a folder step 4 had renamed to
`World/`. A StaticLibrary archives its whole tree and ignores `subject`, so nothing failed — but
`Get-UnitSource` throws `"declares subject Scene but ... does not exist"` the moment anything reads it.
Corrected, and every manifest's subject is now checked against the filesystem.

② **`Application/Api/HostFeature.h` was included by nothing.** The macro reached it from the build and the
`#error` branch worked, but no host had ever included the header — so the whole feature seam was still
dead, which is *precisely* the failure §2 of `CAPABILITY-Ownership.md` records as having caused an agent
to write its own camera, sky and CAD editor. `EditorHost` now includes it and states `HostProduct` at
bring-up, so the seam is demonstrably read by the surviving source.

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

### 4.3 What step 8 actually found

Two defects, both invisible to every validator that existed, and both found only by reading the two
declarations side by side rather than trusting either.

**The duplicated enumeration.** `SharedCadDraftSubject` in `Application/Api/` and `ParametricDraftSubject`
inside `ParametricSketchHost.cpp` were the same 22 members with the same 22 ordinals, and they were
reconciled at three call sites by `static_cast<X>(static_cast<std::uint32_t>(Y))`. That cast is correct only
while both lists stay in the same order. Nothing checked that they did, nothing would have reported it, and
adding one member to either side would have silently mapped every later subject to its neighbour — a
polygon tool that drew slots. Both are now one `DraftSubject`, and `ValidateHostBuildBudgets` fails if a
host re-declares it or if either cast returns.

**The anchor-count table meant two different things at once.** `SharedCadDraftRequiredAnchors` was only ever
READ for the four terminated curves. For the other seventeen subjects it was decoration, and being unread,
it had drifted into inconsistency:

| Subject                        | Table said | Anchors stored | Presses | The column meant |
|--------------------------------|-----------|----------------|---------|------------------|
| `DiameterCircle`, `Polygon`    | 2         | 2              | 2       | anchors stored   |
| `Rectangle`, `CenterRectangle` | 2         | 1 + live hover | 2       | presses made     |
| `Circle`, `Ellipse`            | 1         | 1 + live hover | 2       | neither — wrong  |
| `Point`                        | 1         | 0 + live hover | 1       | neither          |

🔴 **And that inconsistency was hiding a live bug.** `DiameterCircle` and `Polygon` sat in the pointer branch
that commits on the second press *without storing it* — the branch written for `Rectangle`, which reads its
final corner from the live hover. But `CommitDraft` requires **two stored anchors** for both and never reads
the hover, so both subjects reached `CommitDraft` holding one anchor, failed every guard, and returned a
refusal the caller discarded. **Both tools did nothing at all when drawn with the mouse.** They worked only
via Enter, whose branch appends the hover as a real anchor first. The keyboard path masked the pointer path.

The fix is not a special case: `Required` now counts anchors taken, for every subject, and the placement
stores every point it is given. `Circle` and `Ellipse` are therefore `2`, not the `1` the deleted header
stated — three rows deliberately disagree with the original, and `DraftPlacementProof` §1 asserts those
three exceptions individually so the correction stays a checked decision rather than a transcription slip.

**The third defect: nine tools that were four shapes.** `CenterRectangle`, `ThreePointRectangle` and
`Rectangle` were three separate tools that all ended in the same `DeclareRectangle`; the three circle
spellings all ended in `DeclareCircle`. The geometry layer only ever knew four shapes where the tool layer
claimed nine. They were never different shapes — they were different ways of POINTING at one, and the
enumeration had no axis for that, so it grew a member per combination.

The fix is two axes: `SketchSubject` (13 shapes, each one something `SketchStructure` can declare) and
`PlacementMethod` (Extent, Centred, ThreePoint, Diameter, Tangent), with `AcceptedBy` stating which pairs
exist. 21 retired subjects become 27 distinct, individually named placements — and `SketchPlacementProof`
§2 **fails if any two placements share a name**, which is the redundancy rule made executable. It caught a
real one during the split: `Polygon` was reachable under both `Extent` and `Centred`, and a polygon is a
centre and a circumradius with no spanned form, so `Extent` now refuses it.

⚠️ `Parabola` and `Hyperbola` were considered and **rejected**: `CurveSpecification` models no conic beyond
the ellipse, so offering them would be a tile the geometry cannot honour — the same defect as the retired
`DiameterCircle`. They arrive when `SlateShape` grows `DeclareParabola`, not before.

**Measured.** `DriveDrawingWithModifiers` 181 → 76 lines; the 7-arm subject branch replaced by one
`Anchor` → `Seal`. A second copy of the machine, `DriveDrawing` at line 2072, was **never called** and was
deleted with its 67 lines. `ParametricSketchHost` 5 981 → 5 757. `SketchToolset` is 533 lines;
`TextureToolset` is a 155-line skeleton that declares the texturing tool vocabulary and no behaviour, so
that the second copy the sketch side suffered is never written.

📝 `SlateToolset` is the first unit that names no device, no window and no vendor header, so it **links and
runs** in a sandbox with no Vulkan SDK. `Tools/SketchPlacementProof/` executes it — 880 claims across the
split equivalence, name-uniqueness across every legal pair, per-pair completion counts, refusal of
impossible pairs, method carriage through sealing, terminated curves, dimension snapping and refusals. It is the only gate in the repository that runs engine
code rather than parsing it, and it was negative-tested with four separate sabotages before being trusted.


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

### 4.4 What step 9 actually found

A workspace was never written down. Each host CONSTRUCTED its arrangement in code:
`ConstructParametricLayout` ran eleven statements against `PanelStructure` — divide, read back the record,
seat, divide again, seat, seat, proportion, proportion — with a `Deliver` check between each pair and every
call wrapped in `Discard`. The texturing host seated a bare viewport. The combined editor seated a bare
viewport and grew the rest one panel at a time. Three procedures, no declaration, and no way to answer
"what does the sketching discipline seat?" without running one of them.

🔴 **Every one of those calls was `Discard`ed**, so a step that refused was skipped in silence. A partition
that came out with one panel missing looked exactly like one that came out right. `ApplyWorkspace` returns
the first refusal instead, naming the step that failed.

**The arrangement is now data** — `WorkspaceDeclaration` is a fixed array of `ArrangementStep`, sized
against `PanelStructure::RecordLimit` rather than a number chosen locally, so the two cannot drift. Four
declarations: Texturing, Sketching, Combined, Vacant. The combined product **reuses the sketching
arrangement** and only sets a second tool flag, which is §3's "third product at zero additional code" made
real — and `WorkspaceDeclarationProof` §3 fails if anyone later gives it its own arrangement.

⚠️ **The risk of this step was entirely in the slot ordinals.** A declaration says "seat the directory at
slot 1", and slot 1 is the right slot only if `PanelStructure::Divide` allocates the way the declaration
assumes — which is unprovable by reading, because the allocator belongs to a class this unit does not own.
So the proof links the **real** `PanelStructure`, applies the declaration, and compares the result slot for
slot against the transcribed procedure: same panels, same divisions, same 0.27 and 0.33. If `Divide` ever
changes how it hands out ordinals, every arrangement fails at once, which is the correct blast radius.

📝 The proof earned its keep immediately: the first texturing declaration divided the root but never seated
the layer stack, so it produced `[Vacant, Viewport]`. Reading the declaration, that looks right — the bug is
that `Divide` leaves the vacant side where the declaration expected the panel. Only running it showed the
`Vacant`.

### 4.5 What step 10 has found so far

**10a — 195 abandoned lines, and a detector that lied.** Five file-local functions were defined and never
called: `RecordViewportGrid` (60), a three-argument `RecordCadFallback` overload shadowed by the six-argument
one actually used (70), `AppendOverlayArrow` (25), `AppendOverlayArc` (23), `AppendOverlaySquare` (12).

🔴 **The first `-Wunused-function` run reported zero on a host holding five dead functions.** `ParseHosts`
compiles with `-fsyntax-only`, which stops the compiler before the analysis that finds unreachable
definitions — the warning is accepted, never fires, and the report is empty. That is the fourth detector in
this refactor to report zero because it was broken. It is now tested: a known-dead probe is inserted and the
report must name it. `ParseHosts` gained an `abandoned()` check that compiles for real, and it is
negative-tested.

**10b — 119 definitions of nine functions, in 21 files.** `LengthSquared`, `Dot`, `Cross`, `Scaled`,
`Negated`, two `Added` overloads, `Difference` and `Normalize` had been copied byte-for-byte into the
anonymous namespace of nearly every translation unit in the CAD kernel. They now live once, in
`CurveSpecification.h`, beside the `SpatialPoint` and `SpatialDirection` they operate on — **no new unit and
no new role suffix**, because arithmetic on a type belongs with the type.

⚠️ **The sweep is all-or-nothing per file.** A file that kept a local copy AND included the header would
find both by overload resolution — the anonymous-namespace copy and the one found by argument-dependent
lookup are equally good candidates — so every call becomes ambiguous. Verified with a minimal probe before
touching anything, which is why this was one commit and not twenty-one.

📝 Two things were deliberately NOT folded. `FacetPanel::Scaled(float, const ThemeProfile&)` shares only the
name — it scales a figure by display scale. And `Difference(Left, Right)` returns the direction **from** Left
**to** Right, which reads backwards against its name; it is preserved exactly, and pinned by a claim, because
15 files depend on it.

The fold has a wide blast radius by design: a wrong sign is now wrong everywhere at once rather than in one
file. `SpatialArithmeticProof` therefore states the arithmetic independently of the implementation — `Cross`
is checked by perpendicularity as well as by components — and is negative-tested with three sabotages.
