//============================================================================================================================================
//                                                          SELECTIONOPTIONS.H
//============================================================================================================================================
// 🧩 What the Select tool is currently willing to pick, and how forgivingly. The floating tool-options
//    widget edits this; the picker reads it. Nothing here draws, and nothing here knows a widget exists.
//
// 🔴 THE ARTIST ASKED FOR SPECIFIC SELECTION. The picker offered whatever happened to be nearest — a
//    curve, its endpoint or a Bezier handle, whichever won on distance — so reaching for an edge when a
//    vertex sat near it was a matter of luck and zoom. An element mode makes the request explicit: with
//    `Vertex` standing, a curve is not a candidate at all, however close the pointer falls to it.

#pragma once

#include <cstdint>

namespace Slate
{

/// 🧩 Which kind of element a pick is allowed to return.
/// note  🔴 The three are what a sketch actually HAS, named the way an artist names them, and each maps
///        onto a `SketchPickSubject` the picker already resolves:
///          Vertex → an endpoint or a Bezier control handle (Point, Control)
///          Edge   → a curve between them (Curve)
///          Face   → a closed region the curves bound (Record)
///        A fourth mode that admits everything is deliberately ABSENT. "Pick whatever is nearest" is the
///        behaviour the artist reported as the defect, so it is not offered as a setting.
enum class SelectionElement : std::uint32_t
{
    Vertex = 0u,
    Edge   = 1u,
    Face   = 2u,

    ElementCount = 3u
};

/// 🧩 Static text naming one element mode, for the widget's segmented control.
/// cost  ✔️
constexpr const char* SelectionElementText(SelectionElement Declared)
{
    switch (Declared)
    {
        case SelectionElement::Vertex: return "Vertex";
        case SelectionElement::Edge:   return "Edge";
        case SelectionElement::Face:   return "Face";
        case SelectionElement::ElementCount: break;
    }
    return "";
}

/// 🧩 Everything the Select tool is configured with, held in one place.
/// note  ⚠️ `Tolerance` is in PIXELS, not world units. A world tolerance is wrong at both ends of the
///        zoom range -- the same fault the closure tolerance had -- because what the artist means by
///        "near enough to click" is a distance on the screen they are looking at.
/// tag   guarantee, nonallocating, nonthrowing
struct SelectionOptions
{
    static constexpr float ToleranceMinimum = 1.0f;    // [px]
    static constexpr float ToleranceMaximum = 40.0f;   // [px]
    static constexpr float ToleranceDefault = 8.0f;    // [px]

    SelectionElement Element   = SelectionElement::Vertex;
    float            Tolerance = ToleranceDefault;   // [px] - how far a pick may reach

    // 🔴 SNAPPING IS ABSENT FROM THIS STRUCTURE, DELIBERATELY. The reference widget carries a
    //    "Snap to grid" switch and the artist asked, in as many words, not to add it: snapping is what a
    //    DRAWING tool does with a pointer position, and selection does not place anything. A switch here
    //    would imply the picker consults the snap catalogue, which it must not.

    /// 🧩 Whether a pick of this kind is admissible under the standing mode.
    /// cost  ✔️
    constexpr bool Admits(SelectionElement Candidate) const { return Candidate == Element; }

    /// 🧩 The tolerance, held inside its declared range whatever the caller stored.
    /// cost  ✔️
    constexpr float ResolvedTolerance() const
    {
        return Tolerance < ToleranceMinimum ? ToleranceMinimum
             : (Tolerance > ToleranceMaximum ? ToleranceMaximum : Tolerance);
    }
};

/// 🧩 Whether the transform gizmo is offered for the standing selection, and in what manner.
/// note  🔴 One widget, not two. The artist asked for selection and the gizmo to be the SAME floating
///        panel: choosing what to select and choosing what to do with it are one activity, and splitting
///        them across two panels would put the two halves of one decision in two places.
struct GizmoOptions
{
    bool Shown = true;   // [-] - whether the gizmo is drawn for a standing selection
};

}   // namespace Slate
