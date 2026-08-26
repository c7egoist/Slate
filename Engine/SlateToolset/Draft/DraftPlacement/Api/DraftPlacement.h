//============================================================================================================================================
//                                                           DRAFTPLACEMENT.H
//============================================================================================================================================
// 🧩 One drawing tool's placement in progress — what is being placed, the anchors taken so far, and when it is complete.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"
#include "SlateShape/Sketch/SketchSnap/Api/SketchSnap.h"
#include "SlateUI/Interface/ParametricTools/Api/ParametricToolsSpecification.h"

#include <cstdint>
#include <vector>

namespace Slate
{

//------------------------------------------------------------------------------------------------------------------------
//                                                       WHAT IS PLACED
//------------------------------------------------------------------------------------------------------------------------

/// 🧩 The geometry a drawing tool places, independent of the catalogue tile the artist pressed.
/// note  🔴 Deliberately distinct from `ParametricToolSubject`. That enumeration is the CATALOGUE — sixty-one
///        tiles including selection, constraints, datums, imports and lighting, none of which place a curve
///        by taking anchors. This is the far smaller set that is actually drawn, and several tiles map onto
///        one entry here: `Interpolate`, `Approximate` and `BezierCurve` are three tiles and one placement.
/// note  ⚠️ This enumeration was declared TWICE before this unit existed — once as `SharedCadDraftSubject`
///        in `Application/Api/`, once as `ParametricDraftSubject` inside `ParametricSketchHost.cpp` — with
///        identical names and identical ordinals, reconciled at the seam by casting one to the other
///        through `std::uint32_t`. That cast was correct only while both lists stayed in the same order,
///        and nothing verified that they did. There is now one declaration and no cast.
/// tag   guarantee
enum class DraftSubject : std::uint32_t
{
    None                =  0u,   // [-] - no drawing tool is active
    Line                =  1u,
    Rectangle           =  2u,
    Circle              =  3u,
    Arc                 =  4u,
    Polyline            =  5u,
    LinearDimension     =  6u,
    Point               =  7u,
    Ellipse             =  8u,
    Bezier              =  9u,
    EllipticalArc       = 10u,
    BasisSpline         = 11u,
    CenterRectangle     = 12u,
    ThreePointRectangle = 13u,
    DiameterCircle      = 14u,
    ThreePointCircle    = 15u,
    CenterStartEndArc   = 16u,
    TangentArc          = 17u,
    Polygon             = 18u,
    Slot                = 19u,
    Hermite             = 20u,
    RationalSpline      = 21u,
    SubjectCount        = 22u    // [-] - the closed count, never a subject
};

/// 🧩 How a placement decides it has taken every anchor it needs.
/// note  🔴 These are three genuinely different mechanisms, and they were three separate branch groups in
///        the host. `Sufficient` completes the moment the required count is reached. `Terminated` keeps
///        accepting anchors past that count until the artist double-presses, which is how a polyline of
///        unknown length ends. `Resolved` counts only anchors that landed on a snap, because a dimension
///        measures between two features and a contact in empty space is not a feature.
/// tag   guarantee
enum class DraftClosure : std::uint32_t
{
    Sufficient   = 0u,   // [-] - completes as soon as `Required` anchors are taken
    Terminated   = 1u,   // [-] - takes anchors until a terminating contact, then completes if `Required` are taken
    Resolved     = 2u,   // [-] - only snapped anchors count toward `Required`
    ClosureCount = 3u    // [-] - the closed count, never a closure
};

/// 🧩 Everything one drawing tool states about how it is placed.
/// note  📝 Named for `SessionDeclaration` in `SlateRuntime`, which states the same shape of thing: what a
///        mechanism declares once, before it runs.
/// tag   guarantee, nonallocating, nonthrowing
struct DraftDeclaration
{
    std::uint32_t  Required      = 0u;                       // [-] - anchors taken before it may complete
    DraftClosure   Closure       = DraftClosure::Sufficient;  // [-] - how it decides it has them
    bool           ClosedProfile = false;                    // [-] - the placed curve encloses an area
    const char*    Naming        = "";                       // [-] - static text; what the artist calls it
};

/// 🧩 What one drawing tool declares — anchors required, how it closes, whether it encloses an area.
/// in    Subject   [-]  the geometry being placed
/// out   Declared  [-]  the declaration; `Required == 0` for `None`
/// note  🔴 ONE table. It replaces four parallel switch statements — `RequiredAnchors`, `IsMultiClickCurve`,
///        `ProducesClosedProfile` and `SubjectName` — that each enumerated the same twenty-two subjects and
///        could therefore disagree about any one of them. A tool is added by adding one row.
///
/// note  🔴 `Required` counts ANCHORS TAKEN, and it counts them for every subject. The column it replaces
///        did not: `SharedCadDraftRequiredAnchors` was only ever read for the four terminated curves, and
///        for the rest it was unread decoration that had drifted into meaning two different things —
///        "anchors stored" for `DiameterCircle` and `Polygon`, but "presses the artist makes" for
///        `Rectangle` and `CenterRectangle`, whose second press was consumed from the live hover rather
///        than stored. `Circle` and `Ellipse` said `1` while genuinely needing a centre AND a rim point.
///        Because nothing read those rows, nothing ever reported the disagreement.
///
/// note  ⚠️ Three rows therefore state a DIFFERENT number from the header they were transcribed from —
///        `Circle` and `Ellipse` are 2, not 1 — and `DraftPlacementProof` §1 asserts precisely those three
///        exceptions rather than blanket agreement, so the correction is deliberate and checked rather
///        than a transcription slip. The old value described a placement that could not commit: a circle
///        with only its centre taken is not a circle.
/// cost  ✔️
/// tag   api, constexpr, nonallocating, nonthrowing
constexpr DraftDeclaration DeclaredDraft(DraftSubject Subject)
{
    switch (Subject)
    {
        case DraftSubject::Line:                return { 2u, DraftClosure::Sufficient, false, "Line" };
        case DraftSubject::Rectangle:           return { 2u, DraftClosure::Sufficient, true,  "Rectangle" };
        case DraftSubject::CenterRectangle:     return { 2u, DraftClosure::Sufficient, true,  "Center Rectangle" };
        case DraftSubject::DiameterCircle:      return { 2u, DraftClosure::Sufficient, true,  "Diameter Circle" };
        case DraftSubject::Polygon:             return { 2u, DraftClosure::Sufficient, true,  "Polygon" };
        case DraftSubject::Circle:              return { 2u, DraftClosure::Sufficient, true,  "Circle" };
        case DraftSubject::Ellipse:             return { 2u, DraftClosure::Sufficient, true,  "Ellipse" };
        case DraftSubject::Point:               return { 1u, DraftClosure::Sufficient, false, "Point" };
        case DraftSubject::Arc:                 return { 3u, DraftClosure::Sufficient, false, "Arc" };
        case DraftSubject::EllipticalArc:       return { 3u, DraftClosure::Sufficient, false, "Elliptical Arc" };
        case DraftSubject::ThreePointRectangle: return { 3u, DraftClosure::Sufficient, true,  "3-Point Rectangle" };
        case DraftSubject::ThreePointCircle:    return { 3u, DraftClosure::Sufficient, true,  "3-Point Circle" };
        case DraftSubject::CenterStartEndArc:   return { 3u, DraftClosure::Sufficient, false, "Center Arc" };
        case DraftSubject::TangentArc:          return { 3u, DraftClosure::Sufficient, false, "Tangent Arc" };
        case DraftSubject::Slot:                return { 3u, DraftClosure::Sufficient, true,  "Slot" };
        case DraftSubject::Polyline:            return { 2u, DraftClosure::Terminated, false, "Polyline" };
        case DraftSubject::Bezier:              return { 2u, DraftClosure::Terminated, false, "Bezier" };
        case DraftSubject::BasisSpline:         return { 3u, DraftClosure::Terminated, false, "Basis Spline" };
        case DraftSubject::RationalSpline:      return { 3u, DraftClosure::Terminated, false, "NURBS Curve" };
        case DraftSubject::Hermite:             return { 4u, DraftClosure::Terminated, false, "Hermite" };
        case DraftSubject::LinearDimension:     return { 2u, DraftClosure::Resolved,   false, "Dimension" };
        case DraftSubject::None:
        case DraftSubject::SubjectCount:        break;
    }

    return { 0u, DraftClosure::Sufficient, false, "" };
}

/// 🧩 Which geometry a catalogue tile places, or `None` where the tile places none.
/// in    Tile     [-]  the catalogue subject the artist pressed
/// out   Subject  [-]  the geometry placed; `None` for selection, constraints, datums, imports and lighting
/// note  📝 Not injective on purpose. `Interpolate`, `Approximate` and `BezierCurve` are three distinct
///        catalogue tiles that all place a Bezier, and `ConstructionLine` places a Line the caller then
///        marks as construction. The tile carries the artist's intent; the subject carries the geometry.
/// cost  ✔️
/// tag   api, constexpr, nonallocating, nonthrowing
constexpr DraftSubject DraftFromCatalogue(ParametricToolSubject Tile)
{
    switch (Tile)
    {
        case ParametricToolSubject::Line:                return DraftSubject::Line;
        case ParametricToolSubject::Polyline:            return DraftSubject::Polyline;
        case ParametricToolSubject::Rectangle:           return DraftSubject::Rectangle;
        case ParametricToolSubject::Circle:              return DraftSubject::Circle;
        case ParametricToolSubject::Arc:                 return DraftSubject::Arc;
        case ParametricToolSubject::LinearDimension:     return DraftSubject::LinearDimension;
        case ParametricToolSubject::Point:               return DraftSubject::Point;
        case ParametricToolSubject::Ellipse:             return DraftSubject::Ellipse;
        case ParametricToolSubject::EllipticalArc:       return DraftSubject::EllipticalArc;
        case ParametricToolSubject::BezierCurve:
        case ParametricToolSubject::Interpolate:
        case ParametricToolSubject::Approximate:         return DraftSubject::Bezier;
        case ParametricToolSubject::HermiteCurve:        return DraftSubject::Hermite;
        case ParametricToolSubject::BasisSpline:         return DraftSubject::BasisSpline;
        case ParametricToolSubject::RationalSpline:      return DraftSubject::RationalSpline;
        case ParametricToolSubject::ConstructionLine:    return DraftSubject::Line;
        case ParametricToolSubject::CenterRectangle:     return DraftSubject::CenterRectangle;
        case ParametricToolSubject::ThreePointRectangle: return DraftSubject::ThreePointRectangle;
        case ParametricToolSubject::DiameterCircle:      return DraftSubject::DiameterCircle;
        case ParametricToolSubject::ThreePointCircle:    return DraftSubject::ThreePointCircle;
        case ParametricToolSubject::CenterStartEndArc:   return DraftSubject::CenterStartEndArc;
        case ParametricToolSubject::TangentArc:          return DraftSubject::TangentArc;
        case ParametricToolSubject::Polygon:             return DraftSubject::Polygon;
        case ParametricToolSubject::Slot:                return DraftSubject::Slot;
        default:                                         return DraftSubject::None;
    }
}

//------------------------------------------------------------------------------------------------------------------------
//                                                      WHAT ONE CONTACT DOES
//------------------------------------------------------------------------------------------------------------------------

/// 🧩 What one contact did to the placement.
/// note  🔴 `Complete` means the anchors are all taken and the caller must now `Seal`. This component
///        deliberately does not write anything: what a placed curve BECOMES — a sketch record, a revision,
///        a selection — is a document question, and a tool that answered it would have to name documents.
///        It states readiness and holds the anchors; the caller seals and writes.
/// tag   guarantee
enum class DraftArrival : std::uint32_t
{
    Ignored      = 0u,   // [-] - no placement stands, or the contact placed nothing
    Anchored     = 1u,   // [-] - an anchor was taken; the placement continues
    Complete     = 2u,   // [-] - every anchor is taken; `Seal` now
    ArrivalCount = 3u    // [-] - the closed count, never an arrival
};

/// 🧩 The anchors of one finished placement, moved out of the placement that took them.
/// note  📝 Returned by value from `Seal` so the placement it came from is already reset. There is no
///        window in which a sealed draft and the placement that produced it both hold the same anchors.
/// tag   guarantee, owning
struct SealedDraft
{
    DraftSubject                     Subject      = DraftSubject::None;   // [-]
    std::vector<SpatialPoint>        Anchors      = {};                   // [-] - in the order taken
    std::vector<SketchSnapPlacement> Placements   = {};                   // [-] - what each anchor landed on
    bool                             Construction = false;                // [-] - construction geometry
};

//------------------------------------------------------------------------------------------------------------------------
//                                                       THE PLACEMENT
//------------------------------------------------------------------------------------------------------------------------

/// 🧩 One drawing tool's placement in progress — the anchors taken, and whether it is ready to seal.
/// note  🔴 This component exists because the host held this state machine as a chain of `else if` branches
///        over twenty-two subjects, grouped by how many anchors each needed. Adding a tool meant finding
///        the branch with the right count and joining it; getting it wrong meant a curve that completed one
///        anchor early, with nothing to catch it. The count and the closure are now stated once, in
///        `DeclaredDraft`, and this component READS that table rather than restating it.
/// note  ⚠️ Anchors are held, not interpreted. What a Bezier's four anchors mean is the caller's question;
///        this component only knows there must be four before it may complete.
/// note  📝 It names no panel, no document, no device and no tick — a tool that knew when it was recorded
///        would be a tick concern wearing a tool's name.
/// tag   owning
class DraftPlacement
{
public:

    /// 🧩 Declares what is being placed, discarding whatever placement stood before.
    /// in    Subject       [-]  the geometry to place; `None` abandons and stands down
    /// in    Construction  [-]  whether the placed curve is construction geometry
    /// note  📝 Declaring the SAME subject that already stands is a no-op, not a restart — the caller may
    ///        state the active tool every tick without losing the anchors already taken. Declaring a
    ///        DIFFERENT subject abandons the placement in progress, which is the ordinary case: the artist
    ///        pressed a second catalogue tile without finishing the first.
    /// cost  ✔️
    /// tag   api, nonthrowing
    void Declare(DraftSubject Subject, bool Construction = false);

    /// 🧩 Takes one contact as an anchor, and states whether the placement is now complete.
    /// in    Terminating  [-]  whether this contact was a double-press
    /// out   Arrival      [-]  `Complete` when the caller must now `Seal`
    /// note  🔴 The anchor is taken at the CURRENT hover position, not at a position passed here, so the
    ///        snap resolved for the preview is exactly the snap recorded on the anchor. Passing the position
    ///        again would let a caller anchor somewhere it never previewed.
    /// note  🔴 A `Resolved` closure takes only snapped anchors. A dimension measures between two features,
    ///        so a contact in empty space is discarded and reported `Ignored` — which is what tells the
    ///        caller that nothing changed and the contact is still theirs to use.
    /// cost  🚩
    /// tag   api, nonthrowing
    DraftArrival Anchor(bool Terminating = false);

    /// 🧩 States where the pointer is now, so the caller can preview the curve before the next contact.
    /// in    Position   [-]  where the pointer is, already snapped by the caller
    /// in    Placement  [-]  what it snapped to, or an unresolved placement
    /// cost  ✔️
    /// tag   api, nonallocating, nonthrowing
    void Hover(const SpatialPoint& Position, const SketchSnapPlacement& Placement);

    /// 🧩 Moves the anchors out of a complete placement, leaving the same tool ready to place again.
    /// out   Sealed  [-]  the anchors; empty with subject `None` when the placement was not `Complete`
    /// note  🔴 Sealing does NOT stand the tool down. After drawing a line the line tool is still held, which
    ///        is what every CAD application does and what the host's `CancelDraft` followed by next tick's
    ///        `Draft.Subject = Desired` already achieved by a longer route.
    /// use   Called on `Complete` and at no other time.
    /// cost  🚩
    /// tag   api, nonthrowing
    SealedDraft Seal();

    /// 🧩 Discards every anchor taken and stands the tool down.
    /// use   Escape, or a tool change to `None`.
    /// cost  ✔️
    /// tag   api, nonthrowing
    void Abandon();

    /// 🧩 What is being placed, or `None`.
    /// cost  ✔️
    /// tag   api, nonallocating, nonthrowing
    DraftSubject Subject() const { return Placing; }

    /// 🧩 Whether a drawing tool is held at all.
    /// cost  ✔️
    /// tag   api, nonallocating, nonthrowing
    bool Standing() const { return Placing != DraftSubject::None; }

    /// 🧩 Whether the placed curve is construction geometry.
    /// cost  ✔️
    /// tag   api, nonallocating, nonthrowing
    bool Construction() const { return ConstructionDeclared; }

    /// 🧩 The anchors taken so far, in the order they were taken.
    /// cost  ✔️
    /// tag   api, nonallocating, nonthrowing
    const std::vector<SpatialPoint>&        Anchors() const { return Taken; }
    const std::vector<SketchSnapPlacement>& Placements() const { return TakenPlacements; }

    /// 🧩 How many anchors this placement still needs before it may complete.
    /// out   Remaining  [-]  zero once enough are taken, including while a `Terminated` curve grows
    /// note  📝 For the status text the host draws under the pointer: "Polyline — 2 taken, double-press to end".
    /// cost  ✔️
    /// tag   api, nonallocating, nonthrowing
    std::uint32_t Remaining() const;

    /// 🧩 Where the pointer is now, for previewing the curve before the next contact.
    /// out   Standing  [-]  false until the first `Hover` after a `Declare`
    /// cost  ✔️
    /// tag   api, nonallocating, nonthrowing
    bool                       HoverStanding() const { return HoverTaken; }
    const SpatialPoint&        HoverPosition() const { return HoverAt; }
    const SketchSnapPlacement& HoverPlacement() const { return HoverSnap; }

private:

    DraftSubject                     Placing              = DraftSubject::None;   // [-]
    std::vector<SpatialPoint>        Taken                = {};                   // [-] - anchors, in order
    std::vector<SketchSnapPlacement> TakenPlacements      = {};                   // [-] - what each landed on
    SpatialPoint                     HoverAt              = {};                   // [-]
    SketchSnapPlacement              HoverSnap            = {};                   // [-]
    bool                             HoverTaken           = false;                // [-]
    bool                             ConstructionDeclared = false;                // [-]
};

}   // namespace Slate
