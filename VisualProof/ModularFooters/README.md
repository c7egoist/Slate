# Modular editor footer proof

These rasters are produced by `Tools/SceneDirectoryProof/SceneDirectoryProof.cpp`. The harness records the real `WorkspacePanel`, `EditorPanel`, `SceneDirectoryPanel`, and `TexturePaintPanel` through the real recording surface; it does not reproduce the panels in a mock.

- `editor-scene-transfer.png` — presses the Scene Directory's real **Import** footer pill, settles the outer carousel at page 2, and shows the UI-only FBX/OBJ transfer destination. The scenario asserts that Layer Stack navigation remains unchanged.
- `editor-layer-flatten.png` — presses the Layer Stack's real **Export Flattened** footer pill, settles its outer carousel at page 2, and shows the non-destructive PNG/TGA output destination. The scenario asserts that the nested property tab remains unchanged.

The additional interactive `editor-grid-dropdown` scenario is rendered only to scratch output. It presses the Viewport's real Grid pill, opens the nested Grid Type roster, selects **Dotted**, and asserts that the roster remains standing while its parent popup is active.

SHA-256:

- `editor-scene-transfer.png`: `9f626f5e3973c41a84643ba41f6684bbc4a1df4a6deb3ea70712e7f2b97eb55d`
- `editor-layer-flatten.png`: `9ce21c3b464e6e6c784918e6a1db3ad5990fea3c8ced964ee6ed53ab39c1d641`
