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

constexpr ThemeToken Faded(ThemeToken Declared, float Fraction)
{
    const float Held = Fraction < 0.0f ? 0.0f : (Fraction > 1.0f ? 1.0f : Fraction);
    Declared.Opacity = static_cast<std::uint8_t>(static_cast<float>(Declared.Opacity) * Held + 0.5f);
    return Declared;
}


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


void RecordViewportOrientationHud(RecordingSurface& Surface,
                                  const PlaneExtent& Extent,
                                  const PointerCondition& Pointer,
                                  ParametricViewportState& View,
                                  EditorPanelConfiguration& Configuration,
                                  bool& PointerTaken)
{
    bool& Perspective = Configuration.Perspective;
    const SpatialBasis SketchBasis = { {}, { 1.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0 }, { 0.0, 1.0, 0.0 } };
    const ViewFrame Frame = ResolveViewportFrame(SketchBasis, View, Perspective);
    SharedViewportBasis GizmoBasis;
    GizmoBasis.Right[0] = Frame.Right.Left;
    GizmoBasis.Right[1] = Frame.Right.Up;
    GizmoBasis.Right[2] = Frame.Right.Forward;
    GizmoBasis.Up[0] = Frame.Up.Left;
    GizmoBasis.Up[1] = Frame.Up.Up;
    GizmoBasis.Up[2] = Frame.Up.Forward;
    GizmoBasis.Forward[0] = Frame.Forward.Left;
    GizmoBasis.Forward[1] = Frame.Forward.Up;
    GizmoBasis.Forward[2] = Frame.Forward.Forward;

    if (Pointer.ContactPressed)
    {
        const SharedViewportOrientation Hit = HitSharedViewportGizmo(
            Extent, GizmoBasis, Pointer.PositionX, Pointer.PositionY,
            Configuration.Gizmo == PanelGizmo::Cad);
        if (Hit != SharedViewportOrientation::None)
        {
            switch (Hit)
            {
                case SharedViewportOrientation::Top:    ApplyViewportOrientation(View, ParametricViewOrientation::Top, false); break;
                case SharedViewportOrientation::Bottom: ApplyViewportOrientation(View, ParametricViewOrientation::Bottom, false); break;
                case SharedViewportOrientation::Front:  ApplyViewportOrientation(View, ParametricViewOrientation::Front, false); break;
                case SharedViewportOrientation::Back:   ApplyViewportOrientation(View, ParametricViewOrientation::Back, false); break;
                case SharedViewportOrientation::Right:  ApplyViewportOrientation(View, ParametricViewOrientation::Right, false); break;
                case SharedViewportOrientation::Left:   ApplyViewportOrientation(View, ParametricViewOrientation::Left, false); break;
                case SharedViewportOrientation::Iso:    ApplyViewportOrientation(View, ParametricViewOrientation::Isometric, true); break;
                case SharedViewportOrientation::None: break;
            }
            Perspective = Hit == SharedViewportOrientation::Iso ? true : false;
            PointerTaken = true;
        }
    }

    RecordSharedViewportGizmo(Surface, Extent, GizmoBasis, Configuration.Gizmo == PanelGizmo::Cad);
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

void RecordCadFallback(RecordingSurface& Surface,
                       const PlaneExtent& Extent,
                       const SketchStructure& Sketch,
                       const ParametricViewportState& View,
                       bool Perspective,
                       const WorkspaceCadPacket& Packet)
{
    if (!Packet.ExtentStanding || !Sketch.Declared())
        return;

    const SpatialBasis Basis = ResolveSketchBasis(Sketch);
    Surface.Confine(Extent);

    for (std::uint32_t Index = 0u; Index < Packet.FillCount; ++Index)
    {
        const WorkspaceCadFillTriangle& Fill = Packet.Fills[Index];
        float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f, X2 = 0.0f, Y2 = 0.0f;
        if (!ProjectViewportPoint(Basis, View, Perspective, Extent, Fill.Along0, Fill.Across0, X0, Y0) ||
            !ProjectViewportPoint(Basis, View, Perspective, Extent, Fill.Along1, Fill.Across1, X1, Y1) ||
            !ProjectViewportPoint(Basis, View, Perspective, Extent, Fill.Along2, Fill.Across2, X2, Y2))
            continue;
        const float Corners[6] = { X0, Y0, X1, Y1, X2, Y2 };
        Surface.Tongue(Corners, 3u, ThemeToken{
            static_cast<std::uint8_t>((Fill.Packed >> 16u) & 0xFFu),
            static_cast<std::uint8_t>((Fill.Packed >> 8u) & 0xFFu),
            static_cast<std::uint8_t>((Fill.Packed >> 0u) & 0xFFu),
            static_cast<std::uint8_t>((Fill.Packed >> 24u) & 0xFFu) });
    }

    for (std::uint32_t Index = 0u; Index < Packet.SegmentCount; ++Index)
    {
        const WorkspaceCadSegment& Segment = Packet.Segments[Index];
        float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f;
        if (!ProjectViewportPoint(Basis, View, Perspective, Extent, Segment.Along0, Segment.Across0, X0, Y0) ||
            !ProjectViewportPoint(Basis, View, Perspective, Extent, Segment.Along1, Segment.Across1, X1, Y1))
            continue;
        const float PointsX[2] = { X0, X1 };
        const float PointsY[2] = { Y0, Y1 };
        Surface.Polyline(PointsX, PointsY, 2u,
            ThemeToken{ static_cast<std::uint8_t>((Segment.Packed >> 16u) & 0xFFu),
                        static_cast<std::uint8_t>((Segment.Packed >> 8u) & 0xFFu),
                        static_cast<std::uint8_t>((Segment.Packed >> 0u) & 0xFFu),
                        static_cast<std::uint8_t>((Segment.Packed >> 24u) & 0xFFu) },
            Segment.Thickness);
    }

    for (std::uint32_t Index = 0u; Index < Packet.MarkerCount; ++Index)
    {
        const WorkspaceCadMarker& Marker = Packet.Markers[Index];
        float X = 0.0f, Y = 0.0f;
        if (!ProjectViewportPoint(Basis, View, Perspective, Extent, Marker.Along, Marker.Across, X, Y))
            continue;
        Surface.Medallion(X, Y, Marker.Radius,
            ThemeToken{ static_cast<std::uint8_t>((Marker.Packed >> 16u) & 0xFFu),
                        static_cast<std::uint8_t>((Marker.Packed >> 8u) & 0xFFu),
                        static_cast<std::uint8_t>((Marker.Packed >> 0u) & 0xFFu),
                        static_cast<std::uint8_t>((Marker.Packed >> 24u) & 0xFFu) });
    }

    Surface.Release();
}

void RecordViewportStateReadout(RecordingSurface& Surface,
                                const PlaneExtent& Extent,
                                const ParametricViewportState& View,
                                bool Perspective,
                                const WorkspaceCadPacket& Packet)
{
    char Detail[192] = {};
    std::snprintf(Detail, sizeof(Detail),
                  "%s • %s • %u segments • %u fills • %u markers",
                  Perspective ? "Perspective" : "Orthographic",
                  OrientationText(View.Orientation),
                  static_cast<unsigned>(Packet.SegmentCount),
                  static_cast<unsigned>(Packet.FillCount),
                  static_cast<unsigned>(Packet.MarkerCount));
    Surface.TextRun(Extent.MinimumX + 16.0f,
                    Extent.MaximumY - 24.0f,
                    Faded(Covering(0xE5E7EBu), 0.75f),
                    Detail, 11.0f);
}

const char* ConstraintGlyphText(ConstraintSubject Subject)
{
    switch (Subject)
    {
        case ConstraintSubject::Coincident:     return "●";
        case ConstraintSubject::Horizontal:     return "H";
        case ConstraintSubject::Vertical:       return "V";
        case ConstraintSubject::Parallel:       return "∥";
        case ConstraintSubject::Perpendicular:  return "⊥";
        case ConstraintSubject::Tangent:        return "T";
        case ConstraintSubject::Equal:          return "=";
        case ConstraintSubject::Fixed:          return "F";
        case ConstraintSubject::SubjectCount:   return "?";
    }
    return "?";
}

bool ResolveConstraintGlyphAnchor(const SketchStructure& Sketch,
                                  const ReferenceSpecification& Reference,
                                  SpatialPoint& Anchor)
{
    if (Reference.Subject == ReferenceSubject::SketchCurve && Reference.SketchCurve.Assigned() &&
        Reference.SketchCurve.IssuedIndex <= Sketch.Curves().size())
    {
        std::vector<SpatialPoint> Polyline;
        AppendCurvePolyline(Sketch.Curves()[Reference.SketchCurve.IssuedIndex - 1u].Geometry, Polyline, 24u);
        if (Polyline.empty())
            return false;
        Anchor = Polyline[Polyline.size() / 2u];
        return true;
    }
    if (Reference.Subject == ReferenceSubject::SketchPoint && Reference.SketchPoint.Assigned())
    {
        const std::uint32_t CurveIndex = Reference.SketchPoint.IssuedIndex >> 8u;
        std::vector<SketchPointPlacement> Points;
        if (CurveIndex != 0u && ResolveSketchPoints(Sketch, { CurveIndex }, Points))
            for (const SketchPointPlacement& Point : Points)
                if (Point.Name.IssuedIndex == Reference.SketchPoint.IssuedIndex)
                {
                    Anchor = Point.Position;
                    return true;
                }
    }
    return false;
}

void RecordConstraintGlyphs(RecordingSurface& Surface,
                            const PlaneExtent& Extent,
                            const SketchStructure& Sketch,
                            const ParametricViewportState& View,
                            bool Perspective)
{
    if (!Sketch.Declared())
        return;
    const SpatialBasis Basis = ResolveSketchBasis(Sketch);
    for (const ConstraintSpecification& Constraint : Sketch.Constraints())
    {
        SpatialPoint Anchor = {};
        if (!ResolveConstraintGlyphAnchor(Sketch, Constraint.Primary, Anchor))
            continue;
        float X = 0.0f, Y = 0.0f;
        const SpatialDirection Offset = Difference(Basis.Origin, Anchor);
        const double Along = Dot(Offset, Basis.Along);
        const double Across = Dot(Offset, Basis.Across);
        if (!ProjectViewportPoint(Basis, View, Perspective, Extent, Along, Across, X, Y))
            continue;
        Surface.TextRun(X + 6.0f, Y - 6.0f, Covering(0xFBBF24u),
                        ConstraintGlyphText(Constraint.Subject), 12.0f, 0.0f, true);
    }
}

const char* ConstraintDispositionText(ConstraintDisposition Disposition)
{
    switch (Disposition)
    {
        case ConstraintDisposition::Produced:              return "valid";
        case ConstraintDisposition::InvalidSketch:         return "invalid sketch references";
        case ConstraintDisposition::UnsupportedConstraint: return "unsupported constraint";
        case ConstraintDisposition::ConflictingConstraint: return "conflicting constraint";
    }
    return "unknown";
}

bool ProjectAreaPoint(const SpatialBasis& Basis,
                        const ParametricViewportState& View,
                        bool Perspective,
                        const PlaneExtent& Extent,
                        const SpatialPoint& Position,
                        float& X,
                        float& Y)
{
    const SpatialDirection Offset = Difference(Basis.Origin, Position);
    return ProjectViewportPoint(Basis, View, Perspective, Extent,
                                Dot(Offset, Basis.Along), Dot(Offset, Basis.Across), X, Y);
}

void RecordProfileAreaOverlay(RecordingSurface& Surface,
                                const PlaneExtent& Extent,
                                const SketchStructure& Sketch,
                                const ParametricViewportState& View,
                                bool Perspective)
{
    if (!Sketch.Declared())
        return;
    const SpatialBasis Basis = ResolveSketchBasis(Sketch);
    const ProfileAreaAnalysis Analysis = AnalyzeProfileAreas(Sketch, 0.05);

    for (const ProfileAreaTriangle& Triangle : Analysis.Triangles)
    {
        float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f, X2 = 0.0f, Y2 = 0.0f;
        if (!ProjectAreaPoint(Basis, View, Perspective, Extent, Triangle.A, X0, Y0) ||
            !ProjectAreaPoint(Basis, View, Perspective, Extent, Triangle.B, X1, Y1) ||
            !ProjectAreaPoint(Basis, View, Perspective, Extent, Triangle.C, X2, Y2))
            continue;
        const float Corners[6] = { X0, Y0, X1, Y1, X2, Y2 };
        Surface.Tongue(Corners, 3u, Partial(0x5B8CFFu, Triangle.Role == ProfileAreaLoopRole::Outer ? 0.12 : 0.04));
    }

    for (const ProfileAreaLoop& Loop : Analysis.Loops)
    {
        const ThemeToken Tone = Loop.SelfIntersecting ? Covering(0xF97316u)
                              : Loop.Role == ProfileAreaLoopRole::Hole ? Covering(0xA78BFAu)
                                                                         : Faded(Covering(0x34D399u), 0.82f);
        for (std::size_t Index = 0u; Index + 1u < Loop.Points.size(); ++Index)
        {
            float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f;
            if (ProjectAreaPoint(Basis, View, Perspective, Extent, Loop.Points[Index], X0, Y0) &&
                ProjectAreaPoint(Basis, View, Perspective, Extent, Loop.Points[Index + 1u], X1, Y1))
            {
                const float Xs[2] = { X0, X1 };
                const float Ys[2] = { Y0, Y1 };
                Surface.Polyline(Xs, Ys, 2u, Tone, Loop.Role == ProfileAreaLoopRole::Hole ? 1.3f : 1.8f);
            }
        }
        if (!Loop.Points.empty())
        {
            float X = 0.0f, Y = 0.0f;
            if (ProjectAreaPoint(Basis, View, Perspective, Extent, Loop.Points[Loop.Points.size() / 2u], X, Y))
                Surface.TextRun(X + 8.0f, Y + 8.0f,
                                Loop.Role == ProfileAreaLoopRole::Hole ? Covering(0xC4B5FDu) : Covering(0xA7F3D0u),
                                Loop.Role == ProfileAreaLoopRole::Hole ? "hole" : "outer", 10.0f);
        }
    }

    for (const ProfileAreaIssue& Issue : Analysis.Issues)
    {
        float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f;
        if (!ProjectAreaPoint(Basis, View, Perspective, Extent, Issue.Primary, X0, Y0))
            continue;
        const ThemeToken Tone = Issue.Subject == ProfileAreaIssueSubject::SelfIntersection ? Covering(0xF97316u)
                                                                                              : Covering(0xEF4444u);
        if (Issue.Subject == ProfileAreaIssueSubject::Gap &&
            ProjectAreaPoint(Basis, View, Perspective, Extent, Issue.Secondary, X1, Y1))
        {
            const float Xs[2] = { X0, X1 };
            const float Ys[2] = { Y0, Y1 };
            Surface.Polyline(Xs, Ys, 2u, Tone, 2.4f);
        }
        Surface.Medallion(X0, Y0, Issue.Subject == ProfileAreaIssueSubject::SelfIntersection ? 5.5f : 4.5f, Tone);
        Surface.TextRun(X0 + 8.0f, Y0 - 8.0f, Tone,
                        Issue.Subject == ProfileAreaIssueSubject::SelfIntersection ? "self-intersection" : "profile gap", 10.0f);
    }
}

void RecordProfileValidationReadout(RecordingSurface& Surface,
                                    const PlaneExtent& Extent,
                                    const SketchStructure& Sketch)
{
    std::uint32_t ValidProfiles = 0u;
    for (const ProfileSpecification& Profile : Sketch.Profiles())
        if (Profile.Declared())
            ++ValidProfiles;

    const ConstraintDisposition Constraints = EvaluateConstraints(Sketch);
    const bool ConstraintWarning = Constraints == ConstraintDisposition::InvalidSketch
                                || Constraints == ConstraintDisposition::UnsupportedConstraint
                                || Constraints == ConstraintDisposition::ConflictingConstraint;
    const ProfileAreaAnalysis Areas = AnalyzeProfileAreas(Sketch, 0.05);
    char Detail[224] = {};
    std::snprintf(Detail, sizeof(Detail), "%u/%u profiles valid • %u areas/%u issues • %u constraints: %s • %s/%s",
                  static_cast<unsigned>(ValidProfiles),
                  static_cast<unsigned>(Sketch.Profiles().size()),
                  static_cast<unsigned>(Areas.Loops.size()),
                  static_cast<unsigned>(Areas.Issues.size()),
                  static_cast<unsigned>(Sketch.Constraints().size()),
                  ConstraintDispositionText(Constraints),
                  Areas.Clipper2BackendAvailable ? "clipper2" : "poly fallback",
                  Areas.EarcutBackendAvailable ? "earcut" : "fan preview");
    Surface.TextRun(Extent.MinimumX + 16.0f,
                    Extent.MaximumY - 42.0f,
                    ConstraintWarning ? Covering(0xFBBF24u) : Faded(Covering(0xA7F3D0u), 0.85f),
                    Detail, 11.0f);
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


ReferenceSpecification ReferenceFromPoint(SketchPointName Point)
{
    ReferenceSpecification Reference = {};
    Reference.Subject = ReferenceSubject::SketchPoint;
    Reference.SketchPoint = Point;
    return Reference;
}

ReferenceSpecification ReferenceFromCurve(SketchCurveName Curve)
{
    ReferenceSpecification Reference = {};
    Reference.Subject = ReferenceSubject::SketchCurve;
    Reference.SketchCurve = Curve;
    return Reference;
}



SpatialPoint ResolvePlanePosition(const SpatialBasis& Basis, double Along, double Across)
{
    return ResolvePlanarPoint(Basis, Along, Across);
}

SketchSnapPlacement ResolveGridSnap(const SpatialBasis& Basis,
                                    const SpatialPoint& Probe,
                                    double Step,
                                    double MaximumDistance)
{
    double Along = 0.0;
    double Across = 0.0;
    ResolvePlaneCoordinates(Basis, Probe, Along, Across);
    const double SafeStep = std::max(Step, 1.0);
    const double SnappedAlong = std::round(Along / SafeStep) * SafeStep;
    const double SnappedAcross = std::round(Across / SafeStep) * SafeStep;
    const SpatialPoint Snapped = ResolvePlanePosition(Basis, SnappedAlong, SnappedAcross);
    const double Distance = std::sqrt(LengthSquared(Difference(Probe, Snapped)));
    if (Distance > MaximumDistance)
        return {};
    SketchSnapPlacement Placement = {};
    Placement.Subject = SketchSnapSubject::Grid;
    Placement.Position = Snapped;
    Placement.Distance = Distance;
    return Placement;
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
        return ResolvePlanePosition(Basis,
                                    AnchorAlong + std::cos(Angle) * Distance,
                                    AnchorAcross + std::sin(Angle) * Distance);
    }

    if (Tool.Subject() == SketchSubject::Rectangle && Settings.RectangleDimensionAssist)
    {
        const double SignAlong = DeltaAlong < 0.0 ? -1.0 : 1.0;
        const double SignAcross = DeltaAcross < 0.0 ? -1.0 : 1.0;
        return ResolvePlanePosition(Basis,
                                    AnchorAlong + SignAlong * std::max(Settings.RectangleWidth, 0.0),
                                    AnchorAcross + SignAcross * std::max(Settings.RectangleHeight, 0.0));
    }

    if (Tool.Subject() == SketchSubject::Circle && Settings.CircleRadiusAssist)
    {
        const double Radius = std::max(Settings.CircleRadius, 0.0);
        double Angle = Length > 1.0e-6 ? std::atan2(DeltaAcross, DeltaAlong) : 0.0;
        return ResolvePlanePosition(Basis,
                                    AnchorAlong + std::cos(Angle) * Radius,
                                    AnchorAcross + std::sin(Angle) * Radius);
    }

    return Hover;
}


ThemeToken SnapToneFor(SketchSnapSubject Subject)
{
    switch (Subject)
    {
        case SketchSnapSubject::Endpoint:      return Covering(0xFBBF24u);
        case SketchSnapSubject::Midpoint:      return Covering(0x34D399u);
        case SketchSnapSubject::Centre:        return Covering(0x60A5FAu);
        case SketchSnapSubject::Control:       return Covering(0xA78BFAu);
        case SketchSnapSubject::AlongCurve:    return Covering(0xF472B6u);
        case SketchSnapSubject::Intersection:  return Covering(0xF97316u);
        case SketchSnapSubject::Grid:          return Covering(0xE5E7EBu);
        case SketchSnapSubject::Perpendicular: return Covering(0x22D3EEu);
        case SketchSnapSubject::Tangent:       return Covering(0xFACC15u);
        case SketchSnapSubject::None:
        case SketchSnapSubject::SubjectCount:  return Covering(0x5B8CFFu);
    }
    return Covering(0x5B8CFFu);
}

void RecordPlacementPreview(RecordingSurface& Surface,
                        const PlaneExtent& Extent,
                        const SketchStructure& Sketch,
                        const ParametricViewportState& View,
                        bool Perspective,
                        const SketchPlacement& Tool)
{
    if (Tool.Subject() == SketchSubject::None || !Tool.HoverStanding() || !Sketch.Declared())
        return;

    const SpatialBasis Basis = ResolveSketchBasis(Sketch);
    const auto Projected = [&](const SpatialPoint& Position, float& X, float& Y) -> bool
    {
        const SpatialDirection Offset = Difference(Basis.Origin, Position);
        const double Along = Dot(Offset, Basis.Along);
        const double Across = Dot(Offset, Basis.Across);
        return ProjectViewportPoint(Basis, View, Perspective, Extent, Along, Across, X, Y);
    };

    const ThemeToken Preview = Covering(0x5B8CFFu);
    const ThemeToken SnapTone = Tool.HoverPlacement().Resolved() ? SnapToneFor(Tool.HoverPlacement().Subject) : Preview;

    if ((Tool.Subject() == SketchSubject::Line || Tool.Subject() == SketchSubject::Dimension) &&
        Tool.Anchors().size() == 1u)
    {
        float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f;
        if (Projected(Tool.Anchors()[0], X0, Y0) && Projected(Tool.HoverPosition(), X1, Y1))
        {
            const float PointsX[2] = { X0, X1 };
            const float PointsY[2] = { Y0, Y1 };
            Surface.Polyline(PointsX, PointsY, 2u, Preview, Tool.Subject() == SketchSubject::Dimension ? 1.2f : 1.8f);
        }
    }
    else if (Tool.Subject() == SketchSubject::Polyline && !Tool.Anchors().empty())
    {
        float PointsX[128] = {};
        float PointsY[128] = {};
        std::uint32_t Count = 0u;
        for (const SpatialPoint& Anchor : Tool.Anchors())
        {
            if (Count >= 127u)
                break;
            if (Projected(Anchor, PointsX[Count], PointsY[Count]))
                ++Count;
        }
        if (Count < 127u && Projected(Tool.HoverPosition(), PointsX[Count], PointsY[Count]))
            ++Count;
        if (Count >= 2u)
            Surface.Polyline(PointsX, PointsY, Count, Preview, 1.8f);
    }
    else if (Tool.Subject() == SketchSubject::Arc)
    {
        if (Tool.Anchors().size() == 1u)
        {
            float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f;
            if (Projected(Tool.Anchors()[0], X0, Y0) && Projected(Tool.HoverPosition(), X1, Y1))
            {
                const float PointsX[2] = { X0, X1 };
                const float PointsY[2] = { Y0, Y1 };
                Surface.Polyline(PointsX, PointsY, 2u, Preview, 1.4f);
            }
        }
        else if (Tool.Anchors().size() == 2u && ArcReady(Tool.Anchors()[0], Tool.Anchors()[1], Tool.HoverPosition()))
        {
            const CurveSpecification PreviewArc = CurveSpecification::DeclareThreePointArc(Tool.Anchors()[0], Tool.Anchors()[1], Tool.HoverPosition());
            std::vector<SpatialPoint> ArcPoints;
            AppendCurvePolyline(PreviewArc, ArcPoints, 48u);
            float PointsX[64] = {};
            float PointsY[64] = {};
            std::uint32_t Count = 0u;
            for (const SpatialPoint& Position : ArcPoints)
                if (Count < 64u && Projected(Position, PointsX[Count], PointsY[Count]))
                    ++Count;
            if (Count >= 2u)
                Surface.Polyline(PointsX, PointsY, Count, Preview, 1.8f);
        }
    }
    else if (Tool.Subject() == SketchSubject::Bezier && !Tool.Anchors().empty())
    {
        std::vector<SpatialPoint> Controls = Tool.Anchors();
        Controls.push_back(Tool.HoverPosition());
        const CurveSpecification PreviewBezier = CurveSpecification::DeclareBezier(Controls, { 0.0, 1.0 });
        std::vector<SpatialPoint> BezierPoints;
        AppendCurvePolyline(PreviewBezier, BezierPoints, 48u);
        float PointsX[64] = {};
        float PointsY[64] = {};
        std::uint32_t Count = 0u;
        for (const SpatialPoint& Position : BezierPoints)
            if (Count < 64u && Projected(Position, PointsX[Count], PointsY[Count]))
                ++Count;
        if (Count >= 2u)
            Surface.Polyline(PointsX, PointsY, Count, Preview, 1.8f);
    }
    else if (Tool.Subject() == SketchSubject::Ellipse && Tool.Anchors().size() == 1u)
    {
        const SpatialBasis LocalBasis = ResolveSketchBasis(Sketch);
        double CentreAlong = 0.0, CentreAcross = 0.0, HoverAlong = 0.0, HoverAcross = 0.0;
        ResolvePlaneCoordinates(LocalBasis, Tool.Anchors()[0], CentreAlong, CentreAcross);
        ResolvePlaneCoordinates(LocalBasis, Tool.HoverPosition(), HoverAlong, HoverAcross);
        const double Major = std::fabs(HoverAlong - CentreAlong);
        const double Minor = std::max(std::fabs(HoverAcross - CentreAcross), Major * 0.5);
        if (Major > 1.0e-6 && Minor > 1.0e-6)
        {
            float PointsX[65] = {};
            float PointsY[65] = {};
            std::uint32_t Count = 0u;
            for (std::uint32_t Step = 0u; Step <= 64u; ++Step)
            {
                const double Angle = (static_cast<double>(Step) / 64.0) * 2.0 * Pi;
                const SpatialPoint Position = ResolvePlanarPoint(LocalBasis,
                                                                  CentreAlong + std::cos(Angle) * Major,
                                                                  CentreAcross + std::sin(Angle) * Minor);
                if (Projected(Position, PointsX[Count], PointsY[Count]))
                    ++Count;
            }
            if (Count >= 2u)
                Surface.Polyline(PointsX, PointsY, Count, Preview, 1.8f);
        }
    }
    else if (Tool.Subject() == SketchSubject::Rectangle && Tool.Anchors().size() == 1u)
    {
        const SpatialPoint A = Tool.Anchors()[0];
        const SpatialPoint C = Tool.HoverPosition();
        const SpatialPoint B = { C.Left, A.Up, A.Forward };
        const SpatialPoint D = { A.Left, A.Up, C.Forward };
        float X[4] = {}, Y[4] = {};
        if (Projected(A, X[0], Y[0]) && Projected(B, X[1], Y[1]) &&
            Projected(C, X[2], Y[2]) && Projected(D, X[3], Y[3]))
        {
            for (std::uint32_t Index = 0u; Index < 4u; ++Index)
            {
                const std::uint32_t Next = (Index + 1u) % 4u;
                const float PointsX[2] = { X[Index], X[Next] };
                const float PointsY[2] = { Y[Index], Y[Next] };
                Surface.Polyline(PointsX, PointsY, 2u, Preview, 1.8f);
            }
        }
    }
    else if (Tool.Subject() == SketchSubject::Circle && Tool.Anchors().size() == 1u)
    {
        const SpatialDirection Radius = Difference(Tool.Anchors()[0], Tool.HoverPosition());
        const double RadiusLength = std::sqrt(LengthSquared(Radius));
        if (RadiusLength > 1.0e-6)
        {
            float PointsX[49] = {};
            float PointsY[49] = {};
            std::uint32_t Count = 0u;
            for (std::uint32_t Step = 0u; Step <= 48u; ++Step)
            {
                const double Angle = (static_cast<double>(Step) / 48.0) * (2.0 * Pi);
                const SpatialPoint Position = { Tool.Anchors()[0].Left + std::cos(Angle) * RadiusLength,
                                                Tool.Anchors()[0].Up,
                                                Tool.Anchors()[0].Forward + std::sin(Angle) * RadiusLength };
                float X = 0.0f, Y = 0.0f;
                if (!Projected(Position, X, Y))
                    continue;
                PointsX[Count] = X;
                PointsY[Count] = Y;
                ++Count;
            }
            if (Count >= 2u)
                Surface.Polyline(PointsX, PointsY, Count, Preview, 1.8f);
        }
    }

    float MarkerX = 0.0f, MarkerY = 0.0f;
    if (Projected(Tool.HoverPosition(), MarkerX, MarkerY))
        Surface.Medallion(MarkerX, MarkerY, 4.0f, Tool.HoverPlacement().Resolved() ? SnapTone : Preview);
}

namespace
{

std::uint32_t OverlayPacked(std::uint32_t Red,
                            std::uint32_t Green,
                            std::uint32_t Blue,
                            std::uint32_t Alpha = 255u)
{
    return PackOverlayColour(Red, Green, Blue, Alpha);
}


ThemeToken TokenFromPacked(std::uint32_t Packed)
{
    return ThemeToken{
        static_cast<std::uint8_t>((Packed >> 16u) & 0xFFu),
        static_cast<std::uint8_t>((Packed >> 8u) & 0xFFu),
        static_cast<std::uint8_t>((Packed >> 0u) & 0xFFu),
        static_cast<std::uint8_t>((Packed >> 24u) & 0xFFu)
    };
}

bool ProjectWorldPoint(const SpatialBasis& Basis,
                       const ParametricViewportState& View,
                       bool Perspective,
                       const PlaneExtent& Extent,
                       const SpatialPoint& Position,
                       float& ScreenX,
                       float& ScreenY,
                       double* Depth = nullptr)
{
    const ViewFrame Frame = ResolveViewportFrame(Basis, View, Perspective);
    if (!Perspective)
    {
        const SpatialDirection Offset = Difference(View.Focus, Position);
        const double X = Dot(Offset, Frame.Right);
        const double Y = Dot(Offset, Frame.Up);
        if (Depth != nullptr)
            *Depth = Dot(Offset, Frame.Forward);
        ScreenX = static_cast<float>(Extent.MinimumX + Extent.Width() * 0.5 + X * View.OrthoScale);
        ScreenY = static_cast<float>(Extent.MinimumY + Extent.Height() * 0.5 - Y * View.OrthoScale);
        return true;
    }

    const SpatialDirection EyeToPoint = Difference(Frame.Eye, Position);
    const double CameraX = Dot(EyeToPoint, Frame.Right);
    const double CameraY = Dot(EyeToPoint, Frame.Up);
    const double CameraZ = Dot(EyeToPoint, Frame.Forward);
    if (Depth != nullptr)
        *Depth = CameraZ;
    if (CameraZ <= 0.01)
        return false;

    const double TanHalf = std::tan(CadPerspectiveFieldOfViewDegrees * 0.5 * Pi / 180.0);
    const double Focal = (Extent.Height() * 0.5) / TanHalf;
    ScreenX = static_cast<float>(Extent.MinimumX + Extent.Width() * 0.5 + CameraX / CameraZ * Focal);
    ScreenY = static_cast<float>(Extent.MinimumY + Extent.Height() * 0.5 - CameraY / CameraZ * Focal);
    return true;
}

WorkspaceRecordName ResolveSelectedRecord(const WorkspaceDirectoryProjection& Directory,
                                          const ParametricWorkspaceContext& Applied)
{
    if (Applied.RowTaken >= Directory.Rows.size())
        return {};
    const WorkspaceDirectoryRow& Row = Directory.Rows[Applied.RowTaken];
    return Row.Role == WorkspaceDirectoryRowRole::Record ? Row.Record : WorkspaceRecordName{};
}

ParametricViewportSelection ResolveEditableSelection(const SketchStructure& Sketch,
                                                     const WorkspaceRecordStructure& Records,
                                                     WorkspaceRecordName SelectedRecord,
                                                     WorkspaceRecordName PendingSelection,
                                                     const ParametricViewportSelection& SemanticSelection)
{
    if (SemanticSelection.Standing() &&
        ((!SelectedRecord.Assigned() || SemanticSelection.Record.IssuedIndex == SelectedRecord.IssuedIndex) ||
         (PendingSelection.Assigned() && PendingSelection.IssuedIndex == SemanticSelection.Record.IssuedIndex)))
        return SemanticSelection;

    ParametricViewportSelection Selection = {};
    ResolvePickForRecord(Sketch, Records, SelectedRecord, Selection);
    return Selection;
}

void RecordViewportGridOverlay(OverlayGeometry& Overlay,
                               const PlaneExtent& Extent,
                               const SketchStructure& Sketch,
                               const ParametricViewportState& View,
                               bool Perspective,
                               const EditorPanelConfiguration& Configuration)
{
    if (Configuration.Lattice == PanelLatticePresentation::None)
        return;

    const SpatialBasis Basis = Sketch.Declared()
        ? ResolveSketchBasis(Sketch)
        : SpatialBasis{ {}, { 1.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0 }, { 0.0, 1.0, 0.0 } };
    const double Step = std::max(Configuration.LatticeCellMetres * static_cast<double>(Configuration.LatticeScale), 1.0);
    const std::uint32_t Subdivisions = std::max(Configuration.Subdivisions, 2u);
    const std::int32_t Count = Perspective ? 40 : 80;

    for (std::int32_t Index = -Count; Index <= Count; ++Index)
    {
        const bool Major = (std::abs(Index) % static_cast<std::int32_t>(Subdivisions)) == 0;
        const std::uint32_t Packed = Major ? OverlayPacked(0xC4u, 0xC8u, 0xD6u, 56u)
                                           : OverlayPacked(0xC4u, 0xC8u, 0xD6u, 26u);
        const float Weight = Major ? Configuration.LatticeLineWeight + 0.25f : Configuration.LatticeLineWeight;

        float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f;
        if (ProjectViewportPoint(Basis, View, Perspective, Extent, static_cast<double>(Index) * Step, static_cast<double>(-Count) * Step, X0, Y0) &&
            ProjectViewportPoint(Basis, View, Perspective, Extent, static_cast<double>(Index) * Step, static_cast<double>( Count) * Step, X1, Y1))
            Overlay.AddLine(X0, Y0, X1, Y1, Packed, Weight);

        if (ProjectViewportPoint(Basis, View, Perspective, Extent, static_cast<double>(-Count) * Step, static_cast<double>(Index) * Step, X0, Y0) &&
            ProjectViewportPoint(Basis, View, Perspective, Extent, static_cast<double>( Count) * Step, static_cast<double>(Index) * Step, X1, Y1))
            Overlay.AddLine(X0, Y0, X1, Y1, Packed, Weight);
    }

    float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f;
    if (Configuration.AxisX &&
        ProjectViewportPoint(Basis, View, Perspective, Extent, -Count * Step, 0.0, X0, Y0) &&
        ProjectViewportPoint(Basis, View, Perspective, Extent, Count * Step, 0.0, X1, Y1))
        Overlay.AddLine(X0, Y0, X1, Y1, OverlayPacked(0xFCu, 0x5Au, 0x5Au, 208u), 1.6f);

    if (Configuration.AxisZ &&
        ProjectViewportPoint(Basis, View, Perspective, Extent, 0.0, -Count * Step, X0, Y0) &&
        ProjectViewportPoint(Basis, View, Perspective, Extent, 0.0, Count * Step, X1, Y1))
        Overlay.AddLine(X0, Y0, X1, Y1, OverlayPacked(0x5Au, 0x8Bu, 0xFCu, 208u), 1.6f);
}

void RecordViewportOverlayFallback(RecordingSurface& Surface,
                                   const PlaneExtent& Extent,
                                   const OverlayGeometry& Overlay)
{
    Surface.Confine(Extent);
    for (std::uint32_t Index = 0u; Index < Overlay.LineCount; ++Index)
    {
        const OverlayLine& Line = Overlay.Lines[Index];
        const float PointsX[2] = { Line.X0, Line.X1 };
        const float PointsY[2] = { Line.Y0, Line.Y1 };
        Surface.Polyline(PointsX, PointsY, 2u, TokenFromPacked(Line.Packed), Line.Thickness);
    }
    for (std::uint32_t Index = 0u; Index < Overlay.DotCount; ++Index)
    {
        const OverlayDot& Dot = Overlay.Dots[Index];
        Surface.Medallion(Dot.X, Dot.Y, Dot.Radius, TokenFromPacked(Dot.Packed));
    }
    for (std::uint32_t Index = 0u; Index < Overlay.TriangleCount; ++Index)
    {
        const OverlayTriangle& Triangle = Overlay.Triangles[Index];
        const float Corners[6] = { Triangle.X0, Triangle.Y0, Triangle.X1, Triangle.Y1, Triangle.X2, Triangle.Y2 };
        Surface.Tongue(Corners, 3u, TokenFromPacked(Triangle.Packed));
    }
    Surface.Release();
}

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

bool ProjectSceneProxyPoint(const SpatialBasis& Basis,
                            const ParametricViewportState& View,
                            bool Perspective,
                            const PlaneExtent& Extent,
                            const SpatialPoint& Centre,
                            double X,
                            double Y,
                            double Z,
                            float& ScreenX,
                            float& ScreenY)
{
    const SpatialPoint Position = Added(Centre,
        Added(Added(Scaled(Basis.Along, X), Scaled(Basis.Normal, Y)), Scaled(Basis.Across, Z)));
    return ProjectWorldPoint(Basis, View, Perspective, Extent, Position, ScreenX, ScreenY);
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
            if (!ProjectSceneProxyPoint(Basis, View, Perspective, Extent, Centre,
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
                    TriangleStanding = ProjectSceneProxyPoint(Basis, View, Perspective, Extent, Centre,
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
            Standing = ProjectSceneProxyPoint(Basis, View, Perspective, Extent, Centre,
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

void AppendOverlayCircle(OverlayGeometry& Overlay,
                         float CentreX,
                         float CentreY,
                         float Radius,
                         std::uint32_t Packed,
                         float Thickness,
                         std::uint32_t SegmentCount = 20u)
{
    for (std::uint32_t Index = 0u; Index < SegmentCount; ++Index)
    {
        const double A0 = (static_cast<double>(Index) / static_cast<double>(SegmentCount)) * 2.0 * Pi;
        const double A1 = (static_cast<double>(Index + 1u) / static_cast<double>(SegmentCount)) * 2.0 * Pi;
        Overlay.AddLine(CentreX + static_cast<float>(std::cos(A0) * Radius),
                        CentreY + static_cast<float>(std::sin(A0) * Radius),
                        CentreX + static_cast<float>(std::cos(A1) * Radius),
                        CentreY + static_cast<float>(std::sin(A1) * Radius),
                        Packed, Thickness);
    }
}

using ParametricGizmoHandle = GizmoHandle;


void RecordViewportSelectionOverlay(OverlayGeometry& Overlay,
                                    const PlaneExtent& Extent,
                                    const SpatialBasis& Basis,
                                    const ParametricViewportState& View,
                                    bool Perspective,
                                    const SketchStructure& Sketch,
                                    const WorkspaceRecordStructure& Records,
                                    const ParametricViewportSelection& Hovered,
                                    const ParametricViewportSelection& Selected)
{
    const auto RecordCurve = [&](SketchCurveName Curve, std::uint32_t Packed, float Thickness)
    {
        if (!Curve.Assigned() || Curve.IssuedIndex > Sketch.Curves().size())
            return;
        std::vector<SpatialPoint> Polyline;
        AppendCurvePolyline(Sketch.Curves()[Curve.IssuedIndex - 1u].Geometry, Polyline, 48u);
        for (std::size_t Index = 0u; Index + 1u < Polyline.size(); ++Index)
        {
            float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f;
            if (ProjectSpatialPoint(Basis, View, Perspective, Extent, Polyline[Index], X0, Y0) &&
                ProjectSpatialPoint(Basis, View, Perspective, Extent, Polyline[Index + 1u], X1, Y1))
                Overlay.AddLine(X0, Y0, X1, Y1, Packed, Thickness);
        }
    };

    if (Selected.Standing())
    {
        if (Selected.Subject == ParametricSelectionSubject::Curve)
            RecordCurve(Selected.Curve, OverlayPacked(0x5Bu, 0x8Cu, 0xFFu, 255u), 2.4f);
        else if (Selected.Subject == ParametricSelectionSubject::Record)
        {
            const WorkspaceRecord* Record = Records.Resolve(Selected.Record);
            if (Record != nullptr && Record->Profile.Assigned() && Record->Profile.IssuedIndex <= Sketch.Profiles().size())
                for (const ProfileLoop& Loop : Sketch.Profiles()[Record->Profile.IssuedIndex - 1u].HeldLoops())
                    for (const ProfileCurveUse& Use : Loop.Traversal)
                        RecordCurve({ Use.TraversedCurve.IssuedIndex }, OverlayPacked(0x5Bu, 0x8Cu, 0xFFu, 255u), 2.2f);
        }

    }

    const auto RecordPoint = [&](const ParametricViewportSelection& Subject, std::uint32_t Outer, std::uint32_t Inner)
    {
        float X = 0.0f;
        float Y = 0.0f;
        if (!ProjectSpatialPoint(Basis, View, Perspective, Extent, Subject.Position, X, Y))
            return;
        Overlay.AddDot(X, Y, Inner, 4.5f);
        AppendOverlayCircle(Overlay, X, Y, 8.0f, Outer, 1.6f);
    };

    if (Selected.Subject == ParametricSelectionSubject::Point || Selected.Subject == ParametricSelectionSubject::Control)
        RecordPoint(Selected, OverlayPacked(0xFFu, 0xFFu, 0xFFu, 224u), OverlayPacked(0x5Bu, 0x8Cu, 0xFFu, 255u));

    if (Hovered.Subject == ParametricSelectionSubject::Curve)
        RecordCurve(Hovered.Curve, OverlayPacked(0xFBu, 0xBFu, 0x24u, 208u), 1.8f);
    else if (Hovered.Standing())
        RecordPoint(Hovered, OverlayPacked(0xFBu, 0xBFu, 0x24u, 208u), OverlayPacked(0xFBu, 0xBFu, 0x24u, 180u));
}

void RecordViewportGizmo(OverlayGeometry& Overlay,
                         const PlaneExtent& Extent,
                         const SpatialBasis& Basis,
                         const ParametricViewportState& View,
                         bool Perspective,
                         const ParametricViewportSelection& Selected,
                         ParametricGizmoHandle HoveredHandle,
                         const ParametricTransformState& Transform)
{
    if (!Selected.Standing())
        return;

    GizmoScreenBasis Screen = {};
    if (!ResolveGizmoScreenBasis(Basis, View, Perspective, Extent, Selected.Position, Screen))
        return;

    // 🔴 EVERY MAGNITUDE BELOW IS A PIXEL COUNT FROM `GizmoMeasure`, CONVERTED TO WORLD HERE.
    //    This function used to carry its own world constants — a 78-unit shaft, a cone at 102, boxes at
    //    94 — while `ResolveGizmoHandle` tested a 44-PIXEL reach. Those agree at exactly one zoom level.
    //    Zoomed in, the arrow ran seven times past its own hit box; zoomed out it was smaller than it.
    //    Reading the same table through one conversion is what stops the two halves drifting again.
    const auto Px = [&](double Pixels) { return GizmoWorld(Screen, Pixels); };

    const ViewFrame Frame = ResolveViewportFrame(Basis, View, Perspective);
    const SpatialPoint Pivot = Selected.Position;
    const SpatialDirection AxisX = Basis.Along;
    const SpatialDirection AxisY = Basis.Normal;
    const SpatialDirection AxisZ = Basis.Across;
    const std::uint32_t XPacked = OverlayPacked(0xE0u, 0x14u, 0x14u, 255u);
    const std::uint32_t YPacked = OverlayPacked(0x22u, 0xC5u, 0x5Eu, 255u);
    const std::uint32_t ZPacked = OverlayPacked(0x15u, 0x60u, 0xE0u, 255u);
    const std::uint32_t White = OverlayPacked(0xFFu, 0xFFu, 0xFFu, 255u);
    const std::uint32_t Highlight = OverlayPacked(0xFBu, 0xBFu, 0x24u, 255u);
    const std::uint32_t PlaneFill = OverlayPacked(0xFFu, 0xFFu, 0xFFu, 56u);
    const std::uint32_t Guide = OverlayPacked(0xFFu, 0xFFu, 0xFFu, 160u);

    const auto Project = [&](const SpatialPoint& P, float& X, float& Y) -> bool
    {
        return ProjectWorldPoint(Basis, View, Perspective, Extent, P, X, Y);
    };
    const auto AddWorldLine = [&](const SpatialPoint& A, const SpatialPoint& B, std::uint32_t Packed, float Thickness)
    {
        float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f;
        if (Project(A, X0, Y0) && Project(B, X1, Y1))
            Overlay.AddLine(X0, Y0, X1, Y1, Packed, Thickness);
    };
    const auto AddWorldTriangle = [&](const SpatialPoint& A, const SpatialPoint& B, const SpatialPoint& C, std::uint32_t Packed)
    {
        float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f, X2 = 0.0f, Y2 = 0.0f;
        if (Project(A, X0, Y0) && Project(B, X1, Y1) && Project(C, X2, Y2))
            Overlay.AddTriangle(X0, Y0, X1, Y1, X2, Y2, Packed);
    };
    const auto AddWorldQuad = [&](const SpatialPoint& A, const SpatialPoint& B,
                                  const SpatialPoint& C, const SpatialPoint& D,
                                  std::uint32_t Packed, std::uint32_t EdgePacked)
    {
        AddWorldTriangle(A, B, C, Packed);
        AddWorldTriangle(A, C, D, Packed);
        AddWorldLine(A, B, EdgePacked, 1.2f);
        AddWorldLine(B, C, EdgePacked, 1.2f);
        AddWorldLine(C, D, EdgePacked, 1.2f);
        AddWorldLine(D, A, EdgePacked, 1.2f);
    };
    const auto AddBox = [&](const SpatialPoint& Centre,
                            const SpatialDirection& A,
                            const SpatialDirection& B,
                            const SpatialDirection& C,
                            double HA, double HB, double HC,
                            std::uint32_t Packed)
    {
        SpatialPoint P[8] = {};
        const double S[8][3] = { {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1}, {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1} };
        for (std::uint32_t Index = 0u; Index < 8u; ++Index)
            P[Index] = Added(Centre, Added(Added(Scaled(A, S[Index][0] * HA), Scaled(B, S[Index][1] * HB)), Scaled(C, S[Index][2] * HC)));
        AddWorldQuad(P[0], P[1], P[2], P[3], Packed, Packed);
        AddWorldQuad(P[4], P[7], P[6], P[5], Packed, Packed);
        AddWorldQuad(P[0], P[4], P[5], P[1], Packed, Packed);
        AddWorldQuad(P[1], P[5], P[6], P[2], Packed, Packed);
        AddWorldQuad(P[2], P[6], P[7], P[3], Packed, Packed);
        AddWorldQuad(P[3], P[7], P[4], P[0], Packed, Packed);
    };
    const auto AddCylinderShaft = [&](const SpatialDirection& Axis,
                                      const SpatialDirection& Side,
                                      std::uint32_t Packed,
                                      bool Highlighted)
    {
        const double Length = Px(GizmoMeasure::ShaftEnd);
        const double Radius = Px(Highlighted ? GizmoMeasure::ShaftRadius * 1.5 : GizmoMeasure::ShaftRadius);
        const SpatialPoint A = Added(Pivot, Scaled(Axis, Px(GizmoMeasure::ShaftStart)));
        const SpatialPoint B = Added(Pivot, Scaled(Axis, Length));
        const SpatialDirection SideB = Normalize(Cross(Axis, Side));
        AddWorldQuad(Added(A, Scaled(Side, Radius)), Added(B, Scaled(Side, Radius)),
                     Added(B, Scaled(Side, -Radius)), Added(A, Scaled(Side, -Radius)),
                     Packed, Packed);
        AddWorldQuad(Added(A, Scaled(SideB, Radius)), Added(B, Scaled(SideB, Radius)),
                     Added(B, Scaled(SideB, -Radius)), Added(A, Scaled(SideB, -Radius)),
                     Packed, Packed);
    };
    const auto AddConeHead = [&](const SpatialDirection& Axis,
                                 const SpatialDirection& Side,
                                 std::uint32_t Packed)
    {
        const SpatialPoint Tip = Added(Pivot, Scaled(Axis, Px(GizmoMeasure::ArrowTip)));
        const SpatialPoint Base = Added(Pivot, Scaled(Axis, Px(GizmoMeasure::ShaftEnd)));
        const SpatialDirection SideB = Normalize(Cross(Axis, Side));
        const double Radius = Px(GizmoMeasure::ArrowRadius);
        const SpatialPoint P0 = Added(Base, Scaled(Side, Radius));
        const SpatialPoint P1 = Added(Base, Scaled(SideB, Radius));
        const SpatialPoint P2 = Added(Base, Scaled(Side, -Radius));
        const SpatialPoint P3 = Added(Base, Scaled(SideB, -Radius));
        AddWorldTriangle(Tip, P0, P1, Packed);
        AddWorldTriangle(Tip, P1, P2, Packed);
        AddWorldTriangle(Tip, P2, P3, Packed);
        AddWorldTriangle(Tip, P3, P0, Packed);
        AddWorldLine(P0, P2, Packed, 1.2f);
        AddWorldLine(P1, P3, Packed, 1.2f);
    };
    const auto AddRing = [&](const SpatialDirection& A,
                             const SpatialDirection& B,
                             double Radius,
                             std::uint32_t Packed,
                             float Thickness)
    {
        SpatialPoint Prior = Added(Pivot, Scaled(A, Radius));
        for (std::uint32_t Segment = 1u; Segment <= 72u; ++Segment)
        {
            const double T = static_cast<double>(Segment) / 72.0 * 2.0 * Pi;
            const SpatialPoint Next = Added(Pivot, Added(Scaled(A, std::cos(T) * Radius), Scaled(B, std::sin(T) * Radius)));
            AddWorldLine(Prior, Next, Packed, Thickness);
            Prior = Next;
        }
    };
    const auto AddScreenHandle = [&](double Radius, std::uint32_t Packed)
    {
        SpatialPoint Prior = Added(Pivot, Scaled(Frame.Right, Radius));
        for (std::uint32_t Segment = 1u; Segment <= 40u; ++Segment)
        {
            const double T = static_cast<double>(Segment) / 40.0 * 2.0 * Pi;
            const SpatialPoint Next = Added(Pivot, Added(Scaled(Frame.Right, std::cos(T) * Radius), Scaled(Frame.Up, std::sin(T) * Radius)));
            AddWorldLine(Prior, Next, Packed, 2.0f);
            Prior = Next;
        }
    };

    if (Transform.Manner() == ParametricTransformMode::Move)
    {
        const std::uint32_t XColour = HoveredHandle == ParametricGizmoHandle::MoveX ? Highlight : XPacked;
        const std::uint32_t ZColour = HoveredHandle == ParametricGizmoHandle::MoveZ ? Highlight : ZPacked;
        const std::uint32_t PlaneColour = HoveredHandle == ParametricGizmoHandle::MoveFree ? Highlight : OverlayPacked(0x1Fu, 0xC7u, 0xC7u, 160u);
        AddCylinderShaft(AxisX, AxisY, XColour, HoveredHandle == ParametricGizmoHandle::MoveX);
        AddConeHead(AxisX, AxisY, XColour);
        AddCylinderShaft(AxisZ, AxisY, ZColour, HoveredHandle == ParametricGizmoHandle::MoveZ);
        AddConeHead(AxisZ, AxisY, ZColour);
        // 📝 The square the hit test looks for: centred `PlaneOffset` out along each axis, `PlaneHalf` to
        //    a side. The two must be the same rectangle or the artist grabs beside what they can see.
        const double PlaneCentre = Px(GizmoMeasure::PlaneOffset);
        const double PlaneEdge   = Px(GizmoMeasure::PlaneHalf);
        const SpatialPoint C = Added(Pivot, Added(Scaled(AxisX, PlaneCentre), Scaled(AxisZ, PlaneCentre)));
        AddWorldQuad(Added(C, Added(Scaled(AxisX, -PlaneEdge), Scaled(AxisZ, -PlaneEdge))),
                     Added(C, Added(Scaled(AxisX,  PlaneEdge), Scaled(AxisZ, -PlaneEdge))),
                     Added(C, Added(Scaled(AxisX,  PlaneEdge), Scaled(AxisZ,  PlaneEdge))),
                     Added(C, Added(Scaled(AxisX, -PlaneEdge), Scaled(AxisZ,  PlaneEdge))),
                     PlaneFill, PlaneColour);
        AddScreenHandle(GizmoMeasure::CentreGrab, HoveredHandle == ParametricGizmoHandle::MoveFree ? Highlight : White);
    }
    else if (Transform.Manner() == ParametricTransformMode::Rotate)
    {
        AddRing(AxisZ, AxisY, Px(GizmoMeasure::RingRadius), HoveredHandle == ParametricGizmoHandle::Rotate ? Highlight : XPacked, 2.2f);
        AddRing(AxisX, AxisZ, Px(GizmoMeasure::RingRadius), HoveredHandle == ParametricGizmoHandle::Rotate ? Highlight : ZPacked, 2.2f);
        AddRing(AxisX, AxisY, Px(GizmoMeasure::RingRadius), HoveredHandle == ParametricGizmoHandle::Rotate ? Highlight : YPacked, 2.2f);
        AddScreenHandle(GizmoMeasure::RingRadius, HoveredHandle == ParametricGizmoHandle::Rotate ? Highlight : White);
    }
    else
    {
        const std::uint32_t XColour = HoveredHandle == ParametricGizmoHandle::ScaleX ? Highlight : XPacked;
        const std::uint32_t ZColour = HoveredHandle == ParametricGizmoHandle::ScaleZ ? Highlight : ZPacked;
        AddCylinderShaft(AxisX, AxisY, XColour, HoveredHandle == ParametricGizmoHandle::ScaleX);
        AddCylinderShaft(AxisZ, AxisY, ZColour, HoveredHandle == ParametricGizmoHandle::ScaleZ);
        const double BoxOut  = Px(GizmoMeasure::ScaleBox);
        const double BoxHalf = Px(GizmoMeasure::ScaleBoxHalf);
        AddBox(Added(Pivot, Scaled(AxisX, BoxOut)), AxisX, AxisY, AxisZ, BoxHalf, BoxHalf, BoxHalf, XColour);
        AddBox(Added(Pivot, Scaled(AxisZ, BoxOut)), AxisZ, AxisY, AxisX, BoxHalf, BoxHalf, BoxHalf, ZColour);
        const std::uint32_t FreeColour = HoveredHandle == ParametricGizmoHandle::ScaleFree ? Highlight : White;
        AddBox(Pivot, AxisX, AxisY, AxisZ, Px(GizmoMeasure::CentreGrab), Px(GizmoMeasure::CentreGrab), Px(GizmoMeasure::CentreGrab), FreeColour);
        const double ScalePlaneCentre = Px(GizmoMeasure::PlaneOffset);
        const double ScalePlaneEdge   = Px(GizmoMeasure::PlaneHalf);
        const SpatialPoint C = Added(Pivot, Added(Scaled(AxisX, ScalePlaneCentre), Scaled(AxisZ, ScalePlaneCentre)));
        AddWorldQuad(Added(C, Added(Scaled(AxisX, -ScalePlaneEdge), Scaled(AxisZ, -ScalePlaneEdge))),
                     Added(C, Added(Scaled(AxisX,  ScalePlaneEdge), Scaled(AxisZ, -ScalePlaneEdge))),
                     Added(C, Added(Scaled(AxisX,  ScalePlaneEdge), Scaled(AxisZ,  ScalePlaneEdge))),
                     Added(C, Added(Scaled(AxisX, -ScalePlaneEdge), Scaled(AxisZ,  ScalePlaneEdge))),
                     PlaneFill, FreeColour);
    }

    if (Transform.Engaged())
    {
        SpatialDirection GuideAxis = {};
        bool Guided = false;
        if (Transform.Restriction() == ParametricTransformConstraint::AxisX)
        {
            GuideAxis = AxisX;
            Guided = true;
        }
        else if (Transform.Restriction() == ParametricTransformConstraint::AxisZ)
        {
            GuideAxis = AxisZ;
            Guided = true;
        }
        else if (Transform.Restriction() == ParametricTransformConstraint::Curve)
        {
            GuideAxis = Transform.CurveDirection;
            Guided = true;
        }
        if (Guided)
            AddWorldLine(Added(Pivot, Scaled(GuideAxis, -1000.0)), Added(Pivot, Scaled(GuideAxis, 1000.0)), Guide, 1.5f);
    }
}

void RecordViewportTransformReadout(RecordingSurface& Surface,
                                    const PlaneExtent& Extent,
                                    const ParametricTransformState& Transform)
{
    if (!Transform.Engaged())
        return;

    char Detail[160] = {};
    char Command[64] = {};
    FormatTransformCommand(Transform.Standing, Command, sizeof(Command));

    if (Transform.Manner() == ParametricTransformMode::Rotate)
        std::snprintf(Detail, sizeof(Detail), "%s • %.1f° • %s",
                      Command,
                      Transform.PreviewValue,
                      Transform.SlideAlongCurve() ? "curve slide" : TransformMannerText(Transform.Manner()));
    else if (Transform.Manner() == ParametricTransformMode::Scale)
        std::snprintf(Detail, sizeof(Detail), "%s • %.3fx • %s",
                      Command,
                      Transform.PreviewValue,
                      TransformMannerText(Transform.Manner()));
    else
        std::snprintf(Detail, sizeof(Detail), "%s • %.2f • %s",
                      Command,
                      Transform.PreviewValue,
                      Transform.SlideAlongCurve() ? "curve slide" : TransformMannerText(Transform.Manner()));

    const float Width = Surface.MeasureRun(Detail, 11.0f, 0.0f);
    Surface.TextRun(Extent.MinimumX + (Extent.Width() - Width) * 0.5f,
                    Extent.MinimumY + 42.0f,
                    Covering(0xE5E7EBu),
                    Detail, 11.0f, 0.0f, true);
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
    SketchSnapPlacement Placement = Modifiers.Commanded ? SketchSnapPlacement{}
                                                        : ResolveNearestSnap(Sketch, Raw, SnapTolerance);
    if (!Placement.Resolved() && !Modifiers.Commanded)
        Placement = ResolveGridSnap(Basis, Raw, 10.0, SnapTolerance);

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

bool ConstraintToolSubject(ParametricToolSubject Tool,
                           ConstraintSubject& Delivered)
{
    switch (Tool)
    {
        case ParametricToolSubject::HorizontalConstraint:    Delivered = ConstraintSubject::Horizontal; return true;
        case ParametricToolSubject::VerticalConstraint:      Delivered = ConstraintSubject::Vertical; return true;
        case ParametricToolSubject::CoincidentConstraint:    Delivered = ConstraintSubject::Coincident; return true;
        case ParametricToolSubject::ParallelConstraint:      Delivered = ConstraintSubject::Parallel; return true;
        case ParametricToolSubject::PerpendicularConstraint: Delivered = ConstraintSubject::Perpendicular; return true;
        case ParametricToolSubject::TangentConstraint:       Delivered = ConstraintSubject::Tangent; return true;
        case ParametricToolSubject::EqualConstraint:         Delivered = ConstraintSubject::Equal; return true;
        default:                                             return false;
    }
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
    if (!ConstraintToolSubject(Tool, Subject) || !ActiveSelection.Standing())
        return false;

    ConstraintSpecification Constraint = {};
    Constraint.Subject = Subject;
    if (Subject == ConstraintSubject::Horizontal || Subject == ConstraintSubject::Vertical)
    {
        if (!ActiveSelection.Curve.Assigned())
            return false;
        Constraint.Primary = ReferenceFromCurve(ActiveSelection.Curve);
    }
    else if (Subject == ConstraintSubject::Coincident)
    {
        if (!ActiveSelection.Point.Assigned() || !HoveredSelection.Point.Assigned())
            return false;
        Constraint.Primary = ReferenceFromPoint(ActiveSelection.Point);
        Constraint.Secondary = ReferenceFromPoint(HoveredSelection.Point);
    }
    else
    {
        if (!ActiveSelection.Curve.Assigned() || !HoveredSelection.Curve.Assigned())
            return false;
        Constraint.Primary = ReferenceFromCurve(ActiveSelection.Curve);
        Constraint.Secondary = ReferenceFromCurve(HoveredSelection.Curve);
    }

    if (!Constraint.Declared())
        return false;

    // 📝 A constraint applied on its own is still one thing the artist did, so it opens a journal of its
    //    own and closes it immediately - one entry in the history, one press of undo.
    std::vector<WorkspaceRecordName> Written;
    PlacementJournal Journal(Revisions);
    SealConstraintRecord(Naming, Records, Journal, Sketch, Constraint, Written);
    Journal.Close();
    if (!Written.empty())
    {
        PendingSelection = Written.front();
        return true;
    }
    return false;
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
    const WorkspaceRecordName SelectedRecord = ResolveSelectedRecord(Directory, WorkspaceApplied);
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
        ResolveEditableSelection(Sketch, Records, SelectedRecord, PendingSelection, SemanticSelection);

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
    if (!Transform.Engaged() && Pointer.ContactPressed && ConstraintToolSubject(ActiveTool, ActiveConstraintSubject))
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


bool AnySelectedRow(const ParametricWorkspaceContext& Applied, std::uint32_t RowCount)
{
    for (std::uint32_t Index = 0u; Index < RowCount; ++Index)
        if (Applied.RowSelected[Index])
            return true;
    return false;
}

std::uint32_t ResolveInitialRow(const WorkspaceDirectoryProjection& Directory)
{
    for (std::uint32_t Index = 0u; Index < Directory.Rows.size(); ++Index)
        if (Directory.Rows[Index].Role == WorkspaceDirectoryRowRole::Record &&
            Directory.Rows[Index].Subject != WorkspaceRecordSubject::Folder)
            return Index;

    for (std::uint32_t Index = 0u; Index < Directory.Rows.size(); ++Index)
        if (Directory.Rows[Index].Role == WorkspaceDirectoryRowRole::Record)
            return Index;

    return 0u;
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

        const std::uint32_t Initial = ResolveInitialRow(Directory);
        Applied.RowTaken = Initial;
        Applied.RowSelectionAnchor = Initial;
        Applied.RowSelected[Initial] = true;
        Seeded = true;
        return;
    }

    if (Applied.RowTaken >= RowCount || !AnySelectedRow(Applied, RowCount))
    {
        for (std::uint32_t Index = 0u; Index < RowCount; ++Index)
            Applied.RowSelected[Index] = false;

        const std::uint32_t Initial = ResolveInitialRow(Directory);
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

void SynchroniseToolContext(const WorkspaceDirectoryProjection& Directory,
                            const WorkspaceRecordStructure& Records,
                            const SketchStructure& Sketch,
                            const ParametricWorkspaceContext& WorkspaceApplied,
                            ParametricToolsContext& ToolsApplied)
{
    ToolsApplied.WorkplaneActivation = Sketch.Declared();
    ToolsApplied.ReferencePlaneCondition = Sketch.Declared();
    ToolsApplied.PlanarProfileCondition = true;
    ToolsApplied.UniformClosureCondition = true;
    ToolsApplied.PendingGeometryCondition = false;
    ToolsApplied.SourceImageryCondition = false;

    ToolsApplied.SelectedCount = 0u;
    for (std::uint32_t Index = 0u; Index < ParametricWorkspaceContext::RowLimit; ++Index)
        if (WorkspaceApplied.RowSelected[Index])
            ++ToolsApplied.SelectedCount;

    ToolsApplied.ProfileCount = 0u;
    ToolsApplied.PerimeterEdgeCount = 0u;
    ToolsApplied.ExistingCircleCount = 0u;
    ToolsApplied.SolidCount = 0u;
    ToolsApplied.AxisAvailability = false;
    ToolsApplied.PathAvailability = false;
    ToolsApplied.SupportMaterialCondition = false;
    ToolsApplied.TangentEndpointCondition = false;
    ToolsApplied.OpeningCondition = false;
    ToolsApplied.MeasurableCondition = false;
    ToolsApplied.ClosedProfileCondition = false;
    ToolsApplied.ActiveDimension = ParametricToolDimension::Nothing;

    if (WorkspaceApplied.RowTaken >= Directory.Rows.size())
        return;

    const WorkspaceDirectoryRow& Row = Directory.Rows[WorkspaceApplied.RowTaken];
    if (Row.Role != WorkspaceDirectoryRowRole::Record)
        return;

    const WorkspaceRecord* Record = Records.Resolve(Row.Record);
    if (Record == nullptr)
        return;

    switch (Record->Subject)
    {
        case WorkspaceRecordSubject::Point:
            ToolsApplied.ActiveDimension = ParametricToolDimension::Vertex;
            ToolsApplied.MeasurableCondition = true;
            break;
        case WorkspaceRecordSubject::OpenCurve:
            ToolsApplied.ActiveDimension = ParametricToolDimension::Edge;
            ToolsApplied.PathAvailability = true;
            ToolsApplied.AxisAvailability = true;
            ToolsApplied.MeasurableCondition = true;
            ToolsApplied.TangentEndpointCondition = true;
            ToolsApplied.PerimeterEdgeCount = 1u;
            break;
        case WorkspaceRecordSubject::ClosedProfile:
            ToolsApplied.ActiveDimension = ParametricToolDimension::Wire;
            ToolsApplied.ProfileCount = 1u;
            ToolsApplied.AxisAvailability = true;
            ToolsApplied.PathAvailability = true;
            ToolsApplied.ClosedProfileCondition = true;
            ToolsApplied.MeasurableCondition = true;
            ToolsApplied.PerimeterEdgeCount = 5u;
            break;
        case WorkspaceRecordSubject::ThinSurface:
            ToolsApplied.ActiveDimension = ParametricToolDimension::Shell;
            ToolsApplied.ProfileCount = 1u;
            ToolsApplied.SupportMaterialCondition = true;
            ToolsApplied.OpeningCondition = true;
            ToolsApplied.MeasurableCondition = true;
            break;
        case WorkspaceRecordSubject::Solid:
            ToolsApplied.ActiveDimension = ParametricToolDimension::Solid;
            ToolsApplied.SolidCount = 1u;
            ToolsApplied.SupportMaterialCondition = true;
            ToolsApplied.ReferencePlaneCondition = true;
            ToolsApplied.AxisAvailability = true;
            ToolsApplied.MeasurableCondition = true;
            ToolsApplied.ClosedProfileCondition = true;
            break;
        case WorkspaceRecordSubject::Dimension:
        case WorkspaceRecordSubject::Constraint:
            ToolsApplied.ActiveDimension = ParametricToolDimension::Edge;
            ToolsApplied.MeasurableCondition = true;
            break;
        case WorkspaceRecordSubject::Pattern:
        case WorkspaceRecordSubject::Mirror:
            ToolsApplied.ActiveDimension = ParametricToolDimension::Face;
            ToolsApplied.ReferencePlaneCondition = true;
            ToolsApplied.MeasurableCondition = true;
            break;
        case WorkspaceRecordSubject::Folder:
        case WorkspaceRecordSubject::SubjectCount:
            break;
    }
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

        SynchroniseToolContext(Directory, Records, Sketch, ParametricApplied, ToolsApplied);
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
