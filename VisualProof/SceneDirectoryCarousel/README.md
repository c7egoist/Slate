# Scene Directory and carousel proof

These are CPU rasters of the real editor workspace and the real `SceneDirectoryPanel`; they are not mock-ups. The proof harness records through `RecordingSurface` and rasterizes the resulting ImGui draw list using the same fonts and panel code as `EditorHost`.

## Before / after

| State | Actual-panel raster | What it establishes |
|---|---|---|
| Before | [`before/editor-overview.png`](before/editor-overview.png) | Baseline raster retained from the existing `VisualProof/EditorScene` suite before this increment. |
| After | [`after/editor-overview.png`](after/editor-overview.png) | Updated Directory page, preserving the Sun, Sky Atmosphere, Environment, Lighting, and folder hierarchy. |
| After — inspector destination | [`after/editor-outliner-inspector.png`](after/editor-outliner-inspector.png) | The carousel's settled Inspector destination, including the reusable Position, Rotation, and Scale XYZ controls and the visible return-to-Directory action. |
| After — dedicated properties leaf | [`after/editor-sun-props.png`](after/editor-sun-props.png) | The same transform and environment controls in the three-leaf editor arrangement. |

All rasters are 1280 × 900 PNGs. Relevant SHA-256 values:

```text
240296a3b756cc6a7380c2c4e995866d888901ea65426453da0dd6db24350095  before/editor-overview.png
dc40c3bc5dae0102758651688d62f7c07d259b04d591f8c11083647ec3270265  after/editor-overview.png
5ef18a0a851b891e0e85214413f8a4a5da6a211718c80d42e36f5dbdb041fa84  after/editor-outliner-inspector.png
```

## Behavior fixed

- The outliner now has two outer coordinates: Directory at `0 × width` and Inspector at `-1 × width`.
- Properties and History are inner inspector pages with their own eased carousel motion rather than duplicate outer destinations.
- Both departure and arrival pages are recorded side by side and clipped to the real leaf during travel.
- Tab cycles Directory → Properties → History → Directory; the Inspector header also exposes a return action.
- Position, Rotation, and Scale retain axis-specific horizontal scrubbing and now enter expression editing on a double contact.
- All direct editing begins with the insertion caret at byte zero, before the leftmost digit.

## Validation

```text
SlateUI:     32 translation units accepted
Application:  6 translation units accepted
```
