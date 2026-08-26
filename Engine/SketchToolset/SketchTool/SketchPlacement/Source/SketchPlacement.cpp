//============================================================================================================================================
//                                                         SKETCHPLACEMENT.CPP
//============================================================================================================================================

#include "SketchToolset/SketchTool/SketchPlacement/Api/SketchPlacement.h"

#include <utility>

namespace Slate
{

//------------------------------------------------------------------------------------------------------------------------
//                                                       DECLARATION
//------------------------------------------------------------------------------------------------------------------------

void SketchPlacement::Declare(SketchSubject Subject, PlacementMethod Method, bool Construction)
{
    // 🔴 A pair no geometry call can honour stands the tool down rather than holding a tool that can never
    //    complete. The retired table shipped exactly that: `DiameterCircle` sat in a branch storing one
    //    anchor while its commit required two, so the tool was held, consumed every press, and produced
    //    nothing for as long as the artist kept clicking.
    if (!AcceptedBy(Subject, Method))
    {
        Abandon();
        return;
    }

    // 🔴 Declaring what is already held keeps the anchors. The caller states the held tool every tick from
    //    whatever the artist last pressed, so restarting here would discard the first anchor of every
    //    two-anchor placement on the tick after it was taken.
    if (Subject == Placing && Method == PlacingMethod)
    {
        ConstructionDeclared = Construction;
        return;
    }

    Abandon();
    Placing              = Subject;
    PlacingMethod        = Method;
    ConstructionDeclared = Construction;
}

void SketchPlacement::Abandon()
{
    Placing              = SketchSubject::None;
    PlacingMethod        = PlacementMethod::Extent;
    ConstructionDeclared = false;
    HoverTaken           = false;
    HoverAt              = {};
    HoverSnap            = {};

    // 📝 `clear` rather than assigning `{}`: the placement is reused every time the artist draws another
    //    shape with the same tool, so keeping the reserved extent means the common case allocates once.
    Taken.clear();
    TakenPlacements.clear();
}

//------------------------------------------------------------------------------------------------------------------------
//                                                        THE POINTER
//------------------------------------------------------------------------------------------------------------------------

void SketchPlacement::Hover(const SpatialPoint& Position, const SketchSnapPlacement& Placement)
{
    if (!Standing())
        return;

    HoverAt    = Position;
    HoverSnap  = Placement;
    HoverTaken = true;
}

PlacementArrival SketchPlacement::Anchor(bool Terminating)
{
    // 🔴 An anchor is taken at the hover, so a caller that has not stated one this tick has nothing to
    //    anchor. Reporting `Ignored` leaves the contact unconsumed rather than placing an anchor at the
    //    stale position of the previous tick.
    if (!Standing() || !HoverTaken)
        return PlacementArrival::Ignored;

    const PlacementDeclaration Declared = DeclaredPlacement(Placing, PlacingMethod);

    // 🔴 A `Resolved` closure measures between features, so an unsnapped contact is not an anchor at all.
    //    Refusing it lets a dimension tool ignore empty space without the caller knowing dimensions are
    //    special.
    if (Declared.Closure == PlacementClosure::Resolved && !HoverSnap.Resolved())
        return PlacementArrival::Ignored;

    Taken.push_back(HoverAt);
    TakenPlacements.push_back(HoverSnap);

    const std::uint32_t Count = static_cast<std::uint32_t>(Taken.size());

    switch (Declared.Closure)
    {
        case PlacementClosure::Sufficient:
            // 📝 `>=` not `==`: a count reached exactly still completes, and a count somehow passed
            //    completes rather than running on forever.
            return Count >= Declared.Required ? PlacementArrival::Complete : PlacementArrival::Anchored;

        case PlacementClosure::Terminated:
            // 🔴 A terminated curve completes only when the artist says so AND enough anchors stand. A
            //    double-press on the second anchor of a four-anchor Hermite is not a curve.
            return (Terminating && Count >= Declared.Required) ? PlacementArrival::Complete
                                                               : PlacementArrival::Anchored;

        case PlacementClosure::Resolved:
            // 📝 Every anchor here is snapped — the unsnapped ones were refused above — so the count of
            //    anchors and the count of resolved anchors are the same number.
            return Count >= Declared.Required ? PlacementArrival::Complete : PlacementArrival::Anchored;

        case PlacementClosure::ClosureCount:
            break;
    }

    return PlacementArrival::Anchored;
}

//------------------------------------------------------------------------------------------------------------------------
//                                                          SEALING
//------------------------------------------------------------------------------------------------------------------------

SealedPlacement SketchPlacement::Seal()
{
    SealedPlacement Sealed = {};
    if (!Standing() || Taken.empty())
        return Sealed;

    Sealed.Subject      = Placing;
    Sealed.Method       = PlacingMethod;
    Sealed.Construction = ConstructionDeclared;
    Sealed.Anchors      = std::move(Taken);
    Sealed.Placements   = std::move(TakenPlacements);

    // 🔴 The moved-from vectors are cleared, not left in whatever state the move produced, so the tool
    //    that continues to be held starts its next shape empty. The tool itself stays held: after drawing
    //    a line the artist still has the line tool.
    Taken.clear();
    TakenPlacements.clear();
    HoverTaken = false;

    return Sealed;
}

//------------------------------------------------------------------------------------------------------------------------
//                                                        REPORTING
//------------------------------------------------------------------------------------------------------------------------

std::uint32_t SketchPlacement::Remaining() const
{
    if (!Standing())
        return 0u;

    const PlacementDeclaration Declared = DeclaredPlacement(Placing, PlacingMethod);
    const std::uint32_t        Count    = static_cast<std::uint32_t>(Taken.size());

    return Count >= Declared.Required ? 0u : Declared.Required - Count;
}

}   // namespace Slate
