# SlateModel module plan

## Recommendation

Name the shared exact-shape module `SlateModel`.

Rationale:

- It is broader than geometry: it owns curves, profiles, surfaces, topological adjacency, tolerances, evaluators, and shape construction helpers.
- It is narrower than scene/editor features: it should not own UI, workspaces, panels, document activation, or viewport presentation.
- It reads naturally beside `SlateCompute`: `SlateModel` is the CPU/exact model kernel; `SlateCompute` is the GPU/compute execution layer.
- It avoids using topology as the module name, because topology is only one part of the model kernel.

## Dependency position

`SlateModel` should sit between `SlateMath` and feature/document layers:

```text
Foundation
SlateMath
SlateModel
SlateDocument / SlateScene / SlateFeature
SlateUI / SlateVulkan
Application
```

Allowed dependencies for `SlateModel`:

- `Foundation`
- `SlateMath`
- bundled third-party numeric/model helpers when already approved for this area, such as Clipper2 for 2D/profile operations and earcut for planar triangulation

Disallowed dependencies:

- `SlateUI`
- `SlateVulkan`
- `Application`
- host or panel state
- scene-directory presentation state
- texture-paint layer-stack state

## Initial contents

Move exact/model code here first, not UI adapters:

- curve primitives and evaluators
- line/arc/circle/Bezier/NURBS-style curve specifications when present
- surface specifications and evaluators
- profile loops and planar region helpers
- topology adjacency records for vertices, edges, loops, faces, shells, and bodies
- tolerance and snapping helpers that are purely geometric/model based
- triangulation helpers used to present exact planar regions
- lightweight IDs/references used by CAD records to point at model elements

Keep these outside `SlateModel`:

- `WorkspaceRecordStructure` history and naming policy stays in `SlateFeature`
- UI row projections stay in `SlateFeature` / `SlateUI`
- Codex load/save stays in `SlateDocument`
- Vulkan drawing passes stay in `SlateVulkan`
- host orchestration stays in `Application`

## Migration sequence

1. Create `Engine/SlateModel/Module.toml` with dependencies on `Foundation` and `SlateMath`.
2. Move `Engine/SlateGeometry/Geometry/SurfaceSpecification` into `SlateModel/Model/SurfaceSpecification`.
3. Add forwarding headers or update includes in one pass, depending on how strict the build graph is.
4. Move curve/profile utilities next.
5. Move topology adjacency records after curves/profiles are settled.
6. Point `SlateFeature` CAD records at `SlateModel` data types.
7. Leave old module paths empty or as short compatibility includes until all call sites are moved.

## Public vocabulary

Recommended nouns:

- model
- shape
- body
- shell
- face
- loop
- edge
- vertex
- curve
- surface
- profile
- region
- evaluator
- classification
- role
- subject

Avoid making the module name `SlateTopology`; topology is an internal slice, not the complete responsibility.
