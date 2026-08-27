//============================================================================================================================================
//                                                    SKETCHVIEWPORTOVERLAY.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/SketchViewportOverlay/Api/SketchViewportOverlay.h"

#include "SlateShape/Sketch/ConstraintSolver/Api/ConstraintSolver.h"
#include "SlateShape/Sketch/ProfileArea/Api/ProfileArea.h"
#include "SlateShape/Sketch/SketchPolyline/Api/SketchPolyline.h"
#include "SlateWorkspace/Discipline/ConstraintAuthoring/Api/ConstraintAuthoring.h"
#include "SlateWorkspace/Discipline/PlacementCommit/Api/PlacementCommit.h"
#include "SlateWorkspace/Discipline/TransformGizmo/Api/TransformGizmo.h"
#include "SlateWorkspace/Discipline/TransformSequence/Api/TransformSequence.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/DrawableScale.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Slate
{

void RecordViewportOrientationHud(RecordingSurface& Surface,
                                  const PlaneExtent& Extent,
                                  const PointerCondition& Pointer,
                                  ViewportStanding& View,
                                  EditorPanelConfiguration& Configuration,
                                  bool& PointerTaken)
{
    bool& Perspective = Configuration.Perspective;

    // 📝 The widget is drawn from the CAMERA's axes, so the basis handed to it is the view frame about the
    //    world axes — not the sketch plane's basis, which would tilt the cube with the drawing.
    const SpatialBasis WorldBasis = { {}, { 1.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0 }, { 0.0, 1.0, 0.0 } };
    const ViewFrame Frame = ResolveViewportFrame(WorldBasis, View, Perspective);

    CubeBasis Widget;
    Widget.Right[0]   = Frame.Right.Left;
    Widget.Right[1]   = Frame.Right.Up;
    Widget.Right[2]   = Frame.Right.Forward;
    Widget.Up[0]      = Frame.Up.Left;
    Widget.Up[1]      = Frame.Up.Up;
    Widget.Up[2]      = Frame.Up.Forward;
    Widget.Forward[0] = Frame.Forward.Left;
    Widget.Forward[1] = Frame.Forward.Up;
    Widget.Forward[2] = Frame.Forward.Forward;

    const bool Cad = Configuration.Gizmo == PanelGizmo::Cad;

    if (Pointer.ContactPressed)
    {
        // 🔴 The seven-case conversion switch is gone with the duplicate enumeration. The widget answers a
        //    `ViewportOrientation` directly, and a miss is a refusal rather than a `None` member that
        //    every switch over the enumeration then had to carry.
        const Deliver<ViewportOrientation> Struck =
            HitOrientationWidget(Extent, Widget, Pointer.PositionX, Pointer.PositionY, Cad);

        if (Struck.Resolved)
        {
            const bool Isometric = Struck.Delivered == ViewportOrientation::Isometric;
            ApplyViewportOrientation(View, Struck.Delivered, Isometric);
            Perspective = Isometric;
            PointerTaken = true;
        }
    }

    RecordOrientationWidget(Surface, Extent, Widget, Cad);
}

void RecordCadFallback(RecordingSurface& Surface,
                       const PlaneExtent& Extent,
                       const SketchStructure& Sketch,
                       const ViewportStanding& View,
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
                                const ViewportStanding& View,
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

void RecordConstraintGlyphs(RecordingSurface& Surface,
                            const PlaneExtent& Extent,
                            const SketchStructure& Sketch,
                            const ViewportStanding& View,
                            bool Perspective)
{
    if (!Sketch.Declared())
        return;
    const SpatialBasis Basis = ResolveSketchBasis(Sketch);
    for (const ConstraintSpecification& Constraint : Sketch.Constraints())
    {
        SpatialPoint Anchor = {};
        if (!ResolveConstraintAnchor(Sketch, Constraint.Primary, Anchor))
            continue;

        float X = 0.0f, Y = 0.0f;
        if (!ProjectSpatialPoint(Basis, View, Perspective, Extent, Anchor, X, Y))
            continue;

        Surface.TextRun(X + 6.0f, Y - 6.0f, Covering(0xFBBF24u),
                        ConstraintGlyph(Constraint.Subject), 12.0f, 0.0f, true);
    }
}

void RecordProfileAreaOverlay(RecordingSurface& Surface,
                                const PlaneExtent& Extent,
                                const SketchStructure& Sketch,
                                const ViewportStanding& View,
                                bool Perspective)
{
    if (!Sketch.Declared())
        return;
    const SpatialBasis Basis = ResolveSketchBasis(Sketch);
    const ProfileAreaAnalysis Analysis = AnalyzeProfileAreas(Sketch, 0.05);

    for (const ProfileAreaTriangle& Triangle : Analysis.Triangles)
    {
        float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f, X2 = 0.0f, Y2 = 0.0f;
        if (!ProjectSpatialPoint(Basis, View, Perspective, Extent, Triangle.A, X0, Y0) ||
            !ProjectSpatialPoint(Basis, View, Perspective, Extent, Triangle.B, X1, Y1) ||
            !ProjectSpatialPoint(Basis, View, Perspective, Extent, Triangle.C, X2, Y2))
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
            if (ProjectSpatialPoint(Basis, View, Perspective, Extent, Loop.Points[Index], X0, Y0) &&
                ProjectSpatialPoint(Basis, View, Perspective, Extent, Loop.Points[Index + 1u], X1, Y1))
            {
                const float Xs[2] = { X0, X1 };
                const float Ys[2] = { Y0, Y1 };
                Surface.Polyline(Xs, Ys, 2u, Tone, Loop.Role == ProfileAreaLoopRole::Hole ? 1.3f : 1.8f);
            }
        }
        if (!Loop.Points.empty())
        {
            float X = 0.0f, Y = 0.0f;
            if (ProjectSpatialPoint(Basis, View, Perspective, Extent, Loop.Points[Loop.Points.size() / 2u], X, Y))
                Surface.TextRun(X + 8.0f, Y + 8.0f,
                                Loop.Role == ProfileAreaLoopRole::Hole ? Covering(0xC4B5FDu) : Covering(0xA7F3D0u),
                                Loop.Role == ProfileAreaLoopRole::Hole ? "hole" : "outer", 10.0f);
        }
    }

    for (const ProfileAreaIssue& Issue : Analysis.Issues)
    {
        float X0 = 0.0f, Y0 = 0.0f, X1 = 0.0f, Y1 = 0.0f;
        if (!ProjectSpatialPoint(Basis, View, Perspective, Extent, Issue.Primary, X0, Y0))
            continue;
        const ThemeToken Tone = Issue.Subject == ProfileAreaIssueSubject::SelfIntersection ? Covering(0xF97316u)
                                                                                              : Covering(0xEF4444u);
        if (Issue.Subject == ProfileAreaIssueSubject::Gap &&
            ProjectSpatialPoint(Basis, View, Perspective, Extent, Issue.Secondary, X1, Y1))
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
                  ConstraintDispositionNaming(Constraints),
                  Areas.Clipper2BackendAvailable ? "clipper2" : "poly fallback",
                  Areas.EarcutBackendAvailable ? "earcut" : "fan preview");
    Surface.TextRun(Extent.MinimumX + 16.0f,
                    Extent.MaximumY - 42.0f,
                    ConstraintWarning ? Covering(0xFBBF24u) : Faded(Covering(0xA7F3D0u), 0.85f),
                    Detail, 11.0f);
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
                        const ViewportStanding& View,
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
                const double Angle = (static_cast<double>(Step) / 64.0) * 2.0 * ProjectionPi;
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
                const double Angle = (static_cast<double>(Step) / 48.0) * (2.0 * ProjectionPi);
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

void RecordViewportGridOverlay(OverlayGeometry& Overlay,
                               const PlaneExtent& Extent,
                               const SketchStructure& Sketch,
                               const ViewportStanding& View,
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
        const std::uint32_t Packed = Major ? PackOverlayColour(0xC4u, 0xC8u, 0xD6u, 56u)
                                           : PackOverlayColour(0xC4u, 0xC8u, 0xD6u, 26u);
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
        Overlay.AddLine(X0, Y0, X1, Y1, PackOverlayColour(0xFCu, 0x5Au, 0x5Au, 208u), 1.6f);

    if (Configuration.AxisZ &&
        ProjectViewportPoint(Basis, View, Perspective, Extent, 0.0, -Count * Step, X0, Y0) &&
        ProjectViewportPoint(Basis, View, Perspective, Extent, 0.0, Count * Step, X1, Y1))
        Overlay.AddLine(X0, Y0, X1, Y1, PackOverlayColour(0x5Au, 0x8Bu, 0xFCu, 208u), 1.6f);
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
        Surface.Polyline(PointsX, PointsY, 2u, Unpacked(Line.Packed), Line.Thickness);
    }
    for (std::uint32_t Index = 0u; Index < Overlay.DotCount; ++Index)
    {
        const OverlayDot& Dot = Overlay.Dots[Index];
        Surface.Medallion(Dot.X, Dot.Y, Dot.Radius, Unpacked(Dot.Packed));
    }
    for (std::uint32_t Index = 0u; Index < Overlay.TriangleCount; ++Index)
    {
        const OverlayTriangle& Triangle = Overlay.Triangles[Index];
        const float Corners[6] = { Triangle.X0, Triangle.Y0, Triangle.X1, Triangle.Y1, Triangle.X2, Triangle.Y2 };
        Surface.Tongue(Corners, 3u, Unpacked(Triangle.Packed));
    }
    Surface.Release();
}

void AppendOverlayCircle(OverlayGeometry& Overlay,
                         float CentreX,
                         float CentreY,
                         float Radius,
                         std::uint32_t Packed,
                         float Thickness,
                         std::uint32_t SegmentCount)
{
    for (std::uint32_t Index = 0u; Index < SegmentCount; ++Index)
    {
        const double A0 = (static_cast<double>(Index) / static_cast<double>(SegmentCount)) * 2.0 * ProjectionPi;
        const double A1 = (static_cast<double>(Index + 1u) / static_cast<double>(SegmentCount)) * 2.0 * ProjectionPi;
        Overlay.AddLine(CentreX + static_cast<float>(std::cos(A0) * Radius),
                        CentreY + static_cast<float>(std::sin(A0) * Radius),
                        CentreX + static_cast<float>(std::cos(A1) * Radius),
                        CentreY + static_cast<float>(std::sin(A1) * Radius),
                        Packed, Thickness);
    }
}

void RecordViewportSelectionOverlay(OverlayGeometry& Overlay,
                                    const PlaneExtent& Extent,
                                    const SpatialBasis& Basis,
                                    const ViewportStanding& View,
                                    bool Perspective,
                                    const SketchStructure& Sketch,
                                    const WorkspaceRecordStructure& Records,
                                    const SketchPick& Hovered,
                                    const SketchPick& Selected)
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
        if (Selected.Subject == SketchPickSubject::Curve)
            RecordCurve(Selected.Curve, PackOverlayColour(0x5Bu, 0x8Cu, 0xFFu, 255u), 2.4f);
        else if (Selected.Subject == SketchPickSubject::Record)
        {
            const WorkspaceRecord* Record = Records.Resolve(Selected.Record);
            if (Record != nullptr && Record->Profile.Assigned() && Record->Profile.IssuedIndex <= Sketch.Profiles().size())
                for (const ProfileLoop& Loop : Sketch.Profiles()[Record->Profile.IssuedIndex - 1u].HeldLoops())
                    for (const ProfileCurveUse& Use : Loop.Traversal)
                        RecordCurve({ Use.TraversedCurve.IssuedIndex }, PackOverlayColour(0x5Bu, 0x8Cu, 0xFFu, 255u), 2.2f);
        }

    }

    const auto RecordPoint = [&](const SketchPick& Subject, std::uint32_t Outer, std::uint32_t Inner)
    {
        float X = 0.0f;
        float Y = 0.0f;
        if (!ProjectSpatialPoint(Basis, View, Perspective, Extent, Subject.Position, X, Y))
            return;
        Overlay.AddDot(X, Y, Inner, 4.5f);
        AppendOverlayCircle(Overlay, X, Y, 8.0f, Outer, 1.6f);
    };

    if (Selected.Subject == SketchPickSubject::Point || Selected.Subject == SketchPickSubject::Control)
        RecordPoint(Selected, PackOverlayColour(0xFFu, 0xFFu, 0xFFu, 224u), PackOverlayColour(0x5Bu, 0x8Cu, 0xFFu, 255u));

    if (Hovered.Subject == SketchPickSubject::Curve)
        RecordCurve(Hovered.Curve, PackOverlayColour(0xFBu, 0xBFu, 0x24u, 208u), 1.8f);
    else if (Hovered.Standing())
        RecordPoint(Hovered, PackOverlayColour(0xFBu, 0xBFu, 0x24u, 208u), PackOverlayColour(0xFBu, 0xBFu, 0x24u, 180u));
}

void RecordViewportGizmo(OverlayGeometry& Overlay,
                         const PlaneExtent& Extent,
                         const SpatialBasis& Basis,
                         const ViewportStanding& View,
                         bool Perspective,
                         const SketchPick& Selected,
                         GizmoHandle HoveredHandle,
                         const TransformSession& Transform)
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
    const std::uint32_t XPacked = PackOverlayColour(0xE0u, 0x14u, 0x14u, 255u);
    const std::uint32_t YPacked = PackOverlayColour(0x22u, 0xC5u, 0x5Eu, 255u);
    const std::uint32_t ZPacked = PackOverlayColour(0x15u, 0x60u, 0xE0u, 255u);
    const std::uint32_t White = PackOverlayColour(0xFFu, 0xFFu, 0xFFu, 255u);
    const std::uint32_t Highlight = PackOverlayColour(0xFBu, 0xBFu, 0x24u, 255u);
    const std::uint32_t PlaneFill = PackOverlayColour(0xFFu, 0xFFu, 0xFFu, 56u);
    const std::uint32_t Guide = PackOverlayColour(0xFFu, 0xFFu, 0xFFu, 160u);

    const auto Project = [&](const SpatialPoint& P, float& X, float& Y) -> bool
    {
        return ProjectSpatialPoint(Basis, View, Perspective, Extent, P, X, Y);
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
            const double T = static_cast<double>(Segment) / 72.0 * 2.0 * ProjectionPi;
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
            const double T = static_cast<double>(Segment) / 40.0 * 2.0 * ProjectionPi;
            const SpatialPoint Next = Added(Pivot, Added(Scaled(Frame.Right, std::cos(T) * Radius), Scaled(Frame.Up, std::sin(T) * Radius)));
            AddWorldLine(Prior, Next, Packed, 2.0f);
            Prior = Next;
        }
    };

    if (Transform.Manner() == TransformManner::Move)
    {
        const std::uint32_t XColour = HoveredHandle == GizmoHandle::MoveX ? Highlight : XPacked;
        const std::uint32_t ZColour = HoveredHandle == GizmoHandle::MoveZ ? Highlight : ZPacked;
        const std::uint32_t PlaneColour = HoveredHandle == GizmoHandle::MoveFree ? Highlight : PackOverlayColour(0x1Fu, 0xC7u, 0xC7u, 160u);
        AddCylinderShaft(AxisX, AxisY, XColour, HoveredHandle == GizmoHandle::MoveX);
        AddConeHead(AxisX, AxisY, XColour);
        AddCylinderShaft(AxisZ, AxisY, ZColour, HoveredHandle == GizmoHandle::MoveZ);
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
        AddScreenHandle(GizmoMeasure::CentreGrab, HoveredHandle == GizmoHandle::MoveFree ? Highlight : White);
    }
    else if (Transform.Manner() == TransformManner::Rotate)
    {
        AddRing(AxisZ, AxisY, Px(GizmoMeasure::RingRadius), HoveredHandle == GizmoHandle::Rotate ? Highlight : XPacked, 2.2f);
        AddRing(AxisX, AxisZ, Px(GizmoMeasure::RingRadius), HoveredHandle == GizmoHandle::Rotate ? Highlight : ZPacked, 2.2f);
        AddRing(AxisX, AxisY, Px(GizmoMeasure::RingRadius), HoveredHandle == GizmoHandle::Rotate ? Highlight : YPacked, 2.2f);
        AddScreenHandle(GizmoMeasure::RingRadius, HoveredHandle == GizmoHandle::Rotate ? Highlight : White);
    }
    else
    {
        const std::uint32_t XColour = HoveredHandle == GizmoHandle::ScaleX ? Highlight : XPacked;
        const std::uint32_t ZColour = HoveredHandle == GizmoHandle::ScaleZ ? Highlight : ZPacked;
        AddCylinderShaft(AxisX, AxisY, XColour, HoveredHandle == GizmoHandle::ScaleX);
        AddCylinderShaft(AxisZ, AxisY, ZColour, HoveredHandle == GizmoHandle::ScaleZ);
        const double BoxOut  = Px(GizmoMeasure::ScaleBox);
        const double BoxHalf = Px(GizmoMeasure::ScaleBoxHalf);
        AddBox(Added(Pivot, Scaled(AxisX, BoxOut)), AxisX, AxisY, AxisZ, BoxHalf, BoxHalf, BoxHalf, XColour);
        AddBox(Added(Pivot, Scaled(AxisZ, BoxOut)), AxisZ, AxisY, AxisX, BoxHalf, BoxHalf, BoxHalf, ZColour);
        const std::uint32_t FreeColour = HoveredHandle == GizmoHandle::ScaleFree ? Highlight : White;
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
        if (Transform.Restriction() == TransformRestriction::AxisX)
        {
            GuideAxis = AxisX;
            Guided = true;
        }
        else if (Transform.Restriction() == TransformRestriction::AxisZ)
        {
            GuideAxis = AxisZ;
            Guided = true;
        }
        else if (Transform.Restriction() == TransformRestriction::Curve)
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
                                    const TransformSession& Transform)
{
    if (!Transform.Engaged())
        return;

    char Detail[160] = {};
    char Command[64] = {};
    FormatTransformCommand(Transform.Standing, Command, sizeof(Command));

    if (Transform.Manner() == TransformManner::Rotate)
        std::snprintf(Detail, sizeof(Detail), "%s • %.1f° • %s",
                      Command,
                      Transform.PreviewValue,
                      Transform.SlideAlongCurve() ? "curve slide" : TransformMannerText(Transform.Manner()));
    else if (Transform.Manner() == TransformManner::Scale)
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

}   // namespace Slate
