# Camera component, bookmarks, and collapsible-card proof

These 1280 × 900 PNGs are actual-panel rasters recorded through the real editor panels and `RecordingSurface`; they are not mock-ups.

- [`editor-camera-bookmarks.png`](editor-camera-bookmarks.png) — the selected **Editor Camera** on its `Properties | Bookmarks` carousel destination. Bookmark names are reusable editable text fields; **Save Current** captures the live pose, **Go To** requests restoration through the owning `EditorCameraComponent`, and **Delete** removes the selected bookmark. The viewport footer contains Grid but no former Cameras option card.
- [`editor-layerstack-card.png`](editor-layerstack-card.png) — each clean title-case section has its own disclosure chevron. **Effects** was collapsed through the real header interaction while Info, Height to Normal, Colour Blending, and Channel Blending remain open and editable.
- [`editor-camera-fly.png`](editor-camera-fly.png) — camera movement proof after the component rename and rotational-drift fix.

Architecture:

- `Application/CameraComponent/CameraComponent` is the common pose, movement, and positional-lag base.
- `Application/EditorHost/EditorCameraComponent` is the editor identity.
- Future player and spectator camera components can use different controllers over the same base without inheriting editor bookmark UI.
- Rotation is immediate and lag is positional only. The first right-button capture frame also suppresses synthetic cursor travel.

Proof assertions:

```text
[assert] stationary right-click preserves camera rotation
[assert] V-card and double-contact carousel remain independent
```

SHA-256:

```text
02f988f3096e166b0fe7ba1ee66d1fe2de84d026cb734445b21c999df5f9f9e3  editor-camera-bookmarks.png
97fcfbbc3f1304587c71e96d6f798606098860d0ec78f7748f0105049e84ab1f  editor-camera-fly.png
5a8ecd01384b6a7231e83ecb96c4cb418ceb1813d1d73abe73c1e71576ad145b  editor-layerstack-card.png
```
