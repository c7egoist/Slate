//============================================================================================================================================
//                                                   WORLDDRAFTSKETCHBRIDGE.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/WorldDraftSketchBridge/Api/WorldDraftSketchBridge.h"

#include "Shared/WorkspaceCadNearClip.slang.h"
#include "SlateShape/Sketch/SketchSelection/Api/SketchSelection.h"
#include "SlateShape/Sketch/SketchPolyline/Api/SketchPolyline.h"
#include "SlateShape/World/WorldDraftEditing/Api/WorldDraftEditing.h"
#include "SlateWorkspace/Discipline/PlacementCommit/Api/PlacementCommit.h"
#include "SlateWorkspace/Discipline/RecordDeclaration/Api/RecordDeclaration.h"
#include "SketchToolset/SketchTool/SketchPlacement/Api/SketchPlacement.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Slate
{

namespace
{

WorldPlacementFrame ResolveSketchSupportFrame(const SketchStructure& Sketch)
{
    if (!Sketch.PlaneDeclared())
        return {};

    const SketchPlane& Plane = Sketch.HeldPlane();
    return { Plane.Origin, Plane.Normal, Plane.AlongDirection };
}

bool WorkplaneDeclared(const Workplane& ActiveWorkplane)
{
    return LengthSquared(ActiveWorkplane.Along) > 1.0e-18
        && LengthSquared(ActiveWorkplane.Normal) > 1.0e-18
        && LengthSquared(Cross(Normalize(ActiveWorkplane.Normal), Normalize(ActiveWorkplane.Along))) > 1.0e-18;
}

WorldPlacementFrame ResolveWorkplaneSupportFrame(const Workplane& ActiveWorkplane)
{
    if (!WorkplaneDeclared(ActiveWorkplane))
        return {};

    const SpatialBasis Basis = ResolveWorkplaneBasis(ActiveWorkplane);
    return { Basis.Origin, Basis.Normal, Basis.Along };
}

SketchPlane ResolveSketchPlaneFromWorkplane(const Workplane& ActiveWorkplane)
{
    const SpatialBasis Basis = ResolveWorkplaneBasis(ActiveWorkplane);
    return { Basis.Origin, Basis.Normal, Basis.Along };
}

WorldPlacementFrame ResolveProfileSupportFrame(const ProfileSpecification& Profile)
{
    const ProfilePlane& Plane = Profile.HeldPlane();
    return { Plane.Origin, Plane.Normal, Plane.AlongDirection };
}

bool SketchHasCommittedGeometry(const SketchStructure& Sketch)
{
    return !Sketch.Curves().empty() || !Sketch.Profiles().empty();
}

bool ResolveSketchControlPosition(const SketchStructure& Sketch,
                                  SketchControlName Subject,
                                  SpatialPoint& Position)
{
    if (!Subject.Assigned())
        return false;

    const std::uint32_t CurveIndex = Subject.IssuedIndex >> 12u;
    if (CurveIndex == 0u)
        return false;

    std::vector<SketchControlPlacement> Controls;
    if (!ResolveSketchControls(Sketch, { CurveIndex }, Controls))
        return false;

    for (const SketchControlPlacement& Control : Controls)
        if (Control.Name.IssuedIndex == Subject.IssuedIndex)
        {
            Position = Control.Position;
            return true;
        }

    return false;
}

bool ResolveWorldControlPosition(const WorldDraftStructure& Declared,
                                 WorldControlName Subject,
                                 SpatialPoint& Position)
{
    if (!Subject.Assigned())
        return false;

    const std::uint32_t CurveIndex = Subject.IssuedIndex >> 12u;
    if (CurveIndex == 0u)
        return false;

    std::vector<WorldControlPlacement> Controls;
    if (!ResolveWorldDraftControls(Declared, { CurveIndex }, Controls))
        return false;

    for (const WorldControlPlacement& Control : Controls)
        if (Control.Name.IssuedIndex == Subject.IssuedIndex)
        {
            Position = Control.Position;
            return true;
        }

    return false;
}

WorkspaceCadProjectedPoint ResolveProjectedPoint(const ResolvedCamera& Camera,
                                                 const PlaneExtent& Extent,
                                                 const SpatialPoint& Position)
{
    WorkspaceCadProjectedPoint Point = {};

    if (!Camera.Perspective)
    {
        const SpatialDirection Offset = Difference(Camera.Frame.Eye, Position);
        const double CentreX = Extent.MinimumX + Extent.Width() * 0.5;
        const double CentreY = Extent.MinimumY + Extent.Height() * 0.5;
        Point.X = static_cast<Real32>(CentreX + Dot(Offset, Camera.Frame.Right) * Camera.OrthoScale);
        Point.Y = static_cast<Real32>(CentreY - Dot(Offset, Camera.Frame.Up) * Camera.OrthoScale);
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

bool PlacementClosesOnItself(const std::vector<SpatialPoint>& Anchors)
{
    if (Anchors.size() < 3u)
        return false;

    double Longest = 0.0;
    for (std::size_t Index = 0u; Index + 1u < Anchors.size(); ++Index)
        Longest = std::max(Longest, LengthSquared(Difference(Anchors[Index], Anchors[Index + 1u])));

    if (Longest <= 0.0)
        return false;

    const double Tolerance = std::sqrt(Longest) * 0.01;
    return LengthSquared(Difference(Anchors.front(), Anchors.back())) <= Tolerance * Tolerance;
}

bool PlacementFormsProfile(const SealedPlacement& Placed)
{
    if (Placed.Construction || !Placed.ClosedProfile)
        return false;

    const PlacementDeclaration Declared = DeclaredPlacement(Placed.Subject, Placed.Method);
    return Declared.ClosedProfile || (Placed.Subject == SketchSubject::Polyline && PlacementClosesOnItself(Placed.Anchors));
}

bool ResolveWorldBackedPlacementCurves(const SealedPlacement& Placed,
                                       std::vector<CurveSpecification>& Delivered)
{
    Delivered.clear();
    if (Placed.Subject == SketchSubject::None || Placed.Subject == SketchSubject::Point ||
        Placed.Subject == SketchSubject::Dimension || Placed.Anchors.size() < 2u)
        return false;

    std::vector<SpatialPoint> Anchors = Placed.Anchors;
    const SpatialPoint Final = Anchors.back();
    Anchors.pop_back();
    ResolvePlacementCurves(Placed.Subject, Anchors, Final, Delivered,
                           std::clamp(Placed.Resolution, PolygonSideMinimum, PolygonSideMaximum));
    return !Delivered.empty();
}

std::string PlacementCreateOperation(const SealedPlacement& Placed,
                                     bool Profile)
{
    const char* Naming = DeclaredPlacement(Placed.Subject, Placed.Method).Naming;
    const std::string Base = (Naming != nullptr && Naming[0] != '\0') ? Naming : "Shape";
    if (Placed.Construction)
        return std::string("Create Construction ") + Base;
    if (Profile)
        return std::string("Create ") + Base + " Profile";
    return std::string("Create ") + Base;
}

} // namespace

bool MirrorSketchIntoWorldDraft(const SketchStructure& Sketch,
                                WorldDraftStructure& Declared,
                                WorldDraftSketchMapping& Mapping)
{
    Declared.Reclaim();
    Mapping.Loops.clear();

    const WorldPlacementFrame SketchSupport = ResolveSketchSupportFrame(Sketch);
    std::vector<WorldPlacementFrame> CurveSupports(Sketch.Curves().size(), SketchSupport);

    for (std::uint32_t ProfileIndex = 0u; ProfileIndex < Sketch.Profiles().size(); ++ProfileIndex)
    {
        const ProfileSpecification& Profile = Sketch.Profiles()[ProfileIndex];
        const WorldPlacementFrame ProfileSupport = ResolveProfileSupportFrame(Profile);
        if (!ProfileSupport.Declared())
            continue;

        for (const ProfileLoop& Loop : Profile.HeldLoops())
            for (const ProfileCurveUse& Use : Loop.Traversal)
                if (Use.TraversedCurve.IssuedIndex > 0u
                 && Use.TraversedCurve.IssuedIndex <= CurveSupports.size())
                    CurveSupports[Use.TraversedCurve.IssuedIndex - 1u] = ProfileSupport;
    }

    for (std::size_t CurveIndex = 0u; CurveIndex < Sketch.Curves().size(); ++CurveIndex)
    {
        const DeclaredSketchCurve& Curve = Sketch.Curves()[CurveIndex];
        const WorldPlacementFrame& Support = CurveSupports[CurveIndex];
        if (Support.Declared())
            Declared.DeclareCurve(Curve.Geometry, Support);
        else
            Declared.DeclareCurve(Curve.Geometry);
    }

    for (std::uint32_t ProfileIndex = 0u; ProfileIndex < Sketch.Profiles().size(); ++ProfileIndex)
    {
        const ProfileSpecification& Profile = Sketch.Profiles()[ProfileIndex];
        for (std::uint32_t LoopIndex = 0u; LoopIndex < Profile.HeldLoops().size(); ++LoopIndex)
        {
            const ProfileLoop& Loop = Profile.HeldLoops()[LoopIndex];
            DeclaredWorldLoop Mirrored = {};
            Mirrored.Traversal.reserve(Loop.Traversal.size());
            for (const ProfileCurveUse& Use : Loop.Traversal)
                Mirrored.Traversal.push_back({ { Use.TraversedCurve.IssuedIndex }, Use.SameSense });
            Declared.DeclareLoop(Mirrored);
            Mapping.Loops.push_back({ { ProfileIndex + 1u }, LoopIndex });
        }
    }

    return true;
}

bool ApplyWorldDraftToSketch(const WorldDraftStructure& Declared,
                             SketchStructure& Sketch)
{
    if (Declared.CurveCount() != static_cast<std::uint32_t>(Sketch.Curves().size()))
        return false;

    for (std::uint32_t CurveIndex = 1u; CurveIndex <= Declared.CurveCount(); ++CurveIndex)
    {
        const DeclaredWorldCurve* Source = Declared.Resolve(WorldCurveName{ CurveIndex });
        if (Source == nullptr)
            return false;
        Sketch.Curves()[CurveIndex - 1u].Geometry = Source->Geometry;
    }

    return true;
}

WorkspaceRecordName ResolveRecordForWorldLoop(const WorkspaceRecordStructure& Records,
                                              const WorldDraftSketchMapping& Mapping,
                                              WorldLoopName Loop)
{
    if (!Loop.Assigned() || Loop.IssuedIndex > Mapping.Loops.size())
        return {};

    const ProfileNameInFeature Profile = Mapping.Loops[Loop.IssuedIndex - 1u].Profile;
    for (std::uint32_t Index = 1u; Index <= Records.DeclaredCount(); ++Index)
    {
        const WorkspaceRecord* Record = Records.Resolve({ Index });
        if (Record != nullptr && Record->Profile.IssuedIndex == Profile.IssuedIndex)
            return { Index };
    }

    return {};
}

bool ResolveWorldPickForSketchPick(const SketchStructure& Sketch,
                                   const WorkspaceRecordStructure& Records,
                                   const WorldDraftStructure& Declared,
                                   const WorldDraftSketchMapping& Mapping,
                                   const SketchPick& Selection,
                                   WorldPick& Resolved)
{
    static_cast<void>(Sketch);
    Resolved = {};
    if (!Selection.Standing())
        return false;

    switch (Selection.Subject)
    {
        case SketchPickSubject::Point:
            Resolved.Subject = WorldPickSubject::Point;
            Resolved.Point = { Selection.Point.IssuedIndex };
            Resolved.Curve = { Selection.Curve.IssuedIndex };
            return ResolveWorldDraftPointPosition(Declared, Resolved.Point, Resolved.Position);

        case SketchPickSubject::Control:
            Resolved.Subject = WorldPickSubject::Control;
            Resolved.Control = { Selection.Control.IssuedIndex };
            Resolved.Curve = { Selection.Curve.IssuedIndex };
            return ResolveWorldControlPosition(Declared, Resolved.Control, Resolved.Position);

        case SketchPickSubject::Curve:
            Resolved.Subject = WorldPickSubject::Curve;
            Resolved.Curve = { Selection.Curve.IssuedIndex };
            return ResolveWorldCurvePivot(Declared, Resolved.Curve, Resolved.Position);

        case SketchPickSubject::Record:
        {
            const WorkspaceRecord* Record = Selection.Record.Assigned()
                                          ? Records.Resolve(Selection.Record)
                                          : nullptr;
            if (Record == nullptr)
                return false;

            if (Record->SketchPoint.Assigned())
            {
                Resolved.Subject = WorldPickSubject::Point;
                Resolved.Point = { Record->SketchPoint.IssuedIndex };
                Resolved.Curve = { Record->SketchPoint.IssuedIndex >> 8u };
                return ResolveWorldDraftPointPosition(Declared, Resolved.Point, Resolved.Position);
            }

            if (Record->SketchCurve.Assigned())
            {
                Resolved.Subject = WorldPickSubject::Curve;
                Resolved.Curve = { Record->SketchCurve.IssuedIndex };
                return ResolveWorldCurvePivot(Declared, Resolved.Curve, Resolved.Position);
            }

            if (Record->Profile.Assigned())
            {
                for (std::uint32_t LoopIndex = 1u; LoopIndex <= Mapping.Loops.size(); ++LoopIndex)
                    if (Mapping.Loops[LoopIndex - 1u].Profile.IssuedIndex == Record->Profile.IssuedIndex)
                    {
                        Resolved.Subject = WorldPickSubject::Loop;
                        Resolved.Loop = { LoopIndex };
                        return ResolveWorldLoopPivot(Declared, Resolved.Loop, Resolved.Position);
                    }
            }
            return false;
        }

        case SketchPickSubject::None:
            return false;
    }

    return false;
}

bool ResolveSketchPickForWorldPick(const SketchStructure& Sketch,
                                   const WorkspaceRecordStructure& Records,
                                   const WorldDraftSketchMapping& Mapping,
                                   const WorldPick& Selection,
                                   SketchPick& Resolved)
{
    Resolved = {};
    if (!Selection.Standing())
        return false;

    switch (Selection.Subject)
    {
        case WorldPickSubject::Point:
            Resolved.Subject = SketchPickSubject::Point;
            Resolved.Point = { Selection.Point.IssuedIndex };
            Resolved.Curve = { Selection.Point.IssuedIndex >> 8u };
            Resolved.Record = ResolveRecordForPoint(Sketch, Records, Resolved.Point);
            return ResolveSketchPointPosition(Sketch, Resolved.Point, Resolved.Position);

        case WorldPickSubject::Control:
            Resolved.Subject = SketchPickSubject::Control;
            Resolved.Control = { Selection.Control.IssuedIndex };
            Resolved.Curve = { Selection.Control.IssuedIndex >> 12u };
            Resolved.Record = ResolveRecordForCurve(Sketch, Records, Resolved.Curve);
            return ResolveSketchControlPosition(Sketch, Resolved.Control, Resolved.Position);

        case WorldPickSubject::Curve:
            Resolved.Subject = SketchPickSubject::Curve;
            Resolved.Curve = { Selection.Curve.IssuedIndex };
            Resolved.Record = ResolveRecordForCurve(Sketch, Records, Resolved.Curve);
            return ResolveCurvePivot(Sketch, Resolved.Curve, Resolved.Position);

        case WorldPickSubject::Loop:
            Resolved.Subject = SketchPickSubject::Record;
            Resolved.Record = ResolveRecordForWorldLoop(Records, Mapping, Selection.Loop);
            if (!Resolved.Record.Assigned())
                return false;
            if (Selection.Loop.IssuedIndex <= Mapping.Loops.size())
                return ResolveProfilePivot(Sketch, Mapping.Loops[Selection.Loop.IssuedIndex - 1u].Profile,
                                           Resolved.Position);
            return false;

        case WorldPickSubject::None:
            return false;
    }

    return false;
}

Deliver<bool> ProjectWorldBackedSketchRendering(const SketchStructure& Sketch,
                                                const ResolvedCamera& Camera,
                                                const PlaneExtent& LogicalExtent,
                                                const DrawableScale& Drawable,
                                                WorkspaceCadPacket& Delivered,
                                                const WorldDraftRenderingStyle& Style,
                                                double ClosureTolerance,
                                                double CoplanarTolerance)
{
    WorldDraftStructure World;
    WorldDraftSketchMapping Mapping;
    MirrorSketchIntoWorldDraft(Sketch, World, Mapping);
    return ProjectWorldBackedSketchRendering(World, Camera, LogicalExtent, Drawable,
                                             Delivered, Style, ClosureTolerance, CoplanarTolerance);
}

Deliver<bool> ProjectWorldBackedSketchRendering(const WorldDraftStructure& Declared,
                                                const ResolvedCamera& Camera,
                                                const PlaneExtent& LogicalExtent,
                                                const DrawableScale& Drawable,
                                                WorkspaceCadPacket& Delivered,
                                                const WorldDraftRenderingStyle& Style,
                                                double ClosureTolerance,
                                                double CoplanarTolerance)
{
    return ProjectWorldDraftRendering(Declared, Camera, Drawable.ToPhysical(LogicalExtent),
                                      Delivered, Style, ClosureTolerance, CoplanarTolerance);
}

bool ProjectWorldPlacementPreview(const ResolvedCamera& Camera,
                                  const PlaneExtent& LogicalExtent,
                                  const DrawableScale& Drawable,
                                  const std::vector<CurveSpecification>& Geometry,
                                  const std::vector<SpatialPoint>& Anchors,
                                  const SpatialPoint& Hover,
                                  WorkspaceCadPacket& Delivered,
                                  const SketchRenderingStyle& Style)
{
    const PlaneExtent PhysicalExtent = Drawable.ToPhysical(LogicalExtent);
    bool Appended = false;

    for (const CurveSpecification& Span : Geometry)
    {
        if (!Span.Declared())
            continue;

        std::vector<SpatialPoint> Polyline;
        AppendCurvePolyline(Span, Polyline, Style.CurveSteps);
        for (std::size_t Index = 0u; Index + 1u < Polyline.size(); ++Index)
        {
            AppendClippedSegment(Camera, PhysicalExtent,
                                 Polyline[Index], Polyline[Index + 1u],
                                 Style.PreviewCurveColour, Style.CurveThickness, Delivered);
            Appended = true;
        }
    }

    for (const SpatialPoint& Anchor : Anchors)
    {
        float X = 0.0f;
        float Y = 0.0f;
        if (ProjectFromCamera(Camera, PhysicalExtent, Anchor, X, Y))
        {
            Delivered.AddMarker(X, Y, Style.ControlColour, Style.ControlRadius,
                                WorkspaceCadMarkerSubject::SketchControl);
            Appended = true;
        }
    }

    float HoverX = 0.0f;
    float HoverY = 0.0f;
    if (ProjectFromCamera(Camera, PhysicalExtent, Hover, HoverX, HoverY))
    {
        Delivered.AddMarker(HoverX, HoverY, Style.PreviewCurveColour, Style.ControlRadius,
                            WorkspaceCadMarkerSubject::SketchControl);
        Appended = true;
    }

    return Appended;
}

bool CommitPlacementWorldBacked(const Workplane& ActiveWorkplane,
                                WorldDraftStructure& Declared,
                                WorldDraftSketchMapping& Mapping,
                                WorkspaceNameIndex& Naming,
                                SketchStructure& Sketch,
                                WorkspaceRecordStructure& Records,
                                WorkspaceRevisionSequence& Revisions,
                                const SealedPlacement& Placed,
                                WorkspaceRecordName& SelectedRecord)
{
    SelectedRecord = {};

    if (Placed.Subject == SketchSubject::None)
        return false;

    if (Placed.Subject == SketchSubject::Dimension)
    {
        if ((!Sketch.PlaneDeclared() || !SketchHasCommittedGeometry(Sketch)) && WorkplaneDeclared(ActiveWorkplane))
            Sketch.DeclarePlane(ResolveSketchPlaneFromWorkplane(ActiveWorkplane));

        const Deliver<WorkspaceRecordName> Record = CommitPlacement(Naming, Sketch, Records, Revisions, Placed);
        if (!Record.Resolved)
            return false;
        MirrorSketchIntoWorldDraft(Sketch, Declared, Mapping);
        SelectedRecord = Record.Resolve();
        return SelectedRecord.Assigned();
    }

    if (Declared.CurveCount() != static_cast<std::uint32_t>(Sketch.Curves().size()))
        MirrorSketchIntoWorldDraft(Sketch, Declared, Mapping);

    WorldPlacementFrame Support = ResolveWorkplaneSupportFrame(ActiveWorkplane);
    if (!Support.Declared())
        Support = ResolveSketchSupportFrame(Sketch);
    const bool SupportStanding = Support.Declared();

    std::vector<WorldCurveName> WorldCurves;
    if (Placed.Subject == SketchSubject::Point)
    {
        if (Placed.Anchors.empty())
            return false;

        SpatialDirection Along = SupportStanding ? Normalize(Support.AlongDirection)
                                                 : SpatialDirection{ 1.0, 0.0, 0.0 };
        if (LengthSquared(Along) <= 1.0e-12)
            Along = { 1.0, 0.0, 0.0 };

        const SpatialPoint Tip = Added(Placed.Anchors.back(), Scaled(Along, 0.001));
        WorldCurves.push_back(SupportStanding
                            ? Declared.DeclareLine(Placed.Anchors.back(), Tip, Support)
                            : Declared.DeclareLine(Placed.Anchors.back(), Tip));
    }
    else
    {
        std::vector<CurveSpecification> Geometry;
        if (!ResolveWorldBackedPlacementCurves(Placed, Geometry))
            return false;

        WorldCurves.reserve(Geometry.size());
        for (const CurveSpecification& Curve : Geometry)
            WorldCurves.push_back(SupportStanding
                                ? Declared.DeclareCurve(Curve, Support)
                                : Declared.DeclareCurve(Curve));
    }

    if (WorldCurves.empty())
        return false;

    if (PlacementFormsProfile(Placed))
    {
        DeclaredWorldLoop Loop = {};
        for (const WorldCurveName& Curve : WorldCurves)
            Loop.Traversal.push_back({ Curve, true });
        if (!Declared.DeclareLoop(Loop).Assigned())
            return false;
    }

    std::vector<SketchCurveName> SketchCurves;
    SketchCurves.reserve(WorldCurves.size());
    for (const WorldCurveName& Curve : WorldCurves)
    {
        const DeclaredWorldCurve* Resolved = Declared.Resolve(Curve);
        if (Resolved == nullptr || !Resolved->Geometry.Declared())
            return false;
        SketchCurves.push_back(Sketch.DeclareCurve(Resolved->Geometry));
    }

    const bool Profile = PlacementFormsProfile(Placed);
    std::vector<WorkspaceRecordName> Written;
    if (Placed.Subject == SketchSubject::Point)
    {
        std::vector<SketchPointPlacement> Points;
        if (!ResolveSketchPoints(Sketch, SketchCurves.front(), Points) || Points.empty())
            return false;
        const WorkspaceRecordName Record = DeclareWorkspacePoint(Naming, Records, Points.front().Name);
        Written.push_back(Record);
        SelectedRecord = Record;
    }
    else if (Profile)
    {
        ProfileSpecification Shape = {};
        if (SupportStanding)
            Shape.DeclarePlane({ Support.Origin, Support.Normal, Support.AlongDirection });
        else if (Sketch.PlaneDeclared())
            Shape.DeclarePlane({ Sketch.HeldPlane().Origin, Sketch.HeldPlane().Normal, Sketch.HeldPlane().AlongDirection });

        ProfileLoop Loop = {};
        Loop.Orientation = ProfileLoopOrientation::Outer;
        for (const SketchCurveName& Curve : SketchCurves)
            Loop.Traversal.push_back({ { Curve.IssuedIndex }, true });
        Shape.DeclareLoop(Loop);

        const ProfileNameInFeature DeclaredProfile = Sketch.DeclareProfile(Shape);
        Mapping.Loops.push_back({ DeclaredProfile, 0u });
        const WorkspaceRecordName Record = DeclareWorkspaceProfile(Naming, Records, DeclaredProfile);
        Written.push_back(Record);
        SelectedRecord = Record;
    }
    else
    {
        for (const SketchCurveName& Curve : SketchCurves)
            Written.push_back(DeclareWorkspaceCurve(Naming, Records, Curve, Placed.Construction));
        SelectedRecord = Written.empty() ? WorkspaceRecordName{} : Written.front();
    }

    if (!SelectedRecord.Assigned() || Written.empty())
        return false;

    const WorkspaceRecord* Primary = Records.Resolve(SelectedRecord);
    const char* NamingText = DeclaredPlacement(Placed.Subject, Placed.Method).Naming;
    const std::string Description = Primary != nullptr
                                  ? std::string("Declared ") + Primary->Naming
                                  : std::string("Declared ") + (NamingText != nullptr ? NamingText : "shape");
    Revisions.Seal(Description,
                   PlacementCreateOperation(Placed, Profile),
                   Written,
                   Revisions.DeclaredCount() + 1u);
    return true;
}

} // namespace Slate
