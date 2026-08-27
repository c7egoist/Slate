//============================================================================================================================================
//                                                        TRANSFORMGIZMO.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/TransformGizmo/Api/TransformGizmo.h"

#include <cmath>

namespace Slate
{

namespace
{

double DistanceSquared(float X0, float Y0, float X1, float Y1)
{
    const double DX = static_cast<double>(X1) - static_cast<double>(X0);
    const double DY = static_cast<double>(Y1) - static_cast<double>(Y0);
    return DX * DX + DY * DY;
}

/// 🧩 How far a point lies from a segment, squared.
/// note 📝 Squared throughout so no square root is taken to answer a comparison. Every caller compares
///       against a squared reach.
double DistanceToSegmentSquared(float PX, float PY, float AX, float AY, float BX, float BY)
{
    const double DX = static_cast<double>(BX) - static_cast<double>(AX);
    const double DY = static_cast<double>(BY) - static_cast<double>(AY);
    const double LengthSquared = DX * DX + DY * DY;
    if (LengthSquared <= 1.0e-9)
        return DistanceSquared(PX, PY, AX, AY);

    double Parameter = ((static_cast<double>(PX) - AX) * DX + (static_cast<double>(PY) - AY) * DY) / LengthSquared;
    Parameter = Parameter < 0.0 ? 0.0 : (Parameter > 1.0 ? 1.0 : Parameter);

    const double ClosestX = AX + DX * Parameter;
    const double ClosestY = AY + DY * Parameter;
    const double OffsetX = static_cast<double>(PX) - ClosestX;
    const double OffsetY = static_cast<double>(PY) - ClosestY;
    return OffsetX * OffsetX + OffsetY * OffsetY;
}

}   // namespace

bool ResolveGizmoScreenBasis(const SpatialBasis& Basis,
                             const ViewportStanding& View,
                             bool Perspective,
                             const PlaneExtent& Extent,
                             const SpatialPoint& Pivot,
                             GizmoScreenBasis& Resolved)
{
    if (!ProjectSpatialPoint(Basis, View, Perspective, Extent, Pivot, Resolved.PivotX, Resolved.PivotY))
        return false;

    // 🔴 THE PROBE IS BOTH A DIRECTION AND A RULER. Projecting a known world offset and measuring how
    //    many pixels it covered gives the world-per-pixel conversion directly, with no assumption about
    //    which projection is in use or how it is parameterised.
    //
    // ⚠️ IT MUST BE MEASURED OVER THE SPAN IT WILL BE USED FOR. Perspective is not linear, so a ruler read
    //    over a long offset under-reports the rate near the pivot — measuring over a fixed 24 units and
    //    then drawing a 44-pixel arrow gave an arrow 79% of the length asked for at close camera range.
    //    So: read a rough ruler, work out how far the gizmo actually reaches in world units, and read it
    //    again over exactly that. Two passes converge; a third changes nothing an artist could see.
    double ProbeWorld = 24.0;
    bool Measured = false;

    const auto Probe = [&](const SpatialDirection& Direction, float& DirX, float& DirY)
    {
        float X = 0.0f;
        float Y = 0.0f;
        if (!ProjectSpatialPoint(Basis, View, Perspective, Extent,
                               Added(Pivot, Scaled(Direction, ProbeWorld)), X, Y))
            return;

        const double DX = static_cast<double>(X) - Resolved.PivotX;
        const double DY = static_cast<double>(Y) - Resolved.PivotY;
        const double Length = std::sqrt(DX * DX + DY * DY);
        if (Length <= 1.0e-4)
            return;

        DirX = static_cast<float>(DX / Length);
        DirY = static_cast<float>(DY / Length);

        // 🔴 TAKE THE SMALLEST WORLD-PER-PIXEL OF THE TWO AXES, WHICH IS THE ONE THAT PROJECTS LONGEST.
        //    Under an oblique view the two axes foreshorten by different amounts — isometric spreads them
        //    by about 13% — and one factor has to serve both. Sizing by the axis that projects longest
        //    means the longer arm lands on the table's length and the shorter one comes in slightly under
        //    it. Sizing by the other axis overshoots instead, and an arm longer than the table says is an
        //    arm whose tip is outside its own hit box: the artist points at the arrowhead and grabs
        //    nothing. Under-reaching is invisible; over-reaching is a broken control.
        const double Candidate = ProbeWorld / Length;
        if (!Measured || Candidate < Resolved.WorldPerPixel)
        {
            Resolved.WorldPerPixel = Candidate;
            Measured = true;
        }
    };

    Probe(Basis.Along, Resolved.AlongX, Resolved.AlongY);
    Probe(Basis.Across, Resolved.AcrossX, Resolved.AcrossY);

    if (Measured)
    {
        ProbeWorld = Resolved.WorldPerPixel * GizmoMeasure::ShaftEnd;
        if (ProbeWorld > 1.0e-6)
        {
            Measured = false;
            Probe(Basis.Along, Resolved.AlongX, Resolved.AlongY);
            Probe(Basis.Across, Resolved.AcrossX, Resolved.AcrossY);
        }
    }

    if (!Measured)
        Resolved.WorldPerPixel = 1.0;

    return true;
}

GizmoHandle ResolveGizmoHandle(const GizmoScreenBasis& Screen,
                               TransformManner Manner,
                               float PointerX,
                               float PointerY)
{
    const float AlongStartX  = Screen.PivotX + Screen.AlongX  * static_cast<float>(GizmoMeasure::ShaftStart);
    const float AlongStartY  = Screen.PivotY + Screen.AlongY  * static_cast<float>(GizmoMeasure::ShaftStart);
    const float AcrossStartX = Screen.PivotX + Screen.AcrossX * static_cast<float>(GizmoMeasure::ShaftStart);
    const float AcrossStartY = Screen.PivotY + Screen.AcrossY * static_cast<float>(GizmoMeasure::ShaftStart);

    const float AlongEndX  = Screen.PivotX + Screen.AlongX  * static_cast<float>(GizmoMeasure::ShaftEnd);
    const float AlongEndY  = Screen.PivotY + Screen.AlongY  * static_cast<float>(GizmoMeasure::ShaftEnd);
    const float AcrossEndX = Screen.PivotX + Screen.AcrossX * static_cast<float>(GizmoMeasure::ShaftEnd);
    const float AcrossEndY = Screen.PivotY + Screen.AcrossY * static_cast<float>(GizmoMeasure::ShaftEnd);

    const double CentreDistanceSquared = DistanceSquared(PointerX, PointerY, Screen.PivotX, Screen.PivotY);

    if (Manner == TransformManner::Move)
    {
        // The free-move square, offset diagonally so it does not sit on either arrow.
        const float PlaneX = Screen.PivotX + (Screen.AlongX + Screen.AcrossX) * static_cast<float>(GizmoMeasure::PlaneOffset);
        const float PlaneY = Screen.PivotY + (Screen.AlongY + Screen.AcrossY) * static_cast<float>(GizmoMeasure::PlaneOffset);
        const double LocalAlong  = (PointerX - PlaneX) * Screen.AlongX  + (PointerY - PlaneY) * Screen.AlongY;
        const double LocalAcross = (PointerX - PlaneX) * Screen.AcrossX + (PointerY - PlaneY) * Screen.AcrossY;
        if (std::fabs(LocalAlong) <= GizmoMeasure::PlaneHalf && std::fabs(LocalAcross) <= GizmoMeasure::PlaneHalf)
            return GizmoHandle::MoveFree;

        // 🔴 From ShaftStart, not from the pivot: the segment tested is the segment DRAWN. Testing from
        //    the pivot puts the arrow's reach over the nub and makes the nub unreachable.
        if (DistanceToSegmentSquared(PointerX, PointerY, AlongStartX, AlongStartY, AlongEndX, AlongEndY)
            <= GizmoMeasure::ShaftGrab * GizmoMeasure::ShaftGrab)
            return GizmoHandle::MoveX;
        if (DistanceToSegmentSquared(PointerX, PointerY, AcrossStartX, AcrossStartY, AcrossEndX, AcrossEndY)
            <= GizmoMeasure::ShaftGrab * GizmoMeasure::ShaftGrab)
            return GizmoHandle::MoveZ;

        if (CentreDistanceSquared <= GizmoMeasure::CentreGrab * GizmoMeasure::CentreGrab)
            return GizmoHandle::MoveFree;
    }
    else if (Manner == TransformManner::Rotate)
    {
        // 📝 A band around the ring rather than a disc. The inside of the ring belongs to whatever is
        //    drawn there, and a rotation is dragged on the rim.
        const double Distance = std::sqrt(CentreDistanceSquared);
        if (Distance >= GizmoMeasure::RingRadius - GizmoMeasure::RingGrab
         && Distance <= GizmoMeasure::RingRadius + GizmoMeasure::RingGrab)
            return GizmoHandle::Rotate;
        if (CentreDistanceSquared <= GizmoMeasure::CentreGrab * GizmoMeasure::CentreGrab)
            return GizmoHandle::Rotate;
    }
    else
    {
        const float AlongBoxX  = Screen.PivotX + Screen.AlongX  * static_cast<float>(GizmoMeasure::ScaleBox);
        const float AlongBoxY  = Screen.PivotY + Screen.AlongY  * static_cast<float>(GizmoMeasure::ScaleBox);
        const float AcrossBoxX = Screen.PivotX + Screen.AcrossX * static_cast<float>(GizmoMeasure::ScaleBox);
        const float AcrossBoxY = Screen.PivotY + Screen.AcrossY * static_cast<float>(GizmoMeasure::ScaleBox);

        if (DistanceSquared(PointerX, PointerY, AlongBoxX, AlongBoxY)
            <= GizmoMeasure::ScaleGrab * GizmoMeasure::ScaleGrab)
            return GizmoHandle::ScaleX;
        if (DistanceSquared(PointerX, PointerY, AcrossBoxX, AcrossBoxY)
            <= GizmoMeasure::ScaleGrab * GizmoMeasure::ScaleGrab)
            return GizmoHandle::ScaleZ;
        if (CentreDistanceSquared <= GizmoMeasure::CentreGrab * GizmoMeasure::CentreGrab)
            return GizmoHandle::ScaleFree;
    }

    return GizmoHandle::None;
}

TransformManner ResolveHandleManner(GizmoHandle Handle)
{
    switch (Handle)
    {
        case GizmoHandle::MoveFree:
        case GizmoHandle::MoveX:
        case GizmoHandle::MoveZ:      return TransformManner::Move;
        case GizmoHandle::Rotate:     return TransformManner::Rotate;
        case GizmoHandle::ScaleFree:
        case GizmoHandle::ScaleX:
        case GizmoHandle::ScaleZ:     return TransformManner::Scale;
        case GizmoHandle::None:       return TransformManner::Move;
    }
    return TransformManner::Move;
}

TransformRestriction ResolveHandleRestriction(GizmoHandle Handle)
{
    switch (Handle)
    {
        case GizmoHandle::MoveX:
        case GizmoHandle::ScaleX:     return TransformRestriction::AxisX;
        case GizmoHandle::MoveZ:
        case GizmoHandle::ScaleZ:     return TransformRestriction::AxisZ;
        case GizmoHandle::Rotate:     return TransformRestriction::Screen;
        case GizmoHandle::MoveFree:
        case GizmoHandle::ScaleFree:
        case GizmoHandle::None:       return TransformRestriction::Free;
    }
    return TransformRestriction::Free;
}

}   // namespace Slate
