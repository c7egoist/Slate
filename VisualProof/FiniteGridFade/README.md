# Finite grid fade proof

These rasters come from `Tools/SceneDirectoryProof` and record the real viewport/editor panels.

- `before/editor-grid-settings.png` — previous unbounded ground presentation, with no authored extent or camera-fade controls.
- `after/editor-grid-fade.png` — the real Grid footer popup now exposes **World extent** and **Camera fade** in metres using shared magnitude controls.

The GPU implementation remains analytic in `WorkspaceOverlayFragment.slang`; it does not generate a large CPU line set. World-centre and camera-relative fades multiply, so a fragment must be inside both limits. The headless proof environment has no Vulkan shader execution, so the after raster proves the real authored controls while the shader implementation and 128-byte CPU/GPU push layout are validated structurally and by the full translation-unit build.
