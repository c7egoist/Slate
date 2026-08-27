//============================================================================================================================================
//                                                      TRANSFORMGIZMOPROOF.CPP
//============================================================================================================================================
// 🧩 Proves the handles the artist grabs are where they are drawn, at every zoom level, in both
//    projections.

#include "SlateWorkspace/Discipline/TransformGizmo/Api/TransformGizmo.h"

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

const SpatialBasis Ground = { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0 }, { 0.0, 1.0, 0.0 } };
const PlaneExtent  Panel  = { 0.0f, 0.0f, 800.0f, 600.0f };

ViewportStanding Ortho(double Scale)
{
    ViewportStanding View;
    View.Orientation = ViewportOrientation::Top;
    View.Focus       = { 0.0, 0.0, 0.0 };
    View.OrthoScale  = Scale;
    View.Distance    = 240.0;
    return View;
}

ViewportStanding Perspective(double Distance)
{
    ViewportStanding View;
    View.Orientation = ViewportOrientation::Isometric;
    View.Focus       = { 0.0, 0.0, 0.0 };
    View.OrthoScale  = 4.0;
    View.Distance    = Distance;
    return View;
}

//========================================================================================================
// 1. THE GIZMO IS A CONSTANT SIZE ON SCREEN
//========================================================================================================

// 🔴 THE DEFECT THIS UNIT EXISTS TO REMOVE. The host drew a 78-world-unit shaft and hit-tested a 44-pixel
//    one. They agree at one zoom level. Everywhere else the artist grabs empty space or misses the arrow.

void ProveConstantScreenSize()
{
    std::printf("\n1. The gizmo is the same size on screen at every zoom\n");

    const double Scales[] = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0 };

    for (double Scale : Scales)
    {
        GizmoScreenBasis Screen = {};
        Claim(ResolveGizmoScreenBasis(Ground, Ortho(Scale), false, Panel, { 0.0, 0.0, 0.0 }, Screen),
              "the gizmo stands");

        // The drawn arrow: the pixel table converted to world, then projected back.
        const SpatialPoint Origin = { 0.0, 0.0, 0.0 };
        const SpatialPoint Tip = Added(Origin,
                                       Scaled(Ground.Along, GizmoWorld(Screen, GizmoMeasure::ShaftEnd)));
        float TipX = 0.0f;
        float TipY = 0.0f;
        Claim(ProjectSpatialPoint(Ground, Ortho(Scale), false, Panel, Tip, TipX, TipY),
              "its arrow tip projects");

        const double DrawnPixels = std::sqrt((TipX - Screen.PivotX) * (TipX - Screen.PivotX)
                                           + (TipY - Screen.PivotY) * (TipY - Screen.PivotY));
        Claim(std::fabs(DrawnPixels - GizmoMeasure::ShaftEnd) < 0.5,
              "the DRAWN arrow is exactly as long as the table says, whatever the zoom");
    }

    // And the hit test agrees with the drawing at each of those zooms.
    for (double Scale : Scales)
    {
        GizmoScreenBasis Screen = {};
        ResolveGizmoScreenBasis(Ground, Ortho(Scale), false, Panel, { 0.0, 0.0, 0.0 }, Screen);

        // Halfway down the drawn shaft must grab the along arrow.
        const float MidX = Screen.PivotX + Screen.AlongX * static_cast<float>(GizmoMeasure::ShaftEnd * 0.5);
        const float MidY = Screen.PivotY + Screen.AlongY * static_cast<float>(GizmoMeasure::ShaftEnd * 0.5);
        Claim(ResolveGizmoHandle(Screen, TransformManner::Move, MidX, MidY) == GizmoHandle::MoveX,
              "pointing at the middle of the drawn arrow grabs that arrow");

        // Well beyond its drawn end must NOT.
        const float PastX = Screen.PivotX + Screen.AlongX * static_cast<float>(GizmoMeasure::ShaftEnd * 3.0);
        const float PastY = Screen.PivotY + Screen.AlongY * static_cast<float>(GizmoMeasure::ShaftEnd * 3.0);
        Claim(ResolveGizmoHandle(Screen, TransformManner::Move, PastX, PastY) == GizmoHandle::None,
              "pointing well past its end grabs nothing");
    }
}

//========================================================================================================
// 2. THE SAME UNDER PERSPECTIVE
//========================================================================================================

void ProvePerspective()
{
    std::printf("\n2. The same, under perspective, at every camera distance\n");

    for (double Distance : { 80.0, 160.0, 320.0, 640.0, 1280.0 })
    {
        GizmoScreenBasis Screen = {};
        Claim(ResolveGizmoScreenBasis(Ground, Perspective(Distance), true, Panel,
                                      { 0.0, 0.0, 0.0 }, Screen),
              "the gizmo stands under perspective");

        // 🔴 BOTH ARMS, NOT ONE. An oblique view foreshortens the two axes differently, and one
        //    world-per-pixel has to serve both. Measuring only the along arm cannot tell whether the
        //    other has overshot its own hit box.
        const SpatialPoint Origin = { 0.0, 0.0, 0.0 };
        for (int Arm = 0; Arm < 2; ++Arm)
        {
            const SpatialDirection Axis = Arm == 0 ? Ground.Along : Ground.Across;
            const SpatialPoint Tip = Added(Origin, Scaled(Axis, GizmoWorld(Screen, GizmoMeasure::ShaftEnd)));
            float TipX = 0.0f;
            float TipY = 0.0f;
            if (!ProjectSpatialPoint(Ground, Perspective(Distance), true, Panel, Tip, TipX, TipY))
                continue;

            const double DrawnPixels = std::sqrt((TipX - Screen.PivotX) * (TipX - Screen.PivotX)
                                               + (TipY - Screen.PivotY) * (TipY - Screen.PivotY));

            // ⚠️ NOT A SYMMETRIC BAND. Perspective foreshortens along the arrow itself, so a conversion
            //    read at the pivot cannot be exact at the far end — but the error must fall on the SHORT
            //    side. An arm drawn longer than the table says has its arrowhead outside its own hit box,
            //    and the artist points at the head and grabs nothing. Short is invisible; long is broken.
            Claim(DrawnPixels <= GizmoMeasure::ShaftEnd + 0.5,
                  "the drawn arm never OVERSHOOTS the length the hit test reaches to");
            Claim(DrawnPixels > GizmoMeasure::ShaftEnd * 0.85,
                  "and comes acceptably close to it");

            // The tip of what is drawn must actually grab the arm it belongs to.
            Claim(ResolveGizmoHandle(Screen, TransformManner::Move, TipX, TipY)
                  == (Arm == 0 ? GizmoHandle::MoveX : GizmoHandle::MoveZ),
                  "and pointing at the drawn arrowhead grabs that arm");
        }
    }

    // 📝 A NOTE ON A DEGENERACY THAT CANNOT ARISE, recorded because it looks as though it should.
    //    A plane viewed edge-on would project one of its axes to nothing, and dividing by that would
    //    report an enormous world-per-pixel. But `ResolveViewportFrame` derives the camera FROM the
    //    basis, so the view is always oriented relative to the plane and never edge-on to it — the two
    //    probes always project. Tilting a plane from 90 degrees down to a hundredth of a degree leaves
    //    both probes at exactly 96 pixels. The guard in the unit is cheap and stays, but the claim it
    //    defends is unreachable, and pretending otherwise would be a proof of nothing.
    {
        for (double Tilt : { 90.0, 30.0, 1.0, 0.01 })
        {
            const double Radians = Tilt * 3.14159265358979323846 / 180.0;
            const SpatialDirection Normal = Normalize(SpatialDirection{ 0.0, std::sin(Radians), std::cos(Radians) });
            const SpatialDirection Along  = { 1.0, 0.0, 0.0 };
            const SpatialBasis Tilted = { { 0.0, 0.0, 0.0 }, Along, Normalize(Cross(Normal, Along)), Normal };

            GizmoScreenBasis Screen = {};
            Claim(ResolveGizmoScreenBasis(Tilted, Ortho(4.0), false, Panel, { 0.0, 0.0, 0.0 }, Screen),
                  "a tilted plane still stands a gizmo");
            Claim(std::isfinite(Screen.WorldPerPixel) && Screen.WorldPerPixel > 0.0,
                  "with a finite positive ruler");
            Claim(Screen.WorldPerPixel < 100.0, "and a sane one, at every tilt");
        }
    }
}

//========================================================================================================
// 3. EVERY HANDLE IS REACHABLE
//========================================================================================================

// ⚠️ A handle drawn but unreachable is the same defect wearing different clothes. Each is aimed at where
//    the table puts it and must answer with itself.

void ProveHandlesReachable()
{
    std::printf("\n3. Every handle can actually be grabbed\n");

    GizmoScreenBasis Screen = {};
    ResolveGizmoScreenBasis(Ground, Ortho(4.0), false, Panel, { 0.0, 0.0, 0.0 }, Screen);

    // Move: two arrows, the diagonal square, and the centre nub.
    {
        const float AlongX = Screen.PivotX + Screen.AlongX * static_cast<float>(GizmoMeasure::ShaftEnd * 0.6);
        const float AlongY = Screen.PivotY + Screen.AlongY * static_cast<float>(GizmoMeasure::ShaftEnd * 0.6);
        Claim(ResolveGizmoHandle(Screen, TransformManner::Move, AlongX, AlongY) == GizmoHandle::MoveX,
              "the along arrow grabs");

        const float AcrossX = Screen.PivotX + Screen.AcrossX * static_cast<float>(GizmoMeasure::ShaftEnd * 0.6);
        const float AcrossY = Screen.PivotY + Screen.AcrossY * static_cast<float>(GizmoMeasure::ShaftEnd * 0.6);
        Claim(ResolveGizmoHandle(Screen, TransformManner::Move, AcrossX, AcrossY) == GizmoHandle::MoveZ,
              "the across arrow grabs");

        const float PlaneX = Screen.PivotX + (Screen.AlongX + Screen.AcrossX) * static_cast<float>(GizmoMeasure::PlaneOffset);
        const float PlaneY = Screen.PivotY + (Screen.AlongY + Screen.AcrossY) * static_cast<float>(GizmoMeasure::PlaneOffset);
        Claim(ResolveGizmoHandle(Screen, TransformManner::Move, PlaneX, PlaneY) == GizmoHandle::MoveFree,
              "the free-move square grabs");

        Claim(ResolveGizmoHandle(Screen, TransformManner::Move, Screen.PivotX, Screen.PivotY) == GizmoHandle::MoveFree,
              "the centre nub grabs a free move");
    }

    // Rotate: the rim, and not the empty middle of the ring.
    {
        const float RimX = Screen.PivotX + Screen.AlongX * static_cast<float>(GizmoMeasure::RingRadius);
        const float RimY = Screen.PivotY + Screen.AlongY * static_cast<float>(GizmoMeasure::RingRadius);
        Claim(ResolveGizmoHandle(Screen, TransformManner::Rotate, RimX, RimY) == GizmoHandle::Rotate,
              "the rotation ring grabs on its rim");

        const float InsideX = Screen.PivotX + Screen.AlongX * static_cast<float>(GizmoMeasure::RingRadius * 0.55);
        const float InsideY = Screen.PivotY + Screen.AlongY * static_cast<float>(GizmoMeasure::RingRadius * 0.55);
        Claim(ResolveGizmoHandle(Screen, TransformManner::Rotate, InsideX, InsideY) == GizmoHandle::None,
              "and not in the empty space inside it");
    }

    // Scale: the two end boxes and the centre.
    {
        const float BoxX = Screen.PivotX + Screen.AlongX * static_cast<float>(GizmoMeasure::ScaleBox);
        const float BoxY = Screen.PivotY + Screen.AlongY * static_cast<float>(GizmoMeasure::ScaleBox);
        Claim(ResolveGizmoHandle(Screen, TransformManner::Scale, BoxX, BoxY) == GizmoHandle::ScaleX,
              "the along scale box grabs");

        const float AcrossBoxX = Screen.PivotX + Screen.AcrossX * static_cast<float>(GizmoMeasure::ScaleBox);
        const float AcrossBoxY = Screen.PivotY + Screen.AcrossY * static_cast<float>(GizmoMeasure::ScaleBox);
        Claim(ResolveGizmoHandle(Screen, TransformManner::Scale, AcrossBoxX, AcrossBoxY) == GizmoHandle::ScaleZ,
              "the across scale box grabs");

        Claim(ResolveGizmoHandle(Screen, TransformManner::Scale, Screen.PivotX, Screen.PivotY) == GizmoHandle::ScaleFree,
              "the centre grabs a free scale");
    }

    // Far from everything.
    Claim(ResolveGizmoHandle(Screen, TransformManner::Move, Screen.PivotX + 400.0f, Screen.PivotY + 400.0f)
          == GizmoHandle::None, "a click far from the gizmo grabs nothing");
}

//========================================================================================================
// 4. NO PIXEL BELONGS TO TWO HANDLES
//========================================================================================================

// 🔴 THE STRONGEST CLAIM IN THIS FILE. The host's gizmo resolved overlaps by test order: the nub's reach
//    covered the root of both arrows, so which handle an artist got depended on which `if` came first.
//    That is a design where reordering two lines silently changes behaviour, and where "click the centre"
//    and "click the start of the X arrow" are the same pixel meaning two different things.
//
//    The measurements are laid out so the reaches do not touch. This walks the axis and the diagonal a
//    pixel at a time and checks that the answer only ever changes at a boundary the table predicts —
//    which is what makes the ordering of the arms in `ResolveGizmoHandle` irrelevant.

void ProveDisjointReaches()
{
    std::printf("\n4. No pixel belongs to two handles\n");

    GizmoScreenBasis Screen = {};
    ResolveGizmoScreenBasis(Ground, Ortho(4.0), false, Panel, { 0.0, 0.0, 0.0 }, Screen);

    // Walk out along the X axis. The nub gives way to nothing, and nothing gives way to the arrow.
    {
        bool NubNearPivot   = true;
        bool GapBetween     = true;
        bool ArrowOnShaft   = true;

        for (double Out = 0.0; Out <= GizmoMeasure::ShaftEnd + 12.0; Out += 0.5)
        {
            const float X = Screen.PivotX + Screen.AlongX * static_cast<float>(Out);
            const float Y = Screen.PivotY + Screen.AlongY * static_cast<float>(Out);
            const GizmoHandle Handle = ResolveGizmoHandle(Screen, TransformManner::Move, X, Y);

            if (Out < GizmoMeasure::CentreGrab - 0.5)
                NubNearPivot = NubNearPivot && Handle == GizmoHandle::MoveFree;
            else if (Out > GizmoMeasure::CentreGrab + 0.5 && Out < GizmoMeasure::ShaftStart - GizmoMeasure::ShaftGrab - 0.5)
                GapBetween = GapBetween && Handle == GizmoHandle::None;
            else if (Out > GizmoMeasure::ShaftStart + 0.5 && Out < GizmoMeasure::ShaftEnd - 0.5)
                ArrowOnShaft = ArrowOnShaft && Handle == GizmoHandle::MoveX;
        }

        Claim(NubNearPivot, "every pixel inside the nub answers the nub");
        Claim(GapBetween, "the pixels between the nub and the arrow answer nothing - the reaches do not touch");
        Claim(ArrowOnShaft, "every pixel along the drawn shaft answers the arrow");
    }

    // 🔴 The consequence: the arms can be tested in any order and the answer does not change. Proven by
    //    walking the whole neighbourhood of the gizmo and checking no position satisfies two reaches.
    {
        std::uint32_t Ambiguous = 0u;
        std::uint32_t Disagreed = 0u;
        std::uint32_t Probed    = 0u;

        for (float OffsetX = -80.0f; OffsetX <= 80.0f; OffsetX += 1.0f)
        for (float OffsetY = -80.0f; OffsetY <= 80.0f; OffsetY += 1.0f)
        {
            const float X = Screen.PivotX + OffsetX;
            const float Y = Screen.PivotY + OffsetY;
            ++Probed;

            // Count how many of the four Move reaches this position satisfies, independently of the
            // order `ResolveGizmoHandle` happens to test them in.
            const double CentreDistance = std::sqrt(static_cast<double>(OffsetX) * OffsetX
                                                  + static_cast<double>(OffsetY) * OffsetY);
            const bool InNub = CentreDistance <= GizmoMeasure::CentreGrab;

            const float PlaneX = (Screen.AlongX + Screen.AcrossX) * static_cast<float>(GizmoMeasure::PlaneOffset);
            const float PlaneY = (Screen.AlongY + Screen.AcrossY) * static_cast<float>(GizmoMeasure::PlaneOffset);
            const double LocalAlong  = (OffsetX - PlaneX) * Screen.AlongX  + (OffsetY - PlaneY) * Screen.AlongY;
            const double LocalAcross = (OffsetX - PlaneX) * Screen.AcrossX + (OffsetY - PlaneY) * Screen.AcrossY;
            const bool InSquare = std::fabs(LocalAlong) <= GizmoMeasure::PlaneHalf
                               && std::fabs(LocalAcross) <= GizmoMeasure::PlaneHalf;

            const auto OnShaft = [&](float DirX, float DirY)
            {
                const double Projected = OffsetX * DirX + OffsetY * DirY;
                const double Sideways  = OffsetX * -DirY + OffsetY * DirX;
                const double Clamped   = Projected < GizmoMeasure::ShaftStart ? GizmoMeasure::ShaftStart
                                       : (Projected > GizmoMeasure::ShaftEnd ? GizmoMeasure::ShaftEnd : Projected);
                const double Away      = Projected - Clamped;
                return std::sqrt(Away * Away + Sideways * Sideways) <= GizmoMeasure::ShaftGrab;
            };

            const bool OnAlong  = OnShaft(Screen.AlongX, Screen.AlongY);
            const bool OnAcross = OnShaft(Screen.AcrossX, Screen.AcrossY);
            const int Satisfied = (InNub ? 1 : 0) + (InSquare ? 1 : 0) + (OnAlong ? 1 : 0) + (OnAcross ? 1 : 0);
            if (Satisfied > 1)
                ++Ambiguous;

            // 🔴 And the unit agrees with the reaches worked out here independently. This is what turns
            //    "the measurements do not overlap" into "the function answers what the measurements say"
            //    — the table could be disjoint and the code could still read the wrong one.
            const GizmoHandle Answered = ResolveGizmoHandle(Screen, TransformManner::Move, X, Y);
            const GizmoHandle Expected = OnAlong    ? GizmoHandle::MoveX
                                       : OnAcross   ? GizmoHandle::MoveZ
                                       : (InNub || InSquare) ? GizmoHandle::MoveFree
                                                             : GizmoHandle::None;
            if (Answered != Expected)
                ++Disagreed;
        }

        Claim(Probed > 25000u, "the neighbourhood was actually walked");
        Claim(Ambiguous == 0u,
              "NO position in the whole neighbourhood satisfies two handles - the order cannot matter");
        Claim(Disagreed == 0u,
              "and at EVERY one of those positions the unit answers the handle the table predicts");
    }

    // And the same for the two scale boxes against the scale centre.
    {
        std::uint32_t Ambiguous = 0u;
        for (float OffsetX = -80.0f; OffsetX <= 80.0f; OffsetX += 1.0f)
        for (float OffsetY = -80.0f; OffsetY <= 80.0f; OffsetY += 1.0f)
        {
            const double CentreDistance = std::sqrt(static_cast<double>(OffsetX) * OffsetX
                                                  + static_cast<double>(OffsetY) * OffsetY);
            const auto NearBox = [&](float DirX, float DirY)
            {
                const double BoxX = DirX * GizmoMeasure::ScaleBox;
                const double BoxY = DirY * GizmoMeasure::ScaleBox;
                return std::sqrt((OffsetX - BoxX) * (OffsetX - BoxX) + (OffsetY - BoxY) * (OffsetY - BoxY))
                       <= GizmoMeasure::ScaleGrab;
            };
            const int Satisfied = (CentreDistance <= GizmoMeasure::CentreGrab ? 1 : 0)
                                + (NearBox(Screen.AlongX, Screen.AlongY) ? 1 : 0)
                                + (NearBox(Screen.AcrossX, Screen.AcrossY) ? 1 : 0);
            if (Satisfied > 1)
                ++Ambiguous;
        }
        Claim(Ambiguous == 0u, "the scale boxes and the scale centre never overlap either");
    }

    // ⚠️ The invariant the table must keep, asserted where a future edit will trip over it.
    Claim(GizmoMeasure::ShaftStart - GizmoMeasure::ShaftGrab > GizmoMeasure::CentreGrab,
          "the arrow's reach starts beyond the nub's - if this fails the nub becomes unreachable");
    Claim(GizmoMeasure::RingRadius - GizmoMeasure::RingGrab > GizmoMeasure::CentreGrab,
          "the rotation band starts beyond the nub's reach too");
}

//========================================================================================================
// 5. A HANDLE NAMES ITS MANNER AND ITS RESTRICTION
//========================================================================================================

void ProveHandleMeaning()
{
    std::printf("\n5. What grabbing a handle means\n");

    Claim(ResolveHandleManner(GizmoHandle::MoveX) == TransformManner::Move, "the X arrow moves");
    Claim(ResolveHandleRestriction(GizmoHandle::MoveX) == TransformRestriction::AxisX, "along X only");
    Claim(ResolveHandleManner(GizmoHandle::MoveZ) == TransformManner::Move, "the Z arrow moves");
    Claim(ResolveHandleRestriction(GizmoHandle::MoveZ) == TransformRestriction::AxisZ, "along Z only");
    Claim(ResolveHandleManner(GizmoHandle::MoveFree) == TransformManner::Move, "the square moves");
    Claim(ResolveHandleRestriction(GizmoHandle::MoveFree) == TransformRestriction::Free, "unrestricted");

    Claim(ResolveHandleManner(GizmoHandle::Rotate) == TransformManner::Rotate, "the ring rotates");
    Claim(ResolveHandleRestriction(GizmoHandle::Rotate) == TransformRestriction::Screen,
          "about the screen normal");

    Claim(ResolveHandleManner(GizmoHandle::ScaleX) == TransformManner::Scale, "the X box scales");
    Claim(ResolveHandleRestriction(GizmoHandle::ScaleX) == TransformRestriction::AxisX, "along X only");
    Claim(ResolveHandleManner(GizmoHandle::ScaleZ) == TransformManner::Scale, "the Z box scales");
    Claim(ResolveHandleRestriction(GizmoHandle::ScaleZ) == TransformRestriction::AxisZ, "along Z only");
    Claim(ResolveHandleManner(GizmoHandle::ScaleFree) == TransformManner::Scale, "the scale centre scales");
    Claim(ResolveHandleRestriction(GizmoHandle::ScaleFree) == TransformRestriction::Free, "in both axes");
}

} // namespace

int main()
{
    std::printf("=========================================================================\n");
    std::printf("TRANSFORM GIZMO PROOF\n");
    std::printf("=========================================================================\n");

    ProveConstantScreenSize();
    ProvePerspective();
    ProveHandlesReachable();
    ProveDisjointReaches();
    ProveHandleMeaning();

    std::printf("\n=========================================================================\n");
    std::printf("%u claims, %u failures -> %s\n", Claims, Failures,
                Failures == 0u ? "PROVEN" : "REFUTED");
    std::printf("=========================================================================\n");
    return Failures == 0u ? 0 : 1;
}
