# World-native 2D replacement plan

## Goal
Replace the remaining sketch-basis 2D authoring path with a world-native workplane path, while keeping `WorldSketchStructure` as the live authoring authority and preserving the sketch document only as a compatibility mirror for records, revisions, outliner state, and legacy dimension fallback.

## Immediate implementation scope
1. Stop deriving draw-plane authority from `SketchStructure`.
2. Make active `WorkplaneCatalogue` state the input/preview/commit plane for draw tools.
3. Let orthographic `XY/XZ/YZ` views switch the active standing workplane even after geometry already exists.
4. Keep rendering and selection world-backed.
5. Keep legacy sketch fallback paths resynchronising world state.
6. Preserve proof coverage for placement, bridge, drawing, rendering, and workplane behaviour.

## Code plan
### 1. Resolve workplane-native draw basis
- Add a reusable helper to turn `Workplane` into a `SpatialBasis`.
- Use that basis in the editor viewport draw path instead of `ResolveSketchBasis(Sketch)`.

### 2. Make world-backed placement commit take explicit workplane support
- Change `CommitPlacementWorldBacked(...)` so the caller passes the active `Workplane`.
- Use that explicit workplane to author support metadata and mirrored profile planes.
- Stop inferring support for new geometry from the sketch’s one global plane.

### 3. Keep sketch as compatibility only
- Only seed `Sketch.DeclarePlane(...)` when a sketch still has no declared plane.
- Do not require active plane changes to rewrite the sketch’s global plane once geometry already exists.
- Continue mirroring sketch-only fallback mutations back into persistent world state.

### 4. Improve sketch->world remirroring
- When rebuilding world state from sketch compatibility data, prefer each profile’s own stored plane for its member curves.
- Fall back to the sketch plane only for geometry with no profile-owned plane.
- This keeps fallback remirroring from collapsing all profile support back onto one legacy plane.

### 5. Orthographic plane/grid behaviour
- Always allow standing-plane activation from exact orthographic views now that world geometry is authoritative.
- Show the analytic grid only in exact orthographic plane views.
- Keep perspective free of forced workplane-grid presentation.

## Acceptance targets for this implementation
- Draw preview and placement land on the active workplane rather than the sketch basis.
- World-backed draw commit stores the active workplane’s support frame directly.
- Existing world geometry does not move when the active workplane changes.
- Switching to exact `XY`, `XZ`, or `YZ` ortho changes where new drawing lands, even after prior geometry exists.
- Sketch compatibility paths still sync back into persistent world state.
- Proofs stay green.

## Validation plan
- Strict compile:
  - `WorldSketchBridge.cpp`
  - `SketchInteraction.cpp`
  - `EditorHost.cpp` with `-DSLATE_COMBINED_AUTHORING`
- Proofs:
  - `WorldSketchPlacementCommitProof`
  - `WorldSketchBridgeProof`
  - `WorldSketchInteractionProof`
  - `WorldSketchTransformSessionProof`
  - `WorldSketchEditingProof`
  - `WorldSketchPickingProof`
  - `WorldSketchRenderProof`
  - `WorldSketchFoundationProof`
  - `SketchDrawingProof`
  - `WorkplaneCatalogueProof`
