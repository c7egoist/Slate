# Directory selection and transfer proof

These rasters come from `Tools/SceneDirectoryProof`, which records the real `SceneDirectoryPanel` and editor `TexturePaintPanel` through the real `RecordingSurface` before CPU-rasterizing ImGui's draw list.

## Before / after

- `before/editor-scene-transfer.png` — previous transfer page with fixed Scale, Forward, Up axis, and Normals pills.
- `after/editor-scene-transfer.png` — Scale is the reusable expression-capable editable field; Forward, Up Axis, and Normals are shared component dropdowns.
- `before/editor-overview.png` — previous Directory presentation.
- `after/editor-overview.png` — simplified Document Directory title, whole selected-component header as Inspect action, and full-size shared switches.

## Selection proofs

- `after/editor-directory-multiselect.png` — real Scene Directory with Sun, Sky Atmosphere, and Editor Camera selected together.
- `after/editor-layer-multiselect.png` — real production Texture Paint Layer Stack with multiple layer rows selected.

Folder disclosure is driven through the same eased occupancy-and-clip path used by the standing panel rows; the settled captures avoid presenting an arbitrary mid-animation frame.
