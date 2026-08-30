/// 🧩 Proves the construction catalogue: bevel, chamfer, trim, cut, add.
///
/// 🔴 THE POINT OF THIS PROOF IS THAT THE OPERATIONS CHANGE THE GEOMETRY. `ApplyProfileCorner` was 214
///    working lines with no caller anywhere in the tree, and the edit tool's fillet arm only ever split
///    the curve and labelled the revision "Fillet Preparation" -- a name that admits it. Compiling is
///    not evidence. Every claim below measures the produced shape: how many curves came back, where the
///    original corner went, and whether the cut is a straight chamfer or a rounded bevel.
///
/// ⚠️ A corner that has been rounded no longer HAS its original vertex. That absence is the strongest
///    single signal available and most of these claims turn on it.

#include "SlateShape/Sketch/ProfileCorner/Api/ProfileCorner.h"
#include "SlateShape/Sketch/ProfileReshape/Api/ProfileReshape.h"
#include "SlateShape/Sketch/SketchStructure/Api/SketchStructure.h"
#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"
#include "SlateShape/Sketch/SketchPolyline/Api/SketchPolyline.h"
#include "SlateShape/Geometry/ProfileSpecification/Api/ProfileSpecification.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace Slate;

namespace
{
    int Claims = 0;
    int Failures = 0;

    void Claim(const std::string& What, bool Held)
    {
        ++Claims;
        if (!Held)
        {
            ++Failures;
            std::printf("  FAIL  %s\n", What.c_str());
        }
    }

    void ClaimNear(const std::string& What, double Seen, double Wanted, double Tolerance)
    {
        ++Claims;
        if (!(std::fabs(Seen - Wanted) <= Tolerance))
        {
            ++Failures;
            std::printf("  FAIL  %s (saw %.6f, wanted %.6f)\n", What.c_str(), Seen, Wanted);
        }
    }

    /// 🧩 A closed square loop of four lines, corners at the four combinations of +/- `Half`.
    /// note  📝 A square is the right fixture because every corner is a right angle of known position, so
    ///        a rounded or chamfered corner is unmistakable: the vertex is simply not there any more.
    struct SquareFixture
    {
        SketchStructure      Sketch;
        ProfileNameInFeature Profile = {};
        SketchCurveName      Bottom  = {};
        SpatialPoint         Corner  = {};   // [-] the +X +Y corner, where every test cuts
    };

    SpatialPoint At(double X, double Y)
    {
        SpatialPoint Point;
        Point.Left    = X;
        Point.Up      = Y;
        Point.Forward = 0.0;
        return Point;
    }

    SquareFixture DeclareSquare(double Half)
    {
        SquareFixture Fixture;

        // ⚠️ A SKETCH WITHOUT A PLANE IS NOT `Declared()`, and the corner evaluator refuses everything an
        //    undeclared sketch asks for. The first run of this proof failed all five corner claims for
        //    exactly that reason -- the fixture, not the solver.
        SketchPlane Ground;
        Ground.Origin         = At(0.0, 0.0);
        Ground.Normal         = { 0.0, 0.0, 1.0 };
        Ground.AlongDirection = { 1.0, 0.0, 0.0 };
        Fixture.Sketch.DeclarePlane(Ground);

        const SpatialPoint LowerLeft  = At(-Half, -Half);
        const SpatialPoint LowerRight = At(Half, -Half);
        const SpatialPoint UpperRight = At(Half, Half);
        const SpatialPoint UpperLeft  = At(-Half, Half);

        const SketchCurveName Bottom =
            Fixture.Sketch.DeclareCurve(CurveSpecification::DeclareLine(LowerLeft, LowerRight));
        const SketchCurveName Right =
            Fixture.Sketch.DeclareCurve(CurveSpecification::DeclareLine(LowerRight, UpperRight));
        const SketchCurveName Top =
            Fixture.Sketch.DeclareCurve(CurveSpecification::DeclareLine(UpperRight, UpperLeft));
        const SketchCurveName Left =
            Fixture.Sketch.DeclareCurve(CurveSpecification::DeclareLine(UpperLeft, LowerLeft));

        ProfilePlane Plane;
        Plane.Origin         = At(0.0, 0.0);
        Plane.Normal         = { 0.0, 0.0, 1.0 };
        Plane.AlongDirection = { 1.0, 0.0, 0.0 };

        ProfileLoop Loop;
        Loop.Orientation = ProfileLoopOrientation::Outer;
        Loop.Traversal.push_back({ { Bottom.IssuedIndex }, true });
        Loop.Traversal.push_back({ { Right.IssuedIndex }, true });
        Loop.Traversal.push_back({ { Top.IssuedIndex }, true });
        Loop.Traversal.push_back({ { Left.IssuedIndex }, true });

        ProfileSpecification Profile;
        Profile.DeclarePlane(Plane);
        Profile.DeclareLoop(Loop);

        Fixture.Profile = Fixture.Sketch.DeclareProfile(Profile);
        Fixture.Bottom  = Bottom;
        Fixture.Corner  = UpperRight;
        return Fixture;
    }

    /// 🧩 True when some curve of the loop still starts or ends at `Where`.
    /// 🧩 The two ends of a declared curve, read the same way the corner solver reads them.
    bool CurveEnds(const SketchStructure& Sketch, std::uint32_t Issued,
                   SpatialPoint& Start, SpatialPoint& End)
    {
        if (Issued == 0u || Issued > Sketch.Curves().size())
            return false;
        std::vector<SpatialPoint> Polyline;
        AppendCurvePolyline(Sketch.Curves()[Issued - 1u].Geometry, Polyline, 96u);
        if (Polyline.size() < 2u)
            return false;
        Start = Polyline.front();
        End   = Polyline.back();
        return true;
    }

    bool LoopVisits(const SketchStructure& Sketch, ProfileNameInFeature Profile, const SpatialPoint& Where)
    {
        const ProfileSpecification& Held = Sketch.Profiles()[Profile.IssuedIndex - 1u];
        for (const ProfileCurveUse& Use : Held.HeldLoops()[0].Traversal)
        {
            SpatialPoint Start;
            SpatialPoint End;
            if (!CurveEnds(Sketch, Use.TraversedCurve.IssuedIndex, Start, End))
                continue;
            if (LengthSquared(Difference(Start, Where)) < 1.0e-6)
                return true;
            if (LengthSquared(Difference(End, Where)) < 1.0e-6)
                return true;
        }
        return false;
    }

    std::size_t LoopSize(const SketchStructure& Sketch, ProfileNameInFeature Profile)
    {
        return Sketch.Profiles()[Profile.IssuedIndex - 1u].HeldLoops()[0].Traversal.size();
    }

    /// 🧩 How many curves of the loop are CURVED, measured rather than asked.
    /// note  📐 A straight curve's midpoint lies on the chord between its ends; a rounded one bulges off
    ///        it. Measuring the bulge tells a bevel from a chamfer without depending on how the solver
    ///        chose to represent the arc, which is the property actually under test.
    std::size_t LoopArcCount(const SketchStructure& Sketch, ProfileNameInFeature Profile)
    {
        const ProfileSpecification& Held = Sketch.Profiles()[Profile.IssuedIndex - 1u];
        std::size_t Arcs = 0u;
        for (const ProfileCurveUse& Use : Held.HeldLoops()[0].Traversal)
        {
            const std::uint32_t Issued = Use.TraversedCurve.IssuedIndex;
            if (Issued == 0u || Issued > Sketch.Curves().size())
                continue;

            std::vector<SpatialPoint> Polyline;
            AppendCurvePolyline(Sketch.Curves()[Issued - 1u].Geometry, Polyline, 96u);
            if (Polyline.size() < 3u)
                continue;

            const SpatialPoint Start  = Polyline.front();
            const SpatialPoint End    = Polyline.back();
            const SpatialPoint Middle = Polyline[Polyline.size() / 2u];

            const SpatialPoint Chord = { (Start.Left + End.Left) * 0.5,
                                         (Start.Up + End.Up) * 0.5,
                                         (Start.Forward + End.Forward) * 0.5 };
            if (LengthSquared(Difference(Middle, Chord)) > 1.0e-8)
                ++Arcs;
        }
        return Arcs;
    }
}

int main()
{
    std::printf("Construction tool proof\n");

    // ① A BEVEL ROUNDS THE CORNER AWAY.
    {
        SquareFixture Fixture = DeclareSquare(10.0);
        Claim("① the square starts with four curves", LoopSize(Fixture.Sketch, Fixture.Profile) == 4u);
        Claim("① the corner is present before bevelling",
              LoopVisits(Fixture.Sketch, Fixture.Profile, Fixture.Corner));
        Claim("① the square is all straight lines before bevelling",
              LoopArcCount(Fixture.Sketch, Fixture.Profile) == 0u);

        // 📝 Corner 2 is the joint before traversal entry 2, i.e. the end of the right edge: +X +Y.
        const Deliver<ProfileNameInFeature> Produced =
            ApplyProfileCorner(Fixture.Sketch, Fixture.Profile, 0u, 2u, 3.0, false);

        Claim("① the bevel is produced", Produced.Resolved);
        if (Produced.Resolved)
        {
            Claim("① the bevel adds one curve to the loop",
                  LoopSize(Fixture.Sketch, Produced.Resolve()) == 5u);
            Claim("① 🔴 the original corner is GONE",
                  !LoopVisits(Fixture.Sketch, Produced.Resolve(), Fixture.Corner));
            Claim("① a bevel introduces an arc",
                  LoopArcCount(Fixture.Sketch, Produced.Resolve()) == 1u);
        }
    }

    // ② A CHAMFER CUTS THE CORNER OFF FLAT.
    {
        SquareFixture Fixture = DeclareSquare(10.0);
        const Deliver<ProfileNameInFeature> Produced =
            ApplyProfileCorner(Fixture.Sketch, Fixture.Profile, 0u, 2u, 3.0, true);

        Claim("② the chamfer is produced", Produced.Resolved);
        if (Produced.Resolved)
        {
            Claim("② the chamfer adds one curve to the loop",
                  LoopSize(Fixture.Sketch, Produced.Resolve()) == 5u);
            Claim("② 🔴 the original corner is GONE",
                  !LoopVisits(Fixture.Sketch, Produced.Resolve(), Fixture.Corner));
            // ⚠️ This is the claim that separates chamfer from bevel. Without it both tools could be
            //    wired to the same arm and the proof would still pass.
            Claim("② 🔴 a chamfer introduces NO arc -- it is a straight cut",
                  LoopArcCount(Fixture.Sketch, Produced.Resolve()) == 0u);
        }
    }

    // ③ THE CUT LANDS WHERE THE RADIUS SAYS.
    {
        SquareFixture Fixture = DeclareSquare(10.0);
        const Deliver<ProfileNameInFeature> Produced =
            ApplyProfileCorner(Fixture.Sketch, Fixture.Profile, 0u, 2u, 4.0, true);

        Claim("③ the chamfer is produced", Produced.Resolved);
        if (Produced.Resolved)
        {
            // 📐 A 4.0 chamfer on a right angle leaves the edges at 4.0 back from the vertex.
            Claim("③ the loop now visits the point 4.0 down the right edge",
                  LoopVisits(Fixture.Sketch, Produced.Resolve(), At(10.0, 6.0)));
            Claim("③ the loop now visits the point 4.0 along the top edge",
                  LoopVisits(Fixture.Sketch, Produced.Resolve(), At(6.0, 10.0)));
            Claim("③ and the vertex between them is gone",
                  !LoopVisits(Fixture.Sketch, Produced.Resolve(), At(10.0, 10.0)));
        }
    }

    // ④ A RADIUS THAT DOES NOT FIT IS REFUSED, NOT CLAMPED.
    {
        SquareFixture Fixture = DeclareSquare(10.0);
        const Deliver<ProfileNameInFeature> TooBig =
            ApplyProfileCorner(Fixture.Sketch, Fixture.Profile, 0u, 2u, 40.0, false);

        Claim("④ 🔴 a radius larger than the edges REFUSES", !TooBig.Resolved);
        Claim("④ and the square is left untouched", LoopSize(Fixture.Sketch, Fixture.Profile) == 4u);
        Claim("④ with its corner still in place",
              LoopVisits(Fixture.Sketch, Fixture.Profile, Fixture.Corner));

        const Deliver<ProfileNameInFeature> Zero =
            ApplyProfileCorner(Fixture.Sketch, Fixture.Profile, 0u, 2u, 0.0, false);
        Claim("④ a zero radius refuses too", !Zero.Resolved);

        const Deliver<ProfileNameInFeature> Negative =
            ApplyProfileCorner(Fixture.Sketch, Fixture.Profile, 0u, 2u, -3.0, false);
        Claim("④ a negative radius refuses", !Negative.Resolved);
    }

    // ⑤ A CORNER INDEX OFF THE END IS REFUSED.
    {
        SquareFixture Fixture = DeclareSquare(10.0);
        const Deliver<ProfileNameInFeature> Missing =
            ApplyProfileCorner(Fixture.Sketch, Fixture.Profile, 0u, 9u, 2.0, false);
        Claim("⑤ a corner index past the loop refuses", !Missing.Resolved);

        const Deliver<ProfileNameInFeature> NoLoop =
            ApplyProfileCorner(Fixture.Sketch, Fixture.Profile, 7u, 0u, 2.0, false);
        Claim("⑤ a loop index past the profile refuses", !NoLoop.Resolved);
    }

    // ⑥ THE RESOLVER TURNS A SELECTED CURVE AND A CLICK INTO A CORNER.
    //    🔴 This is the seam that did not exist. The artist selects a CURVE; the solver wants a CORNER.
    {
        SquareFixture Fixture = DeclareSquare(10.0);

        // 📝 Clicking near the +X +Y end of the bottom edge's loop should find the nearest corner to the
        //    probe among the corners of the loop that uses the selected curve.
        const Deliver<ProfileCornerTarget> NearLowerRight =
            ResolveProfileCornerNear(Fixture.Sketch, Fixture.Bottom, At(9.5, -9.5));
        Claim("⑥ the resolver finds a corner for a curve in a loop", NearLowerRight.Resolved);
        if (NearLowerRight.Resolved)
        {
            ClaimNear("⑥ it picks the corner nearest the probe (x)",
                      NearLowerRight.Resolve().Position.Left, 10.0, 1.0e-9);
            ClaimNear("⑥ it picks the corner nearest the probe (y)",
                      NearLowerRight.Resolve().Position.Up, -10.0, 1.0e-9);
        }

        const Deliver<ProfileCornerTarget> NearUpperLeft =
            ResolveProfileCornerNear(Fixture.Sketch, Fixture.Bottom, At(-9.0, 9.0));
        Claim("⑥ a different probe finds a different corner", NearUpperLeft.Resolved);
        if (NearUpperLeft.Resolved && NearLowerRight.Resolved)
        {
            Claim("⑥ 🔴 the corner INDEX actually changes with the probe",
                  NearUpperLeft.Resolve().CornerIndex != NearLowerRight.Resolve().CornerIndex);
            ClaimNear("⑥ and it is the upper-left corner (x)",
                      NearUpperLeft.Resolve().Position.Left, -10.0, 1.0e-9);
            ClaimNear("⑥ and it is the upper-left corner (y)",
                      NearUpperLeft.Resolve().Position.Up, 10.0, 1.0e-9);
        }

        // ⚠️ The index the resolver returns must be the index the solver accepts. If these two disagree
        //    the tool rounds the wrong corner and it looks like a geometry bug.
        if (NearLowerRight.Resolved)
        {
            const Deliver<ProfileNameInFeature> Produced =
                ApplyProfileCorner(Fixture.Sketch, NearLowerRight.Resolve().Profile,
                                   NearLowerRight.Resolve().LoopIndex,
                                   NearLowerRight.Resolve().CornerIndex, 3.0, true);
            Claim("⑥ 🔴 the resolved corner is accepted by the solver", Produced.Resolved);
            if (Produced.Resolved)
                Claim("⑥ 🔴 and it cuts the corner the probe was nearest",
                      !LoopVisits(Fixture.Sketch, Produced.Resolve(), At(10.0, -10.0)));
        }
    }

    // ⑦ A CURVE IN NO LOOP IS REFUSED BY THE RESOLVER.
    {
        SquareFixture Fixture = DeclareSquare(10.0);
        const SketchCurveName Loose =
            Fixture.Sketch.DeclareCurve(CurveSpecification::DeclareLine(At(40.0, 40.0), At(50.0, 40.0)));

        const Deliver<ProfileCornerTarget> Nothing =
            ResolveProfileCornerNear(Fixture.Sketch, Loose, At(45.0, 40.0));
        Claim("⑦ 🔴 a loose curve belongs to no loop and the resolver REFUSES", !Nothing.Resolved);

        const Deliver<ProfileCornerTarget> Unnamed =
            ResolveProfileCornerNear(Fixture.Sketch, {}, At(0.0, 0.0));
        Claim("⑦ an unassigned curve name refuses", !Unnamed.Resolved);
    }

    // ⑧ CUT KEEPS BOTH HALVES; TRIM DOES NOT.
    //    📐 This is the whole distinction between the two rows in the menu.
    {
        SquareFixture Fixture = DeclareSquare(10.0);

        const Deliver<std::vector<SketchCurveName>> Divided =
            CutCurve(Fixture.Sketch, Fixture.Bottom, At(0.0, -10.0));
        Claim("⑧ cutting a curve at an interior point resolves", Divided.Resolved);
        if (Divided.Resolved)
            Claim("⑧ 🔴 cut keeps BOTH halves", Divided.Resolve().size() == 2u);

        // 📐 Trim returns ONE curve where cut returned two, and which one it keeps is the caller's
        //    choice. That single-versus-double return IS the difference between the two menu rows.
        SquareFixture Kept = DeclareSquare(10.0);
        const Deliver<SketchCurveName> KeepStart =
            TrimCurve(Kept.Sketch, Kept.Bottom, At(0.0, -10.0), true);
        Claim("⑧ trimming resolves", KeepStart.Resolved);
        Claim("⑧ 🔴 trim returns ONE piece where cut returned two", KeepStart.Resolved);

        if (KeepStart.Resolved)
        {
            SpatialPoint Start;
            SpatialPoint End;
            Claim("⑧ the kept piece is readable",
                  CurveEnds(Kept.Sketch, KeepStart.Resolve().IssuedIndex, Start, End));
            // ⚠️ Keeping the start means the piece runs from the original start to the probe.
            ClaimNear("⑧ the kept piece starts at the original start", Start.Left, -10.0, 1.0e-6);
            ClaimNear("⑧ and stops at the probe", End.Left, 0.0, 1.0e-6);
        }

        SquareFixture Other = DeclareSquare(10.0);
        const Deliver<SketchCurveName> KeepEnd =
            TrimCurve(Other.Sketch, Other.Bottom, At(0.0, -10.0), false);
        Claim("⑧ trimming the other side resolves", KeepEnd.Resolved);
        if (KeepEnd.Resolved)
        {
            SpatialPoint Start;
            SpatialPoint End;
            if (CurveEnds(Other.Sketch, KeepEnd.Resolve().IssuedIndex, Start, End))
            {
                // 🔴 If `KeepStart` were ignored both calls would return the same piece and trim would
                //    only ever cut one way.
                ClaimNear("⑧ 🔴 keeping the END gives the OTHER half", Start.Left, 0.0, 1.0e-6);
                ClaimNear("⑧ which runs to the original terminus", End.Left, 10.0, 1.0e-6);
            }
        }
    }

    // ⑨ BEVELLING TWICE CUTS TWO DIFFERENT CORNERS.
    //    ⚠️ An operation that silently re-cuts the same corner would still pass every single-shot claim.
    {
        SquareFixture Fixture = DeclareSquare(10.0);

        const Deliver<ProfileNameInFeature> First =
            ApplyProfileCorner(Fixture.Sketch, Fixture.Profile, 0u, 2u, 2.0, true);
        Claim("⑨ the first chamfer is produced", First.Resolved);

        if (First.Resolved)
        {
            const Deliver<ProfileCornerTarget> NextCorner =
                ResolveProfileCornerNear(Fixture.Sketch, Fixture.Bottom, At(-9.5, -9.5));
            Claim("⑨ a second corner resolves on the reshaped loop", NextCorner.Resolved);

            if (NextCorner.Resolved)
            {
                const Deliver<ProfileNameInFeature> Second =
                    ApplyProfileCorner(Fixture.Sketch, NextCorner.Resolve().Profile,
                                       NextCorner.Resolve().LoopIndex,
                                       NextCorner.Resolve().CornerIndex, 2.0, true);
                Claim("⑨ the second chamfer is produced", Second.Resolved);
                if (Second.Resolved)
                {
                    Claim("⑨ 🔴 the loop has grown twice", LoopSize(Fixture.Sketch, Second.Resolve()) == 6u);
                    Claim("⑨ both original corners are gone",
                          !LoopVisits(Fixture.Sketch, Second.Resolve(), At(10.0, 10.0)) &&
                          !LoopVisits(Fixture.Sketch, Second.Resolve(), At(-10.0, -10.0)));
                    Claim("⑨ and the untouched corners remain",
                          LoopVisits(Fixture.Sketch, Second.Resolve(), At(10.0, -10.0)) &&
                          LoopVisits(Fixture.Sketch, Second.Resolve(), At(-10.0, 10.0)));
                }
            }
        }
    }

    // ⑩ THE LOOP STAYS CLOSED AFTER EVERY OPERATION.
    //    🔴 A profile whose loop no longer joins end to end cannot be extruded. An operation that leaves
    //       a gap is worse than one that refuses.
    {
        SquareFixture Fixture = DeclareSquare(10.0);
        const Deliver<ProfileNameInFeature> Produced =
            ApplyProfileCorner(Fixture.Sketch, Fixture.Profile, 0u, 2u, 3.0, false);

        if (Produced.Resolved)
        {
            const ProfileSpecification& Held =
                Fixture.Sketch.Profiles()[Produced.Resolve().IssuedIndex - 1u];
            const std::vector<ProfileCurveUse>& Traversal = Held.HeldLoops()[0].Traversal;

            bool Continuous = true;
            for (std::size_t Index = 0u; Index < Traversal.size(); ++Index)
            {
                const std::size_t NextIndex = (Index + 1u) % Traversal.size();

                SpatialPoint HereStart;
                SpatialPoint HereEnd;
                SpatialPoint NextStart;
                SpatialPoint NextEnd;
                if (!CurveEnds(Fixture.Sketch, Traversal[Index].TraversedCurve.IssuedIndex,
                               HereStart, HereEnd) ||
                    !CurveEnds(Fixture.Sketch, Traversal[NextIndex].TraversedCurve.IssuedIndex,
                               NextStart, NextEnd))
                {
                    Continuous = false;
                    break;
                }

                // ⚠️ Sense matters: a loop may traverse a curve backwards, and then its "end" is the
                //    point the geometry calls the start.
                const SpatialPoint Leaving  = Traversal[Index].SameSense ? HereEnd : HereStart;
                const SpatialPoint Arriving = Traversal[NextIndex].SameSense ? NextStart : NextEnd;
                if (LengthSquared(Difference(Leaving, Arriving)) > 1.0e-6)
                    Continuous = false;
            }

            Claim("⑩ 🔴 the bevelled loop is still CLOSED end to end", Continuous);
        }
    }

    std::printf("%d claims, %d failures\n", Claims, Failures);
    return Failures == 0 ? 0 : 1;
}
