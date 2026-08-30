//============================================================================================================================================
//                                                    WORLDSKETCHANALYSIS.CPP
//============================================================================================================================================

#include "SlateShape/World/WorldSketchAnalysis/Api/WorldSketchAnalysis.h"

#include "SlateShape/Sketch/SketchPolyline/Api/SketchPolyline.h"

#include <algorithm>
#include <cmath>

namespace Slate
{

namespace
{
    bool SamePoint(const SpatialPoint& Left,
                   const SpatialPoint& Right,
                   double Tolerance)
    {
        return LengthSquared(Difference(Left, Right)) <= Tolerance * Tolerance;
    }

    bool ResolveCurvePolyline(const WorldSketchStructure& Declared,
                              WorldCurveName Subject,
                              std::uint32_t StepFloor,
                              std::vector<SpatialPoint>& Delivered)
    {
        Delivered.clear();
        const DeclaredWorldCurve* Held = Declared.Resolve(Subject);
        if (Held == nullptr || !Held->Geometry.Declared())
            return false;

        AppendCurvePolyline(Held->Geometry, Delivered,
                            ResolveCurveStepCount(Held->Geometry, std::max(StepFloor, 2u)));
        return Delivered.size() >= 2u;
    }

    bool ResolveLoopOutline(const WorldSketchStructure& Declared,
                            WorldLoopName Subject,
                            std::uint32_t StepFloor,
                            double ClosureTolerance,
                            std::vector<SpatialPoint>& Outline,
                            bool& Closed,
                            std::vector<WorldLoopIssue>& Issues)
    {
        Outline.clear();
        Closed = false;

        const DeclaredWorldLoop* Held = Declared.Resolve(Subject);
        if (Held == nullptr || Held->Traversal.empty())
            return false;

        SpatialPoint FirstStart = {};
        SpatialPoint PreviousEnd = {};
        WorldCurveName PreviousCurve = {};
        bool FirstCurve = true;
        bool Connected = true;

        std::vector<SpatialPoint> Polyline;
        for (const WorldCurveUse& Use : Held->Traversal)
        {
            if (!ResolveCurvePolyline(Declared, Use.TraversedCurve, StepFloor, Polyline))
            {
                Issues.push_back({ Subject, WorldLoopIssueSubject::MissingCurve,
                                   Use.TraversedCurve, {}, {}, {}, 0.0 });
                Outline.clear();
                Closed = false;
                return false;
            }

            if (!Use.SameSense)
                std::reverse(Polyline.begin(), Polyline.end());

            if (FirstCurve)
            {
                FirstStart = Polyline.front();
                Outline = Polyline;
                FirstCurve = false;
            }
            else
            {
                const double GapDistance = std::sqrt(LengthSquared(Difference(PreviousEnd, Polyline.front())));
                if (!SamePoint(PreviousEnd, Polyline.front(), ClosureTolerance))
                {
                    Connected = false;
                    Issues.push_back({ Subject, WorldLoopIssueSubject::Gap,
                                       PreviousCurve, Use.TraversedCurve,
                                       PreviousEnd, Polyline.front(), GapDistance });
                }

                if (!Outline.empty() && SamePoint(Outline.back(), Polyline.front(), ClosureTolerance))
                    Outline.insert(Outline.end(), Polyline.begin() + 1u, Polyline.end());
                else
                    Outline.insert(Outline.end(), Polyline.begin(), Polyline.end());
            }

            PreviousEnd = Polyline.back();
            PreviousCurve = Use.TraversedCurve;
        }

        if (FirstCurve)
            return false;

        if (!Outline.empty() && SamePoint(Outline.front(), Outline.back(), ClosureTolerance))
            Outline.pop_back();

        Closed = Connected && SamePoint(PreviousEnd, FirstStart, ClosureTolerance);
        if (!Closed)
        {
            Issues.push_back({ Subject, WorldLoopIssueSubject::OpenLoop,
                               PreviousCurve, Held->Traversal.front().TraversedCurve,
                               PreviousEnd, FirstStart,
                               std::sqrt(LengthSquared(Difference(PreviousEnd, FirstStart))) });
        }

        return !Outline.empty();
    }

    bool ResolveSupportFrameFromOutline(const std::vector<SpatialPoint>& Outline,
                                        WorldPlacementFrame& Delivered,
                                        double& MaximumDeviation)
    {
        Delivered = {};
        MaximumDeviation = 0.0;
        if (Outline.size() < 3u)
            return false;

        for (std::size_t OriginIndex = 0u; OriginIndex + 2u < Outline.size(); ++OriginIndex)
        {
            for (std::size_t AlongIndex = OriginIndex + 1u; AlongIndex + 1u < Outline.size(); ++AlongIndex)
            {
                const SpatialDirection Along = Difference(Outline[OriginIndex], Outline[AlongIndex]);
                if (LengthSquared(Along) <= 1.0e-18)
                    continue;

                for (std::size_t ThirdIndex = AlongIndex + 1u; ThirdIndex < Outline.size(); ++ThirdIndex)
                {
                    const SpatialDirection Across = Difference(Outline[OriginIndex], Outline[ThirdIndex]);
                    const SpatialDirection RawNormal = Cross(Along, Across);
                    if (LengthSquared(RawNormal) <= 1.0e-18)
                        continue;

                    const SpatialDirection Normal = Normalize(RawNormal);
                    SpatialDirection AlongDirection = Added(Along, Scaled(Normal, -Dot(Along, Normal)));
                    if (LengthSquared(AlongDirection) <= 1.0e-18)
                        continue;

                    Delivered.Origin = Outline[OriginIndex];
                    Delivered.Normal = Normal;
                    Delivered.AlongDirection = Normalize(AlongDirection);

                    for (const SpatialPoint& Point : Outline)
                        MaximumDeviation = std::max(MaximumDeviation,
                                                    std::fabs(Dot(Normal, Difference(Delivered.Origin, Point))));
                    return Delivered.Declared();
                }
            }
        }

        return false;
    }
}

WorldSketchAnalysis AnalyzeWorldSketch(const WorldSketchStructure& Declared,
                                     std::uint32_t StepFloor,
                                     double ClosureTolerance,
                                     double CoplanarTolerance)
{
    WorldSketchAnalysis Analysis;
    Analysis.Loops.reserve(Declared.LoopCount());

    for (std::uint32_t LoopIndex = 1u; LoopIndex <= Declared.LoopCount(); ++LoopIndex)
    {
        WorldLoopAnalysisRecord Record = {};
        Record.Loop = { LoopIndex };

        if (ResolveLoopOutline(Declared, Record.Loop, StepFloor, ClosureTolerance,
                               Record.Outline, Record.Closed, Analysis.Issues) &&
            Record.Closed)
        {
            if (ResolveSupportFrameFromOutline(Record.Outline, Record.SupportFrame,
                                               Record.MaximumDeviation))
            {
                Record.Coplanar = Record.MaximumDeviation <= CoplanarTolerance;
                Record.FillEligible = Record.Coplanar;
                if (!Record.Coplanar)
                {
                    Analysis.Issues.push_back({ Record.Loop, WorldLoopIssueSubject::NonCoplanar,
                                               {}, {}, Record.SupportFrame.Origin, {},
                                               Record.MaximumDeviation });
                }
            }
            else
            {
                Analysis.Issues.push_back({ Record.Loop, WorldLoopIssueSubject::DegeneratePlane,
                                           {}, {}, {}, {}, 0.0 });
            }
        }

        Analysis.Loops.push_back(std::move(Record));
    }

    return Analysis;
}

Deliver<ProfileSpecification> ResolvePlanarWorldLoopProfile(const WorldSketchStructure& Declared,
                                                            WorldLoopName Subject,
                                                            std::uint32_t StepFloor,
                                                            double ClosureTolerance,
                                                            double CoplanarTolerance)
{
    if (!Subject.Assigned() || Subject.IssuedIndex > Declared.LoopCount())
        return Deliver<ProfileSpecification>::Refuse(
            { RefusalReason::ContentUnsupported, "the world loop is not declared" });

    const DeclaredWorldLoop* Held = Declared.Resolve(Subject);
    if (Held == nullptr)
        return Deliver<ProfileSpecification>::Refuse(
            { RefusalReason::ContentUnsupported, "the world loop could not be resolved" });

    const WorldSketchAnalysis Analysis = AnalyzeWorldSketch(Declared, StepFloor,
                                                          ClosureTolerance, CoplanarTolerance);
    const WorldLoopAnalysisRecord* Derived = nullptr;
    for (const WorldLoopAnalysisRecord& Candidate : Analysis.Loops)
        if (Candidate.Loop.IssuedIndex == Subject.IssuedIndex)
        {
            Derived = &Candidate;
            break;
        }

    if (Derived == nullptr || !Derived->Closed)
        return Deliver<ProfileSpecification>::Refuse(
            { RefusalReason::ContentUnsupported, "the world loop is not closed" });
    if (!Derived->Coplanar || !Derived->SupportFrame.Declared())
        return Deliver<ProfileSpecification>::Refuse(
            { RefusalReason::ContentUnsupported, "the world loop is not planar enough for a profile" });

    ProfileSpecification Profile;
    Profile.DeclarePlane({ Derived->SupportFrame.Origin,
                           Derived->SupportFrame.Normal,
                           Derived->SupportFrame.AlongDirection });

    ProfileLoop Loop;
    Loop.Orientation = ProfileLoopOrientation::Outer;
    Loop.Traversal.reserve(Held->Traversal.size());
    for (const WorldCurveUse& Use : Held->Traversal)
        Loop.Traversal.push_back({ { Use.TraversedCurve.IssuedIndex }, Use.SameSense });
    Profile.DeclareLoop(Loop);

    if (!Profile.Declared())
        return Deliver<ProfileSpecification>::Refuse(
            { RefusalReason::ContentUnsupported, "the planar world loop could not become a profile" });

    return Deliver<ProfileSpecification>::Result(Profile);
}

} // namespace Slate
