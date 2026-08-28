//============================================================================================================================================
//                                                       TOOLOPTIONSWIDGET.CPP
//============================================================================================================================================
// 📐 Measures and colours from `References/ToolOptionsWidget.html`, stated once here as named constants so
//    the sheet and the code can be compared line for line.

#include "SlateUI/Interface/ToolOptionsWidget/Api/ToolOptionsWidget.h"

#include <cstdio>

namespace Slate
{

namespace
{

// 📐 The reference's design tokens. Named for what they are in the sheet, so a reader with the file open
//    can find each one.
constexpr ThemeToken PanelGround   = Covering(0x131315u);   // --panel-bg
constexpr ThemeToken PanelHead     = Covering(0x1b1b1eu);   // --panel-head
constexpr ThemeToken ValueGround   = Covering(0x232326u);   // --value-bg
constexpr ThemeToken ValueBlack    = Covering(0x0a0a0bu);   // --value-black
constexpr ThemeToken ValueNumber   = Covering(0x131315u);   // --value-num
constexpr ThemeToken ValueUnit     = Covering(0x33333au);   // --value-unit
constexpr ThemeToken PanelOutline  = Partial (0xFFFFFFu, 0.22);   // --outline
constexpr ThemeToken TrackGround   = Covering(0x2f2f33u);   // --track-bg
constexpr ThemeToken TrackFill     = Covering(0x8a8a8eu);   // --track-fill
constexpr ThemeToken KnobGround    = Covering(0xf4f4f5u);   // --knob
constexpr ThemeToken AccentGround  = Covering(0x4a90e2u);   // --accent
constexpr ThemeToken ColourPrimary = Covering(0xe9e9ecu);   // --text-primary
constexpr ThemeToken ColourMuted   = Covering(0x7b7b82u);   // --text-muted
constexpr ThemeToken ColourValue   = Covering(0xf2f2f4u);   // --text-value

// 📐 Measures, all at a display scale of one.
constexpr float HeaderPadX     = 14.0f;
constexpr float HeaderGlyph    = 18.0f;
constexpr float HeaderButton   = 28.0f;
constexpr float HeaderButtonR  =  9.0f;
constexpr float TitlePoint     = 16.0f;
constexpr float SubPoint       = 10.0f;
constexpr float CaptionPoint   = 12.0f;
constexpr float ValuePoint     = 13.0f;
constexpr float ValuePillWidth = 96.0f;
constexpr float ValueUnitWidth = 30.0f;
constexpr float TrackHeight    = 32.0f;
constexpr float KnobDiameter   = 26.0f;
constexpr float SegmentHeight  = 40.0f;
constexpr float SegmentRadius  = 12.0f;
constexpr float SwitchWidth    = 52.0f;
constexpr float SwitchHeight   = 30.0f;
constexpr float SwitchNub      = 24.0f;
constexpr float SwatchSize     = 26.0f;
constexpr float SwatchRadius   =  8.0f;
constexpr float CaptionGap     =  8.0f;
constexpr float DragThreshold  =  3.0f;   // 📐 the reference's own `Math.abs(dx) > 3`

float Clamped(float Figure, float Lowest, float Highest)
{
    if (Figure < Lowest)  return Lowest;
    if (Figure > Highest) return Highest;
    return Figure;
}

}   // namespace

//------------------------------------------------------------------------------------------------------------------------
//                                                       CONSTRUCTION
//------------------------------------------------------------------------------------------------------------------------

Deliver<bool> ToolOptionsWidget::ConstructToolOptionsWidget(MotionIntegrator& IncomingMotion,
                                                            RecordingSurface& IncomingSurface,
                                                            const ThemeProfile& IncomingAppearance)
{
    if (Motion != nullptr)
        return Deliver<bool>::Refuse({ RefusalReason::ContentUnsupported,
                                       "a tool options widget construction stands" });

    Motion     = &IncomingMotion;
    Surface    = &IncomingSurface;
    Appearance = &IncomingAppearance;

    if (!Interaction.AttachMotion(IncomingMotion).Resolved)
        return Deliver<bool>::Refuse({ RefusalReason::ExtentExhausted,
                                       "tool options interaction was rejected" });

    // 📝 Every identity is registered up front and addressed by ordinal. Registering on demand would give
    //    a row a different identity as soon as the tool above it gained a control, and the integrator's
    //    hover and press animations would follow the ordinal rather than the row.
    for (std::uint32_t Index = 0u; Index < RowLimit; ++Index)
    {
        const Deliver<ControlIdentity> Registered = Interaction.Register();
        if (!Registered.Resolved)
            return Deliver<bool>::Refuse(Registered.Error);
        RowControls[Index] = Registered.Resolve();
    }

    for (std::uint32_t Index = 0u; Index < RowLimit * OptionLimit; ++Index)
    {
        const Deliver<ControlIdentity> Registered = Interaction.Register();
        if (!Registered.Resolved)
            return Deliver<bool>::Refuse(Registered.Error);
        SelectedControls[Index] = Registered.Resolve();
    }

    ControlIdentity* const Actions[4] = { &HeaderGrip, &CollapseAction, &HideAction, &PillAction };
    for (ControlIdentity* Action : Actions)
    {
        const Deliver<ControlIdentity> Registered = Interaction.Register();
        if (!Registered.Resolved)
            return Deliver<bool>::Refuse(Registered.Error);
        *Action = Registered.Resolve();
    }

    return Deliver<bool>::Result(true);
}

void ToolOptionsWidget::Advance(const PointerCondition& Sampled, double Elapsed)
{
    Pointer = Sampled;
    Interaction.Advance(Sampled, Elapsed);
}

void ToolOptionsWidget::Reset()
{
    Interaction.Reset();
    Dragging  = false;
    Travelled = false;
    Folded    = false;
    Presented = true;
    Seated    = false;
}

float ToolOptionsWidget::Scale() const
{
    return Appearance != nullptr ? static_cast<float>(Appearance->Measure.DisplayScale) : 1.0f;
}

bool ToolOptionsWidget::Pressed(ControlIdentity Target, const PlaneExtent& Extent)
{
    const bool Hovered = Extent.Encloses(Pointer.PositionX, Pointer.PositionY);
    if (Hovered && Pointer.ContactPressed)
        Interaction.Grab(Target, ControlPart::Body);
    Interaction.DeclareHovered(Target, Hovered, 130.0);
    return Hovered && Interaction.Released(Target);
}

//------------------------------------------------------------------------------------------------------------------------
//                                                       MEASURING
//------------------------------------------------------------------------------------------------------------------------

float ToolOptionsWidget::MeasureBody(const OptionDeclaration* Declared, std::uint32_t DeclaredCount) const
{
    const float Applied = Scale();
    const std::uint32_t Rows = DeclaredCount < RowLimit ? DeclaredCount : RowLimit;
    if (Rows == 0u)
        return 0.0f;

    float Total = BodyPadding * 2.0f * Applied;
    for (std::uint32_t Index = 0u; Index < Rows; ++Index)
    {
        // 📝 Every kind is a caption line above a control of the standing row height, so the body's
        //    extent is arithmetic rather than a per-kind table that could disagree with what is drawn.
        Total += (CaptionPoint + CaptionGap) * Applied;
        Total += (Declared[Index].Kind == OptionControl::Segmented ? SegmentHeight : RowHeight) * Applied;
        if (Index + 1u < Rows)
            Total += BodyGap * Applied;
    }
    return Total;
}

//------------------------------------------------------------------------------------------------------------------------
//                                                       RECORDING
//------------------------------------------------------------------------------------------------------------------------

Deliver<bool> ToolOptionsWidget::Record(const PlaneExtent& Bounds,
                                        const char* Title,
                                        SymbolSubject Glyph,
                                        OptionDeclaration* Declared,
                                        std::uint32_t DeclaredCount,
                                        bool& PointerTaken)
{
    if (Surface == nullptr || Appearance == nullptr)
        return Deliver<bool>::Refuse({ RefusalReason::ContentUnsupported,
                                       "the widget was recorded before it was constructed" });
    // 🔴 A HIDDEN WIDGET OCCUPIES NOTHING. Leaving the last box standing would have every context menu
    //    keep avoiding a widget that is no longer drawn — the ghost of a panel steering the layout.
    if (!Presented)
    {
        Occupied = {};
        return Deliver<bool>::Result(false);
    }

    const float Applied = Scale();

    // 📝 Seated once against the bounds it is given, so a widget that has never been dragged appears
    //    where the reference puts it rather than at the origin of a viewport that may not start at zero.
    if (!Seated)
    {
        PlacedX = Bounds.MinimumX + 64.0f * Applied;
        PlacedY = Bounds.MinimumY + 110.0f * Applied;
        Seated  = true;
    }

    if (Folded)
    {
        RecordPill(Bounds, Title, Glyph, PointerTaken);
        return Deliver<bool>::Result(true);
    }

    const std::uint32_t Rows = DeclaredCount < RowLimit ? DeclaredCount : RowLimit;
    const float Width  = PanelWidth * Applied;
    const float Header = HeaderHeight * Applied;
    const float Body   = MeasureBody(Declared, Rows);

    // 🔴 THE WIDGET CANNOT BE DRAGGED OFF THE EDGE AND STRANDED. The reference clamps to the window; this
    //    clamps to the viewport leaf, which is the same guarantee against the extent that actually
    //    contains it. A widget dragged behind a drawer could never be dragged back.
    PlacedX = Clamped(PlacedX, Bounds.MinimumX, Bounds.MaximumX - Width);
    PlacedY = Clamped(PlacedY, Bounds.MinimumY, Bounds.MaximumY - Header);

    const PlaneExtent Card   = Spanning(PlacedX, PlacedY, Width, Header + Body);
    const PlaneExtent HeadOf = Spanning(PlacedX, PlacedY, Width, Header);

    // 🔴 Published so a context menu can avoid it. Recorded from the box actually drawn, not from
    //    `PlacedX/PlacedY`, so a clamped or collapsed widget reports where it really is.
    Occupied = Card;

    Surface->Ground(Card, PanelGround, PanelRadius * Applied, CornerAll);
    Surface->Edge(Card, PanelOutline, 1.0f, PanelRadius * Applied, CornerAll);
    Surface->Ground(HeadOf, PanelHead, PanelRadius * Applied, CornerLeadingUpper | CornerTrailingUpper);

    RecordHeader(HeadOf, Title, Glyph, PointerTaken);

    // 📝 Confined to the card, so a row measured wrongly is clipped rather than drawn across the
    //    viewport — a visible bug rather than a confusing one.
    Surface->Confine(Card);

    float Cursor = PlacedY + Header + BodyPadding * Applied;
    for (std::uint32_t Index = 0u; Index < Rows; ++Index)
    {
        OptionDeclaration& Row = Declared[Index];

        Surface->TextRun(PlacedX + BodyPadding * Applied,
                         Cursor + CaptionPoint * Applied,
                         ColourMuted, Row.Caption, CaptionPoint * Applied);
        Cursor += (CaptionPoint + CaptionGap) * Applied;

        const float Height = (Row.Kind == OptionControl::Segmented ? SegmentHeight : RowHeight) * Applied;
        const PlaneExtent Extent = Spanning(PlacedX + BodyPadding * Applied, Cursor,
                                            Width - BodyPadding * 2.0f * Applied, Height);

        switch (Row.Kind)
        {
            case OptionControl::Slider:    RecordSlider(Extent, Row, Index, PointerTaken);    break;
            case OptionControl::Segmented: RecordSegmented(Extent, Row, Index, PointerTaken); break;
            case OptionControl::Toggle:    RecordToggle(Extent, Row, Index, PointerTaken);    break;
            case OptionControl::Swatches:  RecordSwatches(Extent, Row, Index, PointerTaken);  break;
        }

        Cursor += Height + BodyGap * Applied;
    }

    Surface->Release();

    // 🔴 THE WHOLE CARD SWALLOWS THE CONTACT. Without this a press that misses every control but lands
    //    on the panel falls through to the viewport and deselects whatever the artist was adjusting the
    //    options FOR.
    if (Card.Encloses(Pointer.PositionX, Pointer.PositionY))
        PointerTaken = true;

    return Deliver<bool>::Result(true);
}

//------------------------------------------------------------------------------------------------------------------------
//                                                        HEADER
//------------------------------------------------------------------------------------------------------------------------

void ToolOptionsWidget::RecordHeader(const PlaneExtent& Header, const char* Title,
                                     SymbolSubject Glyph, bool& PointerTaken)
{
    const float Applied = Scale();
    const float MiddleY = (Header.MinimumY + Header.MaximumY) * 0.5f;

    const PlaneExtent GlyphExtent = Spanning(Header.MinimumX + HeaderPadX * Applied,
                                             MiddleY - HeaderGlyph * 0.5f * Applied,
                                             HeaderGlyph * Applied, HeaderGlyph * Applied);
    Surface->Stroke(Glyph, GlyphExtent, ColourPrimary);

    const float TitleX = GlyphExtent.MaximumX + 10.0f * Applied;
    Surface->TextRun(TitleX, MiddleY - 1.0f * Applied, ColourPrimary, Title,
                     TitlePoint * Applied, 0.0f, false, FontWeight::Semibold);
    Surface->TextRun(TitleX, MiddleY + SubPoint * Applied + 3.0f * Applied,
                     ColourMuted, "Tool Options", SubPoint * Applied);

    // 📝 The two actions sit at the trailing edge, collapse then hide, in the reference's order.
    const float ButtonY = MiddleY - HeaderButton * 0.5f * Applied;
    const PlaneExtent HideExtent = Spanning(Header.MaximumX - (HeaderPadX + HeaderButton) * Applied,
                                            ButtonY, HeaderButton * Applied, HeaderButton * Applied);
    const PlaneExtent FoldExtent = Spanning(HideExtent.MinimumX - (HeaderButton + 6.0f) * Applied,
                                            ButtonY, HeaderButton * Applied, HeaderButton * Applied);

    for (std::uint32_t Which = 0u; Which < 2u; ++Which)
    {
        const PlaneExtent& Extent = Which == 0u ? FoldExtent : HideExtent;
        const ControlIdentity Target = Which == 0u ? CollapseAction : HideAction;
        const bool Hovered = Extent.Encloses(Pointer.PositionX, Pointer.PositionY);

        Surface->Ground(Extent, Hovered ? ValueGround : PanelGround,
                        HeaderButtonR * Applied, CornerAll);
        Surface->Stroke(Which == 0u ? SymbolSubject::ChevronDown : SymbolSubject::CrossClose,
                        Spanning(Extent.MinimumX + 6.0f * Applied, Extent.MinimumY + 6.0f * Applied,
                                 Extent.Width() - 12.0f * Applied, Extent.Height() - 12.0f * Applied),
                        Hovered ? ColourPrimary : ColourMuted);

        if (Pressed(Target, Extent))
        {
            if (Which == 0u) Folded = true;
            else             Presented = false;
        }
        if (Hovered)
            PointerTaken = true;
    }

    // 🔴 THE HEADER IS THE GRIP, MINUS ITS BUTTONS. The reference refuses a drag that begins on a
    //    `.hdr-btn`, or pressing collapse while moving the mouse a pixel would drag instead of collapse.
    const bool OverAction = FoldExtent.Encloses(Pointer.PositionX, Pointer.PositionY)
                         || HideExtent.Encloses(Pointer.PositionX, Pointer.PositionY);

    if (!OverAction && Header.Encloses(Pointer.PositionX, Pointer.PositionY) && Pointer.ContactPressed)
    {
        Dragging    = true;
        Travelled   = false;
        DragOriginX = Pointer.PositionX - Header.MinimumX;
        DragOriginY = Pointer.PositionY - Header.MinimumY;
        Interaction.Grab(HeaderGrip, ControlPart::Body);
    }

    if (Dragging)
    {
        // 🔴 READ AS TRAVEL, NOT AS A POSITION. A drag that tracked `PositionX` directly would snap the
        //    card's corner to the pointer on the first frame, however far into the header the artist
        //    happened to press.
        if (Pointer.TravelX * Pointer.TravelX + Pointer.TravelY * Pointer.TravelY >
            DragThreshold * DragThreshold * Applied * Applied)
            Travelled = true;

        PlacedX = Pointer.PositionX - DragOriginX;
        PlacedY = Pointer.PositionY - DragOriginY;
        PointerTaken = true;

        if (!Pointer.ContactHeld || Pointer.ContactReleased)
            Dragging = false;
    }
}

//------------------------------------------------------------------------------------------------------------------------
//                                                         PILL
//------------------------------------------------------------------------------------------------------------------------

void ToolOptionsWidget::RecordPill(const PlaneExtent& Bounds, const char* Title,
                                   SymbolSubject Glyph, bool& PointerTaken)
{
    const float Applied = Scale();
    const float Height  = PillHeight * Applied;

    // 🔴 THE PILL IS AS WIDE AS ITS OWN CONTENTS. A fixed width would truncate one tool's name and leave
    //    another swimming — and the whole reason the collapsed form is a pill rather than the reference's
    //    round bubble is that it still says which tool's options are folded inside it.
    const float Caption = Surface->MeasureRun(Title, CaptionPoint * Applied);
    const float Width   = (HeaderPadX + HeaderGlyph + 10.0f) * Applied + Caption
                        + (10.0f + 14.0f + HeaderPadX) * Applied;

    PlacedX = Clamped(PlacedX, Bounds.MinimumX, Bounds.MaximumX - Width);
    PlacedY = Clamped(PlacedY, Bounds.MinimumY, Bounds.MaximumY - Height);

    const PlaneExtent Pill = Spanning(PlacedX, PlacedY, Width, Height);

    Occupied = Pill;
    const bool Hovered = Pill.Encloses(Pointer.PositionX, Pointer.PositionY);

    Surface->Ground(Pill, Hovered ? PanelHead : PanelGround, Height * 0.5f, CornerAll);
    Surface->Edge(Pill, PanelOutline, 1.0f, Height * 0.5f, CornerAll);

    const float MiddleY = (Pill.MinimumY + Pill.MaximumY) * 0.5f;
    Surface->Stroke(Glyph, Spanning(Pill.MinimumX + HeaderPadX * Applied,
                                    MiddleY - HeaderGlyph * 0.5f * Applied,
                                    HeaderGlyph * Applied, HeaderGlyph * Applied),
                    ColourPrimary);
    Surface->TextRun(Pill.MinimumX + (HeaderPadX + HeaderGlyph + 10.0f) * Applied,
                     MiddleY + CaptionPoint * 0.36f * Applied,
                     ColourPrimary, Title, CaptionPoint * Applied);
    Surface->Stroke(SymbolSubject::ChevronDown,
                    Spanning(Pill.MaximumX - (HeaderPadX + 14.0f) * Applied,
                             MiddleY - 7.0f * Applied, 14.0f * Applied, 14.0f * Applied),
                    ColourMuted);

    // 📝 The pill is both the grip and the expand action, exactly as the reference's bubble is: a press
    //    that travelled was a drag and must not also expand.
    if (Hovered && Pointer.ContactPressed)
    {
        Dragging    = true;
        Travelled   = false;
        DragOriginX = Pointer.PositionX - Pill.MinimumX;
        DragOriginY = Pointer.PositionY - Pill.MinimumY;
        Interaction.Grab(PillAction, ControlPart::Body);
    }

    if (Dragging)
    {
        if (Pointer.TravelX * Pointer.TravelX + Pointer.TravelY * Pointer.TravelY >
            DragThreshold * DragThreshold * Applied * Applied)
            Travelled = true;

        PlacedX = Pointer.PositionX - DragOriginX;
        PlacedY = Pointer.PositionY - DragOriginY;

        if (!Pointer.ContactHeld || Pointer.ContactReleased)
        {
            Dragging = false;
            if (!Travelled)
                Folded = false;
        }
    }

    if (Hovered)
        PointerTaken = true;
}

//------------------------------------------------------------------------------------------------------------------------
//                                                        SLIDER
//------------------------------------------------------------------------------------------------------------------------

void ToolOptionsWidget::RecordSlider(const PlaneExtent& Row, OptionDeclaration& Declared,
                                     std::uint32_t Index, bool& PointerTaken)
{
    if (Declared.Reading == nullptr)
        return;

    const float Applied = Scale();
    const float Span    = Declared.Maximum - Declared.Minimum;

    // 📐 A value pill of a fixed width, then the track filling what is left — the reference's own split.
    const float PillWidth = ValuePillWidth * Applied;
    const PlaneExtent Pill = Spanning(Row.MinimumX, Row.MinimumY, PillWidth, Row.Height());

    Surface->Ground(Pill, ValueNumber, ValueRadius * Applied, CornerAll);

    char Written[32] = {};
    std::snprintf(Written, sizeof(Written), "%.2f", static_cast<double>(*Declared.Reading));
    Surface->TextRun(Pill.MinimumX + 12.0f * Applied,
                     (Pill.MinimumY + Pill.MaximumY) * 0.5f + ValuePoint * 0.36f * Applied,
                     ColourValue, Written, ValuePoint * Applied);

    if (Declared.Unit != nullptr && Declared.Unit[0] != '\0')
    {
        const PlaneExtent UnitCell = Spanning(Pill.MaximumX - (ValueUnitWidth + 4.0f) * Applied,
                                              Pill.MinimumY + 4.0f * Applied,
                                              ValueUnitWidth * Applied, Pill.Height() - 8.0f * Applied);
        Surface->Ground(UnitCell, ValueUnit, (ValueRadius - 4.0f) * Applied, CornerAll);
        const float UnitRun = Surface->MeasureRun(Declared.Unit, SubPoint * Applied);
        Surface->TextRun(UnitCell.MinimumX + (UnitCell.Width() - UnitRun) * 0.5f,
                         (UnitCell.MinimumY + UnitCell.MaximumY) * 0.5f + SubPoint * 0.36f * Applied,
                         ColourMuted, Declared.Unit, SubPoint * Applied);
    }

    const PlaneExtent Track = Spanning(Pill.MaximumX + 10.0f * Applied,
                                       Row.MinimumY + (Row.Height() - TrackHeight * Applied) * 0.5f,
                                       Row.MaximumX - Pill.MaximumX - 10.0f * Applied,
                                       TrackHeight * Applied);
    Surface->Ground(Track, ValueBlack, Track.Height() * 0.5f, CornerAll);

    const float Fraction = Span > 0.0f
                         ? Clamped((*Declared.Reading - Declared.Minimum) / Span, 0.0f, 1.0f)
                         : 0.0f;
    const float Travel = Track.Width() - KnobDiameter * Applied;
    const float KnobX  = Track.MinimumX + KnobDiameter * 0.5f * Applied + Travel * Fraction;

    if (Fraction > 0.0f)
    {
        const PlaneExtent Filled = Spanning(Track.MinimumX + 3.0f * Applied,
                                            Track.MinimumY + 3.0f * Applied,
                                            (KnobX - Track.MinimumX - 3.0f * Applied),
                                            Track.Height() - 6.0f * Applied);
        Surface->Ground(Filled, TrackFill, Filled.Height() * 0.5f, CornerAll);
    }
    else
    {
        Surface->Ground(Spanning(Track.MinimumX + 3.0f * Applied, Track.MinimumY + 3.0f * Applied,
                                 Track.Width() - 6.0f * Applied, Track.Height() - 6.0f * Applied),
                        TrackGround, (Track.Height() - 6.0f * Applied) * 0.5f, CornerAll);
    }

    Surface->Medallion(KnobX, (Track.MinimumY + Track.MaximumY) * 0.5f,
                       KnobDiameter * 0.5f * Applied, KnobGround);

    // 🔴 THE KNOB IS DRAGGED WITH POINTER CAPTURE, exactly as the reference does: once the contact is
    //    grabbed the value follows the pointer even when it leaves the track, or a quick drag would
    //    silently stop tracking the moment the pointer slipped a few pixels above the row.
    const ControlIdentity Target = RowControls[Index];
    const bool Over = Track.Encloses(Pointer.PositionX, Pointer.PositionY);

    if (Over && Pointer.ContactPressed)
        Interaction.Grab(Target, ControlPart::Body);

    if (Interaction.Holding(Target) && Travel > 0.0f)
    {
        const float Landed = Clamped((Pointer.PositionX - Track.MinimumX - KnobDiameter * 0.5f * Applied)
                                     / Travel, 0.0f, 1.0f);
        *Declared.Reading = Declared.Minimum + Landed * Span;
        PointerTaken = true;
    }

    Interaction.DeclareHovered(Target, Over, 130.0);
    if (Over)
        PointerTaken = true;
}

//------------------------------------------------------------------------------------------------------------------------
//                                                       SEGMENTED
//------------------------------------------------------------------------------------------------------------------------

void ToolOptionsWidget::RecordSegmented(const PlaneExtent& Row, OptionDeclaration& Declared,
                                        std::uint32_t Index, bool& PointerTaken)
{
    if (Declared.Selected == nullptr || Declared.OptionCount == 0u)
        return;

    const float Applied = Scale();
    const std::uint32_t Count = Declared.OptionCount < OptionLimit ? Declared.OptionCount : OptionLimit;

    Surface->Ground(Row, ValueBlack, SegmentRadius * Applied, CornerAll);

    const float Inset = 3.0f * Applied;
    const float Width = (Row.Width() - Inset * 2.0f) / static_cast<float>(Count);

    for (std::uint32_t Which = 0u; Which < Count; ++Which)
    {
        const PlaneExtent Cell = Spanning(Row.MinimumX + Inset + Width * static_cast<float>(Which),
                                          Row.MinimumY + Inset, Width, Row.Height() - Inset * 2.0f);
        const bool Chosen  = *Declared.Selected == Which;
        const bool Hovered = Cell.Encloses(Pointer.PositionX, Pointer.PositionY);

        if (Chosen)
            Surface->Ground(Cell, KnobGround, (SegmentRadius - 2.0f) * Applied, CornerAll);
        else if (Hovered)
            Surface->Ground(Cell, ValueGround, (SegmentRadius - 2.0f) * Applied, CornerAll);

        const ThemeToken Ink = Chosen ? PanelHead : (Hovered ? ColourPrimary : ColourMuted);

        // 📝 A glyph when the caller supplied one, the caption otherwise. The element modes are three
        //    views of one figure, which reads faster than three words at this size.
        if (Declared.Glyphs != nullptr)
        {
            const float Side = 18.0f * Applied;
            Surface->Stroke(Declared.Glyphs[Which],
                            Spanning(Cell.MinimumX + (Cell.Width() - Side) * 0.5f,
                                     Cell.MinimumY + (Cell.Height() - Side) * 0.5f, Side, Side),
                            Ink);
        }
        else if (Declared.Options != nullptr)
        {
            const float Run = Surface->MeasureRun(Declared.Options[Which], CaptionPoint * Applied);
            Surface->TextRun(Cell.MinimumX + (Cell.Width() - Run) * 0.5f,
                             (Cell.MinimumY + Cell.MaximumY) * 0.5f + CaptionPoint * 0.36f * Applied,
                             Ink, Declared.Options[Which], CaptionPoint * Applied,
                             0.0f, false, Chosen ? FontWeight::Semibold : FontWeight::Regular);
        }

        if (Pressed(SelectedControls[Index * OptionLimit + Which], Cell))
            *Declared.Selected = Which;
        if (Hovered)
            PointerTaken = true;
    }
}

//------------------------------------------------------------------------------------------------------------------------
//                                                        TOGGLE
//------------------------------------------------------------------------------------------------------------------------

void ToolOptionsWidget::RecordToggle(const PlaneExtent& Row, OptionDeclaration& Declared,
                                     std::uint32_t Index, bool& PointerTaken)
{
    if (Declared.Taken == nullptr)
        return;

    const float Applied = Scale();
    const PlaneExtent Switch = Spanning(Row.MaximumX - SwitchWidth * Applied,
                                        Row.MinimumY + (Row.Height() - SwitchHeight * Applied) * 0.5f,
                                        SwitchWidth * Applied, SwitchHeight * Applied);

    Surface->Ground(Switch, *Declared.Taken ? AccentGround : TrackGround,
                    Switch.Height() * 0.5f, CornerAll);

    const float NubRadius = SwitchNub * 0.5f * Applied;
    const float NubX = *Declared.Taken ? Switch.MaximumX - NubRadius - 3.0f * Applied
                                       : Switch.MinimumX + NubRadius + 3.0f * Applied;
    Surface->Medallion(NubX, (Switch.MinimumY + Switch.MaximumY) * 0.5f, NubRadius, KnobGround);

    // 📝 The whole row is the target, not only the switch — a 52 px switch is a small thing to hit and
    //    the caption beside it is part of the same statement.
    if (Pressed(RowControls[Index], Row))
        *Declared.Taken = !*Declared.Taken;
    if (Row.Encloses(Pointer.PositionX, Pointer.PositionY))
        PointerTaken = true;
}

//------------------------------------------------------------------------------------------------------------------------
//                                                       SWATCHES
//------------------------------------------------------------------------------------------------------------------------

void ToolOptionsWidget::RecordSwatches(const PlaneExtent& Row, OptionDeclaration& Declared,
                                       std::uint32_t Index, bool& PointerTaken)
{
    if (Declared.Selected == nullptr || Declared.Colours == nullptr || Declared.OptionCount == 0u)
        return;

    const float Applied = Scale();
    const std::uint32_t Count = Declared.OptionCount < OptionLimit ? Declared.OptionCount : OptionLimit;

    Surface->Ground(Row, ValueBlack, Row.Height() * 0.5f, CornerAll);

    const std::uint32_t Standing = *Declared.Selected < Count ? *Declared.Selected : 0u;
    const ThemeToken Chosen = Declared.Colours[Standing];
    Surface->Medallion(Row.MinimumX + 10.0f * Applied + SwatchSize * 0.5f * Applied,
                       (Row.MinimumY + Row.MaximumY) * 0.5f,
                       SwatchSize * 0.5f * Applied, Chosen);

    char Written[40] = {};
    std::snprintf(Written, sizeof(Written), "rgb(%u, %u, %u)",
                  static_cast<unsigned>(Chosen.Red),
                  static_cast<unsigned>(Chosen.Green),
                  static_cast<unsigned>(Chosen.Blue));
    Surface->TextRun(Row.MinimumX + (10.0f + SwatchSize + 10.0f) * Applied,
                     (Row.MinimumY + Row.MaximumY) * 0.5f + SubPoint * 0.36f * Applied,
                     ColourMuted, Written, SubPoint * Applied);

    // 📝 The swatches run backwards from the trailing edge, so the readout keeps its room whatever the
    //    colour count is.
    for (std::uint32_t Which = 0u; Which < Count; ++Which)
    {
        const std::uint32_t Colour = Count - 1u - Which;
        const PlaneExtent Cell =
            Spanning(Row.MaximumX - (10.0f + (SwatchSize + 6.0f) * static_cast<float>(Which + 1u)) * Applied,
                     (Row.MinimumY + Row.MaximumY) * 0.5f - SwatchSize * 0.5f * Applied,
                     SwatchSize * Applied, SwatchSize * Applied);

        Surface->Ground(Cell, Declared.Colours[Colour], SwatchRadius * Applied, CornerAll);
        if (*Declared.Selected == Colour)
            Surface->Edge(Cell, KnobGround, 2.0f * Applied, SwatchRadius * Applied, CornerAll);

        if (Pressed(SelectedControls[Index * OptionLimit + Colour], Cell))
            *Declared.Selected = Colour;
    }

    if (Row.Encloses(Pointer.PositionX, Pointer.PositionY))
        PointerTaken = true;
}

}   // namespace Slate
