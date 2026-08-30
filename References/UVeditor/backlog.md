# UV Toolkit — Backlog

Deferred features for the browser UV-editor prototype (`.retired/Prototypes/UVeditor/`).
Zero-dependency, camelCase JS, CPU rasterizer to `<canvas>`. Frontier C++ naming/formatting
skills do **not** apply here. Each entry names the real existing functions it builds on so the
work can start without re-discovery.

Current anchor points these features extend:
- `unwrap.js` — `packIslands(islands, opts)` (~486) + packers `packGrid`/`packShelf`/`packSkyline`
  (~393/411/432, active default `skyline`); `lscmIsland` (LSCM conformal solve); `islandWeight`.
- `uveditor2d.js` — `uvedDrawFG()` (~217, fills `TOPO.faceTris`, strokes polygon loop);
  `islandUVBounds` (~255); `islandAt(u,v)` (~275); `islandSignature` (~526).
- `viewport3d.js` — CPU rasterizer, matcap shading, `faceFrontFacing`, `VP3.depth` occlusion.
- Per-vertex UVs stored per island as `isl.uv[v] = [u,v]`. Target space is the 0-1 UV square.

---

## 1. Checker-texture data visualization (2D UV + 3D viewport)  ✔️ DONE (2026-07-04)

**Status:** Implemented with a display-CHANNEL + element-OVERLAY model and a new top
`.viz-toolbar`. New `js/checker.js` exposes the stateless `checkerColor(u,v,opts)` (twoTone +
oriented modes, `checkerWrap` for UDIM tiling) and the `CHECKER` view-state object
(`channel2d`, `enabled3d`, `tiles` 4/8/16/32, `mode`). Both renderers sample the SAME function so
2D and 3D match.
- **Toolbar** (`index.html` `.viz-toolbar` + `app.js wireVizToolbar`): a Channel segmented control
  (Solid · Checker · Stretch-stub) shared by 2D+3D, a Checker dropdown (show / project-on-3D /
  resolution / mode), and an Elements dropdown (faces / edges / vertices / wireframe overlays).
  Follows the existing `__popups` + `data-*` → state → redraw pattern.
- **2D** (`uveditor2d.js`): `uvedDrawChecker` paints the procedural board on the 0-1 box; when the
  checker channel is on, `uvedDrawIsland` fills each fan triangle by sampling `checkerColor` at its
  UV centroid so the board shows THROUGH the islands (only where UVs land).
- **3D** (`viewport3d.js`): `VP3.channel`; `vp3SetUvChannel(islands)` mirrors island UVs into a flat
  `VP3.uv` map (refreshed on every island mutation from `uveditor2d.js`); `vp3RasterCheckerPass` is a
  per-pixel software (ImageData) rasterizer that barycentric-interpolates UV per pixel, samples the
  checker, and MULTIPLIES by the matcap shade (keeps form reading), compositing over the grid via a
  canvas snapshot. Runs ONLY when channel==="checker" AND UVs exist; matcap vector fill stays the
  default. Element overlays (`showFaces/showEdges/showVerts` + existing `showWire`) draw on top,
  depth-clipped, independent of selection mode.
- **Off by default:** 3D shows matcap until "Project on 3D" / the checker channel is enabled.
- Verified: `[checker] shared colour fn + 3D UV plumbing + channel switch ✔` — determinism, two-tone
  parity flips across tile boundaries, resolution/mode honoured, `VP3.uv` populated for every
  unwrapped vert, barycentric identity at corners + centroid = UV mean, and both channel + overlay
  paths run without throwing. Full smoketest passes.
- **Deferred (open Qs):** oriented mode uses a colour/gradient orientation cue only (no rendered
  per-tile digits); tile count is a single global tied to the 0-1 box (not per-island); 3D checker
  multiplies matcap rather than fully replacing albedo.

**Request:** "we need to add data visualization to visulize cheker texture on 2d + 3d"

**Goal:** Render a procedural checker/reference texture in both the 2D UV editor and the 3D viewport, sampled from per-vertex UVs, so texel density, orientation flips, and seam continuity are visible at a glance. Zero-dependency, purely procedural (no image asset).

**Approach:**
- Add a shared `checker.js` module exposing `sampleChecker(u, v, opts)` returning an `[r,g,b]` triple; keep it stateless so both the 2D and 3D paths call the identical function and colors match. `opts` = `{ tiles, mode, colorA, colorB }` where `mode` is `"twoTone"` or `"oriented"`.
- Two-tone mode: `floor(u*tiles) + floor(v*tiles)` parity picks `colorA`/`colorB`. Oriented mode: tint each tile by its `(tileU, tileV)` cell index (e.g. hue ramp across U, value ramp across V) plus a small directional gradient inside the cell so mirror/rotation flips read visually; optionally draw a per-tile digit later.
- 2D UV editor: in `uvedDrawFG()` (uveditor2d.js ~217), before the island fill/stroke loop, paint the checker across the 0-1 square as the editor background. Reuse the existing UV→screen transform already used for the island triangles; either fill an offscreen `ImageData` once per `tiles`/zoom change and `drawImage` it, or draw tile rects directly. Islands then stroke on top via the existing `TOPO.faceTris` loop, so users see UVs laid over the checker.
- 3D viewport: in the CPU raster fill in viewport3d.js, per covered pixel compute barycentric weights across the current fan triangle (the same fan triangulation used for `TOPO.faceTris`/`faceNormal`), interpolate the island's per-vertex UV `isl.uv[v] = [u,v]`, call `sampleChecker`, then modulate by the existing matcap shade term. Gate behind the existing `faceFrontFacing` + `VP3.depth` occlusion so hidden fragments are skipped.
- Toggle + config: add a `checkerEnabled` flag and `checkerOpts` (tiles, mode) to app state in app.js, wired to a UI toggle; both draw paths early-out when disabled and fall back to current matcap-only / plain-background rendering. Faces/islands with no UV assigned skip checker sampling.
- Record toggle/tile changes through the existing history mechanism (history.js) only if they should be undoable; otherwise treat as pure view state.

**Touches:** js/checker.js (new), js/uveditor2d.js, js/viewport3d.js, js/app.js, js/history.js

**Open questions:**
- Should the 3D checker fully replace the matcap albedo or multiply against it (data-viz clarity vs. keeping shape-reading shading)?
- Is tile count a single global value tied to the 0-1 square, or per-island/per-shell so differently packed islands stay comparable?
- Does the "oriented" variant need actual rendered digits/numbers per tile (extra text raster in 3D), or is a color/gradient orientation cue sufficient for the first pass?

---

## 2. UV stretch / distortion visualization  ✔️ DONE (2026-07-04)

**Status:** Implemented as the third display CHANNEL (Solid · Checker · **Stretch**) on the top
`.viz-toolbar`, reusing the same channel/overlay model as the checker (#1). New `js/stretch.js`
exposes the stateless diverging ramp `stretchColorRamp(t)` (blue = compressed · green = neutral ·
red = stretched) and `computeStretchMetrics(islands, mode)` (both metrics the user asked for):
- **Area mode** — per fan triangle `sqrt(uvArea / worldArea)` scale, normalized against the model's
  world-area-weighted GEOMETRIC-mean scale (`refScale`) so neutral texel density = green by
  construction; stored as `log2(scale/refScale)` so compression/stretch are symmetric.
- **Angle mode** — conformal distortion σmax/σmin of the per-triangle Jacobian (built from a local
  2D basis on the 3D triangle → UV), `log2(ratio)` with 0 = angle-preserving.
- `STRETCH_SPREAD` = ±1 octave saturates the ramp; `stretchNormalized(log)` maps to `t∈[0,1]`,
  0.5 = neutral. Degenerate (zero-area / collapsed-UV) triangles are excluded from the reference and
  clamped to full-compression so a `log2(0)` never poisons the mean.
- **2D** (`uveditor2d.js`): a `stretchFill` path in `uvedDrawIsland` (mirrors `checkerFill`) fills each
  fan triangle with `stretchTriColor(face, ti)`. `uvedRefreshStretch()` recomputes on every island
  mutation (setIslands / store / repack) and live during scale/rotate (gated on the channel being
  visible; move is rigid so it's skipped).
- **3D** (`viewport3d.js`): `VP3.channel==="stretch"` colours each fan triangle in the vector fill
  loop by the same per-triangle value × matcap shade (form still reads); `triLocalIndex(t)` maps a
  flat `TOPO.tris` index → its fan index within the parent face. Off unless the channel is stretch.
- **Toolbar** (`index.html` + `app.js` + `css`): the Stretch channel button is now live; a Stretch
  dropdown has show / Project-on-3D / metric switch (Area·Angle) + a colour-ramp legend. `setVizChannel`
  makes the three fill channels mutually exclusive; switching metric recomputes.
- Verified: `[stretch] area+angle metric, diverging ramp, channel switch ✔` — ramp is
  blue→green→red + deterministic + saturates, area-mode neutral = world-area-weighted mean log ≈ 0,
  angle-mode conformal ratio ≥ 1 (uniform-scale → 1, shear → >1), and both 2D+3D channel paths run
  without throwing. Full smoketest passes.

**Request:** "we need to add data visuzliation to vizulize stretch"

**Goal:** Show a per-triangle heatmap comparing each fan triangle's 3D area against its UV-space area (and its conformal/angular distortion), so the user can see at a glance where the unwrap is compressed or stretched, in both the 2D editor and the 3D viewport, with a per-island stretch score in the UI.

**Approach:**
- Add a `stretch.js` module exposing `computeStretchMetrics(islands, mode)` that walks each island's faces via `TOPO.faceTris` (the same fan-tri decomposition `islandSignature` sums for area) and, per fan triangle, computes: 3D area from the object-space vertex positions in `mesh-data.js`, and UV area from the island's `isl.uv[v] = [u,v]` coordinates.
- Compute two ratio families keyed off a `stretchMode`: `"area"` = `uvArea / worldArea` normalized (so 1.0 = neutral, `<1` compressed, `>1` stretched), and `"angle"` = a conformal/singular-value distortion measure reusing `lscmIsland`'s per-triangle local 3D basis framing (the same edge-vector setup LSCM builds when pinning the 2 farthest verts) to derive the Jacobian singular values `σmax/σmin`; store both per faceTri in a `STRETCH.triValue[faceIndex][triIndex]` table plus a per-island aggregate.
- Add `stretchColorRamp(t)` in `stretch.js` mapping normalized distortion to a diverging ramp (compressed = blue, neutral = green/grey, stretched = red), returning an `[r,g,b]` used by both renderers.
- In `uveditor2d.js`, extend `uvedDrawFG()` (~217): when a `showStretch` toggle is on, fill each `TOPO.faceTris` triangle with `stretchColorRamp` of its stored value instead of (or blended over) the flat island fill, keeping the existing polygon-loop stroke.
- In `viewport3d.js`, tint the per-triangle raster fill in the CPU rasterizer using the same `STRETCH.triValue` lookup, layered under the existing matcap shading / `faceFrontFacing` / depth-occlusion path so distortion reads as a colour multiply.
- Surface per-island stretch scores (mean + max distortion, e.g. from the aggregate) in `app.js` UI next to island entries, and add a legend/scale widget (ramp bar with min/mid/max labels) plus the `area`/`angle` mode switch and `showStretch` toggle; recompute metrics after any `packIslands` / `lscmIsland` / unwrap change.

**Touches:** js/stretch.js, js/unwrap.js, js/uveditor2d.js, js/viewport3d.js, js/app.js, js/halfedge.js

**Open questions:**
- Normalization reference for the area ratio: normalize each island against the whole-model 3D:UV area ratio (absolute, cross-island comparable) or per-island (relative, hides global scale bias)? This changes what the score means.
- Angle-stretch metric precision: full per-triangle Jacobian SVD (`σmax/σmin`) reusing `lscmIsland`'s basis, or the cheaper Texture-Stretch L2/L∞ metric (Sander et al.) — and do we need both `area` and `angle` blended into one "combined" mode?
- Legend scaling: fixed distortion range (e.g. 0.5x–2x clamped) for stable colours across edits, or auto-fit to the current model's min/max each recompute?

---

## 3. UDIM tile workflows  ✔️ DONE (2026-07-04)

**Status:** Implemented (full scope, per user choice — grid render + labels + per-tile hit-test +
assign, not just tile-aware packing). New `js/udim.js` holds the tile model: `UDIM` state
(`cols`/`rows`/`activeTile`), `tileToUdim(tu,tv)` = `1001 + tu + tv*10` (tile (0,0)=1001), inverse
`udimToTile`, and `tileOf(u,v)`.
- **Per-tile packing** (js/unwrap.js): `packIslands(islands, {tile})` classifies islands by `isl.tile`
  (via `islandTile`, floor-of-bbox fallback), packs ONLY the target tile's islands in a normalized 0-1
  frame, then offsets the free set by `(tu,tv)`. Pins in other tiles never obstruct this one. Composes
  with pinning (#4): in-tile pinned obstacles are seeded as `pinnedAABB − origin`.
- **2D editor** (js/uveditor2d.js): the transforms `u2sx/v2sy/sx2u/sy2v` were generalized to centre on
  `UVED.viewU0/viewV0` (init 0.5, so single-tile framing is byte-identical) so UVs outside 0-1 render.
  `uvedDrawBG` draws the `UDIM.cols×UDIM.rows` tile grid with a per-tile bounds box + UDIM number
  label (active tile brighter); the checker/texel guide paints per visible tile. `islandAt`/
  `islandUVBounds` are already unbounded, so multi-tile hit-test works. `uvedAssignToTile(tu,tv)`
  translates the selection into a tile (pure `islandMove`, keeps intra-tile layout) and stamps
  `isl.tile`.
- **UI** (index.html + app.js): a UDIM active-tile `#` field (sets `UDIM.activeTile`) + an Assign
  button. `uvedRepack` loops every tile that holds islands. `tile` persists through history + every
  clone site.
- Checker/stretch overlays tile for free — `checkerColor` already wraps via `checkerWrap` (checker.js).
- Verified (`[udim]`): numbering (1001/1010/1011) + round-trip; assign-to-(1,0) shifts u by +1, stamps
  the tile, preserves shape; a pin in tile 1001 doesn't block a free island in tile (1,0); `tile`
  survives a history capture/restore. Full smoketest passes.
- **Deferred:** no per-tile island auto-distribute on overflow (a shell picks its tile explicitly via
  Assign / its UV position); the visible grid is a fixed 10×10 (doesn't auto-grow); no UDIM image/
  texture-set binding (this is a layout model, not a texture loader).

**Request:** "we need to add udim workflows"

**Goal:** Turn the single 0-1 UV square into an addressable grid of 1x1 UDIM tiles (1001 = tile (0,0), numbered `u + 1 + v*10 + 1001`) so islands can be assigned to, packed into, moved between, and rendered across arbitrary tiles.

**Approach:**
- Add a `udim.js` module holding the tile model: `tileToUdim(tu, tv)` returning `tu + 1 + tv*10 + 1001` and inverse `udimToTile(num)`, plus a small `udimGrid` state (columns/rows to display, e.g. 10-wide default matching the `*10` convention) referenced by the other modules.
- In `unwrap.js`, generalize `packIslands(islands, opts)` (~486) to accept a target tile in `opts` (e.g. `opts.tile = {tu, tv}`): the three packers `packGrid`/`packShelf`/`packSkyline` (~393/411/432) keep operating in a normalized 0-1 box, then offset the final placement by the tile origin `(tu, tv)` when writing back to `isl.uv[v]`. `islandWeight`/`PACKERS`/`PACK_METHOD` stay as-is; only the normalization/write-back adds the tile offset.
- Add `assignIslandsToTile(islands, selection, tu, tv)` in `selection.js` (or `udim.js`) that translates each selected island's per-vertex `isl.uv[v] = [u,v]` by the delta between its current tile and the target tile, preserving intra-tile layout — a pure translate, distinct from re-packing.
- In `uveditor2d.js`, extend `uvedDrawFG()` (~217) to iterate visible tiles: draw the tile grid lines and UDIM number labels per cell, and offset the face-triangle fill / polygon stroke by each island's tile origin. Update `islandUVBounds` (~255), `islandAt(u,v)` (~275), and `islandSignature` (~526) to operate in the unbounded multi-tile UV space rather than clamping to 0-1 (hit-testing must resolve which tile a click lands in).
- Make the checker and stretch overlays tile by wrapping UV coords with `frac(u)`/`frac(v)` (per-tile repeat) so the checker pattern and stretch shading read continuously across every UDIM cell.
- Thread a "target tile" selector through `app.js` (current active tile for new packs/assignments) and record tile assignments in `history.js` so move/assign/pack are undoable.

**Touches:** js/udim.js, js/unwrap.js, js/uveditor2d.js, js/selection.js, js/app.js, js/history.js

**Open questions:**
- Where is the authoritative tile origin stored — kept implicitly in the per-vertex `isl.uv` values (derive tile via `floor(u)`/`floor(v)`), or as an explicit `isl.tile` field that packing/rendering read?
- Does `packIslands` pack one tile at a time (caller loops per tile) or gain a multi-tile auto-distribute mode when islands overflow a single tile's 1-unit box?
- What is the visible grid extent and does it grow automatically as islands are moved past the default 10-wide row, or is it a fixed user-set bound?

---

## 4. UV shell pinning (packer avoids moving & avoids overlapping pinned shells)  ✔️ DONE (2026-07-04)

**Status:** Implemented together with the pixel margin (below) and full UDIM (#3). A per-island
`pinned` boolean freezes a shell: a repack NEVER moves or resizes it, and the free set packs AROUND
it. Policy (user choice): **warn + allow overlap** — a free shell that can't clear the pins is placed
anyway and the overflow count is toasted; pinned UVs are never touched.
- **`packIslands` split** (js/unwrap.js): pinned vs free; only `free` are measured/placed; pinned are
  seeded as obstacles. Returns `{ islands, overflow }`. **The final 0-1 rescale is disabled when pins
  exist** in every packer (else the free set would scale off the immovable pins).
- **Obstacle fidelity by packer** (user choice): `packGrid`/`packShelf`/`packSkyline` reject candidate
  placements that intersect a pinned island's **AABB** (`islandAABB` + `rectsOverlap`; skyline also
  seeds its skyline profile over each pin via `addRect`). `packShapeAware` rasterizes each pinned
  island's **TRUE footprint** into the shared `occ` grid + raises `colTop` before the free `items`
  loop (the #10-deferred seam), so free shells interlock around the real concave shape.
- **UI** (index.html Packing section + app.js + uveditor2d.js): Pin / Unpin buttons + **P** keybind
  (`uvedSetPinned` / `uvedTogglePin`); pinned islands draw with a solid amber bbox + 📌 (`uvedDrawIsland`).
  `uvedRepack` toasts the accumulated overflow. Pin flag persists through `history.js` (capture+restore)
  and every island-clone site (`uvedSetIslands`/`uvedStoreIslands`/`uvedMergeIslands`).
- Verified (`[pinpack]`): a pinned island's UVs are byte-unchanged by a repack under BOTH skyline and
  shape-aware (guarding the rescale-vs-pins conflict); free islands pack around with finite UVs; a
  near-full pin forces `overflow >= 1` and the overflow island is still placed (not dropped). Full
  smoketest passes.
- **Note:** manual drags on a pinned island are still allowed (the user grabbed it deliberately) —
  only a *repack* respects the pin.

**Request:** "pining(so it avoids or algotihm adapts around it(avoiding it from moving +avoiding placing shells ontop(repsecting the uv shell)"

**Goal:** Let the user freeze one or more UV islands in place so a re-pack never moves their UVs, while forcing every other island to pack around the pinned footprints instead of overlapping them.

**Approach:**
- Add a per-island `pinned` boolean on the island record (the same `isl` object that carries `isl.uv[v]`), defaulting to `false`; persist it through `history.js` so undo/redo of a pack restores both UVs and pin flags.
- UI to pin/unpin: in `uveditor2d.js`, reuse `islandAt(u,v)` for hit-testing the current selection and add a pin toggle (keybind + context action) that flips `pinned` on the hit island; in `uvedDrawFG()` (~217) render pinned islands with a distinct outline/tint so the frozen set is visible.
- In `packIslands(islands, opts)` (~486): split islands into `pinnedIslands` and `freeIslands` up front. Exclude `pinnedIslands` from the boxes-to-place list entirely (their UVs are left untouched — guarantee 1).
- Compute each pinned island's occupied AABB in 0-1 space via `islandUVBounds` (~255) and collect them as `pinnedBoxes` obstacles (guarantee 2).
- Wire obstacles into the active `packSkyline` (~432): seed the bottom-left skyline profile so each pinned box raises the skyline over its x-span to the box's top edge before placement begins, and add a rejection test so any candidate placement whose box intersects any `pinnedBox` is discarded (fall through to the next skyline node). Do the analogous obstacle-rejection in `packGrid` (~393) and `packShelf` (~411) so all three `PACKERS` respect pins.
- Keep `islandWeight` sqrt-area scaling on the free set only; pinned boxes contribute area to the skyline profile but are not re-scaled.
- Overflow handling: when no candidate position for a free island clears all `pinnedBoxes` and stays within the 0-1 square, emit a warning and place the overflow island into the next unit region (offset the pack target by +1 in u, so `[1,2]` acts as spillover) rather than silently overlapping a pin.

**Touches:** js/unwrap.js, js/uveditor2d.js, js/selection.js, js/history.js, js/app.js

**Open questions:**
- Are pins stored per-island (island identity is stable across re-pack) or per-vertex/per-shell — and how is island identity re-associated after a topology change so a pin doesn't drift onto the wrong shell?
- On overflow, is spilling into the `[1,2]` region acceptable for downstream texel/atlas assumptions, or should the packer instead shrink the free set (down-scale their weights) to fit inside 0-1?
- Should a candidate merely avoid the pinned AABB, or the true pinned polygon footprint (via `islandSignature` perimeter), accepting that AABB obstacles waste space around concave shells?

---

# Known bugs

## 5. Symmetric-unwrap parity (mirror UVs for L/R island pairs)  ⏭️ SKIPPED — NOT FIXED (2026-07-04)

**Status:** UNRESOLVED — two attempts, neither fully fixes it; both reverted / left partial. The
symmetry is better but the second attempt causes **UV overlaps**, so it is not shippable. Skipped to
move on; revisit later, likely with the solver-constrained approach below rather than a post-hoc fold.

**Attempt 1 — cross-island mirror-copy (`pairSymmetricIslands`, still in js/unwrap.js).** Detects two
SEPARATE mutual/equal-arity `SYM.f2f` island pairs and mirror-copies one onto the other
(`mirrorIslandUv`, deterministic `canonicalSource`), gated on `SYM.fraction >= SYM.minFraction` (0.9).
Regression `[sym-unwrap] L/R island pair mirrored` retained. This does NOT fix the reported bug,
because pressing **U** on Suzanne unwraps the body as ONE island that stays CONNECTED across the
mirror plane — there are no separate L/R islands to pair, so this pass is a no-op on the real case.

**Attempt 2 — within-island fold (implemented, then REVERTED).** After the solve, recover the UV
mirror line from the in-island SYM pairs (line normal = minor eigenvector of the pair-midpoint
covariance) and reflect+average each pair so `v` and `SYM.v2v[v]` are exact reflections. It DID make
the pair UVs mathematically symmetric (verified `pairErr ~5e-17` on the 202-pair body island), but
reflect-averaging pushes triangles across the fold line and flips winding, so islands **self-overlap**
in the atlas. Reverted (helpers `symmetrizeIslandUv` / `symmetrizeStraddlingIslands` /
`minorEigenvec2`, the `MIN_ISLAND_SYM_PAIRS` const, the `unwrap()` call, the export, and the
`[sym-fold]` smoketest block all removed) so no overlap regression ships.

**Why it's hard / correct direction:** no unwrap SOLVER (LSCM / ABF++ / SLIM — including Blender's)
yields symmetric UVs on its own; Blender gets symmetry via Copy → Paste Mirrored UVs (a post-process).
A naive post-hoc fold is overlap-prone. The robust fix is to constrain the SOLVE: pin a mirror PAIR
symmetrically and add mirror-equality constraints (u_v = axis - u_mirror, v_v = v_mirror) into the
LSCM normal equations so the flattening is symmetric AND non-overlapping by construction — a bigger
change to `lscmIsland`, deferred.

**Also still open (separate items):**
- **Box-projection symmetry** — `projectBox` (unwrap.js ~166) buckets faces by dominant-normal axis;
  float noise in `TOPO.faceNormal` near the mirror plane sends a face and its mirror into different
  axis buckets/planes (the "jagged misaligned" box result the user saw). Fix = force each `SYM.f2f`
  partner into the axis-mirrored bucket.
- **Lower-stretch solver** — LSCM → ABF++ or SLIM for less distortion (a QUALITY upgrade, orthogonal
  to symmetry).

**Request / bug:** "L and R perfectly symmetrical but one side unwraps better than the other." Decision: "Both, mirror preferred."

**Root cause / goal:** `lscmIsland` (unwrap.js ~211) pins the two farthest-apart verts of *that* island and Gauss-Seidel-solves each island independently, so a left island and its mirror-image right island are never guaranteed to converge to mirrored UVs. Goal: make SYM-detected L/R pairs pixel-identical by mirror-copying one side's solved UVs, with a deterministic-solve fallback for near-symmetric islands that have no detected pair.

**Approach:**
- Primary (mirror-copy): after the normal unwrap pass, walk islands and use `SYM.f2f` (and `SYM.v2v`) from `buildSymmetryMap` to detect pairs where every face of island A maps to a face of island B. For each detected pair, keep the already-solved UVs on one side (choose the canonical "source" side deterministically, e.g. side containing the lowest global vertex index via `islandVerts`), then write mirrored per-vertex UVs onto the partner: for each source vert `v`, look up `SYM.v2v[v]` and set its UV by flipping `u` about the pair's shared axis (`u' = axisU*2 - u`) while keeping `v` unchanged. Result is pixel-identical mirrored islands.
- Fallback (deterministic solve) for near-symmetric islands with no `SYM` pair: make `lscmIsland` pin selection mirror-stable — among the farthest-pair pin candidates, break ties by a canonical rule invariant under the symmetry map (lowest global vertex index from `islandVerts`) so mirror inputs pick mirror pins; raise the Gauss-Seidel iteration budget above the current 300 and tighten `tol` below 1e-7 so convergence is repeatable rather than seed-dependent.
- Add a small `mirrorIslandUv(sourceIsl, targetIsl, axisU)` helper and a `pairSymmetricIslands(islands)` pass invoked from the unwrap driver, gated on `SYM.fraction >= SYM.minFraction`.

**Touches:** js/unwrap.js, js/selection.js (SYM.v2v/f2f, buildSymmetryMap, islandVerts).

**Open questions:**
1. Which side is the canonical "source" to solve-and-mirror (lowest-vertex-index side, +axis side, or larger-area side), and should the user be able to flip it?
2. Should mirror-copy trigger automatically whenever `SYM` pairs are detected, or be an opt-in toggle so users can keep independent per-side unwraps?
3. What `SYM.fraction` threshold and per-pair face-match tolerance count as a "real" pair before mirroring (vs. falling back to the deterministic solve)?

---

## 6. Edge-loop selection uses quad topology, not "straightest" heuristic  ✔️ DONE (2026-07-03)

**Status:** Implemented in `js/selection.js`. `computeLoopEdges` now walks the vertex-centric
quad-loop rule (`walkLoopDir` → `loopContinueAtVert`: at a valence-4 vertex, continue to the edge
sharing NO face with the current edge) from both endpoints, falling back to the old straightness
walk (`walkStraightestDir`) only where topology is irregular (poles/boundaries/non-quad). Both
`selLoopFromEdge` and `loopEdgeSet` call the shared core. Verified: all 863 pure-rule loops on
Suzanne are facing-independent (identical set from any seed edge); locked by a smoketest regression
assertion (`[loop] pure quad-loop rule: 863 loops tested, 0 facing-dependent`). This is the shared
quad-walk foundation #7 and #11 build on.

**Request / bug:** "select loop after loop... depends which edge im facing" — the edge-loop hotkey walks by geometric straightness, so seeding from different edges (or on cylinder caps/poles) gives inconsistent, wrong loops.

**Root cause / goal:** `selLoopFromEdge` (selection.js ~414) and `loopEdgeSet` (~610) run an IDENTICAL greedy walk that continues to the edge whose direction has the max dot with the incoming direction (stop when `bestScore < 0.3` or on revisit). This is a straightness heuristic with no notion of quad topology, so on caps/poles the "straightest" continuation is ambiguous and the result flips with seed edge/facing. The real fix is the standard edge-loop rule: continue across a quad to the OPPOSITE edge.

**Approach:**
- Write one shared walker (e.g. `walkQuadLoop(seedKey)`) that both `selLoopFromEdge` and `loopEdgeSet` call, replacing the duplicated straightness loop in both.
- At each step, from the current edge `edgeVerts[k]=[a,b]` take an adjacent quad via `edgeFaces[k]`; require `faceVerts[f].length === 4` (quad). Use `faceEdges[f]` to find the OPPOSITE edge — the one of the four whose `edgeVerts` shares NO vertex with the current edge — and continue through it.
- Advance across both edge-faces to grow the loop in both directions; stop on revisit (`edgeKeys`/visited Set) or when the loop closes.
- Pole/tri fallback: if the adjacent face is not a quad, or no unique opposite edge exists, either stop that direction or fall back to the existing straightness heuristic (keep that code path as `walkStraightestLoop` so behavior is preserved where topology is irregular).
- Keep `selConnectPath` (~549) EDGE-mode using the shared `loopEdgeSet` so two-pick loop selection inherits the same quad walk.

**Touches:** js/selection.js

**Open questions:**
1. On mixed quad/tri topology, stop hard at the first non-quad or fall through to the straightness heuristic for the remaining span?
2. When a quad has two candidate opposite edges after a prior irregularity (n-gon), pick by best-dot tiebreak or bail?
3. Should the quad walk be the default for the loop hotkey with straightness as an explicit modifier, or auto-detect quad-dominance per seed?

---

## 7. Two-pick connect selects the full loop, not the shortest chord  ⏭️ DEFERRED (skip for now, 2026-07-04)

**Decision so far:** when both picks lie on a loop, select the SHORTER of the two loop arcs
(user choice, 2026-07-03). Reuses the #6 quad-loop walk via an ordered-vert-cycle helper
(`orderedLoopVerts` / `loopArcBetween`). Not yet implemented — skipped to move to #8.

**Request / bug:** "select loop after loop selects shortest path instead of the whole round path" — on a cylinder cap, picking vert 1 then vert 32 selects the 1..32 chord across the cap instead of the round loop.

**Root cause / goal:** In `selConnectPath` (selection.js ~549), VERTEX mode unconditionally calls `shortestVertPath` (Dijkstra on the one-ring), so two co-loop verts resolve to the straight chord. EDGE mode only keeps the loop when `loopEdgeSet(anchor)` happens to contain `newId`, otherwise it falls back to `bestVertPath` (also shortest). Neither mode ever walks the long way around a ring. Goal: when both picks lie on a common loop/ring, prefer the LOOP ARC between them; fall back to shortest-path only when they are genuinely not co-loop.

**Approach:**
- Add a `loopArcBetween(anchorId, targetId)` helper that, in VERTEX mode, derives the vert sequence of the loop through `anchor` (reuse the `loopEdgeSet` walk, mapping its edges to an ordered vert cycle) and, if `target` is on that cycle, returns the arc of verts walking from `anchor` toward `target` in the direction that actually reaches it — so a 32-vert cap gives the long way round when that is the loop.
- In `selConnectPath` VERTEX mode: try `loopArcBetween` first; only call `shortestVertPath` when `anchor` and `target` share no common loop cycle.
- In `selConnectPath` EDGE mode: when `loopEdgeSet(anchor)` does NOT already contain `newId`, walk the ordered loop-edge cycle for the arc between the two picked edges before falling back to `bestVertPath`.
- Reuse existing `loopEdgeSet` (selection.js ~610) for the greedy loop walk and keep `shortestVertPath` / `bestVertPath` as the non-co-loop fallback; share one arc-extraction routine across both modes.

**Touches:** js/selection.js

**Open questions:**
1. When two picks lie on the loop, choose the shorter arc of the two directions, or always the direction that keeps the greedy walk continuous (the "long way" the bug report wants)?
2. On an ambiguous cap where the greedy `loopEdgeSet` walk itself forks (the seed-dependent straightness heuristic), which loop wins if `anchor` sits on more than one candidate cycle?
3. If `target` is near but not exactly on the loop cycle (off-by-one from a non-manifold vert), snap to the nearest loop vert or fall straight through to `shortestVertPath`?

---

# Features

## 8. Selection preview helper (highlight what a hotkey will grab)  ✔️ DONE (2026-07-04)

**Status:** Implemented. While a loop-modifier (Ctrl/Alt/Cmd) is held and hovering, the viewport
ghost-draws the FULL loop/ring a click would grab, in soft amber under the crisp single-element
hover highlight — so the user sees which edges/faces get selected before committing.
- `js/selection.js`: extracted `computeFaceBand(seed)` as the pure (non-mutating) core of
  `selRingFromFace`; edge preview reuses `loopEdgeSet` (the #6 quad walk).
- `js/viewport3d.js`: added `VP3.preview` state; `vp3HoverAt(mx,my,loopMod)` builds it via
  `buildLoopPreview()` (edge→`loopEdgeSet`, face→`computeFaceBand`, vertex→none since its ctrl path
  is a two-pick path); rendered in `vp3DrawOverlays` (depth-clipped, amber). Cleared on modifier
  release (keyup), mouseleave, and commit (mouseup). `previewSig()` gates hover redraws.
- Verified: preview set equals the committed pick's set, never mutates SEL, clears without the
  modifier — locked by a smoketest regression assertion (`[preview] loop/ring hover ghost ...`).
- Deferred from scope: the vertex two-pick path preview and the seed-direction arrow (open Q3) —
  the loop/face facing ambiguity the user reported is fully addressed by the loop/ring ghost.

**Request:** "add a helper so we know which edges/faces/verts will be selected -- an arrow or highlight." Before a loop/ring/grow hotkey commits, show WHAT it will select so there's no confusion about facing/direction.

**Goal:** No preview exists today — `selLoopFromEdge`/`loopEdgeSet` (selection.js ~414/~610) and `selRingFromFace` (~644) mutate SEL immediately on the hotkey, and their greedy straightness walk is seed/facing dependent, so the user cannot tell what a pick will grab until after it commits. Add a non-mutating hover preview that computes and ghost-renders the prospective set for the active tool, committing only on click.

**Approach:**
- Refactor the walk cores into pure functions that return a Set without touching SEL: extract `computeLoopEdges(seedKey)` from the shared body of `selLoopFromEdge`/`loopEdgeSet`, and `computeRing(seedFace)` from `selRingFromFace` — the existing callers then wrap these and commit to SEL.
- For two-pick path preview reuse `shortestVertPath`/`bestVertPath` (vertex mode) and `shortestFacePath` (face mode) from `SEL.lastPick` to the hovered element.
- Add `previewSelection(mode, hoveredId)` in selection.js that dispatches on `SEL.mode` + active tool to the right compute* fn and returns `{verts,edges,faces}` plus a `seed` id — no SEL writes.
- In viewport3d.js add a `PREVIEW` ghost state, populated on hover (or while a modifier such as Shift is held); clear it on mouseleave/commit. Render it in the existing overlay pass as a distinct ghost color/alpha for edges/faces/verts.
- Optional `drawSeedArrow(seed, dir)`: a short direction arrow at the seed edge/face using the walk's incoming-dir vector to disambiguate facing.
- app.js wires the hover handler + modifier state and calls `previewSelection`; click path calls the existing committing selectors so behavior is unchanged on commit.

**Touches:** js/selection.js, js/viewport3d.js, js/app.js

**Open questions:**
1. Always-on hover preview vs. only while a modifier (Shift/Alt) is held — always-on may feel noisy in dense meshes.
2. Should the preview reflect additive/subtractive intent (Shift-add, Ctrl-remove) by tinting the ghost differently?
3. Do we preview the seed-direction arrow for vertex two-pick paths too, or only for loop/ring where facing ambiguity is the actual reported pain?

---

## 9. Island packing-weight control: slider + percentage + xN multiplier  ✔️ DONE (2026-07-04)

**Status:** Implemented (share model, 2026-07-04). The Importance section offers the preset pills
PLUS fine controls: a **0–100% atlas-share slider + % field** (100% = this island fills the box,
others shrink toward nothing) and an **×N raw-multiplier** escape hatch — all views onto the single
stored `isl.weight`. (First pass made the slider a log-weight 0.25×–8× control; that capped the
reachable share at ~8%, so per user feedback the slider + % were rebased to a direct 0–100% atlas
share and `WEIGHT_MAX` raised from 32 to 1e6 so high shares are actually reachable through the
`percentToWeight` solve. `sliderToWeight`/`weightToSlider` remain in unwrap.js for reference.)
- `js/unwrap.js`: added pure conversions `sliderToWeight`/`weightToSlider` (log-spaced so equal
  drag = equal ratio), `percentToWeight`/`weightToPercent` (share = weight ÷ Σweights, since
  `measureBoxes` makes atlas area ∝ weight), plus `WEIGHT_SLIDER_MIN/MAX` (0.25/8); all exported.
- `index.html` + `css/props.css`: slider + `weightMult` (×N) + `weightPct` (%) fields under the
  pills, styled to match the panel-3 rounded controls.
- `js/app.js`: each control writes `isl.weight` via the existing `uvedSetWeight` (clamped 0.1–32),
  repacks, then `syncWeightPreset` re-derives the OTHER views (skipping the focused field) and the
  pills; all edits toast + `recordHistory("weight", …)` for undo/redo. `uvedRenderTree` calls
  `syncWeightPreset` so the fields refresh on selection change.
- Verified by a smoketest regression (`[weight] slider/xN/% views round-trip and stay consistent`):
  slider round-trips are identity in-range, log-spacing holds (equal-position deltas = equal
  ratios), ends clamp to min/max, and the %↔weight round-trip matches the real atlas share.
- Open Qs resolved for this pass: weight is edited per selected island (all selected share the
  value, matching the pre-existing pill behavior); % is a soft target solved against the OTHER
  islands' current weights (no forced live renormalization).

**Request:** Replace the fixed x32 weight drag with user-selectable input styles — a drag slider, a numeric percentage, and the existing xN multiplier — "offer all three, user picks the style." All three are equivalent views of one number.

**Goal:** Keep `isl.weight` as the single source of truth (`measureBoxes` ~370 already scales each island's linear box size by `sqrt(weight)`, so atlas area grows linearly with weight), and expose three interchangeable UI editors that all write that same `isl.weight` and re-pack.

**Approach:**
- Keep `isl.weight` (read by `islandWeight` ~362, consumed by `measureBoxes` ~370) as the one stored number; the three inputs are just views onto it.
- xN field: raw multiplier — writes `isl.weight = n` directly (current behavior, retained).
- Slider: log-spaced map over a range (e.g. 0.25x..8x) via a `sliderToWeight`/`weightToSlider` pair so equal drag distance = equal ratio; writes `isl.weight`.
- Percentage field: target atlas-area share; convert with `percentToWeight(share, islands)` — solve `isl.weight` so its `measureBoxes` area fraction matches `share` relative to the summed weights of the other islands; inverse `weightToPercent` refreshes the field after any edit.
- On any edit, write `isl.weight`, re-derive and repaint the other two fields from it, and re-run `packIslands(islands, opts)` (`packGrid`/`packShelf`/`packSkyline`) on the current island set.
- UI hook in `app.js` toolbar (three linked controls per selected island); redraw through `uvedDrawFG` (~217); route the weight change through `history.js` so it undoes/redoes.

**Touches:** js/unwrap.js, js/uveditor2d.js, js/app.js, js/history.js, index.html

**Open questions:**
1. Slider range and whether it is per-island or a global scale applied to the selection.
2. When percentage is set on one island, do the other islands' percentages renormalize live (must sum to <=100%) or is it a soft target that only nudges relative weight?
3. Is weight edited per-selected-island, or broadcast to all selected islands at once?

---

## 10. Advanced shape-aware packing (beyond bounding boxes)  ✔️ DONE (2026-07-04)

**Status:** Implemented as a FOURTH packer `packShapeAware` registered in `PACKERS`
under the `shapeAware` key, added ALONGSIDE grid/shelf/skyline (the box packers stay
the default — shape-aware is the "Shape" pill, opt-in). It packs each island's true
FOOTPRINT, not its AABB, so concave / L-shaped / diagonal shells interlock.
- **Mask build** (`shapeIslandMask`, unwrap.js): rasterizes the island's fan
  triangles (`TOPO.faceTris` × `isl.uv`) into a small occupancy bitmap, sized from
  the box's weighted `(bw,bh)` so importance still scales area exactly like
  `measureBoxes`. `SHAPE_MASK_MAX`=40 caps a single mask; `SHAPE_GRID`=128 is the
  shared grid. A degenerate rasterization falls back to a solid box (island never
  dropped). `shapeRotateMask(mask, steps)` gives the four 90° orientations.
- **Bin-pack** (`packShapeAware`): largest-mask-first, bottom-left scan trying all 4
  rotations; a pad-dilated mask (Minkowski-grow by `margin` cells, computed ONCE per
  rotation) makes the per-slot `fits` test a plain occupancy overlap, and a per-column
  skyline (`colTop`) starts the upward scan at the lowest possible row + prunes
  candidates that can't beat the incumbent. ~11× faster than the naïve inner-pad scan
  (564ms → 49ms for Suzanne's 13 smart-unwrap islands).
- **Write-back** (`shapePlaceIsland`): maps each vert's native-bbox UV through the
  chosen rotation into the placed grid slot, then a global scale normalizes the packed
  extent into the 0-1 square.
- **UI**: a "Shape" pill added to `#packMode` in `index.html`; `app.js`'s existing
  generic `data-pack` → `setPackMethod` handler wires it with no code change.
- Verified (`[shapepack]`): mask rotation swaps dims + preserves filled count and is
  4-cycle-identity; a real island footprint mask is partial (< its bbox);
  `packShapeAware` produces no NaN UVs, keeps every island inside 0-1, and is
  deterministic from the same starting footprints; and a two-L-tromino case tiles a
  2×3 block (6 cells) where box packing needs two 2×2 AABBs (8 cells) — the concave
  interlock the feature exists for. Full smoketest passes.
- **Note:** on near-rectangular islands (Suzanne's smart charts) the box packers can
  BEAT shape-aware (41% vs 35.5% atlas coverage same-footprint) — no concave gaps to
  reclaim, and shape-aware pays mask-grid quantization + inter-mask padding. That's
  expected and why the box packers remain the default; shape-aware wins on genuinely
  concave/L-shaped layouts.
- **Deferred (open Qs + build-order):** pin obstacles (#4) and UDIM per-tile targets
  (#3) are NOT yet honored — `packShapeAware` packs the whole 0-1 square; when #4/#3
  land, seed pinned masks into `occ` first and run per tile rect. Uses a simple raster
  occupancy scan (with 90° rotation), not a true no-fit-polygon. Mask resolution is a
  single global `SHAPE_GRID`/`cellsPerUnit`, not per-island adaptive. Mirror-symmetry
  (#5) not integrated (islands packed independently).

**Request:** "current uses bounding box which wastes space" — the packers pack weighted bounding boxes, wasting the room around concave/diagonal islands, and they don't respect which islands the user wants big.

**Goal:** `packGrid`/`packShelf`/`packSkyline` (unwrap.js ~393/411/432) and `packIslands` (~486) all pack the axis-aligned weighted boxes from `measureBoxes` (~370), so any island that is L-shaped, diagonal, or concave reserves a full rectangle and the negative space is dead. Add an optional shape-aware packer that packs the island's true FOOTPRINT (not its AABB) so concave shapes interlock, while still honoring `isl.weight` for target size and any pinned islands.

**Approach:**
- Build a per-island occupancy bitmap by rasterizing `faceTris` into a small grid (reuse the fan-tri walk from `uvedDrawFG` ~217 and the footprint in `islandSignature` — perimeter + fan-tri area, uveditor2d.js ~526) — `islandOccupancyMask(isl, cellSize)`.
- Size each mask from `islandWeight(isl)` (~362) the way `measureBoxes` scales by `sqrt(weight)`, so area still grows linearly with weight; keep `measureBoxes` as the box path.
- Add `packShapeAware(islands, opts)`: a per-island occupancy-mask bin-pack that scans the 0-1 grid for the first free placement of each mask, trying rotation in 90-deg steps, so concave/diagonal masks interlock; seed placement of any pinned islands first (see #4) and mark their cells occupied before packing the rest.
- Register it in `PACKERS` under a `PACK_METHOD` key (e.g. `'shapeAware'`) alongside grid/shelf/skyline; keep those as the fast default and expose shape-aware as the "advanced models" option in `packIslands`.
- Honor UDIM tile targets (see #3) by running the pack per target tile rect rather than only the 0-1 square.

**Touches:** js/unwrap.js, js/uveditor2d.js, js/app.js, index.html

**Open questions:**
- Grid resolution for the occupancy mask — one global `cellSize`, or per-island adaptive from `islandSignature` area to bound the pack cost?
- No-fit-polygon vs. simple raster occupancy-mask scan — is the raster mask (with 90-deg rotation) enough for the "advanced" tier, or do concave interlocks need true NFP?
- Should shape-aware packing preserve L/R mirror symmetry (place a mirrored island as the flipped mask of its pair, see #5) or pack each island independently?

---

## 11. 1-button grid-align / square-up stretched faces (follow-active-quads)

**Request:** A one-button operation that takes a selected quad region whose UVs are stretched/skewed and snaps them into a clean regular grid of near-perfect squares — "the outcome of Blender's Follow Active Quads but as a single button, no multi-step process."

**Goal:** Given a quad-dominant selected region, walk the quad grid, assign each quad an integer `(i,j)` lattice cell from its position in the walk, and rewrite per-vertex UVs onto that lattice so stretched/skewed faces become regular squares.

**Approach:**
- New `gridAlignQuads(faces)` in `unwrap.js` (near the island/pack helpers): read `SEL.faces` as the active region and confirm it is quad-dominant via `TOPO.faceVerts[f].length === 4`; skip or triangle-fan tris/poles.
- Walk the quad grid off an active seed quad using `TOPO.faceVerts` loops + `TOPO.faceEdges` adjacency with quad opposite-edge continuation — reuse the SAME quad-topology walk from the edge-loop fix (#6), not the geometric-straightness heuristic; assign each reached quad an `(i,j)` cell by stepping the two edge-loop axes.
- Build a per-vertex lattice-coordinate map so shared verts between adjacent quads land on the same lattice corner (dedupe by vertex id), then write results into `isl.uv` (the same target `lscmIsland` writes).
- Offer a toggle: `forceSquares` (exact unit cells) vs `keepProportions` (scale each cell's width/height by average 3D edge length from `TOPO.edgeVerts` positions) so real aspect ratios are preserved.
- Wire a single toolbar button + toggle in `app.js`, push a history entry, and redraw via `uvedDrawFG()` in `uveditor2d.js`.

**Touches:** js/unwrap.js, js/selection.js, js/app.js, js/uveditor2d.js, index.html

**Open questions:**
1. When the walk hits a tri/pole/T-junction that breaks the lattice, do we stop the region at that boundary, or split into multiple aligned sub-grids?
2. Should the aligned grid be re-anchored/scaled to the region's existing UV bounds (stay in place) or normalized to `0..1`?
3. Does this depend on the edge-loop quad-walk fix (#6) shipping first, or do we implement a local quad-walk here and share it back later?

---

## 12. Import button (noop placeholder)

**Request:** "add an Import button to the toolbar that is currently a NO-OP" — model loading is out of scope for now (baked in via `mesh-data.js`); keep it obviously a placeholder so it can later be wired to an OBJ parser.

**Goal:** Reserve the toolbar affordance and intent for model import now, deferring the actual OBJ-loading pipeline. Today the mesh is generated offline by `tools/obj_to_meshdata.js` into `mesh-data.js` and `index.html` has no import control at all.

**Approach:**
- Add an `importBtn` button to the existing toolbar row in `index.html`, reusing the current control class/styling so it matches the other toggles/buttons.
- Wire it in `app.js` with a stub handler `onImportClick()` bound where the other toolbar buttons register their listeners; the handler does nothing meaningful yet.
- Make the placeholder obvious: either route through the existing toast/status mechanism to show "Import not yet implemented", or visually mark `importBtn` disabled/dimmed.
- Leave a one-line comment in `onImportClick()` naming the future entry point (parse OBJ -> feed the same shape `mesh-data.js` exports -> `buildTopology`) so the wiring target is unambiguous.

**Touches:** index.html, js/app.js

**Open questions:**
- Toast/status message vs. a visibly disabled button — which reads more clearly as "coming soon"?
- Should the button sit at the left of the toolbar (file-level action) or grouped with the other mode toggles?

---

## Build-order notes

- **Packing chain (#4 ✔ → #3 ✔ → #9 ✔ → #10 ✔) — COMPLETE.** Pinning (#4) and UDIM (#3) shipped
  together (2026-07-04) alongside a DCC-style **pixel margin** (px @ 512/1024/2048/4096 resolution →
  fractional pack gap; `PACK` state + `pixelsToMargin`/`marginToPixels`/`setPackMargin` in unwrap.js;
  `marginPx:0` preserves the old 0.01 default). `packIslands` now splits pinned/free, seeds pins as
  obstacles (AABB in the box packers, TRUE occupancy mask in `packShapeAware`'s `occ` grid — the
  #10-deferred seam is now filled), disables the 0-1 rescale when pins exist, packs per UDIM tile, and
  returns `{islands, overflow}`. The weight UI (#9) remains an independent view onto `isl.weight`.
  Two new per-island fields (`pinned`, `tile`) are threaded through every clone site + history.
- **Overlay chain (#1 + #2):** checker (#1) and stretch (#2) share the per-pixel / per-tri overlay plumbing in `viewport3d.js` and `uvedDrawFG()` — land the barycentric-UV interpolation once and reuse it for both. UDIM (#3) makes both tile via `frac(u)/frac(v)`.
- **Quad-topology chain (#6 → #7, #11):** the quad opposite-edge walk from the edge-loop fix (#6) is the foundation reused by two-pick loop-arc (#7) and grid-align (#11). Build #6 first as a shared `walkQuadLoop`, then #7 and #11 consume it. The selection preview (#8) wraps the same pure walk functions.
- **Symmetry (#5):** reuses the existing `SYM` map; independent of the above, but its mirror-copy pass also benefits shape-aware packing (#10) if mirrored islands are packed as flipped masks.
- **#12 (import noop):** trivial and standalone; do any time.
