//============================================================================================================================================
//                                                         SKETCHSNAP.CPP
//============================================================================================================================================

#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"
#include "SlateShape/Sketch/SketchSnap/Api/SketchSnap.h"
#include "SlateShape/Sketch/SketchPolyline/Api/SketchPolyline.h"

#include <algorithm>
#include <cmath>

namespace Slate
{

namespace
{
    SpatialDirection RotateAroundAxis(const SpatialDirection& Subject,
                                      const SpatialDirection& Axis,
                                      double Radians)
    {
        const SpatialDirection UnitAxis = Normalize(Axis);
        const double Cosine = std::cos(Radians);
        const double Sine = std::sin(Radians);
        const double Projection = UnitAxis.Left * Subject.Left + UnitAxis.Up * Subject.Up + UnitAxis.Forward * Subject.Forward;
        const SpatialDirection Parallel = Scaled(UnitAxis, Projection);
        const SpatialDirection Perpendicular = { Subject.Left - Parallel.Left,
                                                 Subject.Up - Parallel.Up,
                                                 Subject.Forward - Parallel.Forward };
        const SpatialDirection Crossed = Cross(UnitAxis, Subject);
        return { Perpendicular.Left * Cosine + Crossed.Left * Sine + Parallel.Left,
                 Perpendicular.Up * Cosine + Crossed.Up * Sine + Parallel.Up,
                 Perpendicular.Forward * Cosine + Crossed.Forward * Sine + Parallel.Forward };
    }

    double DistanceSquared(const SpatialPoint& LeftPoint,
                           const SpatialPoint& RightPoint)
    {
        return LengthSquared(Difference(LeftPoint, RightPoint));
    }

    const DeclaredSketchCurve* ResolveCurve(const SketchStructure& Declared,
                                            SketchCurveName SourceCurve)
    {
        if (!SourceCurve.Assigned() || SourceCurve.IssuedIndex > Declared.Curves().size())
            return nullptr;
        return &Declared.Curves()[SourceCurve.IssuedIndex - 1u];
    }

    void AppendCurvePolylineLocal(const CurveSpecification& Geometry,
                                  std::vector<SpatialPoint>& Polyline)
    {
        Slate::AppendCurvePolyline(Geometry, Polyline, 48u);
    }

    void ConsiderCandidate(const SpatialPoint& Probe,
                           const SpatialPoint& CandidatePosition,
                           SketchCurveName SourceCurve,
                           SketchSnapSubject Subject,
                           SketchPointName SketchPoint,
                           SketchControlName SketchControl,
                           double MaximumDistance,
                           SketchSnapPlacement& Best)
    {
        const double CandidateDistance = std::sqrt(DistanceSquared(Probe, CandidatePosition));
        if (CandidateDistance > MaximumDistance)
            return;
        if (!Best.Resolved() || CandidateDistance < Best.Distance)
            Best = { Subject, SourceCurve, SketchPoint, SketchControl, CandidatePosition, CandidateDistance };
    }

    bool SegmentIntersectionPlanar(const SpatialPoint& A,
                                   const SpatialPoint& B,
                                   const SpatialPoint& C,
                                   const SpatialPoint& D,
                                   SpatialPoint& Result)
    {
        const double ABx = B.Left - A.Left;
        const double ABy = B.Forward - A.Forward;
        const double CDx = D.Left - C.Left;
        const double CDy = D.Forward - C.Forward;
        const double Denominator = ABx * CDy - ABy * CDx;
        if (std::fabs(Denominator) <= 1.0e-9)
            return false;
        const double ACx = C.Left - A.Left;
        const double ACy = C.Forward - A.Forward;
        const double T = (ACx * CDy - ACy * CDx) / Denominator;
        const double U = (ACx * ABy - ACy * ABx) / Denominator;
        if (T < 0.0 || T > 1.0 || U < 0.0 || U > 1.0)
            return false;
        Result = Added(A, Scaled(Difference(A, B), T));
        return true;
    }
}


SketchSnapPlacement ResolveNearestSnap(const SketchStructure& Declared,
                                       const SpatialPoint& Probe,
                                       double MaximumDistance,
                                       const SketchSnapMask& Accepted)
{
    SketchSnapPlacement Best = {};
    Best.Distance = MaximumDistance;

    struct CurvePolyline
    {
        SketchCurveName Curve = {};
        std::vector<SpatialPoint> Points = {};
    };

    std::vector<SketchPointPlacement> Points;
    std::vector<SketchControlPlacement> Controls;
    std::vector<SpatialPoint> Polyline;
    std::vector<CurvePolyline> CurvePolylines;

    for (std::uint32_t CurveIndex = 1u; CurveIndex <= Declared.Curves().size(); ++CurveIndex)
    {
        const SketchCurveName Curve = { CurveIndex };

        if (Accepted.EndpointAccepted && ResolveSketchPoints(Declared, Curve, Points))
        {
            for (const SketchPointPlacement& Point : Points)
                ConsiderCandidate(Probe, Point.Position, Curve, SketchSnapSubject::Endpoint, Point.Name, {}, MaximumDistance, Best);
        }

        if ((Accepted.CentreAccepted || Accepted.ControlAccepted) && ResolveSketchControls(Declared, Curve, Controls))
        {
            for (const SketchControlPlacement& Control : Controls)
            {
                if (Accepted.CentreAccepted && Control.Subject == SketchControlSubject::Centre)
                    ConsiderCandidate(Probe, Control.Position, Curve, SketchSnapSubject::Centre, {}, Control.Name, MaximumDistance, Best);
                else if (Accepted.ControlAccepted)
                    ConsiderCandidate(Probe, Control.Position, Curve, SketchSnapSubject::Control, {}, Control.Name, MaximumDistance, Best);
            }
        }

        const DeclaredSketchCurve* Held = ResolveCurve(Declared, Curve);
        if (Held == nullptr || !Held->Geometry.Declared())
            continue;

        if (Accepted.TangentAccepted && Held->Geometry.Subject() == CurveSubject::Circle)
        {
            const CircleCurve& Circle = Held->Geometry.HeldCircle();
            const SpatialDirection Radial = Difference(Circle.Centre, Probe);
            if (LengthSquared(Radial) > 1.0e-12)
                ConsiderCandidate(Probe, Added(Circle.Centre, Scaled(Normalize(Radial), Circle.Radius)),
                                  Curve, SketchSnapSubject::Tangent, {}, {}, MaximumDistance, Best);
        }

        AppendCurvePolylineLocal(Held->Geometry, Polyline);
        if (Polyline.size() < 2u)
            continue;
        if (Accepted.IntersectionAccepted)
            CurvePolylines.push_back({ Curve, Polyline });

        if (Accepted.MidpointAccepted)
        {
            for (std::size_t PointIndex = 0u; PointIndex + 1u < Polyline.size(); ++PointIndex)
            {
                const SpatialPoint Midpoint = { (Polyline[PointIndex].Left + Polyline[PointIndex + 1u].Left) * 0.5,
                                                (Polyline[PointIndex].Up + Polyline[PointIndex + 1u].Up) * 0.5,
                                                (Polyline[PointIndex].Forward + Polyline[PointIndex + 1u].Forward) * 0.5 };
                ConsiderCandidate(Probe, Midpoint, Curve, SketchSnapSubject::Midpoint, {}, {}, MaximumDistance, Best);
            }
        }

        if (Accepted.AlongCurveAccepted)
        {
            for (std::size_t PointIndex = 0u; PointIndex + 1u < Polyline.size(); ++PointIndex)
            {
                const SpatialDirection Span = Difference(Polyline[PointIndex], Polyline[PointIndex + 1u]);
                const SpatialDirection Offset = Difference(Polyline[PointIndex], Probe);
                const double SpanLengthSquared = LengthSquared(Span);
                const double Parameter = SpanLengthSquared > 1.0e-18
                    ? std::clamp((Offset.Left * Span.Left + Offset.Up * Span.Up + Offset.Forward * Span.Forward) / SpanLengthSquared,
                                 0.0, 1.0)
                    : 0.0;
                const SpatialPoint Closest = Added(Polyline[PointIndex], Scaled(Span, Parameter));
                const SketchSnapSubject Subject = Accepted.PerpendicularAccepted && Parameter > 1.0e-4 && Parameter < 1.0 - 1.0e-4
                                                ? SketchSnapSubject::Perpendicular
                                                : SketchSnapSubject::AlongCurve;
                ConsiderCandidate(Probe, Closest, Curve, Subject, {}, {}, MaximumDistance, Best);
            }
        }
    }

    if (Accepted.IntersectionAccepted)
    {
        for (std::size_t LeftIndex = 0u; LeftIndex < CurvePolylines.size(); ++LeftIndex)
        {
            for (std::size_t RightIndex = LeftIndex + 1u; RightIndex < CurvePolylines.size(); ++RightIndex)
            {
                const CurvePolyline& Left = CurvePolylines[LeftIndex];
                const CurvePolyline& Right = CurvePolylines[RightIndex];
                for (std::size_t A = 0u; A + 1u < Left.Points.size(); ++A)
                {
                    for (std::size_t B = 0u; B + 1u < Right.Points.size(); ++B)
                    {
                        SpatialPoint Intersected = {};
                        if (SegmentIntersectionPlanar(Left.Points[A], Left.Points[A + 1u],
                                                      Right.Points[B], Right.Points[B + 1u],
                                                      Intersected))
                        {
                            ConsiderCandidate(Probe, Intersected, Left.Curve, SketchSnapSubject::Intersection,
                                              {}, {}, MaximumDistance, Best);
                        }
                    }
                }
            }
        }
    }

    return Best;
}

} // namespace Slate
