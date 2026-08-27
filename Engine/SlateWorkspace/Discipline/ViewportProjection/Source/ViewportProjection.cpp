//============================================================================================================================================
//                                                       VIEWPORTPROJECTION.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/ViewportProjection/Api/ViewportProjection.h"

#include <algorithm>

namespace Slate
{

//------------------------------------------------------------------------------------------------------------------------
//                                                  THE PLANE THE SKETCH IS ON
//------------------------------------------------------------------------------------------------------------------------


SpatialPoint ResolvePlanarPoint(const SpatialBasis& Basis, double Along, double Across)
{
    return Added(Basis.Origin,
                 Added(Scaled(Basis.Along, Along),
                       Scaled(Basis.Across, Across)));
}

void ResolvePlaneCoordinates(const SpatialBasis& Basis, const SpatialPoint& Position,
                             double& Along, double& Across)
{
    const SpatialDirection Offset = Difference(Basis.Origin, Position);
    Along  = Dot(Offset, Basis.Along);
    Across = Dot(Offset, Basis.Across);
}

//------------------------------------------------------------------------------------------------------------------------
//                                                    WHERE THE VIEW LOOKS
//------------------------------------------------------------------------------------------------------------------------

void ApplyViewportOrientation(ViewportStanding& View, ViewportOrientation Orientation, bool Perspective)
{
    View.Orientation = Orientation;

    // 🔴 An orthographic frame is decided by the orientation alone, so the orbit is left untouched. That
    //    is what lets an artist switch to Top, back to Isometric, and find the orbit where they left it.
    if (!Perspective)
        return;

    switch (Orientation)
    {
        case ViewportOrientation::Isometric:
            View.OrbitYaw   = 45.0;
            View.OrbitPitch = 30.0;
            break;

        // ⚠️ 89°, not 90°. At exactly 90° the forward direction is parallel to the plane normal, the right
        //    direction becomes the cross of two parallel vectors — nothing at all — and the whole frame
        //    collapses. One degree short is what shipped, and it is load-bearing.
        case ViewportOrientation::Top:
            View.OrbitYaw   = 0.0;
            View.OrbitPitch = 89.0;
            break;

        case ViewportOrientation::Bottom:
            View.OrbitYaw   = 0.0;
            View.OrbitPitch = -89.0;
            break;

        case ViewportOrientation::Front:
            View.OrbitYaw   = 0.0;
            View.OrbitPitch = 0.0;
            break;

        case ViewportOrientation::Back:
            View.OrbitYaw   = 180.0;
            View.OrbitPitch = 0.0;
            break;

        case ViewportOrientation::Left:
            View.OrbitYaw   = -90.0;
            View.OrbitPitch = 0.0;
            break;

        case ViewportOrientation::Right:
            View.OrbitYaw   = 90.0;
            View.OrbitPitch = 0.0;
            break;
    }
}

const char* OrientationText(ViewportOrientation Orientation)
{
    switch (Orientation)
    {
        case ViewportOrientation::Top:       return "Top";
        case ViewportOrientation::Bottom:    return "Bottom";
        case ViewportOrientation::Front:     return "Front";
        case ViewportOrientation::Back:      return "Back";
        case ViewportOrientation::Left:      return "Left";
        case ViewportOrientation::Right:     return "Right";
        case ViewportOrientation::Isometric: return "Perspective";
    }
    return "Top";
}

ViewFrame ResolveViewportFrame(const SpatialBasis& Basis, const ViewportStanding& View, bool Perspective)
{
    if (!Perspective)
    {
        // 📝 The eye sits 100 units off the plane in every orthographic view. The distance does not affect
        //    an orthographic projection at all — it exists only so the eye is on the correct side.
        switch (View.Orientation)
        {
            case ViewportOrientation::Top:
                return { Added(View.Focus, Scaled(Basis.Normal, 100.0)),
                         Basis.Along, Basis.Across, Negated(Basis.Normal) };
            case ViewportOrientation::Bottom:
                return { Added(View.Focus, Scaled(Basis.Normal, -100.0)),
                         Basis.Along, Negated(Basis.Across), Basis.Normal };
            case ViewportOrientation::Front:
                return { Added(View.Focus, Scaled(Basis.Across, -100.0)),
                         Basis.Along, Basis.Normal, Basis.Across };
            case ViewportOrientation::Back:
                return { Added(View.Focus, Scaled(Basis.Across, 100.0)),
                         Basis.Along, Negated(Basis.Normal), Negated(Basis.Across) };
            case ViewportOrientation::Left:
                return { Added(View.Focus, Scaled(Basis.Along, -100.0)),
                         Basis.Across, Basis.Normal, Basis.Along };
            case ViewportOrientation::Right:
                return { Added(View.Focus, Scaled(Basis.Along, 100.0)),
                         Negated(Basis.Across), Basis.Normal, Negated(Basis.Along) };
            case ViewportOrientation::Isometric:
                break;
        }
    }

    const double Yaw   = View.OrbitYaw * ProjectionPi / 180.0;
    const double Pitch = View.OrbitPitch * ProjectionPi / 180.0;

    const SpatialDirection Forward = Normalize(Added(
        Added(Scaled(Basis.Along, std::sin(Yaw) * std::cos(Pitch)),
              Scaled(Basis.Normal, std::sin(Pitch))),
        Scaled(Negated(Basis.Across), std::cos(Yaw) * std::cos(Pitch))));

    const SpatialDirection Right = Normalize(Cross(Forward, Basis.Normal));
    const SpatialDirection Up    = Normalize(Cross(Right, Forward));

    return { Added(View.Focus, Scaled(Forward, -View.Distance)), Right, Up, Forward };
}

//------------------------------------------------------------------------------------------------------------------------
//                                              BETWEEN THE PLANE AND THE SCREEN
//------------------------------------------------------------------------------------------------------------------------

bool ProjectViewportPoint(const SpatialBasis& Basis,
                          const ViewportStanding& View,
                          bool Perspective,
                          const PlaneExtent& Extent,
                          double Along,
                          double Across,
                          float& ScreenX,
                          float& ScreenY)
{
    const ViewFrame    Frame    = ResolveViewportFrame(Basis, View, Perspective);
    const SpatialPoint Position = ResolvePlanarPoint(Basis, Along, Across);

    if (!Perspective)
    {
        const SpatialDirection Offset = Difference(View.Focus, Position);
        const double X = Dot(Offset, Frame.Right);
        const double Y = Dot(Offset, Frame.Up);

        // 📝 Screen Y grows downwards while the frame's up direction grows upwards, which is the whole
        //    reason this one term is subtracted rather than added.
        ScreenX = static_cast<float>(Extent.MinimumX + Extent.Width() * 0.5 + X * View.OrthoScale);
        ScreenY = static_cast<float>(Extent.MinimumY + Extent.Height() * 0.5 - Y * View.OrthoScale);
        return true;
    }

    const SpatialDirection EyeToPoint = Difference(Frame.Eye, Position);
    const double CameraX = Dot(EyeToPoint, Frame.Right);
    const double CameraY = Dot(EyeToPoint, Frame.Up);
    const double CameraZ = Dot(EyeToPoint, Frame.Forward);

    // 🔴 A point at or behind the eye has no screen position. Dividing by it anyway would place the point
    //    mirrored through the centre of the viewport, which draws geometry that is behind the artist.
    if (CameraZ <= 0.01)
        return false;

    const double TanHalf = std::tan(CadPerspectiveFieldOfViewDegrees * 0.5 * ProjectionPi / 180.0);
    const double Focal   = (Extent.Height() * 0.5) / TanHalf;

    ScreenX = static_cast<float>(Extent.MinimumX + Extent.Width() * 0.5 + CameraX / CameraZ * Focal);
    ScreenY = static_cast<float>(Extent.MinimumY + Extent.Height() * 0.5 - CameraY / CameraZ * Focal);
    return true;
}

bool ProjectSpatialPoint(const SpatialBasis& Basis,
                         const ViewportStanding& View,
                         bool Perspective,
                         const PlaneExtent& Extent,
                         const SpatialPoint& Position,
                         float& ScreenX,
                         float& ScreenY)
{
    // 🔴 PROJECTED WHERE IT STANDS, NOT WHERE IT WOULD FALL. This went through
    //    `ResolvePlaneCoordinates` first, which DISCARDS the component along the normal — so a point
    //    fifty units above the plane projected to exactly the same pixel as its shadow on it. The header
    //    said the point need not lie on the plane and the code required it to. Measured before the fix:
    //    193 px of error in perspective for a point 50 up, 400 px for one 80 up, and in orthographic the
    //    height was discarded silently and completely.
    const ViewFrame Frame = ResolveViewportFrame(Basis, View, Perspective);

    if (!Perspective)
    {
        const SpatialDirection Offset = Difference(View.Focus, Position);
        const double X = Dot(Offset, Frame.Right);
        const double Y = Dot(Offset, Frame.Up);

        ScreenX = static_cast<float>(Extent.MinimumX + Extent.Width() * 0.5 + X * View.OrthoScale);
        ScreenY = static_cast<float>(Extent.MinimumY + Extent.Height() * 0.5 - Y * View.OrthoScale);
        return true;
    }

    const SpatialDirection EyeToPoint = Difference(Frame.Eye, Position);
    const double CameraX = Dot(EyeToPoint, Frame.Right);
    const double CameraY = Dot(EyeToPoint, Frame.Up);
    const double CameraZ = Dot(EyeToPoint, Frame.Forward);

    if (CameraZ <= 0.01)
        return false;

    const double TanHalf = std::tan(CadPerspectiveFieldOfViewDegrees * 0.5 * ProjectionPi / 180.0);
    const double Focal   = (Extent.Height() * 0.5) / TanHalf;

    ScreenX = static_cast<float>(Extent.MinimumX + Extent.Width() * 0.5 + CameraX / CameraZ * Focal);
    ScreenY = static_cast<float>(Extent.MinimumY + Extent.Height() * 0.5 - CameraY / CameraZ * Focal);
    return true;
}

bool ProjectOffsetPoint(const SpatialBasis& Basis,
                        const ViewportStanding& View,
                        bool Perspective,
                        const PlaneExtent& Extent,
                        const SpatialPoint& Centre,
                        double Along,
                        double Normal,
                        double Across,
                        float& ScreenX,
                        float& ScreenY)
{
    const SpatialPoint Position =
        Added(Centre, Added(Added(Scaled(Basis.Along, Along), Scaled(Basis.Normal, Normal)),
                            Scaled(Basis.Across, Across)));
    return ProjectSpatialPoint(Basis, View, Perspective, Extent, Position, ScreenX, ScreenY);
}

bool ResolveViewportPlaneIntersection(const SpatialBasis& Basis,
                                      const ViewportStanding& View,
                                      bool Perspective,
                                      const PlaneExtent& Extent,
                                      float ScreenX,
                                      float ScreenY,
                                      SpatialPoint& Position)
{
    const ViewFrame Frame = ResolveViewportFrame(Basis, View, Perspective);

    const double NdcX = (static_cast<double>(ScreenX) - (Extent.MinimumX + Extent.Width() * 0.5))
                      / (Extent.Width() * 0.5);
    const double NdcY = ((Extent.MinimumY + Extent.Height() * 0.5) - static_cast<double>(ScreenY))
                      / (Extent.Height() * 0.5);

    SpatialPoint     RayOrigin    = {};
    SpatialDirection RayDirection = {};

    if (!Perspective)
    {
        // 📝 Every orthographic ray runs parallel; only its origin moves with the cursor.
        const double Along  = NdcX * (Extent.Width() * 0.5) / View.OrthoScale;
        const double Across = NdcY * (Extent.Height() * 0.5) / View.OrthoScale;
        RayOrigin    = Added(View.Focus, Added(Scaled(Frame.Right, Along), Scaled(Frame.Up, Across)));
        RayDirection = Frame.Forward;
    }
    else
    {
        const double TanHalf = std::tan(CadPerspectiveFieldOfViewDegrees * 0.5 * ProjectionPi / 180.0);
        const double Aspect  = Extent.Width() / Extent.Height();
        RayOrigin    = Frame.Eye;
        RayDirection = Normalize(Added(Added(Scaled(Frame.Right, NdcX * TanHalf * Aspect),
                                             Scaled(Frame.Up, NdcY * TanHalf)),
                                       Frame.Forward));
    }

    // 🔴 `Difference(A, B)` returns the direction FROM A TO B, so this already points from the ray origin
    //    towards the plane. The shipped code then NEGATED the numerator below, which flipped the sign of
    //    every distance — see the note on the fix.
    const SpatialDirection RayToPlane  = Difference(RayOrigin, Basis.Origin);
    const double           Denominator = Dot(RayDirection, Basis.Normal);

    // ⚠️ Tested against 1e-6 rather than zero. A ray a hundredth of a degree off parallel meets the plane
    //    millions of units away, and treating that as a hit places a point at an absurd distance instead
    //    of refusing.
    if (std::fabs(Denominator) <= 1.0e-6)
        return false;

    // 🔴 FIXED AT STEP 10e — the shipped line was `-Dot(...)`, and the extra negation is a defect.
    //    Ray-plane intersection is t = Dot(RayOrigin -> PlaneOrigin, Normal) / Dot(RayDirection, Normal),
    //    and `Difference` ALREADY returns that direction, so negating it inverts every distance. An eye
    //    240 units in front of the plane resolved to -240, was read as "behind the viewer", and refused.
    //
    // ⚠️ IT SURVIVED BECAUSE THE ORTHOGRAPHIC ARM CANNOT SEE IT. There the ray origin is built from the
    //    focus, which lies ON the sketch plane, so the numerator is exactly zero and the sign is
    //    invisible. Only a perspective view puts the eye genuinely off the plane — and there, clicking in
    //    the viewport could not place a point at all.
    const double Distance = Dot(RayToPlane, Basis.Normal) / Denominator;

    // 🔴 A negative distance means the plane is behind the EYE, and only a perspective view has one. An
    //    orthographic projection is parallel with no eye at all: its ray origin is just a point on the
    //    projection plane, chosen so the ray passes through the cursor, and the ray extends both ways.
    //    Refusing on sign there rejects exactly half the viewport.
    //
    // ⚠️ This was invisible for the six axis-aligned orthographic views, because their ray origins are
    //    built from the focus and lie ON the sketch plane, giving a distance of zero. Only the isometric
    //    orthographic view tilts the frame enough to put the origin off the plane — and there, the whole
    //    upper-left of the viewport could not place a point.
    if (Perspective && Distance < 0.0)
        return false;

    Position = Added(RayOrigin, Scaled(RayDirection, Distance));
    return true;
}

double ResolveSnapTolerance(const ViewportStanding& View, bool Perspective)
{
    return Perspective ? std::max(View.Distance * 0.02, 2.0)
                       : std::max(10.0 / std::max(View.OrthoScale, 0.001), 0.25);
}

}   // namespace Slate
