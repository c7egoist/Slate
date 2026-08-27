//============================================================================================================================================
//                                                       SKETCHINTERACTION.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/SketchInteraction/Api/SketchInteraction.h"

#include "SlateShape/Sketch/DimensionSolver/Api/DimensionSolver.h"
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

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace Slate
{

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
        if (Perspective)
            View.Distance = std::clamp(View.Distance * (Pointer.WheelY > 0.0f ? 0.9 : 1.1), 20.0, 4000.0);
        else
            View.OrthoScale = std::clamp(View.OrthoScale * (Pointer.WheelY > 0.0f ? 1.1 : 0.9), 0.05, 40.0);
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
    if (ToolContext.ActiveSubject != ParametricToolSubject::Workplane &&
        ToolContext.ActiveSubject != ParametricToolSubject::DatumPlane)
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

    // 📝 The sketch adopts the plane that is now active. Existing curves keep their world coordinates and
    //    do not move; what changes is only the surface the NEXT thing is drawn on.
    Sketch.DeclarePlane({ Workplanes.Active().Origin,
                          Workplanes.Active().Normal,
                          Workplanes.Active().Along });

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
    const SketchSnapPlacement Placement = Modifiers.Commanded
                                        ? SketchSnapPlacement{}
                                        : ResolveNearestSnap(Sketch, Raw, SnapTolerance);

    SpatialPoint Hover = Placement.Resolved() ? Placement.Position : Raw;
    Hover = ApplySketchToolSettings(Tool, Basis, ToolContext, Hover);
    Tool.Hover(Hover, Placement);

    // 🔴 One arrival, one response, for every subject. The keyboard accept and the pointer press differ
    //    only in whether the contact terminates a growing curve — Enter always does, a press does so only
    //    on a double-press. Nothing below names a subject.
    if (!Text.AcceptPressed && !Pointer.ContactPressed)
        return;

    // 🔴 The first placement adopts whatever plane the catalogue says is ACTIVE, which is the ground
    //    plane until the artist chooses otherwise. The shipped code hardcoded the ground plane here, so
    //    activating another plane and then drawing put the geometry on the ground anyway.
    if (!Sketch.Declared())
        Sketch.DeclarePlane({ Workplanes.Active().Origin,
                              Workplanes.Active().Normal,
                              Workplanes.Active().Along });

    const bool Terminating = Text.AcceptPressed || Pointer.ContactDoublePressed;

    PointerTaken = true;
    if (Tool.Anchor(Terminating) != PlacementArrival::Complete)
        return;

    const SealedPlacement Sealed = Tool.Seal();
    const Deliver<WorkspaceRecordName> Record = CommitPlacement(Naming, Sketch, Records, Revisions, Sealed);
    AdoptCommittedShape(Sealed.Subject, Naming, Sketch, Records, Revisions, Record, PendingSelection);
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
                           const SketchPick& ActiveSelection,
                           WorkspaceRecordName& PendingSelection)
{
    if (!ActiveSelection.Curve.Assigned() || !ActiveSelection.Record.Assigned())
        return false;

    std::vector<WorkspaceRecordName> Written;
    if (Tool == ParametricToolSubject::Trim)
    {
        SketchSnapMask TrimMask = {};
        TrimMask.EndpointAccepted = TrimMask.MidpointAccepted = TrimMask.CentreAccepted = false;
        TrimMask.ControlAccepted = TrimMask.AlongCurveAccepted = TrimMask.GridAccepted = false;
        TrimMask.PerpendicularAccepted = TrimMask.TangentAccepted = false;
        TrimMask.IntersectionAccepted = true;
        const SketchSnapPlacement TrimSnap = ResolveNearestSnap(Sketch, Probe, 25.0, TrimMask);
        const SpatialPoint TrimPosition = TrimSnap.Resolved() ? TrimSnap.Position : Probe;
        const Deliver<SketchCurveName> Trimmed = TrimCurve(Sketch, ActiveSelection.Curve, TrimPosition, true);
        if (!Trimmed.Resolved)
            return false;
        Discard(Records.ToggleVisible(ActiveSelection.Record, false));
        CommitCurveSet(Naming, Records, Revisions, { Trimmed.Resolve() }, "Trim Curve", Written);
    }
    else if (Tool == ParametricToolSubject::Extend)
    {
        SketchSnapMask ExtendMask = {};
        ExtendMask.EndpointAccepted = ExtendMask.MidpointAccepted = ExtendMask.CentreAccepted = false;
        ExtendMask.ControlAccepted = ExtendMask.AlongCurveAccepted = ExtendMask.GridAccepted = false;
        ExtendMask.PerpendicularAccepted = ExtendMask.TangentAccepted = false;
        ExtendMask.IntersectionAccepted = true;
        const SketchSnapPlacement ExtendSnap = ResolveNearestSnap(Sketch, Probe, 1000.0, ExtendMask);
        const SpatialPoint ExtendPosition = ExtendSnap.Resolved() ? ExtendSnap.Position : Probe;
        const Deliver<SketchCurveName> Extended = TrimCurve(Sketch, ActiveSelection.Curve, ExtendPosition, false);
        if (!Extended.Resolved)
            return false;
        Discard(Records.ToggleVisible(ActiveSelection.Record, false));
        CommitCurveSet(Naming, Records, Revisions, { Extended.Resolve() }, "Extend Curve", Written);
    }
    else if (Tool == ParametricToolSubject::Offset)
    {
        const SpatialDirection Offset = Difference(ActiveSelection.Position, Probe);
        const Deliver<PatternResult> Duplicated = DuplicateCurves(Sketch, { ActiveSelection.Curve }, Offset);
        if (!Duplicated.Resolved)
            return false;
        CommitCurveSet(Naming, Records, Revisions, Duplicated.Resolve().CurveSet, "Offset Curve", Written);
    }
    else if (Tool == ParametricToolSubject::LinearArray)
    {
        const SpatialDirection Step = Added(Scaled(Basis.Along, 40.0), Scaled(Basis.Across, 0.0));
        const Deliver<PatternResult> Pattern = DeclareLinearPattern(Sketch, { ActiveSelection.Curve }, Step, 3u);
        if (!Pattern.Resolved)
            return false;
        CommitCurveSet(Naming, Records, Revisions, Pattern.Resolve().CurveSet, "Linear Pattern", Written);
    }
    else if (Tool == ParametricToolSubject::Mirror)
    {
        const SpatialPoint AxisStart = Added(Probe, Scaled(Basis.Across, -100.0));
        const SpatialPoint AxisEnd = Added(Probe, Scaled(Basis.Across, 100.0));
        const Deliver<PatternResult> Mirrored = MirrorCurves(Sketch, { ActiveSelection.Curve }, AxisStart, AxisEnd);
        if (!Mirrored.Resolved)
            return false;
        CommitCurveSet(Naming, Records, Revisions, Mirrored.Resolve().CurveSet, "Mirror Curve", Written);
    }
    else if (Tool == ParametricToolSubject::Fillet || Tool == ParametricToolSubject::Chamfer)
    {
        const Deliver<std::vector<SketchCurveName>> Cut = CutCurve(Sketch, ActiveSelection.Curve, Probe);
        if (!Cut.Resolved)
            return false;
        Discard(Records.ToggleVisible(ActiveSelection.Record, false));
        CommitCurveSet(Naming, Records, Revisions, Cut.Resolve(),
                       Tool == ParametricToolSubject::Fillet ? "Fillet Preparation" : "Chamfer Preparation",
                       Written);
    }
    else
    {
        return false;
    }

    if (!Written.empty())
        PendingSelection = Written.front();
    return !Written.empty();
}

void DriveViewportSelectionAndTransform(const PlaneExtent& Extent,
                                        const PointerCondition& Pointer,
                                        const TextInputCondition& TextInput,
                                        const ModifierCondition& Modifiers,
                                        const SpatialBasis& Basis,
                                        const ViewportStanding& View,
                                        bool Perspective,
                                        ParametricToolSubject ActiveTool,
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
                                        double& LastGPressedMilliseconds)
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
        HoveredSelection = ResolveSketchPick(Sketch, Records, Probe, ResolveSnapTolerance(View, Perspective));

    const SketchPick ActiveSelection =
        EditableSelection(Sketch, Records, SelectedRecord, PendingSelection, SemanticSelection);

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
         ActiveTool == ParametricToolSubject::Chamfer || ActiveTool == ParametricToolSubject::LinearArray ||
         ActiveTool == ParametricToolSubject::Mirror))
    {
        if (ApplyViewportEditTool(ActiveTool, Probe, Basis, Naming, Sketch, Records, Revisions,
                                  ActiveSelection, PendingSelection))
        {
            PointerTaken = true;
            SemanticSelection = {};
        }
    }

    GizmoHandle HoveredHandle = GizmoHandle::None;
    if (!Transform.Engaged() && ActiveSelection.Standing() && SelectedTool(ActiveTool).Subject == SketchSubject::None)
    {
        GizmoScreenBasis Screen = {};
        if (ResolveGizmoScreenBasis(Basis, View, Perspective, Extent, ActiveSelection.Position, Screen))
            HoveredHandle = ResolveGizmoHandle(Screen, Transform.Manner(), Pointer.PositionX, Pointer.PositionY);
    }

    if (!PointerTaken && !Transform.Engaged() && SelectedTool(ActiveTool).Subject == SketchSubject::None &&
        Pointer.ContactPressed && HoveredHandle == GizmoHandle::None && HoveredSelection.Standing())
    {
        SemanticSelection = HoveredSelection;
        PendingSelection = HoveredSelection.Record;
        PointerTaken = true;
    }

    const TransformCommandIntake Command =
        ResolveTransformCommand(TextInput.Intake, TextInput.IntakeCount, Transform.Engaged(), Transform.Manner());

    if (!Transform.Engaged() && ActiveSelection.Standing() && Extent.Encloses(Pointer.PositionX, Pointer.PositionY))
    {
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
    RecordViewportGizmo(Overlay, Extent, Basis, View, Perspective,
                        ActiveSelection, HoveredHandle, Transform);
}

}   // namespace Slate
