//============================================================================================================================================
//                                                        TOOLCONTEXTMENU.CPP
//============================================================================================================================================
// 📐 Colours and measures follow `References/ToolOptionsWidget.html`, so a context menu and the options
//    widget read as one family rather than two panels that happen to share a viewport.

#include "SlateUI/Interface/ToolContextMenu/Api/ToolContextMenu.h"

namespace Slate
{

namespace
{

// 📐 The reference's own tokens, the subset a menu needs.
constexpr ThemeToken PanelGround   = Covering(0x131315u);          // --panel-bg
constexpr ThemeToken PanelHead     = Covering(0x1b1b1eu);          // --panel-head
constexpr ThemeToken PanelOutline  = Partial (0xFFFFFFu, 0.22);    // --outline
constexpr ThemeToken RowHovered    = Covering(0x232326u);          // --value-bg
constexpr ThemeToken ColourPrimary = Covering(0xe9e9ecu);          // --text-primary
constexpr ThemeToken ColourMuted   = Covering(0x7b7b82u);          // --text-muted

constexpr float MenuPadX     = 12.0f;
constexpr float RowGlyph     = 14.0f;
constexpr float RowGlyphGap  = 10.0f;
constexpr float TitlePoint   = 11.0f;
constexpr float RowPoint     = 13.0f;
constexpr float RowRadius    =  8.0f;

/// 🔴 The two spellings of a box meet here and nowhere else. `PlaneExtent` is the interface's; `ExtentBand`
///    is Foundation's, which the placement arithmetic speaks because its other caller is device-side and
///    may not name an interface type. Converting in one pair of functions keeps that seam from spreading.
constexpr ExtentBand AsBand(const PlaneExtent& Extent)
{
    return ExtentBand{ Extent.MinimumX, Extent.MinimumY, Extent.MaximumX, Extent.MaximumY };
}

constexpr PlaneExtent AsExtent(const ExtentBand& Band)
{
    return PlaneExtent{ Band.MinimumX, Band.MinimumY, Band.MaximumX, Band.MaximumY };
}

}   // namespace

//------------------------------------------------------------------------------------------------------------------------
//                                                       CONSTRUCTION
//------------------------------------------------------------------------------------------------------------------------

Deliver<bool> ToolContextMenu::ConstructToolContextMenu(MotionIntegrator& IncomingMotion,
                                                        RecordingSurface& IncomingSurface,
                                                        const ThemeProfile& IncomingAppearance)
{
    if (Motion != nullptr)
        return Deliver<bool>::Refuse({ RefusalReason::ContentUnsupported,
                                       "a tool context menu construction stands" });

    Motion     = &IncomingMotion;
    Surface    = &IncomingSurface;
    Appearance = &IncomingAppearance;

    if (!Interaction.AttachMotion(IncomingMotion).Resolved)
        return Deliver<bool>::Refuse({ RefusalReason::ExtentExhausted,
                                       "tool context menu interaction was rejected" });

    // 📝 Registered up front and addressed by ordinal, as the options widget does: a row registered on
    //    demand would change identity the moment a tool offered one more command, and the hover fade
    //    would follow the ordinal instead of the row.
    for (std::uint32_t Index = 0u; Index < ItemLimit; ++Index)
    {
        const Deliver<ControlIdentity> Registered = Interaction.Register();
        if (!Registered.Resolved)
            return Deliver<bool>::Refuse(Registered.Error);
        Rows[Index] = Registered.Resolve();
    }

    return Deliver<bool>::Result(true);
}

void ToolContextMenu::Advance(const PointerCondition& Sampled, double Elapsed)
{
    Pointer = Sampled;
    Interaction.Advance(Sampled, Elapsed);
}

void ToolContextMenu::Open(const PlaneExtent& Anchor)
{
    Anchored = Anchor;
    Opened   = true;
}

void ToolContextMenu::Close()
{
    Opened   = false;
    Occupied = {};
}

void ToolContextMenu::Reset()
{
    Interaction.Reset();
    Close();
    Anchored   = {};
    AvoidCount = 0u;
}

void ToolContextMenu::Avoid(const PlaneExtent& Extent)
{
    // 📝 A zero-area box is not a widget. Admitting one would make every placement avoid the origin.
    if (Extent.Width() <= 0.0f || Extent.Height() <= 0.0f)
        return;

    if (AvoidCount < AvoidLimit)
        Avoided[AvoidCount++] = Extent;
}

//------------------------------------------------------------------------------------------------------------------------
//                                                        MEASURING
//------------------------------------------------------------------------------------------------------------------------

float ToolContextMenu::Scale() const
{
    return Appearance != nullptr ? static_cast<float>(Appearance->Measure.DisplayScale) : 1.0f;
}

float ToolContextMenu::MeasureHeight(const MenuDeclaration& Declared) const
{
    const float Applied = Scale();
    const std::uint32_t Rows_ = Declared.ItemCount < ItemLimit ? Declared.ItemCount : ItemLimit;

    float Height = MenuPadY * 2.0f * Applied + RowHeight * static_cast<float>(Rows_) * Applied;

    if (Declared.Title != nullptr && Declared.Title[0] != '\0')
        Height += HeadHeight * Applied;

    // 📝 A rule occupies the gap above its row rather than a row of its own.
    for (std::uint32_t Index = 0u; Index < Rows_; ++Index)
        if (Declared.Items[Index].Divides && Index != 0u)
            Height += 7.0f * Applied;

    return Height;
}

bool ToolContextMenu::Pressed(ControlIdentity Target, const PlaneExtent& Extent)
{
    const bool Hovered = Extent.Encloses(Pointer.PositionX, Pointer.PositionY);
    if (Hovered && Pointer.ContactPressed)
        Interaction.Grab(Target, ControlPart::Body);
    Interaction.DeclareHovered(Target, Hovered, 130.0);
    return Hovered && Interaction.Released(Target);
}

//------------------------------------------------------------------------------------------------------------------------
//                                                        RECORDING
//------------------------------------------------------------------------------------------------------------------------

Deliver<bool> ToolContextMenu::Record(const PlaneExtent& Bounds,
                                      const MenuDeclaration& Declared,
                                      std::uint32_t& Taken,
                                      bool& PointerTaken)
{
    if (Surface == nullptr || Appearance == nullptr)
        return Deliver<bool>::Refuse({ RefusalReason::ContentUnsupported,
                                       "the menu was recorded before it was constructed" });

    // 🔴 The avoid list is spent by this call. It states what was drawn THIS tick, and a list that
    //    survived into the next one would have the menu dodging widgets that have since moved.
    const std::uint32_t Avoiding = AvoidCount;
    AvoidCount = 0u;

    if (!Opened || Declared.ItemCount == 0u)
    {
        Occupied = {};
        return Deliver<bool>::Result(false);
    }

    const float Applied = Scale();
    const float Width   = MenuWidth * Applied;
    const float Height  = MeasureHeight(Declared);

    ExtentBand Avoidance[AvoidLimit] = {};
    for (std::uint32_t Index = 0u; Index < Avoiding; ++Index)
        Avoidance[Index] = AsBand(Avoided[Index]);

    ExtentBand Chosen = {};
    const bool Placed = PlaceMenuClear(AsBand(Bounds), AsBand(Anchored), Width, Height,
                                       Avoidance, Avoiding, AnchorGap * Applied, Chosen);

    // 🔴 NOWHERE FREE MEANS NOTHING DRAWN. Recording it anyway is precisely the defect this unit exists
    //    to prevent, and half-drawing it over a widget would be worse than not opening at all.
    if (!Placed)
    {
        Occupied = {};
        Close();
        return Deliver<bool>::Result(false);
    }

    const PlaneExtent Menu = AsExtent(Chosen);
    Occupied = Menu;

    // 📝 A press outside both the menu and its tile dismisses it. The tile is excluded so the press that
    //    opened the menu does not immediately close it again.
    if (Pointer.ContactPressed &&
        !Menu.Encloses(Pointer.PositionX, Pointer.PositionY) &&
        !Anchored.Encloses(Pointer.PositionX, Pointer.PositionY))
    {
        Close();
        return Deliver<bool>::Result(false);
    }

    Surface->Ground(Menu, PanelGround, MenuRadius * Applied, CornerAll);
    Surface->Edge(Menu, PanelOutline, 1.0f, MenuRadius * Applied, CornerAll);

    float Cursor = Menu.MinimumY + MenuPadY * Applied;

    if (Declared.Title != nullptr && Declared.Title[0] != '\0')
    {
        Surface->TextRun(Menu.MinimumX + MenuPadX * Applied,
                         Cursor + (HeadHeight * 0.5f - TitlePoint * 0.7f) * Applied,
                         ColourMuted, Declared.Title, TitlePoint * Applied, 0.0f, false);
        Cursor += HeadHeight * Applied;
    }

    const std::uint32_t Rows_ = Declared.ItemCount < ItemLimit ? Declared.ItemCount : ItemLimit;
    bool Chose = false;

    Surface->Confine(Menu);

    for (std::uint32_t Index = 0u; Index < Rows_; ++Index)
    {
        const MenuItem& Item = Declared.Items[Index];

        if (Item.Divides && Index != 0u)
        {
            Cursor += 3.0f * Applied;
            Surface->Ground(Spanning(Menu.MinimumX + MenuPadX * Applied, Cursor,
                                     Menu.Width() - MenuPadX * 2.0f * Applied, 1.0f),
                            PanelOutline);
            Cursor += 4.0f * Applied;
        }

        const PlaneExtent Row = Spanning(Menu.MinimumX + 4.0f * Applied, Cursor,
                                         Menu.Width() - 8.0f * Applied, RowHeight * Applied);

        // 🔴 A disabled row is drawn and never taken. Omitting it instead would make the menu's height
        //    change with the selection, and the artist would lose the muscle memory of where Trim sits.
        const bool Hovered = Item.Enabled && Row.Encloses(Pointer.PositionX, Pointer.PositionY);

        if (Hovered)
            Surface->Ground(Row, RowHovered, RowRadius * Applied, CornerAll);

        float TextX = Row.MinimumX + (MenuPadX - 4.0f) * Applied;

        if (Item.Glyph != SymbolSubject::SubjectCount)
        {
            const float MiddleY = (Row.MinimumY + Row.MaximumY) * 0.5f;
            Surface->Stroke(Item.Glyph,
                            Spanning(TextX, MiddleY - RowGlyph * 0.5f * Applied,
                                     RowGlyph * Applied, RowGlyph * Applied),
                            Item.Enabled ? ColourPrimary : ColourMuted, 1.6f);
            TextX += (RowGlyph + RowGlyphGap) * Applied;
        }

        Surface->TextRun(TextX, (Row.MinimumY + Row.MaximumY) * 0.5f - RowPoint * 0.7f * Applied,
                         Item.Enabled ? ColourPrimary : ColourMuted,
                         Item.Caption, RowPoint * Applied, 0.0f, false);

        if (Item.Enabled && Pressed(Rows[Index], Row))
        {
            Taken = Index;
            Chose = true;
        }

        Cursor += RowHeight * Applied;
    }

    Surface->Release();

    // 📝 The contact is claimed whenever it is over the menu, taken or not, so a press on a disabled row
    //    or on the padding does not fall through and start a drag in the scene behind.
    if (Menu.Encloses(Pointer.PositionX, Pointer.PositionY))
        PointerTaken = true;

    // 🔴 Taking a row closes the menu. One dismissal, not two.
    if (Chose)
        Close();

    return Deliver<bool>::Result(Chose);
}

}   // namespace Slate
