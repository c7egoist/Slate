//============================================================================================================================================
//                                                             PAINTHOST.CPP
//============================================================================================================================================
// 🧩 The painting application — lifetime and tick only, with every device concern held by HostLifecycle.

#define SLATE_PAINT_HOST 1
#include "Foundation/DeliveryGuarantee.h"
#include "SlateWorkspace/Discipline/CodexActivation/Api/CodexActivation.h"
#include "SlateWorkspace/Discipline/OrientationCube/Api/OrientationCube.h"
#include "SlateWorld/World/EditorCameraComponent/Api/EditorCameraComponent.h"
#include "SlateWorkspace/Discipline/WorkspaceDeclaration/Api/WorkspaceDeclaration.h"
#include "SlateUI/Interface/ContentBrowserPanel/Api/ContentBrowserPanel.h"
#include "SlateUI/Interface/ControlCentrePanel/Api/ControlCentrePanel.h"
#include "SlateUI/Interface/ThemeInterchange/Api/ThemeInterchange.h"
#include "SlateUI/Interface/EditorPanel/Api/EditorPanel.h"
#include "SlateUI/Interface/WorkspacePanel/Api/WorkspaceIndex.h"
#include "SlateRuntime/Session/SessionSequence/Api/SessionSequence.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <string>

//------------------------------------------------------------------------------------------------------------------------
//                                                          FIGURES
//------------------------------------------------------------------------------------------------------------------------

namespace
{

using namespace Slate;

constexpr std::uint32_t InitialWidth  = 1280u;   // [px]
constexpr std::uint32_t InitialHeight = 720u;    // [px]

constexpr const char* WindowTitle = "Slate \u2014 Paint";
constexpr const char* HostName    = "PaintHost";

// 📝 The workspace ground the interface is recorded over. Stated here because it is the one visual decision
//    this host makes; everything else it presents belongs to a panel.
//------------------------------------------------------------------------------------------------------------------------
//                                                      THE THREE CEILINGS
//------------------------------------------------------------------------------------------------------------------------

// 🔴 Applying the content browser in the south drawer crossed all three of the budgets that have each, at
//    least once, taken a host down with no window and no log line. They are asserted here so a fourth panel
//    cannot repeat any of them silently.

// ① EASED INTERPOLANTS. `ControlIndex::Register` draws two fades per control, and the integrator's supply
//    is shared by every index in the process — the browser's private index does not get its own pool.
constexpr std::uint32_t EasesPerControl = 2u;
constexpr std::uint32_t CentreControls  = ControlCentrePanel::ControlCapacity;
constexpr std::uint32_t BrowserControls = ContentBrowserPanel::RegistrationDemand;
constexpr std::uint32_t EditorControls  = PanelStructure::RecordLimit * EditorPanel::ControlsPerRecord;
constexpr std::uint32_t BareEases       = 9u + 1u;   // [-] - the Control Centre's own motions

constexpr std::uint32_t DemandedEases =
    ((CentreControls + BrowserControls + EditorControls) * EasesPerControl) + BareEases;

static_assert(DemandedEases <= MotionIntegrator::EaseCapacity,
              "this host's panels demand more eased interpolants than the integrator holds — the panel "
              "constructed last is rejected mid-registration and the host exits before its first frame; raise "
              "MotionIntegrator::EaseCapacity or reduce a panel's control count");

// ② INDEX SLOTS. Counted per index, not per host. The browser is the only owner of its own
//    `BrowserInteraction`, so only its demand is weighed here; the Control Centre answers to its private index.
static_assert(BrowserControls <= ControlIndex::ControlCapacity,
              "the content browser registers more controls than one ControlIndex holds — Construct is "
              "rejected with \"no further control slot\" and the south drawer opens onto blank ground");

// ③ AUTOMATIC STORAGE. A Windows thread is given one megabyte and a refusal here is not a refusal at all:
//    the guard page is touched in the prologue, so the process dies before a statement can report anything.
//    Linux hands out eight megabytes, which is exactly why no gate here can catch it.
constexpr std::size_t WindowsThreadStack = 1048576u;                  // [B] - the shipped linker default
constexpr std::size_t AutomaticLimit   = WindowsThreadStack / 4u;   // [B] - a quarter, leaving room to call

static_assert(sizeof(WorkspaceIndex) + sizeof(WorkspacePanel) + sizeof(EditorPanel)
              + (sizeof(PanelStructure)       * WorkspaceIndex::WorkspaceLimit)
              + (sizeof(EditorPanelConfiguration) * WorkspaceIndex::WorkspaceLimit)
              + sizeof(ControlCentrePanel)  + sizeof(ControlCentreConfiguration)
              + sizeof(ControlIndex)    + sizeof(ContentBrowserPanel)
              + sizeof(ContentBrowserConfiguration) + sizeof(ContentLibrary) <= AutomaticLimit,
              "this host's automatic UI members no longer fit a quarter of a Windows thread stack — the "
              "prologue's stack probe will fault before main runs a statement and the host will exit with "
              "no window and no log line; move the largest member to static storage");

// 🔴 The session is deliberately absent from the sum above, and that is load-bearing rather than an
//    oversight: `SessionSequence` holds `ViewportSequence`, which is over four hundred kilobytes on its
//    own — more than the whole budget below. It is declared `static` in main for exactly that reason, so
//    it never enters this frame. This assertion states the fact the arrangement depends on, so a reader
//    who moves the session onto the stack is refused here rather than by a stack probe with no log line.
static_assert(sizeof(SessionSequence) > AutomaticLimit,
              "SessionSequence now fits the automatic budget — re-check whether it still needs static "
              "storage in main, and whether this assertion is still stating anything true");

constexpr float WorkspaceGround[4] = { 0.06f, 0.06f, 0.08f, 1.0f };   // [-]

bool ProjectPaintScenePoint(const PlaneExtent& Extent,
                            double WorldX,
                            double WorldY,
                            double WorldZ,
                            const EditorCameraComponent& Camera,
                            float& ScreenX,
                            float& ScreenY)
{
    const double Yaw = Camera.LaggedYawDegrees * 3.14159265358979323846 / 180.0;
    const double Pitch = Camera.LaggedPitchDegrees * 3.14159265358979323846 / 180.0;
    const double CosP = std::cos(Pitch), SinP = std::sin(Pitch);
    const double SinY = std::sin(Yaw),   CosY = std::cos(Yaw);
    const double ForwardX = CosP * SinY;
    const double ForwardY = SinP;
    const double ForwardZ = CosP * CosY;
    const double RightX = CosY;
    const double RightZ = -SinY;
    const double UpX = -SinP * SinY;
    const double UpY = CosP;
    const double UpZ = -SinP * CosY;
    const double DX = WorldX - Camera.LaggedPosition[0];
    const double DY = WorldY - Camera.LaggedPosition[1];
    const double DZ = WorldZ - Camera.LaggedPosition[2];
    const double CameraX = DX * RightX + DZ * RightZ;
    const double CameraY = DX * UpX + DY * UpY + DZ * UpZ;
    const double CameraZ = DX * ForwardX + DY * ForwardY + DZ * ForwardZ;
    if (CameraZ <= 0.01)
        return false;
    const double TanV = std::tan(Camera.FieldOfViewDegrees * 0.5 * 3.14159265358979323846 / 180.0);
    const double Aspect = Extent.Height() > 0.0f ? static_cast<double>(Extent.Width()) / static_cast<double>(Extent.Height()) : 1.0;
    ScreenX = static_cast<float>((CameraX / (CameraZ * TanV * Aspect) * 0.5 + 0.5) * Extent.Width() + Extent.MinimumX);
    ScreenY = static_cast<float>((-CameraY / (CameraZ * TanV) * 0.5 + 0.5) * Extent.Height() + Extent.MinimumY);
    return true;
}

void RecordPaintSceneProxy(RecordingSurface& Surface, const PlaneExtent& Extent,
                           const WorkspaceCodex& Scene, bool SceneStanding,
                           const EditorCameraComponent& Camera)
{
    if (!SceneStanding)
        return;
    const ThemeToken Fill = Partial(0xF4F1E8u, 0.38f);
    const ThemeToken Edge = Partial(0xFFFFFFu, 0.72f);
    for (const CodexSceneEntry& Entry : Scene.Scene)
    {
        if (Entry.Subject != CodexSceneSubject::Geometry)
            continue;
        const CodexSceneMesh* Mesh = nullptr;
        for (const CodexSceneMesh& Candidate : Scene.SceneMeshes)
            if (Candidate.Naming == Entry.GeometryReference)
            {
                Mesh = &Candidate;
                break;
            }
        if (Mesh == nullptr)
            continue;
        for (std::uint32_t Index = 0u; Index + 2u < Mesh->Indices.size(); Index += 3u)
        {
            float SX[3] = {};
            float SY[3] = {};
            bool Standing = true;
            for (std::uint32_t Corner = 0u; Corner < 3u; ++Corner)
            {
                const std::uint32_t Vertex = Mesh->Indices[Index + Corner];
                if (Vertex * 3u + 2u >= Mesh->Positions.size())
                {
                    Standing = false;
                    break;
                }
                Standing = ProjectPaintScenePoint(Extent,
                    Entry.Position[0] + Mesh->Positions[Vertex * 3u + 0u] * Entry.Scale[0],
                    Entry.Position[1] + Mesh->Positions[Vertex * 3u + 1u] * Entry.Scale[1],
                    Entry.Position[2] + Mesh->Positions[Vertex * 3u + 2u] * Entry.Scale[2],
                    Camera, SX[Corner], SY[Corner]) && Standing;
            }
            if (Standing)
            {
                const float Corners[6] = { SX[0], SY[0], SX[1], SY[1], SX[2], SY[2] };
                Surface.Tongue(Corners, 3u, Fill);
                const float X0[2] = { SX[0], SX[1] }; const float Y0[2] = { SY[0], SY[1] };
                const float X1[2] = { SX[1], SX[2] }; const float Y1[2] = { SY[1], SY[2] };
                const float X2[2] = { SX[2], SX[0] }; const float Y2[2] = { SY[2], SY[0] };
                Surface.Polyline(X0, Y0, 2u, Edge, 0.7f);
                Surface.Polyline(X1, Y1, 2u, Edge, 0.7f);
                Surface.Polyline(X2, Y2, 2u, Edge, 0.7f);
            }
        }
    }
}

void RecordSharedViewportChrome(RecordingSurface& Surface, const PlaneExtent& Extent,
                                const WorkspaceCodex& Scene, bool SceneStanding,
                                const EditorCameraComponent& Camera,
                                const EditorPanelConfiguration& Configuration)
{
    Surface.Confine(Extent);
    Surface.Ground(Extent, Covering(0x0F1014u), 0.0f, CornerNone);
    RecordPaintSceneProxy(Surface, Extent, Scene, SceneStanding, Camera);
    const float Step = 48.0f;
    const float CentreX = (Extent.MinimumX + Extent.MaximumX) * 0.5f;
    const float CentreY = (Extent.MinimumY + Extent.MaximumY) * 0.5f;
    for (float X = CentreX; X < Extent.MaximumX; X += Step)
    {
        const float PointsX[2] = { X, X };
        const float PointsY[2] = { Extent.MinimumY, Extent.MaximumY };
        Surface.Polyline(PointsX, PointsY, 2u, Partial(0xC4C8D6u, 0.10f), 1.0f);
    }
    for (float X = CentreX - Step; X > Extent.MinimumX; X -= Step)
    {
        const float PointsX[2] = { X, X };
        const float PointsY[2] = { Extent.MinimumY, Extent.MaximumY };
        Surface.Polyline(PointsX, PointsY, 2u, Partial(0xC4C8D6u, 0.10f), 1.0f);
    }
    for (float Y = CentreY; Y < Extent.MaximumY; Y += Step)
    {
        const float PointsX[2] = { Extent.MinimumX, Extent.MaximumX };
        const float PointsY[2] = { Y, Y };
        Surface.Polyline(PointsX, PointsY, 2u, Partial(0xC4C8D6u, 0.10f), 1.0f);
    }
    for (float Y = CentreY - Step; Y > Extent.MinimumY; Y -= Step)
    {
        const float PointsX[2] = { Extent.MinimumX, Extent.MaximumX };
        const float PointsY[2] = { Y, Y };
        Surface.Polyline(PointsX, PointsY, 2u, Partial(0xC4C8D6u, 0.10f), 1.0f);
    }

    const CubeBasis GizmoBasis = CubeBasisFromYawPitch(Camera.LaggedYawDegrees,
                                                                            Camera.LaggedPitchDegrees);
    RecordOrientationWidget(Surface, Extent, GizmoBasis, Configuration.Gizmo == PanelGizmo::Cad);
    Surface.Release();
}

}   // namespace

//------------------------------------------------------------------------------------------------------------------------
//                                                            MAIN
//------------------------------------------------------------------------------------------------------------------------

int main(int ArgumentCount, char** ArgumentValues)
{
    using namespace Slate;

    // ① The session — window, device, chain, recordings, interface, appearance and font atlas, in the one
    //    order every windowed product shares. 🔴 `SlateRuntime` owns that order; this host owns what it
    //    puts inside the tick and nothing else.
    SessionDeclaration Declared;
    Declared.Naming        = HostName;
    Declared.WindowCaption = WindowTitle;
    Declared.InvokedAs     = (ArgumentCount > 0) ? ArgumentValues[0] : "";
    Declared.InitialWidth  = InitialWidth;
    Declared.InitialHeight = InitialHeight;
    Declared.Pacing        = LatencyIntent::SteadyPacing;

    Declared.North.Caption       = "ControlCentre";
    Declared.North.TongueSubject = SymbolSubject::PulseTrace;
    Declared.North.PoseCount     = 2u;

    Declared.South.Caption       = "ContentBrowser";
    Declared.South.TongueSubject = SymbolSubject::FolderClosed;
    Declared.South.PoseCount     = 3u;

    for (std::uint32_t Channel = 0u; Channel < 4u; ++Channel)
        Declared.WorkspaceGround[Channel] = WorkspaceGround[Channel];

    // 📝 Static because `ViewportSequence` alone is 406 KB and a Windows thread is handed one megabyte.
    //    The three ceilings above weigh this host's automatic members; the session is kept out of them.
    static SessionSequence Session;

    const Deliver<bool> Opened = Session.ConstructSession(Declared);

    if (!Opened.Resolved)
    {
        std::printf("%s \u2014 %s\n", HostName, Opened.Error.Detail);
        return 1;
    }

    ViewportSequence& Viewport = Session.Interface();

    // ─────────────────────────────────────────────────────────────────────────────────────────────────────
    //                                                       THE TICK LOOP
    // ─────────────────────────────────────────────────────────────────────────────────────────────────────

    // 📝 🔴 Every refusal is resolved inside Await, before a display image is acquired. A tick that reports
    //    Recording has a recording open and cannot be abandoned; a tick that reports Idle has opened
    //    nothing. That is what makes the `continue` below safe — the arrangement this host previously got
    //    wrong five times over, returning to the top of the loop with a command buffer still recording.

    // ③ The workspaces this host opens. 🔴 The INDEX owns them and the panel presents them — `14` §1
    //    forbids a panel from holding what it displays, and separating the two is the whole reason there
    //    are two components here rather than one.
    // 📝 The subject this host opens by default, named once so the startup registration and the strip's `+`
    //    cannot disagree about what a new workspace is.
    // 🔴 This host seats a BARE VIEWPORT, not the texturing discipline's declared arrangement, and so it
    //    applies `DeclaredVacantWorkspace` rather than the declaration its own subject selects. That is
    //    not an oversight: the texturing arrangement seats a layer-stack panel, and the only panel subject
    //    this host renders is `Viewport` (see the two `LeafSubject` comparisons below). Seating a panel it
    //    cannot draw would leave a blank hole where the layer stack belongs.
    // 📝 `EditorHost` renders every panel subject, so it takes the declaration its subject selects. This
    //    host is deleted at step 11 and the discrepancy goes with it.
    constexpr WorkspaceSubject DefaultSubject = WorkspaceSubject::Painting;

    WorkspaceIndex          Workspaces;
    WorkspacePanel          Workspace;
    EditorPanel             WorkspacePanels;
    PanelStructure          PanelPartitions[WorkspaceIndex::WorkspaceLimit];
    EditorPanelConfiguration    PanelConfiguration[WorkspaceIndex::WorkspaceLimit];
    ControlCentrePanel      ControlCentre;
    ControlCentreConfiguration  ControlCentreValues;

    ControlIndex         BrowserInteraction;

    // 📝 The south drawer's owner. The library is the HOST's, not the panel's — `14` §1 forbids a panel
    //    from holding what it displays, which is the same separation WorkspaceIndex and WorkspacePanel keep.
    ContentBrowserPanel      ContentBrowser;
    ContentBrowserConfiguration  ContentBrowserApplied;
    ContentLibrary           ContentApplied;
    WorkspaceCodex           OpenedScene = {};
    bool                     OpenedSceneStanding = false;
    EditorCameraComponent    EditorCamera;
    bool                     EditorCameraLookLatched = false;
    {
        const ViewportCameraSeed CameraSeed = ViewportCameraSeed{};
        EditorCamera.Position[0] = CameraSeed.Position[0];
        EditorCamera.Position[1] = CameraSeed.Position[1];
        EditorCamera.Position[2] = CameraSeed.Position[2];
        EditorCamera.YawDegrees = CameraSeed.YawDegrees;
        EditorCamera.PitchDegrees = CameraSeed.PitchDegrees;
        EditorCamera.FieldOfViewDegrees = CameraSeed.FieldOfViewDegrees;
        EditorCamera.Snap();
    }

    // 📝 The appearance the session adopted from beside the executable, seated into the Control Centre so
    //    the panel opens on what is actually applied. The session owns the file and the atlas; this host
    //    owns only the panel that presents them.
    const ThemeSelection& Adopted = Session.Inscribed();

    ControlCentreValues.Theme       = Adopted.Current;
    ControlCentreValues.Primary     = Adopted.Primary;
    ControlCentreValues.Secondary   = Adopted.Secondary;
    ControlCentreValues.Information = Adopted.Information;
    ControlCentreValues.Warning     = Adopted.Warning;
    ControlCentreValues.Alert       = Adopted.Alert;

    const std::filesystem::path EngineContentRoot = Session.ContentRoot();

    // 📝 Which dock node the next registered workspace is applied into; zero means the main dock space.
    std::uint32_t  RegisterIntoNode = 0u;

    FontLoader& Fonts = Session.Fonts();

    ControlCentre.SetFontFamilies(Fonts);
    // 📝 Seat the family carousel on the family the appearance names. Without this the carousel opened
    //    on ordinal zero (the alphabetically first family) while the loaded faces were the appearance's
    //    own — and the role strips draw the LOADED family's faces, so the two have to agree at bring-up.
    for (std::uint32_t Index = 0u; Index < Fonts.FamilyCount(); ++Index)
        if (Fonts.FamilyName(Index) != nullptr &&
            std::strcmp(Fonts.FamilyName(Index), Viewport.Appearance().Fonts.Family) == 0)
        {
            ControlCentreValues.Font = Index;
            break;
        }


    if (!Workspace.ConstructWorkspacePanel(Viewport.Surface(), Viewport.Appearance()).Resolved)
    {
        std::printf("%s \u2014 the workspace panel was rejected\n", HostName);
        return 1;
    }

    if (!WorkspacePanels.ConstructEditorPanel(Viewport.MotionSource(), Viewport.Surface(), Viewport.Appearance()).Resolved)
    {
        std::printf("%s \u2014 the editor panels were rejected\n", HostName);
        return 1;
    }

    if (!ControlCentre.ConstructControlCentrePanel(Viewport.MotionSource(), Viewport.Surface(), Viewport.Appearance()).Resolved)
    {
        std::printf("%s \u2014 the Control Centre panel was rejected\n", HostName);
        return 1;
    }

    if (!BrowserInteraction.AttachMotion(Viewport.MotionSource()).Resolved)
    {
        std::printf("%s \u2014 the content browser index was rejected\n", HostName);
        return 1;
    }

    // 🔴 The browser carries its OWN index, as every panel here does, so its registration cannot exhaust the
    //    Control Centre's. Read — an registration refusal is silent at the call site and a browser that was
    //    rejected records nothing at all, which reads as a drawer that opens onto blank ground.
    if (!ContentBrowser.ConstructContentBrowserPanel(BrowserInteraction, Viewport.Surface(), Viewport.Appearance()).Resolved)
    {
        std::printf("%s \u2014 the content browser was rejected\n", HostName);
        return 1;
    }

    // 🔴 The browser takes no appearance at Construct — it is applied here, once the viewport has resolved one.
    ContentBrowser.Reapply(Viewport.Appearance());

    ApplyReferenceContent(ContentApplied);

    // 📝 One workspace open by default, of the subject this host is for. A host that opened none would show
    //    the vacant run on first launch, which reads as a failure rather than as a fresh start.
    const Deliver<std::uint32_t> DefaultWorkspace = Workspaces.Register(DefaultSubject);
    if (!DefaultWorkspace.Resolved)
    {
        std::printf("%s \u2014 the default workspace could not be opened\n", HostName);
        return 1;
    }
    Discard(ApplyWorkspace(DeclaredVacantWorkspace(), PanelPartitions[DefaultWorkspace.Resolve()]));

    std::printf("%s \u2014 opened %s\n", HostName, Workspaces.ActiveTitle());

    while (Session.Active())
    {
        // 🔴 One call answers the window, the resize, the device loss and the interface tick. Every branch
        //    this host used to carry — retiring, rebuilding, renegotiating — is `SlateRuntime`'s, written
        //    once for every product rather than three times with three sets of defects.
        const SessionPass Pass = Session.Await();

        if (Pass.Current == SessionCondition::Closed)
            break;

        // 📝 This host owns no device resources of its own, so a retiring tick asks nothing of it. A
        //    product that holds passes or images releases them here, while the device still stands.
        if (Pass.Current != SessionCondition::Recording)
            continue;

        {
            // 🔴 The workspace is recorded FIRST and the drawers over it. One background draw list, so
            //    the order of recording IS the z-order — and the previous arrangement recorded the
            //    workspace after `RecordDrawers`, which painted the whole surface over the control
            //    centre and the asset browser.
            const PlaneExtent Whole = Spanning(0.0f, 0.0f,
                                               static_cast<float>(Pass.Width),
                                               static_cast<float>(Pass.Height));

            const PointerCondition& ForegroundPointer = Viewport.Surface().Pointer();
            const PlaneExtent NorthInterior = Viewport.Drawers().Interior(DrawerBearing::North);
            const PlaneExtent SouthInterior = Viewport.Drawers().Interior(DrawerBearing::South);
            const bool PointerBehindDrawer =
                NorthInterior.Encloses(ForegroundPointer.PositionX, ForegroundPointer.PositionY) ||
                SouthInterior.Encloses(ForegroundPointer.PositionX, ForegroundPointer.PositionY);
            PointerCondition BackgroundPointer = ForegroundPointer;
            if (PointerBehindDrawer)
            {
                BackgroundPointer.PositionX = BackgroundPointer.PositionY = -1000000.0f;
                BackgroundPointer.TravelX = BackgroundPointer.TravelY = BackgroundPointer.WheelY = 0.0f;
                BackgroundPointer.ContactHeld = BackgroundPointer.ContactPressed = false;
                BackgroundPointer.ContactDoublePressed = BackgroundPointer.ContactReleased = false;
                BackgroundPointer.SecondaryHeld = BackgroundPointer.SecondaryPressed = false;
                BackgroundPointer.SecondaryReleased = false;
            }

            Discard(Workspace.Record(Whole, Workspaces.ActiveTitle()));

            // 🔴 The dock space FIRST, over the whole panel. Every workspace below docks into it, and the
            //    vendor draws their tabs with PatchA's trapezoid — which is what makes a tab draggable out
            //    into a floating window. A hand-recorded tab bar cannot be undocked: the vendor's docking
            //    operates on WINDOWS, so a workspace has to be one.
            Viewport.Seam().RecordDockSpace(Whole);

            const std::uint32_t OpenCount = Workspaces.OpenCount();

            // 🔴 Titles are read through `Titled`, which points into the index's own storage. The delivered
            //    form copies the entry, so a pointer taken from it dangles at the semicolon — every label
            //    then decayed to the same garbage and ImGui reported four conflicting IDs.
            std::uint32_t Withdrawing = OpenCount;

            // 📝 The node the previous tick's `+` named, so the workspace it registered is applied into the
            //    strip the artist actually pressed rather than always into the main dock space.
            const std::uint32_t ApplyInto = RegisterIntoNode;

            RegisterIntoNode = 0u;

            bool PointerOverViewport = false;
            for (std::uint32_t Leaf = 0u; Leaf < WorkspacePanels.LeafCount(); ++Leaf)
            {
                if (WorkspacePanels.LeafSubject(Leaf) == PanelSubject::Viewport &&
                    WorkspacePanels.LeafBody(Leaf).Encloses(ForegroundPointer.PositionX, ForegroundPointer.PositionY))
                {
                    PointerOverViewport = true;
                    break;
                }
            }
            if (ForegroundPointer.SecondaryPressed)
                EditorCameraLookLatched = PointerOverViewport && !PointerBehindDrawer;
            if (ForegroundPointer.SecondaryReleased || !ForegroundPointer.SecondaryHeld)
                EditorCameraLookLatched = false;
            CameraCondition FlyInput = Viewport.Seam().CameraInput(
                (PointerOverViewport || EditorCameraLookLatched) && !PointerBehindDrawer);
            CameraSettings FlySettings;
            FlySettings.FlySpeed = EditorCamera.FlySpeed;
            EditorCamera.Advance(Pass.ElapsedMilliseconds / 1000.0, FlyInput, FlySettings);

            WorkspacePanels.Advance(BackgroundPointer, Pass.ElapsedMilliseconds);

            for (std::uint32_t Index = 0u; Index < OpenCount; ++Index)
            {
                const char* Titled = Workspaces.Titled(Index);

                if (Titled == nullptr)
                    continue;

                bool Current = true;

                const PlaneExtent PanelExtent = Viewport.Seam().EnterWorkspaceWindow(
                    Titled, !Workspaces.Applied(Index), ApplyInto, Current);
                Workspaces.Apply(Index);

                if (PanelExtent.Width() > 0.0f && PanelExtent.Height() > 0.0f)
                {
                    Discard(Viewport.Surface().SwitchToWindow());
                    Discard(WorkspacePanels.Record(PanelExtent,
                                                      PanelPartitions[Index],
                                                      PanelConfiguration[Index],
                                                      Index));
                    for (std::uint32_t Leaf = 0u; Leaf < WorkspacePanels.LeafCount(); ++Leaf)
                    {
                        if (WorkspacePanels.LeafSubject(Leaf) == PanelSubject::Viewport)
                        {
                            const PlaneExtent LeafBody = WorkspacePanels.LeafBody(Leaf);
                            if (BackgroundPointer.ContactPressed &&
                                LeafBody.Encloses(BackgroundPointer.PositionX, BackgroundPointer.PositionY))
                            {
                                const CubeBasis GizmoBasis = CubeBasisFromYawPitch(
                                    EditorCamera.LaggedYawDegrees, EditorCamera.LaggedPitchDegrees);
                                const Deliver<ViewportOrientation> Hit = HitOrientationWidget(
                                    LeafBody, GizmoBasis, BackgroundPointer.PositionX, BackgroundPointer.PositionY,
                                    PanelConfiguration[Index].Gizmo == PanelGizmo::Cad);
                                if (Hit.Resolved)
                                {
                                    double Yaw = EditorCamera.YawDegrees;
                                    double Pitch = EditorCamera.PitchDegrees;
                                    OrientationYawPitch(Hit.Resolve(), Yaw, Pitch);
                                    EditorCamera.YawDegrees = Yaw;
                                    EditorCamera.PitchDegrees = Pitch;
                                    EditorCamera.Snap();
                                }
                            }
                            RecordSharedViewportChrome(Viewport.Surface(), LeafBody,
                                                       OpenedScene, OpenedSceneStanding, EditorCamera,
                                                       PanelConfiguration[Index]);
                        }
                    }
                    if (WorkspacePanels.PointerCaptured(Index))
                        Viewport.Seam().WithholdPointer();
                }

                Viewport.Seam().LeaveWorkspaceWindow();
                Discard(Viewport.Surface().SwitchLayer(RecordingSurface::ShellLayer::Beneath));

                // ⚠️ Recorded, never acted on inside the sweep. Withdrawing here edits the set being walked.
                if (!Current)
                    Withdrawing = Index;
            }

            if (Withdrawing < OpenCount)
            {
                Discard(Workspaces.Withdraw(Withdrawing));
                WorkspacePanels.WithdrawPresentation(Withdrawing);
                for (std::uint32_t Moving = Withdrawing; Moving + 1u < OpenCount; ++Moving)
                {
                    PanelPartitions[Moving] = PanelPartitions[Moving + 1u];
                    PanelConfiguration[Moving]   = PanelConfiguration[Moving + 1u];
                }
                PanelPartitions[OpenCount - 1u].Reset();
                PanelConfiguration[OpenCount - 1u] = EditorPanelConfiguration{};
            }

            // 📝 The `+`, applied inside the dock node's own tab bar so the vendor lays it after the last
            //    tab — always at the end, by construction rather than by arithmetic.
            std::uint32_t AskingNode = 0u;

            if (Viewport.Seam().RecordWorkspaceAddition(Workspace.Strip(), OpenCount, AskingNode))
            {
                // 🔴 The asking node is carried to the NEXT tick, because the workspace it registers is not
                //    recorded until then. Applying it against the main space instead is what put a new
                //    workspace in the wrong window.
                RegisterIntoNode = AskingNode;
                const Deliver<std::uint32_t> RegisteredWorkspace = Workspaces.Register(DefaultSubject);
                if (RegisteredWorkspace.Resolved)
                    Discard(ApplyWorkspace(DeclaredVacantWorkspace(), PanelPartitions[RegisteredWorkspace.Resolve()]));
            }

            // 🔴 With nothing open there is no tab bar to seat a `+` in, so the empty shell carries the
            //    invitation itself. `WorkspacePanel` draws "CREATE PANEL" on plain black; a press anywhere
            //    on that ground registers one, which is the way out of a state that otherwise has none.
            if (OpenCount == 0u && Viewport.Seam().VacantPressed(Whole))
            {
                const Deliver<std::uint32_t> RegisteredWorkspace = Workspaces.Register(DefaultSubject);
                if (RegisteredWorkspace.Resolved)
                    Discard(ApplyWorkspace(DeclaredVacantWorkspace(), PanelPartitions[RegisteredWorkspace.Resolve()]));
            }

            // 📝 The drawers last, so they sit ABOVE the workspace as the sheet lays them.
            Viewport.RecordDrawers();
            Viewport.DrawerPanels();

            // ⑤ The south drawer's browser, recorded before the north drawer's Control Centre so the
            //     Control Centre's own exclusions are the last thing declared and the two cannot disagree.
            //     🔴 The interior is asked for every tick and not cached — the drawer is springing, so the
            //     extent it offers is a different one on almost every tick of an open or a close.
            const PlaneExtent BrowserInterior = Viewport.Drawers().Interior(DrawerBearing::South);

            BrowserInteraction.Advance(Viewport.Surface().Pointer(), Pass.ElapsedMilliseconds);
            ContentBrowser.Advance(Viewport.Surface().Pointer(), Pass.ElapsedMilliseconds);

            if (BrowserInterior.Width() > 0.0f && BrowserInterior.Height() > 0.0f)
            {
                Discard(Viewport.Surface().SwitchLayer(RecordingSurface::ShellLayer::Above));
                Viewport.Surface().Ground(BrowserInterior, Viewport.Appearance().Colour.SurfaceCurrent,
                                          0.0f, CornerNone);
                ContentBrowser.RecordBrowser(BrowserInterior, ContentApplied, ContentBrowserApplied);
                ContentBrowser.RecordDeferred();
                const CodexActivation ActivatedScene = ConsumeCodexActivation(
                    ContentBrowserApplied, ContentApplied, EngineContentRoot);
                if (ActivatedScene.Resolved)
                {
                    OpenedScene = ActivatedScene.Scene.Workspace;
                    OpenedSceneStanding = true;
                }
                else if (ActivatedScene.Requested)
                {
                    std::printf("%s — workspace activation refused (reason %u: %s)\n", HostName,
                                static_cast<unsigned>(ActivatedScene.Error.DeclaredReason), ActivatedScene.Error.Detail);
                }
                // 🔴 Declared every tick or lost. Without it the drawer owns every contact inside its own
                //    body, so taking a record or dragging the lattice slides the drawer instead.
                ContentBrowser.Exclude(Viewport.Drawers(), DrawerBearing::South);
                Discard(Viewport.Surface().SwitchLayer(RecordingSurface::ShellLayer::Beneath));
            }

            const PlaneExtent ControlInterior = Viewport.Drawers().Interior(DrawerBearing::North);
            ControlCentre.Advance(Viewport.Surface().Pointer(), Pass.ElapsedMilliseconds);
            // 📝 The artist's per-role weights are declared every tick so the workspace's panels read the
            //    current choice; the viewport re-states them after each resolve.
            Viewport.ApplyTypographyRoles(ControlCentreValues.TypographySize,
                                          ControlCentreValues.TypographyWeight);
            Discard(Viewport.Surface().SwitchLayer(RecordingSurface::ShellLayer::Above));
            if (ControlInterior.Width() > 0.0f && ControlInterior.Height() > 0.0f)
                Viewport.Surface().Ground(ControlInterior, Viewport.Appearance().Colour.SurfaceCurrent,
                                          0.0f, CornerNone);
            Discard(ControlCentre.Record(ControlInterior, ControlCentreValues));

            Discard(Viewport.Seam().ApplyInterfaceAntialiasing(
                ControlCentreValues.GeometryAntialiasing));

            // 📝 What the Control Centre holds this tick, handed to the session. The comparison against
            //    what was last inscribed, the write beside the executable and the font pipeline are all
            //    the session's — this host reapplies only the panel that keeps its own copy of the inks.
            {
                ThemeSelection Chosen;
                Chosen.Current     = ControlCentreValues.Theme;
                Chosen.Primary     = ControlCentreValues.Primary;
                Chosen.Secondary   = ControlCentreValues.Secondary;
                Chosen.Information = ControlCentreValues.Information;
                Chosen.Warning     = ControlCentreValues.Warning;
                Chosen.Alert       = ControlCentreValues.Alert;

                if (ControlCentreValues.Font < Fonts.FamilyCount() && Fonts.FamilyName(ControlCentreValues.Font) != nullptr)
                    std::snprintf(Chosen.FontFamily, sizeof(Chosen.FontFamily), "%s", Fonts.FamilyName(ControlCentreValues.Font));

                if (Session.RestateAppearance(Chosen, ControlCentreValues.Scaling))
                    ContentBrowser.Reapply(Viewport.Appearance());
            }

            ControlCentre.Exclude(Viewport.Drawers());
            Discard(Viewport.Surface().SwitchLayer(RecordingSurface::ShellLayer::Beneath));
        }

        // 🔴 The interface is sealed and recorded by the session, which owns the display scope. A host that
        //    records device work of its own does it after this returns true.
        static_cast<void>(Session.Seal(Pass));

        if (!Session.Complete())
            break;
    }

    // ─────────────────────────────────────────────────────────────────────────────────────────────────────
    //                                                      RECLAMATION
    // ─────────────────────────────────────────────────────────────────────────────────────────────────────

    // 📝 This host's own panels first, then the session — the interface and every device lifetime beneath
    //    it are retired inside `Reclaim`, in the exact reverse of the order they were constructed.
    ControlCentre.Reset();
    WorkspacePanels.Reset();
    for (std::uint32_t Index = 0u; Index < WorkspaceIndex::WorkspaceLimit; ++Index)
        PanelPartitions[Index].Reset();
    Workspace.Reset();
    Workspaces.Reset();

    // 🔴 Returned rather than only stated. A validation run needs an exit code, so that a serious arrival
    //    fails whatever invoked the host instead of scrolling past in a console nobody reads.
    return (Session.Reclaim() == 0u) ? 0 : 1;
}
