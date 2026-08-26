# CAPABILITY-Ownership — Where Each Capability Already Lives

🔴 **Read this before writing any capability into a host.** Every entry below already exists, is already
linked into every product, and is already reachable through its `Api/` header. Writing a second copy is
the defect this document exists to prevent — it has happened at least three times.

## The rule

> A host owns `main()`, the tick order, and nothing else. Every capability lives in a library unit and is
> reached through its `Api/` header. If a host needs a behaviour that is not a unit, **the unit is what
> gets written** — never a local copy inside the host.

This is `32` §5's gate, restated so it can be checked. `Tools/VerifyHostPartition.py` enforces it.

## The register

| Capability                     | Owner                                                                    |
|--------------------------------|--------------------------------------------------------------------------|
| Editor camera, orbit, fly      | `SlateScene/Scene/EditorCameraComponent`                                 |
| Player / spectator camera      | `SlateScene/Scene/{PlayerCameraComponent,SpectatorCameraComponent}`       |
| Camera projection matrices     | `SlateDocument/Document/CameraProjection`                                |
| Sky, atmosphere, sun           | `SlateScene/Scene/AtmosphereComponent` + `SlateCompute/Compute/AtmosphereIntegrator` |
| Sky presentation on the device | `SlateVulkan/Device/AtmospherePresentationSurface`                       |
| Directional light              | `SlateScene/Scene/DirectionalLightComponent`                             |
| Ground grid, lattice, overlay  | `SlateVulkan/Device/WorkspaceOverlayPass`                                |
| Scene pass, CAD pass           | `SlateVulkan/Device/{WorkspaceScenePass,WorkspaceCadPass}`               |
| Window, device, chain, tick    | `SlateVulkan/Device/HostLifecycle`                                       |
| Interface tick and recording   | `SlateUI/Interface/{InterfaceExchange,ViewportSequence}`                 |
| Immediate-mode drawing         | `SlateUI/Interface/RecordingSurface` (inside `InterfaceExchange`)         |
| Panels, drawers, docking       | `SlateUI/Interface/{EditorPanel,WorkspacePanel,PanelStructure,DrawerSpace}` |
| Theme, appearance, typography  | `SlateUI/Interface/{ThemeInterchange,ThemeSpecification,AppearanceSpecification}` |
| Content browser                | `SlateUI/Interface/ContentBrowserPanel`                                  |
| Scene outliner / directory     | `SlateUI/Interface/SceneDirectoryPanel`                                  |
| Texture paint layer stack      | `SlateUI/Interface/TexturePaintPanel`                                    |
| Sketch tool interface          | `SlateUI/Interface/ParametricTools`                                      |
| Sketch geometry and solving    | `SlateFeature/Sketch/*` — 20 modules, incl. `SketchStructure`, `ConstraintSolver`, `ProfileSolver` |
| Feature history, recompute     | `SlateFeature/Feature/{FeatureStructure,RecomputeScheduler}`             |
| Pick classification, provenance| `SlateFeature/Reference/{PickClassifier,ProvenanceIndex}`                |
| Exact curves, surfaces, solids | `SlateGeometry/{Geometry,Topology}`                                      |
| Boolean, fillet, extrude, loft | `SlateGeometry/Operation/*`                                              |
| Tessellation                   | `SlateGeometry/Discrete/TessellationSpecification`                       |
| Brush intent                   | `SlateDocument/Document/BrushSpecification`                              |
| Layer sequence                 | `SlateDocument/Document/SurfaceLayerSequence`                            |
| Stroke execution               | `SlateCompute/Compute/{ImpressionSequence,StrokeSpace}`                  |
| Texture depots and tiles       | `SlateCompute/Compute/{SurfaceDepot,UvSurfaceDepot,SurfaceTileSpace,TileSpace}` |
| Scene import (glTF, OBJ)       | `SlateDocument/Format/SceneMeshImport`                                   |
| Codex interchange              | `SlateDocument/Format/{CodexInterchange,WorkspaceSceneActivation}`       |
| Revisions and undo             | `SlateDocument/Document/RevisionSequence`                                |
| Selection                      | `SlateDocument/Document/SelectionSequence`                               |
| Exact predicates (both toolchains) | `Engine/Shared/*.slang.h`                                            |
| Refusal and delivery contracts | `Engine/Foundation/DeliveryOutcome.h`                                    |

⚠️ Unit names change during the refactor described in `References/UnitAndProductPlan.md`
(`SlateScene` → `SlateWorld`, `SlateGeometry` → `SlateShape`, `SlateFeature` folded in). The **owner**
does not change — only the path. Update this register in the same commit as any rename.

## What went wrong, so it is not repeated

On 26 August an agent asked to run paint and sketch inside the editor wrote **a second camera, a second
sky and a second CAD editor**, all of which appear in the register above. Two mechanisms allowed it, and
both are being closed:

1. **The feature seam was dead.** `SharedViewportHostBridge.h` declared `SLATE_*_HOST` masks, the build
   defined none of them, and no caller ever read the result. An agent looking for the live mechanism
   correctly concluded there was none and wrote its own. Closed by the `[variant]` define and the
   `#error` fallback.
2. **Capabilities were unreachable.** `ParametricSketchHost.cpp` held 5 981 lines with 138 local function
   definitions — a library written inside an executable, reachable from no other host. An agent asked to
   reuse it had no way to. Closed by lifting those behaviours into units.

📝 `Engine/Application/EditorHost/Source/SkyImage.cpp` sat referenced by **nothing** for long enough that
it was read, assumed to be unimplemented, and rewritten. Dead source is not free; it actively misleads.
