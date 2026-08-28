// 🧩 Every curve subject must draw, show what shapes it, keep its shape as it grows, land on the
//    workplane, and close on its own endpoint.
//
// 🔴 FIVE INDEPENDENT DEFECTS SHARED ONE SYMPTOM -- "the drawing tools don't work":
//    1. The preview named ONE SUBJECT PER BRANCH, so Hermite, basis spline and NURBS had no branch and
//       drew no feedback at all. All three committed correctly; nothing ever showed it.
//    2. Nothing emitted a marker for an anchor, so a Bezier drew a bare curve with no control points.
//    3. Tessellation was a flat 48 steps whatever the curve's size, and every chord of an approximated
//       arc lies INSIDE the true curve -- so the larger a shape was drawn, the further inside its own
//       outline it was drawn. "The longer the curve, the more it shrinks."
//    4. The polygon commit hardcoded six sides, so the tool drew a hexagon and nothing else.
//    5. `ResolveSketchBasis` asked `Declared()`, which is all-or-nothing over the sketch's CURVES too,
//       so the FIRST shape on a fresh sketch was mapped through the world origin rather than the
//       active workplane -- it drew in mid-air.
//    6. Snapping raced every candidate on raw distance, and the perpendicular foot on a segment is
//       always at least as near as that segment's endpoint, so a polyline could never close on itself.

#include "SketchToolset/SketchTool/SketchPlacement/Api/SketchPlacement.h"
#include "SlateShape/Sketch/SketchPolyline/Api/SketchPolyline.h"
#include "SlateShape/Sketch/SketchRenderingProjection/Api/SketchRenderingProjection.h"
#include "SlateShape/Sketch/SketchSnap/Api/SketchSnap.h"
#include "SlateWorkspace/Discipline/PlacementCommit/Api/PlacementCommit.h"
#include "SlateWorkspace/Discipline/SketchInteraction/Api/SketchInteraction.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/SketchBasis.h"
#include "SlateWorkspace/Discipline/WorkplaneCatalogue/Api/WorkplaneCatalogue.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace Slate;

namespace
{

std::uint32_t Claims = 0u;
std::uint32_t Failures = 0u;

void Require(bool Held, const char* Claim)
{
    ++Claims;
    if (!Held)
    {
        ++Failures;
        std::printf("  FAILED  %s\n", Claim);
    }
}

/// 📝 Drives one subject through `Clicks` contacts and reports what its preview produced.
void DrivePreview(SketchSubject Subject,
                  PlacementMethod Method,
                  std::uint32_t Clicks,
                  WorkspaceCadPacket& Delivered,
                  bool& CurveDeclared)
{
    SketchStructure   Sketch;
    WorkplaneCatalogue Workplanes;
    SketchPlacement   Tool;

    Sketch.DeclarePlane({ Workplanes.Active().Origin,
                          Workplanes.Active().Normal,
                          Workplanes.Active().Along });

    Tool.Declare(Subject, Method, false);
    for (std::uint32_t Index = 0u; Index < Clicks; ++Index)
    {
        Tool.Hover({ 30.0 * Index, 0.0, (Index % 2u) ? 45.0 : 0.0 }, {});
        static_cast<void>(Tool.Anchor(false));
    }
    Tool.Hover({ 30.0 * Clicks, 0.0, 20.0 }, {});

    const CurveSpecification Geometry =
        ResolvePlacementCurve(Tool.Subject(), Tool.Anchors(), Tool.HoverPosition());
    CurveDeclared = Geometry.Declared();

    static_cast<void>(ProjectPlacementPreview(Sketch, Geometry, Tool.Anchors(),
                                              Tool.HoverPosition(), Delivered));
}

/// 📝 The worst distance by which a tessellated circle falls inside the circle it approximates.
double ChordErrorForRadius(double Radius)
{
    CircleCurve Round;
    Round.Centre         = { 0.0, 0.0, 0.0 };
    Round.Normal         = { 0.0, 1.0, 0.0 };
    Round.StartDirection = { 1.0, 0.0, 0.0 };
    Round.Radius         = Radius;

    const CurveSpecification Geometry = CurveSpecification::DeclareCircle(Round);
    const std::uint32_t Steps = ResolveCurveStepCount(Geometry, 48u);
    return Radius * (1.0 - std::cos(3.14159265358979 / static_cast<double>(Steps)));
}

}   // namespace

int main()
{
    std::printf("[SketchDrawingProof] every curve subject draws, shows its controls and holds its shape\n");

    //--------------------------------------------------------------------------------------------
    // 🔴 EVERY CURVE SUBJECT PREVIEWS. The four spline subjects are the ones that drew nothing.
    //--------------------------------------------------------------------------------------------
    {
        struct Case
        {
            SketchSubject   Subject;
            PlacementMethod Method;
            std::uint32_t   Clicks;
            const char*     Naming;
        };

        const Case Cases[] = {
            { SketchSubject::Bezier,         PlacementMethod::Extent,  3u, "Bezier" },
            { SketchSubject::Hermite,        PlacementMethod::Extent,  4u, "Hermite" },
            { SketchSubject::BasisSpline,    PlacementMethod::Extent,  4u, "basis spline" },
            { SketchSubject::RationalSpline, PlacementMethod::Extent,  4u, "NURBS curve" },
            { SketchSubject::Polygon,        PlacementMethod::Centred, 1u, "polygon" },
            { SketchSubject::Line,           PlacementMethod::Extent,  1u, "line" },
        };

        for (const Case& Subject : Cases)
        {
            WorkspaceCadPacket Delivered;
            bool CurveDeclared = false;
            DrivePreview(Subject.Subject, Subject.Method, Subject.Clicks, Delivered, CurveDeclared);

            Require(CurveDeclared, "the placement in progress must describe a curve");
            Require(Delivered.SegmentCount >= 1u, "every curve subject must preview at least one segment");

            // 🔴 THE CONTROL POINTS. One marker per anchor, plus one for the hover.
            Require(Delivered.MarkerCount == Subject.Clicks + 1u,
                    "every anchor and the hover must draw a control marker");

            std::printf("  %-14s segments=%3u  controls=%u\n",
                        Subject.Naming, Delivered.SegmentCount, Delivered.MarkerCount);
        }
    }

    //--------------------------------------------------------------------------------------------
    // 🔴 THE PREVIEW GOES IN THE PACKET THE GPU PASS READS, appended to the committed shapes rather
    //    than drawn separately -- which is what takes it off the CPU.
    //--------------------------------------------------------------------------------------------
    {
        SketchStructure           Sketch;
        WorkspaceRecordStructure  Records;
        WorkspaceRevisionSequence Revisions;
        WorkspaceNameIndex        Naming;
        WorkplaneCatalogue        Workplanes;
        SketchPlacement           Tool;
        WorkspaceRecordName       Pending;

        Sketch.DeclarePlane({ Workplanes.Active().Origin,
                              Workplanes.Active().Normal,
                              Workplanes.Active().Along });

        Tool.Declare(SketchSubject::Line, PlacementMethod::Extent, false);
        Tool.Hover({ 0.0, 0.0, 0.0 }, {});
        static_cast<void>(Tool.Anchor(false));
        Tool.Hover({ 100.0, 0.0, 0.0 }, {});
        static_cast<void>(Tool.Anchor(true));

        const SealedPlacement Sealed = Tool.Seal();
        const Deliver<WorkspaceRecordName> Committed =
            CommitPlacement(Naming, Sketch, Records, Revisions, Sealed);
        AdoptCommittedShape(Sealed.Subject, Naming, Sketch, Records, Revisions, Committed, Pending);

        WorkspaceCadPacket Delivered;
        static_cast<void>(ProjectSketchRendering(Sketch, Records, Delivered));
        const std::uint32_t Committed_Segments = Delivered.SegmentCount;
        Require(Committed_Segments >= 1u, "the committed line must project into the packet");

        Tool.Declare(SketchSubject::Bezier, PlacementMethod::Extent, false);
        Tool.Hover({ 0.0, 0.0, 50.0 }, {});
        static_cast<void>(Tool.Anchor(false));
        Tool.Hover({ 80.0, 0.0, 90.0 }, {});

        static_cast<void>(ProjectPlacementPreview(
            Sketch,
            ResolvePlacementCurve(Tool.Subject(), Tool.Anchors(), Tool.HoverPosition()),
            Tool.Anchors(), Tool.HoverPosition(), Delivered));

        // 🔴 APPENDED, NOT REPLACED. The committed geometry must survive the preview being added, or
        //    the shape under the pointer would blank everything already drawn.
        Require(Delivered.SegmentCount > Committed_Segments,
                "the preview must be appended to the same packet as the committed shapes");
        Require(Delivered.MarkerCount >= 1u,
                "the preview's control points must reach the packet the GPU pass reads");
    }

    //--------------------------------------------------------------------------------------------
    // 🔴 A CURVE MUST NOT SHRINK AS IT GROWS. Chord error is held near-constant instead of rising in
    //    proportion to the radius, which is what a fixed step count does.
    //--------------------------------------------------------------------------------------------
    {
        const double Small = ChordErrorForRadius(40.0);
        const double Large = ChordErrorForRadius(1000.0);
        const double Huge  = ChordErrorForRadius(4000.0);

        Require(Large <= Small * 2.0,
                "a 25x larger curve must not be drawn 25x further inside its own outline");
        Require(Huge <= 0.5,
                "chord error must stay well under half a unit at every drawable size");

        // 🔴 THE NEGATIVE CLAIM. Had the step count stayed fixed, error would rise LINEARLY with the
        //    radius -- this is the number the defect produced, and it must not come back.
        const double FixedStepError = 1000.0 * (1.0 - std::cos(3.14159265358979 / 48.0));
        Require(Large < FixedStepError * 0.25,
                "adaptive tessellation must beat the fixed 48-step count it replaced");

        std::printf("  chord error r=40 %.4f  r=1000 %.4f  (fixed-48 would be %.4f)\n",
                    Small, Large, FixedStepError);

        // 🔴 AND THE RENDER PATH MUST ACTUALLY USE IT. Asserting the helper alone leaves the call
        //    sites free to keep passing a flat 48 -- which is precisely the defect. A large circle is
        //    committed and projected end-to-end, and the packet must carry more than the fixed count.
        {
            SketchStructure           Round;
            WorkspaceRecordStructure  RoundRecords;
            WorkspaceRevisionSequence RoundRevisions;
            WorkspaceNameIndex        RoundNaming;
            WorkplaneCatalogue        RoundPlanes;
            SketchPlacement           RoundTool;
            WorkspaceRecordName       RoundPending;

            Round.DeclarePlane({ RoundPlanes.Active().Origin,
                                 RoundPlanes.Active().Normal,
                                 RoundPlanes.Active().Along });

            RoundTool.Declare(SketchSubject::Circle, PlacementMethod::Centred, false);
            RoundTool.Hover({ 0.0, 0.0, 0.0 }, {});
            static_cast<void>(RoundTool.Anchor(false));
            RoundTool.Hover({ 1000.0, 0.0, 0.0 }, {});
            static_cast<void>(RoundTool.Anchor(true));

            const SealedPlacement RoundSealed = RoundTool.Seal();
            const Deliver<WorkspaceRecordName> RoundCommitted =
                CommitPlacement(RoundNaming, Round, RoundRecords, RoundRevisions, RoundSealed);
            AdoptCommittedShape(RoundSealed.Subject, RoundNaming, Round, RoundRecords,
                                RoundRevisions, RoundCommitted, RoundPending);

            WorkspaceCadPacket RoundPacket;
            static_cast<void>(ProjectSketchRendering(Round, RoundRecords, RoundPacket));

            // 🔴 THE SHRINKING, MEASURED ON THE DRAWN GEOMETRY ITSELF. Every chord's midpoint lies
            //    inside the true circle by the sagitta, so the smallest radius across the packet IS
            //    how far the drawn shape has pulled in from the shape the artist asked for. Counting
            //    segments is too weak a claim -- a circle commits as four arcs, so a flat 48 steps
            //    still yields 192 segments and a count-based claim passes while the shape shrinks.
            double Tightest = 1.0e30;
            for (std::uint32_t Index = 0u; Index < RoundPacket.SegmentCount; ++Index)
            {
                const WorkspaceCadSegment& Drawn = RoundPacket.Segments[Index];
                const double MidAlong  = (static_cast<double>(Drawn.Along0)  + Drawn.Along1)  * 0.5;
                const double MidAcross = (static_cast<double>(Drawn.Across0) + Drawn.Across1) * 0.5;
                Tightest = std::min(Tightest, std::sqrt(MidAlong * MidAlong + MidAcross * MidAcross));
            }

            // ⚠️ THE THRESHOLD IS THE TOLERANCE, NOT A ROUND NUMBER. A loose bound is a gate that
            //    cannot fail: the flat-48 defect shrinks this circle by 0.134 units, so any threshold
            //    above that passes while the bug is present. Twice `CurveChordTolerance` is the
            //    tightest bound the adaptive path is guaranteed to meet and the defect cannot.
            const double Shrinkage = 1000.0 - Tightest;
            Require(Shrinkage < CurveChordTolerance * 2.0,
                    "a radius-1000 circle must not be drawn measurably inside its own outline");

            std::printf("  radius-1000 circle: %u segments, drawn %.4f units inside "
                        "(a flat 48 steps shrinks it by %.4f)\n",
                        RoundPacket.SegmentCount, Shrinkage,
                        1000.0 * (1.0 - std::cos(3.14159265358979 / (48.0 * 4.0))));
        }

        // 📝 A line is exact at two points however long it is; it must not be given 512 of them.
        const CurveSpecification Straight =
            CurveSpecification::DeclareLine({ 0.0, 0.0, 0.0 }, { 9000.0, 0.0, 0.0 });
        Require(ResolveCurveStepCount(Straight, 48u) == 2u,
                "a line needs exactly two points however long it is drawn");

        // 📝 And the ceiling must hold, or a curve drawn kilometres wide would ask for millions.
        Require(ResolveCurveStepCount(
                    CurveSpecification::DeclareLine({ 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }), 48u)
                <= CurveStepLimit,
                "the step count must never exceed its limit");
    }

    //--------------------------------------------------------------------------------------------
    // 🔴 A POLYGON IS A CIRCLE WITH A SIDE COUNT: drag for the radius, WHEEL for the resolution.
    //--------------------------------------------------------------------------------------------
    {
        SketchStructure           Sketch;
        WorkspaceRecordStructure  Records;
        WorkspaceRevisionSequence Revisions;
        WorkspaceNameIndex        Naming;
        WorkplaneCatalogue        Workplanes;
        SketchPlacement           Tool;
        WorkspaceRecordName       Pending;

        Sketch.DeclarePlane({ Workplanes.Active().Origin,
                              Workplanes.Active().Normal,
                              Workplanes.Active().Along });

        Tool.Declare(SketchSubject::Polygon, PlacementMethod::Centred, false);
        Tool.Hover({ 0.0, 0.0, 0.0 }, {});
        static_cast<void>(Tool.Anchor(false));
        Tool.Hover({ 80.0, 0.0, 0.0 }, {});

        Require(Tool.Resolution() == PolygonSideDefault, "a polygon starts at its default resolution");

        Require(Tool.Resolve(2.0f), "the wheel must be consumed while a polygon is being placed");
        Require(Tool.Resolution() == PolygonSideDefault + 2u, "two notches must add two sides");

        static_cast<void>(Tool.Resolve(-50.0f));
        Require(Tool.Resolution() == PolygonSideMinimum, "the side count must clamp at its minimum");

        static_cast<void>(Tool.Resolve(500.0f));
        Require(Tool.Resolution() == PolygonSideMaximum, "the side count must clamp at its maximum");

        static_cast<void>(Tool.Resolve(-56.0f));
        const std::uint32_t Wanted = Tool.Resolution();
        Require(Wanted == 8u, "the wheel must land on the side count it was driven to");

        Require(Tool.Anchor(true) == PlacementArrival::Complete, "Enter must complete the polygon");
        const SealedPlacement Sealed = Tool.Seal();
        Require(Sealed.Resolution == Wanted, "the sealed placement must carry the chosen resolution");

        const Deliver<WorkspaceRecordName> Committed =
            CommitPlacement(Naming, Sketch, Records, Revisions, Sealed);
        Require(Committed.Resolved, "the polygon must commit");

        // 🔴 THE HARDCODED SIX. An eight-sided polygon must have eight edges, not six.
        Require(Sketch.Curves().size() == Wanted,
                "the committed polygon must have the number of sides the wheel chose");

        std::printf("  polygon wheeled to %u sides -> %zu curves committed\n",
                    Wanted, Sketch.Curves().size());
    }

    //--------------------------------------------------------------------------------------------
    // 🔴 THE WHEEL BELONGS TO THE CAMERA FOR EVERY OTHER TOOL. Consuming it always would break zoom.
    //--------------------------------------------------------------------------------------------
    {
        SketchPlacement Tool;
        Tool.Declare(SketchSubject::Circle, PlacementMethod::Centred, false);
        Require(!Tool.Resolve(3.0f), "the wheel must still zoom while a circle is being drawn");

        Tool.Declare(SketchSubject::Polygon, PlacementMethod::Centred, false);
        Require(!Tool.Resolve(0.0f), "an idle wheel must not be consumed");
    }

    //--------------------------------------------------------------------------------------------
    // 🔴 SHAPES LAND ON THE ACTIVE WORKPLANE, NOT IN MID-AIR -- including the FIRST shape drawn, when
    //    the sketch holds a plane but not yet a single curve.
    //--------------------------------------------------------------------------------------------
    {
        WorkplaneCatalogue Workplanes;
        for (std::uint32_t Index = 1u; Index <= 3u; ++Index)
        {
            Discard(Workplanes.Activate({ Index }));

            SketchStructure Sketch;
            Sketch.DeclarePlane({ Workplanes.Active().Origin,
                                  Workplanes.Active().Normal,
                                  Workplanes.Active().Along });

            // 🔴 A plane and no curves is the state the FIRST shape of every sketch is drawn in.
            Require(Sketch.PlaneDeclared(), "a sketch given a plane must report that plane as declared");

            const SpatialBasis Basis = ResolveSketchBasis(Sketch);
            const SpatialDirection Wanted = Workplanes.Active().Normal;

            Require(std::fabs(Basis.Normal.Left    - Wanted.Left)    < 1.0e-9 &&
                    std::fabs(Basis.Normal.Up      - Wanted.Up)      < 1.0e-9 &&
                    std::fabs(Basis.Normal.Forward - Wanted.Forward) < 1.0e-9,
                    "the drawing basis must be the ACTIVE workplane, not the world origin");
        }
    }

    //--------------------------------------------------------------------------------------------
    // 🔴 ONE MALFORMED CURVE MUST NOT MOVE EVERY OTHER SHAPE INTO MID-AIR. `Declared()` is
    //    all-or-nothing across every curve, so a single degenerate one flipped the whole sketch to
    //    "not declared" -- and the basis silently fell back to the world origin, relocating every
    //    shape already drawn on a Front or Right workplane.
    //--------------------------------------------------------------------------------------------
    {
        SketchStructure Sketch;
        Sketch.DeclarePlane({ { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0 }, { 1.0, 0.0, 0.0 } });
        static_cast<void>(Sketch.DeclareLine({ 0.0, 0.0, 0.0 }, { 50.0, 0.0, 0.0 }));

        const SpatialBasis Healthy = ResolveSketchBasis(Sketch);

        // 📝 A default-constructed specification is undeclared: the state a refused placement leaves.
        static_cast<void>(Sketch.DeclareCurve(CurveSpecification{}));
        Require(!Sketch.Declared(), "one malformed curve makes the whole sketch undeclared");
        Require(Sketch.PlaneDeclared(), "but the plane it was drawn on is still perfectly good");

        const SpatialBasis Wounded = ResolveSketchBasis(Sketch);
        Require(std::fabs(Healthy.Normal.Left    - Wounded.Normal.Left)    < 1.0e-12 &&
                std::fabs(Healthy.Normal.Up      - Wounded.Normal.Up)      < 1.0e-12 &&
                std::fabs(Healthy.Normal.Forward - Wounded.Normal.Forward) < 1.0e-12,
                "a malformed curve must not relocate every other shape onto a different plane");
        Require(std::fabs(Wounded.Normal.Forward - 1.0) < 1.0e-12,
                "the basis must stay on the Front workplane it was given");
    }

    //--------------------------------------------------------------------------------------------
    // 🔴 A POLYLINE MUST CLOSE ON ITS OWN ENDPOINT. The perpendicular foot on a segment is always at
    //    least as near as that segment's end, so distance alone can never pick the corner.
    //--------------------------------------------------------------------------------------------
    {
        SketchStructure Sketch;
        Sketch.DeclarePlane({ { 0.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }, { 1.0, 0.0, 0.0 } });

        static_cast<void>(Sketch.DeclareLine({ 0.0, 0.0, 0.0 },   { 100.0, 0.0, 0.0 }));
        static_cast<void>(Sketch.DeclareLine({ 100.0, 0.0, 0.0 }, { 100.0, 0.0, 100.0 }));
        static_cast<void>(Sketch.DeclareLine({ 100.0, 0.0, 100.0 }, { 0.0, 0.0, 100.0 }));

        // 📝 Probing diagonally inward from the start corner: the foot of the perpendicular onto the
        //    first segment sits at (2,0,0), strictly nearer than the corner at the origin.
        const SketchSnapPlacement Placed = ResolveNearestSnap(Sketch, { 2.0, 0.0, 2.0 }, 10.0);
        Require(Placed.Resolved(), "a probe within tolerance of a corner must snap to something");
        Require(Placed.Subject == SketchSnapSubject::Endpoint,
                "an endpoint must beat a nearer point along the same curve");
        Require(std::fabs(Placed.Position.Left) < 1.0e-9 &&
                std::fabs(Placed.Position.Forward) < 1.0e-9,
                "closing a loop must land exactly on the start point");

        // 🔴 THE NEGATIVE CLAIM. Precedence must not swallow distance: two endpoints still resolve to
        //    the nearer one, or snapping would jump to whichever corner happened to be declared first.
        const SketchSnapPlacement Nearer = ResolveNearestSnap(Sketch, { 98.0, 0.0, 2.0 }, 10.0);
        Require(Nearer.Subject == SketchSnapSubject::Endpoint, "the far corner must also snap by kind");
        Require(std::fabs(Nearer.Position.Left - 100.0) < 1.0e-9,
                "between two endpoints the nearer one must still win");
    }

    //--------------------------------------------------------------------------------------------
    // 🔴 A POLYLINE MUST CLOSE ON ITS OWN IN-PROGRESS ANCHORS. Nothing is declared into the sketch
    //    until the placement seals, so the FIRST loop an artist draws has no declared curve to aim at.
    //--------------------------------------------------------------------------------------------
    {
        SketchStructure Sketch;
        Sketch.DeclarePlane({ { 0.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }, { 1.0, 0.0, 0.0 } });

        Require(Sketch.Curves().empty(), "the sketch holds no declared curve yet");

        const std::vector<SpatialPoint> Pending = {
            { 0.0, 0.0, 0.0 }, { 100.0, 0.0, 0.0 }, { 100.0, 0.0, 100.0 }
        };

        const SketchSnapPlacement Blind = ResolveNearestSnap(Sketch, { 2.0, 0.0, 2.0 }, 10.0, {}, 10.0, {});
        Require(Blind.Subject != SketchSnapSubject::Endpoint,
                "with nothing declared and nothing pending there is no endpoint to find");

        const SketchSnapPlacement Seeing =
            ResolveNearestSnap(Sketch, { 2.0, 0.0, 2.0 }, 10.0, {}, 10.0, Pending);
        Require(Seeing.Subject == SketchSnapSubject::Endpoint,
                "the placement in progress must offer its own anchors as endpoints");
        Require(std::fabs(Seeing.Position.Left) < 1.0e-9 &&
                std::fabs(Seeing.Position.Forward) < 1.0e-9,
                "the first loop must close exactly on the point it started from");
    }

    std::printf("[SketchDrawingProof] %u claims, %u failures\n", Claims, Failures);
    return Failures == 0u ? 0 : 1;
}
