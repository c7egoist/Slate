# Sketch Scene Viewport and Editor Panel Integration Plan

## Source-of-truth decisions

- `PaintHost` is standalone for paint-focused documents such as `.pigment`.
- `ParametricSketchHost` is standalone for sketch-focused documents such as `.sketch` and sketch-contained scene references.
- `EditorHost` is the combined host. It may expose paint and sketch panels together when the opened document/profile supports them.
- `WorkspaceCodex` (`.codex`) is the general binary workspace container.
- `SketchCodex` (`.sketch`) carries constraint sketches and construction geometry.
- `PigmentCodex` (`.pigment`) carries paint layers and paint-channel content.
- Do not put texture-paint layer-stack logic in `ParametricSketchHost`.
- Do not put CAD/sketch logic in `PaintHost`.
- Use compile/profile gates for combined-editor composition once host roles are split further.

## Panel naming and hierarchy

The UI has two separate hierarchies:

1. `Scene Directory`
   - scene-facing hierarchy;
   - objects, lights, cameras, folders, imported mesh placements, and references to CAD records;
   - formerly shown as `Outliner`.

2. `Sketch Directory` / `Parametric Directory`
   - exact CAD/sketch hierarchy;
   - points, curves, closed profiles, surfaces, solids, dimensions, constraints, operations, and CAD folders;
   - backed by `WorkspaceRecordStructure` and `WorkspaceDirectoryProjection`.

The construction catalogue panel should be exposed as:

- standalone sketch host header: `Construction Catalogue` where appropriate;
- editor panel/dropdown title: `Parametric Tools`.

Panel dropdown target names:

- `Scene Directory`
- `Sketch Directory`
- `Parametric Tools`
- `Layer Stack` only for paint-capable/editor contexts
- `3D Viewport`
- `UV Editor` where still applicable

## CAD references in Scene Directory

CAD records are not duplicated into the scene hierarchy.
Scene Directory rows for CAD-owned geometry are references back to `WorkspaceRecordName`.

Initial automatic references:

- `ClosedProfile`
- `ThinSurface`
- `Solid`

Do not automatically reference:

- `Point`
- `OpenCurve`
- `Dimension`
- `Constraint`
- `Pattern`
- `Mirror`
- CAD-only folders

Reference behavior:

- rename from Sketch Directory updates Scene Directory projection;
- rename from Scene Directory routes to the CAD record;
- visibility and lock should route to the authoritative CAD record first;
- viewport edits of CAD references route through exact sketch/CAD editing paths.

## Imported mesh-file scene placement

There are no standalone OBJ scene files in `EngineContent` for the current target.
The current visible scene proof is `WhiteTeaService.codex`, a binary workspace container.

Rules:

- do not expose fake catalogue entries such as `Hangar_Interior.fbx`;
- remove hardcoded reference/demo entries from the runtime content catalogue;
- keep scene-loadable runtime content focused on `WhiteTeaService.codex` until real files are added;
- keep source OBJ generation files out of `EngineContent` after the codex proof exists;
- scratch/generated interchange sources may live outside `EngineContent` when needed by agent tooling.

## Unified viewport rule

There is one viewport/camera space in the sketch host and in the sketch-capable editor layout.
It renders, in order:

1. scene polygon geometry from the opened binary workspace/profiles;
2. CAD/sketch geometry;
3. grid, guides, selection, and gizmo overlay;
4. UI shell, drawers, and deferred popups.

Implementation may use specialized internal passes inside the same viewport/frame:

- scene mesh pass;
- CAD pass;
- overlay pass.

The forbidden architecture is separate CAD and scene viewports/worlds for the same sketch workspace.

## Implementation task list

### Phase 1 — panel names and Sketch Directory subject

Status: started in implementation.

- Rename the displayed `Outliner` panel to `Scene Directory`.
- Rename the displayed `Construction` panel to `Parametric Tools`.
- Add `PanelSubject::SketchDirectory`.
- Add chooser/dropdown entries for `Sketch Directory`.
- In `ParametricSketchHost`, make the default left panel `SketchDirectory` instead of `Outliner`.
- Route `SketchDirectory` to `ParametricWorkspacePanel`.
- Reserve `Outliner`/`Scene Directory` for scene-reference rows.

### Phase 2 — editor host sketch panels

Status: started in implementation with editor dropdown routing and empty-panel hosting.

- Add `ParametricWorkspacePanel` and `ParametricToolsPanel` to `EditorHost`.
- Add sketch directory and tool contexts to `EditorHost`.
- Route editor leaves:
  - `Scene Directory` -> `SceneDirectoryPanel`;
  - `Sketch Directory` -> `ParametricWorkspacePanel`;
  - `Parametric Tools` -> `ParametricToolsPanel`.
- Keep paint-specific `Layer Stack` routing out of `ParametricSketchHost`.

### Phase 3 — runtime content catalogue cleanup

- Remove fake hardcoded entries from `ContentBrowserReferenceCatalog.inc`.
- Expose `WhiteTeaService.codex` as the scene-loadable runtime proof.
- Stop advertising OBJ scene loading in the sketch host until a real mesh-placement path exists.
- Keep the binary `.codex` as the shipped proof.

### Phase 4 — Sketch-scene references

Status: started in implementation with Scene Directory row projection.

- Add host-side scene reference records for sketch workspaces.
- Project renderable CAD records into Scene Directory as references.
- Keep exact CAD ownership in `WorkspaceRecordStructure`.

### Phase 5 — binary scene loading into unified viewport

Status: started in implementation with `WhiteTeaService.codex` activation into Scene Directory rows, shared viewport focus state, and a first in-viewport codex scene proxy render.

- Load `WhiteTeaService.codex` through the existing codex activation path.
- Register scene rows in Scene Directory.
- Render codex scene geometry and CAD geometry in the same viewport/camera path.
- `WhiteTeaService.codex` now carries an embedded binary mesh section (`MESH`) for the tea-service proof.
- The first viewport render reads that binary mesh payload and draws white-dielectric triangles in the same viewport path; the dedicated Vulkan `WorkspaceScenePass` remains the next GPU-residency step.

### Phase 6 — real 3D gizmo and CAD orientation cube

Status: started in implementation. The transform gizmo now emits world-space/projection-based geometry into the existing viewport overlay pass instead of fixed 2D glyphs, preserving the CAD/grid/scene ordering while the future GPU gizmo pass is designed.

- Move mode: arrows, cone heads, cylinders/shafts, axis planes, and white screen-space move handle.
- Scale mode: axis boxes, planar boxes, and white screen-space scale handle.
- Rotate mode: axis rings/tori/arcs and white screen-space rotate ring.
- Keep Blender command chains: `G`, `G Z 50`, `S X 2`, `R X 90`, and double-tap `G G` for line/curve slide.
- CAD orientation cube: top-right projected cube has distinct face, edge, and corner hit regions; visible faces use the existing baked font path for labels and the remaining face labels continue through the orientation buttons.
- Viewport gizmo-style dropdown keeps both `Blender Gizmo` and `CAD Gizmo`; chosen style controls both handle presentation and orientation-cube behavior.

### Phase 7 — selection sync

- Scene Directory CAD reference selection selects the underlying CAD record.
- Sketch Directory CAD selection highlights matching Scene Directory references when present.

### Phase 8 — 2D CAD tool completion

Status: started in implementation. The viewport drafting path now covers the first expanded entity set: line, polyline, rectangle, circle, three-point arc, and aligned linear dimension placement from snapped sketch references. Snapping is now on by default while drafting; Command temporarily bypasses snap for free placement.

- Basic sketch entities: line, polyline, rectangle/profile, circle/profile, three-point arc, point markers, ellipse/profile, and Bezier drafting are active in the parametric viewport.
- Snapping: endpoint, midpoint, centre/control, along-curve, intersection, and fallback grid snaps are resolved by the CPU sketch snap path during drafting; snap glyphs now use distinct colours by snap subject.
- Dimensions: the Linear Dimension tool can place an aligned dimension between two snapped sketch references and records a dimension row in the Sketch Directory; circle profiles also record a radius dimension.
- Constraints: horizontal/vertical and coincident constraints are emitted from line/polyline drafting when the committed geometry and snapped references imply them.
- Editing: trim, extend, offset, fillet/chamfer preparation, mirror, and linear-array hooks are wired to selected curves and produce new sketch records while preserving CPU-authoritative geometry.
- Hardening pass added dedicated constraint toolbar entries for horizontal, vertical, coincident, parallel, perpendicular, tangent, equal, midpoint, symmetry, and concentric constraints; applying supported constraint entries creates constraint rows and invokes the constraint solver immediately.
- Viewport validation now reports valid profiles and constraint health in the sketch viewport, and the viewport draws compact constraint glyphs at sketch references.
- The constraint solver now runs a bounded multi-pass solve over the sketch constraint set and handles projected line/curve tangent contact plus circle-circle external tangency as practical hardening, not full industrial constraint solving.
- The C++ snap resolver now promotes nearest interior projection to perpendicular snap and adds circle tangent candidates.
- Dimension rows can be selected and edited with typed numeric input; selected sketch records can be deleted through the viewport edit path with revision sealing.
- Trim/extend now snap the edit probe to the nearest sketch intersection before committing the record operation.
- Visual/code validation proof lives at `References/Cad/SketchHardeningValidation.svg` and is generated by `Tools/ValidateSketchHardening.py`; the proof now covers 13 checks including non-contact tangent projection and circle-circle tangent movement.
- Profile area validation now detects open chains, highlights gaps, marks self-intersections, classifies outer loops versus holes, draws fill preview triangles for valid closed areas, and auto-declares profile records from closed curve chains. The profile area module exposes Clipper2/earcut backend seams with local deterministic fallbacks when the third-party headers are not present in the sandbox checkout.
- Sketch entity polish added visible tool entries and draft commits for elliptical arcs, basis splines, construction lines, center rectangles, three-point rectangles, diameter circles, three-point circles, center/start/end arcs, tangent-arc entry routing, polygons, and slots.
- Mesh import / scene path now accepts OBJ, glTF text MVP, GLB MVP, ASCII FBX mesh arrays, STL, and ASCII PLY through `SceneMeshImport`; selected imports become embedded workspace scene meshes, Scene Directory rows, selectable viewport objects, material-slot-tagged entries, and transformable scene entries. A dedicated `WorkspaceScenePass` upload boundary now stands beside CAD/overlay while the host keeps the CPU fallback renderer active until scene shaders are landed.
- Next: complete real midpoint/symmetry/concentric solver subjects, selected-boundary extend UI, split/duplicate/undo-redo transaction surfacing, CAD-grade offset/fillet/chamfer previews, deeper mode-specific property editing for the expanded sketch tools, and full shader-backed scene pass rasterisation.
- Scene geometry selection selects Scene Directory rows.
- CAD point/control selection remains exact and CPU-authoritative.
