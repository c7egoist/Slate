//============================================================================================================================================
//                                                        TRANSFORMGIZMO.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/TransformGizmo/Api/TransformGizmo.h"

#include <cmath>

namespace Slate
{

namespace
{

constexpr double GizmoPi = 3.14159265358979323846;

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

double DistanceToArcBarSquared(float PX,
                               float PY,
                               float OriginX,
                               float OriginY,
                               float UX,
                               float UY,
                               float VX,
                               float VY)
{
    const double Start = GizmoPi * 0.25 - GizmoMeasure::RotateSweepRadians * 0.5;
    double Best = 1.0e30;
    for (std::uint32_t Segment = 0u; Segment < GizmoMeasure::RotateSegments; ++Segment)
    {
        const double A0 = Start + GizmoMeasure::RotateSweepRadians
                                * static_cast<double>(Segment)
                                / static_cast<double>(GizmoMeasure::RotateSegments);
        const double A1 = Start + GizmoMeasure::RotateSweepRadians
                                * static_cast<double>(Segment + 1u)
                                / static_cast<double>(GizmoMeasure::RotateSegments);

        const float X0 = OriginX
                       + (UX * static_cast<float>(std::cos(A0)) + VX * static_cast<float>(std::sin(A0)))
                         * static_cast<float>(GizmoMeasure::RotateRadius);
        const float Y0 = OriginY
                       + (UY * static_cast<float>(std::cos(A0)) + VY * static_cast<float>(std::sin(A0)))
                         * static_cast<float>(GizmoMeasure::RotateRadius);
        const float X1 = OriginX
                       + (UX * static_cast<float>(std::cos(A1)) + VX * static_cast<float>(std::sin(A1)))
                         * static_cast<float>(GizmoMeasure::RotateRadius);
        const float Y1 = OriginY
                       + (UY * static_cast<float>(std::cos(A1)) + VY * static_cast<float>(std::sin(A1)))
                         * static_cast<float>(GizmoMeasure::RotateRadius);
        const double Candidate = DistanceToSegmentSquared(PX, PY, X0, Y0, X1, Y1);
        if (Candidate < Best)
            Best = Candidate;
    }
    return Best;
}

}   // namespace

bool ResolveGizmoScreenBasis(const SpatialBasis& Basis,
                             const ViewportStanding& View,
                             bool Perspective,
                             const PlaneExtent& Extent,
                             const SpatialPoint& Pivot,
                             GizmoScreenBasis& Resolved)
{
    Resolved = {};
    if (!ProjectSpatialPoint(Basis, View, Perspective, Extent, Pivot, Resolved.PivotX, Resolved.PivotY))
        return false;

    // 🔴 THE PROBE IS BOTH A DIRECTION AND A RULER. Projecting a known world offset and measuring how
    //    many pixels it covered gives the world-per-pixel conversion directly, with no assumption about
    //    which projection is in use or how it is parameterised.
    //
    // ⚠️ IT MUST BE MEASURED OVER THE SPAN IT WILL BE USED FOR. Perspective is not linear, so a ruler read
    //    over a long offset under-reports the rate near the pivot. So: read a rough ruler, work out how
    //    far the gizmo actually reaches in world units, and read it again over exactly that.
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

        // 🔴 TAKE THE SMALLEST WORLD-PER-PIXEL OF THE VISIBLE AXES, WHICH IS THE ONE THAT PROJECTS LONGEST.
        //    That guarantees none of the HTML-sized handles overshoots its own screen footprint.
        const double Candidate = ProbeWorld / Length;
        if (!Measured || Candidate < Resolved.WorldPerPixel)
        {
            Resolved.WorldPerPixel = Candidate;
            Measured = true;
        }
    };

    Probe(Basis.Along, Resolved.AlongX, Resolved.AlongY);
    Probe(Basis.Across, Resolved.AcrossX, Resolved.AcrossY);
    Probe(Basis.Normal, Resolved.NormalX, Resolved.NormalY);

    if (Measured)
    {
        ProbeWorld = Resolved.WorldPerPixel * GizmoMeasure::AxisEnd;
        if (ProbeWorld > 1.0e-6)
        {
            Measured = false;
            Probe(Basis.Along, Resolved.AlongX, Resolved.AlongY);
            Probe(Basis.Across, Resolved.AcrossX, Resolved.AcrossY);
            Probe(Basis.Normal, Resolved.NormalX, Resolved.NormalY);
        }
    }

    if (!Measured)
        Resolved.WorldPerPixel = 1.0;

    return true;
}

bool ResolveGizmoScreenBasis(const ResolvedCamera& Camera,
                             const PlaneExtent& Extent,
                             const SpatialPoint& Pivot,
                             GizmoScreenBasis& Resolved)
{
    Resolved = {};
    if (!ProjectFromCamera(Camera, Extent, Pivot, Resolved.PivotX, Resolved.PivotY))
        return false;

    double ProbeWorld = 24.0;
    bool Measured = false;

    const auto Probe = [&](const SpatialDirection& Direction, float& DirX, float& DirY)
    {
        float X = 0.0f;
        float Y = 0.0f;
        if (!ProjectFromCamera(Camera, Extent, Added(Pivot, Scaled(Direction, ProbeWorld)), X, Y))
            return;

        const double DX = static_cast<double>(X) - Resolved.PivotX;
        const double DY = static_cast<double>(Y) - Resolved.PivotY;
        const double Length = std::sqrt(DX * DX + DY * DY);
        if (Length <= 1.0e-4)
            return;

        DirX = static_cast<float>(DX / Length);
        DirY = static_cast<float>(DY / Length);

        const double Candidate = ProbeWorld / Length;
        if (!Measured || Candidate < Resolved.WorldPerPixel)
        {
            Resolved.WorldPerPixel = Candidate;
            Measured = true;
        }
    };

    Probe(Camera.Basis.Along, Resolved.AlongX, Resolved.AlongY);
    Probe(Camera.Basis.Across, Resolved.AcrossX, Resolved.AcrossY);
    Probe(Camera.Basis.Normal, Resolved.NormalX, Resolved.NormalY);

    if (Measured)
    {
        ProbeWorld = Resolved.WorldPerPixel * GizmoMeasure::AxisEnd;
        if (ProbeWorld > 1.0e-6)
        {
            Measured = false;
            Probe(Camera.Basis.Along, Resolved.AlongX, Resolved.AlongY);
            Probe(Camera.Basis.Across, Resolved.AcrossX, Resolved.AcrossY);
            Probe(Camera.Basis.Normal, Resolved.NormalX, Resolved.NormalY);
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
    const double CentreDistanceSquared = DistanceSquared(PointerX, PointerY, Screen.PivotX, Screen.PivotY);

    const auto ConeDistanceSquared = [&](float DirX, float DirY)
    {
        const float BaseX = Screen.PivotX + DirX * static_cast<float>(GizmoMeasure::AxisEnd - GizmoMeasure::ConeLength);
        const float BaseY = Screen.PivotY + DirY * static_cast<float>(GizmoMeasure::AxisEnd - GizmoMeasure::ConeLength);
        const float TipX  = Screen.PivotX + DirX * static_cast<float>(GizmoMeasure::AxisEnd);
        const float TipY  = Screen.PivotY + DirY * static_cast<float>(GizmoMeasure::AxisEnd);
        return DistanceToSegmentSquared(PointerX, PointerY, BaseX, BaseY, TipX, TipY);
    };

    const auto CylinderDistanceSquared = [&](float DirX, float DirY)
    {
        const float StartX = Screen.PivotX + DirX * static_cast<float>(GizmoMeasure::ScaleCentre - GizmoMeasure::ScaleLength * 0.5);
        const float StartY = Screen.PivotY + DirY * static_cast<float>(GizmoMeasure::ScaleCentre - GizmoMeasure::ScaleLength * 0.5);
        const float EndX   = Screen.PivotX + DirX * static_cast<float>(GizmoMeasure::ScaleCentre + GizmoMeasure::ScaleLength * 0.5);
        const float EndY   = Screen.PivotY + DirY * static_cast<float>(GizmoMeasure::ScaleCentre + GizmoMeasure::ScaleLength * 0.5);
        return DistanceToSegmentSquared(PointerX, PointerY, StartX, StartY, EndX, EndY);
    };

    if (Manner == TransformManner::Move)
    {
        // 📝 The sketch's free-move plane is the HTML gizmo's XZ square — the square normal to +Y.
        const float PlaneX = Screen.PivotX + (Screen.AlongX + Screen.AcrossX) * static_cast<float>(GizmoMeasure::PlaneCentre);
        const float PlaneY = Screen.PivotY + (Screen.AlongY + Screen.AcrossY) * static_cast<float>(GizmoMeasure::PlaneCentre);
        const double LocalAlong  = (PointerX - PlaneX) * Screen.AlongX  + (PointerY - PlaneY) * Screen.AlongY;
        const double LocalAcross = (PointerX - PlaneX) * Screen.AcrossX + (PointerY - PlaneY) * Screen.AcrossY;
        if (std::fabs(LocalAlong) <= GizmoMeasure::PlaneHalf && std::fabs(LocalAcross) <= GizmoMeasure::PlaneHalf)
            return GizmoHandle::MoveFree;

        if (ConeDistanceSquared(Screen.AlongX, Screen.AlongY) <= GizmoMeasure::MoveGrab * GizmoMeasure::MoveGrab)
            return GizmoHandle::MoveX;
        if (ConeDistanceSquared(Screen.AcrossX, Screen.AcrossY) <= GizmoMeasure::MoveGrab * GizmoMeasure::MoveGrab)
            return GizmoHandle::MoveZ;

        if (CentreDistanceSquared <= GizmoMeasure::CentreGrab * GizmoMeasure::CentreGrab * 0.25)
            return GizmoHandle::MoveFree;
    }
    else if (Manner == TransformManner::Rotate)
    {
        const double ReachSquared = GizmoMeasure::RotateGrab * GizmoMeasure::RotateGrab;
        if (DistanceToArcBarSquared(PointerX, PointerY,
                                    Screen.PivotX, Screen.PivotY,
                                    Screen.AcrossX, Screen.AcrossY,
                                    Screen.NormalX, Screen.NormalY) <= ReachSquared)
            return GizmoHandle::Rotate;
        if (DistanceToArcBarSquared(PointerX, PointerY,
                                    Screen.PivotX, Screen.PivotY,
                                    Screen.AlongX, Screen.AlongY,
                                    Screen.AcrossX, Screen.AcrossY) <= ReachSquared)
            return GizmoHandle::Rotate;
        if (DistanceToArcBarSquared(PointerX, PointerY,
                                    Screen.PivotX, Screen.PivotY,
                                    Screen.AlongX, Screen.AlongY,
                                    Screen.NormalX, Screen.NormalY) <= ReachSquared)
            return GizmoHandle::Rotate;

        if (CentreDistanceSquared <= GizmoMeasure::CentreGrab * GizmoMeasure::CentreGrab * 0.25)
            return GizmoHandle::Rotate;
    }
    else
    {
        if (CylinderDistanceSquared(Screen.AlongX, Screen.AlongY) <= GizmoMeasure::ScaleGrab * GizmoMeasure::ScaleGrab)
            return GizmoHandle::ScaleX;
        if (CylinderDistanceSquared(Screen.AcrossX, Screen.AcrossY) <= GizmoMeasure::ScaleGrab * GizmoMeasure::ScaleGrab)
            return GizmoHandle::ScaleZ;
        if (CentreDistanceSquared <= GizmoMeasure::CentreGrab * GizmoMeasure::CentreGrab * 0.25)
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
