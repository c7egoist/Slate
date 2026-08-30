# Realtime denoiser and lens flare recommendations

Date: 2026-08-25

Scope: research and recommendation only. This complements the ReSTIR GI plan with quality tiers for denoising and lens flare.

## Denoiser recommendation

### Top 3 denoiser choices

1. NVIDIA NRD
   - Best fit for Slate's ReSTIR GI and RTX path.
   - Supports Vulkan integration and the exact signal split needed for realtime ray tracing: diffuse/specular radiance, hit distance, normal, roughness, depth, motion and history.
   - Use REBLUR for most diffuse/specular GI, RELAX for RTXDI/ReSTIR direct-light signals or cleaner high-ray signals, and SIGMA for shadow masks.
   - Recommendation: primary high-quality realtime denoiser.

2. AMD FidelityFX Denoiser
   - Best fit for open, lightweight, cross-vendor shadow/reflection denoising.
   - Provides specialized spatio-temporal denoisers for shadows and reflections, supports DirectX 12 and Vulkan, and is MIT licensed.
   - It is not a full GI denoiser replacement for NRD, but it is valuable for economic modes and as a reference for Slate's built-in fallback filters.
   - Recommendation: economic/standard fallback for shadows and reflections, plus design reference for internal filters.

3. Intel Open Image Denoise
   - Best fit for cinematic stills, offline capture, lightmap/path-traced preview output and high-quality non-interactive renders.
   - OIDN 2.x runs on CPU and GPUs from Intel, AMD and NVIDIA, and is widely used in DCC/rendering pipelines.
   - Current OIDN 2.x is spatial-image oriented rather than a game-frame temporal denoiser, so it should not be the main realtime gameplay denoiser.
   - Recommendation: cinematic/export/offline denoiser, not the per-frame game path.

### Slate quality tiers

Economic:
- Use Slate built-in temporal accumulation plus edge-aware À-trous/SVGF-style fallback.
- Optionally use AMD FidelityFX shadow/reflection denoisers where matching signals exist.
- Target GTX 1060+ and other non-ray-query devices.
- Quarter or checkerboard half-resolution GI.

Standard:
- Use NRD REBLUR diffuse/specular where available.
- Use SIGMA for selected ray-traced shadows.
- Half-resolution GI with stable motion vectors and conservative history validation.

Quality:
- Use NRD REBLUR high settings or RELAX for ReSTIR direct-light signals.
- Separate diffuse/specular histories.
- Add normalized hit distance and better firefly clamp.

Ultra:
- Use NRD SG/SH-style directional variants where the signal quality justifies the memory cost.
- Full or near-full resolution GI on RTX-class hardware.
- More spatial reuse and stricter reactive/disocclusion masks.

Cinematic:
- Use OIDN for still capture, viewport final-quality snapshot, lightmap bake preview or offline sequence export.
- Keep NRD for interactive preview, then hand OIDN a higher-sample accumulated buffer for final output.

## Lens flare recommendation

### Top 3 lens flare choices

1. Economic screen-space flare
   - Bright-pass threshold, downsample chain, dirt mask, halo, streaks and a few ghost sprites.
   - Occlude with depth around bright source positions.
   - Cheap enough for GTX baseline and stylized game use.
   - This should be Slate's first lens flare implementation.

2. Standard/Quality convolution bloom flare
   - Use HDR bloom with custom kernel textures, including anamorphic streak, starburst and dirt responses.
   - Unreal's convolution bloom documentation describes physically motivated bloom via an FFT-accelerated convolution kernel.
   - Best fit for standard and quality presets because it is stable, art-directable and scalable.

3. Ultra/Cinematic physical lens ghosting
   - Model lens ghost families from an optical system, cull weak ghosts, then draw surviving ghost patches or sprites.
   - The 2023 efficient tile-based lens ghost approach is the right research direction for a realtime high tier.
   - Best fit for cutscenes, photo mode, sun/moon flares and hero lights; too expensive for every small light on GTX.

### Slate lens flare tiers

Economic:
- Screen-space bright-pass flare.
- Dirt mask and small sprite ghost chain.
- Optional anamorphic horizontal streak.
- Per-light intensity threshold and depth occlusion.

Standard:
- Add starburst kernel, lens dirt and controlled ghost count.
- Tie flare colour to source temperature or light colour.
- Use temporal smoothing so flare elements do not shimmer.

Quality:
- Add convolution bloom option for HDR highlights.
- Add aperture-shape selection and chromatic fringe.
- Add better depth-aware occlusion and edge fading.

Ultra:
- Add physically inspired ghost placement from lens parameters.
- Culling pass removes invisible/low-energy ghosts before drawing.
- Use compute-generated indirect draw lists.

Cinematic:
- Physical lens profile presets: clean prime, anamorphic, vintage, sci-fi, dirty glass.
- Higher ghost count, aperture diffraction, spectral tint and film-grade dirt response.
- Suitable for cutscene/photo mode/offline export first, then scaled down for gameplay.

## Integration order

1. Denoiser signals first
   - Add diffuse/specular noisy radiance, hit distance, normal, roughness, depth, motion and confidence buffers.
   - Without these, neither NRD nor a strong internal denoiser can be stable.

2. Internal fallback denoiser
   - Implement temporal accumulation and edge-aware spatial filtering before taking external dependencies.
   - This provides GTX and non-NVIDIA coverage.

3. Optional NRD bridge
   - Add capability/configuration switch for NRD.
   - Keep Slate-owned signal layout and translate into NRD frontend packing.

4. Lens flare economic path
   - Implement bright-pass, downsample, dirt, streak and ghost sprite path.
   - Add debug controls for threshold, intensity, dirt, ghost count and occlusion.

5. Convolution/physical flare tiers
   - Add HDR kernel convolution for Quality.
   - Add physical ghost generation for Ultra/Cinematic.

## Source trail

- NVIDIA NRD README: https://github.com/NVIDIA-RTX/NRD
- AMD FidelityFX Denoiser: https://gpuopen.com/fidelityfx-denoiser/
- AMD FidelityFX Denoiser manual: https://gpuopen.com/manuals/fidelityfx_sdk/techniques/denoiser/
- Intel Open Image Denoise 2 GPU support summary: https://www.cgchannel.com/2023/07/intel-releases-open-image-denoise-2/
- Unreal Engine bloom/convolution bloom: https://dev.epicgames.com/documentation/unreal-engine/bloom-in-unreal-engine
- Lens flare ghost reference implementation: https://github.com/bodonyiandi94/LensFlareFramework
