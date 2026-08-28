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

### 4. Bottom-toolbar dropdowns are transparent — READ

There is a `SlateUI/Interface/Dropdown` unit but it has no `Source/`, so the bottom bar's
dropdown plate is drawn elsewhere. Locate the actual draw call, and give the plate an opaque
fill in the same theme token the other popovers use.

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

### Step 3 — Orthographic zoom
Probe which of the three candidates holds, fix it, and prove the behavioural claim.

### Step 4 — Opaque dropdown plates
Find the draw, make it opaque. **Proof:** the plate's fill alpha is fully opaque wherever a
dropdown is drawn.

### Step 5 — `Q` selects, and does not draw
`Q` activates a Select tool. While it is active no placement begins on a press; the press picks.
**Proof:** a press with Select active adds no curve and no anchor to the sketch.

### Step 6 — The floating tool-options widget
A viewport-anchored popover, per the attached HTML: a persistent panel for Select carrying
**Vertex / Edge / Face** modes and a transform gizmo toggle, collapsing to a **pill** reading
`Menu Name ⌄` with an SVG-style icon. Built on the existing panel and theme components, not a
new widget stack.

> ⚠️ **The attached `ToolOptionsWidget.html` did not arrive in the workspace** — `/home/user/uploads/`
> is empty. Step 6 is specified from the description alone and will be built to the file once it
> is re-attached, so that the visual result matches rather than approximates.

### Step 7 — Context menus that avoid each other
A context menu (Bevel, Chamfer, Trim, Cut, Add) opens in a free corner, never on top of another
widget. **Proof:** with the Select widget placed, an opened context menu's rectangle does not
intersect it, tested in every corner the layout allows.

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
