# Material System Completion Progress

Status: completed through requested stop point: flattened texture writing, GPU texture upload/sampling path, export UI wiring, and full imported-image decode path via external decoder fallback.

## Completed sequence

1. Mesh import / scene path MVP
   - Mesh formats route into workspace scene records and embedded scene meshes.
   - Scene Directory and shared CAD/polygon viewport paths stand.

2. Material System Pass 1: document authority
   - Document-backed material records and layer commands.
   - Mandatory Base Material layer.
   - Masks/coverage, dirty snapshots, and viewport mode routing.

3. Material System Pass 2: scene render routing
   - Scene material buffers and scene shaders.
   - Lit, Matcap, Source Wire, Triangulated Wire, Points, Normal, Metallic and Illumination modes.

4. Material System Pass 3: image references
   - Imported material image metadata.
   - Channel source indices and material-local image tables.
   - Image binding to material channels.

5. Material System Pass 4: image sampling
   - CPU-side U/V image sampling.
   - Native BMP/TGA decoding.
   - PNG/JPEG/WebP/EXR decode path through external decoder fallback.

6. Material System Pass 5: painting dirtiness bridge
   - Painted material layer creation.
   - Stroke declaration from brush channels.
   - One-stroke/one-transaction seal path with dirty tile and channel reports.

7. Material System Pass 6: export package declarations
   - Slate/Blender/Unreal/Unity/glTF export presets.
   - ORM, Unity mask, albedo/opacity, normal and emission packing.
   - Manifest generation.

8. Material System Pass 7: requested completion slice
   - Actual flattened texture generation and image writing from export packages.
   - Native TGA writing and PNG/EXR output through external conversion from flattened pixels.
   - GPU texture image/view/sampler upload path with staging copy and shader-read layout transition.
   - Editor Texture Paint export footer now builds and writes material export packages.
   - Import metadata and sampling both support full PNG/JPEG/WebP/EXR decode through external decoder fallback.

## Important boundaries preserved

- No Texture Paint workspace logic was added to `ParametricSketchHost`.
- `SlateVulkan` texture upload code remains independent of `SlateDocument` and `SlateCompute` material declarations.
- Missing references refuse instead of fabricating replacement pixels.
- Per-face material IDs, full UV editor UI and production GPU transfer scheduling remain future work.
