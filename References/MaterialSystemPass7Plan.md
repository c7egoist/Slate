# Material System Pass 7 Plan

Status: implemented for the requested completion slice, then stopped.

## Goal

Complete the requested remaining material-system phases: actual flattened texture/image writing from export packages, GPU texture upload/sampling path, export UI wiring, and full PNG/JPEG/WebP/EXR decode path.

## Delivered

1. Flattened texture writing
   - Added `MaterialTextureExport` in `SlateCompute`.
   - `FlattenImage()` resolves export package lane declarations into RGBA float textures.
   - `WriteImage()` writes native TGA directly and writes PNG/EXR through external conversion from flattened pixels.
   - `WritePackage()` writes every export image plus the manifest JSON file.

2. GPU texture upload/sampling path
   - Added `MaterialTextureUpload` in `SlateVulkan`.
   - Owns Vulkan image, image view and sampler lifetime.
   - Accepts RGBA float uploads and exposes a shader-readable image/sampler binding.
   - Keeps document/compute dependencies out of `SlateVulkan`.

3. Export UI wiring
   - Editor Texture Paint export footer now builds a `WorkspaceMaterialRecord`, constructs export options from UI fields, builds an export package, and writes flattened textures/manifests.

4. Full imported image decode path
   - Material image import metadata now falls back to external image identification for formats not parsed natively.
   - Material image sampling now falls back to external decode/conversion for PNG, JPEG, WebP and EXR, while preserving native BMP/TGA decode.

## Stop point

Stopped after the requested full PNG/JPEG/WebP/EXR decode path and export/upload/UI completion slice.
