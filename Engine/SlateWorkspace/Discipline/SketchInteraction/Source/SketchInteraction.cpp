//============================================================================================================================================
//                                                       SKETCHINTERACTION.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/SketchInteraction/Api/SketchInteraction.h"

#include "SlateShape/Sketch/DimensionSolver/Api/DimensionSolver.h"
#include "SlateShape/Sketch/ProfileCorner/Api/ProfileCorner.h"
#include "SlateShape/Sketch/ProfilePattern/Api/ProfilePattern.h"
#include "SlateShape/Sketch/ProfileReshape/Api/ProfileReshape.h"
#include "SlateShape/Sketch/SketchEditing/Api/SketchEditing.h"
#include "SlateWorkspace/Discipline/ConstraintAuthoring/Api/ConstraintAuthoring.h"
#include "SlateWorkspace/Discipline/RecordDeclaration/Api/RecordDeclaration.h"
#include "SlateWorkspace/Discipline/SketchViewportOverlay/Api/SketchViewportOverlay.h"
#include "SlateWorkspace/Discipline/ToolAvailability/Api/ToolAvailability.h"
#include "SlateWorkspace/Discipline/TransformGizmo/Api/TransformGizmo.h"
#include "SlateWorkspace/Discipline/TransformSequence/Api/TransformSequence.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/SketchBasis.h"
#include "SlateWorkspace/Discipline/WorkplaneStanding/Api/WorkplaneStanding.h"
#include "SlateWorkspace/Discipline/WorldDraftInteraction/Api/WorldDraftInteraction.h"
#include "SlateWorkspace/Discipline/WorldDraftSketchBridge/Api/WorldDraftSketchBridge.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace Slate
{

namespace
{

SelectionElement ResolveToolSelectionElement(ParametricToolSubject Tool,
                                             const SelectionOptions& Selection)
{
    switch (Tool)
    {
        case ParametricToolSubject::Fillet:
        case ParametricToolSubject::Chamfer:
            return SelectionElement::Free;

        case ParametricToolSubject::Trim:
        case ParametricToolSubject::Extend:
        case ParametricToolSubject::Offset:
        case ParametricToolSubject::Cut:
        case ParametricToolSubject::LinearArray:
        case ParametricToolSubject::Mirror:
            return SelectionElement::Edge;

        default:
            return Selection.Element;
    }
}

bool CurveSelectionStanding(const SketchPick& Selection)
{
    return Selection.Standing() && Selection.Curve.Assigned() && Selection.Record.Assigned();
}

bool EdgeSelectionStanding(const SketchPick& Selection)
{
    return CurveSelectionStanding(Selection) && Selection.Subject == SketchPickSubject::Curve;
}

bool CornerSelectionStanding(const SketchPick& Selection)
{
    if (!CurveSelectionStanding(Selection))
        return false;

    return Selection.Subject == SketchPickSubject::Point
        || Selection.Subject == SketchPickSubject::Control
        || Selection.Subject == SketchPickSubject::Curve;
}

bool SketchHasCommittedGeometry(const SketchStructure& Sketch)
{
    return !Sketch.Curves().empty() || !Sketch.Profiles().empty();
}

SketchPlane ResolveSketchPlaneFromWorkplane(const Workplane& ActiveWorkplane)
{
    const SpatialBasis Basis = ResolveWorkplaneBasis(ActiveWorkplane);
    return { Basis.Origin, Basis.Normal, Basis.Along };
}

SketchPick ResolveViewportEditSelection(ParametricToolSubject Tool,
                                        const SketchPick& ActiveSelection,
                                        const SketchPick& HoveredSelection)
{
    const auto HoverOrActiveCurve = [&]() -> SketchPick
    {
        if (EdgeSelectionStanding(HoveredSelection))
            return HoveredSelection;
        if (EdgeSelectionStanding(ActiveSelection))
            return ActiveSelection;
        return {};
    };

    switch (Tool)
    {
        case ParametricToolSubject::Fillet:
        case ParametricToolSubject::Chamfer:
            if (CornerSelectionStanding(HoveredSelection))
                return HoveredSelection;
            if (CornerSelectionStanding(ActiveSelection))
                return ActiveSelection;
            return {};

        case ParametricToolSubject::Trim:
        case ParametricToolSubject::Extend:
        case ParametricToolSubject::Offset:
        case ParametricToolSubject::Cut:
        case ParametricToolSubject::LinearArray:
        case ParametricToolSubject::Mirror:
            return HoverOrActiveCurve();

        default:
            return {};
    }
}

[[maybe_unused]] SpatialPoint ResolveViewportEditProbe(ParametricToolSubject Tool,
                                      const SpatialPoint& Probe,
                                      const SketchPick& TargetSelection)
{
    if ((Tool == ParametricToolSubject::Fillet || Tool == ParametricToolSubject::Chamfer) &&
        (TargetSelection.Subject == SketchPickSubject::Point ||
         TargetSelection.Subject == SketchPickSubject::Control))
    {
        return TargetSelection.Position;
    }

    return Probe;
}

bool SamePickIdentity(const SketchPick& Left,
                      const SketchPick& Right)
{
    if (Left.Subject != Right.Subject)
        return false;

    switch (Left.Subject)
    {
        case SketchPickSubject::Point:
            return Left.Point.IssuedIndex == Right.Point.IssuedIndex;
        case SketchPickSubject::Control:
            return Left.Control.IssuedIndex == Right.Control.IssuedIndex;
        case SketchPickSubject::Curve:
            return Left.Curve.IssuedIndex == Right.Curve.IssuedIndex;
        case SketchPickSubject::Record:
            return Left.Record.IssuedIndex == Right.Record.IssuedIndex;
        case SketchPickSubject::None:
            return true;
    }
    return false;
}

GizmoHandle ResolveUniversalGizmoHandle(const GizmoScreenBasis& Screen,
                                        float PointerX,
                                        float PointerY)
{
    const GizmoHandle Move = ResolveGizmoHandle(Screen, TransformManner::Move, PointerX, PointerY);
    if (Move == GizmoHandle::MoveFree)
        return Move;

    const GizmoHandle Scale = ResolveGizmoHandle(Screen, TransformManner::Scale, PointerX, PointerY);
    if (Scale != GizmoHandle::None)
        return Scale;

    if (Move != GizmoHandle::None)
        return Move;

    return ResolveGizmoHandle(Screen, TransformManner::Rotate, PointerX, PointerY);
}

}   // namespace

void DriveViewport(const PlaneExtent& Extent,
                   const PointerCondition& Pointer,
                   const ModifierCondition& Modifiers,
                   ViewportStanding& View,
                   bool Perspective)
{
    const bool PointerOverViewport = Extent.Encloses(Pointer.PositionX, Pointer.PositionY);
    if (!PointerOverViewport && !Pointer.SecondaryHeld)
        return;

    if (PointerOverViewport && Pointer.WheelY != 0.0f)
    {
        // 🔴 RECIPROCALS, NOT 1.1 AND 0.9. Those two are not inverses — their product is 0.99 — so
        //    rocking the wheel in and out drifted the view smaller by 1% a pair. Dividing by the same
        //    factor that multiplies makes a zoom in and back out land exactly where it began.
        constexpr double ZoomStep = 1.1;
        if (Perspective)
            View.Distance = std::clamp(Pointer.WheelY > 0.0f ? View.Distance / ZoomStep
                                                             : View.Distance * ZoomStep,
                                       20.0, 4000.0);
        else
            View.OrthoScale = std::clamp(Pointer.WheelY > 0.0f ? View.OrthoScale * ZoomStep
                                                               : View.OrthoScale / ZoomStep,
                                         0.05, 40.0);
    }

    if (!Pointer.SecondaryHeld)
        return;

    const SpatialBasis Basis = { {}, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 1.0, 0.0} };
    const ViewFrame Frame = ResolveViewportFrame(Basis, View, Perspective);

    if (Perspective && !Modifiers.Shifted)
    {
        View.OrbitYaw -= static_cast<double>(Pointer.TravelX) * 0.35;
        View.OrbitPitch = std::clamp(View.OrbitPitch + static_cast<double>(Pointer.TravelY) * 0.25, -89.0, 89.0);
        View.Orientation = ViewportOrientation::Isometric;
        return;
    }

    const double Scale = Perspective ? (View.Distance * 0.0025) : (1.0 / std::max(View.OrthoScale, 0.001));
    const SpatialDirection Pan = Added(Scaled(Frame.Right, -static_cast<double>(Pointer.TravelX) * Scale),
                                       Scaled(Frame.Up, static_cast<double>(Pointer.TravelY) * Scale));
    View.Focus = Added(View.Focus, Pan);
}

void AdoptCommittedShape(SketchSubject Subject,
                         WorkspaceNameIndex& Naming,
                         SketchStructure& Sketch,
                         WorkspaceRecordStructure& Records,
                         WorkspaceRevisionSequence& Revisions,
                         const Deliver<WorkspaceRecordName>& Record,
                         WorkspaceRecordName& PendingSelection)
{
    if (!Record.Resolved)
        return;

    PendingSelection = Record.Resolve();
    if (DeclaredPlacement(Subject).ClosedProfile)
    {
        const WorkspaceRecordName ProfileRecord = AutoDeclareWorkspaceProfilesFromChains(Naming, Sketch, Records, Revisions);
        if (ProfileRecord.Assigned())
            PendingSelection = ProfileRecord;
    }
}

SpatialPoint ApplySketchToolSettings(const SketchPlacement& Tool,
                                          const SpatialBasis& Basis,
                                          const ParametricToolsContext& Settings,
                                          SpatialPoint Hover)
{
    if (Tool.Anchors().empty())
        return Hover;

    double AnchorAlong = 0.0;
    double AnchorAcross = 0.0;
    double HoverAlong = 0.0;
    double HoverAcross = 0.0;
    ResolvePlaneCoordinates(Basis, Tool.Anchors()[0], AnchorAlong, AnchorAcross);
    ResolvePlaneCoordinates(Basis, Hover, HoverAlong, HoverAcross);

    const double DeltaAlong = HoverAlong - AnchorAlong;
    const double DeltaAcross = HoverAcross - AnchorAcross;
    const double Length = std::sqrt(DeltaAlong * DeltaAlong + DeltaAcross * DeltaAcross);

    if (Tool.Subject() == SketchSubject::Line && (Settings.LineLengthAssist || Settings.LineAngleAssist))
    {
        double Angle = Length > 1.0e-6 ? std::atan2(DeltaAcross, DeltaAlong) : Settings.LineAngleDegrees * ProjectionPi / 180.0;
        double Distance = Length;
        if (Settings.LineAngleAssist)
            Angle = Settings.LineAngleDegrees * ProjectionPi / 180.0;
        if (Settings.LineLengthAssist)
            Distance = std::max(Settings.LineLength, 0.0);
        return ResolvePlanarPoint(Basis,
                                    AnchorAlong + std::cos(Angle) * Distance,
                                    AnchorAcross + std::sin(Angle) * Distance);
    }

    if (Tool.Subject() == SketchSubject::Rectangle && Settings.RectangleDimensionAssist)
    {
        const double SignAlong = DeltaAlong < 0.0 ? -1.0 : 1.0;
        const double SignAcross = DeltaAcross < 0.0 ? -1.0 : 1.0;
        return ResolvePlanarPoint(Basis,
                                    AnchorAlong + SignAlong * std::max(Settings.RectangleWidth, 0.0),
                                    AnchorAcross + SignAcross * std::max(Settings.RectangleHeight, 0.0));
    }

    if (Tool.Subject() == SketchSubject::Circle && Settings.CircleRadiusAssist)
    {
        const double Radius = std::max(Settings.CircleRadius, 0.0);
        double Angle = Length > 1.0e-6 ? std::atan2(DeltaAcross, DeltaAlong) : 0.0;
        return ResolvePlanarPoint(Basis,
                                    AnchorAlong + std::cos(Angle) * Radius,
                                    AnchorAcross + std::sin(Angle) * Radius);
    }

    return Hover;
}

/// 🧩 The Workplane tool: names the surface the artist will draw on by pointing at the viewport.
/// out   Taken  [-]  true when the tool consumed the press, so no sketch tool also acts on it
/// note 🔴 SCREEN SPACE IS THE POINT. The new plane faces the viewer, so what the artist draws next lands
///       where they draw it instead of skewed across a plane seen edge-on. This is the same thing as
///       putting an empty somewhere and drawing on the grid through it — origin plus orientation — with
///       the orientation taken from where the camera is rather than left as the world's.
/// note ⚠️ Only fires on a press. Hovering must not move the plane out from under a half-drawn curve.
/// note 📝 A plane is a document-level decision, so it seals a revision and can be walked back.
bool ApplyWorkplaneTool(const PlaneExtent& Extent,
                        const PointerCondition& Pointer,
                        const SpatialBasis& Basis,
                        const ViewportStanding& View,
                        bool Perspective,
                        const ParametricToolsContext& ToolContext,
                        WorkspaceNameIndex& Naming,
                        SketchStructure& Sketch,
                        WorkspaceRecordStructure& Records,
                        WorkspaceRevisionSequence& Revisions,
                        WorkplaneCatalogue& Workplanes)
{
    if (ToolContext.ActiveSubject != ParametricToolSubject::Workplane)
        return false;

    if (!Pointer.ContactPressed || !Extent.Encloses(Pointer.PositionX, Pointer.PositionY))
        return false;

    // Where the artist pointed, resolved onto whatever plane is standing now.
    SpatialPoint Pointed = {};
    if (!ResolveViewportPlaneIntersection(Basis, View, Perspective, Extent,
                                          Pointer.PositionX, Pointer.PositionY, Pointed))
        return false;

    // 🔴 The direction the viewer is looking. `ResolveViewportFrame` gives the frame the viewport is
    //    drawn with, and its Forward runs from the eye into the display — exactly the normal a plane
    //    square to the display needs.
    const ViewFrame Frame = ResolveViewportFrame(Basis, View, Perspective);

    const Workplane Placed = ResolvePlacedWorkplane(Pointed, Frame.Forward);

    if (!Placed.Declared())
        return false;

    // 🔴 THE PLANE JOINS THE OTHERS RATHER THAN REPLACING THE ONE THE SKETCH HOLDS. This is the fix for
    //    "drew in the wrong place": the shipped code called `Sketch.DeclarePlane` straight away, and
    //    because a sketch holds exactly one plane and overwrites it, everything already drawn was from
    //    then on measured against a surface it had never been drawn on. Nothing moved in world terms and
    //    nothing refused, so the drawing simply stopped meaning what it had meant.
    const WorkplaneName Named =
        Workplanes.Declare(Placed, ResolveWorkplaneNaming(Workplanes, WorkplaneOrigin::Placed));
    if (!Named.Assigned())
        return false;

    // 📝 The compatibility sketch is seeded from the active plane only while it does not already carry
    //    committed geometry. Once geometry exists, the active workplane may change independently and the
    //    world draft remains the authority for where new drawing lands.
    if (!Sketch.PlaneDeclared() || !SketchHasCommittedGeometry(Sketch))
        Sketch.DeclarePlane(ResolveSketchPlaneFromWorkplane(Workplanes.Active()));

    // 📝 Written into the directory so the artist can see it, select it and walk it back.
    const CataloguedWorkplane* Held = Workplanes.Resolve(Named);
    WorkspaceRecord Record = {};
    Record.Subject        = WorkspaceRecordSubject::Folder;
    Record.FolderCategory = WorkspaceCategory::Geometry;
    Record.ParentFolder   = ResolveCategoryFolder(Records, WorkspaceCategory::Geometry);
    Record.Naming         = Held != nullptr
                          ? Held->Naming
                          : std::string("Workplane ") + Naming.Issue(WorkspaceRecordSubject::Folder);
    const WorkspaceRecordName Written = Records.Declare(Record);

    Revisions.Seal("Placed a workplane facing the view", "Place Workplane", { Written },
                   Revisions.DeclaredCount() + 1u);
    return true;
}

void DriveDrawingWithModifiers(const PlaneExtent& Extent,
                               const PointerCondition& Pointer,
                               const TextInputCondition& Text,
                               const ModifierCondition& Modifiers,
                               const SpatialBasis& Basis,
                               const ViewportStanding& View,
                               bool Perspective,
                               const ParametricToolsContext& ToolContext,
                               WorkspaceNameIndex& Naming,
                               SketchStructure& Sketch,
                               WorldDraftStructure& World,
                               WorldDraftSketchMapping& Mapping,
                               WorkspaceRecordStructure& Records,
                               WorkspaceRevisionSequence& Revisions,
                               WorkplaneCatalogue& Workplanes,
                               WorkspaceRecordName& PendingSelection,
                               SketchPlacement& Tool,
                               bool& PointerTaken)
{
    // 🔴 What is left here is only what a HOST can answer: where the pointer lands on the sketch plane,
    //    what it snapped to, and what a finished placement becomes in the document. How many anchors a
    //    subject needs, whether a double-press ends it, and whether an unsnapped contact counts are all
    //    `SketchPlacement`'s to answer — they were a chain of `else if` branches over twenty-two subjects
    //    here, and the branch a subject fell into was the only thing that decided when it committed.
    // 🔴 The workplane tool changes the surface rather than drawing on it, so it is answered BEFORE the
    //    sketch tools and consumes the press when it fires.
    if (ApplyWorkplaneTool(Extent, Pointer, Basis, View, Perspective, ToolContext,
                           Naming, Sketch, Records, Revisions, Workplanes))
    {
        PointerTaken = true;
        return;
    }

    const SketchToolSelection Desired = SelectedTool(ToolContext.ActiveSubject);

    const bool Construction = ToolContext.ConstructionGeometry ||
                              ToolContext.ActiveSubject == ParametricToolSubject::ConstructionLine;
    Tool.Declare(Desired.Subject, Desired.Method, Construction);

    // 🔴 The closed-profile choice reaches the tool, which seals it into the placement, which is what
    //    the commit reads. A panel flag the commit cannot see would decide nothing.
    Tool.DeclareClosedProfile(ToolContext.ClosedProfileFill);

    if (Desired.Subject == SketchSubject::None)
        return;

    if (Text.CancelPressed)
    {
        Tool.Abandon();
        PointerTaken = true;
        return;
    }

    if (!Extent.Encloses(Pointer.PositionX, Pointer.PositionY))
        return;

    SpatialPoint Raw = {};
    if (!ResolveViewportPlaneIntersection(Basis, View, Perspective, Extent,
                                          Pointer.PositionX, Pointer.PositionY, Raw))
        return;

    // 📝 Snapping stays with the host: it needs the sketch, the view scale and the modifier that suspends
    //    it. The placement is told where the pointer ended up, not how it got there.
    const double SnapTolerance = ResolveSnapTolerance(View, Perspective);
    // 🔴 THE WHEEL SETS A POLYGON'S RESOLUTION, exactly as the circle tool's drag sets its radius.
    //    `Resolve` refuses for every other subject, so the wheel still zooms while anything else is
    //    being drawn, and the press is consumed only when it was actually used.
    if (Tool.Resolve(Pointer.WheelY))
        PointerTaken = true;

    // 🔴 SNAPPING IS HELD, NOT SUFFERED. Control USED to suspend it, which meant every pointer move
    //    was silently dragged onto the nearest endpoint, midpoint, intersection or grid corner whether
    //    the artist wanted it or not -- placing a point in open space meant holding a key to be left
    //    alone. It is off by default now and Control turns it on for as long as it is held, so the
    //    unmodified pointer means exactly where the pointer is.
    //
    // 🔴 The placement in progress is offered its OWN anchors, which is what lets a polyline or a
    //    spline close back onto the point it started from. Until it seals, that geometry exists
    //    nowhere else.
    const SketchSnapPlacement Placement = Modifiers.Commanded
                                        ? ResolveNearestSnap(Sketch, Workplanes.Active(),
                                                             Raw, SnapTolerance, {}, 10.0,
                                                             Tool.Anchors())
                                        : SketchSnapPlacement{};

    SpatialPoint Hover = Placement.Resolved() ? Placement.Position : Raw;
    Hover = ApplySketchToolSettings(Tool, Basis, ToolContext, Hover);
    Tool.Hover(Hover, Placement);

    // 🔴 One arrival, one response, for every subject. The keyboard accept and the pointer press differ
    //    only in whether the contact terminates a growing curve — Enter always does, a press does so only
    //    on a double-press. Nothing below names a subject.
    if (!Text.AcceptPressed && !Pointer.ContactPressed)
        return;

    // 🔴 The compatibility sketch is seeded once from whatever plane the catalogue says is ACTIVE,
    //    which is the ground plane until the artist chooses otherwise. After geometry exists the active
    //    workplane may continue to change without rewriting the sketch's one global plane.
    if (!Sketch.PlaneDeclared() || !SketchHasCommittedGeometry(Sketch))
        Sketch.DeclarePlane(ResolveSketchPlaneFromWorkplane(Workplanes.Active()));

    const bool Terminating = Text.AcceptPressed || Pointer.ContactDoublePressed;

    PointerTaken = true;
    if (Tool.Anchor(Terminating) != PlacementArrival::Complete)
        return;

    const SealedPlacement Sealed = Tool.Seal();
    if (CommitPlacementWorldBacked(Workplanes.Active(), World, Mapping,
                                  Naming, Sketch, Records, Revisions,
                                  Sealed, PendingSelection))
        return;

    const Deliver<WorkspaceRecordName> Record = CommitPlacement(Naming, Sketch, Records, Revisions, Sealed);
    AdoptCommittedShape(Sealed.Subject, Naming, Sketch, Records, Revisions, Record, PendingSelection);
    MirrorSketchIntoWorldDraft(Sketch, World, Mapping);
}

bool ApplyDimensionTextEdit(const TextInputCondition& TextInput,
                            SketchStructure& Sketch,
                            WorkspaceRecordStructure& Records,
                            WorkspaceRevisionSequence& Revisions,
                            WorkspaceRecordName SelectedRecord)
{
    const WorkspaceRecord* Record = Records.Resolve(SelectedRecord);
    if (Record == nullptr || Record->Subject != WorkspaceRecordSubject::Dimension || !Record->Dimension.Assigned())
        return false;
    char Numeric[32] = {};
    std::size_t Count = 0u;
    for (std::uint32_t Index = 0u; Index < TextInput.IntakeCount && Count + 1u < sizeof(Numeric); ++Index)
    {
        const char Character = TextInput.Intake[Index];
        if ((Character >= '0' && Character <= '9') || Character == '.' || Character == '-')
            Numeric[Count++] = Character;
    }
    Numeric[Count] = '\0';
    if (Count == 0u || Record->Dimension.IssuedIndex == 0u || Record->Dimension.IssuedIndex > Sketch.Dimensions().size())
        return false;
    const double Target = std::atof(Numeric);
    if (Target <= 0.0)
        return false;
    Sketch.Dimensions()[Record->Dimension.IssuedIndex - 1u].Target = Target;
    Discard(ApplyDimension(Sketch, Record->Dimension));
    Revisions.Seal("Edited " + Record->Naming, "Edit Dimension", { SelectedRecord }, Revisions.DeclaredCount() + 1u);
    return true;
}

bool ApplyViewportConstraintTool(ParametricToolSubject Tool,
                                 WorkspaceNameIndex& Naming,
                                 SketchStructure& Sketch,
                                 WorkspaceRecordStructure& Records,
                                 WorkspaceRevisionSequence& Revisions,
                                 const SketchPick& ActiveSelection,
                                 const SketchPick& HoveredSelection,
                                 WorkspaceRecordName& PendingSelection)
{
    ConstraintSubject Subject = ConstraintSubject::Fixed;
    if (!SelectedConstraint(Tool, Subject) || !ActiveSelection.Standing())
        return false;

    // 🔴 What the relationship NEEDS is asked of the unit rather than decided by which branch the subject
    //    falls into. The chain here tested the subject and then reached for whichever selection field the
    //    branch assumed, so what a constraint demanded was a property of its position in the chain.
    const Deliver<ConstraintSpecification> Declared =
        DeclareConstraintFrom(Subject,
                              ActiveSelection.Curve, HoveredSelection.Curve,
                              ActiveSelection.Point, HoveredSelection.Point);
    if (!Declared.Resolved)
        return false;

    const Deliver<WorkspaceRecordName> Committed =
        CommitConstraint(Naming, Sketch, Records, Revisions, Declared.Delivered);
    if (!Committed.Resolved)
        return false;

    PendingSelection = Committed.Delivered;
    return true;
}

bool CommitCurveSet(WorkspaceNameIndex& Naming,
                    WorkspaceRecordStructure& Records,
                    WorkspaceRevisionSequence& Revisions,
                    const std::vector<SketchCurveName>& Curves,
                    const char* Label,
                    std::vector<WorkspaceRecordName>& Written)
{
    Written.clear();
    for (SketchCurveName Curve : Curves)
        if (Curve.Assigned())
            Written.push_back(DeclareWorkspaceCurve(Naming, Records, Curve));
    if (Written.empty())
        return false;
    Revisions.Seal(Label, "Edit Sketch", Written, Revisions.DeclaredCount() + 1u);
    return true;
}

bool ApplyViewportEditTool(ParametricToolSubject Tool,
                           const SpatialPoint& Probe,
                           const SpatialBasis& Basis,
                           WorkspaceNameIndex& Naming,
                           SketchStructure& Sketch,
                           WorkspaceRecordStructure& Records,
                           WorkspaceRevisionSequence& Revisions,
                           const SketchPick& TargetSelection,
                           double CornerDistance,
                           bool KeepStart,
                           WorkspaceRecordName& PendingSelection)
{
    static_cast<void>(Tool);
    static_cast<void>(Probe);
    static_cast<void>(Basis);
    static_cast<void>(Naming);
    static_cast<void>(Sketch);
    static_cast<void>(Records);
    static_cast<void>(Revisions);
    static_cast<void>(TargetSelection);
    static_cast<void>(CornerDistance);
    static_cast<void>(KeepStart);
    static_cast<void>(PendingSelection);
    return false;
}

void DriveViewportSelectionAndTransform(const PlaneExtent& Extent,
                                        const PointerCondition& Pointer,
                                        const TextInputCondition& TextInput,
                                        const ModifierCondition& Modifiers,
                                        const SpatialBasis& Basis,
                                        const ViewportStanding& View,
                                        bool Perspective,
                                        ParametricToolSubject ActiveTool,
                                        const SelectionOptions& Selection,
                                        const GizmoOptions& Gizmo,
                                        WorkspaceNameIndex& Naming,
                                        const WorkspaceDirectoryProjection& Directory,
                                        const ParametricWorkspaceContext& WorkspaceApplied,
                                        SketchStructure& Sketch,
                                        WorkspaceRecordStructure& Records,
                                        WorkspaceRevisionSequence& Revisions,
                                        WorkspaceRecordName& PendingSelection,
                                        SketchPick& SemanticSelection,
                                        SketchPick& HoveredSelection,
                                        TransformSession& Transform,
                                        OverlayGeometry& Overlay,
                                        bool& PointerTaken,
                                        double SessionMilliseconds,
                                        double& LastGPressedMilliseconds,
                                        double CornerDistance,
                                        bool KeepStart)
{
    const WorkspaceRecordName SelectedRecord = SelectedRecordIn(Directory, WorkspaceApplied);
    if (ApplyDimensionTextEdit(TextInput, Sketch, Records, Revisions, SelectedRecord))
        PointerTaken = true;
    if (SemanticSelection.Standing() && SelectedRecord.Assigned() &&
        SemanticSelection.Record.IssuedIndex != SelectedRecord.IssuedIndex &&
        (!PendingSelection.Assigned() || PendingSelection.IssuedIndex != SemanticSelection.Record.IssuedIndex))
        SemanticSelection = {};

    HoveredSelection = {};
    SpatialPoint Probe = {};
    const bool Probed = ResolveViewportPlaneIntersection(Basis, View, Perspective, Extent,
                                                         Pointer.PositionX, Pointer.PositionY, Probe);
    if (Probed)
    {
        // 🔴 THE TOLERANCE IS A DISTANCE ON THE SCREEN, converted here to the world the probe lives in.
        //    `ResolveSnapTolerance` answers a world span for SNAPPING, which is a different question with
        //    a different answer: snapping asks "what would the artist have meant", selection asks "what
        //    is under the pointer". Selecting through the snap tolerance is why reaching for one vertex
        //    at a distant zoom picked its neighbour.
        const double Reach = ResolvePickTolerance(View, Perspective, Selection.ResolvedTolerance(), Extent.Height());
        const SelectionElement HoverElement = ResolveToolSelectionElement(ActiveTool, Selection);
        HoveredSelection = ResolveSketchPickForElement(Sketch, Records, Probe, Reach, HoverElement);
    }

    const SketchPick ActiveSelection =
        EditableSelection(Sketch, Records, SelectedRecord, PendingSelection, SemanticSelection);
    const bool SelectStanding = ActiveTool == ParametricToolSubject::Select;

    if (TextInput.DeletePressed && ActiveSelection.Record.Assigned())
    {
        Discard(Records.ToggleVisible(ActiveSelection.Record, false));
        Revisions.Seal("Deleted selected sketch record", "Delete Sketch Record", { ActiveSelection.Record },
                       Revisions.DeclaredCount() + 1u);
        PendingSelection = {};
        SemanticSelection = {};
        PointerTaken = true;
    }

    ConstraintSubject ActiveConstraintSubject = ConstraintSubject::Fixed;
    if (!Transform.Engaged() && Pointer.ContactPressed && SelectedConstraint(ActiveTool, ActiveConstraintSubject))
    {
        if (ApplyViewportConstraintTool(ActiveTool, Naming, Sketch, Records, Revisions,
                                        ActiveSelection, HoveredSelection, PendingSelection))
        {
            PointerTaken = true;
            SemanticSelection = {};
        }
    }

    if (!PointerTaken && !Transform.Engaged() && Probed && Pointer.ContactPressed &&
        (ActiveTool == ParametricToolSubject::Trim || ActiveTool == ParametricToolSubject::Extend ||
         ActiveTool == ParametricToolSubject::Offset || ActiveTool == ParametricToolSubject::Fillet ||
         ActiveTool == ParametricToolSubject::Chamfer || ActiveTool == ParametricToolSubject::Cut ||
         ActiveTool == ParametricToolSubject::LinearArray || ActiveTool == ParametricToolSubject::Mirror))
    {
        const SketchPick EditSelection = ResolveViewportEditSelection(ActiveTool, ActiveSelection, HoveredSelection);
        if (ApplyViewportEditTool(ActiveTool, Probe, Basis, Naming, Sketch, Records, Revisions,
                                  EditSelection, CornerDistance, KeepStart, PendingSelection))
        {
            PointerTaken = true;
            SemanticSelection = {};
        }
    }

    // 🔴 A HIDDEN GIZMO OFFERS NO HANDLES. Resolving them anyway would leave invisible handles over the
    //    geometry, swallowing presses meant for the shapes underneath — which is exactly the complaint an
    //    artist turns the gizmo off to avoid.
    GizmoHandle HoveredHandle = GizmoHandle::None;
    if (Gizmo.Shown && !Transform.Engaged() && ActiveSelection.Standing() && SelectStanding)
    {
        GizmoScreenBasis Screen = {};
        if (ResolveGizmoScreenBasis(Basis, View, Perspective, Extent, ActiveSelection.Position, Screen))
            HoveredHandle = Transform.Engaged()
                          ? ResolveGizmoHandle(Screen, Transform.Manner(), Pointer.PositionX, Pointer.PositionY)
                          : ResolveUniversalGizmoHandle(Screen, Pointer.PositionX, Pointer.PositionY);
    }

    if (!PointerTaken && !Transform.Engaged() && SelectStanding &&
        Pointer.ContactPressed && HoveredHandle == GizmoHandle::None && HoveredSelection.Standing())
    {
        SemanticSelection = HoveredSelection;
        PendingSelection = HoveredSelection.Record;
        PointerTaken = true;
    }

    const TransformCommandIntake Command =
        ResolveTransformCommand(TextInput.Intake, TextInput.IntakeCount, Transform.Engaged(), Transform.Manner());

    if (!Transform.Engaged() && SelectStanding && ActiveSelection.Standing() &&
        Extent.Encloses(Pointer.PositionX, Pointer.PositionY))
    {
        if (HoveredHandle == GizmoHandle::None && !PointerTaken && Pointer.ContactHeld &&
            !Pointer.ContactPressed && (std::fabs(Pointer.TravelX) + std::fabs(Pointer.TravelY)) > 0.0f &&
            HoveredSelection.Standing() && SamePickIdentity(HoveredSelection, ActiveSelection))
        {
            PointerTaken = StartTransformSession(Sketch, Records, Basis, View, Perspective, Extent,
                                                 Pointer.PositionX, Pointer.PositionY, ActiveSelection,
                                                 TransformManner::Move, TransformRestriction::Free,
                                                 false, true, Transform);
        }

        if (HoveredHandle != GizmoHandle::None && Pointer.ContactPressed)
        {
            // 📝 A handle names both what it does and what it restricts, so the two are read from it
            //    rather than reconstructed by a switch at the call site.
            const TransformManner Mode = ResolveHandleManner(HoveredHandle);
            const TransformRestriction Constraint = ResolveHandleRestriction(HoveredHandle);
            const bool Slide = false;

            PointerTaken = StartTransformSession(Sketch, Records, Basis, View, Perspective, Extent,
                                                 Pointer.PositionX, Pointer.PositionY, ActiveSelection,
                                                 Mode, Constraint, Slide, true, Transform);
        }
        else if (Command.StartRequested)
        {
            const bool Slide = Command.StartManner == TransformManner::Move
                            && ResolveSlideRequested(Command.MoveTapCount,
                                                         SessionMilliseconds,
                                                         LastGPressedMilliseconds,
                                                         ActiveSelection.Curve.Assigned());
            if (Command.MoveTapCount > 0u)
                LastGPressedMilliseconds = SessionMilliseconds;
            PointerTaken = StartTransformSession(Sketch, Records, Basis, View, Perspective, Extent,
                                                 Pointer.PositionX, Pointer.PositionY, ActiveSelection,
                                                 Command.StartManner,
                                                 Slide ? TransformRestriction::Curve
                                                       : (Command.StartManner == TransformManner::Rotate
                                                            ? TransformRestriction::Screen
                                                            : TransformRestriction::Free),
                                                 Slide, false, Transform);
        }
    }

    if (Transform.Engaged())
    {
        const bool SlideRequested = Transform.Manner() == TransformManner::Move
                                 && ResolveSlideRequested(Command.MoveTapCount,
                                                              SessionMilliseconds,
                                                              LastGPressedMilliseconds,
                                                              Transform.Target.Curve.Assigned());
        if (Transform.Manner() == TransformManner::Move && Command.MoveTapCount > 0u)
            LastGPressedMilliseconds = SessionMilliseconds;

        if (SlideRequested)
        {
            Transform.Restriction() = TransformRestriction::Curve;
            Transform.SlideAlongCurve() = true;
        }
        else if (Command.RestrictionRequested)
        {
            Transform.Restriction() = Command.Restriction;
            Transform.SlideAlongCurve() = false;
        }

        if (Command.NumericAppend[0] != '\0')
            AppendTransformNumericRun(Transform.Standing.Numeric, TransformNumericLimit, Command.NumericAppend);
        if (TextInput.BackspacePressed)
            RetractTransformCommand(Transform.Standing);
        if (TextInput.DeletePressed)
            ClearTransformNumeric(Transform.Standing);

        if (TextInput.CancelPressed)
        {
            CancelTransformSession(Sketch, Transform);
            PointerTaken = true;
        }
        else
        {
            UpdateTransformSession(Basis, View, Perspective, Extent,
                                   Pointer.PositionX, Pointer.PositionY, Modifiers.Commanded,
                                   Sketch, Transform);
            PointerTaken = true;

            if (Transform.AwaitingRelease)
            {
                if (Pointer.ContactReleased)
                    CommitTransformSession(Records, Revisions, Transform);
            }
            else if (TextInput.AcceptPressed || Pointer.ContactPressed)
            {
                CommitTransformSession(Records, Revisions, Transform);
            }
        }
    }

    RecordViewportSelectionOverlay(Overlay, Extent, Basis, View, Perspective,
                                   Sketch, Records, HoveredSelection, ActiveSelection);
    // 📝 The selection outline is drawn either way: knowing WHAT is selected is not the same question as
    //    wanting handles on it, and hiding the gizmo must not hide the selection.
    if (Gizmo.Shown && SelectStanding)
        RecordViewportGizmo(Overlay, Extent, Basis, View, Perspective,
                            ActiveSelection, HoveredHandle, Transform);
}

void DriveViewportSelectionAndTransformWorldBacked(const PlaneExtent& Extent,
                                                   const PointerCondition& Pointer,
                                                   const TextInputCondition& TextInput,
                                                   const SelectionOptions& Selection,
                                                   const GizmoOptions& Gizmo,
                                                   const ResolvedCamera& Camera,
                                                   const WorkspaceDirectoryProjection& Directory,
                                                   const ParametricWorkspaceContext& WorkspaceApplied,
                                                   SketchStructure& Sketch,
                                                   WorldDraftStructure& World,
                                                   WorldDraftSketchMapping& Mapping,
                                                   WorkspaceRecordStructure& Records,
                                                   WorkspaceRevisionSequence& Revisions,
                                                   WorkspaceRecordName& PendingSelection,
                                                   SketchPick& SemanticSelection,
                                                   SketchPick& HoveredSelection,
                                                   WorldDraftTransformSession& Transform,
                                                   OverlayGeometry& Overlay,
                                                   bool& PointerTaken,
                                                   double SessionMilliseconds,
                                                   double& LastGPressedMilliseconds)
{
    const WorkspaceRecordName SelectedRecord = SelectedRecordIn(Directory, WorkspaceApplied);
    if (ApplyDimensionTextEdit(TextInput, Sketch, Records, Revisions, SelectedRecord))
    {
        MirrorSketchIntoWorldDraft(Sketch, World, Mapping);
        PointerTaken = true;
    }
    if (SemanticSelection.Standing() && SelectedRecord.Assigned() &&
        SemanticSelection.Record.IssuedIndex != SelectedRecord.IssuedIndex &&
        (!PendingSelection.Assigned() || PendingSelection.IssuedIndex != SemanticSelection.Record.IssuedIndex))
        SemanticSelection = {};

    const SketchPick ActiveSketchSelection =
        EditableSelection(Sketch, Records, SelectedRecord, PendingSelection, SemanticSelection);

    if (TextInput.DeletePressed && ActiveSketchSelection.Record.Assigned())
    {
        Discard(Records.ToggleVisible(ActiveSketchSelection.Record, false));
        Revisions.Seal("Deleted selected sketch record", "Delete Sketch Record", { ActiveSketchSelection.Record },
                       Revisions.DeclaredCount() + 1u);
        PendingSelection = {};
        SemanticSelection = {};
        HoveredSelection = {};
        PointerTaken = true;
        return;
    }
    MirrorSketchIntoWorldDraft(Sketch, World, Mapping);

    WorldPick WorldSemantic = {};
    WorldPick WorldHovered = {};
    if (ActiveSketchSelection.Standing())
        ResolveWorldPickForSketchPick(Sketch, Records, World, Mapping, ActiveSketchSelection, WorldSemantic);

    const bool WasEngaged = Transform.Engaged();
    const bool WasAwaitingRelease = Transform.AwaitingRelease;
    const bool WasChanged = Transform.Changed;
    const TransformManner WasManner = Transform.Manner();

    GizmoHandle HoveredHandle = GizmoHandle::None;
    DriveWorldDraftSelectionAndTransform(Extent, Pointer, TextInput,
                                         Selection, Gizmo, Camera,
                                         World, WorldSemantic, WorldHovered,
                                         Transform, PointerTaken,
                                         SessionMilliseconds, LastGPressedMilliseconds,
                                         &HoveredHandle);

    ApplyWorldDraftToSketch(World, Sketch);

    if (!ResolveSketchPickForWorldPick(Sketch, Records, Mapping, WorldSemantic, SemanticSelection))
        SemanticSelection = {};
    if (!ResolveSketchPickForWorldPick(Sketch, Records, Mapping, WorldHovered, HoveredSelection))
        HoveredSelection = {};

    if (SemanticSelection.Record.Assigned())
        PendingSelection = SemanticSelection.Record;

    const bool Committed = WasEngaged && WasChanged && !Transform.Engaged() && !TextInput.CancelPressed &&
                         ((WasAwaitingRelease && Pointer.ContactReleased)
                       || (!WasAwaitingRelease && (TextInput.AcceptPressed || Pointer.ContactPressed)));
    if (Committed && SemanticSelection.Record.Assigned())
    {
        const WorkspaceRecord* Record = Records.Resolve(SemanticSelection.Record);
        if (Record != nullptr)
            Revisions.Seal(std::string(TransformMannerText(WasManner)) + " " + Record->Naming,
                           "Edit Sketch", { SemanticSelection.Record }, Revisions.DeclaredCount() + 1u);
    }

    RecordViewportSelectionOverlay(Overlay, Extent, Camera, World, WorldHovered, WorldSemantic);
    if (Gizmo.Shown && WorldSemantic.Standing())
        RecordViewportGizmo(Overlay, Extent, Camera, WorldSemantic, HoveredHandle, Transform);
}

}   // namespace Slate
