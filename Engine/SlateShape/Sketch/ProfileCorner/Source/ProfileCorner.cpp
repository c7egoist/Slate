//============================================================================================================================================
//                                                         PROFILECORNER.CPP
//============================================================================================================================================

#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"
#include "SlateShape/Sketch/ProfileCorner/Api/ProfileCorner.h"
#include "SlateShape/Sketch/ProfileReshape/Api/ProfileReshape.h"
#include "SlateShape/Sketch/SketchPolyline/Api/SketchPolyline.h"

#include <algorithm>
#include <cmath>

namespace Slate
{

namespace
{
    bool SamePoint(const SpatialPoint& LeftPoint, const SpatialPoint& RightPoint)
    {
        return LengthSquared(Difference(LeftPoint, RightPoint)) <= 1.0e-18;
    }

    struct LoopCurveEndpoint
    {
        SketchCurveName Curve = {};
        SpatialPoint StartPoint = {};
        SpatialPoint EndPoint = {};
        SpatialDirection StartTangent = {};
        SpatialDirection EndTangent = {};
    };

    bool ResolveLoopCurve(const SketchStructure& Declared,
                          const ProfileCurveUse& Use,
                          LoopCurveEndpoint& Resolved)
    {
        if (!Use.TraversedCurve.Assigned() || Use.TraversedCurve.IssuedIndex > Declared.Curves().size())
            return false;
        Resolved.Curve = { Use.TraversedCurve.IssuedIndex };
        const CurveSpecification& Geometry = Declared.Curves()[Use.TraversedCurve.IssuedIndex - 1u].Geometry;
        if (!Geometry.Declared())
            return false;

        std::vector<SpatialPoint> Polyline;
        AppendCurvePolyline(Geometry, Polyline, 96u);
        if (Polyline.size() < 2u)
            return false;

        SpatialPoint StartPoint = Polyline.front();
        SpatialPoint EndPoint = Polyline.back();
        SpatialDirection StartTangent = Normalize(Difference(Polyline[0], Polyline[1]));
        SpatialDirection EndTangent = Normalize(Difference(Polyline[Polyline.size() - 2u], Polyline.back()));

        if (Use.SameSense)
        {
            Resolved.StartPoint = StartPoint;
            Resolved.EndPoint = EndPoint;
            Resolved.StartTangent = StartTangent;
            Resolved.EndTangent = EndTangent;
        }
        else
        {
            Resolved.StartPoint = EndPoint;
            Resolved.EndPoint = StartPoint;
            Resolved.StartTangent = { -EndTangent.Left, -EndTangent.Up, -EndTangent.Forward };
            Resolved.EndTangent = { -StartTangent.Left, -StartTangent.Up, -StartTangent.Forward };
        }

        return true;
    }

    bool ResolveLoopCurveSet(const SketchStructure& Declared,
                             const ProfileLoop& Loop,
                             std::vector<LoopCurveEndpoint>& Curves)
    {
        Curves.clear();
        if (Loop.Traversal.size() < 3u)
            return false;
        Curves.reserve(Loop.Traversal.size());

        SpatialPoint PreviousEnd = {};
        SpatialPoint FirstStart = {};
        bool First = true;
        for (const ProfileCurveUse& Use : Loop.Traversal)
        {
            LoopCurveEndpoint Curve;
            if (!ResolveLoopCurve(Declared, Use, Curve))
                return false;
            if (First)
            {
                FirstStart = Curve.StartPoint;
                First = false;
            }
            else if (!SamePoint(PreviousEnd, Curve.StartPoint))
                return false;
            Curves.push_back(Curve);
            PreviousEnd = Curve.EndPoint;
        }

        return SamePoint(PreviousEnd, FirstStart);
    }
}

CornerDisposition EvaluateProfileCorner(const SketchStructure& Declared,
                                        ProfileNameInFeature Subject,
                                        std::uint32_t LoopIndex,
                                        std::uint32_t CornerIndex,
                                        double Radius,
                                        bool Chamfer)
{
    if (!Subject.Assigned() && Radius == 0.0)
        return CornerDisposition::NotRequested;
    if (!Declared.Declared() || !Subject.Assigned() || Radius <= 0.0)
        return CornerDisposition::InvalidSpecification;
    if (Subject.IssuedIndex > Declared.Profiles().size())
        return CornerDisposition::InvalidSpecification;

    const ProfileSpecification& Profile = Declared.Profiles()[Subject.IssuedIndex - 1u];
    if (LoopIndex >= Profile.HeldLoops().size())
        return CornerDisposition::InvalidSpecification;

    std::vector<LoopCurveEndpoint> Curves;
    if (!ResolveLoopCurveSet(Declared, Profile.HeldLoops()[LoopIndex], Curves))
        return CornerDisposition::UnsupportedGeometry;
    if (CornerIndex >= Curves.size())
        return CornerDisposition::InvalidSpecification;
    (void)Chamfer;
    return CornerDisposition::Produced;
}

Deliver<ProfileNameInFeature> ApplyProfileCorner(SketchStructure& Declared,
                                                 ProfileNameInFeature Subject,
                                                 std::uint32_t LoopIndex,
                                                 std::uint32_t CornerIndex,
                                                 double Radius,
                                                 bool Chamfer)
{
    if (EvaluateProfileCorner(Declared, Subject, LoopIndex, CornerIndex, Radius, Chamfer) != CornerDisposition::Produced)
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the requested profile corner is unsupported" });

    const ProfileSpecification& Source = Declared.Profiles()[Subject.IssuedIndex - 1u];
    std::vector<LoopCurveEndpoint> Curves;
    ResolveLoopCurveSet(Declared, Source.HeldLoops()[LoopIndex], Curves);

    const std::size_t Count = Curves.size();
    const std::size_t PriorIndex = (CornerIndex + Count - 1u) % Count;
    const std::size_t NextIndex = CornerIndex;
    const SpatialPoint Corner = Curves[PriorIndex].EndPoint;
    const SpatialDirection Incoming = Curves[PriorIndex].EndTangent;
    const SpatialDirection Outgoing = Curves[NextIndex].StartTangent;
    const double Cosine = std::clamp(Dot(Incoming, Outgoing), -1.0, 1.0);
    const double Radians = std::acos(Cosine);
    if (Radians <= 1.0e-6 || std::fabs(3.141592653589793 - Radians) <= 1.0e-6)
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the selected corner is degenerate" });

    const double BackwardLength = std::sqrt(LengthSquared(Difference(Curves[PriorIndex].StartPoint, Corner)));
    const double ForwardLength = std::sqrt(LengthSquared(Difference(Corner, Curves[NextIndex].EndPoint)));
    const double TangentDistance = Radius / std::tan(Radians * 0.5);
    if (TangentDistance <= 0.0 || TangentDistance >= BackwardLength || TangentDistance >= ForwardLength)
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the requested radius does not fit the selected corner" });

    const SpatialPoint EnterPoint = Added(Corner, Scaled(Incoming, -TangentDistance));
    const SpatialPoint ExitPoint = Added(Corner, Scaled(Outgoing, TangentDistance));

    const Deliver<SketchCurveName> TrimmedPrior = TrimCurve(Declared, Curves[PriorIndex].Curve, EnterPoint, true);
    const Deliver<SketchCurveName> TrimmedNext = TrimCurve(Declared, Curves[NextIndex].Curve, ExitPoint, false);
    if (!TrimmedPrior || !TrimmedNext)
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "one neighbouring curve could not be trimmed" });

    ProfileSpecification Produced;
    Produced.DeclarePlane(Source.HeldPlane());
    for (std::size_t SourceLoopIndex = 0u; SourceLoopIndex < Source.HeldLoops().size(); ++SourceLoopIndex)
    {
        if (SourceLoopIndex != LoopIndex)
        {
            Produced.DeclareLoop(Source.HeldLoops()[SourceLoopIndex]);
            continue;
        }

        ProfileLoop Loop;
        Loop.Orientation = Source.HeldLoops()[SourceLoopIndex].Orientation;
        for (std::size_t CurveIndex = 0u; CurveIndex < Count; ++CurveIndex)
        {
            if (CurveIndex == PriorIndex)
            {
                Loop.Traversal.push_back({ { TrimmedPrior.Resolve().IssuedIndex }, true });
                if (Chamfer)
                {
                    const SketchCurveName ChamferEdge = Declared.DeclareLine(EnterPoint, ExitPoint);
                    Loop.Traversal.push_back({ { ChamferEdge.IssuedIndex }, true });
                }
                else
                {
                    const SpatialPoint ThroughPoint = { (EnterPoint.Left + ExitPoint.Left + Corner.Left) / 3.0,
                                                       (EnterPoint.Up + ExitPoint.Up + Corner.Up) / 3.0,
                                                       (EnterPoint.Forward + ExitPoint.Forward + Corner.Forward) / 3.0 };
                    const SketchCurveName Arc = Declared.DeclareThreePointArc(EnterPoint, ThroughPoint, ExitPoint);
                    Loop.Traversal.push_back({ { Arc.IssuedIndex }, true });
                }
                continue;
            }
            if (CurveIndex == NextIndex)
            {
                Loop.Traversal.push_back({ { TrimmedNext.Resolve().IssuedIndex }, true });
                continue;
            }
            Loop.Traversal.push_back(Source.HeldLoops()[SourceLoopIndex].Traversal[CurveIndex]);
        }
        Produced.DeclareLoop(Loop);
    }

    return Deliver<ProfileNameInFeature>::Result(Declared.DeclareProfile(Produced));
}

} // namespace Slate
