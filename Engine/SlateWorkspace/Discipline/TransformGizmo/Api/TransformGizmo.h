//============================================================================================================================================
//                                                         TRANSFORMGIZMO.H
//============================================================================================================================================
// 🧩 The handles the artist grabs to move, turn and resize a selection.
//
// 🔴 A GIZMO IS A SCREEN-SPACE OBJECT. It is a constant size in pixels however far the camera stands off,
//    because it is a control, not part of the drawing. An arrow the artist can no longer hit after zooming
//    out is a broken control; an arrow that swallows the whole viewport after zooming in is worse.
//
// ⚠️ THE HOST HAD THE HIT TEST AND THE DRAWING MEASURED IN DIFFERENT UNITS. `ResolveGizmoHandle` tested a
//    44-PIXEL axis while `RecordViewportGizmo` drew a 78-WORLD-UNIT shaft with a cone out at 102 and a
//    scale box at 94, then projected them. Those agree at exactly one zoom level and nowhere else:
//
//        OrthoScale   drawn shaft      hit-test reach
//        0.5           39 px            44 px          (near enough)
//        1.0           78 px            44 px
//        4.0          312 px            44 px          the arrow is seven times longer than its hit box
//        32.0        2496 px            44 px          the arrow crosses the display; only its root grabs
//
//    Zoomed in, the artist grabs empty space near the pivot and gets the arrow; zoomed out, the arrow is
//    smaller than its own hit box and clicking beside it still grabs. The measurements below are the ONE
//    table both halves read, in pixels, so the two cannot drift apart again.
//
// 📝 The drawing converts each pixel measurement to world at the pivot through `WorldPerPixel`, so the
//    projected result comes back the size the table asked for.

#pragma once

#include "SlateWorkspace/Discipline/TransformSession/Api/TransformSession.h"

namespace Slate
{

/// 🧩 Which handle the pointer is over.
enum class GizmoHandle : std::uint32_t
{
    None      = 0u,
    MoveFree  = 1u,
    MoveX     = 2u,
    MoveZ     = 3u,
    Rotate    = 4u,
    ScaleFree = 5u,
    ScaleX    = 6u,
    ScaleZ    = 7u
};

/// 🧩 The one table of measurements, in screen pixels.
///
/// 🔴 Both the hit test and the drawing read THESE and nothing else. A measurement that appears in only
///    one of the two is the defect this unit was written to remove.
///
/// 🔴 THE REACHES ARE DISJOINT, AND THAT IS A DESIGNED PROPERTY, NOT AN ACCIDENT. No two handles claim the
///    same pixel, so the order they are tested in cannot change the answer. The alternative — overlapping
///    reaches resolved by test order — means the handle an artist gets depends on a line ordering in a
///    switch rather than on where they pointed, and every later edit risks silently reordering it.
///
///    Along an axis, from the pivot outwards:
///        0 .. 9      the centre nub
///        9 .. 10     nothing
///       10 .. 51     the arrow  (drawn 17..44, grabbed within 7 of that segment)
///
///    The free-move square sits diagonally at 16 out, so its nearest edge is 9 pixels off BOTH axes and
///    clear of both arrows. The rotation band is 28..44 from the pivot. The scale boxes are at 44.
struct GizmoMeasure
{
    /// The axis arrow: shaft from `ShaftStart` out to `ShaftEnd`, cone tip at `ArrowTip`.
    /// ⚠️ `ShaftStart - ShaftGrab` must stay greater than `CentreGrab`, or the arrow's grab region
    ///    reaches back over the nub and the nub becomes unreachable. That is not hypothetical: the first
    ///    version of this table hit-tested the shaft from the pivot itself, and the proof caught that
    ///    clicking the centre in Move always answered the X arrow.
    static constexpr double ShaftStart   = 17.0;
    static constexpr double ShaftEnd     = 44.0;
    static constexpr double ArrowTip     = 58.0;
    static constexpr double ArrowRadius  = 5.5;
    static constexpr double ShaftRadius  = 1.7;
    static constexpr double ShaftGrab    = 7.0;

    /// The free-move square, offset diagonally from the pivot.
    static constexpr double PlaneOffset  = 16.0;
    static constexpr double PlaneHalf    = 7.0;

    /// The centre nub, and the ring a rotation is dragged around.
    static constexpr double CentreGrab   = 9.0;
    static constexpr double RingRadius   = 36.0;
    static constexpr double RingGrab     = 8.0;

    /// The cube on the end of a scale axis.
    static constexpr double ScaleBox     = 44.0;
    static constexpr double ScaleBoxHalf = 5.0;
    static constexpr double ScaleGrab    = 10.0;
};

/// 🧩 Where the gizmo sits on screen and which way its axes run from there.
struct GizmoScreenBasis
{
    float PivotX = 0.0f;
    float PivotY = 0.0f;
    float AlongX = 1.0f;
    float AlongY = 0.0f;
    float AcrossX = 0.0f;
    float AcrossY = -1.0f;

    /// 🔴 How many world units one screen pixel covers AT THE PIVOT. This is what lets the drawing express
    ///    the pixel table above in the world units it has to build geometry from.
    /// note ⚠️ Measured at the pivot, not globally. Under perspective the conversion differs across the
    ///       display, and a gizmo forty pixels wide at the centre would be sixty at the edge if a single
    ///       factor were used everywhere.
    double WorldPerPixel = 1.0;
};

/// 🧩 Reads where the gizmo stands from the camera and the pivot.
/// out   Named  [-]  false when the pivot is behind the camera and there is nothing to draw or grab
bool ResolveGizmoScreenBasis(const SpatialBasis& Basis,
                             const ViewportStanding& View,
                             bool Perspective,
                             const PlaneExtent& Extent,
                             const SpatialPoint& Pivot,
                             GizmoScreenBasis& Resolved);

/// 🧩 Which handle a pointer position is over.
/// note 🔴 The reaches in `GizmoMeasure` are disjoint, so this answers the same handle whatever order the
///       arms are written in. Compare `SketchPicking`, where a curve genuinely passes through its own
///       endpoints and the order therefore IS the design — here the geometry is separated instead, which
///       is the stronger arrangement when it can be had.
GizmoHandle ResolveGizmoHandle(const GizmoScreenBasis& Screen,
                               TransformManner Manner,
                               float PointerX,
                               float PointerY);

/// 🧩 A pixel measurement expressed in world units at the pivot, for building the drawn geometry.
/// note 📝 One line, but it is the line that keeps the two halves honest. Everything the gizmo draws goes
///       through it, so the drawn size is the table's size by construction rather than by coincidence.
inline double GizmoWorld(const GizmoScreenBasis& Screen, double Pixels)
{
    return Pixels * Screen.WorldPerPixel;
}

/// 🧩 Which manner a handle belongs to, and what it restricts the drag to.
/// note ⚠️ A handle names both at once — grabbing the X arrow is "move, along X" in one gesture — so the
///       two are read from it together rather than inferred separately at each call site.
TransformManner      ResolveHandleManner(GizmoHandle Handle);
TransformRestriction ResolveHandleRestriction(GizmoHandle Handle);

}   // namespace Slate
