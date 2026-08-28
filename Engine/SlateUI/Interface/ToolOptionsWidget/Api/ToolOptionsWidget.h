//============================================================================================================================================
//                                                        TOOLOPTIONSWIDGET.H
//============================================================================================================================================
// 🧩 The floating tool-options panel — a draggable card of controls for the active tool, collapsing to a
//    pill that still names what it holds.
//
// 📐 Built to `References/ToolOptionsWidget.html`. The reference declares exactly four kinds of control —
//    a slider, a segmented choice, a toggle and a colour bar — and every tool's options are a table of
//    them. That is the whole grammar, so it is the whole grammar here: a widget that grew a fifth kind
//    per tool would be five widgets wearing one name.
//
// 🔴 SELECT AND THE GIZMO ARE ONE WIDGET, NOT TWO. They are one thing an artist does — choose something,
//    then move it — and splitting them would put the gizmo's visibility switch in a panel the artist has
//    to go and find while the thing they want to move is already selected.
//
// 🔴 THE COLLAPSED FORM IS A PILL, NOT A BUBBLE. The reference collapses to a 56 px round bubble showing
//    only an icon, which says nothing about which tool's options are inside it. The pill keeps the title
//    and a chevron, so a collapsed widget still answers the question it exists to answer.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "SketchToolset/SketchTool/SelectionOptions/Api/SelectionOptions.h"
#include "SlateUI/Interface/AppearanceSpecification/Api/AppearanceSpecification.h"
#include "SlateUI/Interface/ControlIndex/Api/ControlIndex.h"
#include "SlateUI/Interface/InterfaceExchange/Api/RecordingSurface.h"
#include "SlateUI/Interface/MotionIntegrator/Api/MotionIntegrator.h"

#include <cstdint>

namespace Slate
{

//------------------------------------------------------------------------------------------------------------------------
//                                                    WHAT A CONTROL IS
//------------------------------------------------------------------------------------------------------------------------

/// 🧩 The four kinds of control the reference declares, and the only four.
/// tag   guarantee
enum class OptionControl : std::uint32_t
{
    Slider    = 0u,   // [-] - a value pill beside a track with a dragged knob
    Segmented = 1u,   // [-] - one of a short row of named choices
    Toggle    = 2u,   // [-] - a switch, on or off
    Swatches  = 3u    // [-] - one of a row of colours
};

/// 🧩 One row of the widget's body, borrowed from the caller for a single tick.
/// note  ⚠️ `Reading` POINTS AT THE CALLER'S DATUM and is written through. The widget owns no options; it
///        edits the ones the tool already has, so there is never a second copy to fall out of step with
///        the first. `Selected` is the same arrangement for the three discrete kinds.
/// note  📝 `Unit` is drawn in its own darker cell exactly as the reference does, which is what stops a
///        number and its unit reading as one longer number.
/// tag   guarantee, nonallocating, nonthrowing
struct OptionDeclaration
{
    OptionControl       Kind        = OptionControl::Slider;
    const char*         Caption     = "";        // [-] - the row's label
    const char*         Unit        = "";        // [-] - drawn beside a slider's value; empty draws no cell

    float*              Reading     = nullptr;   // [-] - the caller's datum, for a slider
    float               Minimum     = 0.0f;      // [-]
    float               Maximum     = 1.0f;      // [-]

    std::uint32_t*      Selected      = nullptr;   // [-] - the caller's index, for a segmented row or swatches
    const char* const*  Options     = nullptr;   // [-] - the segmented captions
    const SymbolSubject* Glyphs     = nullptr;   // [-] - optional, one per option; drawn instead of the caption
    const ThemeToken*   Colours     = nullptr;   // [-] - the swatch colours
    std::uint32_t       OptionCount = 0u;

    bool*               Taken       = nullptr;   // [-] - the caller's flag, for a toggle
};

//------------------------------------------------------------------------------------------------------------------------
//                                                      THE WIDGET
//------------------------------------------------------------------------------------------------------------------------

/// 🧩 A floating, draggable options card that persists until the artist dismisses it.
/// note  🔴 IT ALWAYS PERSISTS. It is not a popup that a click elsewhere closes — an artist adjusting a
///        tolerance and then clicking in the viewport to test it must not have to reopen the panel to
///        adjust it again. Only its own hide action closes it.
/// tag   owning, nonallocating, nonthrowing
class ToolOptionsWidget
{
public:

    static constexpr std::uint32_t RowLimit    = 8u;      // [-] - bounded rows per tool
    static constexpr std::uint32_t OptionLimit = 8u;      // [-] - bounded choices per row

    // 📐 The reference's own measures, at a display scale of one.
    static constexpr float PanelWidth   = 300.0f;   // [px] - `--panel-w`
    static constexpr float PanelRadius  = 20.0f;    // [px] - `--panel-radius`
    static constexpr float ValueRadius  = 13.0f;    // [px] - `--value-radius`
    static constexpr float RowHeight    = 44.0f;    // [px] - `--row-h`
    static constexpr float HeaderHeight = 52.0f;    // [px] - 12 + 18 icon + ... + 12, measured
    static constexpr float BodyPadding  = 18.0f;    // [px]
    static constexpr float BodyGap      = 16.0f;    // [px]
    static constexpr float PillHeight   = 40.0f;    // [px] - the collapsed form

    Deliver<bool> ConstructToolOptionsWidget(MotionIntegrator& Motion,
                                             RecordingSurface& Surface,
                                             const ThemeProfile& Appearance);

    void Advance(const PointerCondition& Sampled, double Elapsed);

    /// 🧩 Records the widget, or its pill, and edits the caller's declarations in place.
    /// in    Bounds  [px] the extent the widget may be dragged within — the viewport leaf, so it can never
    ///                    be dragged behind a drawer and stranded there
    /// out   PointerTaken  [-] set when the widget consumed the contact, so the viewport does not also
    ///                        read it as a click into the scene
    /// note  🔴 The caller passes its OWN storage in `Declared`; every edit lands there directly.
    Deliver<bool> Record(const PlaneExtent& Bounds,
                         const char* Title,
                         SymbolSubject Glyph,
                         OptionDeclaration* Declared,
                         std::uint32_t DeclaredCount,
                         bool& PointerTaken);

    /// 🧩 Whether the card is expanded, collapsed to its pill, or dismissed entirely.
    bool Shown() const { return Presented; }
    bool Collapsed() const { return Folded; }
    void Show()  { Presented = true; }
    void Hide()  { Presented = false; }
    void Toggle() { Presented = !Presented; }

    /// 🧩 Where the widget sits, so a host may seat it once and let the artist move it thereafter.
    void Seat(float X, float Y) { PlacedX = X; PlacedY = Y; Seated = true; }

    void Reset();

private:

    float MeasureBody(const OptionDeclaration* Declared, std::uint32_t DeclaredCount) const;
    float Scale() const;

    void RecordHeader(const PlaneExtent& Header, const char* Title, SymbolSubject Glyph, bool& PointerTaken);
    void RecordPill(const PlaneExtent& Bounds, const char* Title, SymbolSubject Glyph, bool& PointerTaken);
    void RecordSlider(const PlaneExtent& Row, OptionDeclaration& Declared, std::uint32_t Index, bool& PointerTaken);
    void RecordSegmented(const PlaneExtent& Row, OptionDeclaration& Declared, std::uint32_t Index, bool& PointerTaken);
    void RecordToggle(const PlaneExtent& Row, OptionDeclaration& Declared, std::uint32_t Index, bool& PointerTaken);
    void RecordSwatches(const PlaneExtent& Row, OptionDeclaration& Declared, std::uint32_t Index, bool& PointerTaken);
    bool Pressed(ControlIdentity Target, const PlaneExtent& Extent);

    MotionIntegrator*   Motion     = nullptr;
    RecordingSurface*   Surface    = nullptr;
    const ThemeProfile* Appearance = nullptr;
    ControlIndex        Interaction = {};
    PointerCondition    Pointer    = {};

    // 📝 One identity per row plus the header's three actions, registered once at construction. Rows are
    //    addressed by ordinal, so a tool with fewer rows simply leaves the tail unused.
    ControlIdentity     RowControls[RowLimit] = {};
    ControlIdentity     SelectedControls[RowLimit * OptionLimit] = {};
    ControlIdentity     HeaderGrip = {};
    ControlIdentity     CollapseAction = {};
    ControlIdentity     HideAction = {};
    ControlIdentity     PillAction = {};

    float  PlacedX   = 64.0f;    // [px] - the reference's own left
    float  PlacedY   = 110.0f;   // [px] - and its top
    bool   Seated    = false;
    bool   Presented = true;
    bool   Folded    = false;

    // 📝 Dragging is held here rather than in `ControlIndex` because the thing being dragged is the
    //    WIDGET, not a control's value — the integrator's grab origin answers a different question.
    bool   Dragging  = false;
    float  DragOriginX = 0.0f;   // [px] - where the contact arrived, relative to the card's corner
    float  DragOriginY = 0.0f;   // [px]
    bool   Travelled = false;    // [-]  - whether this drag moved far enough to not be a click
};

}   // namespace Slate
