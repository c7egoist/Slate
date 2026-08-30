#include "SlateWorkspace/Discipline/WorldSketchBridge/Api/WorldSketchBridge.h"
#include "SlateWorkspace/Discipline/SketchInteraction/Api/SketchInteraction.h"

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
    PlaneExtent Extent = { 0.0f, 0.0f, 800.0f, 600.0f };
    DrawableScale Drawable = { 1.0 };
    ResolvedCamera Camera = ResolveFreeCamera({ 0.0, 50.0, -300.0 }, 0.0, 0.0, 60.0, true, 1.0);
    SketchStructure Sketch = {};
    WorldSketchStructure World = {};
    WorldSketchMapping Mapping = {};
    WorkspaceRecordStructure Records = {};
    WorkspaceRevisionSequence Revisions = {};
    WorkspaceDirectoryProjection Directory = {};
    ParametricWorkspaceContext WorkspaceApplied = {};
    WorkspaceRecordName PendingSelection = {};
    SketchPick SemanticSelection = {};
    SketchPick HoveredSelection = {};
    WorldSketchTransformSession Transform = {};
    SelectionOptions Selection = {};
    GizmoOptions Gizmo = {};
    double LastGPressedMilliseconds = 0.0;
    OverlayGeometry Overlay = {};
    WorkspaceRecordName ProfileRecord = {};
    SketchCurveName AB = {};
    SketchCurveName BC = {};
    SketchCurveName CD = {};
    SketchCurveName DA = {};

    Bench()
    {
        Sketch.DeclarePlane({ { 0.0, 0.0, 40.0 }, { 0.0, 0.0, 1.0 }, { 1.0, 0.0, 0.0 } });
        AB = Sketch.DeclareLine({ 0.0, 0.0, 40.0 }, { 100.0, 0.0, 40.0 });
        BC = Sketch.DeclareLine({ 100.0, 0.0, 40.0 }, { 100.0, 100.0, 40.0 });
        CD = Sketch.DeclareLine({ 100.0, 100.0, 40.0 }, { 0.0, 100.0, 40.0 });
        DA = Sketch.DeclareLine({ 0.0, 100.0, 40.0 }, { 0.0, 0.0, 40.0 });

        ProfileSpecification Profile;
        Profile.DeclarePlane({ Sketch.HeldPlane().Origin, Sketch.HeldPlane().Normal, Sketch.HeldPlane().AlongDirection });
        ProfileLoop Loop;
        Loop.Orientation = ProfileLoopOrientation::Outer;
        Loop.Traversal = {
            { { AB.IssuedIndex }, true },
            { { BC.IssuedIndex }, true },
            { { CD.IssuedIndex }, true },
            { { DA.IssuedIndex }, true }
        };
        Profile.DeclareLoop(Loop);
        const ProfileNameInFeature DeclaredProfile = Sketch.DeclareProfile(Profile);

        WorkspaceRecord Record = {};
        Record.Subject = WorkspaceRecordSubject::ClosedProfile;
        Record.Naming = "Rectangle";
        Record.Profile = DeclaredProfile;
        ProfileRecord = Records.Declare(Record);

        MirrorSketchIntoWorldSketch(Sketch, World, Mapping);
        ProjectWorkspaceDirectory(Records, Directory);
        for (bool& Held : WorkspaceApplied.RowSelected)
            Held = false;
        WorkspaceApplied.RowTaken = 0u;

        Selection.Element = SelectionElement::Edge;
        Selection.Tolerance = 8.0f;
        Gizmo.Shown = true;
    }

    SpatialPoint EdgePivot() const
    {
        SpatialPoint Pivot = {};
        ResolveCurvePivot(Sketch, BC, Pivot);
        return Pivot;
    }

    void Drive(const PointerCondition& Pointer,
               const TextInputCondition& Text,
               double Milliseconds,
               bool& PointerTaken)
    {
        Overlay.Reset();
        DriveViewportSelectionAndTransformWorldBacked(
            Extent, Pointer, Text, Selection, Gizmo, Camera,
            Directory, WorkspaceApplied,
            Sketch, World, Mapping, Records, Revisions,
            PendingSelection, SemanticSelection, HoveredSelection,
            Transform, Overlay, PointerTaken,
            Milliseconds, LastGPressedMilliseconds);
    }
};

void ProveMirrorAndPickMapping()
{
    std::printf("\n1. Sketch geometry mirrors into a world sketch and picks round-trip\n");

    Bench Stage;

    Claim(Stage.World.CurveCount() == 4u,
          "the sketch's four edges mirror into four persistent world curves");
    Claim(Stage.World.LoopCount() == 1u && Stage.Mapping.Loops.size() == 1u,
          "and the rectangle profile mirrors into one persistent world loop with recorded source mapping");

    SketchPick ProfilePick = {};
    ProfilePick.Subject = SketchPickSubject::Record;
    ProfilePick.Record = Stage.ProfileRecord;
    ResolveProfilePivot(Stage.Sketch, { 1u }, ProfilePick.Position);

    WorldPick Mirrored = {};
    Claim(ResolveWorldPickForSketchPick(Stage.Sketch, Stage.Records, Stage.World, Stage.Mapping, ProfilePick, Mirrored),
          "a selected profile record maps into a world-loop selection");
    Claim(Mirrored.Subject == WorldPickSubject::Loop && Mirrored.Loop.IssuedIndex == 1u,
          "and it chooses the mirrored rectangle loop");

    SketchPick Back = {};
    Claim(ResolveSketchPickForWorldPick(Stage.Sketch, Stage.Records, Stage.Mapping, Mirrored, Back),
          "a mirrored world-loop selection maps back into sketch selection space");
    Claim(Back.Subject == SketchPickSubject::Record && Back.Record.IssuedIndex == Stage.ProfileRecord.IssuedIndex,
          "and it comes back as the same profile record");
}

void ProveWorldBackedViewportFlow()
{
    std::printf("\n2. The world-backed viewport flow selects, drags, and seals a revision\n");

    Bench Stage;
    Stage.Gizmo.Shown = false;
    const SpatialPoint Pivot = Stage.EdgePivot();
    float StartX = 0.0f;
    float StartY = 0.0f;
    float EndX = 0.0f;
    float EndY = 0.0f;
    Claim(ProjectFromCamera(Stage.Camera, Stage.Extent, Pivot, StartX, StartY),
          "the selected edge pivot projects for a live viewport drag");
    Claim(ProjectFromCamera(Stage.Camera, Stage.Extent, { 130.0, 70.0, 40.0 }, EndX, EndY),
          "and the drag target projects too");

    PointerCondition Click = {};
    Click.PositionX = StartX;
    Click.PositionY = StartY;
    Click.ContactPressed = true;
    bool PointerTaken = false;
    Stage.Drive(Click, {}, 1000.0, PointerTaken);

    Claim(PointerTaken,
          "clicking the rectangle edge is consumed by the world-backed selection flow");
    Claim(Stage.SemanticSelection.Subject == SketchPickSubject::Curve,
          "and the standing sketch-space semantic selection becomes that edge");
    Claim(Stage.PendingSelection.IssuedIndex == Stage.ProfileRecord.IssuedIndex,
          "with the profile record kept in sync for the outliner selection");

    PointerCondition Arm = {};
    Arm.PositionX = StartX;
    Arm.PositionY = StartY;
    Arm.ContactHeld = true;
    Arm.TravelX = 1.0f;
    PointerTaken = false;
    Stage.Drive(Arm, {}, 1016.0, PointerTaken);
    Claim(Stage.Transform.Engaged() && Stage.Transform.AwaitingRelease,
          "a held drag on the selected edge starts a pointer-driven world transform session");

    PointerCondition Drag = Arm;
    Drag.PositionX = EndX;
    Drag.PositionY = EndY;
    Drag.TravelX = EndX - StartX;
    Drag.TravelY = EndY - StartY;
    PointerTaken = false;
    Stage.Drive(Drag, {}, 1032.0, PointerTaken);

    const DeclaredWorldCurve* WorldCurve = Stage.World.Resolve(WorldCurveName{ 2u });
    Claim(WorldCurve != nullptr
       && SamePoint(WorldCurve->Geometry.HeldLine().Origin, { 130.0, 20.0, 40.0 })
       && SamePoint(WorldCurve->Geometry.HeldLine().Terminus, { 130.0, 120.0, 40.0 })
       && SamePoint(Stage.Sketch.Curves()[1u].Geometry.HeldLine().Origin, { 130.0, 20.0, 40.0 })
       && SamePoint(Stage.Sketch.Curves()[1u].Geometry.HeldLine().Terminus, { 130.0, 120.0, 40.0 }),
          "the persistent world sketch moves first and the sketch mirror follows it");

    PointerCondition Release = Drag;
    Release.ContactHeld = false;
    Release.ContactReleased = true;
    Release.TravelX = Release.TravelY = 0.0f;
    PointerTaken = false;
    Stage.Drive(Release, {}, 1048.0, PointerTaken);

    Claim(!Stage.Transform.Engaged(),
          "releasing the pointer commits and clears the world-backed session");
    Claim(Stage.Revisions.DeclaredCount() == 1u,
          "and a committed drag seals one sketch revision for undo/redo");
}

void ProveWorldBackedRenderingAndPreview()
{
    std::printf("\n3. The persistent world sketch renders directly and preview appends in screen space\n");

    Bench Stage;
    DeclaredWorldCurve* Raised = Stage.World.Resolve(WorldCurveName{ 2u });
    Claim(Raised != nullptr,
          "the persistent world sketch exposes the rectangle edge for direct rendering edits");
    if (Raised != nullptr)
    {
        Raised->Geometry.HeldLine().Origin.Forward = 100.0;
        Raised->Geometry.HeldLine().Terminus.Forward = 100.0;
    }

    WorkspaceCadPacket Packet = {};
    Discard(ProjectWorldBackedSketchRendering(Stage.World, Stage.Camera,
                                              Stage.Extent, Stage.Drawable, Packet));
    Claim(Packet.SegmentCount > 0u,
          "the mirrored sketch still produces visible linework through the world renderer");
    Claim(Packet.FillCount == 0u,
          "and a no-longer-coplanar closed shape loses only its fill rather than the whole profile");

    std::vector<CurveSpecification> PreviewSpans;
    PreviewSpans.push_back(CurveSpecification::DeclareLine({ 0.0, 0.0, 40.0 }, { 60.0, 0.0, 40.0 }));
    const std::vector<SpatialPoint> Anchors = { { 0.0, 0.0, 40.0 }, { 60.0, 0.0, 40.0 } };
    Claim(ProjectWorldPlacementPreview(Stage.Camera, Stage.Extent, Stage.Drawable,
                                       PreviewSpans, Anchors, { 80.0, 0.0, 40.0 }, Packet),
          "a live placement preview appends through the same world-backed CAD packet path");
    Claim(Packet.MarkerCount >= 3u,
          "and it contributes both anchor markers and the moving hover marker");
}

} // namespace

int main()
{
    std::printf("=========================================================================\n");
    std::printf("WORLD SKETCH SKETCH BRIDGE PROOF\n");
    std::printf("=========================================================================\n");

    ProveMirrorAndPickMapping();
    ProveWorldBackedViewportFlow();
    ProveWorldBackedRenderingAndPreview();

    std::printf("\n=========================================================================\n");
    std::printf("%u claims, %u failures -> %s\n", Claims, Failures,
                Failures == 0u ? "PROVEN" : "REFUTED");
    std::printf("=========================================================================\n");
    return Failures == 0u ? 0 : 1;
}
