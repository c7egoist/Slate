//============================================================================================================================================
//                                                            EXTENTBANDS.H
//============================================================================================================================================
// 🧩 Splitting a rectangle around a rectangle: the arithmetic that lets a one-rectangle scissor exclude a box.
//
// 🔴 WHY THIS IS IN FOUNDATION AND NOT BESIDE `PlaneExtent`. The two callers are the GPU passes in
//    `SlateVulkan`, and `SlateVulkan` requires `SlateMath` alone — deliberately, so nothing device-side
//    can name an interface type. `PlaneExtent` lives in `SlateUI`, so stating the boxes in its terms
//    would have quietly inverted that dependency. The arithmetic is pure numbers and belongs to no
//    layer, so it sits here and both sides convert at the edge.

#pragma once

#include <cstdint>

namespace Slate
{

/// 🧩 One axis-aligned box in the caller's own units, as its two corners.
/// note  Deliberately anonymous about which space it is in. The GPU passes hold physical pixels and the
///       interface holds logical ones; this arithmetic is correct in either and mixes neither.
struct ExtentBand
{
    float MinimumX = 0.0f;
    float MinimumY = 0.0f;
    float MaximumX = 0.0f;
    float MaximumY = 0.0f;

    constexpr bool Occupied() const { return MaximumX > MinimumX && MaximumY > MinimumY; }
};

/// 🧩 The at-most-four disjoint bands of a clip box that a withheld box does not cover.
/// in    ClipX0..ClipY1          [-] the box to record
/// in    WithheldX0..WithheldY1  [-] the box to keep clear; a zero-area box yields the clip alone
/// out   Bands                   [-] filled from index zero
/// out   Result                  [-] how many bands were written: 0, 1, 2, 3 or 4
/// note  🔴 A SCISSOR IS ONE RECTANGLE. "Everything except this box" therefore has to be recorded as
///        several, and the GPU passes that record AFTER the interface — the grid, the axes and the
///        sketch — need exactly that to stop texturing over an open menu. Drawn over, an opaque menu
///        reads as a transparent one, because what shows through it is the viewport behind.
/// note  ⚠️ The bands are DISJOINT by construction: the upper and lower ones span the full width, and
///        the leading and trailing ones are cut to the withheld box's own rows. Overlapping bands would
///        shade a fragment twice, which a straight-alpha blend shows as a seam.
/// note  ⚠️ Both boxes must be in the same space. Mixing logical and physical ordinates clips the wrong
///        area on any display whose scale is not one, and does so silently.
/// cost  ✔️
/// tag   guarantee, nonallocating, nonthrowing
constexpr std::uint32_t ExtentBandsAround(float ClipX0, float ClipY0, float ClipX1, float ClipY1,
                                          float WithheldX0, float WithheldY0,
                                          float WithheldX1, float WithheldY1,
                                          ExtentBand (&Bands)[4])
{
    std::uint32_t Written = 0u;

    // 🔴 THE ONLY GUARD, DELIBERATELY. Every degenerate case reduces to a band with no area: an
    //    inverted clip, a zero-width clip, a withheld box flush against an edge, and a withheld box
    //    covering the clip entirely. Testing the clip separately at the top read as defensive and was
    //    measurably redundant -- removing it changed no claim in the proof -- so there is one gate here
    //    rather than two that could disagree.
    const auto Emit = [&](float X0, float Y0, float X1, float Y1)
    {
        if (X1 > X0 && Y1 > Y0)
            Bands[Written++] = ExtentBand{ X0, Y0, X1, Y1 };
    };

    const float HoldX0 = ClipX0 > WithheldX0 ? ClipX0 : WithheldX0;
    const float HoldY0 = ClipY0 > WithheldY0 ? ClipY0 : WithheldY0;
    const float HoldX1 = ClipX1 < WithheldX1 ? ClipX1 : WithheldX1;
    const float HoldY1 = ClipY1 < WithheldY1 ? ClipY1 : WithheldY1;

    // Nothing withheld, or it misses this box entirely.
    if (HoldX1 <= HoldX0 || HoldY1 <= HoldY0)
    {
        Emit(ClipX0, ClipY0, ClipX1, ClipY1);
        return Written;
    }

    Emit(ClipX0, ClipY0, ClipX1, HoldY0);    // above
    Emit(ClipX0, HoldY1, ClipX1, ClipY1);    // below
    Emit(ClipX0, HoldY0, HoldX0, HoldY1);    // leading
    Emit(HoldX1, HoldY0, ClipX1, HoldY1);    // trailing

    return Written;
}

}   // namespace Slate
