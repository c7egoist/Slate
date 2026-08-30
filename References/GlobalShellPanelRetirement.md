# Validation shell retirement

`GlobalShellPanel` was removed after its required runtime behaviour had standing owners:

- `SceneDirectoryPanel` owns the editor directory and details/bookmark pages.
- `TexturePaintPanel` owns the texture stack and its property/export pages.
- `EditorPanel` and `PanelStructure` own workspace leaf composition.
- `ControlCentrePanel` and `ContentBrowserPanel` remain runtime drawer panels in `EditorHost` and `PaintHost`.
- The validation-only `LayerStackPanel` and its private data model were removed after reference scans confirmed no runtime owner.

`InterfaceValidationHost` no longer constructs or records the retired shell, Control Centre, Content Browser, or Layer Stack. It is now restricted to reusable component validation plus the existing editor/facet component fixtures. No runtime implementation was removed for Control Centre, Content Browser, or Texture Paint; they remain owned by their application hosts.
