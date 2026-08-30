# Parametric Workspace and Host Plan

## Goal

Add a dedicated `ParametricSketchHost` executable and a dedicated `ParametricWorkspace` UI path for the CAD editor, while keeping exact sketch, profile and solid authority on the CPU and rendering the workspace geometry on the GPU.

## Core rule

- CPU owns exact sketch/profile/solid declarations, snapping, picking, selection, constraints, dimensions and history.
- GPU owns stroke, fill, marker, dimension and overlay presentation.

## Dedicated host

Add a new executable host:

- `Engine/Application/ParametricSketchHost/Source/ParametricSketchHost.cpp`

The host will:

- construct one `ViewportSequence`;
- open one parametric workspace window;
- own exact CAD state and tool state;
- own the CAD render packet derivation and GPU pass objects;
- route pointer and key input into the workspace.

## Dedicated workspace UI

Add a new SlateUI panel path:

- `Engine/SlateUI/Interface/ParametricWorkspace/{Api,Source}`

The workspace panel will present:

- tool rail;
- tool options strip;
- sketch canvas;
- outliner;
- properties/history pages;
- status strip.

## 2D render pass strategy

Do not overload `WorkspaceOverlayPass`.

Instead add a dedicated CAD pass object:

- `SlateVulkan/Device/WorkspaceCadPass/...`

The pass will record inside the same dynamic rendering scope as the standing geometry and interface recordings, but it will own its own pipelines and buffers.

## Shared 2D rendering seam

Add a CPU packet derivation seam that reuses the standing sketch declarations and the approved third-party helpers:

- use `SketchPolyline` for curve approximation;
- use `clipper2` for two-dimensional profile boolean/offset style subproblems where needed;
- use `earcut` for filled profile triangulation.

The pass packet should include:

- filled profile triangles;
- curve strokes;
- control polygons;
- point/control markers;
- dimension strokes;
- constraint glyph markers;
- snap marker;
- two-dimensional grid / gizmo overlays.

## Outliner plan

The parametric workspace reuses scene-directory mechanics, not scene semantics.
A dedicated CAD UI contract should sit in `SlateUI`, while the exact-record / revision backends remain in `SlateFeature`; the host later bridges the two.

### Backend subjects

Committed semantic rows only, never transient clicks:

- `Folder`
- `Point`
- `OpenCurve`
- `ClosedProfile`
- `ThinSurface`
- `Solid`
- `Dimension`
- `Constraint`
- `Pattern`
- `Mirror`

### Dynamic promotion

The outliner reflects committed semantic promotion:

- open creation state does not appear as a permanent row;
- a committed line appears as `Line_#`;
- a committed closed profile appears as `Profile_#`;
- a resulting solid appears as `Solid_#`.

### Naming

Monotonic per-subject names:

- `Point_1`
- `Line_1`
- `Arc_1`
- `Bezier_1`
- `Profile_1`
- `Solid_1`
- `Constraint_1`
- `Dimension_1`
- `Folder_1`

Never renumber after deletion.

### System folders

Dynamic category roots:

- `Sketch`
  - `Points`
  - `Open Curves`
  - `Closed Profiles`
  - `Construction`
- `Geometry`
  - `Thin Surfaces`
  - `Solids`
- `Annotation`
  - `Dimensions`
  - `Constraints`
- `Operations`
  - `Patterns`
  - `Mirrors`

User folders sit under these roots.

Stage 2 projects the four top-level category roots first; finer subject buckets can be layered in once the dedicated panel exists.

## Properties and revision

The right-side properties leaf is a two-page structure:

- `Properties`
- `Revision`

Properties show the selected semantic object and current sub-selection details.
Revision is filtered from one global committed workspace revision stream by selected object identity.

## Backend structures to add first

1. Workspace record structure.
2. Monotonic name allocator.
3. Revision/history sequence with per-record filtering.
4. Property/history projection for the selected row.
5. Outliner projection compatible with `SceneDirectoryPanel`-style presentation.

## Implementation order

### Stage 1

Backend only:

- workspace record structure;
- name allocator;
- revision/history sequence;
- property/history projection.

### Stage 2

Backend/UI seam for the CAD outliner before the full workspace panel:

- SceneDirectory-style CAD projection adapter;
- category-root / folder row projection;
- record-to-row selection lookup;
- subject/category search tags.

### Stage 3

Host and panel seams:

- parametric workspace UI guarantee for directory, properties and revision content;
- host-side bridge from exact workspace projections into UI rows and property/revision presentations;
- dedicated CAD outliner and Properties | Revision leaf panel shell in SlateUI;
- one-leaf Directory → Properties | Revision slide path, matching scene-directory travel;
- dedicated `ParametricTools` leaf subject and a catalogue → settings slide shell;
- fidelity pass toward `ConstructionCatalogueMenu.html`: richer band/tool data, probe sections and settings readouts;
- viewport view-state pass: top/front/right-style orientation chooser, perspective/orthographic switching and mouse-driven pan/orbit/zoom;
- camera-aware CAD pass projection derived from the sketch plane and viewport state;
- first drawable viewport pass: active line / rectangle / circle tools, mouse-to-plane projection, Ctrl-gated snapping, preview and commit into exact sketch records;
- viewport semantic selection and planar point/control editing through exact sketch selection/edit seams;
- GPU overlay path for grid, viewport selection highlight, transform gizmo handles and command feedback;
- Blender-style transform command chains for planar sketch edit (`G`, `G X/Z`, `G G`, `R`, `S X/Z`, numeric values);
- creation settings wired from `ParametricToolsContext` into drawing behavior: construction drafting, line length/angle assists, rectangle width/height assists and circle radius/diameter assists;
- dedicated `ParametricSketchHost` bring-up owning exact records, revisions and the CAD panel bridge;
- Control Centre and Asset Browser drawers brought up on the parametric host like the other application hosts;
- shared CAD drawing packet and exact sketch rendering projection seam;
- dedicated `WorkspaceCadPass` shell consuming the CAD packet inside the dynamic rendering scope;
- workspace panel contract.

### Stage 4

GPU pass and viewport integration:

- CAD geometry pass;
- clipped recording inside the same display rendering scope;
- deferred popups above the CAD pass.

### Stage 5

Full tool routing and UI integration.
