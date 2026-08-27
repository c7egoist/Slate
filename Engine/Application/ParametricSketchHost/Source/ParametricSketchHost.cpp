//============================================================================================================================================
//                                                        PARAMETRICSKETCHHOST.CPP
//============================================================================================================================================
// 🧩 Dedicated bring-up host for the parametric workspace path: docked workspace chrome, the dedicated CAD
//    outliner and Properties | Revision leaves, and host-owned exact-record / revision state bridged into the
//    UI guarantee. The CAD render pass remains a later phase; viewport leaves are placeholders for now.

#define SLATE_PARAMETRIC_SKETCH_HOST 1
#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"
#include "Foundation/DeliveryGuarantee.h"
#include "Application/Api/SharedViewportHostBridge.h"
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
#include "SlateWorkspace/Discipline/ToolAvailability/Api/ToolAvailability.h"
#include "SlateWorkspace/Discipline/TransformGizmo/Api/TransformGizmo.h"
#include "SlateWorkspace/Discipline/TransformSession/Api/TransformSession.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/SketchBasis.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/ViewportProjection.h"
#include "SlateWorkspace/Discipline/WorkspaceDeclaration/Api/WorkspaceDeclaration.h"
#include "Application/Api/ParametricWorkspaceBridge.h"
#include "Application/Api/SketchSceneDirectoryBridge.h"
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

std::string ShaderStreamDirectory()
{
    std::error_code Error;
    std::filesystem::path Binary;

#if defined(_WIN32)
    {
        wchar_t Executable[32768] = {};
        const DWORD Written = GetModuleFileNameW(nullptr, Executable, 32768);
        if (Written > 0u && Written < 32768u)
            Binary = std::filesystem::path(Executable).parent_path();
    }
#endif

    if (Binary.empty())
    {
#if !defined(_WIN32)
        std::vector<char> Executable(32768, '\0');
        const std::size_t Written = readlink("/proc/self/exe", Executable.data(), Executable.size());
        if (Written > 0u && Written < Executable.size())
            Binary = std::filesystem::path(std::string(Executable.data(), Written)).parent_path();
#endif
    }

    if (Binary.empty())
        Binary = std::filesystem::current_path(Error);

    return (Binary / ".." / "Shader").lexically_normal().string();
}



InterfaceAttachment Attach(const DeviceOffering& Offered)
{
    InterfaceAttachment Incoming = {};
    Incoming.Instance = Offered.Instance;
    Incoming.ScoredDevice = Offered.ScoredDevice;
    Incoming.ActiveDevice = Offered.ActiveDevice;
    Incoming.GraphicsQueue = Offered.GraphicsQueue;
    Incoming.GraphicsFamilyIndex = Offered.GraphicsFamilyIndex;
    Incoming.ColourTargetFormat = Offered.ColourTargetFormat;
    Incoming.MinimumDisplayImageCount = Offered.MinimumDisplayImageCount;
    Incoming.DisplayImageCount = Offered.DisplayImageCount;
    Incoming.NativeWindowSlot = Offered.NativeWindowSlot;
    return Incoming;
}

std::filesystem::path HomeProfilePath()
{
#if defined(_WIN32)
    char* Home = nullptr;
    std::size_t Count = 0u;
    if (_dupenv_s(&Home, &Count, "USERPROFILE") == 0 && Home != nullptr && Count > 1u)
    {
        std::filesystem::path Result = Home;
        std::free(Home);
        return Result;
    }
    if (Home != nullptr) std::free(Home);
    return {};
#else
    const char* Home = std::getenv("HOME");
    return (Home != nullptr && Home[0] != '\0') ? std::filesystem::path(Home) : std::filesystem::path{};
#endif
}

void PopulateImportDirectory(ContentBrowserConfiguration& Browser, const std::filesystem::path& Requested)
{
    std::error_code Error;
    std::filesystem::path Resolved = Requested;
    if (Requested == "Home")
    {
        const std::filesystem::path Home = HomeProfilePath();
        if (!Home.empty()) Resolved = Home;
    }
    if (Resolved.empty()) Resolved = std::filesystem::current_path(Error);

    Browser.ImportEntryCount = 0u;
    Browser.ImportTaken = ContentLibrary::AbsentIndex;
    std::snprintf(Browser.ImportLocation, sizeof(Browser.ImportLocation), "%s", Resolved.generic_string().c_str());
    if (Error || !std::filesystem::is_directory(Resolved, Error) || Error) return;

    std::vector<std::filesystem::directory_entry> Entries;
    for (std::filesystem::directory_iterator Current(Resolved, Error), End; !Error && Current != End; Current.increment(Error))
        Entries.push_back(*Current);
    std::sort(Entries.begin(), Entries.end(), [](const auto& Left, const auto& Right)
    {
        const bool LeftDirectory = Left.is_directory();
        const bool RightDirectory = Right.is_directory();
        return LeftDirectory != RightDirectory ? LeftDirectory : Left.path().filename() < Right.path().filename();
    });

    for (const auto& Current : Entries)
    {
        if (Browser.ImportEntryCount >= 128u) break;
        ContentImportEntry& Written = Browser.ImportEntries[Browser.ImportEntryCount++];
        const std::string Name = Current.path().filename().string();
        const std::string Extension = Current.path().extension().string();
        Written.Directory = Current.is_directory(Error) && !Error;
        Written.Octets = Written.Directory ? 0u : Current.file_size(Error);
        if (Error) { Error.clear(); Written.Octets = 0u; }
        std::snprintf(Written.Naming, sizeof(Written.Naming), "%s", Name.c_str());
        std::snprintf(Written.Extension, sizeof(Written.Extension), "%s", Extension.c_str());
        Written.Supported = Written.Directory || Extension == ".codex" || Extension == ".sketch" ||
                            SceneMeshFormatSupported(Current.path().string()) ||
                            MaterialImageFormatSupported(Current.path().string());
    }
}

// 🔴 TAKES THE LEAF IN LOGICAL POINTS AND THE SWAPCHAIN IN PHYSICAL PIXELS, AND CONVERTS.
//    The shipped version did not. It built `CentreX`, `CentreY` and `Focal` from the logical leaf extent
//    and handed the shader a `DisplayWidth` in physical pixels; the shader divides one by the other to
//    reach clip space, so at any display scaling other than 100% the two disagree by exactly the scale
//    factor. A point the picker placed at logical x=1200 was drawn at x=800 on a 150% display — the
//    geometry landed hundreds of pixels from the cursor that placed it.
//
// ⚠️ `OrthoScale` and the perspective `Focal` are scaled too, not just the origin. Converting only the
//    centre would put the geometry in the right place at the wrong size.
WorkspaceCadProjection ResolveCadProjection(const SpatialBasis& Basis,
                                            const ParametricViewportState& View,
                                            bool Perspective,
                                            const PlaneExtent& LogicalExtent,
                                            const DrawableScale& Drawable,
                                            std::uint32_t DisplayWidth,
                                            std::uint32_t DisplayHeight)
{
    const ViewFrame Frame = ResolveViewportFrame(Basis, View, Perspective);

    // Everything below is in PHYSICAL pixels, matching the DisplayWidth the shader divides by.
    const PlaneExtent Extent = Drawable.ToPhysical(LogicalExtent);
    const double      OrthoScale = View.OrthoScale * Drawable.Factor;

    const float CentreX = Extent.MinimumX + Extent.Width() * 0.5f;
    const float CentreY = Extent.MinimumY + Extent.Height() * 0.5f;

    const auto ScreenProjection = [&](const SpatialPoint& Origin,
                                      const SpatialDirection& Along,
                                      const SpatialDirection& Across,
                                      float* Projection0,
                                      float* Projection1,
                                      float* Projection2)
    {
        if (!Perspective)
        {
            const SpatialDirection FocusToOrigin = Difference(View.Focus, Origin);
            const double BaseX = Dot(FocusToOrigin, Frame.Right);
            const double BaseY = Dot(FocusToOrigin, Frame.Up);
            const double AlongX = Dot(Along, Frame.Right);
            const double AlongY = Dot(Along, Frame.Up);
            const double AcrossX = Dot(Across, Frame.Right);
            const double AcrossY = Dot(Across, Frame.Up);

            Projection0[0] = CentreX + static_cast<float>(BaseX * OrthoScale);
            Projection0[1] = CentreY - static_cast<float>(BaseY * OrthoScale);
            Projection0[2] = 0.0f;
            Projection0[3] = 1.0f;

            Projection1[0] = static_cast<float>(AlongX * OrthoScale);
            Projection1[1] = static_cast<float>(-AlongY * OrthoScale);
            Projection1[2] = 0.0f;
            Projection1[3] = 0.0f;

            Projection2[0] = static_cast<float>(AcrossX * OrthoScale);
            Projection2[1] = static_cast<float>(-AcrossY * OrthoScale);
            Projection2[2] = 0.0f;
            Projection2[3] = 0.0f;
            return;
        }

        const double TanHalf = std::tan(CadPerspectiveFieldOfViewDegrees * 0.5 * Pi / 180.0);
        const double Focal = (Extent.Height() * 0.5) / TanHalf;
        const SpatialDirection EyeToOrigin = Difference(Frame.Eye, Origin);
        const double BaseX = Dot(EyeToOrigin, Frame.Right);
        const double BaseY = Dot(EyeToOrigin, Frame.Up);
        const double BaseZ = Dot(EyeToOrigin, Frame.Forward);
        const double AlongX = Dot(Along, Frame.Right);
        const double AlongY = Dot(Along, Frame.Up);
        const double AlongZ = Dot(Along, Frame.Forward);
        const double AcrossX = Dot(Across, Frame.Right);
        const double AcrossY = Dot(Across, Frame.Up);
        const double AcrossZ = Dot(Across, Frame.Forward);

        Projection0[0] = static_cast<float>(CentreX * BaseZ + Focal * BaseX);
        Projection0[1] = static_cast<float>(CentreY * BaseZ - Focal * BaseY);
        Projection0[2] = 0.0f;
        Projection0[3] = static_cast<float>(BaseZ);

        Projection1[0] = static_cast<float>(CentreX * AlongZ + Focal * AlongX);
        Projection1[1] = static_cast<float>(CentreY * AlongZ - Focal * AlongY);
        Projection1[2] = 0.0f;
        Projection1[3] = static_cast<float>(AlongZ);

        Projection2[0] = static_cast<float>(CentreX * AcrossZ + Focal * AcrossX);
        Projection2[1] = static_cast<float>(CentreY * AcrossZ - Focal * AcrossY);
        Projection2[2] = 0.0f;
        Projection2[3] = static_cast<float>(AcrossZ);
    };

    WorkspaceCadProjection Projection = {};
    Projection.DisplayWidth = static_cast<float>(DisplayWidth);
    Projection.DisplayHeight = static_cast<float>(DisplayHeight);
    ScreenProjection(Basis.Origin, Basis.Along, Basis.Across,
                     Projection.Projection0, Projection.Projection1, Projection.Projection2);
    return Projection;
}





void DriveViewport(const PlaneExtent& Extent,
                   const PointerCondition& Pointer,
                   const ModifierCondition& Modifiers,
                   ParametricViewportState& View,
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
        View.Orientation = ParametricViewOrientation::Isometric;
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
        double Angle = Length > 1.0e-6 ? std::atan2(DeltaAcross, DeltaAlong) : Settings.LineAngleDegrees * Pi / 180.0;
        double Distance = Length;
        if (Settings.LineAngleAssist)
            Angle = Settings.LineAngleDegrees * Pi / 180.0;
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




namespace
{





constexpr double CodexSceneMetreScale = 1000.0;

SpatialPoint CodexScenePosition(const CodexSceneEntry& Entry)
{
    return { Entry.Position[0] * CodexSceneMetreScale,
             Entry.Position[1] * CodexSceneMetreScale,
             Entry.Position[2] * CodexSceneMetreScale };
}

void ResolveCodexProxyExtent(const CodexSceneEntry& Entry,
                             double& HalfX,
                             double& HalfY,
                             double& HalfZ)
{
    const char* Name = Entry.Naming.c_str();
    if (std::strstr(Name, "Teapot") != nullptr)
    {
        HalfX = 150.0; HalfY = 85.0; HalfZ = 105.0;
    }
    else if (std::strstr(Name, "Teacup") != nullptr)
    {
        HalfX = 62.0; HalfY = 48.0; HalfZ = 62.0;
    }
    else if (std::strstr(Name, "Saucer") != nullptr)
    {
        HalfX = 88.0; HalfY = 10.0; HalfZ = 88.0;
    }
    else if (std::strstr(Name, "Sugar") != nullptr)
    {
        HalfX = 78.0; HalfY = 58.0; HalfZ = 72.0;
    }
    else if (std::strstr(Name, "Milk") != nullptr)
    {
        HalfX = 66.0; HalfY = 72.0; HalfZ = 54.0;
    }
    else if (std::strstr(Name, "Floor") != nullptr)
    {
        HalfX = 1000.0 * Entry.Scale[0]; HalfY = 1.0; HalfZ = 1000.0 * Entry.Scale[2];
    }
    else
    {
        HalfX = 60.0; HalfY = 60.0; HalfZ = 60.0;
    }
}


void SeedSceneDirectoryTransformsFromCodex(const WorkspaceCodex& Scene,
                                           const SketchSceneDirectoryStorage& Storage,
                                           SceneDirectoryContext& Applied)
{
    if (Applied.TransformSeeded)
        return;
    for (std::uint32_t Row = 0u; Row < Storage.RowCount && Row < SceneDirectoryContext::EntityLimit; ++Row)
    {
        const StableRowIdentity Identity = Storage.Rows[Row].Identity;
        if (Identity < 6200u)
            continue;
        const std::uint32_t SceneIndex = Identity - 6200u;
        if (SceneIndex >= Scene.Scene.size())
            continue;
        const CodexSceneEntry& Entry = Scene.Scene[SceneIndex];
        for (std::uint32_t Axis = 0u; Axis < 3u; ++Axis)
        {
            Applied.EntityPosition[Row][Axis] = Entry.Position[Axis];
            Applied.EntityRotation[Row][Axis] = Entry.Rotation[Axis];
            Applied.EntityScale[Row][Axis] = Entry.Scale[Axis];
        }
    }
    Applied.TransformSeeded = true;
}

void SynchroniseCodexTransformsFromSceneDirectory(WorkspaceCodex& Scene,
                                                  const SketchSceneDirectoryStorage& Storage,
                                                  const SceneDirectoryContext& Applied,
                                                  bool SceneStanding)
{
    if (!SceneStanding)
        return;
    for (std::uint32_t Row = 0u; Row < Storage.RowCount && Row < SceneDirectoryContext::EntityLimit; ++Row)
    {
        const StableRowIdentity Identity = Storage.Rows[Row].Identity;
        if (Identity < 6200u)
            continue;
        const std::uint32_t SceneIndex = Identity - 6200u;
        if (SceneIndex >= Scene.Scene.size())
            continue;
        CodexSceneEntry& Entry = Scene.Scene[SceneIndex];
        for (std::uint32_t Axis = 0u; Axis < 3u; ++Axis)
        {
            Entry.Position[Axis] = Applied.EntityPosition[Row][Axis];
            Entry.Rotation[Axis] = Applied.EntityRotation[Row][Axis];
            Entry.Scale[Axis] = Applied.EntityScale[Row][Axis] == 0.0 ? 1.0 : Applied.EntityScale[Row][Axis];
        }
    }
}

bool ResolveSelectedSceneMeshPivot(const WorkspaceCodex& Scene,
                                   bool SceneStanding,
                                   const SketchSceneDirectoryStorage& Storage,
                                   const SceneDirectoryContext& Applied,
                                   SpatialPoint& Pivot)
{
    if (!SceneStanding || Applied.EntityTaken >= Storage.RowCount || Applied.EntityTaken >= SceneDirectoryContext::EntityLimit)
        return false;
    const StableRowIdentity Identity = Storage.Rows[Applied.EntityTaken].Identity;
    if (Identity < 6200u)
        return false;
    const std::uint32_t SceneIndex = Identity - 6200u;
    if (SceneIndex >= Scene.Scene.size() || Scene.Scene[SceneIndex].Subject != CodexSceneSubject::Geometry)
        return false;
    Pivot = CodexScenePosition(Scene.Scene[SceneIndex]);
    return true;
}

bool SelectSceneMeshAtPointer(const PlaneExtent& Extent,
                              const PointerCondition& Pointer,
                              const SpatialBasis& Basis,
                              const ParametricViewportState& View,
                              bool Perspective,
                              const WorkspaceCodex& Scene,
                              bool SceneStanding,
                              const SketchSceneDirectoryStorage& Storage,
                              SceneDirectoryContext& Applied)
{
    if (!SceneStanding || !Pointer.ContactPressed || !Extent.Encloses(Pointer.PositionX, Pointer.PositionY))
        return false;

    std::uint32_t BestRow = SceneDirectoryContext::EntityLimit;
    double BestArea = 1.0e300;
    for (std::uint32_t Row = 0u; Row < Storage.RowCount && Row < SceneDirectoryContext::EntityLimit; ++Row)
    {
        const StableRowIdentity Identity = Storage.Rows[Row].Identity;
        if (Identity < 6200u)
            continue;
        const std::uint32_t SceneIndex = Identity - 6200u;
        if (SceneIndex >= Scene.Scene.size())
            continue;
        const CodexSceneEntry& Entry = Scene.Scene[SceneIndex];
        const CodexSceneMesh* Mesh = nullptr;
        for (const CodexSceneMesh& Candidate : Scene.SceneMeshes)
            if (Candidate.Naming == Entry.GeometryReference)
            {
                Mesh = &Candidate;
                break;
            }
        if (Mesh == nullptr || Mesh->Positions.size() < 3u)
            continue;

        const SpatialPoint Centre = CodexScenePosition(Entry);
        float MinX = 1.0e30f, MinY = 1.0e30f, MaxX = -1.0e30f, MaxY = -1.0e30f;
        for (std::size_t Vertex = 0u; Vertex * 3u + 2u < Mesh->Positions.size(); ++Vertex)
        {
            float X = 0.0f, Y = 0.0f;
            if (!ProjectOffsetPoint(Basis, View, Perspective, Extent, Centre,
                Mesh->Positions[Vertex * 3u + 0u] * Entry.Scale[0] * CodexSceneMetreScale,
                Mesh->Positions[Vertex * 3u + 1u] * Entry.Scale[1] * CodexSceneMetreScale,
                Mesh->Positions[Vertex * 3u + 2u] * Entry.Scale[2] * CodexSceneMetreScale, X, Y))
                continue;
            MinX = std::min(MinX, X); MinY = std::min(MinY, Y);
            MaxX = std::max(MaxX, X); MaxY = std::max(MaxY, Y);
        }
        if (MinX > MaxX || MinY > MaxY)
            continue;
        const float Pad = 8.0f;
        if (Pointer.PositionX + Pad < MinX || Pointer.PositionX - Pad > MaxX ||
            Pointer.PositionY + Pad < MinY || Pointer.PositionY - Pad > MaxY)
            continue;
        const double Area = static_cast<double>(MaxX - MinX) * static_cast<double>(MaxY - MinY);
        if (Area < BestArea)
        {
            BestArea = Area;
            BestRow = Row;
        }
    }

    if (BestRow >= SceneDirectoryContext::EntityLimit)
        return false;
    for (bool& Selected : Applied.EntitySelected)
        Selected = false;
    Applied.EntitySelected[BestRow] = true;
    Applied.EntityTaken = BestRow;
    Applied.EntitySelectionAnchor = BestRow;
    return true;
}

ThemeToken CodexMaterialToken(const WorkspaceCodex& Scene,
                              const CodexSceneEntry& Entry,
                              double Alpha,
                              std::uint32_t Fallback)
{
    for (const WorkspaceMaterialRecord& Material : Scene.Materials)
    {
        if (Material.Reference != Entry.MaterialReference)
            continue;
        const ChannelSpecification& Albedo = Material.Material.Channel(ChannelSubject::AlbedoColour);
        if (!Albedo.ChannelDeclared || Albedo.Measured != ChannelMeasure::Reflectance ||
            Albedo.Source != ChannelSource::Constant || !Albedo.ConstantColour.ColourDeclared())
            break;

        const auto Byte = [](double Value) -> std::uint32_t
        {
            const double Clamped = std::max(0.0, std::min(1.0, Value));
            return static_cast<std::uint32_t>(Clamped * 255.0 + 0.5);
        };
        const std::uint32_t Packed = (Byte(Albedo.ConstantColour.RedCoordinate) << 16u)
                                   | (Byte(Albedo.ConstantColour.GreenCoordinate) << 8u)
                                   | Byte(Albedo.ConstantColour.BlueCoordinate);
        return Partial(Packed, Alpha);
    }
    return Partial(Fallback, Alpha);
}

void RecordCodexSceneProxy(RecordingSurface& Surface,
                           const PlaneExtent& Extent,
                           const SpatialBasis& Basis,
                           const ParametricViewportState& View,
                           bool Perspective,
                           const WorkspaceCodex& Scene,
                           bool SceneStanding,
                           const SketchSceneDirectoryStorage& Storage,
                           const SceneDirectoryContext& Applied)
{
    if (!SceneStanding)
        return;

    Surface.Confine(Extent);
    const ThemeToken FaceLit = Partial(0xFFFFFFu, 0.22);
    const ThemeToken Edge = Partial(0xE7E3D8u, 0.74);
    const ThemeToken SelectedEdge = Partial(0xFBBF24u, 0.95);
    const ThemeToken Floor = Partial(0xFFFFFFu, 0.08);

    for (std::uint32_t SceneIndex = 0u; SceneIndex < Scene.Scene.size(); ++SceneIndex)
    {
        const CodexSceneEntry& Entry = Scene.Scene[SceneIndex];
        if (Entry.Subject != CodexSceneSubject::Geometry)
            continue;

        const SpatialPoint Centre = CodexScenePosition(Entry);
        const ThemeToken Fill = CodexMaterialToken(Scene, Entry, 0.34, 0xF4F1E8u);
        const CodexSceneMesh* Mesh = nullptr;
        for (const CodexSceneMesh& Candidate : Scene.SceneMeshes)
            if (Candidate.Naming == Entry.GeometryReference)
            {
                Mesh = &Candidate;
                break;
            }
        if (Mesh != nullptr && Mesh->Positions.size() >= 9u && Mesh->Indices.size() >= 3u)
        {
            for (std::uint32_t Index = 0u; Index + 2u < Mesh->Indices.size(); Index += 3u)
            {
                float SX[3] = {};
                float SY[3] = {};
                bool TriangleStanding = true;
                for (std::uint32_t Corner = 0u; Corner < 3u; ++Corner)
                {
                    const std::uint32_t Vertex = Mesh->Indices[Index + Corner];
                    if (Vertex * 3u + 2u >= Mesh->Positions.size())
                    {
                        TriangleStanding = false;
                        break;
                    }
                    TriangleStanding = ProjectOffsetPoint(Basis, View, Perspective, Extent, Centre,
                        Mesh->Positions[Vertex * 3u + 0u] * Entry.Scale[0] * CodexSceneMetreScale,
                        Mesh->Positions[Vertex * 3u + 1u] * Entry.Scale[1] * CodexSceneMetreScale,
                        Mesh->Positions[Vertex * 3u + 2u] * Entry.Scale[2] * CodexSceneMetreScale,
                        SX[Corner], SY[Corner]) && TriangleStanding;
                }
                if (TriangleStanding)
                {
                    const float Corners[6] = { SX[0], SY[0], SX[1], SY[1], SX[2], SY[2] };
                    Surface.Tongue(Corners, 3u, std::strstr(Entry.Naming.c_str(), "Floor") != nullptr ? Floor : Fill);
                }
            }
        }

        double HalfX = 0.0, HalfY = 0.0, HalfZ = 0.0;
        ResolveCodexProxyExtent(Entry, HalfX, HalfY, HalfZ);

        float X[8] = {};
        float Y[8] = {};
        const double Signs[8][3] =
        {
            { -1.0, -1.0, -1.0 }, {  1.0, -1.0, -1.0 }, {  1.0, -1.0,  1.0 }, { -1.0, -1.0,  1.0 },
            { -1.0,  1.0, -1.0 }, {  1.0,  1.0, -1.0 }, {  1.0,  1.0,  1.0 }, { -1.0,  1.0,  1.0 }
        };
        bool Standing = true;
        for (std::uint32_t Index = 0u; Index < 8u; ++Index)
            Standing = ProjectOffsetPoint(Basis, View, Perspective, Extent, Centre,
                                              Signs[Index][0] * HalfX,
                                              Signs[Index][1] * HalfY,
                                              Signs[Index][2] * HalfZ,
                                              X[Index], Y[Index]) && Standing;
        if (!Standing)
            continue;

        const bool IsFloor = std::strstr(Entry.Naming.c_str(), "Floor") != nullptr;
        const bool Selected = [&]()
        {
            for (std::uint32_t Row = 0u; Row < Storage.RowCount && Row < SceneDirectoryContext::EntityLimit; ++Row)
                if (Storage.Rows[Row].Identity == 6200u + SceneIndex && Applied.EntitySelected[Row])
                    return true;
            return false;
        }();
        const auto Triangle = [&](std::uint32_t A, std::uint32_t B, std::uint32_t C, ThemeToken Colour)
        {
            const float Corners[6] = { X[A], Y[A], X[B], Y[B], X[C], Y[C] };
            Surface.Tongue(Corners, 3u, Colour);
        };
        const auto Line = [&](std::uint32_t A, std::uint32_t B)
        {
            const float PointsX[2] = { X[A], X[B] };
            const float PointsY[2] = { Y[A], Y[B] };
            Surface.Polyline(PointsX, PointsY, 2u, Selected ? SelectedEdge : Edge, Selected ? 1.8f : 1.1f);
        };

        if (IsFloor)
        {
            Triangle(0u, 1u, 2u, Floor);
            Triangle(0u, 2u, 3u, Floor);
        }
        else
        {
            Triangle(4u, 5u, 6u, FaceLit);
            Triangle(4u, 6u, 7u, FaceLit);
            Triangle(0u, 1u, 5u, Fill);
            Triangle(0u, 5u, 4u, Fill);
            Triangle(1u, 2u, 6u, Fill);
            Triangle(1u, 6u, 5u, Fill);
            Triangle(2u, 3u, 7u, Fill);
            Triangle(2u, 7u, 6u, Fill);
            Triangle(3u, 0u, 4u, Fill);
            Triangle(3u, 4u, 7u, Fill);
        }

        Line(0u, 1u); Line(1u, 2u); Line(2u, 3u); Line(3u, 0u);
        Line(4u, 5u); Line(5u, 6u); Line(6u, 7u); Line(7u, 4u);
        Line(0u, 4u); Line(1u, 5u); Line(2u, 6u); Line(3u, 7u);
    }
    Surface.Release();
}


using ParametricGizmoHandle = GizmoHandle;





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
                        const ParametricViewportState& View,
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
                               const ParametricViewportState& View,
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
                                 const ParametricViewportSelection& ActiveSelection,
                                 const ParametricViewportSelection& HoveredSelection,
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
                           const ParametricViewportSelection& ActiveSelection,
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
                                        const ParametricViewportState& View,
                                        bool Perspective,
                                        ParametricToolSubject ActiveTool,
                                        WorkspaceNameIndex& Naming,
                                        const WorkspaceDirectoryProjection& Directory,
                                        const ParametricWorkspaceContext& WorkspaceApplied,
                                        SketchStructure& Sketch,
                                        WorkspaceRecordStructure& Records,
                                        WorkspaceRevisionSequence& Revisions,
                                        WorkspaceRecordName& PendingSelection,
                                        ParametricViewportSelection& SemanticSelection,
                                        ParametricViewportSelection& HoveredSelection,
                                        ParametricTransformState& Transform,
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

    const ParametricViewportSelection ActiveSelection =
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

    ParametricGizmoHandle HoveredHandle = ParametricGizmoHandle::None;
    if (!Transform.Engaged() && ActiveSelection.Standing() && SelectedTool(ActiveTool).Subject == SketchSubject::None)
    {
        GizmoScreenBasis Screen = {};
        if (ResolveGizmoScreenBasis(Basis, View, Perspective, Extent, ActiveSelection.Position, Screen))
            HoveredHandle = ResolveGizmoHandle(Screen, Transform.Manner(), Pointer.PositionX, Pointer.PositionY);
    }

    if (!PointerTaken && !Transform.Engaged() && SelectedTool(ActiveTool).Subject == SketchSubject::None &&
        Pointer.ContactPressed && HoveredHandle == ParametricGizmoHandle::None && HoveredSelection.Standing())
    {
        SemanticSelection = HoveredSelection;
        PendingSelection = HoveredSelection.Record;
        PointerTaken = true;
    }

    const ParametricTransformCommandInput Command =
        ResolveTransformCommand(TextInput.Intake, TextInput.IntakeCount, Transform.Engaged(), Transform.Manner());

    if (!Transform.Engaged() && ActiveSelection.Standing() && Extent.Encloses(Pointer.PositionX, Pointer.PositionY))
    {
        if (HoveredHandle != ParametricGizmoHandle::None && Pointer.ContactPressed)
        {
            // 📝 A handle names both what it does and what it restricts, so the two are read from it
            //    rather than reconstructed by a switch at the call site.
            const ParametricTransformMode Mode = ResolveHandleManner(HoveredHandle);
            const ParametricTransformConstraint Constraint = ResolveHandleRestriction(HoveredHandle);
            const bool Slide = false;

            PointerTaken = StartTransformSession(Sketch, Records, Basis, View, Perspective, Extent,
                                                 Pointer.PositionX, Pointer.PositionY, ActiveSelection,
                                                 Mode, Constraint, Slide, true, Transform);
        }
        else if (Command.StartRequested)
        {
            const bool Slide = Command.StartManner == ParametricTransformMode::Move
                            && ResolveSlideRequested(Command.MoveTapCount,
                                                         SessionMilliseconds,
                                                         LastGPressedMilliseconds,
                                                         ActiveSelection.Curve.Assigned());
            if (Command.MoveTapCount > 0u)
                LastGPressedMilliseconds = SessionMilliseconds;
            PointerTaken = StartTransformSession(Sketch, Records, Basis, View, Perspective, Extent,
                                                 Pointer.PositionX, Pointer.PositionY, ActiveSelection,
                                                 Command.StartManner,
                                                 Slide ? ParametricTransformConstraint::Curve
                                                       : (Command.StartManner == ParametricTransformMode::Rotate
                                                            ? ParametricTransformConstraint::Screen
                                                            : ParametricTransformConstraint::Free),
                                                 Slide, false, Transform);
        }
    }

    if (Transform.Engaged())
    {
        const bool SlideRequested = Transform.Manner() == ParametricTransformMode::Move
                                 && ResolveSlideRequested(Command.MoveTapCount,
                                                              SessionMilliseconds,
                                                              LastGPressedMilliseconds,
                                                              Transform.Target.Curve.Assigned());
        if (Transform.Manner() == ParametricTransformMode::Move && Command.MoveTapCount > 0u)
            LastGPressedMilliseconds = SessionMilliseconds;

        if (SlideRequested)
        {
            Transform.Restriction() = ParametricTransformConstraint::Curve;
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

} // namespace

void ClearInspectorBridge(ParametricWorkspaceBridgeStorage& Bridge)
{
    Bridge.Property = ParametricPropertyPresentation{};
    Bridge.PropertyNaming.clear();
    Bridge.PropertySecondary.clear();
    for (std::string& Run : Bridge.PropertyCaptions) Run.clear();
    for (std::string& Run : Bridge.PropertyValues)   Run.clear();
    for (std::string& Run : Bridge.PropertyTrails)   Run.clear();
    Bridge.RevisionRows.clear();
    Bridge.RevisionBacking.clear();
}

void SeedParametricWorkspace(WorkspaceNameIndex& Naming,
                             SketchStructure& Sketch,
                             WorkspaceRecordStructure& Records,
                             WorkspaceRevisionSequence& Revisions)
{
    // Parametric sketch starts empty. The default grid/basis is supplied by
    // ResolveSketchBasis until the artist creates real CAD records.
    static_cast<void>(Naming);
    static_cast<void>(Sketch);
    static_cast<void>(Records);
    static_cast<void>(Revisions);
}




void SeatParametricContext(const WorkspaceDirectoryProjection& Directory,
                           ParametricWorkspaceContext& Applied,
                           bool& Seeded)
{
    const std::uint32_t RowCount = std::min<std::uint32_t>(
        static_cast<std::uint32_t>(Directory.Rows.size()), ParametricWorkspaceContext::RowLimit);

    for (std::uint32_t Index = RowCount; Index < ParametricWorkspaceContext::RowLimit; ++Index)
    {
        Applied.RowExpanded[Index] = false;
        Applied.RowSelected[Index] = false;
    }

    if (RowCount == 0u)
    {
        Applied.RowTaken = 0u;
        Applied.RowSelectionAnchor = 0u;
        return;
    }

    if (!Seeded)
    {
        for (std::uint32_t Index = 0u; Index < ParametricWorkspaceContext::RowLimit; ++Index)
            Applied.RowSelected[Index] = false;

        for (std::uint32_t Index = 0u; Index < RowCount; ++Index)
            if (Directory.Rows[Index].Subject == WorkspaceRecordSubject::Folder)
                Applied.RowExpanded[Index] = true;

        const std::uint32_t Initial = InitialRowIn(Directory);
        Applied.RowTaken = Initial;
        Applied.RowSelectionAnchor = Initial;
        Applied.RowSelected[Initial] = true;
        Seeded = true;
        return;
    }

    if (Applied.RowTaken >= RowCount || !AnyRowSelected(Applied, RowCount))
    {
        for (std::uint32_t Index = 0u; Index < RowCount; ++Index)
            Applied.RowSelected[Index] = false;

        const std::uint32_t Initial = InitialRowIn(Directory);
        Applied.RowTaken = Initial;
        Applied.RowSelectionAnchor = Initial;
        Applied.RowSelected[Initial] = true;
    }
}

Deliver<bool> SynchroniseParametricPresentation(const WorkspaceRecordStructure& Records,
                                                const WorkspaceRevisionSequence& Revisions,
                                                WorkspaceDirectoryProjection& Directory,
                                                ParametricWorkspaceBridgeStorage& Bridge,
                                                ParametricWorkspaceContext& Applied,
                                                WorkspaceRecordName& PendingSelection,
                                                bool& Seeded)
{
    ProjectWorkspaceDirectory(Records, Directory);

    const Deliver<bool> DirectoryBridge = BridgeParametricDirectory(Directory, Bridge);
    if (!DirectoryBridge.Resolved)
        return DirectoryBridge;

    SeatParametricContext(Directory, Applied, Seeded);

    if (PendingSelection.Assigned())
    {
        const Deliver<std::uint32_t> Row = ResolveWorkspaceDirectoryRow(Directory, PendingSelection);
        if (Row.Resolved && Row.Resolve() < ParametricWorkspaceContext::RowLimit)
        {
            for (std::uint32_t Index = 0u; Index < ParametricWorkspaceContext::RowLimit; ++Index)
                Applied.RowSelected[Index] = false;
            Applied.RowTaken = Row.Resolve();
            Applied.RowSelectionAnchor = Row.Resolve();
            Applied.RowSelected[Row.Resolve()] = true;
        }
        PendingSelection = {};
    }

    const std::uint32_t RowCount = static_cast<std::uint32_t>(Directory.Rows.size());
    if (Applied.RowTaken >= RowCount || Directory.Rows.empty() ||
        Directory.Rows[Applied.RowTaken].Role != WorkspaceDirectoryRowRole::Record)
    {
        ClearInspectorBridge(Bridge);
        return Deliver<bool>::Result(true);
    }

    const WorkspaceRecordName Selected = Directory.Rows[Applied.RowTaken].Record;
    const Deliver<WorkspacePropertyProjection> Property =
        ProjectWorkspaceProperty(Records, Revisions, Selected);
    if (!Property.Resolved)
        return Deliver<bool>::Refuse(Property.Error);

    return BridgeParametricInspector(Records, Selected, Property.Resolve(), Revisions, Bridge);
}

Deliver<bool> SynchroniseCadPacket(const SketchStructure& Sketch,
                                   const WorkspaceRecordStructure& Records,
                                   WorkspaceCadPacket& Delivered)
{
    return ProjectSketchRendering(Sketch, Records, Delivered, {});
}


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
#ifdef SLATE_DEBUG
    Declared.DiagnosticRequested = true;
#endif

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
    SketchSceneDirectoryStorage SceneDirectoryStorage;
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
    ParametricWorkspaceBridgeStorage Bridge;
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

        const Deliver<bool> PacketProjected = SynchroniseCadPacket(Sketch, Records, CadPacket);
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
            BridgeSketchSceneDirectory(OpenedScene, SceneDirectoryStorage);
        else
            SceneDirectoryStorage = SketchSceneDirectoryStorage{};
        AppendSketchCadReferences(Records, SceneDirectoryStorage);
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

                            if (!Transform.Engaged())
                                DriveDrawingWithModifiers(LeafBody, BackgroundPointer,
                                                         Viewport.Surface().TextInput(), Modifiers,
                                                         Basis, View,
                                                         PanelConfiguration[Index].Perspective,
                                                         ToolsApplied,
                                                         Naming, Sketch, Records, Revisions, Workplanes,
                                                         PendingSelection, Tool, PointerTaken);

                            if (!PointerTaken && PointerInside && ToolsApplied.ActiveSubject == ParametricToolSubject::Select)
                                PointerTaken = SelectSceneMeshAtPointer(LeafBody, BackgroundPointer, Basis, View,
                                                                        PanelConfiguration[Index].Perspective,
                                                                        OpenedScene, OpenedSceneStanding,
                                                                        SceneDirectoryStorage, SceneApplied);
                            RecordCodexSceneProxy(Viewport.Surface(), LeafBody, Basis, View,
                                                  PanelConfiguration[Index].Perspective,
                                                  OpenedScene, OpenedSceneStanding,
                                                  SceneDirectoryStorage, SceneApplied);

                            Discard(SynchroniseCadPacket(Sketch, Records, CadPacket));
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
                                                                  SceneDirectoryStorage, SceneApplied, ScenePivot))
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

            const SharedCodexActivation ActivatedScene = ConsumeSharedCodexActivation(
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
                ApplySketchSceneEnvironment(OpenedScene, SceneApplied);

                SpatialPoint Focus = {};
                std::uint32_t GeometryCount = 0u;
                for (const CodexSceneEntry& Entry : OpenedScene.Scene)
                {
                    if (Entry.Subject != CodexSceneSubject::Geometry)
                        continue;
                    const SpatialPoint Position = CodexScenePosition(Entry);
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
