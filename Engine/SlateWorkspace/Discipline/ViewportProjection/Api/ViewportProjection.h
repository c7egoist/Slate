//============================================================================================================================================
//                                                        VIEWPORTPROJECTION.H
//============================================================================================================================================
// 🧩 Where a point on the sketch plane lands on screen, and which point on the sketch plane a screen
//    position names. The standing orientation, the frame it implies, and the two directions between the
//    plane and the display.
//
// 🔴 This is the mathematics ONLY. Nothing here records, draws, or holds a surface — it answers "where is
//    this" and hands the answer back. The recording that uses those answers stays with the surface that
//    owns it, which is why `ProjectViewportPoint` is here and `RecordViewportGrid` is not.
//
// 🔴 The projection and its inverse both live here deliberately. They are two statements of one transform,
//    and a projection whose inverse disagrees with it puts the cursor somewhere other than where the artist
//    is pointing. Keeping them together is what lets `ViewportProjectionProof` round-trip a point through
//    both and require it to come back.
//
// 📝 Lifted verbatim out of `ParametricSketchHost` at step 10e, where it sat between the viewport drawing
//    and the snap logic and could not be exercised without a Vulkan device.

#pragma once

#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"
#include "SlateUI/Interface/InterfaceExchange/Api/RecordingSurface.h"

#include <cmath>
#include <cstdint>

namespace Slate
{

//------------------------------------------------------------------------------------------------------------------------
//                                                    THE PLANE AND THE VIEW
//------------------------------------------------------------------------------------------------------------------------

/// 🧩 The sketch plane as three orthonormal directions about an origin.
/// note 📝 `Along` and `Across` span the plane; `Normal` leaves it. A planar coordinate pair is measured
///       against the first two, which is what makes a sketch two-dimensional while living in space.
struct SpatialBasis
{
    SpatialPoint     Origin = {};
    SpatialDirection Along  = { 1.0, 0.0, 0.0 };
    SpatialDirection Across = { 0.0, 0.0, 1.0 };
    SpatialDirection Normal = { 0.0, 1.0, 0.0 };
};

/// 🧩 Which way the viewport is looking.
/// note ⚠️ `Isometric` is the only orientation that is not axis-aligned, and it is the one an orbit lands
///       in. The six named views are exact; the seventh is wherever the artist dragged to.
enum class ViewportOrientation : std::uint32_t
{
    Top       = 0u,
    Bottom    = 1u,
    Front     = 2u,
    Back      = 3u,
    Left      = 4u,
    Right     = 5u,
    Isometric = 6u
};

/// 🧩 Everything the viewport remembers about where it is looking from.
struct ViewportStanding
{
    ViewportOrientation Orientation = ViewportOrientation::Top;
    SpatialPoint        Focus       = {};
    double              Distance    = 240.0;
    double              OrthoScale  = 3.0;
    double              OrbitYaw    = 45.0;
    double              OrbitPitch  = 30.0;
};

/// 🧩 The camera the standing view resolves to: where the eye is and which way is right, up and forward.
struct ViewFrame
{
    SpatialPoint     Eye     = {};
    SpatialDirection Right   = { 1.0, 0.0, 0.0 };
    SpatialDirection Up      = { 0.0, 0.0, 1.0 };
    SpatialDirection Forward = { 0.0, -1.0, 0.0 };
};

/// 🧩 The vertical angle a perspective viewport subtends, in degrees.
constexpr double CadPerspectiveFieldOfViewDegrees = 42.0;

/// 🧩 Half a turn, in radians per degree.
constexpr double ProjectionPi = 3.14159265358979323846;

//------------------------------------------------------------------------------------------------------------------------
//                                                  THE PLANE THE SKETCH IS ON
//------------------------------------------------------------------------------------------------------------------------

// 🔴 `ResolveSketchBasis` — the one function here that reads a `SketchStructure` — lives in the separate
//    `SketchBasis.h` beside this file. Declaring it here would make every consumer of the projection
//    depend on the whole sketch kernel to link, including a proof that only wants the arithmetic. The
//    projection's behaviour depends on a basis, never on where the basis came from.

/// 🧩 The spatial point a planar coordinate pair names.
SpatialPoint ResolvePlanarPoint(const SpatialBasis& Basis, double Along, double Across);

/// 🧩 The planar coordinate pair a spatial point measures to.
/// note ⚠️ A point off the plane is measured by its projection onto it; the normal distance is discarded.
void ResolvePlaneCoordinates(const SpatialBasis& Basis, const SpatialPoint& Position,
                             double& Along, double& Across);

//------------------------------------------------------------------------------------------------------------------------
//                                                    WHERE THE VIEW LOOKS
//------------------------------------------------------------------------------------------------------------------------

/// 🧩 Points the view at a named orientation.
/// note 🔴 In an orthographic view the orientation alone decides the frame, so nothing else is written. In
///       a perspective view the orientation is expressed as an orbit, because a perspective camera has a
///       position and the named views are only angles.
/// note ⚠️ Top and bottom orbit to ±89°, not ±90°. At exactly 90° the forward direction is parallel to the
///       normal and the right direction is the cross of two parallel vectors, which is nothing at all — the
///       frame would collapse. One degree short is what shipped and it is load-bearing.
void ApplyViewportOrientation(ViewportStanding& View, ViewportOrientation Orientation, bool Perspective);

/// 🧩 The word for an orientation.
/// note 📝 `Isometric` reads as "Perspective", because that is the control the artist reached for.
const char* OrientationText(ViewportOrientation Orientation);

/// 🧩 The camera the standing view resolves to.
ViewFrame ResolveViewportFrame(const SpatialBasis& Basis, const ViewportStanding& View, bool Perspective);

//------------------------------------------------------------------------------------------------------------------------
//                                              BETWEEN THE PLANE AND THE SCREEN
//------------------------------------------------------------------------------------------------------------------------

/// 🧩 Where a planar coordinate pair lands on screen.
/// out   ScreenX, ScreenY  [-]  written only when the point is in front of the eye
/// out   Standing          [-]  false when the point is behind a perspective eye
/// note 🔴 An orthographic view always projects; only perspective can refuse. A point at or behind the eye
///       has no screen position, and returning one anyway would draw it mirrored through the origin.
bool ProjectViewportPoint(const SpatialBasis& Basis,
                          const ViewportStanding& View,
                          bool Perspective,
                          const PlaneExtent& Extent,
                          double Along,
                          double Across,
                          float& ScreenX,
                          float& ScreenY);

/// 🧩 Where a spatial point lands on screen.
/// note 📝 The same projection as above, entered from a point already in space rather than from planar
///       coordinates. It does NOT require the point to lie on the plane.
bool ProjectSpatialPoint(const SpatialBasis& Basis,
                         const ViewportStanding& View,
                         bool Perspective,
                         const PlaneExtent& Extent,
                         const SpatialPoint& Position,
                         float& ScreenX,
                         float& ScreenY);

/// 🧩 Which point on the sketch plane a screen position names.
/// out   Position  [-]  written only when the ray meets the plane in front of the viewer
/// out   Standing  [-]  false when the ray runs parallel to the plane, or meets it behind the viewer
/// note 🔴 This is the inverse of `ProjectViewportPoint`, and the pair is what puts a placed point under
///       the cursor. `ViewportProjectionProof` round-trips through both and requires the point back.
/// note ⚠️ Parallel is tested against 1e-6, not against zero. A ray a hundredth of a degree off parallel
///       meets the plane millions of units away, and treating that as a hit would place a point at an
///       absurd distance rather than refusing.
bool ResolveViewportPlaneIntersection(const SpatialBasis& Basis,
                                      const ViewportStanding& View,
                                      bool Perspective,
                                      const PlaneExtent& Extent,
                                      float ScreenX,
                                      float ScreenY,
                                      SpatialPoint& Position);

/// 🧩 How far from a point, in plane units, still counts as touching it.
/// note 📝 Scales with the view so a snap stays roughly the same size on screen however far the artist has
///       zoomed. The two arms are unrelated formulas because the two projections are: an orthographic
///       tolerance divides by the scale, a perspective one grows with the distance to the focus.
/// note ⚠️ Both arms carry a floor — 2.0 plane units in perspective, 0.25 orthographic — so a snap never
///       collapses to nothing at extreme zoom. The orthographic arm also floors the DIVISOR at 0.001,
///       because a scale of zero would otherwise divide by it.
double ResolveSnapTolerance(const ViewportStanding& View, bool Perspective);

}   // namespace Slate
