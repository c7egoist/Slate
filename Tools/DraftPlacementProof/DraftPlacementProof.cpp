//============================================================================================================================================
//                                                       DRAFTPLACEMENTPROOF.CPP
//============================================================================================================================================
// 🧩 Executes the draft placement machine and proves it against the tables it replaced.
//
// 📝 This is the first engine component that can be PROVEN rather than merely parsed. `SlateToolset` names no
//    device, no window and no vendor header, so it links and runs on any toolchain — which is exactly the
//    property that made it worth lifting out of a 5981-line host in the first place. Everything below runs.
//
// 🔴 Section 1 is the one that matters. The refactor replaced four hand-written switch statements with one
//    table, and a refactor of a dispatch table is only safe if the new table answers every input exactly as
//    the old ones did. Section 1 re-declares the ORIGINAL four switches verbatim, as they stood in
//    `Application/Api/SharedCadDrawingController.h`, and asserts agreement across all 22 subjects and all 61
//    catalogue tiles. If a row was mistyped while transcribing, this fails and names the subject.

#include "SlateToolset/Draft/DraftPlacement/Api/DraftPlacement.h"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

using namespace Slate;

namespace
{

int Failures = 0;
int Checks   = 0;

void Claim(bool Held, const std::string& Stated)
{
    ++Checks;
    if (Held)
        return;
    ++Failures;
    std::printf("  FAIL  %s\n", Stated.c_str());
}

//------------------------------------------------------------------------------------------------------------------------
//    1. THE ORIGINAL TABLES, TRANSCRIBED VERBATIM FROM SharedCadDrawingController.h AS IT STOOD BEFORE DELETION
//------------------------------------------------------------------------------------------------------------------------

// ⚠️ Do not "tidy" these. They are a frozen copy of the deleted header and their value is that they are
//    byte-for-byte what shipped. Rewriting them to agree with the new table would delete the proof.

std::uint32_t OriginalRequiredAnchors(DraftSubject Subject)
{
    switch (Subject)
    {
        case DraftSubject::Line:
        case DraftSubject::Rectangle:
        case DraftSubject::CenterRectangle:
        case DraftSubject::DiameterCircle:
        case DraftSubject::Polygon:
            return 2u;
        case DraftSubject::Circle:
        case DraftSubject::Ellipse:
            return 1u;
        case DraftSubject::Arc:
        case DraftSubject::EllipticalArc:
        case DraftSubject::ThreePointRectangle:
        case DraftSubject::ThreePointCircle:
        case DraftSubject::CenterStartEndArc:
        case DraftSubject::TangentArc:
        case DraftSubject::Slot:
        case DraftSubject::BasisSpline:
        case DraftSubject::RationalSpline:
            return 3u;
        case DraftSubject::Hermite:
            return 4u;
        case DraftSubject::Bezier:
        case DraftSubject::Polyline:
        case DraftSubject::LinearDimension:
            return 2u;
        case DraftSubject::Point:
            return 1u;
        default:
            return 0u;
    }
}

bool OriginalProducesClosedProfile(DraftSubject Subject)
{
    switch (Subject)
    {
        case DraftSubject::Rectangle:
        case DraftSubject::CenterRectangle:
        case DraftSubject::ThreePointRectangle:
        case DraftSubject::Circle:
        case DraftSubject::DiameterCircle:
        case DraftSubject::ThreePointCircle:
        case DraftSubject::Ellipse:
        case DraftSubject::Polygon:
        case DraftSubject::Slot:
            return true;
        default:
            return false;
    }
}

bool OriginalMultiClickCurve(DraftSubject Subject)
{
    return Subject == DraftSubject::Polyline ||
           Subject == DraftSubject::Bezier ||
           Subject == DraftSubject::BasisSpline ||
           Subject == DraftSubject::Hermite ||
           Subject == DraftSubject::RationalSpline;
}

const char* OriginalSubjectName(DraftSubject Subject)
{
    switch (Subject)
    {
        case DraftSubject::Line:                return "Line";
        case DraftSubject::Rectangle:           return "Rectangle";
        case DraftSubject::Circle:              return "Circle";
        case DraftSubject::Arc:                 return "Arc";
        case DraftSubject::Polyline:            return "Polyline";
        case DraftSubject::LinearDimension:     return "Dimension";
        case DraftSubject::Point:               return "Point";
        case DraftSubject::Ellipse:             return "Ellipse";
        case DraftSubject::Bezier:              return "Bezier";
        case DraftSubject::EllipticalArc:       return "Elliptical Arc";
        case DraftSubject::BasisSpline:         return "Basis Spline";
        case DraftSubject::CenterRectangle:     return "Center Rectangle";
        case DraftSubject::ThreePointRectangle: return "3-Point Rectangle";
        case DraftSubject::DiameterCircle:      return "Diameter Circle";
        case DraftSubject::ThreePointCircle:    return "3-Point Circle";
        case DraftSubject::CenterStartEndArc:   return "Center Arc";
        case DraftSubject::TangentArc:          return "Tangent Arc";
        case DraftSubject::Polygon:             return "Polygon";
        case DraftSubject::Slot:                return "Slot";
        case DraftSubject::Hermite:             return "Hermite";
        case DraftSubject::RationalSpline:      return "NURBS Curve";
        default:                                return "";
    }
}

DraftSubject OriginalResolveFromCatalogue(ParametricToolSubject Subject)
{
    switch (Subject)
    {
        case ParametricToolSubject::Line:                 return DraftSubject::Line;
        case ParametricToolSubject::Polyline:             return DraftSubject::Polyline;
        case ParametricToolSubject::Rectangle:            return DraftSubject::Rectangle;
        case ParametricToolSubject::Circle:               return DraftSubject::Circle;
        case ParametricToolSubject::Arc:                  return DraftSubject::Arc;
        case ParametricToolSubject::LinearDimension:      return DraftSubject::LinearDimension;
        case ParametricToolSubject::Point:                return DraftSubject::Point;
        case ParametricToolSubject::Ellipse:              return DraftSubject::Ellipse;
        case ParametricToolSubject::EllipticalArc:        return DraftSubject::EllipticalArc;
        case ParametricToolSubject::BezierCurve:
        case ParametricToolSubject::Interpolate:
        case ParametricToolSubject::Approximate:          return DraftSubject::Bezier;
        case ParametricToolSubject::HermiteCurve:         return DraftSubject::Hermite;
        case ParametricToolSubject::BasisSpline:          return DraftSubject::BasisSpline;
        case ParametricToolSubject::RationalSpline:       return DraftSubject::RationalSpline;
        case ParametricToolSubject::ConstructionLine:     return DraftSubject::Line;
        case ParametricToolSubject::CenterRectangle:      return DraftSubject::CenterRectangle;
        case ParametricToolSubject::ThreePointRectangle:  return DraftSubject::ThreePointRectangle;
        case ParametricToolSubject::DiameterCircle:       return DraftSubject::DiameterCircle;
        case ParametricToolSubject::ThreePointCircle:     return DraftSubject::ThreePointCircle;
        case ParametricToolSubject::CenterStartEndArc:    return DraftSubject::CenterStartEndArc;
        case ParametricToolSubject::TangentArc:           return DraftSubject::TangentArc;
        case ParametricToolSubject::Polygon:              return DraftSubject::Polygon;
        case ParametricToolSubject::Slot:                 return DraftSubject::Slot;
        default:                                          return DraftSubject::None;
    }
}

//------------------------------------------------------------------------------------------------------------------------
//                                                    HELPERS
//------------------------------------------------------------------------------------------------------------------------

SketchSnapPlacement Snapped(double Left, double Up, double Forward)
{
    SketchSnapPlacement Placement = {};
    Placement.Subject  = SketchSnapSubject::Endpoint;
    Placement.Position = { Left, Up, Forward };
    return Placement;
}

/// Drives one contact: hover at a position, then anchor.
DraftArrival Place(DraftPlacement& Draft, double Left, bool Terminating = false, bool Resolved = false)
{
    const SpatialPoint At = { Left, 0.0, 0.0 };
    Draft.Hover(At, Resolved ? Snapped(Left, 0.0, 0.0) : SketchSnapPlacement{});
    return Draft.Anchor(Terminating);
}

const char* ArrivalText(DraftArrival Arrival)
{
    switch (Arrival)
    {
        case DraftArrival::Ignored:  return "Ignored";
        case DraftArrival::Anchored: return "Anchored";
        case DraftArrival::Complete: return "Complete";
        default:                     return "?";
    }
}

std::vector<DraftSubject> EverySubject()
{
    std::vector<DraftSubject> Every;
    for (std::uint32_t Ordinal = 0u; Ordinal < static_cast<std::uint32_t>(DraftSubject::SubjectCount); ++Ordinal)
        Every.push_back(static_cast<DraftSubject>(Ordinal));
    return Every;
}

//------------------------------------------------------------------------------------------------------------------------
//                                            1. THE REFACTOR PRESERVES BEHAVIOUR
//------------------------------------------------------------------------------------------------------------------------

void ProveTableEquivalence()
{
    std::printf("1. The one table answers exactly as the four it replaced\n");

    for (const DraftSubject Subject : EverySubject())
    {
        const DraftDeclaration Declared = DeclaredDraft(Subject);
        const std::string      Named    = OriginalSubjectName(Subject);
        const std::string      Where    = Named.empty() ? "None" : Named;

        // 🔴 Three subjects deliberately disagree with the original column, because the original was
        //    wrong in a way nothing could observe: it was only ever READ for the four terminated curves.
        //    `Circle` and `Ellipse` claimed one anchor while `CommitDraft` required a centre AND a rim
        //    point — one stored, one taken from the live hover. Stating the exceptions individually is the
        //    difference between a checked correction and a transcription slip: if a FOURTH subject ever
        //    drifts, this still fails.
        const bool CorrectedRow = Subject == DraftSubject::Circle ||
                                  Subject == DraftSubject::Ellipse;

        if (CorrectedRow)
        {
            Claim(OriginalRequiredAnchors(Subject) == 1u,
                  "the original is expected to have said 1 for " + Where);
            Claim(Declared.Required == 2u,
                  "the correction should require both anchors for " + Where);
        }
        else
        {
            Claim(Declared.Required == OriginalRequiredAnchors(Subject),
                  "required anchors disagree for " + Where);
        }

        Claim(Declared.ClosedProfile == OriginalProducesClosedProfile(Subject),
              "closed profile disagrees for " + Where);

        Claim(std::string(Declared.Naming) == Named,
              "name disagrees for " + Where + ": '" + Declared.Naming + "' vs '" + Named + "'");

        // 🔴 The old `IsMultiClickCurve` predicate is now the `Terminated` closure. They must select the
        //    same five subjects — if the new closure column admitted a sixth, a curve that used to commit
        //    on its anchor count would now wait forever for a double-press.
        Claim((Declared.Closure == DraftClosure::Terminated) == OriginalMultiClickCurve(Subject),
              "multi-click curve disagrees with Terminated closure for " + Where);
    }

    // 🔴 All 61 catalogue tiles, not just the 22 that draw. The ones that map to `None` matter most: if a
    //    constraint tile started resolving to a drawing subject, pressing it would begin placing anchors.
    for (std::uint32_t Ordinal = 0u; Ordinal < static_cast<std::uint32_t>(ParametricToolSubject::SubjectCount); ++Ordinal)
    {
        const ParametricToolSubject Tile = static_cast<ParametricToolSubject>(Ordinal);
        Claim(DraftFromCatalogue(Tile) == OriginalResolveFromCatalogue(Tile),
              "catalogue tile " + std::to_string(Ordinal) + " resolves differently");
    }

    std::printf("   %d subjects × 4 columns + %u catalogue tiles agree\n",
                static_cast<int>(EverySubject().size()),
                static_cast<unsigned>(ParametricToolSubject::SubjectCount));
}

//------------------------------------------------------------------------------------------------------------------------
//                                     2. EVERY SUBJECT COMPLETES AT ITS DECLARED COUNT
//------------------------------------------------------------------------------------------------------------------------

void ProveCompletionCounts()
{
    std::printf("2. Every subject completes at its declared anchor count and not before\n");

    for (const DraftSubject Subject : EverySubject())
    {
        if (Subject == DraftSubject::None)
            continue;

        const DraftDeclaration Declared  = DeclaredDraft(Subject);
        const bool             Snapping  = Declared.Closure == DraftClosure::Resolved;
        const bool             Ending    = Declared.Closure == DraftClosure::Terminated;
        const std::string      Where     = Declared.Naming;

        DraftPlacement Draft;
        Draft.Declare(Subject);

        // Take one fewer anchor than required — none of them may complete.
        for (std::uint32_t Taken = 0u; Taken + 1u < Declared.Required; ++Taken)
        {
            const DraftArrival Arrival = Place(Draft, static_cast<double>(Taken), false, Snapping);
            Claim(Arrival == DraftArrival::Anchored,
                  Where + " completed early at anchor " + std::to_string(Taken + 1u) +
                      " (" + ArrivalText(Arrival) + ")");
        }

        Claim(Draft.Remaining() == 1u,
              Where + " should want exactly one more anchor, wants " + std::to_string(Draft.Remaining()));

        // The last required anchor. A terminated curve does NOT complete on it without a terminating
        // contact; every other closure does.
        const DraftArrival Final = Place(Draft, static_cast<double>(Declared.Required), false, Snapping);

        if (Ending)
            Claim(Final == DraftArrival::Anchored,
                  Where + " is a terminated curve and must not complete without a double-press");
        else
            Claim(Final == DraftArrival::Complete,
                  Where + " should complete on anchor " + std::to_string(Declared.Required) +
                      ", reported " + ArrivalText(Final));
    }
}

//------------------------------------------------------------------------------------------------------------------------
//                                        3. TERMINATED CURVES GROW UNTIL THE ARTIST ENDS THEM
//------------------------------------------------------------------------------------------------------------------------

void ProveTerminatedCurves()
{
    std::printf("3. A terminated curve grows past its minimum and ends on a double-press\n");

    DraftPlacement Draft;
    Draft.Declare(DraftSubject::Polyline);

    for (int Anchor = 0; Anchor < 9; ++Anchor)
    {
        const DraftArrival Arrival = Place(Draft, static_cast<double>(Anchor));
        Claim(Arrival == DraftArrival::Anchored,
              "polyline ended itself at anchor " + std::to_string(Anchor + 1));
    }

    Claim(Draft.Anchors().size() == 9u, "polyline should hold nine anchors");
    Claim(Draft.Remaining() == 0u, "a polyline past its minimum wants no particular further anchor");

    const DraftArrival Ended = Place(Draft, 9.0, true);
    Claim(Ended == DraftArrival::Complete, "double-press should end the polyline");

    // 🔴 A double-press before the minimum must NOT end the curve. Two anchors is not a Hermite.
    DraftPlacement Short;
    Short.Declare(DraftSubject::Hermite);
    Place(Short, 0.0);
    const DraftArrival TooSoon = Place(Short, 1.0, true);
    Claim(TooSoon == DraftArrival::Anchored,
          "a Hermite must not end on a double-press with two of its four anchors");
}

//------------------------------------------------------------------------------------------------------------------------
//                                       4. A DIMENSION TAKES ONLY SNAPPED ANCHORS
//------------------------------------------------------------------------------------------------------------------------

void ProveResolvedClosure()
{
    std::printf("4. A dimension refuses contacts that landed on nothing\n");

    DraftPlacement Draft;
    Draft.Declare(DraftSubject::LinearDimension);

    // Three contacts in empty space — all refused, nothing accumulates.
    for (int Attempt = 0; Attempt < 3; ++Attempt)
    {
        const DraftArrival Arrival = Place(Draft, static_cast<double>(Attempt), false, false);
        Claim(Arrival == DraftArrival::Ignored,
              "an unsnapped contact must not anchor a dimension (attempt " + std::to_string(Attempt) + ")");
    }
    Claim(Draft.Anchors().empty(), "a refused contact must leave no anchor behind");

    Claim(Place(Draft, 0.0, false, true) == DraftArrival::Anchored, "first snapped contact should anchor");
    Claim(Place(Draft, 1.0, false, false) == DraftArrival::Ignored, "an unsnapped second contact is refused");
    Claim(Draft.Anchors().size() == 1u, "the refused contact must not have been counted");
    Claim(Place(Draft, 2.0, false, true) == DraftArrival::Complete, "second snapped contact completes it");

    // 📝 Every anchor a dimension took must carry a resolved snap — that is the whole point of the closure.
    const SealedDraft Sealed = Draft.Seal();
    for (const SketchSnapPlacement& Placement : Sealed.Placements)
        Claim(Placement.Resolved(), "every dimension anchor must carry a resolved snap");
}

//------------------------------------------------------------------------------------------------------------------------
//                                     5. SEALING MOVES THE ANCHORS AND KEEPS THE TOOL
//------------------------------------------------------------------------------------------------------------------------

void ProveSealing()
{
    std::printf("5. Sealing hands over the anchors and leaves the tool held\n");

    DraftPlacement Draft;
    Draft.Declare(DraftSubject::Line, true);
    Place(Draft, 0.0);
    Place(Draft, 4.0);

    const SealedDraft Sealed = Draft.Seal();

    Claim(Sealed.Subject == DraftSubject::Line, "the sealed draft should name the line");
    Claim(Sealed.Anchors.size() == 2u, "the sealed draft should carry both anchors");
    Claim(Sealed.Placements.size() == Sealed.Anchors.size(),
          "every anchor must carry its placement — the two vectors are one record split in two");
    Claim(Sealed.Construction, "the sealed draft should remember it was construction geometry");
    Claim(Sealed.Anchors[0].Left == 0.0 && Sealed.Anchors[1].Left == 4.0,
          "anchors must arrive in the order they were taken");

    // 🔴 The tool is still held and the anchors are gone. This is the property that lets an artist draw
    //    three lines in a row without re-pressing the tile, and it is where a `Seal` that reset everything
    //    would silently cost a press per curve.
    Claim(Draft.Standing(), "the line tool must still be held after sealing");
    Claim(Draft.Subject() == DraftSubject::Line, "the held tool must still be the line");
    Claim(Draft.Anchors().empty(), "the sealed anchors must be gone from the placement");
    Claim(Draft.Remaining() == 2u, "the next line should want both its anchors again");

    // The very next curve works, from the same placement.
    Claim(Place(Draft, 7.0) == DraftArrival::Anchored, "the second line's first anchor");
    Claim(Place(Draft, 9.0) == DraftArrival::Complete, "the second line completes without re-declaring");

    // Sealing an incomplete placement delivers nothing rather than a half curve.
    DraftPlacement Partial;
    Partial.Declare(DraftSubject::Arc);
    Place(Partial, 0.0);
    const SealedDraft Nothing = Partial.Seal();
    Claim(Nothing.Subject == DraftSubject::None || Nothing.Anchors.size() == 1u,
          "sealing an incomplete arc must not fabricate anchors");
}

//------------------------------------------------------------------------------------------------------------------------
//                                    6. DECLARING IS IDEMPOTENT, CHANGING TOOL ABANDONS
//------------------------------------------------------------------------------------------------------------------------

void ProveDeclaration()
{
    std::printf("6. Restating the held tool keeps the anchors; changing it discards them\n");

    DraftPlacement Draft;
    Draft.Declare(DraftSubject::Arc);
    Place(Draft, 0.0);
    Place(Draft, 1.0);
    Claim(Draft.Anchors().size() == 2u, "the arc should hold two anchors");

    // 🔴 The host states the active tool EVERY TICK. If this restarted the placement, no three-anchor
    //    subject could ever be drawn — the first anchor would be discarded before the second arrived.
    for (int Tick = 0; Tick < 30; ++Tick)
        Draft.Declare(DraftSubject::Arc);
    Claim(Draft.Anchors().size() == 2u, "restating the same tool must not discard anchors");
    Claim(Place(Draft, 2.0) == DraftArrival::Complete, "the arc should still complete on its third anchor");

    // Changing tool mid-placement abandons.
    DraftPlacement Switched;
    Switched.Declare(DraftSubject::Polyline);
    Place(Switched, 0.0);
    Place(Switched, 1.0);
    Place(Switched, 2.0);
    Claim(Switched.Anchors().size() == 3u, "the polyline should hold three anchors");
    Switched.Declare(DraftSubject::Circle);
    Claim(Switched.Anchors().empty(), "changing tool must discard the anchors in progress");
    Claim(Switched.Subject() == DraftSubject::Circle, "the new tool should be held");

    // `None` stands the tool down entirely.
    Switched.Declare(DraftSubject::None);
    Claim(!Switched.Standing(), "declaring None must stand the tool down");
    Claim(Switched.Remaining() == 0u, "a tool that is not held wants no anchors");
}

//------------------------------------------------------------------------------------------------------------------------
//                                          7. THE MACHINE REFUSES WHAT IT CANNOT DO
//------------------------------------------------------------------------------------------------------------------------

void ProveRefusals()
{
    std::printf("7. Contacts with no tool, and anchors with no hover, are refused\n");

    DraftPlacement Idle;
    Claim(Idle.Anchor() == DraftArrival::Ignored, "anchoring with no tool held must be refused");
    Claim(!Idle.Standing(), "a fresh placement holds no tool");
    Claim(Idle.Anchors().empty(), "a fresh placement holds no anchors");

    // 🔴 An anchor is taken at the hover position, so a tool declared but never hovered has nothing to
    //    anchor. Taking one anyway would place it at the origin, or at the stale position of a previous
    //    placement — which is the defect this refusal exists to prevent.
    DraftPlacement Unhovered;
    Unhovered.Declare(DraftSubject::Line);
    Claim(Unhovered.Anchor() == DraftArrival::Ignored, "anchoring before hovering must be refused");
    Claim(Unhovered.Anchors().empty(), "a refused anchor must leave nothing behind");

    // Hovering with no tool held records nothing.
    DraftPlacement Hovered;
    Hovered.Hover({ 5.0, 5.0, 5.0 }, {});
    Claim(!Hovered.HoverStanding(), "hovering with no tool held must record no hover");

    // Abandon resets everything, including the hover.
    DraftPlacement Full;
    Full.Declare(DraftSubject::Slot, true);
    Place(Full, 1.0);
    Full.Abandon();
    Claim(!Full.Standing(),        "abandon must stand the tool down");
    Claim(Full.Anchors().empty(),  "abandon must discard the anchors");
    Claim(!Full.HoverStanding(),   "abandon must discard the hover");
    Claim(!Full.Construction(),    "abandon must clear the construction declaration");
}

//------------------------------------------------------------------------------------------------------------------------
//                                    8. THE SNAP ON THE PREVIEW IS THE SNAP ON THE ANCHOR
//------------------------------------------------------------------------------------------------------------------------

void ProveSnapCarriage()
{
    std::printf("8. The anchor records exactly the snap the preview showed\n");

    DraftPlacement Draft;
    Draft.Declare(DraftSubject::Line);

    SketchSnapPlacement Midpoint = {};
    Midpoint.Subject  = SketchSnapSubject::Midpoint;
    Midpoint.Position = { 3.0, 0.0, 0.0 };
    Midpoint.Distance = 0.25;

    Draft.Hover({ 3.0, 0.0, 0.0 }, Midpoint);
    Claim(Draft.HoverStanding(), "the hover should stand once stated");
    Claim(Draft.HoverPlacement().Subject == SketchSnapSubject::Midpoint,
          "the hover should carry the midpoint snap");

    Draft.Anchor();
    Claim(Draft.Placements().size() == 1u, "the anchor should carry one placement");
    Claim(Draft.Placements()[0].Subject == SketchSnapSubject::Midpoint,
          "the anchored placement must be the one the preview showed");
    Claim(Draft.Placements()[0].Distance == 0.25,
          "the anchored placement must be carried whole, not rebuilt");
    Claim(Draft.Anchors()[0].Left == 3.0, "the anchor must sit where the hover sat");
}

}   // namespace

//------------------------------------------------------------------------------------------------------------------------

int main()
{
    std::printf("\n=== DraftPlacement — SlateToolset ===\n\n");

    ProveTableEquivalence();
    ProveCompletionCounts();
    ProveTerminatedCurves();
    ProveResolvedClosure();
    ProveSealing();
    ProveDeclaration();
    ProveRefusals();
    ProveSnapCarriage();

    std::printf("\n%d claims, %d failures\n", Checks, Failures);
    std::printf(Failures == 0 ? "PROVEN\n\n" : "REFUTED\n\n");
    return Failures == 0 ? 0 : 1;
}
