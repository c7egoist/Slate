//============================================================================================================================================
//                                                  SKETCHRENDERINGPROJECTION.CPP
//============================================================================================================================================

#include "SlateFeature/Sketch/SketchRenderingProjection/Api/SketchRenderingProjection.h"

#include "SlateFeature/Sketch/SketchPolyline/Api/SketchPolyline.h"
#include "SlateFeature/Sketch/SketchSelection/Api/SketchSelection.h"

#include <algorithm>
#include <cmath>

namespace Slate
{

namespace
{

struct PlanarBasis
{
    SpatialPoint Origin = {};
    SpatialDirection Along = { 1.0, 0.0, 0.0 };
    SpatialDirection Across = { 0.0, 0.0, 1.0 };
};

struct PlanarVertex
{
    Real32 Along = 0.0f;
    Real32 Across = 0.0f;
};

struct FillCandidate
{
    std::vector<PlanarVertex> Outline = {};
    bool Inner = false;
};

constexpr double LengthSquared(const SpatialDirection& Direction)
{
    return Direction.Left * Direction.Left + Direction.Up * Direction.Up + Direction.Forward * Direction.Forward;
}

SpatialDirection Normalize(const SpatialDirection& Direction)
{
    const double Length = std::sqrt(LengthSquared(Direction));
    return Length > 0.0 ? SpatialDirection{ Direction.Left / Length,
                                            Direction.Up / Length,
                                            Direction.Forward / Length }
                        : SpatialDirection{ 1.0, 0.0, 0.0 };
}

SpatialDirection Difference(const SpatialPoint& LeftPoint,
                            const SpatialPoint& RightPoint)
{
    return { RightPoint.Left - LeftPoint.Left,
             RightPoint.Up - LeftPoint.Up,
             RightPoint.Forward - LeftPoint.Forward };
}

SpatialDirection Cross(const SpatialDirection& LeftDirection,
                       const SpatialDirection& RightDirection)
{
    return {
        LeftDirection.Up * RightDirection.Forward - LeftDirection.Forward * RightDirection.Up,
        LeftDirection.Forward * RightDirection.Left - LeftDirection.Left * RightDirection.Forward,
        LeftDirection.Left * RightDirection.Up - LeftDirection.Up * RightDirection.Left
    };
}

constexpr double Dot(const SpatialDirection& LeftDirection,
                     const SpatialDirection& RightDirection)
{
    return LeftDirection.Left * RightDirection.Left
         + LeftDirection.Up * RightDirection.Up
         + LeftDirection.Forward * RightDirection.Forward;
}

PlanarBasis ResolvePlanarBasis(const SketchPlane& Plane)
{
    const SpatialDirection Along = Normalize(Plane.AlongDirection);
    const SpatialDirection Across = Normalize(Cross(Plane.Normal, Along));
    return { Plane.Origin, Along, Across };
}

PlanarVertex ProjectVertex(const PlanarBasis& Basis, const SpatialPoint& Position)
{
    const SpatialDirection Offset = Difference(Basis.Origin, Position);
    return { static_cast<Real32>(Dot(Offset, Basis.Along)),
             static_cast<Real32>(Dot(Offset, Basis.Across)) };
}

bool SameVertex(const PlanarVertex& LeftPoint, const PlanarVertex& RightPoint)
{
    const double Along = static_cast<double>(RightPoint.Along) - static_cast<double>(LeftPoint.Along);
    const double Across = static_cast<double>(RightPoint.Across) - static_cast<double>(LeftPoint.Across);
    return Along * Along + Across * Across <= 1.0e-12;
}

void AppendPolylineSegments(const std::vector<PlanarVertex>& Polyline,
                            Unsigned32 Packed,
                            Real32 Thickness,
                            WorkspaceCadPacket& Delivered)
{
    if (Polyline.size() < 2u)
        return;

    for (std::size_t Index = 0u; Index + 1u < Polyline.size(); ++Index)
    {
        Delivered.AddSegment(Polyline[Index].Along, Polyline[Index].Across,
                             Polyline[Index + 1u].Along, Polyline[Index + 1u].Across,
                             Packed, Thickness);
    }
}

bool ResolveCurvePolyline(const SketchStructure& Sketch,
                          SketchCurveName Subject,
                          const PlanarBasis& Basis,
                          Unsigned32 StepCount,
                          std::vector<PlanarVertex>& Delivered)
{
    Delivered.clear();
    if (!Subject.Assigned() || Subject.IssuedIndex > Sketch.Curves().size())
        return false;

    std::vector<SpatialPoint> SpatialPolyline;
    AppendCurvePolyline(Sketch.Curves()[Subject.IssuedIndex - 1u].Geometry, SpatialPolyline, StepCount);
    if (SpatialPolyline.size() < 2u)
        return false;

    Delivered.reserve(SpatialPolyline.size());
    for (const SpatialPoint& Position : SpatialPolyline)
        Delivered.push_back(ProjectVertex(Basis, Position));
    return true;
}

bool ResolveSketchPointPosition(const SketchStructure& Sketch,
                                SketchPointName Subject,
                                SpatialPoint& Delivered)
{
    if (!Subject.Assigned())
        return false;

    const std::uint32_t CurveIndex = Subject.IssuedIndex >> 8u;
    if (CurveIndex == 0u)
        return false;

    std::vector<SketchPointPlacement> Points;
    if (!ResolveSketchPoints(Sketch, { CurveIndex }, Points))
        return false;

    for (const SketchPointPlacement& Current : Points)
        if (Current.Name.IssuedIndex == Subject.IssuedIndex)
        {
            Delivered = Current.Position;
            return true;
        }

    return false;
}

bool ResolveReferenceAnchor(const SketchStructure& Sketch,
                            const ReferenceSpecification& Reference,
                            const PlanarBasis& Basis,
                            PlanarVertex& Delivered)
{
    if (!Reference.Declared())
        return false;

    if (Reference.SketchPoint.Assigned())
    {
        SpatialPoint Position = {};
        if (!ResolveSketchPointPosition(Sketch, Reference.SketchPoint, Position))
            return false;
        Delivered = ProjectVertex(Basis, Position);
        return true;
    }

    if (Reference.SketchCurve.Assigned())
    {
        std::vector<PlanarVertex> Polyline;
        if (!ResolveCurvePolyline(Sketch, Reference.SketchCurve, Basis, 48u, Polyline) || Polyline.empty())
            return false;
        Delivered = Polyline[Polyline.size() / 2u];
        return true;
    }

    if (Reference.Profile.Assigned() && Reference.Profile.IssuedIndex <= Sketch.Profiles().size())
    {
        const ProfileSpecification& Profile = Sketch.Profiles()[Reference.Profile.IssuedIndex - 1u];
        if (Profile.HeldLoops().empty())
            return false;
        const ProfileLoop& Loop = Profile.HeldLoops().front();
        if (Loop.Traversal.empty())
            return false;

        std::vector<PlanarVertex> Polyline;
        if (!ResolveCurvePolyline(Sketch, { Loop.Traversal.front().TraversedCurve.IssuedIndex }, Basis, 48u, Polyline)
            || Polyline.empty())
            return false;

        Delivered = Polyline.front();
        return true;
    }

    return false;
}

bool ResolveDimensionAnchor(const SketchStructure& Sketch,
                            DimensionName Subject,
                            const PlanarBasis& Basis,
                            PlanarVertex& Delivered)
{
    if (!Subject.Assigned() || Subject.IssuedIndex > Sketch.Dimensions().size())
        return false;

    const DimensionSpecification& Dimension = Sketch.Dimensions()[Subject.IssuedIndex - 1u];
    PlanarVertex Primary = {};
    PlanarVertex Secondary = {};
    const bool HasPrimary = ResolveReferenceAnchor(Sketch, Dimension.Primary, Basis, Primary);
    const bool HasSecondary = ResolveReferenceAnchor(Sketch, Dimension.Secondary, Basis, Secondary);

    if (HasPrimary && HasSecondary)
    {
        Delivered = { (Primary.Along + Secondary.Along) * 0.5f,
                      (Primary.Across + Secondary.Across) * 0.5f };
        return true;
    }

    if (HasPrimary)
    {
        Delivered = Primary;
        return true;
    }

    if (HasSecondary)
    {
        Delivered = Secondary;
        return true;
    }

    return false;
}

bool ResolveConstraintAnchor(const SketchStructure& Sketch,
                             ConstraintName Subject,
                             const PlanarBasis& Basis,
                             PlanarVertex& Delivered)
{
    if (!Subject.Assigned() || Subject.IssuedIndex > Sketch.Constraints().size())
        return false;

    const ConstraintSpecification& Constraint = Sketch.Constraints()[Subject.IssuedIndex - 1u];
    PlanarVertex Primary = {};
    PlanarVertex Secondary = {};
    const bool HasPrimary = ResolveReferenceAnchor(Sketch, Constraint.Primary, Basis, Primary);
    const bool HasSecondary = ResolveReferenceAnchor(Sketch, Constraint.Secondary, Basis, Secondary);

    if (HasPrimary && HasSecondary)
    {
        Delivered = { (Primary.Along + Secondary.Along) * 0.5f,
                      (Primary.Across + Secondary.Across) * 0.5f };
        return true;
    }

    if (HasPrimary)
    {
        Delivered = Primary;
        return true;
    }

    if (HasSecondary)
    {
        Delivered = Secondary;
        return true;
    }

    return false;
}

double SignedArea(const std::vector<PlanarVertex>& Outline)
{
    if (Outline.size() < 3u)
        return 0.0;

    double Sum = 0.0;
    for (std::size_t Index = 0u; Index < Outline.size(); ++Index)
    {
        const std::size_t NextIndex = (Index + 1u) % Outline.size();
        Sum += static_cast<double>(Outline[Index].Along) * static_cast<double>(Outline[NextIndex].Across)
             - static_cast<double>(Outline[NextIndex].Along) * static_cast<double>(Outline[Index].Across);
    }
    return Sum * 0.5;
}

bool ConvexOutline(const std::vector<PlanarVertex>& Outline)
{
    if (Outline.size() < 3u)
        return false;

    double Sign = 0.0;
    for (std::size_t Index = 0u; Index < Outline.size(); ++Index)
    {
        const std::size_t NextIndex = (Index + 1u) % Outline.size();
        const std::size_t ThirdIndex = (Index + 2u) % Outline.size();
        const double Along0 = static_cast<double>(Outline[NextIndex].Along) - static_cast<double>(Outline[Index].Along);
        const double Across0 = static_cast<double>(Outline[NextIndex].Across) - static_cast<double>(Outline[Index].Across);
        const double Along1 = static_cast<double>(Outline[ThirdIndex].Along) - static_cast<double>(Outline[NextIndex].Along);
        const double Across1 = static_cast<double>(Outline[ThirdIndex].Across) - static_cast<double>(Outline[NextIndex].Across);
        const double Crossed = Along0 * Across1 - Across0 * Along1;
        if (std::fabs(Crossed) <= 1.0e-8)
            continue;
        if (Sign == 0.0)
            Sign = Crossed;
        else if ((Crossed > 0.0) != (Sign > 0.0))
            return false;
    }
    return Sign != 0.0;
}

bool ResolveLoopOutline(const SketchStructure& Sketch,
                        const ProfileLoop& Loop,
                        const PlanarBasis& Basis,
                        Unsigned32 StepCount,
                        std::vector<PlanarVertex>& Delivered)
{
    Delivered.clear();
    if (Loop.Traversal.empty())
        return false;

    std::vector<PlanarVertex> CurvePolyline;
    for (const ProfileCurveUse& Use : Loop.Traversal)
    {
        if (!ResolveCurvePolyline(Sketch, { Use.TraversedCurve.IssuedIndex }, Basis, StepCount, CurvePolyline))
            return false;

        if (!Use.SameSense)
            std::reverse(CurvePolyline.begin(), CurvePolyline.end());

        if (!Delivered.empty() && !CurvePolyline.empty() && SameVertex(Delivered.back(), CurvePolyline.front()))
            Delivered.insert(Delivered.end(), CurvePolyline.begin() + 1u, CurvePolyline.end());
        else
            Delivered.insert(Delivered.end(), CurvePolyline.begin(), CurvePolyline.end());
    }

    if (Delivered.size() >= 2u && SameVertex(Delivered.front(), Delivered.back()))
        Delivered.pop_back();

    return Delivered.size() >= 3u;
}

void AppendProfileWire(const SketchStructure& Sketch,
                       ProfileNameInFeature Subject,
                       const PlanarBasis& Basis,
                       const SketchRenderingStyle& Style,
                       Unsigned32 Packed,
                       WorkspaceCadPacket& Delivered)
{
    if (!Subject.Assigned() || Subject.IssuedIndex > Sketch.Profiles().size())
        return;

    const ProfileSpecification& Profile = Sketch.Profiles()[Subject.IssuedIndex - 1u];
    std::vector<PlanarVertex> Outline;
    for (const ProfileLoop& Loop : Profile.HeldLoops())
        if (ResolveLoopOutline(Sketch, Loop, Basis, Style.CurveSteps, Outline))
        {
            AppendPolylineSegments(Outline, Packed, Style.ProfileThickness, Delivered);
            if (Outline.size() >= 2u)
                Delivered.AddSegment(Outline.back().Along, Outline.back().Across,
                                     Outline.front().Along, Outline.front().Across,
                                     Packed, Style.ProfileThickness);
        }
}

void AppendProfileFill(const SketchStructure& Sketch,
                       ProfileNameInFeature Subject,
                       const PlanarBasis& Basis,
                       Unsigned32 StepCount,
                       Unsigned32 Packed,
                       WorkspaceCadPacket& Delivered)
{
    if (!Subject.Assigned() || Subject.IssuedIndex > Sketch.Profiles().size())
        return;

    const ProfileSpecification& Profile = Sketch.Profiles()[Subject.IssuedIndex - 1u];
    if (Profile.HeldLoops().empty())
        return;

    std::vector<FillCandidate> Candidates;
    Candidates.reserve(Profile.HeldLoops().size());

    for (const ProfileLoop& Loop : Profile.HeldLoops())
    {
        FillCandidate Candidate = {};
        Candidate.Inner = Loop.Orientation == ProfileLoopOrientation::Inner;
        if (!ResolveLoopOutline(Sketch, Loop, Basis, StepCount, Candidate.Outline))
            return;
        Candidates.push_back(std::move(Candidate));
    }

    if (Candidates.size() != 1u || Candidates.front().Inner)
        return;

    std::vector<PlanarVertex>& Outline = Candidates.front().Outline;
    if (!ConvexOutline(Outline))
        return;

    if (SignedArea(Outline) < 0.0)
        std::reverse(Outline.begin(), Outline.end());

    for (std::size_t Index = 1u; Index + 1u < Outline.size(); ++Index)
        Delivered.AddFill(Outline[0u].Along, Outline[0u].Across,
                          Outline[Index].Along, Outline[Index].Across,
                          Outline[Index + 1u].Along, Outline[Index + 1u].Across,
                          Packed);
}

} // namespace

Deliver<bool> ProjectSketchRendering(const SketchStructure& Sketch,
                                     const WorkspaceRecordStructure& Records,
                                     WorkspaceCadPacket& Delivered,
                                     const SketchRenderingStyle& Style)
{
    Delivered.Reset();

    if (!Sketch.Declared())
        return Deliver<bool>::Refuse({ RefusalReason::ContentUnsupported,
                                       "the sketch declarations are incomplete" });

    const PlanarBasis Basis = ResolvePlanarBasis(Sketch.HeldPlane());
    std::vector<PlanarVertex> Polyline;

    for (std::uint32_t RecordIndex = 1u; RecordIndex <= Records.DeclaredCount(); ++RecordIndex)
    {
        const WorkspaceRecord* Record = Records.Resolve({ RecordIndex });
        if (Record == nullptr || !Record->Visible)
            continue;

        switch (Record->Subject)
        {
            case WorkspaceRecordSubject::Point:
            {
                SpatialPoint Position = {};
                if (ResolveSketchPointPosition(Sketch, Record->SketchPoint, Position))
                {
                    const PlanarVertex Projected = ProjectVertex(Basis, Position);
                    Delivered.AddMarker(Projected.Along, Projected.Across,
                                        Style.PointColour, Style.PointRadius,
                                        WorkspaceCadMarkerSubject::SketchPoint);
                }
                break;
            }

            case WorkspaceRecordSubject::OpenCurve:
                if (ResolveCurvePolyline(Sketch, Record->SketchCurve, Basis, Style.CurveSteps, Polyline))
                    AppendPolylineSegments(Polyline,
                                           Record->ConstructionSemantic ? Style.ConstructionCurveColour : Style.SketchCurveColour,
                                           Record->ConstructionSemantic ? Style.ConstructionThickness : Style.CurveThickness,
                                           Delivered);
                break;

            case WorkspaceRecordSubject::ClosedProfile:
                AppendProfileFill(Sketch, Record->Profile, Basis, Style.CurveSteps,
                                  Style.ProfileFillColour, Delivered);
                AppendProfileWire(Sketch, Record->Profile, Basis, Style,
                                  Style.ProfileCurveColour, Delivered);
                break;

            case WorkspaceRecordSubject::ThinSurface:
                AppendProfileFill(Sketch, Record->Profile, Basis, Style.CurveSteps,
                                  Style.SurfaceFillColour, Delivered);
                AppendProfileWire(Sketch, Record->Profile, Basis, Style,
                                  Style.SurfaceCurveColour, Delivered);
                break;

            case WorkspaceRecordSubject::Solid:
                AppendProfileFill(Sketch, Record->Profile, Basis, Style.CurveSteps,
                                  Style.SolidFillColour, Delivered);
                AppendProfileWire(Sketch, Record->Profile, Basis, Style,
                                  Style.SolidCurveColour, Delivered);
                break;

            case WorkspaceRecordSubject::Dimension:
            {
                PlanarVertex Anchor = {};
                if (ResolveDimensionAnchor(Sketch, Record->Dimension, Basis, Anchor))
                    Delivered.AddMarker(Anchor.Along, Anchor.Across,
                                        Style.DimensionColour, Style.DimensionRadius,
                                        WorkspaceCadMarkerSubject::Dimension);
                break;
            }

            case WorkspaceRecordSubject::Constraint:
            {
                PlanarVertex Anchor = {};
                if (ResolveConstraintAnchor(Sketch, Record->Constraint, Basis, Anchor))
                    Delivered.AddMarker(Anchor.Along, Anchor.Across,
                                        Style.ConstraintColour, Style.ConstraintRadius,
                                        WorkspaceCadMarkerSubject::Constraint);
                break;
            }

            case WorkspaceRecordSubject::Pattern:
            case WorkspaceRecordSubject::Mirror:
            case WorkspaceRecordSubject::Folder:
            case WorkspaceRecordSubject::SubjectCount:
                break;
        }
    }

    return Deliver<bool>::Result(true);
}

} // namespace Slate
