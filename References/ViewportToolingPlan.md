# Viewport Tooling, Selection and Idle Cost — Plan

Eight items, ordered so that each one is provable before the next begins. Findings marked
**MEASURED** were reproduced in this workspace before the plan was written; the rest are read
from the source and are marked **READ** until a runtime probe confirms them.

---

## Diagnosis

### 1. The Bezier flattens as you add points — MEASURED

`AppendCurvePolyline` evaluates `CurveSubject::Bezier` by running de Casteljau across **every**
control point at once (`SketchPolyline.cpp:16`, called at `:309`). Placing N anchors therefore
builds a single Bezier of **degree N-1**, not a chain of cubics.

A Bezier of degree N-1 interpolates only its first and last control point. Every interior anchor
is a *pull*, not a point the curve passes through, and the pull weakens as the degree rises —
each added point makes the curve smoother and further from what was drawn. Measured, on a
zig-zag of alternating height 0 and 80:

| Anchors | Curve misses its interior anchors by |
|--------:|-------------------------------------:|
| 3 | 40.00 |
| 4 | 40.93 |
| 5 | 41.74 |
| 6 | 42.26 |
| 7 | 42.61 |
| 8 | 42.86 |

Monotonically worse, exactly as reported. `BasisSpline` shows the same effect at 4 and 5 points
and then settles at 32.31 — it is degree-clamped, so it stops degrading, but it still does not
pass through its anchors.

**Fix.** Treat consecutive anchors as a **chain of cubic segments** rather than one high-degree
curve. Interior tangents come from the neighbouring anchors (Catmull-Rom converted to Bezier
control points), so the curve passes through every point the artist clicked and adding a point
changes only the two segments beside it. The existing `Hermite` arm already has the right shape
for this and is the model to follow.

> ⚠️ This is a **shape change to committed geometry**, not just the preview. `PlacementCommit`'s
> `DeclareBezier` hands all anchors to `SketchStructure::DeclareBezier`, so the commit path
> changes with the preview or the two disagree — which the Task-12 agreement claims will catch.

### 2. Idle CPU of 8–9% — MEASURED, FIXED

The editor never blocks. `EditorHost.cpp:713` loops on `Session.Active()` and every pass runs a
full interface tick and a full present. `WindowInterchange::Await()` (`:151`, `glfwWaitEvents`)
is the blocking call, and the **only** caller is `HostLifecycle.cpp:354`, reached solely when the
window is minimised to a zero extent. So while the window is visible the host redraws at the
pacing rate forever, whether or not anything changed.

`DisplayScheduler::ScorePacing` returns `VK_PRESENT_MODE_FIFO_KHR` for `SteadyPacing`, which the
editor selects at `EditorHost.cpp:193`. FIFO paces to the display, so the cost is one full
interface rebuild plus one command buffer plus one present, sixty times a second, at rest.

The engine already contains the component that fixes this, unused by any host:
`SlateUI/Interface/RedrawScheduler`. Its own header states the problem —

> *"Under FIFO pacing `DisplayScheduler` presents sixty identical images a second forever unless
> something declines to rotate, and a UI that re-presents an unchanged image is not idle no matter
> how little it recomputed. `Waking` is the only sanctioned reader of that question."*

`bool Waking(bool AnythingMoving, bool ArrivalHeld)` at `RedrawScheduler.h:125` is the wake rule.
`ComponentSpecification`, `ControlPanel` and `ViewportSequence` already raise marks; nothing reads
them to decide whether to present.

**Fix.** Have the host consult `Waking` and call `WindowInterchange::Await()` when it says no.
Anything animating — a projection transit, a hover, a tool preview, a text caret — raises a mark
and keeps the loop hot; a genuinely idle viewport blocks in the window system at ~0%.

> ⚠️ **Attribute before optimising.** 8–9% may be the redraw, or it may be one expensive thing
> inside it (an every-frame tessellation, an ear-clip, a sky rebuild). Step 2 begins with a
> per-stage frame timing probe so the report says *which* stage costs what, rather than assuming.

### 3. Orthographic zoom does not change the apparent size — READ

`SketchInteraction.cpp:39-45` does branch on projection and scales `View.OrthoScale` by 1.1/0.9
per notch, clamped to `[0.05, 40.0]`; `CadProjection.cpp:24` multiplies by it. The wiring reads
correct, so the defect is elsewhere and must be found at runtime rather than guessed. The three
candidates, in the order they will be tested:

1. the leaf passes `Perspective == true` while presenting as ortho, so the wheel drives
   `View.Distance` (which a parallel projection ignores) instead of `OrthoScale`;
2. `OrthoScale` is recomputed from the transit each frame (`EditorHost.cpp:1262`,
   `ResolveTransitGroundScale`) and overwrites what the wheel just wrote;
3. the clamp is reached immediately because the starting value is already at a bound.

**Fix.** Determined by the probe. The claim to prove is behavioural: *a wheel notch in a parallel
view changes the on-screen distance between two fixed world points.*

### 4. Bottom-toolbar dropdowns are transparent — **DONE**

**They were never transparent.** Every menu plate already filled itself with `ChromeGround`, an
opaque token, and had done all along. What actually happened is that **both GPU passes record
AFTER the interface** and **scissor to the whole viewport leaf**, so the grid, the axes and the
sketch drew straight over any menu opened from the footer. What showed through was the viewport
behind it — which is exactly what a transparent menu looks like.

`EditorPanel::AnyPopupStanding()` already existed, with a comment describing this defect
precisely, and **the host never called it** — the fourth "declared, tested, never called" in this
tree. Worse, `SceneDirectoryProof` drives it and cites *"see the host's `OverlayWithheld`"*, a
function that does not exist: the proof was simulating a fix nobody had written.

The obvious cure is the wrong one. Suppressing the overlay whenever a menu stands is what that
predicate invites, and it is the same trade that once made an open drawer erase the whole sketch —
a menu covers a few hundred pixels, and blanking a viewport's grid to protect them swaps one
visible defect for a worse one. So the menu's box is **subtracted** from the scissor instead.

A scissor is one rectangle, so the remainder is recorded as **up to four disjoint bands**. The
arithmetic is `ExtentBandsAround` in `Foundation/ExtentBands.h` — Foundation because the callers
are in `SlateVulkan`, which requires `SlateMath` alone; stating the boxes as `PlaneExtent` would
have inverted that dependency, and `VerifyPartition` caught the attempt. Each pass gained a
`RecordAround` that delegates to it, so the host holds no new logic and `VerifyHostPartition`
still reports zero definitions.

Also fixed along the way: every menu plate is now drawn through one `RecordMenuPlate`, so a menu
added later cannot forget to report its box — which is how the previous attempt was left inert.

**Proven:** `OverlayExclusionProof`, 69 claims. Coverage is measured by rasterising the bands and
counting each pixel's writes, so a decomposition that merely looks plausible cannot pass —
overlaps, gaps and trespass are all counted separately. Sabotaged four ways: overlapping bands
(20 failures), the withheld box ignored (25), the overhang clamp dropped (1), the sole
degeneracy guard removed (14).

---

## Work, in order

Each step ends with: the gates green, a claim that fails when the fix is reverted, and a commit.

### Step 1 — Bezier and NURBS pass through their anchors
Chain of cubics for `Bezier`; the same treatment for `BasisSpline`/`RationalSpline` so all three
interpolate. Preview and commit share the change. **Proof:** every interior anchor lies within a
small tolerance of the drawn curve, at 3, 4, 5, 6, 7 and 8 anchors — the table above, driven to
≈0. Plus the existing preview↔commit agreement claim.

### Step 2 — Audit the frame, then stop presenting unchanged images — **DONE**
Measured first. The sketch geometry was **not** the cost:

| Sketch | Project to screen | Tessellate every curve | Cost at 60 fps |
|-------:|------------------:|-----------------------:|---------------:|
| 20 curves | 12.1 µs | 12.1 µs | 0.15% of a core |
| 80 curves | 11.6 µs | 50.2 µs | 0.37% of a core |
| 240 curves | 12.2 µs | 149.8 µs | 0.97% of a core |

Under 1% even at 240 curves, so the 8–9% is the unconditional rebuild-and-present, exactly as
`RedrawScheduler`'s own header predicted. `ViewportSequence::Waking()` now reads all three
operands and `SessionSequence` sleeps the tick when it says no.

**Bounded, not blocking.** `AwaitFor(0.05)` rather than `Await()`: if the wake rule ever misses a
source of change, a bounded wait costs a late frame, while an unbounded one costs a window that
never redraws until the artist moves the pointer — which is reported as a hang, not as a missed
redraw. `Tools/IdleWakeProof` carries 18 claims and five sabotages, catching both a rule that
never sleeps and a rule that sleeps when it should not.

### Step 3 — Orthographic zoom — **DONE**
None of the three candidates was the cause. **`OrthoScale` was written in exactly one place in the
whole tree** — `DriveViewport`, a function with **no call sites** — so the value sat at its default
of `3.0` for the entire session and the wheel drove nothing. The projection arithmetic was never
wrong: a 10-unit span measures 30.0 px at scale 3.0 and 43.9 px at 4.392, correctly, all along.

That answers the question I had been holding for the user: **it failed in every orientation**, not
some, because the wheel never reached the value at all. Same class of defect as Task 15 — working
code with nothing calling it.

Two further defects fell out of writing the proof:

| Defect | Effect on the artist |
| --- | --- |
| `1.1` and `0.9` used as the two wheel directions | They are not reciprocals — their product is `0.99`, so every zoom-in-then-out pair shrank the view 1%. Forty pairs left it a third smaller and the zoom appeared to wander. Now `× 1.1` and `÷ 1.1`. |
| The fly camera read `WASDEQ` whenever the artist was not typing | `Q` both chose the Select tool **and** sank the camera. The fly keys now belong to the fly gesture, as in every reference fly-cam this was modelled on. |

A probe span of `(10, 0, 0)` is invisible from the Left and Right views — it points at the eye and
projects to zero length. That looked like two more zoom failures and was a badly chosen probe; the
span now has a component on all three axes.

**Proven:** `OrthographicZoomProof` 30 claims across **all seven** orientations, sabotaged three
ways — a dead wheel (9 failures), the non-reciprocal ratio (4), a missing bound (3).
`ValidateHostBuildBudgets` gained two claims: that the host drives `OrthoScale`, and that the fly
keys are gated on the look gesture. Both sabotaged.

### Step 4 — Opaque dropdown plates
Find the draw, make it opaque. **Proof:** the plate's fill alpha is fully opaque wherever a
dropdown is drawn.

### Step 5 — `Q` selects, and does not draw — **DONE**
`Q` activates a Select tool. While it is active no placement begins on a press; the press picks.

**What was actually wrong was worse than the plan assumed.** The Select tool, the gizmo, the
transform sessions and the whole Blender-style `G`/`R`/`S` grammar were already implemented and
already gated correctly on `SelectedTool(Select).Subject == SketchSubject::None`. The reason none
of it worked is that **`DriveViewportSelectionAndTransform` had zero call sites** — the host never
invoked it. Nothing was broken; nothing ran.

| Piece | Before | After |
| --- | --- | --- |
| Selection / gizmo path called by the host | never | every viewport tick |
| `Q` bound to Select | no binding | `KeySubject::ChooseSelect`, guarded by `WantTextInput` |
| Element mode | none — one fixed Point→Control→Curve priority | Vertex / Edge / Face, each refusing to fall back |
| Selection tolerance | shared with the snap tolerance | stated in pixels, converted by inverting the projection |
| `R Y 35` | the `Y` was silently dropped | read as the plane normal and shown in the readout |

`ValidateHostBuildBudgets` claimed *"the interaction it now calls"* and never checked that it did.
It checks three entry points now; deleting the call fails the gate.

**Proven:** `SelectionModeProof` 20 claims, sabotaged three ways. `TransformSequenceProof` 68
claims, sabotaged three ways.

### Step 6 — The floating tool-options widget — **DONE**
Built to `References/ToolOptionsWidget.html`, read from the repository the link names. The
reference declares exactly four kinds of control — slider, segmented, toggle, swatches — and every
tool's options are a table of them, so that is the whole grammar of `ToolOptionsWidget`.

Select and the gizmo are **one** widget, because choosing something and then moving it is one
thing the artist does. Its three rows are the element mode, the tolerance in pixels, and whether
the gizmo is shown. **There is no snapping row** — choosing a specific element and being dragged
onto the nearest grid line are opposite intentions.

Two deliberate departures from the reference, both recorded in the code:

- **The collapsed form is a pill, not the reference's 56 px round bubble.** A bubble shows an icon
  and cannot answer "which tool's options are folded in here". The pill keeps the title and a
  chevron and is sized from its own caption, so the answer always fits.
- **The card clamps to the viewport leaf, not the window.** A widget clamped to the window could be
  dragged behind a drawer and never dragged back.

`VertexPoint`, `EdgeSegment` and `FacePlanar` were unresolved stubs and are now drawn: one
quadrilateral with the selected part emphasised, so the three modes read as one choice about one
thing. `CrossClose` was added for the dismiss action.

**Proven:** `ToolOptionsWidgetProof` 25 claims — the edge clamp, the knob/readout round trip, the
zero-width span, the pill sizing and the row bounds — sabotaged two ways.

### Step 7 — Context menus that avoid each other — **DONE**

`ToolContextMenu` opens from a stationary right-click in the sketch leaf and takes the first free
corner of the tile it belongs to. The arithmetic is `PlaceMenuClear` in `Foundation/ExtentBands.h`,
beside the band splitting from step 4 — same reason, same layer: the boxes are pure numbers and
`SlateVulkan` may not name an interface type.

Three things the design had to get right, none of them obvious from the requirement:

| Question | Answer |
| --- | --- |
| Four rigid corners of the anchor | Too few. A tile near the leaf's top-left has three of them off the edge, so one blocked corner meant refusing. A corner that hangs off is now **slid back on** along the offending axis — it keeps the SIDE it was asked for and gives up only its exact ordinate. |
| What if every corner is spent? | The menu **detaches** and takes a free corner of the leaf. It stops looking attached to its tile, which is a real cost, but it opens and it still covers nothing. Only when the leaf itself has no room does it refuse. |
| What opens it? | **Not the secondary press** — that already flies the camera, and binding a second feature to it is exactly what `Q` did. A press that travelled was a look; the menu opens on a release that did not, measured over the whole hold. |

The options widget now publishes `Occupies()`, the box it actually recorded this tick, and the host
declares that to the menu each frame. A widget is draggable, so a constant kept beside it would
steer the menu into the very thing it was avoiding. A hidden widget reports an empty box, so its
ghost stops steering the layout.

**Proven:** `MenuPlacementProof`, 74 claims — every corner of the leaf, three corners blocked, two
menus at once, flush neighbours, refusal when swamped, and the intersection test measured directly
rather than trusted. Sabotaged four ways: flush treated as overlap (4 failures), stopping at the
first blocked corner (10), never refusing (2), dropping the slide (6). Three gate claims guard the
wiring, each sabotaged.

### Step 8 — Construction tools
`Bevel`, `Chamfer`, `Trim` (delete a region bounded by two points), `Cut` (split at selected
points), `Add`. Each is a tile plus a context menu, and each needs a selection — which is why it
follows steps 5 to 7. `Operation/ChamferSolver` and `TaperSolver` already exist as stubs to
build into.

---

## Sequencing note

Steps 1–4 are the reported defects and are independent of one another. Steps 5–8 build one
feature in dependency order: selection, then the widget that configures it, then the menus the
construction tools present, then the tools. Nothing in 5–8 can be shown to work before 5 exists,
which is why the defects come first.
