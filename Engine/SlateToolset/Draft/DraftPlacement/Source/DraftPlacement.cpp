//============================================================================================================================================
//                                                          DRAFTPLACEMENT.CPP
//============================================================================================================================================

#include "SlateToolset/Draft/DraftPlacement/Api/DraftPlacement.h"

#include <utility>

namespace Slate
{

//------------------------------------------------------------------------------------------------------------------------
//                                                       DECLARATION
//------------------------------------------------------------------------------------------------------------------------

void DraftPlacement::Declare(DraftSubject Subject, bool Construction)
{
    // 🔴 Declaring the subject already held keeps the anchors. The caller states the active tool every tick
    //    from whatever the artist last pressed, so restarting here would discard the first anchor of every
    //    two-anchor placement on the tick after it was taken.
    if (Subject == Placing)
    {
        ConstructionDeclared = Construction;
        return;
    }

    Abandon();
    Placing              = Subject;
    ConstructionDeclared = Construction;
}

void DraftPlacement::Abandon()
{
    Placing              = DraftSubject::None;
    ConstructionDeclared = false;
    HoverTaken           = false;
    HoverAt              = {};
    HoverSnap            = {};

    // 📝 `clear` rather than assigning `{}`: the placement is reused every time the artist draws another
    //    curve with the same tool, and keeping the reserved extent means the common case allocates once.
    Taken.clear();
    TakenPlacements.clear();
}

//------------------------------------------------------------------------------------------------------------------------
//                                                        THE POINTER
//------------------------------------------------------------------------------------------------------------------------

void DraftPlacement::Hover(const SpatialPoint& Position, const SketchSnapPlacement& Placement)
{
    if (!Standing())
        return;

    HoverAt    = Position;
    HoverSnap  = Placement;
    HoverTaken = true;
}

DraftArrival DraftPlacement::Anchor(bool Terminating)
{
    // 🔴 An anchor is taken at the hover position, so a caller that has not stated one this tick has nothing
    //    to anchor. Reporting `Ignored` leaves the contact unconsumed rather than placing an anchor at the
    //    stale position of the previous tick.
    if (!Standing() || !HoverTaken)
        return DraftArrival::Ignored;

    const DraftDeclaration Declared = DeclaredDraft(Placing);

    // 🔴 A `Resolved` closure measures between features, so an unsnapped contact is not an anchor at all.
    //    The contact is refused rather than taken, which is what lets a dimension tool ignore empty space
    //    without the caller having to know that dimensions are special.
    if (Declared.Closure == DraftClosure::Resolved && !HoverSnap.Resolved())
        return DraftArrival::Ignored;

    Taken.push_back(HoverAt);
    TakenPlacements.push_back(HoverSnap);

    const std::uint32_t Count = static_cast<std::uint32_t>(Taken.size());

    switch (Declared.Closure)
    {
        case DraftClosure::Sufficient:
            // 📝 `>=` not `==`: a subject whose declared count is reached exactly still completes, and a
            //    count that has somehow been passed completes rather than running on forever.
            return Count >= Declared.Required ? DraftArrival::Complete : DraftArrival::Anchored;

        case DraftClosure::Terminated:
            // 🔴 A terminated curve completes only when the artist says so AND enough anchors stand. A
            //    double-press on the second anchor of a four-anchor Hermite is not a curve.
            return (Terminating && Count >= Declared.Required) ? DraftArrival::Complete
                                                               : DraftArrival::Anchored;

        case DraftClosure::Resolved:
            // 📝 Every anchor here is snapped — the unsnapped ones were refused above — so the count of
            //    anchors and the count of resolved anchors are the same number.
            return Count >= Declared.Required ? DraftArrival::Complete : DraftArrival::Anchored;

        case DraftClosure::ClosureCount:
            break;
    }

    return DraftArrival::Anchored;
}

//------------------------------------------------------------------------------------------------------------------------
//                                                          SEALING
//------------------------------------------------------------------------------------------------------------------------

SealedDraft DraftPlacement::Seal()
{
    SealedDraft Sealed = {};
    if (!Standing() || Taken.empty())
        return Sealed;

    Sealed.Subject      = Placing;
    Sealed.Construction = ConstructionDeclared;
    Sealed.Anchors      = std::move(Taken);
    Sealed.Placements   = std::move(TakenPlacements);

    // 🔴 The moved-from vectors are cleared, not left in whatever state the move produced, so the placement
    //    that continues holding this tool starts its next curve empty. The tool itself stays held: after
    //    drawing a line the artist still has the line tool, which is what every CAD application does.
    Taken.clear();
    TakenPlacements.clear();
    HoverTaken = false;

    return Sealed;
}

//------------------------------------------------------------------------------------------------------------------------
//                                                        REPORTING
//------------------------------------------------------------------------------------------------------------------------

std::uint32_t DraftPlacement::Remaining() const
{
    if (!Standing())
        return 0u;

    const DraftDeclaration Declared = DeclaredDraft(Placing);
    const std::uint32_t    Count    = static_cast<std::uint32_t>(Taken.size());

    return Count >= Declared.Required ? 0u : Declared.Required - Count;
}

}   // namespace Slate
