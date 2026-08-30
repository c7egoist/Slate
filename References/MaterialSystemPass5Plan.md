# Material System Pass 5 Plan

Status: implemented for the document/compute painting MVP.

## Goal

Connect Slate's existing brush/impression machinery to the material layer system without adding painting UI to the parametric host. This phase creates document-owned painted material layers, prepares stroke declarations from brush channels and layer packing, commits resolved strokes as one transaction, and reports precise dirty tiles/channels for later GPU/tile refresh.

## Scope delivered

1. Painted material layer creation
   - Added `MaterialPaintExchange::CreatePaintedLayer()`.
   - Creates `LayerContentSource::PaintedImpressions` entries with owned `PaintedContent` texels.
   - Validates channel mask, working extent and component count before appending.

2. Stroke declaration bridge
   - Added `MaterialPaintExchange::DeclareStroke()`.
   - Verifies the target layer is painted content and its working extent/component count match.
   - Builds `ChannelPlacement` entries from the brush channel list.
   - Refuses brushes that write channels the layer does not own or that do not fit the component packing.

3. Commit and dirty reporting
   - Added `MaterialPaintExchange::CommitResolvedStroke()`.
   - Uses the existing `ImpressionSequence::Seal()` path so one stroke remains one revision transaction and speculative strokes still refuse.
   - Converts the sealed stroke's touched cells into `MaterialPaintDirtyTile` records.
   - Reports dirty channel masks and material snapshot fingerprints.

4. No parametric paint UI
   - This phase is document/compute only.
   - `ParametricSketchHost` is not given Texture Paint workspace logic.

## Still out of scope

- Brush UI and interactive painting controls.
- GPU tile upload/refresh.
- Painted mask UI.
- UV editor.
- PNG/JPEG/WebP/EXR sampled painting sources.
