//============================================================================================================================================
//                                                   WORLDDRAFTSTRUCTURE.CPP
//============================================================================================================================================

#include "SlateShape/World/WorldDraftStructure/Api/WorldDraftStructure.h"

#include <cmath>

namespace Slate
{

namespace
{
    SpatialDirection ResolveFrameAlong(const WorldPlacementFrame& Frame)
    {
        return Normalize(Frame.AlongDirection);
    }

    SpatialDirection ResolveFrameNormal(const WorldPlacementFrame& Frame)
    {
        return Normalize(Frame.Normal);
    }

    SpatialDirection ResolveFrameAcross(const WorldPlacementFrame& Frame)
    {
        return Normalize(Cross(ResolveFrameNormal(Frame), ResolveFrameAlong(Frame)));
    }
}

bool WorldPlacementFrame::Declared() const
{
    return LengthSquared(Normal) > 0.0
        && LengthSquared(AlongDirection) > 0.0
        && LengthSquared(Cross(Normal, AlongDirection)) > 0.0;
}

WorldCurveName WorldDraftStructure::DeclareCurve(const CurveSpecification& Incoming)
{
    HeldCurves.push_back({ Incoming, {}, false });
    return { static_cast<std::uint32_t>(HeldCurves.size()) };
}

WorldCurveName WorldDraftStructure::DeclareCurve(const CurveSpecification& Incoming,
                                                 const WorldPlacementFrame& SupportFrame)
{
    HeldCurves.push_back({ Incoming, SupportFrame, SupportFrame.Declared() });
    return { static_cast<std::uint32_t>(HeldCurves.size()) };
}

WorldLoopName WorldDraftStructure::DeclareLoop(const DeclaredWorldLoop& Incoming)
{
    HeldLoops.push_back(Incoming);
    return { static_cast<std::uint32_t>(HeldLoops.size()) };
}

bool WorldDraftStructure::DeclareCurveSupportFrame(WorldCurveName Subject,
                                                   const WorldPlacementFrame& SupportFrame)
{
    DeclaredWorldCurve* Held = Resolve(Subject);
    if (Held == nullptr || !SupportFrame.Declared())
        return false;
    Held->SupportFrame = SupportFrame;
    Held->SupportFrameStanding = true;
    return true;
}

WorldCurveName WorldDraftStructure::DeclareLine(const SpatialPoint& Origin,
                                                const SpatialPoint& Terminus)
{
    return DeclareCurve(CurveSpecification::DeclareLine(Origin, Terminus));
}

WorldCurveName WorldDraftStructure::DeclareLine(const SpatialPoint& Origin,
                                                const SpatialPoint& Terminus,
                                                const WorldPlacementFrame& SupportFrame)
{
    return DeclareCurve(CurveSpecification::DeclareLine(Origin, Terminus), SupportFrame);
}

WorldCurveName WorldDraftStructure::DeclareThreePointArc(const SpatialPoint& StartPoint,
                                                         const SpatialPoint& ThroughPoint,
                                                         const SpatialPoint& EndPoint)
{
    return DeclareCurve(CurveSpecification::DeclareThreePointArc(StartPoint, ThroughPoint, EndPoint));
}

WorldCurveName WorldDraftStructure::DeclareThreePointArc(const SpatialPoint& StartPoint,
                                                         const SpatialPoint& ThroughPoint,
                                                         const SpatialPoint& EndPoint,
                                                         const WorldPlacementFrame& SupportFrame)
{
    return DeclareCurve(CurveSpecification::DeclareThreePointArc(StartPoint, ThroughPoint, EndPoint),
                        SupportFrame);
}

WorldCurveName WorldDraftStructure::DeclareCircle(const CircleCurve& Declared)
{
    return DeclareCurve(CurveSpecification::DeclareCircle(Declared));
}

WorldCurveName WorldDraftStructure::DeclareCircle(const CircleCurve& Declared,
                                                  const WorldPlacementFrame& SupportFrame)
{
    return DeclareCurve(CurveSpecification::DeclareCircle(Declared), SupportFrame);
}

WorldCurveName WorldDraftStructure::DeclareEllipse(const EllipseCurve& Declared)
{
    return DeclareCurve(CurveSpecification::DeclareEllipse(Declared));
}

WorldCurveName WorldDraftStructure::DeclareEllipse(const EllipseCurve& Declared,
                                                   const WorldPlacementFrame& SupportFrame)
{
    return DeclareCurve(CurveSpecification::DeclareEllipse(Declared), SupportFrame);
}

WorldCurveName WorldDraftStructure::DeclareBezier(const std::vector<SpatialPoint>& ControlPoints)
{
    return DeclareCurve(CurveSpecification::DeclareBezier(ControlPoints, { 0.0, 1.0 }));
}

WorldCurveName WorldDraftStructure::DeclareBezier(const std::vector<SpatialPoint>& ControlPoints,
                                                  const WorldPlacementFrame& SupportFrame)
{
    return DeclareCurve(CurveSpecification::DeclareBezier(ControlPoints, { 0.0, 1.0 }), SupportFrame);
}

WorldCurveName WorldDraftStructure::DeclareBasisSpline(const BasisSplineCurve& Declared)
{
    return DeclareCurve(CurveSpecification::DeclareBasisSpline(Declared, { 0.0, 1.0 }));
}

WorldCurveName WorldDraftStructure::DeclareBasisSpline(const BasisSplineCurve& Declared,
                                                       const WorldPlacementFrame& SupportFrame)
{
    return DeclareCurve(CurveSpecification::DeclareBasisSpline(Declared, { 0.0, 1.0 }), SupportFrame);
}

WorldCurveName WorldDraftStructure::DeclareRationalSpline(const RationalSplineCurve& Declared)
{
    return DeclareCurve(CurveSpecification::DeclareRationalSpline(Declared, { 0.0, 1.0 }));
}

WorldCurveName WorldDraftStructure::DeclareRationalSpline(const RationalSplineCurve& Declared,
                                                          const WorldPlacementFrame& SupportFrame)
{
    return DeclareCurve(CurveSpecification::DeclareRationalSpline(Declared, { 0.0, 1.0 }), SupportFrame);
}

WorldCurveName WorldDraftStructure::DeclareHermite(const HermiteCurve& Declared)
{
    return DeclareCurve(CurveSpecification::DeclareHermite(Declared, { 0.0, 1.0 }));
}

WorldCurveName WorldDraftStructure::DeclareHermite(const HermiteCurve& Declared,
                                                   const WorldPlacementFrame& SupportFrame)
{
    return DeclareCurve(CurveSpecification::DeclareHermite(Declared, { 0.0, 1.0 }), SupportFrame);
}

const DeclaredWorldCurve* WorldDraftStructure::Resolve(WorldCurveName Subject) const
{
    if (!Subject.Assigned() || Subject.IssuedIndex > HeldCurves.size())
        return nullptr;
    return &HeldCurves[Subject.IssuedIndex - 1u];
}

DeclaredWorldCurve* WorldDraftStructure::Resolve(WorldCurveName Subject)
{
    if (!Subject.Assigned() || Subject.IssuedIndex > HeldCurves.size())
        return nullptr;
    return &HeldCurves[Subject.IssuedIndex - 1u];
}

const DeclaredWorldLoop* WorldDraftStructure::Resolve(WorldLoopName Subject) const
{
    if (!Subject.Assigned() || Subject.IssuedIndex > HeldLoops.size())
        return nullptr;
    return &HeldLoops[Subject.IssuedIndex - 1u];
}

DeclaredWorldLoop* WorldDraftStructure::Resolve(WorldLoopName Subject)
{
    if (!Subject.Assigned() || Subject.IssuedIndex > HeldLoops.size())
        return nullptr;
    return &HeldLoops[Subject.IssuedIndex - 1u];
}

void WorldDraftStructure::ResolveCurves(std::vector<CurveSpecification>& Delivered) const
{
    Delivered.clear();
    Delivered.reserve(HeldCurves.size());
    for (const DeclaredWorldCurve& Curve : HeldCurves)
        Delivered.push_back(Curve.Geometry);
}

bool WorldDraftStructure::Declared() const
{
    for (const DeclaredWorldCurve& Curve : HeldCurves)
    {
        if (!Curve.Geometry.Declared())
            return false;
        if (Curve.SupportFrameStanding && !Curve.SupportFrame.Declared())
            return false;
    }

    for (const DeclaredWorldLoop& Loop : HeldLoops)
    {
        if (Loop.Traversal.empty())
            return false;
        for (const WorldCurveUse& Use : Loop.Traversal)
            if (!Use.TraversedCurve.Assigned() || Use.TraversedCurve.IssuedIndex > HeldCurves.size())
                return false;
    }

    return true;
}

void WorldDraftStructure::Reclaim()
{
    HeldCurves.clear();
    HeldLoops.clear();
}

void ResolveWorldPlacementCoordinates(const WorldPlacementFrame& Frame,
                                      const SpatialPoint& Position,
                                      double& Along,
                                      double& Across)
{
    const SpatialDirection Offset = Difference(Frame.Origin, Position);
    const SpatialDirection FrameAlong = ResolveFrameAlong(Frame);
    const SpatialDirection FrameAcross = ResolveFrameAcross(Frame);
    Along = Dot(Offset, FrameAlong);
    Across = Dot(Offset, FrameAcross);
}

SpatialPoint ResolveWorldPlacementPosition(const WorldPlacementFrame& Frame,
                                           double Along,
                                           double Across)
{
    return Added(Frame.Origin,
                 Added(Scaled(ResolveFrameAlong(Frame), Along),
                       Scaled(ResolveFrameAcross(Frame), Across)));
}

double ResolveWorldPlacementOffset(const WorldPlacementFrame& Frame,
                                   const SpatialPoint& Position)
{
    return Dot(ResolveFrameNormal(Frame), Difference(Frame.Origin, Position));
}

SpatialPoint ResolveWorldPlacementProjection(const WorldPlacementFrame& Frame,
                                             const SpatialPoint& Position)
{
    return Added(Position, Scaled(ResolveFrameNormal(Frame), -ResolveWorldPlacementOffset(Frame, Position)));
}

bool ResolveWorldPlacementIntersection(const WorldPlacementFrame& Frame,
                                       const SpatialPoint& RayOrigin,
                                       const SpatialDirection& RayDirection,
                                       SpatialPoint& Delivered)
{
    if (!Frame.Declared())
        return false;

    const SpatialDirection Normal = ResolveFrameNormal(Frame);
    const double Denominator = Dot(Normal, RayDirection);
    if (std::fabs(Denominator) <= 1.0e-12)
        return false;

    const double Distance = Dot(Normal, Difference(RayOrigin, Frame.Origin)) / Denominator;
    Delivered = Added(RayOrigin, Scaled(RayDirection, Distance));
    return true;
}

} // namespace Slate
