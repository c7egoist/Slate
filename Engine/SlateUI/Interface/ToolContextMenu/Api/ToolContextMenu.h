//============================================================================================================================================
//                                                         TOOLCONTEXTMENU.H
//============================================================================================================================================
// 🧩 The popup a construction tool raises to ask for its parameters — Bevel's distance, Chamfer's setback
//    — placed in a corner that is free, and never on top of another widget.
//
// 🔴 THIS IS A PARAMETER POPUP, NOT A COMMAND LIST. It was first built as a menu of five rows — Bevel,
//    Chamfer, Trim, Cut, Add — and that was wrong. The tools already have tiles in the catalogue; a menu
//    listing them again is a second way to start the same command. What the artist has no way to say is
//    HOW FAR: the bevel distance, the chamfer setback, the number of segments. So the popup carries the
//    tool's parameters and an Apply/Cancel pair, and it appears AFTER the tool is chosen.
//
// 🔴 IT IS THE OPTIONS WIDGET'S BODY IN A SMALLER FRAME. `References/ToolOptionsWidget.html` defines one
//    grammar of controls — slider, segmented, toggle, swatches — and this popup presents exactly that
//    grammar, through the very same renderers, so a slider here cannot drift from a slider there. The
//    difference is the frame: a popup is transient, anchored, and ends in Apply or Cancel, where the
//    widget persists and edits live.
//
// 🔴 THE PLACEMENT IS STILL THE FEATURE. A popup that lands on top of the options widget hides the thing
//    the artist just used. The corner is chosen against the boxes actually occupied this tick, not
//    against a layout someone assumed. `PlaceMenuClear` does the arithmetic; this unit does the drawing.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "Foundation/ExtentBands.h"
#include "SlateUI/Interface/AppearanceSpecification/Api/AppearanceSpecification.h"
#include "SlateUI/Interface/ControlIndex/Api/ControlIndex.h"
#include "SlateUI/Interface/InterfaceExchange/Api/RecordingSurface.h"
#include "SlateUI/Interface/MotionIntegrator/Api/MotionIntegrator.h"
#include "SlateUI/Interface/OptionControls/Api/OptionControls.h"
#include "SlateUI/Interface/SymbolSpecification/Api/SymbolSpecification.h"

#include <cstdint>

namespace Slate
{

//------------------------------------------------------------------------------------------------------------------------
//                                                    WHAT A POPUP PRESENTS
//------------------------------------------------------------------------------------------------------------------------

/// 🧩 How the artist left a parameter popup.
/// note  📝 `Standing` is the ordinary answer: the popup is open and the artist is still adjusting. The
///        other two are terminal and the caller acts on them once.
/// tag   guarantee
enum class PopupVerdict : std::uint32_t
{
    Standing = 0u,   // [-] - still open, nothing decided
    Applied  = 1u,   // [-] - Apply was taken; the parameters are final
    Cancelled = 2u   // [-] - Cancel, Escape, or a press outside
};

/// 🧩 What one parameter popup asks for.
/// note  ⚠️ Rows are BORROWED and written through, exactly as the options widget does it. The popup owns
///        no parameters; it edits the caller's, so a preview drawn from the same data cannot fall out of
///        step with what the popup shows.
struct PopupDeclaration
{
    const char*        Title     = "";        // [-] - the heading, e.g. "Bevel"
    SymbolSubject      Glyph     = SymbolSubject::SubjectCount;
    OptionDeclaration* Rows      = nullptr;   // [-] - borrowed; outlives the tick
    std::uint32_t      RowCount  = 0u;
};

//------------------------------------------------------------------------------------------------------------------------
//                                                        THE POPUP
//------------------------------------------------------------------------------------------------------------------------

/// 🧩 A parameter popup opened by a construction tool, placed clear of every widget the caller declares.
/// tag   owning, nonallocating, nonthrowing
class ToolContextMenu
{
public:

    static constexpr std::uint32_t RowLimit    = 6u;      // [-] - parameters one popup may ask for
    static constexpr std::uint32_t OptionLimit = 8u;      // [-] - options within one segmented row
    static constexpr std::uint32_t AvoidLimit  = 8u;      // [-] - boxes it can be asked to avoid

    // 📐 Narrower than the 300 px options card, because a popup asks for two or three figures rather than
    //    holding a whole tool's settings — but every inner measure is the reference's own.
    static constexpr float PopupWidth   = 260.0f;   // [px]
    static constexpr float HeadHeight   = 44.0f;    // [px]
    static constexpr float FootHeight   = 52.0f;    // [px] - the Apply / Cancel pair
    static constexpr float BodyPadding  = 14.0f;    // [px]
    static constexpr float BodyGap      = 14.0f;    // [px]
    static constexpr float RowHeight    = 44.0f;    // [px] - `--row-h`
    static constexpr float SegmentHeight = 40.0f;   // [px]
    static constexpr float CaptionPoint =  12.0f;   // [px]
    static constexpr float CaptionGap   =   8.0f;   // [px]
    static constexpr float PopupRadius  = 20.0f;    // [px] - `--panel-radius`
    static constexpr float ActionHeight = 36.0f;    // [px]
    static constexpr float ActionRadius = 12.0f;    // [px]
    static constexpr float AnchorGap    =  6.0f;    // [px] - clear distance from the anchor

    Deliver<bool> ConstructToolContextMenu(MotionIntegrator& Motion,
                                           RecordingSurface& Surface,
                                           const ThemeProfile& Appearance);

    void Advance(const PointerCondition& Sampled, double Elapsed);

    /// 🧩 Opens the popup against an anchor — the tile pressed, or the point clicked in the viewport.
    /// in    Anchor  [px] what the popup belongs to; it is placed in a free corner of this
    /// note  📝 Opening an already-open popup moves it, which is what a second tool press should do.
    void Open(const PlaneExtent& Anchor);

    /// 🧩 Closes the popup. Safe when it is already closed.
    void Close();

    bool Standing() const { return Opened; }

    /// 🧩 Declares a box the popup must not cover. Cleared at the head of every `Record`.
    /// note  🔴 The caller states these each tick from what it actually drew, because the widgets move.
    ///        A box retained across ticks is the ghost that steers the layout after its widget is gone.
    void Avoid(const PlaneExtent& Extent);

    /// 🧩 Records the popup, if it stands, editing the caller's parameters in place.
    /// in    Bounds        [px] the extent the popup must stay inside — the viewport leaf
    /// in    Declared      [-]  the title and the parameter rows
    /// out   PointerTaken  [-]  set when the popup consumed the contact
    /// out   Result        [-]  `Applied` or `Cancelled` exactly once, `Standing` otherwise
    /// note  🔴 THE POPUP CLOSES ITSELF on Apply and on Cancel, and reports which. A caller that acted on
    ///        `Standing` would apply the operation every frame the popup was open.
    /// note  ⚠️ When no placement is free the popup records NOTHING and stays closed rather than drawing
    ///        over a widget. That is the requirement, and it is visible rather than silent.
    /// cost  🚩
    /// tag   api, nonallocating, nonthrowing
    Deliver<PopupVerdict> Record(const PlaneExtent& Bounds,
                                 const PopupDeclaration& Declared,
                                 bool& PointerTaken);

    /// 🧩 The box the popup last occupied, so a second popup can avoid the first.
    /// out   Result  [px] a zero-area extent while the popup is closed
    const PlaneExtent& Occupies() const { return Occupied; }

    void Reset();

private:

    float Scale() const;
    float MeasureBody(const PopupDeclaration& Declared) const;
    bool  Pressed(ControlIdentity Target, const PlaneExtent& Extent);

    MotionIntegrator*   Motion      = nullptr;
    RecordingSurface*   Surface     = nullptr;
    const ThemeProfile* Appearance  = nullptr;
    ControlIndex        Interaction = {};
    PointerCondition    Pointer     = {};

    OptionControlPalette Controls = {};   // [-] - the shared renderers; the popup owns no drawing of its own

    ControlIdentity RowControls[RowLimit] = {};
    ControlIdentity SelectedControls[RowLimit * OptionLimit] = {};
    ControlIdentity ApplyAction  = {};
    ControlIdentity CancelAction = {};

    PlaneExtent   Avoided[AvoidLimit] = {};
    std::uint32_t AvoidCount          = 0u;

    PlaneExtent Anchored = {};   // [px] - what it was opened against
    PlaneExtent Occupied = {};   // [px] - what it drew this tick
    bool        Opened   = false;
};

}   // namespace Slate
