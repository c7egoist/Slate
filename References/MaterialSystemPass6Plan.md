# Material System Pass 6 Plan

Status: implemented for export packing declarations and manifest generation.

## Goal

Add material texture-set export planning after document materials, image references, imported-image sampling, and painting dirtiness stand. This phase describes what a target DCC/engine export writes, which channels occupy which image lanes, and emits a manifest that downstream image writers can follow.

## Scope delivered

1. Export targets and options
   - Added `MaterialExportTarget` presets for Slate, Blender, Unreal, Unity, and glTF.
   - Added image format, bit-depth, normal convention, resolution, dilation, output name, and output directory options.

2. Channel packing presets
   - Albedo/opacity export declaration.
   - Normal export declaration with DirectX green-channel inversion support.
   - Unreal/glTF ORM packing: R ambient occlusion, G roughness, B metallic, A opacity.
   - Unity mask packing: R metallic, G ambient occlusion, B displacement, A inverted roughness.
   - Slate/Blender generic roughness/metallic/occlusion/opacity packing.
   - Emission declaration.

3. Package filtering
   - `BuildMaterialExportPackage()` filters preset images to channels the material consumes or explicitly declares.
   - The package records exported channel masks, referenced image counts, target options, reflectance selection, and a material fingerprint.

4. Manifest generation
   - `EncodeMaterialExportManifest()` emits a JSON-like manifest naming output files and lane/channel/inversion assignments.
   - This is an export plan, not a pixel writer; it keeps the actual image generation as the next implementation seam.

## Still out of scope

- Writing PNG/TGA/EXR files.
- GPU or CPU flattened image generation.
- Export UI wiring.
- Per-face material IDs from imported meshes.
