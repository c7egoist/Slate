//============================================================================================================================================
//                                             WORLDSKETCHRENDERINGPROJECTION.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/WorldSketchRenderingProjection/Api/WorldSketchRenderingProjection.h"

#include "Shared/WorkspaceCadNearClip.slang.h"
#include "SlateShape/Sketch/SketchPolyline/Api/SketchPolyline.h"
#include "SlateShape/World/WorldSketchAnalysis/Api/WorldSketchAnalysis.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Slate
{

namespace
{

struct PlanarVertex
{
    double Along = 0.0;
    double Across = 0.0;
    SpatialPoint World = {};
};

WorkspaceCadProjectedPoint ResolveProjectedPoint(const ResolvedCamera& Camera,
                                                 const PlaneExtent& Extent,
                                                 const SpatialPoint& Position)
{
    WorkspaceCadProjectedPoint Point = {};
    if (!Camera.Perspective)
    {
        float ScreenX = 0.0f;
        float ScreenY = 0.0f;
        static_cast<void>(ProjectFromCamera(Camera, Extent, Position, ScreenX, ScreenY));
        Point.X = ScreenX;
        Point.Y = ScreenY;
        Point.W = 1.0f;
        return Point;
    }

    const SpatialDirection EyeToPoint = Difference(Camera.Frame.Eye, Position);
    const double CameraX = Dot(EyeToPoint, Camera.Frame.Right);
    const double CameraY = Dot(EyeToPoint, Camera.Frame.Up);
    const double CameraZ = Dot(EyeToPoint, Camera.Frame.Forward);
    const double TanHalf = std::tan(Camera.FieldOfViewDegrees * 0.5 * ProjectionPi / 180.0);
    const double Focal = (Extent.Height() * 0.5) / std::max(TanHalf, 1.0e-6);
    const double CentreX = Extent.MinimumX + Extent.Width() * 0.5;
    const double CentreY = Extent.MinimumY + Extent.Height() * 0.5;

    Point.X = static_cast<Real32>(CentreX * CameraZ + Focal * CameraX);
    Point.Y = static_cast<Real32>(CentreY * CameraZ - Focal * CameraY);
    Point.W = static_cast<Real32>(CameraZ);
    return Point;
}

void AppendClippedSegment(const ResolvedCamera& Camera,
                          const PlaneExtent& Extent,
                          const SpatialPoint& Start,
                          const SpatialPoint& End,
                          Unsigned32 Packed,
                          Real32 Thickness,
                          WorkspaceCadPacket& Delivered)
{
    WorkspaceCadProjectedPoint First = ResolveProjectedPoint(Camera, Extent, Start);
    WorkspaceCadProjectedPoint Second = ResolveProjectedPoint(Camera, Extent, End);
    if (Camera.Perspective && !ClipWorkspaceCadSegmentNear(First, Second))
        return;

    const WorkspaceCadScreenPoint A = ResolveWorkspaceCadScreenPoint(First);
    const WorkspaceCadScreenPoint B = ResolveWorkspaceCadScreenPoint(Second);
    Delivered.AddSegment(A.X, A.Y, B.X, B.Y, Packed, Thickness);
}

void AppendClippedTriangle(const ResolvedCamera& Camera,
                           const PlaneExtent& Extent,
                           const SpatialPoint& A,
                           const SpatialPoint& B,
                           const SpatialPoint& C,
                           Unsigned32 Packed,
                           WorkspaceCadPacket& Delivered)
{
    WorkspaceCadProjectedPoint First = ResolveProjectedPoint(Camera, Extent, A);
    WorkspaceCadProjectedPoint Second = ResolveProjectedPoint(Camera, Extent, B);
    WorkspaceCadProjectedPoint Third = ResolveProjectedPoint(Camera, Extent, C);

    if (!Camera.Perspective)
    {
        const WorkspaceCadScreenPoint ScreenA = ResolveWorkspaceCadScreenPoint(First);
        const WorkspaceCadScreenPoint ScreenB = ResolveWorkspaceCadScreenPoint(Second);
        const WorkspaceCadScreenPoint ScreenC = ResolveWorkspaceCadScreenPoint(Third);
        Delivered.AddFill(ScreenA.X, ScreenA.Y, ScreenB.X, ScreenB.Y, ScreenC.X, ScreenC.Y, Packed);
        return;
    }

    WorkspaceCadProjectedPoint Clipped[4] = {};
    const Unsigned32 Count = ClipWorkspaceCadFillTriangleNear(First, Second, Third, Clipped);
    if (Count == 3u)
    {
        const WorkspaceCadScreenPoint ScreenA = ResolveWorkspaceCadScreenPoint(Clipped[0u]);
        const WorkspaceCadScreenPoint ScreenB = ResolveWorkspaceCadScreenPoint(Clipped[1u]);
        const WorkspaceCadScreenPoint ScreenC = ResolveWorkspaceCadScreenPoint(Clipped[2u]);
        Delivered.AddFill(ScreenA.X, ScreenA.Y, ScreenB.X, ScreenB.Y, ScreenC.X, ScreenC.Y, Packed);
        return;
    }
    if (Count == 4u)
    {
        const WorkspaceCadScreenPoint ScreenA = ResolveWorkspaceCadScreenPoint(Clipped[0u]);
        const WorkspaceCadScreenPoint ScreenB = ResolveWorkspaceCadScreenPoint(Clipped[1u]);
        const WorkspaceCadScreenPoint ScreenC = ResolveWorkspaceCadScreenPoint(Clipped[2u]);
        const WorkspaceCadScreenPoint ScreenD = ResolveWorkspaceCadScreenPoint(Clipped[3u]);
        Delivered.AddFill(ScreenA.X, ScreenA.Y, ScreenB.X, ScreenB.Y, ScreenC.X, ScreenC.Y, Packed);
        Delivered.AddFill(ScreenA.X, ScreenA.Y, ScreenC.X, ScreenC.Y, ScreenD.X, ScreenD.Y, Packed);
    }
}

bool ResolveCurvePolyline(const WorldSketchStructure& Declared,
                          WorldCurveName Subject,
                          Unsigned32 StepFloor,
                          std::vector<SpatialPoint>& Delivered)
{
    Delivered.clear();
    const DeclaredWorldCurve* Held = Declared.Resolve(Subject);
    if (Held == nullptr || !Held->Geometry.Declared())
        return false;

    AppendCurvePolyline(Held->Geometry, Delivered, ResolveCurveStepCount(Held->Geometry, StepFloor));
    return Delivered.size() >= 2u;
}

double SignedArea(const std::vector<PlanarVertex>& Outline)
{
    if (Outline.size() < 3u)
        return 0.0;

    double Sum = 0.0;
    for (std::size_t Index = 0u; Index < Outline.size(); ++Index)
    {
        const std::size_t Next = (Index + 1u) % Outline.size();
        Sum += Outline[Index].Along * Outline[Next].Across
             - Outline[Next].Along * Outline[Index].Across;
    }
    return Sum * 0.5;
}

double TurnOf(const PlanarVertex& A, const PlanarVertex& B, const PlanarVertex& C)
{
    return (B.Along - A.Along) * (C.Across - A.Across)
         - (B.Across - A.Across) * (C.Along - A.Along);
}

bool WithinTriangle(const PlanarVertex& A,
                    const PlanarVertex& B,
                    const PlanarVertex& C,
                    const PlanarVertex& Point)
{
    const double First = TurnOf(A, B, Point);
    const double Second = TurnOf(B, C, Point);
    const double Third = TurnOf(C, A, Point);
    const bool AnyNegative = First < 0.0 || Second < 0.0 || Third < 0.0;
    const bool AnyPositive = First > 0.0 || Second > 0.0 || Third > 0.0;
    return !(AnyNegative && AnyPositive);
}

bool ClipEars(const std::vector<PlanarVertex>& Outline,
              std::vector<Unsigned32>& Delivered)
{
    Delivered.clear();
    if (Outline.size() < 3u)
        return false;

    std::vector<Unsigned32> Remaining;
    Remaining.reserve(Outline.size());
    for (Unsigned32 Index = 0u; Index < static_cast<Unsigned32>(Outline.size()); ++Index)
        Remaining.push_back(Index);

    std::size_t Attempts = Remaining.size() * Remaining.size() + 4u;
    while (Remaining.size() > 3u && Attempts-- > 0u)
    {
        bool Clipped = false;
        for (std::size_t Position = 0u; Position < Remaining.size(); ++Position)
        {
            const Unsigned32 Previous = Remaining[(Position + Remaining.size() - 1u) % Remaining.size()];
            const Unsigned32 Current = Remaining[Position];
            const Unsigned32 Next = Remaining[(Position + 1u) % Remaining.size()];
            if (TurnOf(Outline[Previous], Outline[Current], Outline[Next]) <= 0.0)
                continue;

            bool Swallows = false;
            for (const Unsigned32 Other : Remaining)
            {
                if (Other == Previous || Other == Current || Other == Next)
                    continue;
                if (WithinTriangle(Outline[Previous], Outline[Current], Outline[Next], Outline[Other]))
                {
                    Swallows = true;
                    break;
                }
            }
            if (Swallows)
                continue;

            Delivered.push_back(Previous);
            Delivered.push_back(Current);
            Delivered.push_back(Next);
            Remaining.erase(Remaining.begin() + static_cast<std::ptrdiff_t>(Position));
            Clipped = true;
            break;
        }

        if (!Clipped)
            return false;
    }

    if (Remaining.size() != 3u)
        return false;

    Delivered.push_back(Remaining[0u]);
    Delivered.push_back(Remaining[1u]);
    Delivered.push_back(Remaining[2u]);
    return true;
}

void AppendFillForLoop(const WorldLoopAnalysisRecord& Loop,
                       const ResolvedCamera& Camera,
                       const PlaneExtent& Extent,
                       Unsigned32 Packed,
                       WorkspaceCadPacket& Delivered)
{
    if (!Loop.FillEligible || !Loop.SupportFrame.Declared() || Loop.Outline.size() < 3u)
        return;

    std::vector<PlanarVertex> Outline;
    Outline.reserve(Loop.Outline.size());
    for (const SpatialPoint& Point : Loop.Outline)
    {
        double Along = 0.0;
        double Across = 0.0;
        ResolveWorldPlacementCoordinates(Loop.SupportFrame, Point, Along, Across);
        Outline.push_back({ Along, Across, Point });
    }

    if (SignedArea(Outline) < 0.0)
        std::reverse(Outline.begin(), Outline.end());

    std::vector<Unsigned32> Triangles;
    if (!ClipEars(Outline, Triangles))
        return;

    for (std::size_t Index = 0u; Index + 2u < Triangles.size(); Index += 3u)
        AppendClippedTriangle(Camera, Extent,
                              Outline[Triangles[Index]].World,
                              Outline[Triangles[Index + 1u]].World,
                              Outline[Triangles[Index + 2u]].World,
                              Packed, Delivered);
}

} // namespace

WorkspaceCadProjection ResolveWorldSketchScreenProjection(std::uint32_t DisplayWidth,
                                                         std::uint32_t DisplayHeight)
{
    WorkspaceCadProjection Projection = {};
    Projection.DisplayWidth = static_cast<float>(DisplayWidth);
    Projection.DisplayHeight = static_cast<float>(DisplayHeight);
    Projection.Projection0[0] = 0.0f;
    Projection.Projection0[1] = 0.0f;
    Projection.Projection0[2] = 0.0f;
    Projection.Projection0[3] = 1.0f;
    Projection.Projection1[0] = 1.0f;
    Projection.Projection1[1] = 0.0f;
    Projection.Projection1[2] = 0.0f;
    Projection.Projection1[3] = 0.0f;
    Projection.Projection2[0] = 0.0f;
    Projection.Projection2[1] = 1.0f;
    Projection.Projection2[2] = 0.0f;
    Projection.Projection2[3] = 0.0f;
    return Projection;
}

Deliver<bool> ProjectWorldSketchRendering(const WorldSketchStructure& Declared,
                                         const ResolvedCamera& Camera,
                                         const PlaneExtent& PhysicalExtent,
                                         WorkspaceCadPacket& Delivered,
                                         const WorldSketchRenderingStyle& Style,
                                         double ClosureTolerance,
                                         double CoplanarTolerance)
{
    Delivered.Reset();

    std::vector<SpatialPoint> Polyline;
    for (std::uint32_t CurveIndex = 1u; CurveIndex <= Declared.CurveCount(); ++CurveIndex)
    {
        if (!ResolveCurvePolyline(Declared, { CurveIndex }, Style.CurveSteps, Polyline))
            continue;

        for (std::size_t PointIndex = 0u; PointIndex + 1u < Polyline.size(); ++PointIndex)
            AppendClippedSegment(Camera, PhysicalExtent,
                                 Polyline[PointIndex], Polyline[PointIndex + 1u],
                                 Style.CurveColour, Style.CurveThickness, Delivered);
    }

    const WorldSketchAnalysis Analysis = AnalyzeWorldSketch(Declared, Style.CurveSteps,
                                                          ClosureTolerance, CoplanarTolerance);
    for (const WorldLoopAnalysisRecord& Loop : Analysis.Loops)
        AppendFillForLoop(Loop, Camera, PhysicalExtent, Style.FillColour, Delivered);

    return Deliver<bool>::Result(true);
}

} // namespace Slate
