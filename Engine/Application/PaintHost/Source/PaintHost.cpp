//============================================================================================================================================
//                                                             PAINTHOST.CPP
//============================================================================================================================================
// 🧩 The painting application — lifetime and tick only, with every device concern held by HostLifecycle.

#define SLATE_PAINT_HOST 1
#include "Foundation/DeliveryOutcome.h"
#include "Application/Api/SharedViewportHostBridge.h"
#include "SlateWorld/World/EditorCameraComponent/Api/EditorCameraComponent.h"
#include "SlateUI/Interface/ContentBrowserPanel/Api/ContentBrowserPanel.h"
#include "SlateUI/Interface/ControlCentrePanel/Api/ControlCentrePanel.h"
#include "SlateUI/Interface/ThemeInterchange/Api/ThemeInterchange.h"
#include "SlateUI/Interface/EditorPanel/Api/EditorPanel.h"
#include "SlateUI/Interface/ViewportSequence/Api/ViewportSequence.h"
#include "SlateUI/Interface/WorkspacePanel/Api/WorkspaceIndex.h"
#include "SlateVulkan/Device/HostLifecycle/Api/HostLifecycle.h"

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

    const SharedViewportBasis GizmoBasis = SharedViewportBasisFromYawPitch(Camera.LaggedYawDegrees,
                                                                            Camera.LaggedPitchDegrees);
    RecordSharedViewportGizmo(Surface, Extent, GizmoBasis, Configuration.Gizmo == PanelGizmo::Cad);
    Surface.Release();
}

/// 🧩 Copies the device handles across the layer seam into the attachment the interface declares.
/// note  🔴 `SlateVulkan` cannot name `InterfaceAttachment` — it lives one layer above — so `HostLifecycle`
///        offers the same handles as `DeviceOffering` and the host performs the copy. The copy IS the seam:
///        it happens in the one translation unit that is allowed to see both sides.


InterfaceAttachment Attach(const DeviceOffering& Offered)
{
    InterfaceAttachment Incoming = {};

    Incoming.Instance                 = Offered.Instance;
    Incoming.ScoredDevice             = Offered.ScoredDevice;
    Incoming.ActiveDevice             = Offered.ActiveDevice;
    Incoming.GraphicsQueue            = Offered.GraphicsQueue;
    Incoming.GraphicsFamilyIndex    = Offered.GraphicsFamilyIndex;
    Incoming.ColourTargetFormat       = Offered.ColourTargetFormat;
    Incoming.MinimumDisplayImageCount = Offered.MinimumDisplayImageCount;
    Incoming.DisplayImageCount        = Offered.DisplayImageCount;
    Incoming.NativeWindowSlot         = Offered.NativeWindowSlot;

    return Incoming;
}

}   // namespace

//------------------------------------------------------------------------------------------------------------------------
//                                                            MAIN
//------------------------------------------------------------------------------------------------------------------------

int main(int ArgumentCount, char** ArgumentValues)
{
    using namespace Slate;

    // ① The five lifetimes — window, instance, surface, diagnostic, device, chain, slots, recordings.
    HostDeclaration Declared;
    Declared.Naming        = HostName;
    Declared.WindowCaption = WindowTitle;
    Declared.InitialWidth  = InitialWidth;
    Declared.InitialHeight = InitialHeight;
    Declared.Pacing        = LatencyIntent::SteadyPacing;

#ifdef SLATE_DEBUG
    Declared.DiagnosticRequested = true;
#endif

    HostLifecycle Lifetime;

    if (!Lifetime.ConstructHost(Declared).Resolved)
        return 1;

    // ② The viewport sequence — springs, drawers, and the assembled recording.
    static ViewportSequence Viewport;

    DrawerDeclaration NorthDrawer;
    NorthDrawer.Caption       = "ControlCentre";
    NorthDrawer.TongueSubject = SymbolSubject::PulseTrace;
    NorthDrawer.PoseCount     = 2u;

    DrawerDeclaration SouthDrawer;
    SouthDrawer.Caption       = "ContentBrowser";
    SouthDrawer.TongueSubject = SymbolSubject::FolderClosed;
    SouthDrawer.PoseCount     = 3u;

    if (!Viewport.ConstructViewportSequence(Attach(Lifetime.Offering()), NorthDrawer, SouthDrawer).Resolved)
    {
        std::printf("%s \u2014 the viewport sequence was rejected\n", HostName);
        return 1;
    }

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
    constexpr WorkspaceSubject DefaultSubject = WorkspaceSubject::Painting;

    WorkspaceIndex          Workspaces;
    WorkspacePanel          Workspace;
    EditorPanel             WorkspacePanels;
    PanelStructure          PanelPartitions[WorkspaceIndex::WorkspaceLimit];
    EditorPanelConfiguration    PanelConfiguration[WorkspaceIndex::WorkspaceLimit];
    ControlCentrePanel      ControlCentre;
    ControlCentreConfiguration  ControlCentreValues;
    FontLoader                  Fonts;

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
        const SharedViewportCameraSeed CameraSeed = SharedViewportDefaultCamera();
        EditorCamera.Position[0] = CameraSeed.Position[0];
        EditorCamera.Position[1] = CameraSeed.Position[1];
        EditorCamera.Position[2] = CameraSeed.Position[2];
        EditorCamera.YawDegrees = CameraSeed.YawDegrees;
        EditorCamera.PitchDegrees = CameraSeed.PitchDegrees;
        EditorCamera.FieldOfViewDegrees = CameraSeed.FieldOfViewDegrees;
        EditorCamera.Snap();
    }

    // 📝 The appearance file sits beside the executable and is read once, before any panel is recorded. A
    //    first run has no file yet, which is the ordinary case and not a fault — the build's own appearance
    //    stands and the first colour the artist changes writes the file.
    const char* const InvokedAs = (ArgumentCount > 0) ? ArgumentValues[0] : "";
    const std::filesystem::path ExecutablePath = InvokedAs[0] != '\0'
                                               ? std::filesystem::absolute(InvokedAs)
                                               : std::filesystem::current_path();
    const std::filesystem::path EngineContentRoot = ResolveEngineContentRoot(ExecutablePath);
    const std::string FontRoot = (EngineContentRoot / "FontArchives").string();

    {
        ThemeSelection Recorded;

        if (ThemeInterchange::AdoptBeside(InvokedAs, Recorded))
        {
            ControlCentreValues.Theme       = Recorded.Current;
            ControlCentreValues.Primary     = Recorded.Primary;
            ControlCentreValues.Secondary   = Recorded.Secondary;
            ControlCentreValues.Information = Recorded.Information;
            ControlCentreValues.Warning     = Recorded.Warning;
            ControlCentreValues.Alert       = Recorded.Alert;
        }
    }

    // 🔴 What was last written, so the file is inscribed when a colour actually changes and not every tick.
    //    A write per frame would rewrite the whole appearance sixty times a second for as long as the
    //    Control Centre is open, which is a disk cost no artist asked for.
    ThemeSelection InscribedSelection;
    InscribedSelection.Current   = ControlCentreValues.Theme;
    InscribedSelection.Primary     = ControlCentreValues.Primary;
    InscribedSelection.Secondary   = ControlCentreValues.Secondary;
    InscribedSelection.Information = ControlCentreValues.Information;
    InscribedSelection.Warning     = ControlCentreValues.Warning;
    InscribedSelection.Alert       = ControlCentreValues.Alert;

    // 🔴 Declared BEFORE any panel is constructed. Panels that copy their inks do so out of the appearance the
    //    viewport hands them at Construct, so a selection declared afterwards would leave the first frames
    //    drawn in the transcription's own theme and only correct itself on the artist's first colour change.
    Viewport.Retint(InscribedSelection);

    // 📝 Which dock node the next registered workspace is applied into; zero means the main dock space.
    std::uint32_t  RegisterIntoNode = 0u;

    Viewport.Surface().ApplyFontLoader(Fonts);
    Discard(Fonts.Discover(FontRoot.c_str()));
    // 📝 The family carousel's preview faces are added to the atlas BEFORE the first tick records. Added
    //    during recording instead, the faces would land in an atlas the renderer had already uploaded and
    //    the preview tiles would draw from stale texture data.
    Discard(Fonts.PreparePreviews(1.0f));
    Discard(Fonts.Load(FontRoot.c_str(), Viewport.Appearance().Fonts, 1.0f));
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
    const Outcome<std::uint32_t> DefaultWorkspace = Workspaces.Register(DefaultSubject);
    if (!DefaultWorkspace.Resolved)
    {
        std::printf("%s \u2014 the default workspace could not be opened\n", HostName);
        return 1;
    }
    PanelPartitions[DefaultWorkspace.Resolve()].ConstructPanelPartition(PanelSubject::Viewport);

    // 🔴 The sheet's tab figures applied into the vendor's style, including the four `Patches/` adds. They
    //    default to 0.0f, at which a patched build draws stock rectangular tabs — so this call is what
    //    turns the trapezoid on.
    if (!Viewport.Seam().ApplyWorkspaceStyle(Viewport.Appearance().WorkspaceMeasure,
                                                 Viewport.Appearance().Workspace).Resolved)
    {
        std::printf("%s \u2014 the workspace style was not applied\n", HostName);
    }

    std::printf("%s \u2014 opened %s\n", HostName, Workspaces.ActiveTitle());

    while (Lifetime.Active())
    {
        const TickPass Pass = Lifetime.Await(WorkspaceGround);
        Discard(Fonts.FlushPending());

        if (Pass.Current == TickCondition::Closed)
            break;

        // 🔴 Phase one of a device rebuild. The device STILL STANDS here, so this is the one moment the
        //    interface can release its descriptor pool, font atlas and pipelines against a live handle.
        //    Reclaiming after the rebuild idles a device the vendor has already destroyed, which the
        //    loader reports as VUID-vkDeviceWaitIdle-device-parameter.
        if (Pass.DeviceRetiring)
        {
            Viewport.Reclaim();
            continue;
        }

        // ③·i 🔴 The DEVICE was rebuilt, so every device handle the interface holds names an object the
        //      vendor has returned — its font atlas, its descriptor sets, its pipelines. Renegotiating the
        //      image counts would restate figures against a device that no longer exists, so the interface
        //      is reclaimed and reconstructed against the handles the rebuilt device offers.
        //      Tested before DisplayRecovered because a device rebuild raises both.
        if (Lifetime.DeviceRecovered())
        {
            // 📝 Not reclaimed here: the retiring tick above already did it, while the device lived.
            if (!Viewport.ConstructViewportSequence(Attach(Lifetime.Offering()), NorthDrawer, SouthDrawer).Resolved)
            {
                std::printf("%s \u2014 the interface could not be rebuilt on the recovered device\n", HostName);
                break;
            }

            // 📝 The display recovery this rebuild also raised is consumed here. The reconstruction above
            //    already took the counts the new chain holds, and renegotiating them again would restate
            //    what was just constructed.
            static_cast<void>(Lifetime.DisplayRecovered());
        }

        // ③ The chain was re-established. The interface is told the counts it now holds, exactly once.
        else if (Lifetime.DisplayRecovered())
        {
            const DeviceOffering Offered = Lifetime.Offering();
            // 🔴 Read, not discarded. An interface still holding the previous image counts records
            //    against a chain depth that no longer exists, and the vendor reports that as a
            //    descriptor mismatch several ticks later rather than as the resize that caused it.
            if (!Viewport.Renegotiate(Offered.MinimumDisplayImageCount, Offered.DisplayImageCount))
            {
                std::printf("%s \u2014 the interface rejected the restated image counts\n", HostName);
            }
        }

        if (Pass.Current != TickCondition::Recording)
            continue;

        // ④ Build the interface tick. A refusal here abandons the tick's content, and the recording is
        //    still surrendered — an empty rendering scope presents the cleared ground, which is correct
        //    and is what the artist sees for one tick.
        if (Viewport.Advance(Pass.ElapsedMilliseconds).Resolved)
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
                                const SharedViewportBasis GizmoBasis = SharedViewportBasisFromYawPitch(
                                    EditorCamera.LaggedYawDegrees, EditorCamera.LaggedPitchDegrees);
                                const SharedViewportOrientation Hit = HitSharedViewportGizmo(
                                    LeafBody, GizmoBasis, BackgroundPointer.PositionX, BackgroundPointer.PositionY,
                                    PanelConfiguration[Index].Gizmo == PanelGizmo::Cad);
                                if (Hit != SharedViewportOrientation::None)
                                {
                                    double Yaw = EditorCamera.YawDegrees;
                                    double Pitch = EditorCamera.PitchDegrees;
                                    SharedViewportOrientationPreset(Hit, Yaw, Pitch);
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
                const Outcome<std::uint32_t> RegisteredWorkspace = Workspaces.Register(DefaultSubject);
                if (RegisteredWorkspace.Resolved)
                    PanelPartitions[RegisteredWorkspace.Resolve()].ConstructPanelPartition(PanelSubject::Viewport);
            }

            // 🔴 With nothing open there is no tab bar to seat a `+` in, so the empty shell carries the
            //    invitation itself. `WorkspacePanel` draws "CREATE PANEL" on plain black; a press anywhere
            //    on that ground registers one, which is the way out of a state that otherwise has none.
            if (OpenCount == 0u && Viewport.Seam().VacantPressed(Whole))
            {
                const Outcome<std::uint32_t> RegisteredWorkspace = Workspaces.Register(DefaultSubject);
                if (RegisteredWorkspace.Resolved)
                    PanelPartitions[RegisteredWorkspace.Resolve()].ConstructPanelPartition(PanelSubject::Viewport);
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
                const SharedCodexActivation ActivatedScene = ConsumeSharedCodexActivation(
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

            // Apply the Control Centre preference to the shared appearance instead of displaying a
            // disconnected percentage. Content Browser caches derived metrics and is reseated explicitly.
            if (Viewport.ApplyInterfaceScale(ControlCentreValues.Scaling))
            {
                Discard(Viewport.Seam().ApplyWorkspaceStyle(
                    Viewport.Appearance().WorkspaceMeasure,
                    Viewport.Appearance().Workspace));
                ContentBrowser.Reapply(Viewport.Appearance());
            }
            Discard(Viewport.Seam().ApplyInterfaceAntialiasing(
                ControlCentreValues.GeometryAntialiasing));

            // 📝 Compared rather than watched. The Control Centre writes the artist's choice straight into the
            //    ordinates, so the change is visible here as a difference and needs no callback to report it.
            {
                ThemeSelection Chosen;
                Chosen.Current   = ControlCentreValues.Theme;
                Chosen.Primary     = ControlCentreValues.Primary;
                Chosen.Secondary   = ControlCentreValues.Secondary;
                Chosen.Information = ControlCentreValues.Information;
                Chosen.Warning     = ControlCentreValues.Warning;
                Chosen.Alert       = ControlCentreValues.Alert;
                if (ControlCentreValues.Font < Fonts.FamilyCount() && Fonts.FamilyName(ControlCentreValues.Font) != nullptr)
                    std::snprintf(Chosen.FontFamily, sizeof(Chosen.FontFamily), "%s", Fonts.FamilyName(ControlCentreValues.Font));

                // 🔴 Only the family re-runs the font pipeline. The other members are colours and reach
                //    every panel through the appearance; re-loading fonts for them would re-rasterise
                //    the whole atlas on every theme or colour edit.
                const bool FamilyAltered = std::strcmp(Chosen.FontFamily, InscribedSelection.FontFamily) != 0;
                const bool Altered = Chosen.Current   != InscribedSelection.Current
                                  || Chosen.Primary     != InscribedSelection.Primary
                                  || Chosen.Secondary   != InscribedSelection.Secondary
                                  || Chosen.Information != InscribedSelection.Information
                                  || Chosen.Warning     != InscribedSelection.Warning
                                  || Chosen.Alert       != InscribedSelection.Alert
                                  || std::strcmp(Chosen.FontFamily, InscribedSelection.FontFamily) != 0;

                // 🔴 The record is advanced whether the write was delivered or rejected. A read-only folder would
                //    otherwise have every later tick retry the same rejected write for the life of the process.
                if (Altered)
                {
                    Discard(ThemeInterchange::RecordBeside(InvokedAs, Chosen));
                    InscribedSelection = Chosen;

                    // 🔴 Declared to the viewport, which re-anchors the whole appearance on the next tick, and
                    //    then pushed into the two panels that keep their own copy of the inks. The shell reads
                    //    the appearance through its own Reapply, which the viewport already calls.
                    Viewport.Retint(Chosen);
                    Discard(Viewport.Seam().ApplyWorkspaceStyle(
                        Viewport.Appearance().WorkspaceMeasure,
                        Viewport.Appearance().Workspace));
                    ContentBrowser.Reapply(Viewport.Appearance());
                    if (FamilyAltered)
                        Fonts.RequestLoad(FontRoot.c_str(), Viewport.Appearance().Fonts, 1.0f);
                }
            }
            ControlCentre.Exclude(Viewport.Drawers());
            Discard(Viewport.Surface().SwitchLayer(RecordingSurface::ShellLayer::Beneath));

            if (Viewport.SealPanels().Resolved)
            {
                Discard(Lifetime.BeginDisplay());

                // 🔴 Read. A rejected Record presents the cleared ground with nothing on it, which is
                //    indistinguishable from a panel that drew nothing, so the refusal is named here.
                if (!Viewport.Record(Pass.Recording))
                {
                    std::printf("%s \u2014 the interface content was not recorded\n", HostName);
                }
            }
            else
            {
                Discard(Viewport.Abandon());
            }
        }
        else
        {
            Discard(Viewport.Abandon());
        }

        // ⑤ Close the scope, submit, present, advance. A rejected present re-establishes the chain rather
        //    than ending the loop.
        if (!Lifetime.Complete().Resolved)
            break;
    }

    // ─────────────────────────────────────────────────────────────────────────────────────────────────────
    //                                                      RECLAMATION
    // ─────────────────────────────────────────────────────────────────────────────────────────────────────

    // 📝 The viewport is retired before the lifetimes it was constructed over. HostLifecycle idles the
    //    device inside Reclaim, so nothing here needs to.
    // 🔴 Read before Reclaim. The register is Device lifetime, and a reclaimed device has emptied it.
    const std::uint32_t Serious = Lifetime.StateDiagnostics();

    ControlCentre.Reset();
    WorkspacePanels.Reset();
    for (std::uint32_t Index = 0u; Index < WorkspaceIndex::WorkspaceLimit; ++Index)
        PanelPartitions[Index].Reset();
    Workspace.Reset();
    Workspaces.Reset();
    Viewport.Reclaim();
    Lifetime.Reclaim();

    std::printf("%s \u2014 exited cleanly\n", HostName);

    // 🔴 Returned rather than only stated. A validation run needs an exit code, so that a serious arrival
    //    fails whatever invoked the host instead of scrolling past in a console nobody reads.
    return (Serious == 0u) ? 0 : 1;
}
