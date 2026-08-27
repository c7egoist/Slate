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

**10c — the hosts stop building arrangements.** `ConstructParametricLayout` is deleted and its three call
sites apply `DeclaredSketchWorkspace`. The other two hosts had a quieter version of the same defect: each ran
`Register(subject)` on one line and hand-built a partition on another, with **nothing tying the two
together** — a host could register one discipline and seat another's panels and still compile.
`DeclaredWorkspaceFor` is that join.

🔴 **`PaintHost` is deliberately NOT wired to its own subject.** Routing it through
`DeclaredWorkspaceFor(Painting)` would seat the texturing arrangement, which includes a layer-stack panel —
and the only panel subject that host renders is `Viewport`. It would have drawn a blank hole. It applies the
blank workspace explicitly, with the reason recorded at the declaration. That is the **second** time this
refactor that moving a host onto shared code would have silently changed what the artist sees; checking
which panel subjects a host can actually render is now part of the procedure.

**10d — the transform keyboard grammar, lifted and tested for the first time.** G/R/S start a move, rotate
or scale, X and Z restrict to an axis, digits type an exact amount, backspace walks back out, and a second G
slides along the curve. Eleven file-local functions, ~200 lines, now
`SlateWorkspace/Discipline/TransformSequence`.

🔴 **It had never been tested, because it could not be.** Interleaved with viewport drawing, it needed a
Vulkan device to reach. `TransformSequenceProof` is the 60 claims that were owed, and it found three
behaviours worth pinning rather than correcting: a minus sign is taken mid-run (so `strtod` truncates rather
than the keystroke being refused); two G in one frame count as a double tap (a slow frame must not break the
gesture); and **the reader sees characters, not words — a stray `r` in ordinary text starts a rotate**. That
last one is safe only because the viewport and a text field never both receive a frame, a constraint the
host stated nowhere.

⚠️ The host keeps its 93 use sites by holding ONE `TransformStanding` and exposing the five old names as
accessors onto it. There is no second copy — a host copy and a unit copy would drift the first time one was
written and the other was not. Folding the fields caught a real defect in the process:
`sizeof(Transform.Numeric())` on an accessor is `sizeof(char*)`, so the extent now comes from the grammar's
own `TransformNumericLimit`.

Negative-tested with four sabotages: retracting the restriction before the digits, letting X restrict a
rotation, widening the tap window, and letting a bare X restrict with no manner standing.

`ParametricSketchHost` 5 757 → **5 268** across step 10 so far.

**10e — the viewport projection, and TWO SHIPPED DEFECTS it exposed.** Where a point on the sketch plane
lands on screen, and which point a screen position names: nine functions and four types, now
`SlateWorkspace/Discipline/ViewportProjection`.

🔴 **A projection can only be checked against its own inverse.** Screen coordinates are meaningless on their
own — no reader can say whether 412.7 is the right pixel — but *project this point, unproject the result,
get the point back* is checkable without knowing the formula, and it fails the moment either direction
disagrees. The round trip immediately reported **107 failures**, and two of them were real:

1. **The ray-plane distance was negated.** `Difference(A, B)` returns the direction FROM A TO B, so it
   already points from the eye towards the plane; the shipped `-Dot(...)` inverted every distance. A plane
   240 units in *front* resolved to −240 and was refused as behind the viewer. **Clicking anywhere in a
   perspective viewport could not place a point.**
2. **An orthographic view was refused on a negative distance** — but only a perspective view has an eye for
   something to be behind. A parallel projection's ray runs both ways. This rejected roughly half the
   isometric orthographic viewport.

⚠️ **Both hid for the same reason.** The six axis-aligned orthographic views build their ray origin from the
focus, which lies ON the sketch plane, so the numerator is exactly zero and the sign cannot show. Only the
perspective and isometric views put the origin genuinely off the plane. A defect that is invisible in 6 of 8
cases is exactly what a round-trip catches and inspection does not.

📝 The other 102 failures were **my test being wrong, not the code**: Front, Back, Left and Right look ALONG
the sketch plane, so their rays are parallel to it and there is no inverse to check. Refusing is correct, and
the claim now asserts the refusal is confined to views that are actually edge-on.

⚠️ `ResolveSketchBasis` is the one function here that reads a `SketchStructure`, and it lives in a separate
`SketchBasis.h`. Declaring it beside the projection would make everything that projects a point drag in the
whole sketch kernel to link — which is exactly what happened, and cost a round of chasing undefined
references before the seam was drawn properly.

Negative-tested with five sabotages, including reinstating both original defects. A sixth attempt was caught
by `-Werror=unused-parameter` before the proof could even run.

**A tenth duplicate surfaced while doing this:** `RotateAroundAxis` existed in **ten copies across five
different spellings** — some calling `Negated`, some subtracting members by hand, some inlining the dot
product — all computing the same Rodrigues rotation. Folded into `CurveSpecification.h` beside the others and
pinned by six new claims. `DimensionSolver` also gave up a 28-line function that nothing had ever called.

`ParametricSketchHost` 5 757 → **5 022** across step 10.

**10f — writing a sketched thing into the workspace directory.** Five functions that differed in exactly
three places — the subject the record carries, the folder it files under, and the payload member set —
with the folder lookup and the naming written out five times around them. Now
`SlateWorkspace/Discipline/RecordDeclaration`.

📝 Kept as five named entry points rather than collapsed into one call taking a subject. The payload differs
per subject, so a single entry point needs a variant or five optional arguments — a worse statement of the
same thing, and one that would let a caller declaring a dimension hand over a curve. The repetition that
actually mattered, the folder lookup, is now written once.

🔴 **The proof corrected me twice, and neither correction was in the code.** The bench declared no sketch
plane, so every area query *refused* rather than returning nothing, and seven claims failed for a reason
unrelated to what they tested. Then, having fixed that, my claim that an upright square declares no profile
turned out to be simply wrong: **closure is topological, not areal.** Curves chain end to end and the loop
closes when the chain returns to its start; signed area is computed afterwards to sort loops and identify
holes, never to decide whether a loop exists. An upright square declares a profile with zero area. The same
round corrected a comment I had just written — the `0.05` is a **closure tolerance**, the largest gap two
curve ends may leave and still be treated as joined, not a minimum area. Two claims now pin it from both
sides: 0.03 of slack closes, 0.5 does not.

⚠️ **Negative testing found a hole in the proof, not in the unit.** Six sabotages, five caught. The survivor
removed the top-level restriction from the folder lookup — and the proof stayed green, because the nested
folder in the test happened to have a higher index than the top-level one, so a forward walk reached the
right answer by accident. Two claims were describing declaration order rather than the rule. The
discriminating case is a category whose *only* folder is nested: it must resolve to nothing, so a new record
files at the top level instead of burying itself in somebody's subfolder. With that added the sabotage is
caught.

The claim worth keeping is the undo cost: closing a rectangle is one action and seals **one** revision
listing every profile it wrote. One revision per profile would make the artist press undo four times to walk
back one gesture.

`ParametricSketchHost` **5 022 → 4 915**. 90 claims. Thirteen gates.

**10g — the mouse drew in the wrong place, and it was a unit mismatch.** Reported as "in the parametric
sketcher the mouse was inverted or something, so when I drew it drew in the wrong place".

🔴 **A viewport lives in two kinds of pixel and they are not the same number.** *Logical points* are what
ImGui reports — `io.MousePos`, every `PlaneExtent` a panel hands back — and are the space the artist points
in. *Physical pixels* are what the swapchain is made of, and are the space the image is drawn in. On an
unscaled display the two are equal and every confusion between them is invisible.

`ResolveCadProjection` built its screen mapping from a `PlaneExtent` in **logical** points and handed the
shader a `DisplayWidth` in **physical** pixels. The shader divides one by the other to reach clip space, so
at any display scaling other than 100% the drawn geometry is out by exactly the scale factor. At 150% a
point the picker placed at x=1200 was drawn at **x=800** — four hundred points from the cursor that placed
it. The scissor had the same mismatch from the other side: a logical rectangle clamped against a physical
width, a clamp that never fires and silently clips the wrong region.

⚠️ **This is not the same defect as step 10e's two.** Those made the viewport *refuse* a click; this one
*accepts* it and draws the result somewhere else. Fixing the first pair would not have fixed this, which is
why the report was worth re-diagnosing from scratch rather than assuming it was already covered.

The new `DrawableScale` exists so the conversion cannot be forgotten — a bare `float` width carries no
indication of which pixel it is, which is exactly how the two got mixed.

**Workplanes, and why drawing worked without one.** The `Workplane` tool button has existed in the panel
since the toolset was written; the host never handled it, so pressing it did nothing. `ParametricToolSubject`
already reserved both `Workplane` and `DatumPlane`.

📝 A workplane is an origin plus an orientation — which is exactly "put an empty somewhere and draw on the
grid through it". That is not a workaround, it IS what a workplane is, and the new
`SlateWorkspace/Discipline/WorkplaneStanding` says so directly.

🔴 **A sketch with no workplane still draws.** The ground plane through the world origin is the standing
default and is adopted on the first placement. Demanding a declared plane before anything can be tried is
the most common complaint about parametric sketchers, and the default is now a proved claim rather than an
accident of one `if`.

🔴 **A placed plane faces the viewer**, which is what makes drawing in screen space work — a plane seen
edge-on projects to a line and anything drawn on it lands nowhere near where it was drawn. Which direction
becomes "along" matters too: the world axis that survives projection onto the plane best, so the grid does
not arrive rolled and does not swing as the view turns.

⚠️ **Negative testing found a hole in the workplane proof.** Seven sabotages, six caught. The survivor
reversed the cross product deriving `Across` and all 286 claims stayed green — a round trip uses the same
flipped axis in both directions and cannot see the flip, even though the sketch would be **mirrored**
against the world. Handedness has to be asserted against a direction written out by hand. Exactly the class
of bug originally reported, and the proof would have missed it.

`ParametricSketchHost` 4 915 → **5 014** — this step ADDS a feature rather than lifting one. 37 + 293 new
claims. Fifteen gates.

**10h — what the artist just pointed at.** Fifteen functions, `SlateWorkspace/Discipline/SketchPicking`.

🔴 **The pick priority is the whole design.** A point, a spline control and a curve can all be within reach
of one cursor position, and picking the nearest of the three is *wrong*: a curve passes through every one of
its own endpoints, so at an endpoint both sit at distance zero and a distance comparison could hand back the
curve. The artist would never grab the end they were aiming at. The order is **point, then control, then
curve** — smallest target first — each answered at its own full tolerance rather than competing.

📝 **The proof corrected me on deduplication.** I claimed two lines meeting at a corner share that corner
and it must be collected once. Wrong — each curve names its *own* endpoints, so the corner is point 258 of
one line and point 513 of the other. They are coincident but genuinely distinct subjects, and both must
move, or dragging one line would silently drag its neighbour's end with it. The uniqueness guard is for a
subject reachable by two **routes**: a curve collected directly and again as part of a profile containing
it. That is what the section now proves.

⚠️ **One guard was added rather than transcribed.** `ResolveRecordForPoint` and `ResolveRecordForCurve`
matched on `IssuedIndex` with no check that the incoming name was assigned. An unassigned name is index
zero, and every record carrying no point also holds zero — so an unassigned name resolved to the first
folder or dimension in the directory, handing back a record as the owner of something that does not exist.
Both refuse now.

**An eleventh duplicate:** the host's `ResolvePlanarCoordinates` was character-for-character the projection
unit's `ResolvePlaneCoordinates`, differing only by an unconditional `return true` no caller could use.

`ParametricSketchHost` 5 014 → **4 675**. 48 claims, seven sabotages caught.

**10i — the drag itself.** Nine functions, `SlateWorkspace/Discipline/TransformSession`. This is the other
half of 10d: that unit reads the keyboard grammar, this one moves the points it names.

🔴 **A session replays from the original positions; it does not accumulate onto the live geometry.** Every
frame applies one transform to where the subjects *started*. That is what makes a long drag exact — a
hundred frames of dragging is one transform, not a hundred stacked roundings — and it is what makes
cancelling free, because putting the originals back is all there is to do.

⚠️ **Three of eight sabotages survived the first proof, and the proof was at fault.** The fixture stood a
line symmetrically about the world origin, where rotating about the pivot and rotating about the origin are
the same operation, and used round coordinates a "restore to three decimals" implementation reproduces
perfectly. A degenerate fixture makes a broken implementation look correct. Two sections were added — one
standing the line off-origin at 10..50, one beginning a scale exactly *on* the pivot where the start radius
divides by zero — and all eight sabotages are now caught.

`ParametricSketchHost` 4 675 → **4 296**. 53 claims.

**10j — the handles.** `SlateWorkspace/Discipline/TransformGizmo`.

🔴 **A real defect, the same class as the placement bug: the hit test and the drawing were measured in
different units.** `ResolveGizmoHandle` tested a **44-pixel** axis; `RecordViewportGizmo` drew a
**78-world-unit** shaft with a cone at 102 and scale boxes at 94, then projected them. Those agree at one
zoom level and nowhere else — measured:

| OrthoScale | drawn shaft | hit-test reach |
|---|---|---|
| 0.5 | 39 px | 44 px |
| 1.0 | 78 px | 44 px |
| 4.0 | 312 px | 44 px |
| 32.0 | 2 496 px | 44 px |

Zoomed in the arrow ran seven times past its own hit box, so the artist pointed at the arrowhead and
grabbed nothing; zoomed out the arrow was smaller than its hit box and clicking beside it still grabbed.
A gizmo is a **screen-space control** and must be a constant size in pixels however far the camera stands
off. There is now one table of pixel measurements that both halves read, and the drawing converts through
`GizmoWorld` at the pivot.

🔴 **The reaches were made disjoint, which is a stronger arrangement than an ordering.** The host resolved
overlaps by test order — the nub's reach covered both arrow roots, so which handle the artist got depended
on which `if` came first. The proof walks 25 921 positions and checks that none satisfies two handles *and*
that the unit answers what the table predicts at every one of them. Contrast `SketchPicking`, where a curve
genuinely passes through its own endpoints and the priority order therefore **is** the design; separate the
geometry when you can, order it when you cannot.

📝 **Two things the proof taught me.** Measuring the world-per-pixel ruler over a fixed 24 units and then
drawing a 44-pixel arrow gave an arrow 79% of the length asked for at close camera range — perspective is
not linear, so the ruler must be read over the span it will be used for. And the error must fall on the
**short** side: an arm drawn longer than the hit test reaches has its arrowhead outside its own hit box.

⚠️ **A guard I wrote against a degeneracy that cannot occur.** I defended against an axis pointing at the
camera projecting to nothing — but `ResolveViewportFrame` derives the camera *from* the basis, so the view
is never edge-on to the plane. Tilting from 90° to 0.01° leaves both probes at exactly 96 px. The real
reason to take the smallest world-per-pixel is oblique foreshortening (~13% apart under isometric), which
is what the corrected note and sabotage S3 now pin.

`ParametricSketchHost` 4 296 → **4 146**. 125 claims, eight sabotages caught.

**10k — the commit.** `SlateWorkspace/Discipline/PlacementCommit`.

🔴 **The unit that fixes the reported bug: the host chose the geometry by SUBJECT ALONE when the decision
has TWO axes.** `CommitPlacement` was a 399-line `if`-chain testing `Tool.Subject`, so of the 27
subject-and-method pairs the catalogue accepts, **nine reached an arm not written for them**:

| the artist asked for | the host drew |
|---|---|
| Circle, Centred | a circle across the two anchors, at half the radius, offset to one side |
| Arc, Centred / Tangent / ThreePoint | a three-point arc, whatever the method |
| Ellipse, Centred / Diameter | an extent ellipse |
| EllipticalArc, Centred / ThreePoint | an extent elliptical arc |
| Polygon, Centred | an extent polygon |

The `Arc` `Centred | Tangent` arm sat *below* the bare `Arc` arm and was therefore **unreachable dead
code** — it read like a feature and had never once run. `Tangent` had no reachable implementation at all.
This is exactly the redundancy the tool-list rework set out to remove: separating shape from method in the
catalogue accomplishes nothing while the commit still dispatches on shape. The chain is now a **27-row
table keyed on both axes**, and a pair with no row is refused by name rather than silently drawn wrong.

🔴 **One drawn thing was many presses of undo.** Each arm sealed its own revision as it went, so a line
sealed two (the line, then its coincidence), a circle sealed two (the profile, then its radius dimension),
and a five-anchor polyline sealed **five**. `PlacementJournal` now accumulates the affected records and
seals exactly one revision when the placement closes. It deliberately mirrors `WorkspaceRevisionSequence`'s
`Seal` signature so that every arm body could be moved across **verbatim**.

🔴 **Three defects outside the unit that the proof caught, all of which stopped geometry appearing.**

- **A five-element brace list for a seven-field struct.** `CircularArcCurve` hides `ThroughPoint` and
  `ThroughDeclared` between `StartDirection` and `Radius`; three sites in `SketchStructure.cpp` passed five
  values, so the radius landed in `ThroughPoint` and **every circle profile and every slot had radius
  zero**. It compiled silently, declared, named and listed correctly, and drew nothing.
- **`DeclareThreePointArc` was wrong three separate ways** — reversed `Difference` arguments, a normalised
  vector in the numerator against an unnormalised denominator, and both cross products *and* both weights
  swapped against the standard circumcentre form, which cancelled in magnitude and negated the centre. It
  returned a centre near the origin with radius 0.02. Fixing two of the three left it looking nearly right.
- **`acos` of a dot product was unclamped**, so a rounding overshoot returned NaN and a NaN sweep drew
  nothing.

⚠️ **`AppendCurvePolyline` clears the vector it is handed, in spite of its name.** Sampling a whole sketch
by looping it over one accumulator keeps only the last curve, so a correct four-arc circle measured as a
single quadrant — half the radius, offset by half the radius. Every shipped caller passes a fresh vector,
so nothing was broken, but the name means the opposite of the behaviour and it cost real time.

`ParametricSketchHost` 4 146 → **3 635**. 142 claims, four sabotages caught — two of them at compile time,
because `-Werror=unused-function` turns a deleted table row into a build failure.

### 4.6 The workplane defect

**`SlateWorkspace/Discipline/WorkplaneCatalogue`.**

🔴 **A sketch held exactly ONE plane and overwrote it, so placing a workplane silently re-interpreted
everything already drawn.** `SketchStructure::DeclarePlane` assigns; the workplane tool called it directly.
Curves keep their world coordinates, so nothing visibly jumps — but from that moment every measurement,
every grid line and every projection answers for a surface the geometry was never drawn on. Nothing
refused and nothing warned. Measured: a line drawn on the ground, then a second plane placed, and the
sketch reports the *second* plane for all of it.

🔴 **A workplane was a name and nothing else.** The shipped tool wrote a `Folder` record carrying only a
string and put the actual plane nowhere. It could not be selected, re-activated, offset, measured against
or removed, and it did not survive reopening. A plane the artist can make but cannot then *use* is worse
than no tool at all, because it also moves their drawing.

📝 **The arrangement is the one every parametric sketcher converges on, for the same reason:** planes are a
named collection, one is active, and the standing planes are entries in it rather than a separate
mechanism — so "sketch on the front plane" and "sketch on the one I just placed" are the same act.
`WorkplaneStanding` already knew what a plane *is*; this unit is where they live and which is current.

⚠️ **Two host sites were hardcoded to the ground plane** and both now read the catalogue: the workplane
tool, and the first-placement seeding at what was `:2004`, which meant activating a plane and then drawing
put the geometry on the ground anyway.

🔴 **A proof that passed while the behaviour was wrong.** `ResolvePointedWorkplane` was written so the
artist could point at an existing plane and draw on it, and its first proof passed — because the fixture
used a plane standing at y=60, well away from everything. Wired into the host and actually measured, **all
three standing planes pass through the world origin**, so a ray aimed anywhere near the middle of the
scene passes within reach of every one of them and "nearest along the ray" is decided by where the camera
happens to stand. From one ordinary three-quarter view, clicking the ground selects Ground, Front, Front
and Side at four different points. The host wiring was withdrawn and §7 now pins the instability itself as
a measured claim. **A fixture that avoids the degenerate case proves nothing about it** — the same lesson
as the transform-session proof, learned again.

The unit keeps `ResolvePointedWorkplane` because it is correct and proven for planes that stand apart; what
it cannot do is disambiguate coincident ones, and that wants a selection affordance rather than a bare
click.

105 claims, eight sabotages caught.

### 4.7 Finishing step 10

**10l — the projections.** Four host helpers, `ProjectAreaPoint`, `ProjectWorldPoint`,
`ProjectSceneProxyPoint` and `ResolvePlanePosition`, were thin covers over `ViewportProjection`. Three were
deleted outright and the fourth became `ProjectOffsetPoint`, which states an offset in the basis's own
three axes — the host had rebuilt that offset by hand at three sites.

🔴 **`ProjectSpatialPoint` DID NOT DO WHAT ITS HEADER SAID.** The note read "it does NOT require the point
to lie on the plane"; the body ran the point through `ResolvePlaneCoordinates` first, which **discards the
component along the normal**. A point fifty units above the plane projected to exactly the pixel of its
shadow on it, and in orthographic the height was discarded silently and completely. Measured: 193 px of
error in perspective at 50 units up, 400 px at 80. `ViewportProjectionProof` had a claim that projecting a
point and projecting its coordinates agree — and both sides of it lie on the plane by construction, so the
case was never covered. Eight new claims now pin it, including the sign, and the old body fails all eight.

⚠️ **I manufactured a defect and then had to un-find it.** Comparing the host's projection against the
unit's, I typed a 60° field of view into the scratch copy where the codebase uses 42°, measured up to 56 px
of divergence, and briefly believed the host and the unit disagreed. They are algebraically identical.
**Copy a constant, do not retype it** — and when two implementations of the same formula differ, suspect
the harness before the code.

**10m — the constraints.** `SlateWorkspace/Discipline/ConstraintAuthoring`.

🔴 **What a constraint DEMANDS of the selection was decided by where it sat in an `if`-chain** — the same
shape as the placement defect. The host tested the subject and then reached for `.Curve` or `.Point`
according to the branch, so what a relationship needed was a property of its position in the chain rather
than of the relationship. It is now a table of eight rows stating demand, badge and name, and every one of
the sixteen selections the artist could be holding is walked against every subject.

🔴 **Two of the six solver verdicts were unnamed.** `ConstraintDispositionText` handled four and fell
through to "unknown", so a sketch that was simply *not solved yet* and one carrying a *duplicate
constraint* both read as an unexplained failure. The compiler found it the moment the switch moved
somewhere warnings are errors.

🔴 **Every constraint badge on a line hung on the line's endpoint.** The anchor was `Polyline[size / 2]`,
and **a line samples to exactly two points however many steps are asked for** — so `size / 2` is index 1,
its end. That is precisely where lines meet, so badges on connected lines stacked on top of one another.
The anchor is now the midpoint *by length*, which is right for a two-point line and a fifty-point arc
alike.

⚠️ **A sabotage survived because the fixture asked the unit what to expect.** §2 read each demand from
`DeclaredConstraint` and then checked the unit against it, which proves only that the unit agrees with
itself: changing Coincident to want two curves passed. The expectation is now stated independently, from
what each relationship *means*. **A proof must know the answer before it asks.**

`ParametricSketchHost` 3 635 → **3 512**. Engine-mechanism definitions 18 → **11**. 448 claims, five
sabotages caught.

### 4.8 Finishing step 10 — the overlay, and the widget three hosts copied

**10n — snapping.** `SketchSnap` had **no proof at all**, and two defects were living in it.

🔴 **INTERSECTION SNAPPING WORKED ON THE GROUND PLANE AND NOWHERE ELSE.** `SegmentIntersectionPlanar` read
`.Left` and `.Forward` off every point — the ground plane's two spanning axes. Two lines crossing on the
front plane share a constant `.Forward`, so the determinant vanished for every pair and the crossing was
never found. Silently: the artist got no snap rather than a wrong one. This is precisely the failure that
makes drawing on a chosen workplane feel broken, and it was sitting underneath Task 6 the whole time.

🔴 **`SketchSnapMask::GridAccepted` WAS INERT.** The flag was declared the day the mask was written and
`ResolveNearestSnap` never produced a `Grid` placement. Every caller that wanted grid snapping had to write
its own, and the host did — with its own step, its own tolerance and its own ordering. The unit now honours
the flag and states the rule that matters: **the grid is a last resort, never a competitor**, because a
grid corner sits within half a step of every probe and would otherwise beat the endpoint the artist was
reaching for.

**10n also.** `ConstraintToolSubject` → `SelectedConstraint` in `SketchToolset`, beside `SelectedTool`,
which is where tile-to-subject mapping already lived. ⚠️ **Three tiles cannot be honoured and now say so.**
The palette offers `Midpoint`, `Symmetry` and `Concentric`; `ConstraintSubject` declares none of them. They
fell to `default` alongside every non-constraint tile, so pressing one was indistinguishable from pressing
nothing at all.

**10o — the viewport overlay.** `SlateWorkspace/Discipline/SketchViewportOverlay`, **829 lines, 14
definitions**: the grid, the orientation HUD, the selection highlight, the gizmo, the placement preview,
the constraint badges, the profile shading and the edge readouts.

🔴 **NOTHING BUT THE HOST COULD DRAW A SKETCH.** All fourteen were file-local to `ParametricSketchHost`. A
second viewport — a detail view, a print preview, the combined authoring host — had no way to reach them
and would have had to copy them.

**10p — the orientation widget.** `SlateWorkspace/Discipline/OrientationCube`, lifted out of
`Application/Api/SharedViewportHostBridge.h`.

🔴 **472 LINES OF `inline` IN A HEADER ALL THREE HOSTS INCLUDED.** Every host compiled its own copy, and a
header under `Application/` is reachable by no unit and no test, so the widget could never be exercised.
That header is deleted at step 11; this is where its one real behaviour goes.

🔴 **THE DUPLICATE ORIENTATION ENUMERATION IS GONE.** `SharedViewportOrientation` named the same seven
directions as `ViewportOrientation` in different words — `Iso` against `Isometric` — plus a `None` member
that meant only "the pointer hit nothing". Two enumerations for one idea is how a conversion switch gets
written, and the host had one, seven cases long. A miss is now a refusal, which is what it always was.

| Measure                              | Step 10 start | Now       |
|--------------------------------------|---------------|-----------|
| `ParametricSketchHost` lines         | 5 757         | **2 613** |
| Definitions in the host              | 138           | **33**    |
| Engine-mechanism definitions         | 18            | **7**     |

**10q — what the selection makes possible.** `SlateWorkspace/Discipline/ToolAvailability`.

🔴 **TWELVE FLAGS, RESET IN ONE BLOCK AND THEN SET BY A NINE-ARM SWITCH.** Each arm assigned only the flags
it cared about, so the block and the arms had to agree — in two places, forever. An arm that forgot a flag
inherited the reset's value; a reset that forgot one let the previous selection's value survive into the
next. It is now one row per subject, applied whole, and the proof writes a **saturated** context before
every call and demands the answer be identical to the one produced from a blank one. That is the claim the
shipped shape could not make. Removing a single field from the application trips **10** claims.

⚠️ **A SABOTAGE SURVIVED BECAUSE THE CLAIM READ THE ANSWER RATHER THAN THE BEHAVIOUR.** Removing the bound
on `AnyRowSelected` scored **zero** failures: reading past a 128-element array usually returns the same
answer, so a claim on the result cannot see it. The context is now embedded in a larger block whose tail is
deliberately non-zero, and the claim is about where the search *stops*. **When proving a bound, place
something on the other side of it.**

⚠️ **`ParametricWorkspaceContext` ARRIVES WITH ROW ZERO ALREADY TICKED** — `RowSelected[RowLimit] = { true }`
initialises the first element to true and the rest to false. My first fixture assumed a blank context
selected nothing and reported the unit wrong.

| Measure                              | Step 10 start | Now       |
|--------------------------------------|---------------|-----------|
| `ParametricSketchHost` lines         | 5 757         | **2 474** |
| Definitions in the host              | 138           | **28**    |
| Engine-mechanism definitions         | 18            | **4**     |

**10r — the codex proxy.** `SlateWorkspace/Discipline/CodexSceneProxy`, **330 lines from the host plus 180
from a bridge header**.

🔴 **ALL THREE HOSTS DREW SCENE PROXIES AND ALL THREE PROJECTED THE POINTS THEMSELVES.**
`ParametricSketchHost` had `RecordCodexSceneProxy`; `EditorHost` has `RecordWorkspaceCodexProxy` and
`ProjectWorkspaceCodexPoint`; `PaintHost` has `ProjectPaintScenePoint`. Three copies of one idea, each with
its own projection — which is how they came to disagree about where a box belongs.

🔴 **`Application/Api/SketchSceneDirectoryBridge.h` IS DELETED — THE FIRST OF THE FOUR.** Two hosts included
it and nothing else could reach it. `Bridge` named the file rather than the behaviour; what it actually did
was turn a codex into directory rows, so it is now `BuildSceneDirectoryRows` in a unit.

⚠️ **TWO GATES ASSERTED THE BEHAVIOUR BY ITS ADDRESS.** `ValidateMeshScenePath` and
`ValidateMaterialSystemPass2` both checked for the function *inside the host file*, so lifting it read as
deletion. They now check that the host still CALLS it and that the unit DEFINES it — which is a stronger
statement than either made before, and it fails if a host ever defines one again.
