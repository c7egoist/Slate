# Material System Pass 2 Plan

Status: implemented for the current MVP slice.

## Goal

Move the material work from document-only plumbing into viewport-facing scene rendering: constant/fill material records should produce renderer-ready scene material packets, the dedicated scene pass should own geometry and material buffers, and Lit/Matcap/Wire/Points diagnostic modes should route through one view-mode push path.

## Scope delivered

1. Scene pass material binding
   - `WorkspaceScenePass` now owns a material buffer beside its triangle buffer.
   - `WorkspaceSceneMaterial` mirrors the renderer-facing physical surface constants without pulling document strata into `SlateVulkan`.
   - `UploadMaterials()` uploads document-resolved material constants and dirty fingerprints.

2. Scene pass shaders
   - Added `WorkspaceSceneVertex.slang` and `WorkspaceSceneFragment.slang`.
   - The vertex stage expands triangle records directly from storage buffers.
   - Points mode expands each source vertex into a small marker quad.
   - The fragment stage consumes the material packet for Lit, Matcap, Source Wire, Triangulated Wire, Points, Normal, Metallic, and Illumination modes.

3. Pipeline construction
   - `ConstructWorkspaceScenePass()` now resolves shader modules, allocates storage descriptors, creates a dynamic-rendering graphics pipeline, and labels the pipeline/buffer.
   - `Record()` binds the scene descriptors, pushes projection/view-mode state, and draws either triangle fill or point markers.

4. CPU fallback material preview
   - The current parametric host CPU scene proxy now derives its fill colour from the scene entry's document-backed material record instead of only hard-coded white.
   - This keeps the shared viewport honest until the GPU scene pass is wired into the host frame path.

## Still out of scope

- Image material sources.
- UV editor.
- Painting.
- Full texture sampling or tile dirtiness.
- Per-face imported material assignment beyond slot-level material records.
- Depth-buffer integration and host frame sequencing for the new scene pass.
