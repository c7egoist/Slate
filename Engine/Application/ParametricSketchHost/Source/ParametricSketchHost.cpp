//============================================================================================================================================
//                                                        PARAMETRICSKETCHHOST.CPP
//============================================================================================================================================
// 🧩 Dedicated bring-up host for the parametric workspace path: docked workspace chrome, the dedicated CAD
//    outliner and Properties | Revision leaves, and host-owned exact-record / revision state bridged into the
//    UI guarantee. The CAD render pass remains a later phase; viewport leaves are placeholders for now.

#define SLATE_PARAMETRIC_SKETCH_HOST 1
#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"
#include "Foundation/DeliveryGuarantee.h"
#include "SlateWorkspace/Discipline/CodexActivation/Api/CodexActivation.h"
#include "SlateWorkspace/Discipline/OrientationCube/Api/OrientationCube.h"
#include "SketchToolset/SketchTool/SketchPlacement/Api/SketchPlacement.h"
#include "SlateWorkspace/Discipline/RecordDeclaration/Api/RecordDeclaration.h"
#include "SlateWorkspace/Discipline/SketchPicking/Api/SketchPicking.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/DrawableScale.h"
#include "SlateWorkspace/Discipline/WorkplaneStanding/Api/WorkplaneStanding.h"
#include "SlateWorkspace/Discipline/TransformSequence/Api/TransformSequence.h"
#include "SlateWorkspace/Discipline/PlacementCommit/Api/PlacementCommit.h"
#include "SlateWorkspace/Discipline/WorkplaneCatalogue/Api/WorkplaneCatalogue.h"
#include "SlateWorkspace/Discipline/ConstraintAuthoring/Api/ConstraintAuthoring.h"
#include "SlateWorkspace/Discipline/SketchViewportOverlay/Api/SketchViewportOverlay.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/CadProjection.h"
#include "SlateRuntime/Session/HostEnvironment/Api/HostEnvironment.h"
#include "SlateWorkspace/Discipline/SketchDirectoryPresentation/Api/SketchDirectoryPresentation.h"
#include "SlateWorkspace/Discipline/SketchInteraction/Api/SketchInteraction.h"
#include "SlateWorkspace/Discipline/ToolAvailability/Api/ToolAvailability.h"
#include "SlateWorkspace/Discipline/TransformGizmo/Api/TransformGizmo.h"
#include "SlateWorkspace/Discipline/TransformSession/Api/TransformSession.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/SketchBasis.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/ViewportProjection.h"
#include "SlateWorkspace/Discipline/WorkspaceDeclaration/Api/WorkspaceDeclaration.h"
#include "SlateWorkspace/Discipline/CodexSceneProxy/Api/CodexSceneProxy.h"
#include "SlateShape/Record/WorkspaceDirectoryProjection/Api/WorkspaceDirectoryProjection.h"
#include "SlateShape/Record/WorkspaceNameIndex/Api/WorkspaceNameIndex.h"
#include "SlateShape/Record/WorkspacePropertyProjection/Api/WorkspacePropertyProjection.h"
#include "SlateShape/Record/WorkspaceRecordStructure/Api/WorkspaceRecordStructure.h"
#include "SlateShape/Record/WorkspaceRevisionSequence/Api/WorkspaceRevisionSequence.h"
#include "SlateShape/Sketch/ConstraintSolver/Api/ConstraintSolver.h"
#include "SlateShape/Sketch/DimensionSolver/Api/DimensionSolver.h"
#include "SlateShape/Sketch/SketchEditing/Api/SketchEditing.h"
#include "SlateShape/Sketch/ProfilePattern/Api/ProfilePattern.h"
#include "SlateShape/Sketch/ProfileReshape/Api/ProfileReshape.h"
#include "SlateShape/Sketch/ProfileArea/Api/ProfileArea.h"
#include "SlateShape/Sketch/SketchPolyline/Api/SketchPolyline.h"
#include "SlateShape/Sketch/SketchRenderingProjection/Api/SketchRenderingProjection.h"
#include "SlateShape/Sketch/SketchSelection/Api/SketchSelection.h"
#include "SlateShape/Sketch/SketchSnap/Api/SketchSnap.h"
#include "SlateShape/Sketch/SketchStructure/Api/SketchStructure.h"
#include "SlateUI/Interface/ContentBrowserPanel/Api/ContentBrowserPanel.h"
#include "SlateUI/Interface/ControlCentrePanel/Api/ControlCentrePanel.h"
#include "SlateUI/Interface/EditorPanel/Api/EditorPanel.h"
#include "SlateUI/Interface/ParametricWorkspace/Api/ParametricWorkspacePanel.h"
#include "SlateUI/Interface/ParametricTools/Api/ParametricToolsPanel.h"
#include "SlateUI/Interface/SceneDirectoryPanel/Api/SceneDirectoryPanel.h"
#include "SlateUI/Interface/ThemeInterchange/Api/ThemeInterchange.h"
#include "SlateUI/Interface/ViewportSequence/Api/ViewportSequence.h"
#include "SlateUI/Interface/WorkspacePanel/Api/WorkspaceIndex.h"
#include "Shared/OverlayGeometry.slang.h"
#include "SlateVulkan/Device/HostLifecycle/Api/HostLifecycle.h"
#include "SlateVulkan/Device/ShaderCodec/Api/ShaderCodec.h"
#include "SlateVulkan/Device/WorkspaceCadPass/Api/WorkspaceCadPass.h"
#include "SlateVulkan/Device/WorkspaceOverlayPass/Api/WorkspaceOverlayPass.h"
#include "SlateDocument/Format/WorkspaceSceneActivation/Api/WorkspaceSceneActivation.h"
#include "SlateDocument/Format/MaterialImageImport/Api/MaterialImageImport.h"
#include "SlateDocument/Format/SceneMeshImport/Api/SceneMeshImport.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <utility>
#if defined(_WIN32)
    #include <windows.h>
#else
    #include <unistd.h>
#endif
#include <vector>

namespace
{

using namespace Slate;

constexpr std::uint32_t InitialWidth  = 1440u;
constexpr std::uint32_t InitialHeight = 900u;
constexpr const char* WindowTitle = "Slate — Parametric Sketch";
constexpr const char* HostName = "ParametricSketchHost";
constexpr float WorkspaceGround[4] = { 0.06f, 0.06f, 0.08f, 1.0f };

constexpr double Pi = 3.14159265358979323846;
constexpr double CadPerspectiveFieldOfViewDegrees = 42.0;

// 📝 The plane, the view and the projection now live in
//    `SlateWorkspace/Discipline/ViewportProjection`. These aliases keep the host's existing spellings so
//    the lift reads as a lift in the diff rather than as a rename of everything it touches.
using ParametricViewOrientation = ViewportOrientation;

using ParametricViewportState = ViewportStanding;

// 📝 Picking now lives in `SlateWorkspace/Discipline/SketchPicking`. These aliases keep the host's
//    existing use sites reading as they did, so the lift shows in the diff as a lift.
using ParametricSelectionSubject = SketchPickSubject;

// 📝 The grammar and its vocabulary now live in `SlateWorkspace/Discipline/TransformSequence`. These
//    aliases keep the host's 93 existing use sites reading as they did, so the lift is visible in the
//    diff as a lift rather than as a rename of everything it touches.
using ParametricTransformMode = TransformManner;

using ParametricTransformConstraint = TransformRestriction;

using ParametricViewportSelection = SketchPick;

using ParametricTransformPlacement = SketchPlacementSubject;

// 🔴 The five members the grammar owns — Mode, Engaged, SlideAlongCurve, Constraint and Numeric — are now
//    one `TransformStanding`, reached through the accessors below. They are NOT duplicated here: a host
//    copy and a unit copy would drift the first time one was written and the other was not, which is
//    exactly the defect this step exists to remove.
/// 📝 The drag itself now lives in SlateWorkspace/Discipline/TransformSession. The host keeps the
///    old spelling as an alias so its ~90 read and write sites stay legible.
using ParametricTransformState = TransformSession;

using ParametricTransformCommandInput = TransformCommandIntake;







// 🔴 TAKES THE LEAF IN LOGICAL POINTS AND THE SWAPCHAIN IN PHYSICAL PIXELS, AND CONVERTS.
//    The shipped version did not. It built `CentreX`, `CentreY` and `Focal` from the logical leaf extent
//    and handed the shader a `DisplayWidth` in physical pixels; the shader divides one by the other to
//    reach clip space, so at any display scaling other than 100% the two disagree by exactly the scale
//    factor. A point the picker placed at logical x=1200 was drawn at x=800 on a 150% display — the
//    geometry landed hundreds of pixels from the cursor that placed it.
//
// ⚠️ `OrthoScale` and the perspective `Focal` are scaled too, not just the origin. Converting only the
//    centre would put the geometry in the right place at the wrong size.



























namespace
{















using ParametricGizmoHandle = GizmoHandle;













} // namespace










} // namespace

int main(int ArgumentCount, char** ArgumentValues)
{
    using namespace Slate;

    HostDeclaration Declared;
    Declared.Naming = HostName;
    Declared.WindowCaption = WindowTitle;
    Declared.InitialWidth = InitialWidth;
    Declared.InitialHeight = InitialHeight;
    Declared.Pacing = LatencyIntent::SteadyPacing;
    Declared.DiagnosticRequested = DiagnosticLayersRequested();

    HostLifecycle Lifetime;
    if (!Lifetime.ConstructHost(Declared).Resolved)
        return 1;

    static ViewportSequence Viewport;
    DrawerDeclaration NorthDrawer = { "ControlCentre", SymbolSubject::PulseTrace, 2u };
    DrawerDeclaration SouthDrawer = { "ContentBrowser", SymbolSubject::FolderClosed, 3u };
    if (!Viewport.ConstructViewportSequence(Attach(Lifetime.Offering()), NorthDrawer, SouthDrawer).Resolved)
    {
        std::printf("%s — the viewport sequence was rejected\n", HostName);
        return 1;
    }

    WorkspaceIndex Workspaces;
    WorkspacePanel Workspace;
    EditorPanel WorkspacePanels;
    PanelStructure PanelPartitions[WorkspaceIndex::WorkspaceLimit];
    EditorPanelConfiguration PanelConfiguration[WorkspaceIndex::WorkspaceLimit];
    ParametricViewportState ViewStates[WorkspaceIndex::WorkspaceLimit] = {};
    ControlCentrePanel ControlCentre;
    ControlCentreConfiguration ControlCentreValues;
    FontLoader Fonts;
    ControlIndex BrowserInteraction;
    ContentBrowserPanel ContentBrowser;
    ContentBrowserConfiguration ContentBrowserApplied;
    ContentLibrary ContentApplied;
    ControlIndex ParametricInteraction;
    ParametricWorkspacePanel ParametricPanel;
    ParametricToolsContext ToolsApplied = {};
    ParametricToolsPanel ToolPanel;
    ControlIndex SceneInteraction;
    SceneDirectoryPanel SceneDirectory;
    SceneDirectoryContext SceneApplied;
    SceneDirectoryRows SceneDirectoryStorage;
    WorkspaceCodex OpenedScene = {};
    bool OpenedSceneStanding = false;
    EntityRow* PresentedSceneRows = nullptr;
    std::uint32_t PresentedSceneRowCount = 0u;
    ShaderCodec CadCodec;
    WorkspaceCadPass CadPass;
    WorkspaceOverlayPass OverlayPass;

    WorkspaceNameIndex Naming;
    SketchStructure Sketch;
    WorkspaceRecordStructure Records;
    WorkspaceRevisionSequence Revisions;
    // 🔴 The planes the workspace has. Seated here rather than inside the sketch because a sketch holds
    //    exactly ONE plane and overwrites it, which is what made placing a second workplane silently
    //    re-interpret everything already drawn.
    WorkplaneCatalogue Workplanes;
    WorkspaceDirectoryProjection Directory;
    SketchDirectoryPresentation Bridge;
    static WorkspaceCadPacket CadPacket;
    static OverlayGeometry ViewportOverlays[PanelStructure::RecordLimit];
    ParametricWorkspaceContext ParametricApplied = {};
    SketchPlacement Tool = {};
    ParametricViewportSelection SemanticSelection = {};
    ParametricViewportSelection HoveredSelection = {};
    ParametricTransformState Transform = {};
    WorkspaceRecordName PendingSelection = {};
    bool ParametricSeeded = false;
    bool ProjectionWarned = false;
    bool CadPacketWarned = false;
    bool CadPassWarned = false;
    bool OverlayPassWarned = false;
    std::uint32_t UploadedCadGeneration = 0xFFFFFFFFu;
    std::uint32_t RegisterIntoNode = 0u;
    double SessionMilliseconds = 0.0;
    double LastGPressedMilliseconds = -1000.0;

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
            ControlCentreValues.Theme = Recorded.Current;
            ControlCentreValues.Primary = Recorded.Primary;
            ControlCentreValues.Secondary = Recorded.Secondary;
            ControlCentreValues.Information = Recorded.Information;
            ControlCentreValues.Warning = Recorded.Warning;
            ControlCentreValues.Alert = Recorded.Alert;
        }
    }

    ThemeSelection InscribedSelection;
    InscribedSelection.Current = ControlCentreValues.Theme;
    InscribedSelection.Primary = ControlCentreValues.Primary;
    InscribedSelection.Secondary = ControlCentreValues.Secondary;
    InscribedSelection.Information = ControlCentreValues.Information;
    InscribedSelection.Warning = ControlCentreValues.Warning;
    InscribedSelection.Alert = ControlCentreValues.Alert;
    Viewport.Retint(InscribedSelection);

    Viewport.Surface().ApplyFontLoader(Fonts);
    Discard(Fonts.Discover(FontRoot.c_str()));
    Discard(Fonts.PreparePreviews(1.0f));
    Discard(Fonts.Load(FontRoot.c_str(), Viewport.Appearance().Fonts, 1.0f));
    ControlCentre.SetFontFamilies(Fonts);
    for (std::uint32_t Index = 0u; Index < Fonts.FamilyCount(); ++Index)
        if (Fonts.FamilyName(Index) != nullptr &&
            std::strcmp(Fonts.FamilyName(Index), Viewport.Appearance().Fonts.Family) == 0)
        {
            ControlCentreValues.Font = Index;
            break;
        }

    SeedParametricWorkspace(Naming, Sketch, Records, Revisions);

    if (!Workspace.ConstructWorkspacePanel(Viewport.Surface(), Viewport.Appearance()).Resolved)
    {
        std::printf("%s — the workspace panel was rejected\n", HostName);
        return 1;
    }

    if (!WorkspacePanels.ConstructEditorPanel(Viewport.MotionSource(), Viewport.Surface(), Viewport.Appearance()).Resolved)
    {
        std::printf("%s — the editor panels were rejected\n", HostName);
        return 1;
    }

    if (!ControlCentre.ConstructControlCentrePanel(Viewport.MotionSource(), Viewport.Surface(), Viewport.Appearance()).Resolved)
    {
        std::printf("%s — the Control Centre panel was rejected\n", HostName);
        return 1;
    }

    if (!BrowserInteraction.AttachMotion(Viewport.MotionSource()).Resolved)
    {
        std::printf("%s — the content browser index was rejected\n", HostName);
        return 1;
    }

    if (!ContentBrowser.ConstructContentBrowserPanel(BrowserInteraction, Viewport.Surface(), Viewport.Appearance()).Resolved)
    {
        std::printf("%s — the content browser was rejected\n", HostName);
        return 1;
    }

    ContentBrowser.Reapply(Viewport.Appearance());
    ApplyReferenceContent(ContentApplied);
    PopulateImportDirectory(ContentBrowserApplied, EngineContentRoot);

    if (!ParametricInteraction.AttachMotion(Viewport.MotionSource()).Resolved)
    {
        std::printf("%s — the parametric workspace index was rejected\n", HostName);
        return 1;
    }

    if (!ParametricPanel.ConstructParametricWorkspacePanel(ParametricInteraction,
                                                           Viewport.MotionSource(),
                                                           Viewport.Surface(),
                                                           Viewport.Appearance()).Resolved)
    {
        std::printf("%s — the parametric workspace panel was rejected\n", HostName);
        return 1;
    }

    if (!ToolPanel.ConstructParametricToolsPanel(ParametricInteraction,
                                                 Viewport.MotionSource(),
                                                 Viewport.Surface(),
                                                 Viewport.Appearance()).Resolved)
    {
        std::printf("%s — the parametric tools panel was rejected\n", HostName);
        return 1;
    }

    if (!SceneInteraction.AttachMotion(Viewport.MotionSource()).Resolved)
    {
        std::printf("%s — the scene directory index was rejected\n", HostName);
        return 1;
    }

    if (!SceneDirectory.ConstructSceneDirectoryPanel(SceneInteraction,
                                                     Viewport.MotionSource(),
                                                     Viewport.Surface(),
                                                     Viewport.Appearance()).Resolved)
    {
        std::printf("%s — the scene directory was rejected\n", HostName);
        return 1;
    }

    const Deliver<bool> CodecDelivery =
        CadCodec.AttachShaderStreams(Lifetime.DeviceExchange(), ShaderStreamDirectory());
    if (!CodecDelivery.Resolved)
    {
        std::printf("%s — the CAD shader streams were not attached (reason %u: %s)\n",
                    HostName,
                    static_cast<unsigned>(CodecDelivery.Error.DeclaredReason),
                    CodecDelivery.Error.Detail);
    }
    else
    {
        const Deliver<bool> PassDelivery = CadPass.ConstructWorkspaceCadPass(Lifetime.DeviceExchange(),
                                                                            Lifetime.DiagnosticsExtension(),
                                                                            CadCodec,
                                                                            Lifetime.Offering().ColourTargetFormat);
        if (!PassDelivery.Resolved)
        {
            CadPassWarned = true;
            std::printf("%s — the CAD pass was not standing (reason %u: %s); using fallback presentation\n",
                        HostName,
                        static_cast<unsigned>(PassDelivery.Error.DeclaredReason),
                        PassDelivery.Error.Detail);
        }
    }

    if (!Viewport.Seam().ApplyWorkspaceStyle(Viewport.Appearance().WorkspaceMeasure,
                                             Viewport.Appearance().Workspace).Resolved)
    {
        std::printf("%s — the workspace style was not applied\n", HostName);
    }

    const Deliver<std::uint32_t> DefaultWorkspace = Workspaces.Register(WorkspaceSubject::Parametric);
    if (!DefaultWorkspace.Resolved)
    {
        std::printf("%s — the default workspace could not be opened\n", HostName);
        return 1;
    }
    Discard(ApplyWorkspace(DeclaredSketchWorkspace(), PanelPartitions[DefaultWorkspace.Resolve()]));
    std::printf("%s — opened %s\n", HostName, Workspaces.ActiveTitle());

    while (Lifetime.Active())
    {
        const TickPass Pass = Lifetime.Await(WorkspaceGround);
        Discard(Fonts.FlushPending());
        SessionMilliseconds += Pass.ElapsedMilliseconds;

        if (Pass.Current == TickCondition::Closed)
            break;

        if (Pass.DeviceRetiring)
        {
            Viewport.Reclaim();
            OverlayPass.Reclaim();
            CadPass.Reclaim();
            CadCodec.Reclaim();
            UploadedCadGeneration = 0xFFFFFFFFu;
            continue;
        }

        if (Lifetime.DeviceRecovered())
        {
            if (!Viewport.ConstructViewportSequence(Attach(Lifetime.Offering()), NorthDrawer, SouthDrawer).Resolved)
            {
                std::printf("%s — the interface could not be rebuilt on the recovered device\n", HostName);
                break;
            }
            static_cast<void>(Lifetime.DisplayRecovered());
            static_cast<void>(Viewport.Seam().ApplyWorkspaceStyle(Viewport.Appearance().WorkspaceMeasure,
                                                                  Viewport.Appearance().Workspace));
            ParametricPanel.Reapply(Viewport.Appearance());
            ToolPanel.Reapply(Viewport.Appearance());
            SceneDirectory.Reapply(Viewport.Appearance());
            ContentBrowser.Reapply(Viewport.Appearance());

            const Deliver<bool> Reattached =
                CadCodec.AttachShaderStreams(Lifetime.DeviceExchange(), ShaderStreamDirectory());
            if (Reattached.Resolved)
            {
                const Deliver<bool> Rebuilt = CadPass.ConstructWorkspaceCadPass(Lifetime.DeviceExchange(),
                                                                                Lifetime.DiagnosticsExtension(),
                                                                                CadCodec,
                                                                                Lifetime.Offering().ColourTargetFormat);
                if (!Rebuilt.Resolved && !CadPassWarned)
                {
                    CadPassWarned = true;
                    std::printf("%s — the CAD pass could not be rebuilt (reason %u: %s)\n",
                                HostName,
                                static_cast<unsigned>(Rebuilt.Error.DeclaredReason),
                                Rebuilt.Error.Detail);
                }
                else if (Rebuilt.Resolved)
                {
                    CadPassWarned = false;
                }
            }
            else if (!CadPassWarned)
            {
                CadPassWarned = true;
                std::printf("%s — the CAD shader streams could not be reattached (reason %u: %s)\n",
                            HostName,
                            static_cast<unsigned>(Reattached.Error.DeclaredReason),
                            Reattached.Error.Detail);
            }

            UploadedCadGeneration = 0xFFFFFFFFu;
        }
        else if (Lifetime.DisplayRecovered())
        {
            const DeviceOffering Offered = Lifetime.Offering();
            if (!Viewport.Renegotiate(Offered.MinimumDisplayImageCount, Offered.DisplayImageCount))
                std::printf("%s — the interface rejected the restated image counts\n", HostName);
        }

        if (Pass.Current != TickCondition::Recording)
            continue;

        if (!Viewport.Advance(Pass.ElapsedMilliseconds).Resolved)
        {
            Discard(Viewport.Abandon());
            continue;
        }

        const Deliver<bool> Presented = SynchroniseParametricPresentation(Records, Revisions,
                                                                          Directory, Bridge,
                                                                          ParametricApplied,
                                                                          PendingSelection,
                                                                          ParametricSeeded);
        if (!Presented.Resolved && !ProjectionWarned)
        {
            std::printf("%s — the parametric presentation bridge refused (reason %u: %s)\n",
                        HostName,
                        static_cast<unsigned>(Presented.Error.DeclaredReason),
                        Presented.Error.Detail);
            ProjectionWarned = true;
        }
        else if (Presented.Resolved)
        {
            ProjectionWarned = false;
        }
        else
        {
            Directory.Reclaim();
            Bridge.Reclaim();
        }

        const Deliver<bool> PacketProjected = ProjectSketchRendering(Sketch, Records, CadPacket);
        if (!PacketProjected.Resolved && !CadPacketWarned)
        {
            std::printf("%s — the CAD packet projection refused (reason %u: %s)\n",
                        HostName,
                        static_cast<unsigned>(PacketProjected.Error.DeclaredReason),
                        PacketProjected.Error.Detail);
            CadPacketWarned = true;
        }
        else if (PacketProjected.Resolved)
        {
            CadPacketWarned = false;
        }
        else
        {
            CadPacket.Reset();
        }

        ResolveToolContext(Directory, Records, Sketch, ParametricApplied, ToolsApplied);
        if (OpenedSceneStanding)
            BuildSceneDirectoryRows(OpenedScene, SceneDirectoryStorage);
        else
            SceneDirectoryStorage = SceneDirectoryRows{};
        AppendCadReferenceRows(Records, SceneDirectoryStorage);
        if (OpenedSceneStanding)
            SeedSceneDirectoryTransformsFromCodex(OpenedScene, SceneDirectoryStorage, SceneApplied);
        PresentedSceneRows = SceneDirectoryStorage.RowCount > 0u ? SceneDirectoryStorage.Rows : nullptr;
        PresentedSceneRowCount = SceneDirectoryStorage.RowCount;

        const PlaneExtent Whole = Spanning(0.0f, 0.0f,
                                           static_cast<float>(Pass.Width),
                                           static_cast<float>(Pass.Height));

        const PointerCondition& ForegroundPointer = Viewport.Surface().Pointer();
        const PlaneExtent NorthInterior = Viewport.Drawers().Interior(DrawerBearing::North);
        const PlaneExtent SouthInterior = Viewport.Drawers().Interior(DrawerBearing::South);
        const bool ForegroundDrawerStanding =
            (NorthInterior.MaximumY > 0.0f && NorthInterior.MinimumY < static_cast<float>(Pass.Height)) ||
            (SouthInterior.MaximumY > 0.0f && SouthInterior.MinimumY < static_cast<float>(Pass.Height));
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

        const ModifierCondition Modifiers = Viewport.Seam().Modifiers();

        Discard(Workspace.Record(Whole, Workspaces.ActiveTitle()));
        Viewport.Seam().RecordDockSpace(Whole);

        const std::uint32_t OpenCount = Workspaces.OpenCount();
        std::uint32_t Withdrawing = OpenCount;
        const std::uint32_t ApplyInto = RegisterIntoNode;
        RegisterIntoNode = 0u;

        PlaneExtent ViewportLeafRects[PanelStructure::RecordLimit] = {};

        // 📝 The leaf rectangles above are LOGICAL points; this is what converts them for the scissor.
        DrawableScale ViewportLeafScale = {};
        WorkspaceCadProjection ViewportCadProjections[PanelStructure::RecordLimit] = {};
        std::uint32_t ViewportLeafTally = 0u;
        PlaneExtent ToolLeafRects[PanelStructure::RecordLimit] = {};
        std::uint32_t ToolLeafTally = 0u;
        PlaneExtent SceneLeafRects[PanelStructure::RecordLimit] = {};
        std::uint32_t SceneLeafTally = 0u;
        PlaneExtent SketchLeafRects[PanelStructure::RecordLimit] = {};
        std::uint32_t SketchLeafTally = 0u;

        const bool TabPressed = Viewport.Seam().KeyPressed(KeySubject::Summon);

        // Panel leaves must sample pointer/contact before they record. This keeps CAD tool tiles,
        // Sketch Directory rows and Scene Directory rows clickable in the same frame; advancing them
        // after Record meant their widgets saw stale contact state and user clicks appeared to do nothing.
        ParametricInteraction.Advance(Viewport.Surface().Pointer(), Pass.ElapsedMilliseconds);
        SceneInteraction.Advance(Viewport.Surface().Pointer(), Pass.ElapsedMilliseconds);
        ParametricPanel.Advance(BackgroundPointer, Pass.ElapsedMilliseconds,
                                ParametricApplied, false, Viewport.Seam().Modifiers());
        ToolPanel.Advance(BackgroundPointer, Pass.ElapsedMilliseconds,
                          ToolsApplied, false);
        SceneDirectory.Advance(BackgroundPointer, Pass.ElapsedMilliseconds,
                               SceneApplied, false, Viewport.Seam().Modifiers());

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
                                               Index,
                                               true));

                for (std::uint32_t Leaf = 0u; Leaf < WorkspacePanels.LeafCount(); ++Leaf)
                {
                    const PlaneExtent LeafBody = WorkspacePanels.LeafBody(Leaf);
                    switch (WorkspacePanels.LeafSubject(Leaf))
                    {
                        case PanelSubject::Outliner:
                            if (SceneLeafTally < PanelStructure::RecordLimit)
                                SceneLeafRects[SceneLeafTally++] = LeafBody;
                            SceneDirectory.RecordOutliner(LeafBody, SceneApplied,
                                                          PresentedSceneRows, PresentedSceneRowCount);
                            break;

                        case PanelSubject::SketchDirectory:
                        {
                            if (SketchLeafTally < PanelStructure::RecordLimit)
                                SketchLeafRects[SketchLeafTally++] = LeafBody;
                            const bool PropertyPresented = Bridge.Property.Naming != nullptr
                                                        && Bridge.Property.Naming[0] != '\0';
                            ParametricPanel.RecordOutliner(LeafBody, ParametricApplied,
                                Bridge.DirectoryRows.empty() ? nullptr : Bridge.DirectoryRows.data(),
                                static_cast<std::uint32_t>(Bridge.DirectoryRows.size()),
                                PropertyPresented ? &Bridge.Property : nullptr,
                                Bridge.RevisionRows.empty() ? nullptr : Bridge.RevisionRows.data(),
                                static_cast<std::uint32_t>(Bridge.RevisionRows.size()));
                            if (ParametricApplied.ExtrusionCapToggleDemand)
                            {
                                WorkspaceRecordName Subject = { static_cast<std::uint32_t>(ParametricApplied.ExtrusionCapToggleIdentity) };
                                if (WorkspaceRecord* Record = Records.Resolve(Subject))
                                {
                                    Record->CappedExtrusionSemantic = !Record->CappedExtrusionSemantic;
                                    Revisions.Seal(std::string("Set ") + Record->Naming +
                                                   (Record->CappedExtrusionSemantic ? " solid extrusion" : " wall extrusion"),
                                                   "Toggle Extrude Caps", { Subject },
                                                   Revisions.DeclaredCount() + 1u);
                                    PendingSelection = Subject;
                                }
                                ParametricApplied.ExtrusionCapToggleDemand = false;
                                ParametricApplied.ExtrusionCapToggleIdentity = 0u;
                            }
                            break;
                        }

                        case PanelSubject::ParametricTools:
                            if (ToolLeafTally < PanelStructure::RecordLimit)
                                ToolLeafRects[ToolLeafTally++] = LeafBody;
                            ToolPanel.Record(LeafBody, ToolsApplied);
                            break;

                        case PanelSubject::Viewport:
                        {
                            OverlayGeometry* LeafOverlay = nullptr;
                            if (ViewportLeafTally < PanelStructure::RecordLimit)
                            {
                                ViewportLeafRects[ViewportLeafTally] = LeafBody;
                                LeafOverlay = &ViewportOverlays[ViewportLeafTally];
                                LeafOverlay->Reset();
                            }

                            ParametricViewportState& View = ViewStates[Index];
                            const bool PointerInside = LeafBody.Encloses(BackgroundPointer.PositionX, BackgroundPointer.PositionY);
                            bool PointerTaken = false;
                            Viewport.Surface().Confine(LeafBody);
                            RecordViewportOrientationHud(Viewport.Surface(), LeafBody, BackgroundPointer,
                                                         View, PanelConfiguration[Index],
                                                         PointerTaken);
                            if (!PointerTaken && !Transform.Engaged())
                                DriveViewport(LeafBody, BackgroundPointer, Modifiers,
                                              View, PanelConfiguration[Index].Perspective);

                            const SpatialBasis Basis = ResolveSketchBasis(Sketch);
                            if (LeafOverlay != nullptr)
                                RecordViewportGridOverlay(*LeafOverlay, LeafBody, Sketch, View,
                                                          PanelConfiguration[Index].Perspective,
                                                          PanelConfiguration[Index]);

                            DriveViewportSelectionAndTransform(LeafBody, BackgroundPointer,
                                                               Viewport.Surface().TextInput(), Modifiers,
                                                               Basis, View,
                                                               PanelConfiguration[Index].Perspective,
                                                               ToolsApplied.ActiveSubject,
                                                               Naming,
                                                               Directory, ParametricApplied,
                                                               Sketch, Records, Revisions,
                                                               PendingSelection,
                                                               SemanticSelection, HoveredSelection,
                                                               Transform,
                                                               LeafOverlay != nullptr ? *LeafOverlay : ViewportOverlays[0],
                                                               PointerTaken,
                                                               SessionMilliseconds,
                                                               LastGPressedMilliseconds);

                            // 🔴 THE WORKPLANE TOOL RUNS BEFORE THE DRAWING TOOLS AND CONSUMES THE PRESS.
                            //    It returns true only when the Workplane subject is active, so the order
                            //    costs nothing for every other tool; what it buys is that the press which
                            //    places a plane cannot also be read as the first point of a curve on the
                            //    plane it just replaced.
                            if (!Transform.Engaged() && !PointerTaken)
                                PointerTaken = ApplyWorkplaneTool(LeafBody, BackgroundPointer,
                                                                  Basis, View,
                                                                  PanelConfiguration[Index].Perspective,
                                                                  ToolsApplied,
                                                                  Naming, Sketch, Records, Revisions,
                                                                  Workplanes);

                            if (!Transform.Engaged() && !PointerTaken)
                                DriveDrawingWithModifiers(LeafBody, BackgroundPointer,
                                                         Viewport.Surface().TextInput(), Modifiers,
                                                         Basis, View,
                                                         PanelConfiguration[Index].Perspective,
                                                         ToolsApplied,
                                                         Naming, Sketch, Records, Revisions, Workplanes,
                                                         PendingSelection, Tool, PointerTaken);

                            // 📝 The parametric workspace works in millimetres; a codex stores metres.
                            const ResolvedCamera SceneCamera =
                                ResolveOrbitCamera(Basis, View, PanelConfiguration[Index].Perspective);

                            if (!PointerTaken && PointerInside && ToolsApplied.ActiveSubject == ParametricToolSubject::Select)
                                PointerTaken = SelectSceneMeshAtPointer(LeafBody, BackgroundPointer, SceneCamera,
                                                                        OpenedScene, OpenedSceneStanding,
                                                                        SceneDirectoryStorage,
                                                                        CodexMetresToMillimetres, SceneApplied);
                            RecordCodexSceneProxy(Viewport.Surface(), LeafBody, SceneCamera,
                                                  OpenedScene, OpenedSceneStanding,
                                                  SceneDirectoryStorage, SceneApplied,
                                                  CodexMetresToMillimetres);

                            Discard(ProjectSketchRendering(Sketch, Records, CadPacket));
                            if (!CadPass.Standing())
                                RecordCadFallback(Viewport.Surface(), LeafBody, Sketch, View,
                                                  PanelConfiguration[Index].Perspective, CadPacket);
                            RecordPlacementPreview(Viewport.Surface(), LeafBody, Sketch, View,
                                               PanelConfiguration[Index].Perspective, Tool);
                            RecordViewportStateReadout(Viewport.Surface(), LeafBody, View,
                                                       PanelConfiguration[Index].Perspective, CadPacket);
                            RecordProfileAreaOverlay(Viewport.Surface(), LeafBody, Sketch, View,
                                                       PanelConfiguration[Index].Perspective);
                            RecordConstraintGlyphs(Viewport.Surface(), LeafBody, Sketch, View,
                                                   PanelConfiguration[Index].Perspective);
                            RecordProfileValidationReadout(Viewport.Surface(), LeafBody, Sketch);
                            if (LeafOverlay != nullptr)
                            {
                                SpatialPoint ScenePivot = {};
                                if (ResolveSelectedSceneMeshPivot(OpenedScene, OpenedSceneStanding,
                                                                  SceneDirectoryStorage, SceneApplied,
                                                                  CodexMetresToMillimetres, ScenePivot))
                                {
                                    ParametricViewportSelection SceneSelection = {};
                                    SceneSelection.Subject = ParametricSelectionSubject::Record;
                                    SceneSelection.Position = ScenePivot;
                                    RecordViewportGizmo(*LeafOverlay, LeafBody, Basis, View,
                                                        PanelConfiguration[Index].Perspective,
                                                        SceneSelection, ParametricGizmoHandle::None, Transform);
                                }
                            }
                            RecordViewportTransformReadout(Viewport.Surface(), LeafBody, Transform);
                            if (LeafOverlay != nullptr && !OverlayPass.Standing())
                                RecordViewportOverlayFallback(Viewport.Surface(), LeafBody, *LeafOverlay);
                            Viewport.Surface().Release();
                            if (PointerTaken)
                                Viewport.Seam().WithholdPointer();

                            if (ViewportLeafTally < PanelStructure::RecordLimit)
                            {
                                // 🔴 Read from the two extents that were actually measured this frame,
                                //    rather than a scale reported separately: a reported value can be a
                                //    frame stale after the window moves between monitors, and a stale
                                //    scale is precisely the mismatch this is here to prevent.
                                const DrawableScale Drawable = DrawableScale::Between(
                                    Viewport.Surface().Display().Width,
                                    static_cast<double>(Pass.Width));

                                ViewportCadProjections[ViewportLeafTally] = ResolveCadProjection(
                                    Basis, View,
                                    PanelConfiguration[Index].Perspective,
                                    LeafBody, Drawable, Pass.Width, Pass.Height);
                                ViewportLeafScale = Drawable;
                                ++ViewportLeafTally;
                            }
                            break;
                        }

                        default:
                            break;
                    }
                }

                WorkspacePanels.RecordDeferredPopups(PanelPartitions[Index], PanelConfiguration[Index]);
                if (WorkspacePanels.PointerCaptured(Index))
                    Viewport.Seam().WithholdPointer();
            }

            Viewport.Seam().LeaveWorkspaceWindow();
            Discard(Viewport.Surface().SwitchLayer(RecordingSurface::ShellLayer::Beneath));
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
                PanelConfiguration[Moving] = PanelConfiguration[Moving + 1u];
            }
            PanelPartitions[OpenCount - 1u].Reset();
            PanelConfiguration[OpenCount - 1u] = EditorPanelConfiguration{};
        }

        std::uint32_t AskingNode = 0u;
        if (Viewport.Seam().RecordWorkspaceAddition(Workspace.Strip(), OpenCount, AskingNode))
        {
            RegisterIntoNode = AskingNode;
            const Deliver<std::uint32_t> RegisteredWorkspace = Workspaces.Register(WorkspaceSubject::Parametric);
            if (RegisteredWorkspace.Resolved)
                Discard(ApplyWorkspace(DeclaredSketchWorkspace(), PanelPartitions[RegisteredWorkspace.Resolve()]));
        }

        if (OpenCount == 0u && Viewport.Seam().VacantPressed(Whole))
        {
            const Deliver<std::uint32_t> RegisteredWorkspace = Workspaces.Register(WorkspaceSubject::Parametric);
            if (RegisteredWorkspace.Resolved)
                Discard(ApplyWorkspace(DeclaredSketchWorkspace(), PanelPartitions[RegisteredWorkspace.Resolve()]));
        }

        Viewport.RecordDrawers();
        Viewport.DrawerPanels();

        const PlaneExtent BrowserInterior = Viewport.Drawers().Interior(DrawerBearing::South);
        BrowserInteraction.Advance(Viewport.Surface().Pointer(), Pass.ElapsedMilliseconds);
        ContentBrowser.Advance(Viewport.Surface().Pointer(), Pass.ElapsedMilliseconds);
        if (BrowserInterior.Width() > 0.0f && BrowserInterior.Height() > 0.0f)
        {
            Discard(Viewport.Surface().SwitchLayer(RecordingSurface::ShellLayer::Above));
            const PlaneExtent BrowserBody = Viewport.Drawers().Body(DrawerBearing::South);
            Viewport.Surface().Ground(BrowserBody, Covering(0x111114u), 0.0f, CornerNone);
            Viewport.Surface().Ground(BrowserInterior, Viewport.Appearance().Colour.SurfaceCurrent,
                                      0.0f, CornerNone);
            ContentBrowser.RecordBrowser(BrowserInterior, ContentApplied, ContentBrowserApplied);
            ContentBrowser.RecordDeferred();

            const CodexActivation ActivatedScene = ConsumeCodexActivation(
                ContentBrowserApplied, ContentApplied, EngineContentRoot);
            if (ActivatedScene.Requested && !ActivatedScene.Resolved)
            {
                std::printf("%s — workspace activation refused (reason %u: %s)\n", HostName,
                            static_cast<unsigned>(ActivatedScene.Error.DeclaredReason), ActivatedScene.Error.Detail);
            }
            else if (ActivatedScene.Resolved)
            {
                OpenedScene = ActivatedScene.Scene.Workspace;
                OpenedSceneStanding = true;
                SceneApplied.TransformSeeded = false;
                ApplySceneEnvironment(OpenedScene, SceneApplied);

                SpatialPoint Focus = {};
                std::uint32_t GeometryCount = 0u;
                for (const CodexSceneEntry& Entry : OpenedScene.Scene)
                {
                    if (Entry.Subject != CodexSceneSubject::Geometry)
                        continue;
                    const SpatialPoint Position = CodexScenePosition(Entry, CodexMetresToMillimetres);
                    Focus.Left += Position.Left;
                    Focus.Up += Position.Up;
                    Focus.Forward += Position.Forward;
                    ++GeometryCount;
                }
                if (GeometryCount > 0u)
                {
                    Focus.Left /= static_cast<double>(GeometryCount);
                    Focus.Up /= static_cast<double>(GeometryCount);
                    Focus.Forward /= static_cast<double>(GeometryCount);
                    for (ParametricViewportState& View : ViewStates)
                    {
                        View.Focus = Focus;
                        View.Distance = std::max(View.Distance, 420.0);
                        View.OrthoScale = std::max(View.OrthoScale, 3.0);
                    }
                }
            }
            if (ContentBrowserApplied.ImportConfirmed)
            {
                if (ContentBrowserApplied.ImportTaken < ContentBrowserApplied.ImportEntryCount &&
                    !ContentBrowserApplied.ImportEntries[ContentBrowserApplied.ImportTaken].Directory)
                {
                    const std::filesystem::path ImportPath = std::filesystem::path(ContentBrowserApplied.ImportLocation) /
                        ContentBrowserApplied.ImportEntries[ContentBrowserApplied.ImportTaken].Naming;
                    if (MaterialImageFormatSupported(ImportPath.string()))
                    {
                        const Deliver<ImportedMaterialImage> Imported = ImportMaterialImageReference(ImportPath.string());
                        if (!Imported.Resolved)
                        {
                            std::printf("%s — material image import refused (reason %u: %s)\n", HostName,
                                        static_cast<unsigned>(Imported.Error.DeclaredReason), Imported.Error.Detail);
                        }
                        else
                        {
                            if (!OpenedSceneStanding)
                            {
                                OpenedScene = {};
                                OpenedScene.Naming = "Imported Material Scene";
                                OpenedSceneStanding = true;
                            }
                            EnsureWorkspaceMaterialRecords(OpenedScene);
                            std::string MaterialReference = OpenedScene.Materials.empty()
                                ? std::string("ImportedImageMaterial") : OpenedScene.Materials.front().Reference;
                            if (SceneApplied.EntityTaken < SceneDirectoryStorage.RowCount)
                            {
                                const StableRowIdentity Identity = SceneDirectoryStorage.Rows[SceneApplied.EntityTaken].Identity;
                                if (Identity >= 6200u && Identity - 6200u < OpenedScene.Scene.size())
                                    MaterialReference = OpenedScene.Scene[Identity - 6200u].MaterialReference;
                            }
                            if (OpenedScene.Materials.empty())
                                OpenedScene.Materials.push_back(DefaultWorkspaceMaterialRecord(MaterialReference));
                            for (WorkspaceMaterialRecord& Material : OpenedScene.Materials)
                            {
                                if (Material.Reference != MaterialReference)
                                    continue;
                                const Deliver<std::uint32_t> Bound = BindWorkspaceMaterialImage(
                                    Material, Imported.Resolve().SuggestedChannel, Imported.Resolve().Reference);
                                if (!Bound.Resolved)
                                    std::printf("%s — material image bind refused (reason %u: %s)\n", HostName,
                                                static_cast<unsigned>(Bound.Error.DeclaredReason), Bound.Error.Detail);
                                else
                                    std::printf("%s — imported material image %s for channel %u on %s\n",
                                                HostName, Imported.Resolve().Reference.ReferenceName.c_str(),
                                                static_cast<unsigned>(Imported.Resolve().SuggestedChannel),
                                                Material.Reference.c_str());
                                break;
                            }
                        }
                    }
                    else
                    {
                        const Deliver<ImportedSceneMesh> Imported = ImportSceneMeshFile(ImportPath.string());
                        if (!Imported.Resolved)
                        {
                            std::printf("%s — mesh import refused (reason %u: %s)\n", HostName,
                                        static_cast<unsigned>(Imported.Error.DeclaredReason), Imported.Error.Detail);
                        }
                        else
                        {
                            if (!OpenedSceneStanding)
                            {
                                OpenedScene = {};
                                OpenedScene.Naming = "Imported Mesh Scene";
                                OpenedSceneStanding = true;
                            }
                            OpenedScene.Scene.push_back(Imported.Resolve().Entry);
                            OpenedScene.SceneMeshes.push_back(Imported.Resolve().Mesh);
                            for (const WorkspaceMaterialRecord& Material : Imported.Resolve().MaterialRecords)
                                OpenedScene.Materials.push_back(Material);
                            EnsureWorkspaceMaterialRecords(OpenedScene);
                            SceneApplied.TransformSeeded = false;
                            std::printf("%s — imported mesh %s (%zu vertices, %zu triangles, %zu material slots)\n",
                                        HostName, Imported.Resolve().Entry.Naming.c_str(),
                                        Imported.Resolve().Mesh.Positions.size() / 3u,
                                        Imported.Resolve().Mesh.Indices.size() / 3u,
                                        Imported.Resolve().MaterialSlots.size());
                        }
                    }
                }
                ContentBrowserApplied.ImportConfirmed = false;
            }

            if (ContentBrowserApplied.ImportBrowseRequested)
            {
                std::filesystem::path Destination(ContentBrowserApplied.ImportLocation);
                if (ContentBrowserApplied.ImportTaken < ContentBrowserApplied.ImportEntryCount &&
                    ContentBrowserApplied.ImportEntries[ContentBrowserApplied.ImportTaken].Directory)
                {
                    Destination /= ContentBrowserApplied.ImportEntries[ContentBrowserApplied.ImportTaken].Naming;
                }
                PopulateImportDirectory(ContentBrowserApplied, Destination);
                ContentBrowserApplied.ImportBrowseRequested = false;
            }

            ContentBrowser.Exclude(Viewport.Drawers(), DrawerBearing::South);
            Discard(Viewport.Surface().SwitchLayer(RecordingSurface::ShellLayer::Beneath));
        }

        const PlaneExtent ControlInterior = Viewport.Drawers().Interior(DrawerBearing::North);
        ControlCentre.Advance(Viewport.Surface().Pointer(), Pass.ElapsedMilliseconds);
        Viewport.ApplyTypographyRoles(ControlCentreValues.TypographySize,
                                      ControlCentreValues.TypographyWeight);
        Discard(Viewport.Surface().SwitchLayer(RecordingSurface::ShellLayer::Above));
        if (ControlInterior.Width() > 0.0f && ControlInterior.Height() > 0.0f)
        {
            const PlaneExtent ControlBody = Viewport.Drawers().Body(DrawerBearing::North);
            Viewport.Surface().Ground(ControlBody, Covering(0x111114u), 0.0f, CornerNone);
            Viewport.Surface().Ground(ControlInterior, Viewport.Appearance().Colour.SurfaceCurrent,
                                      0.0f, CornerNone);
        }
        Discard(ControlCentre.Record(ControlInterior, ControlCentreValues));

        if (Viewport.ApplyInterfaceScale(ControlCentreValues.Scaling))
        {
            Discard(Viewport.Seam().ApplyWorkspaceStyle(Viewport.Appearance().WorkspaceMeasure,
                                                        Viewport.Appearance().Workspace));
            ContentBrowser.Reapply(Viewport.Appearance());
            ParametricPanel.Reapply(Viewport.Appearance());
            ToolPanel.Reapply(Viewport.Appearance());
            SceneDirectory.Reapply(Viewport.Appearance());
        }
        Discard(Viewport.Seam().ApplyInterfaceAntialiasing(ControlCentreValues.GeometryAntialiasing));

        {
            ThemeSelection Chosen;
            Chosen.Current = ControlCentreValues.Theme;
            Chosen.Primary = ControlCentreValues.Primary;
            Chosen.Secondary = ControlCentreValues.Secondary;
            Chosen.Information = ControlCentreValues.Information;
            Chosen.Warning = ControlCentreValues.Warning;
            Chosen.Alert = ControlCentreValues.Alert;
                if (ControlCentreValues.Font < Fonts.FamilyCount() && Fonts.FamilyName(ControlCentreValues.Font) != nullptr)
                    std::snprintf(Chosen.FontFamily, sizeof(Chosen.FontFamily), "%s", Fonts.FamilyName(ControlCentreValues.Font));

            const bool FamilyAltered = std::strcmp(Chosen.FontFamily, InscribedSelection.FontFamily) != 0;
            const bool Altered = Chosen.Current != InscribedSelection.Current
                              || Chosen.Primary != InscribedSelection.Primary
                              || Chosen.Secondary != InscribedSelection.Secondary
                              || Chosen.Information != InscribedSelection.Information
                              || Chosen.Warning != InscribedSelection.Warning
                              || Chosen.Alert != InscribedSelection.Alert
                              || std::strcmp(Chosen.FontFamily, InscribedSelection.FontFamily) != 0;

            if (Altered)
            {
                Discard(ThemeInterchange::RecordBeside(InvokedAs, Chosen));
                InscribedSelection = Chosen;
                Viewport.Retint(Chosen);
                Discard(Viewport.Seam().ApplyWorkspaceStyle(Viewport.Appearance().WorkspaceMeasure,
                                                            Viewport.Appearance().Workspace));
                ContentBrowser.Reapply(Viewport.Appearance());
                ParametricPanel.Reapply(Viewport.Appearance());
                ToolPanel.Reapply(Viewport.Appearance());
                SceneDirectory.Reapply(Viewport.Appearance());
                if (FamilyAltered)
                    Fonts.RequestLoad(FontRoot.c_str(), Viewport.Appearance().Fonts, 1.0f);
            }
        }
        ControlCentre.Exclude(Viewport.Drawers());
        Discard(Viewport.Surface().SwitchLayer(RecordingSurface::ShellLayer::Beneath));

        if (TabPressed && !PointerBehindDrawer)
        {
            const PointerCondition& Hovered = Viewport.Surface().Pointer();
            for (std::uint32_t Index = 0u; Index < SketchLeafTally; ++Index)
                if (SketchLeafRects[Index].Encloses(Hovered.PositionX, Hovered.PositionY))
                    ParametricApplied.OutlinePage = ParametricApplied.OutlinePage == 0u ? 1u : 0u;
            for (std::uint32_t Index = 0u; Index < ToolLeafTally; ++Index)
                if (ToolLeafRects[Index].Encloses(Hovered.PositionX, Hovered.PositionY))
                    ToolsApplied.Page = ToolsApplied.Page == ParametricToolPage::Catalogue
                                      ? ParametricToolPage::Settings : ParametricToolPage::Catalogue;
            for (std::uint32_t Index = 0u; Index < SceneLeafTally; ++Index)
                if (SceneLeafRects[Index].Encloses(Hovered.PositionX, Hovered.PositionY))
                    SceneApplied.OutlinePage = SceneApplied.OutlinePage == 0u ? 1u : 0u;
        }
        SynchroniseCodexTransformsFromSceneDirectory(OpenedScene, SceneDirectoryStorage,
                                                     SceneApplied, OpenedSceneStanding);

        if (ParametricApplied.SearchTaken)
        {
            static_cast<void>(Viewport.Seam().AcceptTyped(ParametricApplied.RowRetention,
                                                          ParametricWorkspaceContext::SearchLimit));

            if (Viewport.Seam().KeyPressed(KeySubject::Retract))
            {
                std::uint32_t Occupied = 0u;
                while (Occupied + 1u < ParametricWorkspaceContext::SearchLimit &&
                       ParametricApplied.RowRetention[Occupied] != '\0')
                    ++Occupied;
                if (Occupied > 0u)
                    ParametricApplied.RowRetention[Occupied - 1u] = '\0';
            }

            if (Viewport.Seam().KeyPressed(KeySubject::Withdraw))
                ParametricApplied.RowRetention[0] = '\0';
        }
        else if (Viewport.Seam().KeyPressed(KeySubject::Withdraw))
        {
            if (Transform.Engaged())
                CancelTransformSession(Sketch, Transform);
            else
                Tool.Abandon();
        }

        if (Viewport.SealPanels().Resolved)
        {
            Discard(Lifetime.BeginDisplay());
            if (!Viewport.Record(Pass.Recording))
                std::printf("%s — the interface content was not recorded\n", HostName);

            if (CadPass.Standing())
            {
                if (UploadedCadGeneration != CadPacket.Generation)
                {
                    CadPass.Upload(CadPacket);
                    UploadedCadGeneration = CadPacket.Generation;
                }

                for (std::uint32_t ViewportIndex = 0u; ViewportIndex < ViewportLeafTally; ++ViewportIndex)
                {
                    // 🔴 The scissor is a PHYSICAL rectangle. `LeafRect` is logical, and the pass clamps
                    //    it against a physical DisplayWidth — a clamp that never fires and silently
                    //    leaves logical numbers in a physical field, clipping the wrong region.
                    const PlaneExtent LeafRect =
                        ViewportLeafScale.ToPhysical(ViewportLeafRects[ViewportIndex]);
                    CadPass.Record(Pass.Recording, ViewportCadProjections[ViewportIndex],
                                   LeafRect.MinimumX, LeafRect.MinimumY,
                                   LeafRect.MaximumX, LeafRect.MaximumY);
                }
            }

            if (OverlayPass.Standing())
            {
                for (std::uint32_t ViewportIndex = 0u;
                     !ForegroundDrawerStanding && ViewportIndex < ViewportLeafTally;
                     ++ViewportIndex)
                {
                    const PlaneExtent& LeafRect = ViewportLeafRects[ViewportIndex];
                    OverlayPass.Upload(ViewportOverlays[ViewportIndex]);
                    OverlayPass.Record(Pass.Recording, Pass.Width, Pass.Height,
                                       LeafRect.MinimumX, LeafRect.MinimumY,
                                       LeafRect.MaximumX, LeafRect.MaximumY);
                }
            }
        }
        else
        {
            Discard(Viewport.Abandon());
        }

        if (!Lifetime.Complete().Resolved)
            break;
    }

    const std::uint32_t Serious = Lifetime.StateDiagnostics();
    ControlCentre.Reset();
    ContentBrowser.Reset();
    ParametricPanel.Reset();
    ToolPanel.Reset();
    WorkspacePanels.Reset();
    for (std::uint32_t Index = 0u; Index < WorkspaceIndex::WorkspaceLimit; ++Index)
        PanelPartitions[Index].Reset();
    Workspace.Reset();
    Workspaces.Reset();
    OverlayPass.Reclaim();
    CadPass.Reclaim();
    CadCodec.Reclaim();
    Viewport.Reclaim();
    Lifetime.Reclaim();

    std::printf("%s — exited cleanly\n", HostName);
    return (Serious == 0u) ? 0 : 1;
}
