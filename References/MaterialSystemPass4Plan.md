# Material System Pass 4 Plan

Status: implemented for CPU-side imported-image material sampling MVP.

## Goal

Turn imported image references from metadata-only records into sampleable material inputs without introducing painting or a UV editor yet. The pass uses material-local image source ordinals and explicit UV coordinates, keeps missing references as refusals, and supports only simple decoded formats until a full image pipeline lands.

## Scope delivered

1. Imported image sampling seam
   - Added `MaterialImageSampling` in `SlateCompute`.
   - `OpenReference()` decodes uncompressed BMP and uncompressed TGA into RGBA float rasters.
   - PNG/JPEG/WebP/EXR stay registered as references but refuse sampling until decoder support lands.
   - `FingerprintMaterialImageReference()` keys sampled references by name, path, extent, component layout and colour/data intent.

2. UV-addressed sample requests
   - Added `MaterialImageSampleRequest` with explicit U/V coordinates.
   - Added clamp/repeat/mirror address modes.
   - `SampleReference()` and `SampleMaterialChannel()` produce either colour or scalar results from material-local imported image sources.

3. Material imported-channel resolution
   - `ChannelSpecification::SourceIndex` now participates in material dirty keys and codex persistence.
   - `MaterialProcessingCapabilities::ImportedImageResolution` is now true for this CPU MVP.
   - Missing files or unsupported encodings refuse rather than substituting default pixels.

## Still out of scope

- Full UV editor UI.
- PNG/JPEG/WebP/EXR pixel decoding.
- GPU texture upload and sampling.
- Painting or tile dirtiness.
- Per-face imported material IDs.
