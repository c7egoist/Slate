// 🧩 Phase-6 proof for host-style selection and transform flow on the world sketch.

#include "SlateWorkspace/Discipline/WorldSketchInteraction/Api/WorldSketchInteraction.h"

#include <cmath>
#include <cstdio>

using namespace Slate;

namespace
{

std::uint32_t Claims = 0u;
std::uint32_t Failures = 0u;

void Claim(bool Held, const char* Sentence)
{
    ++Claims;
    if (!Held)
    {
        ++Failures;
        std::printf("  FAILED  %s\n", Sentence);
    }
}

bool Near(double Left, double Right, double Tolerance = 1.0e-4)
{
    return std::fabs(Left - Right) <= Tolerance;
}

bool SamePoint(const SpatialPoint& Left,
               const SpatialPoint& Right,
               double Tolerance = 1.0e-4)
{
    return Near(Left.Left, Right.Left, Tolerance)
        && Near(Left.Up, Right.Up, Tolerance)
        && Near(Left.Forward, Right.Forward, Tolerance);
}

struct Bench
{
    WorldSketchStructure Sketch;
    PlaneExtent Extent = { 0.0f, 0.0f, 800.0f, 600.0f };
    ResolvedCamera Perspective = ResolveFreeCamera({ 0.0, 50.0, -300.0 }, 0.0, 0.0, 60.0, true, 1.0);
    ResolvedCamera Ortho = ResolveFreeCamera({ -250.0, 160.0, -250.0 }, 45.0, -20.0, 60.0, false, 3.0);
    SelectionOptions Selection = {};
    GizmoOptions Gizmo = {};
    WorldPick SemanticSelection = {};
    WorldPick HoveredSelection = {};
    WorldSketchTransformSession Transform = {};
    double LastGPressedMilliseconds = 0.0;

    WorldPlacementFrame Front = {{ 0.0, 0.0, 40.0 }, { 0.0, 0.0, 1.0 }, { 1.0, 0.0, 0.0 }};
    WorldCurveName AB = {};
    WorldCurveName BC = {};
    WorldCurveName CD = {};
    WorldCurveName DA = {};
    WorldLoopName Loop = {};

    Bench()
    {
        Selection.Element = SelectionElement::Edge;
        Selection.Tolerance = 8.0f;
        Gizmo.Shown = true;

        AB = Sketch.DeclareLine({ 0.0, 0.0, 40.0 }, { 100.0, 0.0, 40.0 }, Front);
        BC = Sketch.DeclareLine({ 100.0, 0.0, 40.0 }, { 100.0, 100.0, 40.0 }, Front);
        CD = Sketch.DeclareLine({ 100.0, 100.0, 40.0 }, { 0.0, 100.0, 40.0 }, Front);
        DA = Sketch.DeclareLine({ 0.0, 100.0, 40.0 }, { 0.0, 0.0, 40.0 }, Front);
        Loop = Sketch.DeclareLoop({ { { AB, true }, { BC, true }, { CD, true }, { DA, true } } });
    }

    WorldPick EdgePick() const
    {
        WorldPick Pick = {};
        Pick.Subject = WorldPickSubject::Curve;
        Pick.Curve = BC;
        ResolveWorldCurvePivot(Sketch, BC, Pick.Position);
        return Pick;
    }

    WorldPick VertexPick() const
    {
        std::vector<WorldPointPlacement> Points;
        ResolveWorldSketchPoints(Sketch, BC, Points);
        WorldPick Pick = {};
        if (Points.size() >= 2u)
        {
            Pick.Subject = WorldPickSubject::Point;
            Pick.Point = Points[1u].Name;
            Pick.Curve = BC;
            Pick.Position = Points[1u].Position;
        }
        return Pick;
    }

    void Drive(const PointerCondition& Pointer,
               const TextInputCondition& Text,
               const ResolvedCamera& Camera,
               double Milliseconds,
               bool& PointerTaken,
               GizmoHandle* HoveredHandle = nullptr)
    {
        DriveWorldSketchSelectionAndTransform(Extent, Pointer, Text,
                                             Selection, Gizmo, Camera,
                                             Sketch, SemanticSelection, HoveredSelection,
                                             Transform, PointerTaken,
                                             Milliseconds, LastGPressedMilliseconds,
                                             HoveredHandle);
    }
};

void ProveHoverAndClickSelection()
{
    std::printf("\n1. Hover and click selection are routed through the world picker\n");

    Bench Stage;
    const WorldPick Edge = Stage.EdgePick();
    float X = 0.0f;
    float Y = 0.0f;
    Claim(ProjectFromCamera(Stage.Perspective, Stage.Extent, Edge.Position, X, Y),
          "the edge pivot projects for hovering");

    PointerCondition Hover = {};
    Hover.PositionX = X;
    Hover.PositionY = Y;
    bool PointerTaken = false;
    GizmoHandle HoveredHandle = GizmoHandle::None;
    Stage.Drive(Hover, {}, Stage.Perspective, 1000.0, PointerTaken, &HoveredHandle);

    Claim(Stage.HoveredSelection.Subject == WorldPickSubject::Curve,
          "hovering in edge mode resolves a world curve selection");
    Claim(Stage.HoveredSelection.Curve.IssuedIndex == Stage.BC.IssuedIndex,
          "and it is the expected rectangle edge");
    Claim(HoveredHandle == GizmoHandle::None,
          "with nothing selected yet the gizmo offers no hovered handle");

    PointerCondition Click = Hover;
    Click.ContactPressed = true;
    PointerTaken = false;
    Stage.Drive(Click, {}, Stage.Perspective, 1016.0, PointerTaken, &HoveredHandle);
    Claim(PointerTaken,
          "clicking the hovered world edge consumes the press");
    Claim(Stage.SemanticSelection.Subject == WorldPickSubject::Curve,
          "and the hovered edge becomes the standing semantic selection");
    Claim(Stage.SemanticSelection.Curve.IssuedIndex == Stage.BC.IssuedIndex,
          "with the same curve identity carried into the standing selection");
}

void ProveMouseDragAndReleaseCommit()
{
    std::printf("\n2. Mouse dragging the selected edge starts a move session and release commits it\n");

    Bench Stage;
    Stage.Gizmo.Shown = false;
    const WorldPick Edge = Stage.EdgePick();
    float StartX = 0.0f;
    float StartY = 0.0f;
    float EndX = 0.0f;
    float EndY = 0.0f;
    Claim(ProjectFromCamera(Stage.Perspective, Stage.Extent, Edge.Position, StartX, StartY),
          "the selected edge pivot projects for dragging");
    Claim(ProjectFromCamera(Stage.Perspective, Stage.Extent, { 130.0, 70.0, 40.0 }, EndX, EndY),
          "and the aimed drag target projects too");

    Stage.SemanticSelection = Edge;
    RefreshWorldSketchPick(Stage.Sketch, Stage.SemanticSelection);

    PointerCondition Arm = {};
    Arm.PositionX = StartX;
    Arm.PositionY = StartY;
    Arm.ContactHeld = true;
    Arm.TravelX = 1.0f;
    bool PointerTaken = false;
    Stage.Drive(Arm, {}, Stage.Perspective, 2000.0, PointerTaken);
    Claim(Stage.Transform.Engaged(),
          "a held drag over the selected edge starts a world transform session");
    Claim(Stage.Transform.AwaitingRelease,
          "and a pointer-driven session waits for release before committing");

    PointerCondition Drag = Arm;
    Drag.PositionX = EndX;
    Drag.PositionY = EndY;
    Drag.TravelX = EndX - StartX;
    Drag.TravelY = EndY - StartY;
    PointerTaken = false;
    Stage.Drive(Drag, {}, Stage.Perspective, 2016.0, PointerTaken);

    const DeclaredWorldCurve* HeldBC = Stage.Sketch.Resolve(Stage.BC);
    const DeclaredWorldCurve* HeldAB = Stage.Sketch.Resolve(Stage.AB);
    const DeclaredWorldCurve* HeldCD = Stage.Sketch.Resolve(Stage.CD);
    Claim(HeldBC != nullptr && SamePoint(HeldBC->Geometry.HeldLine().Origin, { 130.0, 20.0, 40.0 })
                         && SamePoint(HeldBC->Geometry.HeldLine().Terminus, { 130.0, 120.0, 40.0 }),
          "the selected world edge moves under the drag");
    Claim(HeldAB != nullptr && SamePoint(HeldAB->Geometry.HeldLine().Terminus, { 130.0, 20.0, 40.0 })
                         && HeldCD != nullptr && SamePoint(HeldCD->Geometry.HeldLine().Origin, { 130.0, 120.0, 40.0 }),
          "and the shared corners stay welded to the neighbouring edges");

    PointerCondition Release = Drag;
    Release.ContactHeld = false;
    Release.ContactReleased = true;
    Release.TravelX = Release.TravelY = 0.0f;
    PointerTaken = false;
    Stage.Drive(Release, {}, Stage.Perspective, 2032.0, PointerTaken);
    Claim(!Stage.Transform.Engaged(),
          "releasing the pointer commits and closes the world transform session");
    Claim(Stage.SemanticSelection.Subject == WorldPickSubject::Curve
       && Stage.SemanticSelection.Curve.IssuedIndex == Stage.BC.IssuedIndex,
          "the same edge remains selected after the commit");
    Claim(SamePoint(Stage.SemanticSelection.Position, { 130.0, 70.0, 40.0 }),
          "and the standing selection refreshes to the edge's new pivot position");
}

void ProveGizmoAxisStartAndCancel()
{
    std::printf("\n3. A gizmo handle starts an axis-locked move and escape restores it\n");

    Bench Stage;
    Stage.SemanticSelection = Stage.EdgePick();
    RefreshWorldSketchPick(Stage.Sketch, Stage.SemanticSelection);

    GizmoScreenBasis Screen = {};
    Claim(ResolveGizmoScreenBasis(Stage.Perspective, Stage.Extent, Stage.SemanticSelection.Position, Screen),
          "the selected edge can resolve a gizmo screen basis from the active camera");

    PointerCondition Hover = {};
    Hover.PositionX = Screen.PivotX + Screen.AlongX * static_cast<float>(GizmoMeasure::AxisEnd - GizmoMeasure::ConeLength * 0.5);
    Hover.PositionY = Screen.PivotY + Screen.AlongY * static_cast<float>(GizmoMeasure::AxisEnd - GizmoMeasure::ConeLength * 0.5);
    bool PointerTaken = false;
    GizmoHandle HoveredHandle = GizmoHandle::None;
    Stage.Drive(Hover, {}, Stage.Perspective, 3000.0, PointerTaken, &HoveredHandle);
    Claim(HoveredHandle == GizmoHandle::MoveX,
          "hovering the X arrow resolves the X move gizmo handle");

    PointerCondition Press = Hover;
    Press.ContactPressed = true;
    PointerTaken = false;
    Stage.Drive(Press, {}, Stage.Perspective, 3016.0, PointerTaken, &HoveredHandle);
    Claim(Stage.Transform.Engaged(),
          "pressing the hovered X handle starts a transform session");
    Claim(Stage.Transform.Restriction() == TransformRestriction::AxisX,
          "and that session is axis-locked to X from the handle it came from");

    PointerCondition Drag = Hover;
    Drag.ContactHeld = true;
    float EndX = 0.0f;
    float EndY = 0.0f;
    Claim(ProjectFromCamera(Stage.Perspective, Stage.Extent, { 125.0, 50.0, 40.0 }, EndX, EndY),
          "an X-only target projects for the gizmo drag");
    Drag.PositionX = EndX;
    Drag.PositionY = EndY;
    Drag.TravelX = EndX - Hover.PositionX;
    Drag.TravelY = EndY - Hover.PositionY;
    PointerTaken = false;
    Stage.Drive(Drag, {}, Stage.Perspective, 3032.0, PointerTaken, &HoveredHandle);

    const DeclaredWorldCurve* HeldBC = Stage.Sketch.Resolve(Stage.BC);
    const double ExpectedX = 100.0 + Stage.Transform.PreviewValue;
    Claim(HeldBC != nullptr
       && Near(HeldBC->Geometry.HeldLine().Origin.Left, ExpectedX)
       && Near(HeldBC->Geometry.HeldLine().Terminus.Left, ExpectedX)
       && Near(HeldBC->Geometry.HeldLine().Origin.Up, 0.0)
       && Near(HeldBC->Geometry.HeldLine().Terminus.Up, 100.0)
       && Near(HeldBC->Geometry.HeldLine().Origin.Forward, 40.0)
       && Near(HeldBC->Geometry.HeldLine().Terminus.Forward, 40.0),
          "the X-handle drag moves the edge only along world X");

    TextInputCondition Cancel = {};
    Cancel.CancelPressed = true;
    PointerCondition Still = Drag;
    PointerTaken = false;
    Stage.Drive(Still, Cancel, Stage.Perspective, 3048.0, PointerTaken, &HoveredHandle);
    HeldBC = Stage.Sketch.Resolve(Stage.BC);
    Claim(HeldBC != nullptr && SamePoint(HeldBC->Geometry.HeldLine().Origin, { 100.0, 0.0, 40.0 })
                         && SamePoint(HeldBC->Geometry.HeldLine().Terminus, { 100.0, 100.0, 40.0 }),
          "escape cancels the gizmo drag and restores the original edge");
}

void ProveKeyboardRestrictionAndNumeric()
{
    std::printf("\n4. Keyboard move grammar drives world-axis restriction and numeric distance\n");

    Bench Stage;
    Stage.Selection.Element = SelectionElement::Object;
    WorldPick Whole = {};
    Whole.Subject = WorldPickSubject::Loop;
    Whole.Loop = Stage.Loop;
    ResolveWorldLoopPivot(Stage.Sketch, Stage.Loop, Whole.Position);
    Stage.SemanticSelection = Whole;

    PointerCondition Pointer = {};
    float PivotX = 0.0f;
    float PivotY = 0.0f;
    Claim(ProjectFromCamera(Stage.Ortho, Stage.Extent, Whole.Position, PivotX, PivotY),
          "the loop pivot projects for a keyboard-started transform");
    Pointer.PositionX = PivotX;
    Pointer.PositionY = PivotY;

    TextInputCondition Start = {};
    Start.Intake[0] = 'g';
    Start.Intake[1] = '\0';
    Start.IntakeCount = 1u;
    bool PointerTaken = false;
    Stage.Drive(Pointer, Start, Stage.Ortho, 4000.0, PointerTaken);
    Claim(Stage.Transform.Engaged(),
          "typing G with a standing selection starts a world move session");
    Claim(!Stage.Transform.AwaitingRelease,
          "and a keyboard-started world move does not wait for a pointer release");

    TextInputCondition RestrictAndAmount = {};
    RestrictAndAmount.Intake[0] = 'z';
    RestrictAndAmount.Intake[1] = '6';
    RestrictAndAmount.Intake[2] = '0';
    RestrictAndAmount.Intake[3] = '\0';
    RestrictAndAmount.IntakeCount = 3u;
    PointerTaken = false;
    Stage.Drive(Pointer, RestrictAndAmount, Stage.Ortho, 4016.0, PointerTaken);

    const DeclaredWorldCurve* HeldAB = Stage.Sketch.Resolve(Stage.AB);
    Claim(Stage.Transform.Restriction() == TransformRestriction::AxisZ,
          "typing Z during the session locks it to world Z");
    Claim(HeldAB != nullptr && SamePoint(HeldAB->Geometry.HeldLine().Origin, { 0.0, 0.0, 100.0 })
                         && SamePoint(HeldAB->Geometry.HeldLine().Terminus, { 100.0, 0.0, 100.0 }),
          "and the numeric distance moves the selected loop sixty units on world Z");

    TextInputCondition Accept = {};
    Accept.AcceptPressed = true;
    PointerTaken = false;
    Stage.Drive(Pointer, Accept, Stage.Ortho, 4032.0, PointerTaken);
    Claim(!Stage.Transform.Engaged(),
          "accept commits the keyboard-driven world move session");
}

void ProveCurveSlideGesture()
{
    std::printf("\n5. A fast second G starts slide-along-curve instead of free drag\n");

    WorldSketchStructure Sketch;
    const WorldPlacementFrame Support = {{}, { 0.0, 1.0, 0.0 }, { 1.0, 0.0, 0.0 }};
    const WorldCurveName Diagonal = Sketch.DeclareLine({ 0.0, 0.0, 0.0 }, { 100.0, 0.0, 100.0 }, Support);

    const PlaneExtent Extent = { 0.0f, 0.0f, 800.0f, 600.0f };
    const ResolvedCamera Camera = ResolveFreeCamera({ -220.0, 120.0, -220.0 }, 45.0, -20.0, 60.0, false, 3.0);
    SelectionOptions Selection = {};
    Selection.Element = SelectionElement::Edge;
    GizmoOptions Gizmo = {};
    WorldPick Semantic = {};
    Semantic.Subject = WorldPickSubject::Curve;
    Semantic.Curve = Diagonal;
    ResolveWorldCurvePivot(Sketch, Diagonal, Semantic.Position);
    WorldPick Hovered = {};
    WorldSketchTransformSession Transform = {};
    double LastG = 1000.0;
    bool PointerTaken = false;

    float PivotX = 0.0f;
    float PivotY = 0.0f;
    Claim(ProjectFromCamera(Camera, Extent, Semantic.Position, PivotX, PivotY),
          "the diagonal curve pivot projects for the slide gesture");

    PointerCondition Pointer = {};
    Pointer.PositionX = PivotX;
    Pointer.PositionY = PivotY;
    TextInputCondition Start = {};
    Start.Intake[0] = 'g';
    Start.Intake[1] = '\0';
    Start.IntakeCount = 1u;
    DriveWorldSketchSelectionAndTransform(Extent, Pointer, Start,
                                         Selection, Gizmo, Camera,
                                         Sketch, Semantic, Hovered, Transform,
                                         PointerTaken, 1200.0, LastG, nullptr);
    Claim(Transform.Engaged(),
          "a second G within the tap window starts a session");
    Claim(Transform.Restriction() == TransformRestriction::Curve && Transform.SlideAlongCurve(),
          "and that session begins in slide-along-curve mode");

    const SpatialDirection Slide = ResolveWorldCurveSlideDirection(Sketch, Diagonal, Semantic.Position);
    float EndX = 0.0f;
    float EndY = 0.0f;
    Claim(ProjectFromCamera(Camera, Extent, Added(Semantic.Position, Scaled(Slide, 25.0)), EndX, EndY),
          "a point along the curve tangent projects for the slide update");

    Pointer.ContactHeld = true;
    Pointer.PositionX = EndX;
    Pointer.PositionY = EndY;
    Pointer.TravelX = EndX - PivotX;
    Pointer.TravelY = EndY - PivotY;
    PointerTaken = false;
    DriveWorldSketchSelectionAndTransform(Extent, Pointer, {},
                                         Selection, Gizmo, Camera,
                                         Sketch, Semantic, Hovered, Transform,
                                         PointerTaken, 1216.0, LastG, nullptr);

    const DeclaredWorldCurve* Held = Sketch.Resolve(Diagonal);
    Claim(Held != nullptr
       && SamePoint(Held->Geometry.HeldLine().Origin,
                    Added(SpatialPoint{ 0.0, 0.0, 0.0 }, Scaled(Slide, 25.0)))
       && SamePoint(Held->Geometry.HeldLine().Terminus,
                    Added(SpatialPoint{ 100.0, 0.0, 100.0 }, Scaled(Slide, 25.0))),
          "updating the slide session moves the curve only along its own direction");
}

} // namespace

int main()
{
    std::printf("=========================================================================\n");
    std::printf("WORLD SKETCH INTERACTION PROOF\n");
    std::printf("=========================================================================\n");

    ProveHoverAndClickSelection();
    ProveMouseDragAndReleaseCommit();
    ProveGizmoAxisStartAndCancel();
    ProveKeyboardRestrictionAndNumeric();
    ProveCurveSlideGesture();

    std::printf("\n=========================================================================\n");
    std::printf("%u claims, %u failures -> %s\n", Claims, Failures,
                Failures == 0u ? "PROVEN" : "REFUTED");
    std::printf("=========================================================================\n");
    return Failures == 0u ? 0 : 1;
}
