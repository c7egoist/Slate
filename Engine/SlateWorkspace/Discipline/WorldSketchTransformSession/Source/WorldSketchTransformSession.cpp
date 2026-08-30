//============================================================================================================================================
//                                                  WORLDSKETCHTRANSFORMSESSION.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/WorldSketchTransformSession/Api/WorldSketchTransformSession.h"

#include "SlateShape/Sketch/SketchPolyline/Api/SketchPolyline.h"

#include <algorithm>
#include <cmath>

namespace Slate
{

namespace
{

bool SamePoint(const SpatialPoint& Left,
               const SpatialPoint& Right,
               double Tolerance = 1.0e-5)
{
    return LengthSquared(Difference(Left, Right)) <= Tolerance * Tolerance;
}

bool ResolveCameraRay(const ResolvedCamera& Camera,
                      const PlaneExtent& Extent,
                      float ScreenX,
                      float ScreenY,
                      SpatialPoint& RayOrigin,
                      SpatialDirection& RayDirection)
{
    const double CentreX = Extent.MinimumX + Extent.Width() * 0.5;
    const double CentreY = Extent.MinimumY + Extent.Height() * 0.5;
    const double NdcX = (static_cast<double>(ScreenX) - CentreX)
                      / std::max(static_cast<double>(Extent.Width()) * 0.5, 1.0);
    const double NdcY = (CentreY - static_cast<double>(ScreenY))
                      / std::max(static_cast<double>(Extent.Height()) * 0.5, 1.0);

    if (!Camera.Perspective)
    {
        const double Along = NdcX / std::max(Camera.OrthoScale, 0.001) * (Extent.Width() * 0.5);
        const double Upward = NdcY / std::max(Camera.OrthoScale, 0.001) * (Extent.Height() * 0.5);
        RayOrigin = Added(Camera.Frame.Eye,
                          Added(Scaled(Camera.Frame.Right, Along),
                                Scaled(Camera.Frame.Up, Upward)));
        RayDirection = Normalize(Camera.Frame.Forward);
        return true;
    }

    const double TanHalf = std::tan(Camera.FieldOfViewDegrees * 0.5 * ProjectionPi / 180.0);
    const double Aspect = Extent.Width() / std::max(Extent.Height(), 1.0f);
    RayOrigin = Camera.Frame.Eye;
    RayDirection = Normalize(Added(Added(Scaled(Camera.Frame.Right, NdcX * TanHalf * Aspect),
                                          Scaled(Camera.Frame.Up, NdcY * TanHalf)),
                                    Camera.Frame.Forward));
    return true;
}

bool ResolvePlaneIntersection(const SpatialPoint& PlaneOrigin,
                              const SpatialDirection& PlaneNormal,
                              const SpatialPoint& RayOrigin,
                              const SpatialDirection& RayDirection,
                              SpatialPoint& Position)
{
    const SpatialDirection Normal = Normalize(PlaneNormal);
    const double Denominator = Dot(Normal, RayDirection);
    if (std::fabs(Denominator) <= 1.0e-9)
        return false;

    const double Distance = Dot(Normal, Difference(RayOrigin, PlaneOrigin)) / Denominator;
    if (std::isnan(Distance) || std::isinf(Distance))
        return false;

    Position = Added(RayOrigin, Scaled(RayDirection, Distance));
    return true;
}

bool ResolveDragReference(const ResolvedCamera& Camera,
                          const PlaneExtent& Extent,
                          float ScreenX,
                          float ScreenY,
                          const SpatialPoint& Pivot,
                          const SpatialDirection& PlaneNormal,
                          SpatialPoint& Reference)
{
    SpatialPoint RayOrigin = {};
    SpatialDirection RayDirection = {};
    if (!ResolveCameraRay(Camera, Extent, ScreenX, ScreenY, RayOrigin, RayDirection))
        return false;
    return ResolvePlaneIntersection(Pivot, PlaneNormal, RayOrigin, RayDirection, Reference);
}

SpatialDirection ResolvePerpendicularComponent(const SpatialDirection& Subject,
                                               const SpatialDirection& Axis)
{
    const SpatialDirection UnitAxis = Normalize(Axis);
    return Added(Subject, Negated(Scaled(UnitAxis, Dot(Subject, UnitAxis))));
}

SpatialDirection ResolveAxisDirection(const WorldSketchTransformSession& Session)
{
    if (Session.Restriction() == TransformRestriction::AxisX)
        return { 1.0, 0.0, 0.0 };
    if (Session.Restriction() == TransformRestriction::AxisY)
        return { 0.0, 1.0, 0.0 };
    if (Session.Restriction() == TransformRestriction::AxisZ)
        return { 0.0, 0.0, 1.0 };
    return Normalize(Session.AxisDirection);
}

bool ResolveAxisReference(const ResolvedCamera& Camera,
                          const PlaneExtent& Extent,
                          float ScreenX,
                          float ScreenY,
                          const SpatialPoint& Pivot,
                          const SpatialDirection& AxisDirection,
                          SpatialPoint& Reference)
{
    SpatialDirection PlaneNormal = ResolvePerpendicularComponent(Camera.Frame.Forward, AxisDirection);
    if (LengthSquared(PlaneNormal) <= 1.0e-12)
        PlaneNormal = ResolvePerpendicularComponent(Camera.Frame.Up, AxisDirection);
    if (LengthSquared(PlaneNormal) <= 1.0e-12)
        PlaneNormal = ResolvePerpendicularComponent(Camera.Frame.Right, AxisDirection);
    if (LengthSquared(PlaneNormal) <= 1.0e-12)
        return false;

    return ResolveDragReference(Camera, Extent, ScreenX, ScreenY, Pivot, PlaneNormal, Reference);
}

bool ResolveCurrentControlPlacement(const WorldSketchStructure& Declared,
                                    WorldControlName Subject,
                                    WorldControlPlacement& Placement)
{
    if (!Subject.Assigned())
        return false;

    const std::uint32_t CurveIndex = Subject.IssuedIndex >> 12u;
    if (CurveIndex == 0u || CurveIndex > Declared.CurveCount())
        return false;

    std::vector<WorldControlPlacement> Controls;
    if (!ResolveWorldSketchControls(Declared, { CurveIndex }, Controls))
        return false;

    for (const WorldControlPlacement& Control : Controls)
        if (Control.Name.IssuedIndex == Subject.IssuedIndex)
        {
            Placement = Control;
            return true;
        }

    return false;
}

void CollectCoincidentPointPlacements(const WorldSketchStructure& Declared,
                                      const SpatialPoint& Anchor,
                                      std::vector<WorldPlacementSubject>& Placements)
{
    std::vector<WorldPointPlacement> Points;
    for (std::uint32_t CurveIndex = 1u; CurveIndex <= Declared.CurveCount(); ++CurveIndex)
    {
        if (!ResolveWorldSketchPoints(Declared, { CurveIndex }, Points))
            continue;

        for (const WorldPointPlacement& Point : Points)
            if (SamePoint(Point.Position, Anchor))
                AppendWorldPlacementUnique(Placements, { false, Point.Name, {}, Point.Position });
    }
}

void CollectConnectedCurvePlacements(const WorldSketchStructure& Declared,
                                     WorldCurveName Curve,
                                     std::vector<WorldPlacementSubject>& Placements)
{
    CollectWorldCurvePlacements(Declared, Curve, Placements);

    std::vector<WorldPointPlacement> Points;
    if (!ResolveWorldSketchPoints(Declared, Curve, Points))
        return;

    for (const WorldPointPlacement& Point : Points)
        CollectCoincidentPointPlacements(Declared, Point.Position, Placements);
}

bool ResolveCurvePolyline(const WorldSketchStructure& Declared,
                          WorldCurveName Curve,
                          std::vector<SpatialPoint>& Polyline)
{
    Polyline.clear();
    const DeclaredWorldCurve* Held = Declared.Resolve(Curve);
    if (Held == nullptr || !Held->Geometry.Declared())
        return false;

    AppendCurvePolyline(Held->Geometry, Polyline, 48u);
    return Polyline.size() >= 2u;
}

SpatialDirection ResolveWorldOffset(const WorldSketchTransformSession& Session,
                                    const SpatialPoint& Reference)
{
    if (Session.Restriction() == TransformRestriction::AxisX
     || Session.Restriction() == TransformRestriction::AxisY
     || Session.Restriction() == TransformRestriction::AxisZ
     || Session.Restriction() == TransformRestriction::Curve)
    {
        const SpatialDirection Axis = ResolveAxisDirection(Session);
        const double Started = Dot(Difference(Session.Pivot, Session.StartReference), Axis);
        const double Current = Dot(Difference(Session.Pivot, Reference), Axis);
        return Scaled(Axis, Current - Started);
    }

    return Difference(Session.StartReference, Reference);
}

} // namespace

bool ResolveWorldTransformPlacements(const WorldSketchStructure& Declared,
                                     const WorldPick& Target,
                                     SpatialPoint& Pivot,
                                     std::vector<WorldPlacementSubject>& Placements)
{
    Placements.clear();
    Pivot = {};

    if (Target.Subject == WorldPickSubject::Point)
    {
        SpatialPoint Position = {};
        if (!ResolveWorldSketchPointPosition(Declared, Target.Point, Position))
            return false;

        CollectCoincidentPointPlacements(Declared, Position, Placements);
        if (Placements.empty())
            Placements.push_back({ false, Target.Point, {}, Position });
        Pivot = Position;
        return !Placements.empty();
    }

    if (Target.Subject == WorldPickSubject::Control)
    {
        WorldControlPlacement Placement = {};
        if (!ResolveCurrentControlPlacement(Declared, Target.Control, Placement))
            return false;
        Placements.push_back({ true, {}, Target.Control, Placement.Position });
        Pivot = Placement.Position;
        return true;
    }

    if (Target.Subject == WorldPickSubject::Curve)
    {
        CollectConnectedCurvePlacements(Declared, Target.Curve, Placements);
        if (!ResolveWorldCurvePivot(Declared, Target.Curve, Pivot))
            return false;
        return !Placements.empty();
    }

    if (Target.Subject == WorldPickSubject::Loop)
    {
        CollectWorldLoopPlacements(Declared, Target.Loop, Placements);
        if (!ResolveWorldLoopPivot(Declared, Target.Loop, Pivot))
            return false;
        return !Placements.empty();
    }

    return false;
}

SpatialDirection ResolveWorldCurveSlideDirection(const WorldSketchStructure& Declared,
                                                 WorldCurveName Curve,
                                                 const SpatialPoint& NearPosition)
{
    std::vector<SpatialPoint> Polyline;
    if (!ResolveCurvePolyline(Declared, Curve, Polyline))
        return { 1.0, 0.0, 0.0 };

    double BestDistanceSquared = 1.0e30;
    SpatialDirection BestDirection = { 1.0, 0.0, 0.0 };
    for (std::size_t Index = 0u; Index + 1u < Polyline.size(); ++Index)
    {
        const SpatialPoint& StartPoint = Polyline[Index];
        const SpatialPoint& EndPoint = Polyline[Index + 1u];
        const SpatialDirection Segment = Difference(StartPoint, EndPoint);
        const double SegmentLengthSquared = LengthSquared(Segment);
        if (SegmentLengthSquared <= 1.0e-12)
            continue;

        const SpatialDirection Offset = Difference(StartPoint, NearPosition);
        const double Parameter = std::clamp(Dot(Offset, Segment) / SegmentLengthSquared, 0.0, 1.0);
        const SpatialPoint Closest = Added(StartPoint, Scaled(Segment, Parameter));
        const double CandidateDistanceSquared = LengthSquared(Difference(Closest, NearPosition));
        if (CandidateDistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = CandidateDistanceSquared;
            BestDirection = Normalize(Segment);
        }
    }

    return BestDirection;
}

void ApplyWorldTransformPlacements(WorldSketchStructure& Declared,
                                   const WorldSketchTransformSession& Session,
                                   const SpatialDirection& Offset)
{
    for (std::size_t Index = 0u; Index < Session.Placements.size() && Index < Session.Origins.size(); ++Index)
    {
        const WorldPlacementSubject& Placement = Session.Placements[Index];
        const SpatialPoint Position = Added(Session.Origins[Index], Offset);
        if (Placement.ControlPlacement)
            Discard(EnforceWorldSketchControl(Declared, Placement.Control, Position));
        else
            Discard(EnforceWorldSketchPoint(Declared, Placement.Point, Position));
    }
}

void RestoreWorldTransformPlacements(WorldSketchStructure& Declared,
                                     const WorldSketchTransformSession& Session)
{
    for (std::size_t Index = 0u; Index < Session.Placements.size() && Index < Session.Origins.size(); ++Index)
    {
        const WorldPlacementSubject& Placement = Session.Placements[Index];
        const SpatialPoint& Position = Session.Origins[Index];
        if (Placement.ControlPlacement)
            Discard(EnforceWorldSketchControl(Declared, Placement.Control, Position));
        else
            Discard(EnforceWorldSketchPoint(Declared, Placement.Point, Position));
    }
}

void ClearWorldSketchTransformSession(WorldSketchTransformSession& Session)
{
    Session.Engaged() = false;
    Session.AwaitingRelease = false;
    Session.Changed = false;
    Session.SlideAlongCurve() = false;
    Session.Restriction() = TransformRestriction::Free;
    Session.Target = {};
    Session.Placements.clear();
    Session.Origins.clear();
    Session.Pivot = {};
    Session.StartReference = {};
    Session.AxisDirection = { 1.0, 0.0, 0.0 };
    Session.PreviewValue = 0.0;
    Session.Standing.Numeric[0] = '\0';
}

bool StartWorldSketchTransformSession(const WorldSketchStructure& Declared,
                                     const ResolvedCamera& Camera,
                                     const PlaneExtent& Extent,
                                     float PointerX,
                                     float PointerY,
                                     const WorldPick& Target,
                                     TransformRestriction Restriction,
                                     bool SlideAlongCurve,
                                     WorldSketchTransformSession& Session,
                                     bool MouseDriven)
{
    SpatialPoint Pivot = {};
    std::vector<WorldPlacementSubject> Placements;
    if (!ResolveWorldTransformPlacements(Declared, Target, Pivot, Placements))
        return false;

    ClearWorldSketchTransformSession(Session);
    Session.Manner() = TransformManner::Move;
    Session.Engaged() = true;
    Session.AwaitingRelease = MouseDriven;
    Session.Restriction() = Restriction;
    Session.SlideAlongCurve() = SlideAlongCurve || Restriction == TransformRestriction::Curve;
    Session.Target = Target;
    Session.Pivot = Pivot;
    Session.Placements = Placements;
    Session.Origins.reserve(Placements.size());
    for (const WorldPlacementSubject& Placement : Placements)
        Session.Origins.push_back(Placement.Position);

    Session.AxisDirection = Session.SlideAlongCurve() && Target.Curve.Assigned()
                          ? ResolveWorldCurveSlideDirection(Declared, Target.Curve, Target.Position)
                          : ResolveAxisDirection(Session);

    const bool AxisDrag = Session.Restriction() == TransformRestriction::AxisX
                       || Session.Restriction() == TransformRestriction::AxisY
                       || Session.Restriction() == TransformRestriction::AxisZ
                       || Session.Restriction() == TransformRestriction::Curve;
    const bool Resolved = AxisDrag
                        ? ResolveAxisReference(Camera, Extent, PointerX, PointerY,
                                               Session.Pivot, Session.AxisDirection, Session.StartReference)
                        : ResolveDragReference(Camera, Extent, PointerX, PointerY,
                                               Session.Pivot, Camera.Frame.Forward, Session.StartReference);
    if (!Resolved)
        Session.StartReference = Session.Pivot;

    return true;
}

void UpdateWorldSketchTransformSession(const ResolvedCamera& Camera,
                                      const PlaneExtent& Extent,
                                      float PointerX,
                                      float PointerY,
                                      WorldSketchStructure& Declared,
                                      WorldSketchTransformSession& Session)
{
    if (!Session.Engaged())
        return;

    SpatialPoint Reference = Session.StartReference;
    const bool AxisDrag = Session.Restriction() == TransformRestriction::AxisX
                       || Session.Restriction() == TransformRestriction::AxisY
                       || Session.Restriction() == TransformRestriction::AxisZ
                       || Session.Restriction() == TransformRestriction::Curve;
    const bool Resolved = AxisDrag
                        ? ResolveAxisReference(Camera, Extent, PointerX, PointerY,
                                               Session.Pivot, ResolveAxisDirection(Session), Reference)
                        : ResolveDragReference(Camera, Extent, PointerX, PointerY,
                                               Session.Pivot, Camera.Frame.Forward, Reference);
    if (!Resolved)
        return;

    SpatialDirection Offset = ResolveWorldOffset(Session, Reference);

    double Numeric = 0.0;
    if (ResolveNumericOverride(Session.Standing, Numeric))
    {
        if (AxisDrag)
            Offset = Scaled(ResolveAxisDirection(Session), Numeric);
        else
            Offset = { Numeric, 0.0, 0.0 };
    }

    RestoreWorldTransformPlacements(Declared, Session);
    ApplyWorldTransformPlacements(Declared, Session, Offset);

    Session.PreviewValue = AxisDrag ? Dot(Offset, ResolveAxisDirection(Session))
                                    : std::sqrt(LengthSquared(Offset));
    if (LengthSquared(Offset) > 1.0e-18)
        Session.Changed = true;
}

void CommitWorldSketchTransformSession(WorldSketchTransformSession& Session)
{
    ClearWorldSketchTransformSession(Session);
}

void CancelWorldSketchTransformSession(WorldSketchStructure& Declared,
                                      WorldSketchTransformSession& Session)
{
    RestoreWorldTransformPlacements(Declared, Session);
    ClearWorldSketchTransformSession(Session);
}

} // namespace Slate
