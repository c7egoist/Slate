//============================================================================================================================================
//                                                    WORLDDRAFTPICKING.CPP
//============================================================================================================================================

#include "SlateShape/World/WorldDraftPicking/Api/WorldDraftPicking.h"

#include "SlateShape/Sketch/SketchPolyline/Api/SketchPolyline.h"
#include "SlateShape/World/WorldDraftAnalysis/Api/WorldDraftAnalysis.h"

#include <algorithm>
#include <cmath>

namespace Slate
{

namespace
{

struct PickMetric
{
    double ScreenDistance = 1.0e300;
    double Depth = 1.0e300;
};

WorldPointName EncodePointName(WorldCurveName Curve,
                               std::uint32_t LocalIndex)
{
    return { (Curve.IssuedIndex << 8u) | ((LocalIndex + 1u) & 0xFFu) };
}

WorldControlName EncodeControlName(WorldCurveName Curve,
                                   WorldControlSubject Subject,
                                   std::uint32_t LocalIndex)
{
    return { (Curve.IssuedIndex << 12u)
           | ((static_cast<std::uint32_t>(Subject) & 0xFu) << 8u)
           | ((LocalIndex + 1u) & 0xFFu) };
}

const DeclaredWorldCurve* ResolveCurve(const WorldDraftStructure& Declared,
                                       WorldCurveName SourceCurve)
{
    return Declared.Resolve(SourceCurve);
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

double ResolveCameraDepth(const ResolvedCamera& Camera,
                          const SpatialPoint& Position)
{
    return Dot(Difference(Camera.Frame.Eye, Position), Camera.Frame.Forward);
}

bool CandidateVisible(const ResolvedCamera& Camera,
                      double Depth)
{
    return !Camera.Perspective || Depth > 0.01;
}

bool BetterMetric(const PickMetric& Candidate,
                  const PickMetric& Current)
{
    if (Candidate.ScreenDistance + 1.0e-9 < Current.ScreenDistance)
        return true;
    if (std::fabs(Candidate.ScreenDistance - Current.ScreenDistance) <= 1.0e-9)
        return Candidate.Depth < Current.Depth;
    return false;
}

double DistancePointSegmentSquared2(const double X,
                                    const double Y,
                                    const double X0,
                                    const double Y0,
                                    const double X1,
                                    const double Y1)
{
    const double DX = X1 - X0;
    const double DY = Y1 - Y0;
    const double LengthSquared = DX * DX + DY * DY;
    if (LengthSquared <= 1.0e-18)
    {
        const double RX = X - X0;
        const double RY = Y - Y0;
        return RX * RX + RY * RY;
    }

    const double Parameter = std::clamp(((X - X0) * DX + (Y - Y0) * DY) / LengthSquared, 0.0, 1.0);
    const double HitX = X0 + DX * Parameter;
    const double HitY = Y0 + DY * Parameter;
    const double RX = X - HitX;
    const double RY = Y - HitY;
    return RX * RX + RY * RY;
}

bool LoopContainsCurve(const DeclaredWorldLoop& Loop,
                       WorldCurveName Curve)
{
    for (const WorldCurveUse& Use : Loop.Traversal)
        if (Use.TraversedCurve.IssuedIndex == Curve.IssuedIndex)
            return true;
    return false;
}

bool ResolveCurvePolyline(const WorldDraftStructure& Declared,
                          WorldCurveName Curve,
                          std::vector<SpatialPoint>& Polyline)
{
    Polyline.clear();
    const DeclaredWorldCurve* Held = ResolveCurve(Declared, Curve);
    if (Held == nullptr || !Held->Geometry.Declared())
        return false;

    AppendCurvePolyline(Held->Geometry, Polyline, 48u);
    return Polyline.size() >= 2u;
}

bool ResolveLoopOwnerForCurve(const WorldDraftStructure& Declared,
                              WorldCurveName Curve,
                              WorldLoopName& Owner)
{
    Owner = {};
    for (std::uint32_t LoopIndex = 1u; LoopIndex <= Declared.LoopCount(); ++LoopIndex)
    {
        const DeclaredWorldLoop* Loop = Declared.Resolve(WorldLoopName{ LoopIndex });
        if (Loop != nullptr && LoopContainsCurve(*Loop, Curve))
        {
            Owner = { LoopIndex };
            return true;
        }
    }
    return false;
}

bool PickingPointInsideLoop(const WorldPlacementFrame& Frame,
                            const std::vector<SpatialPoint>& Loop,
                            const SpatialPoint& Probe)
{
    if (Loop.size() < 3u || !Frame.Declared())
        return false;

    std::vector<double> Alongs;
    std::vector<double> Acrosses;
    Alongs.reserve(Loop.size());
    Acrosses.reserve(Loop.size());
    for (const SpatialPoint& Point : Loop)
    {
        double Along = 0.0;
        double Across = 0.0;
        ResolveWorldPlacementCoordinates(Frame, Point, Along, Across);
        Alongs.push_back(Along);
        Acrosses.push_back(Across);
    }

    double ProbeAlong = 0.0;
    double ProbeAcross = 0.0;
    ResolveWorldPlacementCoordinates(Frame, Probe, ProbeAlong, ProbeAcross);

    bool Inside = false;
    for (std::size_t Index = 0u, Prior = Loop.size() - 1u; Index < Loop.size(); Prior = Index++)
    {
        const bool Crosses = ((Acrosses[Index] > ProbeAcross) != (Acrosses[Prior] > ProbeAcross))
                          && (ProbeAlong < (Alongs[Prior] - Alongs[Index])
                                          * (ProbeAcross - Acrosses[Index])
                                          / ((Acrosses[Prior] - Acrosses[Index]) + 1.0e-300)
                                          + Alongs[Index]);
        if (Crosses)
            Inside = !Inside;
    }
    return Inside;
}

bool ResolveNearestWorldDraftPoint(const WorldDraftStructure& Declared,
                                   const ResolvedCamera& Camera,
                                   const PlaneExtent& Extent,
                                   float ScreenX,
                                   float ScreenY,
                                   double MaximumDistancePixels,
                                   WorldPointPlacement& Resolved,
                                   PickMetric& Metric)
{
    Metric = {};
    std::vector<WorldPointPlacement> PointSet;
    bool Found = false;

    for (std::uint32_t CurveIndex = 1u; CurveIndex <= Declared.CurveCount(); ++CurveIndex)
    {
        if (!ResolveWorldDraftPoints(Declared, { CurveIndex }, PointSet))
            continue;

        for (const WorldPointPlacement& Point : PointSet)
        {
            const double Depth = ResolveCameraDepth(Camera, Point.Position);
            if (!CandidateVisible(Camera, Depth))
                continue;

            float CandidateX = 0.0f;
            float CandidateY = 0.0f;
            if (!ProjectFromCamera(Camera, Extent, Point.Position, CandidateX, CandidateY))
                continue;

            const double DX = static_cast<double>(CandidateX) - static_cast<double>(ScreenX);
            const double DY = static_cast<double>(CandidateY) - static_cast<double>(ScreenY);
            const PickMetric Candidate = { std::sqrt(DX * DX + DY * DY), Depth };
            if (Candidate.ScreenDistance <= MaximumDistancePixels && BetterMetric(Candidate, Metric))
            {
                Metric = Candidate;
                Resolved = Point;
                Found = true;
            }
        }
    }

    return Found;
}

bool ResolveNearestWorldDraftControl(const WorldDraftStructure& Declared,
                                     const ResolvedCamera& Camera,
                                     const PlaneExtent& Extent,
                                     float ScreenX,
                                     float ScreenY,
                                     double MaximumDistancePixels,
                                     WorldControlPlacement& Resolved,
                                     PickMetric& Metric)
{
    Metric = {};
    std::vector<WorldControlPlacement> ControlSet;
    bool Found = false;

    for (std::uint32_t CurveIndex = 1u; CurveIndex <= Declared.CurveCount(); ++CurveIndex)
    {
        if (!ResolveWorldDraftControls(Declared, { CurveIndex }, ControlSet))
            continue;

        for (const WorldControlPlacement& Control : ControlSet)
        {
            const double Depth = ResolveCameraDepth(Camera, Control.Position);
            if (!CandidateVisible(Camera, Depth))
                continue;

            float CandidateX = 0.0f;
            float CandidateY = 0.0f;
            if (!ProjectFromCamera(Camera, Extent, Control.Position, CandidateX, CandidateY))
                continue;

            const double DX = static_cast<double>(CandidateX) - static_cast<double>(ScreenX);
            const double DY = static_cast<double>(CandidateY) - static_cast<double>(ScreenY);
            const PickMetric Candidate = { std::sqrt(DX * DX + DY * DY), Depth };
            if (Candidate.ScreenDistance <= MaximumDistancePixels && BetterMetric(Candidate, Metric))
            {
                Metric = Candidate;
                Resolved = Control;
                Found = true;
            }
        }
    }

    return Found;
}

bool ResolveNearestWorldDraftCurve(const WorldDraftStructure& Declared,
                                   const ResolvedCamera& Camera,
                                   const PlaneExtent& Extent,
                                   float ScreenX,
                                   float ScreenY,
                                   double MaximumDistancePixels,
                                   WorldCurveName& Resolved,
                                   SpatialPoint& HitPosition,
                                   PickMetric& Metric)
{
    Metric = {};
    std::vector<SpatialPoint> Polyline;
    bool Found = false;

    for (std::uint32_t CurveIndex = 1u; CurveIndex <= Declared.CurveCount(); ++CurveIndex)
    {
        if (!ResolveCurvePolyline(Declared, { CurveIndex }, Polyline))
            continue;

        for (std::size_t PointIndex = 0u; PointIndex + 1u < Polyline.size(); ++PointIndex)
        {
            float X0 = 0.0f;
            float Y0 = 0.0f;
            float X1 = 0.0f;
            float Y1 = 0.0f;
            if (!ProjectFromCamera(Camera, Extent, Polyline[PointIndex], X0, Y0) ||
                !ProjectFromCamera(Camera, Extent, Polyline[PointIndex + 1u], X1, Y1))
                continue;

            const double CandidateDistance = std::sqrt(DistancePointSegmentSquared2(
                ScreenX, ScreenY, X0, Y0, X1, Y1));
            if (CandidateDistance > MaximumDistancePixels)
                continue;

            const double DX = static_cast<double>(X1) - static_cast<double>(X0);
            const double DY = static_cast<double>(Y1) - static_cast<double>(Y0);
            const double LengthSquared = DX * DX + DY * DY;
            const double Parameter = LengthSquared > 1.0e-18
                                   ? std::clamp(((static_cast<double>(ScreenX) - X0) * DX
                                               + (static_cast<double>(ScreenY) - Y0) * DY) / LengthSquared,
                                                0.0, 1.0)
                                   : 0.0;
            const SpatialPoint CandidateHit = Added(Polyline[PointIndex],
                                                    Scaled(Difference(Polyline[PointIndex], Polyline[PointIndex + 1u]),
                                                           Parameter));
            const double Depth = ResolveCameraDepth(Camera, CandidateHit);
            if (!CandidateVisible(Camera, Depth))
                continue;

            const PickMetric Candidate = { CandidateDistance, Depth };
            if (BetterMetric(Candidate, Metric))
            {
                Metric = Candidate;
                Resolved = { CurveIndex };
                HitPosition = CandidateHit;
                Found = true;
            }
        }
    }

    return Found;
}

bool ResolveNearestWorldDraftLoop(const WorldDraftStructure& Declared,
                                  const ResolvedCamera& Camera,
                                  const PlaneExtent& Extent,
                                  float ScreenX,
                                  float ScreenY,
                                  WorldLoopName& Resolved,
                                  SpatialPoint& HitPosition,
                                  PickMetric& Metric)
{
    Metric = {};
    SpatialPoint RayOrigin = {};
    SpatialDirection RayDirection = {};
    if (!ResolveCameraRay(Camera, Extent, ScreenX, ScreenY, RayOrigin, RayDirection))
        return false;

    const WorldDraftAnalysis Analysis = AnalyzeWorldDraft(Declared);
    bool Found = false;

    for (const WorldLoopAnalysisRecord& Loop : Analysis.Loops)
    {
        if (!Loop.FillEligible || !Loop.SupportFrame.Declared() || Loop.Outline.size() < 3u)
            continue;

        SpatialPoint Hit = {};
        if (!ResolveWorldPlacementIntersection(Loop.SupportFrame, RayOrigin, RayDirection, Hit))
            continue;
        if (!PickingPointInsideLoop(Loop.SupportFrame, Loop.Outline, Hit))
            continue;

        const double Depth = ResolveCameraDepth(Camera, Hit);
        if (!CandidateVisible(Camera, Depth))
            continue;

        const PickMetric Candidate = { 0.0, Depth };
        if (BetterMetric(Candidate, Metric))
        {
            Metric = Candidate;
            Resolved = Loop.Loop;
            HitPosition = Hit;
            Found = true;
        }
    }

    return Found;
}

} // namespace

bool ResolveWorldDraftPoints(const WorldDraftStructure& Declared,
                             WorldCurveName SourceCurve,
                             std::vector<WorldPointPlacement>& Resolved)
{
    Resolved.clear();

    const DeclaredWorldCurve* Held = ResolveCurve(Declared, SourceCurve);
    if (Held == nullptr || !Held->Geometry.Declared())
        return false;

    switch (Held->Geometry.Subject())
    {
        case CurveSubject::Line:
            Resolved.push_back({ EncodePointName(SourceCurve, 0u), SourceCurve, Held->Geometry.HeldLine().Origin });
            Resolved.push_back({ EncodePointName(SourceCurve, 1u), SourceCurve, Held->Geometry.HeldLine().Terminus });
            return true;

        case CurveSubject::CircularArc:
            Resolved.push_back({ EncodePointName(SourceCurve, 0u), SourceCurve,
                                 Added(Held->Geometry.HeldCircularArc().Centre,
                                       Scaled(Normalize(Held->Geometry.HeldCircularArc().StartDirection),
                                              Held->Geometry.HeldCircularArc().Radius)) });
            Resolved.push_back({ EncodePointName(SourceCurve, 1u), SourceCurve,
                                 Added(Held->Geometry.HeldCircularArc().Centre,
                                       Scaled(RotateAroundAxis(Normalize(Held->Geometry.HeldCircularArc().StartDirection),
                                                               Held->Geometry.HeldCircularArc().Normal,
                                                               Held->Geometry.HeldCircularArc().SweepRadians),
                                              Held->Geometry.HeldCircularArc().Radius)) });
            return true;

        case CurveSubject::Circle:
            return false;

        case CurveSubject::EllipticalArc:
        {
            std::vector<SpatialPoint> Polyline;
            AppendCurvePolyline(Held->Geometry, Polyline, 48u);
            if (Polyline.size() < 2u)
                return false;
            Resolved.push_back({ EncodePointName(SourceCurve, 0u), SourceCurve, Polyline.front() });
            Resolved.push_back({ EncodePointName(SourceCurve, 1u), SourceCurve, Polyline.back() });
            return true;
        }

        case CurveSubject::Ellipse:
            return false;

        case CurveSubject::Bezier:
            if (Held->Geometry.HeldBezier().ControlPoints.size() < 2u)
                return false;
            Resolved.push_back({ EncodePointName(SourceCurve, 0u), SourceCurve,
                                 Held->Geometry.HeldBezier().ControlPoints.front() });
            Resolved.push_back({ EncodePointName(SourceCurve, 1u), SourceCurve,
                                 Held->Geometry.HeldBezier().ControlPoints.back() });
            return true;

        case CurveSubject::BasisSpline:
            if (Held->Geometry.HeldBasisSpline().ControlPoints.size() < 2u)
                return false;
            Resolved.push_back({ EncodePointName(SourceCurve, 0u), SourceCurve,
                                 Held->Geometry.HeldBasisSpline().ControlPoints.front() });
            Resolved.push_back({ EncodePointName(SourceCurve, 1u), SourceCurve,
                                 Held->Geometry.HeldBasisSpline().ControlPoints.back() });
            return true;

        case CurveSubject::RationalSpline:
            if (Held->Geometry.HeldRationalSpline().ControlPoints.size() < 2u)
                return false;
            Resolved.push_back({ EncodePointName(SourceCurve, 0u), SourceCurve,
                                 Held->Geometry.HeldRationalSpline().ControlPoints.front() });
            Resolved.push_back({ EncodePointName(SourceCurve, 1u), SourceCurve,
                                 Held->Geometry.HeldRationalSpline().ControlPoints.back() });
            return true;

        case CurveSubject::Hermite:
            Resolved.push_back({ EncodePointName(SourceCurve, 0u), SourceCurve, Held->Geometry.HeldHermite().StartPoint });
            Resolved.push_back({ EncodePointName(SourceCurve, 1u), SourceCurve, Held->Geometry.HeldHermite().EndPoint });
            return true;

        case CurveSubject::SubjectCount:
            return false;
    }

    return false;
}

bool ResolveWorldDraftControls(const WorldDraftStructure& Declared,
                               WorldCurveName SourceCurve,
                               std::vector<WorldControlPlacement>& Resolved)
{
    Resolved.clear();

    const DeclaredWorldCurve* Held = ResolveCurve(Declared, SourceCurve);
    if (Held == nullptr || !Held->Geometry.Declared())
        return false;

    switch (Held->Geometry.Subject())
    {
        case CurveSubject::Line:
            return true;

        case CurveSubject::CircularArc:
        {
            const CircularArcCurve& Arc = Held->Geometry.HeldCircularArc();
            Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::Centre, 0u), SourceCurve,
                                 WorldControlSubject::Centre, 0u, Arc.Centre });
            Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::Radius, 0u), SourceCurve,
                                 WorldControlSubject::Radius, 0u,
                                 Added(Arc.Centre, Scaled(Normalize(Arc.StartDirection), Arc.Radius)) });
            if (Arc.ThroughDeclared)
                Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::Through, 0u), SourceCurve,
                                     WorldControlSubject::Through, 0u, Arc.ThroughPoint });
            return true;
        }

        case CurveSubject::Circle:
        {
            const CircleCurve& Circle = Held->Geometry.HeldCircle();
            Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::Centre, 0u), SourceCurve,
                                 WorldControlSubject::Centre, 0u, Circle.Centre });
            Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::Radius, 0u), SourceCurve,
                                 WorldControlSubject::Radius, 0u,
                                 Added(Circle.Centre, Scaled(Normalize(Circle.StartDirection), Circle.Radius)) });
            return true;
        }

        case CurveSubject::EllipticalArc:
        {
            const EllipticalArcCurve& Arc = Held->Geometry.HeldEllipticalArc();
            const SpatialDirection MajorDirection = Normalize(Arc.MajorDirection);
            const SpatialDirection MinorDirection = Normalize(Cross(Arc.Normal, MajorDirection));
            Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::Centre, 0u), SourceCurve,
                                 WorldControlSubject::Centre, 0u, Arc.Centre });
            Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::MajorAxis, 0u), SourceCurve,
                                 WorldControlSubject::MajorAxis, 0u,
                                 Added(Arc.Centre, Scaled(MajorDirection, Arc.MajorRadius)) });
            Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::MinorAxis, 0u), SourceCurve,
                                 WorldControlSubject::MinorAxis, 0u,
                                 Added(Arc.Centre, Scaled(MinorDirection, Arc.MinorRadius)) });
            return true;
        }

        case CurveSubject::Ellipse:
        {
            const EllipseCurve& Ellipse = Held->Geometry.HeldEllipse();
            const SpatialDirection MajorDirection = Normalize(Ellipse.MajorDirection);
            const SpatialDirection MinorDirection = Normalize(Cross(Ellipse.Normal, MajorDirection));
            Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::Centre, 0u), SourceCurve,
                                 WorldControlSubject::Centre, 0u, Ellipse.Centre });
            Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::MajorAxis, 0u), SourceCurve,
                                 WorldControlSubject::MajorAxis, 0u,
                                 Added(Ellipse.Centre, Scaled(MajorDirection, Ellipse.MajorRadius)) });
            Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::MinorAxis, 0u), SourceCurve,
                                 WorldControlSubject::MinorAxis, 0u,
                                 Added(Ellipse.Centre, Scaled(MinorDirection, Ellipse.MinorRadius)) });
            return true;
        }

        case CurveSubject::Bezier:
            for (std::uint32_t PointIndex = 0u; PointIndex < Held->Geometry.HeldBezier().ControlPoints.size(); ++PointIndex)
                Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::ControlPoint, PointIndex), SourceCurve,
                                     WorldControlSubject::ControlPoint, PointIndex,
                                     Held->Geometry.HeldBezier().ControlPoints[PointIndex] });
            return true;

        case CurveSubject::BasisSpline:
            for (std::uint32_t PointIndex = 0u; PointIndex < Held->Geometry.HeldBasisSpline().ControlPoints.size(); ++PointIndex)
                Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::ControlPoint, PointIndex), SourceCurve,
                                     WorldControlSubject::ControlPoint, PointIndex,
                                     Held->Geometry.HeldBasisSpline().ControlPoints[PointIndex] });
            return true;

        case CurveSubject::RationalSpline:
            for (std::uint32_t PointIndex = 0u; PointIndex < Held->Geometry.HeldRationalSpline().ControlPoints.size(); ++PointIndex)
                Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::ControlPoint, PointIndex), SourceCurve,
                                     WorldControlSubject::ControlPoint, PointIndex,
                                     Held->Geometry.HeldRationalSpline().ControlPoints[PointIndex] });
            return true;

        case CurveSubject::Hermite:
        {
            const HermiteCurve& Hermite = Held->Geometry.HeldHermite();
            Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::StartTangent, 0u), SourceCurve,
                                 WorldControlSubject::StartTangent, 0u,
                                 Added(Hermite.StartPoint, Hermite.StartTangent) });
            Resolved.push_back({ EncodeControlName(SourceCurve, WorldControlSubject::EndTangent, 0u), SourceCurve,
                                 WorldControlSubject::EndTangent, 0u,
                                 Added(Hermite.EndPoint, Hermite.EndTangent) });
            return true;
        }

        case CurveSubject::SubjectCount:
            return false;
    }

    return false;
}

bool ResolveWorldCurvePivot(const WorldDraftStructure& Declared,
                            WorldCurveName Curve,
                            SpatialPoint& Pivot)
{
    const DeclaredWorldCurve* Held = ResolveCurve(Declared, Curve);
    if (Held == nullptr || !Held->Geometry.Declared())
        return false;

    switch (Held->Geometry.Subject())
    {
        case CurveSubject::Line:
        {
            const LineCurve& Line = Held->Geometry.HeldLine();
            Pivot = { (Line.Origin.Left + Line.Terminus.Left) * 0.5,
                      (Line.Origin.Up + Line.Terminus.Up) * 0.5,
                      (Line.Origin.Forward + Line.Terminus.Forward) * 0.5 };
            return true;
        }
        case CurveSubject::CircularArc:
            Pivot = Held->Geometry.HeldCircularArc().Centre;
            return true;
        case CurveSubject::Circle:
            Pivot = Held->Geometry.HeldCircle().Centre;
            return true;
        case CurveSubject::EllipticalArc:
            Pivot = Held->Geometry.HeldEllipticalArc().Centre;
            return true;
        case CurveSubject::Ellipse:
            Pivot = Held->Geometry.HeldEllipse().Centre;
            return true;
        case CurveSubject::Bezier:
            if (!Held->Geometry.HeldBezier().ControlPoints.empty())
            {
                Pivot = Held->Geometry.HeldBezier().ControlPoints[Held->Geometry.HeldBezier().ControlPoints.size() / 2u];
                return true;
            }
            return false;
        case CurveSubject::BasisSpline:
            if (!Held->Geometry.HeldBasisSpline().ControlPoints.empty())
            {
                Pivot = Held->Geometry.HeldBasisSpline().ControlPoints[Held->Geometry.HeldBasisSpline().ControlPoints.size() / 2u];
                return true;
            }
            return false;
        case CurveSubject::RationalSpline:
            if (!Held->Geometry.HeldRationalSpline().ControlPoints.empty())
            {
                Pivot = Held->Geometry.HeldRationalSpline().ControlPoints[Held->Geometry.HeldRationalSpline().ControlPoints.size() / 2u];
                return true;
            }
            return false;
        case CurveSubject::Hermite:
        {
            const HermiteCurve& Hermite = Held->Geometry.HeldHermite();
            Pivot = { (Hermite.StartPoint.Left + Hermite.EndPoint.Left) * 0.5,
                      (Hermite.StartPoint.Up + Hermite.EndPoint.Up) * 0.5,
                      (Hermite.StartPoint.Forward + Hermite.EndPoint.Forward) * 0.5 };
            return true;
        }
        case CurveSubject::SubjectCount:
            return false;
    }

    return false;
}

bool ResolveWorldLoopPivot(const WorldDraftStructure& Declared,
                           WorldLoopName Loop,
                           SpatialPoint& Pivot)
{
    const DeclaredWorldLoop* Held = Declared.Resolve(Loop);
    if (Held == nullptr || Held->Traversal.empty())
        return false;

    Pivot = {};
    std::uint32_t Count = 0u;
    for (const WorldCurveUse& Use : Held->Traversal)
    {
        SpatialPoint CurvePivot = {};
        if (!ResolveWorldCurvePivot(Declared, Use.TraversedCurve, CurvePivot))
            continue;
        Pivot = { Pivot.Left + CurvePivot.Left,
                  Pivot.Up + CurvePivot.Up,
                  Pivot.Forward + CurvePivot.Forward };
        ++Count;
    }

    if (Count == 0u)
        return false;

    const double Scale = 1.0 / static_cast<double>(Count);
    Pivot = { Pivot.Left * Scale, Pivot.Up * Scale, Pivot.Forward * Scale };
    return true;
}

bool ResolveWorldDraftPick(const WorldDraftStructure& Declared,
                           const ResolvedCamera& Camera,
                           const PlaneExtent& Extent,
                           float ScreenX,
                           float ScreenY,
                           double MaximumDistancePixels,
                           WorldPick& Resolved)
{
    return ResolveWorldDraftPickForElement(Declared, Camera, Extent, ScreenX, ScreenY,
                                           MaximumDistancePixels, SelectionElement::Free, Resolved);
}

bool ResolveWorldDraftPickForElement(const WorldDraftStructure& Declared,
                                     const ResolvedCamera& Camera,
                                     const PlaneExtent& Extent,
                                     float ScreenX,
                                     float ScreenY,
                                     double MaximumDistancePixels,
                                     SelectionElement Element,
                                     WorldPick& Resolved)
{
    Resolved = {};

    if (Element == SelectionElement::Vertex || Element == SelectionElement::Free)
    {
        WorldPointPlacement Point = {};
        PickMetric Metric = {};
        if (ResolveNearestWorldDraftPoint(Declared, Camera, Extent, ScreenX, ScreenY,
                                          MaximumDistancePixels, Point, Metric))
        {
            Resolved.Subject = WorldPickSubject::Point;
            Resolved.Point = Point.Name;
            Resolved.Curve = Point.SourceCurve;
            Resolved.Position = Point.Position;
            return true;
        }

        WorldControlPlacement Control = {};
        if (ResolveNearestWorldDraftControl(Declared, Camera, Extent, ScreenX, ScreenY,
                                            MaximumDistancePixels, Control, Metric))
        {
            Resolved.Subject = WorldPickSubject::Control;
            Resolved.Control = Control.Name;
            Resolved.Curve = Control.SourceCurve;
            Resolved.Position = Control.Position;
            return true;
        }

        if (Element == SelectionElement::Vertex)
            return false;
    }

    if (Element == SelectionElement::Edge || Element == SelectionElement::Free)
    {
        PickMetric Metric = {};
        SpatialPoint Hit = {};
        WorldCurveName Curve = {};
        if (ResolveNearestWorldDraftCurve(Declared, Camera, Extent, ScreenX, ScreenY,
                                          MaximumDistancePixels, Curve, Hit, Metric))
        {
            Resolved.Subject = WorldPickSubject::Curve;
            Resolved.Curve = Curve;
            Resolved.Position = Hit;
            return true;
        }

        if (Element == SelectionElement::Edge)
            return false;
    }

    if (Element == SelectionElement::Face || Element == SelectionElement::Object || Element == SelectionElement::Free)
    {
        PickMetric Metric = {};
        SpatialPoint Hit = {};
        WorldLoopName Loop = {};
        if (ResolveNearestWorldDraftLoop(Declared, Camera, Extent, ScreenX, ScreenY, Loop, Hit, Metric))
        {
            Resolved.Subject = WorldPickSubject::Loop;
            Resolved.Loop = Loop;
            if (!ResolveWorldLoopPivot(Declared, Loop, Resolved.Position))
                Resolved.Position = Hit;
            return true;
        }

        if (Element == SelectionElement::Face)
            return false;

        if (Element == SelectionElement::Object)
        {
            SpatialPoint CurveHit = {};
            WorldCurveName Curve = {};
            if (ResolveNearestWorldDraftCurve(Declared, Camera, Extent, ScreenX, ScreenY,
                                              MaximumDistancePixels, Curve, CurveHit, Metric))
            {
                WorldLoopName Owner = {};
                if (ResolveLoopOwnerForCurve(Declared, Curve, Owner))
                {
                    Resolved.Subject = WorldPickSubject::Loop;
                    Resolved.Loop = Owner;
                    if (!ResolveWorldLoopPivot(Declared, Owner, Resolved.Position))
                        Resolved.Position = CurveHit;
                    return true;
                }

                Resolved.Subject = WorldPickSubject::Curve;
                Resolved.Curve = Curve;
                Resolved.Position = CurveHit;
                return true;
            }
        }
    }

    return false;
}

} // namespace Slate
