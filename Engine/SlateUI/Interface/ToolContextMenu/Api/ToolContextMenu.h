//============================================================================================================================================
//                                                         TOOLCONTEXTMENU.H
//============================================================================================================================================
// 🧩 The small menu a construction tile opens — Bevel, Chamfer, Trim, Cut, Add — placed in a corner that
//    is free, and never on top of another widget.
//
// 🔴 THE PLACEMENT IS THE FEATURE. A menu that lands on top of the Select widget is worse than no menu:
//    it hides the thing the artist just used to make the selection the tool is about to act on. So the
//    corner is chosen against the boxes actually occupied this tick, not against a layout someone
//    assumed. `PlaceMenuClear` does the arithmetic; this unit does the drawing and the pointer.
//
// 🔴 THE MENU BORROWS ITS ITEMS AND OWNS NOTHING. Same arrangement as the options widget: the caller
//    holds the captions and the taken index, and the menu edits them in place. A menu that copied its
//    own roster would need invalidating every time a tool changed what it offers.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "Foundation/ExtentBands.h"
#include "SlateUI/Interface/AppearanceSpecification/Api/AppearanceSpecification.h"
#include "SlateUI/Interface/ControlIndex/Api/ControlIndex.h"
#include "SlateUI/Interface/InterfaceExchange/Api/RecordingSurface.h"
#include "SlateUI/Interface/MotionIntegrator/Api/MotionIntegrator.h"
#include "SlateUI/Interface/SymbolSpecification/Api/SymbolSpecification.h"

#include <cstdint>

namespace Slate
{

//------------------------------------------------------------------------------------------------------------------------
//                                                    WHAT A MENU PRESENTS
//------------------------------------------------------------------------------------------------------------------------

/// 🧩 One row of a context menu.
/// note  Captions are BORROWED and must outlive the tick. Nothing here is copied.
struct MenuItem
{
    const char*   Caption  = "";      // [-] - the row's text
    SymbolSubject Glyph    = SymbolSubject::SubjectCount;   // [-] - `SubjectCount` draws no glyph
    bool          Enabled  = true;    // [-] - a disabled row is drawn quiet and cannot be taken
    bool          Divides  = false;   // [-] - a rule is drawn above this row
};

/// 🧩 What one context menu offers.
struct MenuDeclaration
{
    const char*     Title     = "";        // [-] - the heading; empty draws no heading
    const MenuItem* Items     = nullptr;   // [-] - borrowed; outlives the tick
    std::uint32_t   ItemCount = 0u;        // [-] - zero records nothing at all
};

//------------------------------------------------------------------------------------------------------------------------
//                                                        THE MENU
//------------------------------------------------------------------------------------------------------------------------

/// 🧩 A context menu opened from a tile, placed clear of every widget the caller declares.
class ToolContextMenu
{
public:

    static constexpr std::uint32_t ItemLimit    = 12u;     // [-] - rows one menu may hold
    static constexpr std::uint32_t AvoidLimit   = 8u;      // [-] - boxes it can be asked to avoid

    // 📐 Measures in the options widget's own idiom, so the two read as one family.
    static constexpr float MenuWidth   = 180.0f;   // [px]
    static constexpr float RowHeight   = 34.0f;    // [px]
    static constexpr float HeadHeight  = 30.0f;    // [px] - only when a title is declared
    static constexpr float MenuPadY    = 6.0f;     // [px]
    static constexpr float MenuRadius  = 12.0f;    // [px]
    static constexpr float AnchorGap   = 6.0f;     // [px] - clear distance from the tile

    Deliver<bool> ConstructToolContextMenu(MotionIntegrator& Motion,
                                           RecordingSurface& Surface,
                                           const ThemeProfile& Appearance);

    void Advance(const PointerCondition& Sampled, double Elapsed);

    /// 🧩 Opens the menu against a tile. A menu already open is moved to the new tile.
    /// in    Anchor  [px] the tile pressed
    void Open(const PlaneExtent& Anchor);

    /// 🧩 Closes the menu. Safe when it is already closed.
    void Close();

    bool Standing() const { return Opened; }

    /// 🧩 Declares a box the menu must not cover. Cleared at the head of every `Record`.
    /// note  🔴 The caller states these each tick from what it actually drew, because the widgets move.
    ///        A box retained across ticks is the ghost that steers the layout after its widget is gone.
    void Avoid(const PlaneExtent& Extent);

    /// 🧩 Records the menu, if it stands, and reports the row taken.
    /// in    Bounds        [px] the extent the menu must stay inside — the viewport leaf, not the window
    /// in    Declared      [-]  the rows to present
    /// out   Taken         [-]  the row index taken this tick; untouched when nothing was taken
    /// out   PointerTaken  [-]  set when the menu consumed the contact
    /// out   Result        [-]  true when a row was taken, so a caller may branch on the return alone
    /// note  🔴 The menu CLOSES ITSELF on a take and on a press outside it. A context menu that lingers
    ///        after its command has run is a menu the artist has to dismiss twice.
    /// note  ⚠️ When no placement is free the menu records NOTHING and stays closed rather than drawing
    ///        over a widget. That is the requirement, and it is visible rather than silent.
    /// cost  🚩
    /// tag   api, nonallocating, nonthrowing
    Deliver<bool> Record(const PlaneExtent& Bounds,
                         const MenuDeclaration& Declared,
                         std::uint32_t& Taken,
                         bool& PointerTaken);

    /// 🧩 The box the menu last occupied, so a second menu can avoid the first.
    /// out   Result  [px] a zero-area extent while the menu is closed
    const PlaneExtent& Occupies() const { return Occupied; }

    void Reset();

private:

    float Scale() const;
    float MeasureHeight(const MenuDeclaration& Declared) const;
    bool  Pressed(ControlIdentity Target, const PlaneExtent& Extent);

    MotionIntegrator*   Motion     = nullptr;
    RecordingSurface*   Surface    = nullptr;
    const ThemeProfile* Appearance = nullptr;
    ControlIndex        Interaction = {};
    PointerCondition    Pointer    = {};

    ControlIdentity Rows[ItemLimit] = {};

    PlaneExtent   Avoided[AvoidLimit] = {};
    std::uint32_t AvoidCount          = 0u;

    PlaneExtent Anchored = {};   // [px] - the tile it was opened from
    PlaneExtent Occupied = {};   // [px] - what it drew this tick
    bool        Opened   = false;
};

}   // namespace Slate
