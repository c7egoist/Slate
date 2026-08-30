# Material System Pass 1 Plan

Status: in implementation. The document bridge, mandatory material records, uniform mask plumbing, dirty snapshots, viewport mode routing, and validation proof are now present; GPU viewport material resolve and image-backed masks remain follow-up work.

## Goal

Build the first real material pipeline slice before painting or UV editing: material slots own document materials, every material has a mandatory Base Material layer, the Layer Stack issues document-backed commands, masks affect layer contribution, dirty snapshots prevent broad recompute, and viewport mode routing exposes Lit/Matcap/source-wire/triangulated-wire/points modes.

## Decisions

- Keep the 20-channel material schema as the authoring and export source of truth.
- Add specialised shader closures through `ReflectanceSelection`, not as a second material authority.
- The first closures are Standard/Lit, EmissiveOnly, Unlit, Matcap as a viewport mode, and placeholders for cloth/clear-coat/transmission where the channel schema already stands.
- Layer masks are layer coverage: they affect the layer/folder they are attached to. Clipping masks and cross-layer masks are future modes.
- No painting and no UV editor in this pass. Mask/image sources are represented and evaluated as uniform/procedural placeholders until image loading and paint content arrive.
- Layer Stack UI remains in the paint-capable Editor host. `ParametricSketchHost` keeps simple scene material display and must not receive Texture Paint layer-stack logic.

## Phase 1 scope

1. Material/layer document bridge
   - Seed a `MaterialSpecification` and `SurfaceLayerSequence` from the editor Layer Stack.
   - Ensure a mandatory Base Material layer always stands.
   - Mirror add/remove/reorder/folder/mask UI changes into document layer declarations.
   - Preserve robust selection and row clamping in the existing `TexturePaintStack` path.

2. Mask plumbing
   - Add mask coverage density, invert, and channel-mask targets to document coverage.
   - Add `SurfaceLayerSequence` amendment calls for source, channel mask, and coverage.
   - Hash mask fields into material dirty keys.
   - Resolve uniform mask coverage for validation/preview before image loading exists.

3. Dirty snapshots
   - Capture immutable material/layer snapshots after stack changes.
   - Compare snapshots and expose affected channel/layer dirtiness.
   - Avoid full recompute unless structure/coverage changes require it.

4. View modes
   - Extend viewport shading names to Lit, Matcap, Source Wire, Triangulated Wire, Points, Normal, Metallic, and Illumination.
   - Keep source-wire versus triangulated-wire distinct for future imported topology display.

5. Validation proof
   - Add a validation script covering base-layer protection, layer command mirroring, mask dirty keys, and view-mode routing.

## Explicit non-goals

- No painting.
- No UV editor.
- No image loading for material textures or masks.
- No full shader graph.
- No full procedural generator library.
- No export packing until viewport material resolve is stable.
- No Texture Paint layer-stack logic inside `ParametricSketchHost`.

## Follow-up phases

- Phase 2: GPU constant/fill material evaluation into viewport scene rendering.
- Phase 3: image source loading and material slot assignment UI.
- Phase 4: UV editor and image-backed mask/material sampling.
- Phase 5: painting and tile dirtiness.
- Phase 6: export packing and DCC presets.
