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
| Editor camera, orbit, fly      | `SlateWorld/World/EditorCameraComponent`                                 |
| Player / spectator camera      | `SlateWorld/World/{PlayerCameraComponent,SpectatorCameraComponent}`       |
| Camera projection matrices     | `SlateDocument/Document/CameraProjection`                                |
| Sky, atmosphere, sun           | `SlateWorld/World/AtmosphereComponent` + `SlateCompute/Compute/AtmosphereIntegrator` |
| Sky presentation on the device | `SlateVulkan/Device/AtmospherePresentationSurface`                       |
| Directional light              | `SlateWorld/World/DirectionalLightComponent`                             |
| Ground grid, lattice, overlay  | `SlateVulkan/Device/WorkspaceOverlayPass`                                |
| Scene pass, CAD pass           | `SlateVulkan/Device/{WorkspaceScenePass,WorkspaceCadPass}`               |
| Window, device, chain, tick    | `SlateVulkan/Device/HostLifecycle`                                       |
| Bring-up, tick order, teardown | `SlateRuntime/Session/SessionSequence` — the order the two below are driven in |
| Product feature mask           | `Application/Api/HostFeature.h` — the ONE reader of the product macro    |
| Interface tick and recording   | `SlateUI/Interface/{InterfaceExchange,ViewportSequence}`                 |
| Immediate-mode drawing         | `SlateUI/Interface/RecordingSurface` (inside `InterfaceExchange`)         |
| Panels, drawers, docking       | `SlateUI/Interface/{EditorPanel,WorkspacePanel,PanelStructure,DrawerSpace}` |
| Theme, appearance, typography  | `SlateUI/Interface/{ThemeInterchange,ThemeSpecification,AppearanceSpecification}` |
| Content browser                | `SlateUI/Interface/ContentBrowserPanel`                                  |
| Scene outliner / directory     | `SlateUI/Interface/SceneDirectoryPanel`                                  |
| Texture paint layer stack      | `SlateUI/Interface/TexturePaintPanel`                                    |
| Sketch tool interface          | `SlateUI/Interface/ParametricTools`                                      |
| Sketch geometry and solving    | `SlateShape/Sketch/*` — 20 modules, incl. `SketchStructure`, `ConstraintSolver`, `ProfileSolver` |
| Feature history, recompute     | `SlateShape/Sequence/{FeatureStructure,RecomputeScheduler}`             |
| Pick classification, provenance| `SlateShape/Reference/{PickClassifier,ProvenanceIndex}`                |
| Exact curves, surfaces, solids | `SlateShape/{Geometry,Topology}`                                      |
| Boolean, fillet, extrude, loft | `SlateShape/Operation/*`                                              |
| Tessellation                   | `SlateShape/Discrete/TessellationSpecification`                       |
| Brush intent                   | `SlateDocument/Document/BrushSpecification`                              |
| Layer sequence                 | `SlateDocument/Document/SurfaceLayerSequence`                            |
| Stroke execution               | `SlateCompute/Compute/{ImpressionSequence,StrokeSpace}`                  |
| Texture depots and tiles       | `SlateCompute/Compute/{SurfaceDepot,UvSurfaceDepot,SurfaceTileSpace,TileSpace}` |
| Scene import (glTF, OBJ)       | `SlateDocument/Format/SceneMeshImport`                                   |
| Codex interchange              | `SlateDocument/Format/{CodexInterchange,WorkspaceSceneActivation}`       |
| Revisions and undo             | `SlateDocument/Document/RevisionSequence`                                |
| Selection                      | `SlateDocument/Document/SelectionSequence`                               |
| Exact predicates (both toolchains) | `Engine/Shared/*.slang.h`                                            |
| Refusal and delivery contracts | `Engine/Foundation/DeliveryGuarantee.h`                                    |

⚠️ Unit names change during the refactor described in `References/UnitAndProductPlan.md`. Applied so
far: `SlateScene` → `SlateWorld` (step 4), `SlateGeometry` → `SlateShape` (step 5), `SlateFeature` folded
into `SlateShape` (step 6), and **`SlateRuntime` created (step 7)**. Still to come: `SlateToolset` and
`SlateWorkspace` (steps 8–9). The **owner** does not change — only the path. Update this register in the
same commit as any rename.

🔴 **A host now owns `main()`, its panels, its own device estate, and nothing else.** The bring-up order,
the tick prologue, every recovery branch and the teardown belong to `SessionSequence`. A host that writes
its own `HostLifecycle` / `ViewportSequence` bring-up is reintroducing the duplication step 7 removed —
`PaintHost` reaches **zero** `Lifetime.*` calls, which is the shape a lifted host has.

## What went wrong, so it is not repeated

On 26 August an agent asked to run paint and sketch inside the editor wrote **a second camera, a second
sky and a second CAD editor**, all of which appear in the register above. Two mechanisms allowed it, and
both are being closed:

1. **The feature seam was dead.** `SharedViewportHostBridge.h` declared `SLATE_*_HOST` masks, the build
   defined none of them, and no caller ever read the result. An agent looking for the live mechanism
   correctly concluded there was none and wrote its own. Closed by the `[product]` define and the
   `#error` fallback — and then **only half closed**: step 7 found that `HostFeature.h`, the replacement,
   was itself included by no host at all. A seam is not live because it compiles; it is live because
   something reads it. `EditorHost` now includes it and states `HostProduct` at bring-up.
2. **Capabilities were unreachable.** `ParametricSketchHost.cpp` held 5 981 lines with 138 local function
   definitions — a library written inside an executable, reachable from no other host. An agent asked to
   reuse it had no way to. Closed by lifting those behaviours into units.

📝 `Engine/Application/EditorHost/Source/SkyImage.cpp` sat referenced by **nothing** for long enough that
it was read, assumed to be unimplemented, and rewritten. Dead source is not free; it actively misleads.
