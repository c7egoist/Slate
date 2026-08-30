# Layer Stack inline-card proof

These 1280 × 900 PNGs are rasters of the real editor workspace and the real `TexturePaintPanel` / `SceneDirectoryPanel`. The proof harness records through `RecordingSurface` and CPU-rasterizes the resulting ImGui draw list; these are not mock-ups.

- [`editor-layerstack-card.png`](editor-layerstack-card.png) — the **Levels** entry's trailing V opened its card beneath the row. The clean title-case sections are limited to Info, Height to Normal, Effects, Colour Blending, and Channel Blending. Their resolution, tag colour, toggle, blend, effect, and opacity controls are the real reusable editable fields.
- [`editor-overview.png`](editor-overview.png) — the Directory destination with its footer spanning the complete Directory page, including beneath the details side.

The layer-card scenario drives the actual interactions and asserts both paths remain independent:

1. Press the trailing disclosure V and assert the inline card opens while `StackPage == 0`.
2. Close it, double-contact the row body, and assert the carousel travels to the properties page.
3. Press Tab to return, reopen the V-card, and capture the raster.

```text
[assert] V-card and double-contact carousel remain independent
```

SHA-256:

```text
1275f8454509a12f4ee710f574c734b287adba8ab5848c75a1fb76d705bb4957  editor-layerstack-card.png
7eb404fceb383f7d329c36fbe27d14751fc61193f67b213dc2db138dccb5f9ce  editor-overview.png
```
