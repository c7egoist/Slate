# Material System Pass 3 Plan

Status: implemented for metadata/reference intake and material binding.

## Goal

Add the first image-backed material source path without painting or a UV editor: imported image files are recognised, their dimensions/data-vs-colour intent are recorded, material channels can point at image source ordinals, and scene material slots can be reassigned without duplicating geometry.

## Scope delivered

1. Imported image metadata intake
   - Added `MaterialImageImport` for PNG, JPEG, BMP, TGA, WebP and EXR classification.
   - PNG/JPEG/BMP/TGA parse dimensions and component metadata directly from file headers.
   - WebP/EXR are accepted as referenced image sources with placeholder dimensions until a full decoder lands.
   - Filename heuristics suggest the material channel: albedo, normal, roughness, metallic, opacity, ambient occlusion or emission.

2. Document material image references
   - `WorkspaceMaterialImageReference` records reference name, origin path, extent, component count, bit depth and colour/data intent.
   - `WorkspaceMaterialRecord` now owns imported image references.
   - `ChannelSpecification` now carries a `SourceIndex`, allowing `ChannelSource::Imported` to address the material-local image table.
   - Workspace Codex material sections now serialize channel source indices and image references.

3. Binding and assignment commands
   - Added `BindWorkspaceMaterialImage()` to bind an imported image reference to a material channel.
   - Added `AssignWorkspaceMaterial()` to assign an existing or new material reference to a geometry scene entry.
   - Binding an image sets the channel to `ChannelSource::Imported` and preserves defaults for unresolved/missing imagery.

4. Host import route
   - `ParametricSketchHost` import browsing now recognises material image formats.
   - Importing an image binds it to the selected scene object's material where possible, or to the first/default material otherwise.
   - This is material reference plumbing only: no Texture Paint workspace logic is added to the parametric host.

## Still out of scope

- Pixel decoding and sampling.
- UV editor.
- Painting and tile dirtiness.
- Texture preview thumbnails.
- Per-face material IDs from imported meshes.
