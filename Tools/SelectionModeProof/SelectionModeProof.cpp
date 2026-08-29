//============================================================================================================================================
//                                                        SELECTIONMODEPROOF.CPP
//============================================================================================================================================
// ⭐ THE SELECT TOOL MUST PICK THE KIND OF ELEMENT THE ARTIST ASKED FOR.
//
// 🔴 It picked whatever won a fixed priority — point, then control, then curve — so reaching for an EDGE
//    with a vertex sitting anywhere near it returned the vertex instead, and whether the artist got what
//    they meant depended on how far the view happened to be zoomed. Measured on a line with a probe three
//    units from an endpoint and one unit from the edge through it, the unrestricted search returns the
//    Point every time.
//
// 🔴 A MODE THAT MERELY PREFERS ITS ELEMENT IS THE SAME DEFECT WEARING A HAT. If `Vertex` fell back to a
//    curve whenever no vertex was in range, the artist would still be guessing which kind they were about
//    to get. Claim ③ is the one that matters: a miss must be a miss.

#include "SlateWorkspace/Discipline/RecordDeclaration/Api/RecordDeclaration.h"
#include "SlateWorkspace/Discipline/SketchPicking/Api/SketchPicking.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/ViewportProjection.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <sstream>
#include <fstream>

namespace
{

std::uint32_t Claims = 0u;
std::uint32_t Failures = 0u;

// 📝 The catalogue claims read the source text: "is this call ordered before that one" is a question
//    about the wiring, and the wiring is what was broken while every unit passed.
std::string ReadWhole(const char* Path)
{
    std::ifstream Stream(Path);
    if (!Stream)
        return std::string();

    std::ostringstream Gathered;
    Gathered << Stream.rdbuf();
    return Gathered.str();
}

void Require(bool Held, const char* Naming)
{
    ++Claims;
    if (Held)
        return;
    ++Failures;
    std::printf("  FAILED  %s\n", Naming);
}

}   // namespace

int main()
{
    using namespace Slate;

    std::printf("[SelectionModeProof]\n");

    SketchStructure Sketch;
    WorkspaceRecordStructure Records;
    WorkspaceNameIndex Naming;

    Sketch.DeclarePlane({ { 0.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 }, { 1.0, 0.0, 0.0 } });
    const SketchCurveName Line = Sketch.DeclareLine({ 0.0, 0.0, 0.0 }, { 200.0, 0.0, 0.0 });
    static_cast<void>(DeclareWorkspaceCurve(Naming, Records, Line));

    // ① THE UNRESTRICTED SEARCH IS BIASED, AND THAT IS WHY THE MODE EXISTS. Three units from an
    //    endpoint and one unit from the edge — nearer the EDGE — it still returns the point.
    {
        const SpatialPoint Probe = { 3.0, 0.0, 1.0 };
        const SketchPick Free = ResolveSketchPick(Sketch, Records, Probe, 20.0);
        Require(Free.Subject == SketchPickSubject::Point,
                "the unrestricted search returns a vertex even when the edge is nearer");
    }

    // ② EACH MODE RETURNS ITS OWN KIND, from one probe that is in range of all three.
    {
        const SpatialPoint Probe = { 3.0, 0.0, 1.0 };

        const SketchPick Vertex =
            ResolveSketchPickForElement(Sketch, Records, Probe, 20.0, SelectionElement::Vertex);
        Require(Vertex.Subject == SketchPickSubject::Point, "Vertex mode returns a vertex");

        const SketchPick Edge =
            ResolveSketchPickForElement(Sketch, Records, Probe, 20.0, SelectionElement::Edge);
        Require(Edge.Subject == SketchPickSubject::Curve,
                "Edge mode returns the EDGE, though a vertex is nearer and would have won");
        Require(Edge.Curve.Assigned(), "and names the curve it picked");

        const SketchPick Face =
            ResolveSketchPickForElement(Sketch, Records, Probe, 20.0, SelectionElement::Face);
        Require(Face.Subject == SketchPickSubject::Record, "Face mode returns the record");
        Require(Face.Record.Assigned(), "and names it");
    }

    // ③ 🔴 A MISS IS A MISS. At the edge's midpoint, a hundred units from either endpoint, Vertex mode
    //    must return NOTHING rather than quietly handing back the edge.
    {
        const SpatialPoint Middle = { 100.0, 0.0, 2.0 };

        const SketchPick Vertex =
            ResolveSketchPickForElement(Sketch, Records, Middle, 20.0, SelectionElement::Vertex);
        Require(!Vertex.Standing(),
                "Vertex mode picks NOTHING where there is no vertex, rather than falling back");

        const SketchPick Edge =
            ResolveSketchPickForElement(Sketch, Records, Middle, 20.0, SelectionElement::Edge);
        Require(Edge.Subject == SketchPickSubject::Curve,
                "while Edge mode picks the edge from the same probe");
    }

    // ④ TOLERANCE STILL BOUNDS THE SEARCH. A mode does not make a distant element reachable.
    {
        const SpatialPoint Distant = { 3.0, 0.0, 500.0 };
        for (const SelectionElement Element : { SelectionElement::Vertex,
                                                SelectionElement::Edge,
                                                SelectionElement::Face })
        {
            const SketchPick Missed =
                ResolveSketchPickForElement(Sketch, Records, Distant, 20.0, Element);
            Require(!Missed.Standing(), "nothing within tolerance picks nothing, in every mode");
        }
    }

    // ⑤ THE OPTIONS ADMIT ONE KIND AND REFUSE THE REST — the declaration the widget writes into.
    {
        SelectionOptions Options;
        Options.Element = SelectionElement::Edge;
        Require(Options.Admits(SelectionElement::Edge), "the standing element is admitted");
        Require(!Options.Admits(SelectionElement::Vertex), "and every other kind is refused");
        Require(!Options.Admits(SelectionElement::Face), "including a face");

        Options.Tolerance = 5000.0f;
        Require(Options.ResolvedTolerance() == SelectionOptions::ToleranceMaximum,
                "a tolerance beyond the range is held at the bound");
        Options.Tolerance = -1.0f;
        Require(Options.ResolvedTolerance() == SelectionOptions::ToleranceMinimum,
                "and a negative one cannot disable picking");
    }

    // ⑥ 🔴 THE TOLERANCE IS PIXELS, AND THE CONVERSION IS THE PROJECTION'S OWN, INVERTED. A world-unit
    //    tolerance is wrong at both ends of the zoom range: the same number that picks one vertex when
    //    zoomed in swallows a dozen when zoomed out.
    {
        ViewportStanding View;
        View.OrthoScale = 4.0;   // [px/unit] - what the projection multiplies by

        // 8 px at 4 px per unit is 2 units. Exactly, with no fitted constant.
        Require(std::fabs(ResolvePickTolerance(View, false, 8.0, 900.0) - 2.0) < 1.0e-9,
                "an orthographic reach is the pixel radius divided by pixels-per-unit");

        // 🔴 THE POINT OF THE WHOLE EXERCISE: zoom in tenfold and the same 8 px reaches a tenth as far
        //    into the world, so it stays 8 px on the screen the artist is looking at.
        View.OrthoScale = 40.0;
        Require(std::fabs(ResolvePickTolerance(View, false, 8.0, 900.0) - 0.2) < 1.0e-9,
                "and shrinks in world units exactly as the view zooms in");

        // The perspective arm inverts `ProjectThroughFrame`'s focal length, so a point ONE tolerance
        // away from the pointer projects to exactly the stated pixel radius. Verified by projecting.
        View.OrthoScale = 1.0;
        View.Distance = 240.0;
        View.FieldOfViewDegrees = 60.0;
        const double Reach = ResolvePickTolerance(View, true, 8.0, 900.0);
        const double TanHalf = std::tan(60.0 * 0.5 * 3.14159265358979323846 / 180.0);
        const double Pixels = Reach * ((900.0 * 0.5) / TanHalf) / 240.0;
        Require(std::fabs(Pixels - 8.0) < 1.0e-9,
                "a perspective reach projects back to the stated pixel radius at the focus");

        // ⚠️ And a nonsense reach cannot disable picking or divide by nothing.
        Require(ResolvePickTolerance(View, false, 0.0, 900.0) > 0.0,
                "a reach of nothing still reaches something");
    }

    // ─────────────────────────────────────────────────────────────────────────────────────────────────
    //  ⑦ THE FIVE MODES, AND WHAT EACH ONE REFUSES.
    // ─────────────────────────────────────────────────────────────────────────────────────────────────
    {
        Require(static_cast<std::uint32_t>(SelectionElement::ElementCount) == 5u,
                "five modes stand: Vertex, Edge, Face, Object, Free");

        // 🔴 EVERY MODE MUST BE NAMED. A mode with an empty caption draws a blank segment the artist
        //    cannot tell from a disabled one.
        for (std::uint32_t Index = 0u; Index < 5u; ++Index)
        {
            const char* const Named = SelectionElementText(static_cast<SelectionElement>(Index));
            Require(Named != nullptr && Named[0] != '\0', "each mode carries a caption");
        }

        // 🔴 EXACTNESS IS THE WHOLE POINT of the four named modes: each admits its own kind and refuses
        //    every other, which is what the artist asked for when reaching for an edge kept giving them
        //    a vertex.
        SelectionOptions Options;
        Options.Element = SelectionElement::Edge;
        Require(Options.Admits(SelectionElement::Edge), "Edge admits an edge");
        Require(!Options.Admits(SelectionElement::Vertex), "and refuses a vertex, however near");
        Require(!Options.Admits(SelectionElement::Face), "and refuses a face");

        Options.Element = SelectionElement::Object;
        Require(Options.Admits(SelectionElement::Object), "Object admits an object");
        Require(!Options.Admits(SelectionElement::Edge), "and refuses the edge it was reached through");

        // 🔴 FREE IS THE ONE THAT ADMITS EVERYTHING, and it is a mode the artist CHOOSES rather than the
        //    behaviour they are stuck with. That distinction is the entire reason it is allowed back.
        Options.Element = SelectionElement::Free;
        Require(Options.Admits(SelectionElement::Vertex)
             && Options.Admits(SelectionElement::Edge)
             && Options.Admits(SelectionElement::Face)
             && Options.Admits(SelectionElement::Object),
                "Free admits every kind, which is what makes it Free");
    }

    // ─────────────────────────────────────────────────────────────────────────────────────────────────
    //  ⑧ THE CATALOGUE DEADLOCK. 🔴 THE OPERATIONS WERE NEVER MISSING — THEY WERE FILTERED OUT.
    //
    //  Every tool in the "Sketch Modify" band declares `MinimumDimension = Edge`, and the panel hides a
    //  tool whose minimum the ACTIVE dimension does not reach. Nothing ever set the active dimension from
    //  the real sketch: it held `Nothing` for the whole session, so Fillet, Chamfer, Trim, Extend and
    //  Offset could not appear at any point, under any selection. The artist reported them as absent.
    //
    //  It was also circular. The operations needed a selection to become visible, and an artist cannot
    //  say what they mean to operate on before the operation exists to be chosen. Nothing in a proof of
    //  the picker alone could see it, because the picker was never the broken part.
    // ─────────────────────────────────────────────────────────────────────────────────────────────────
    {
        const std::string Host = ReadWhole("Engine/Application/EditorHost/Source/EditorHost.cpp");
        Require(!Host.empty(), "the host source is readable");

        Require(Host.find("ParametricToolsApplied.ActiveDimension =") != std::string::npos,
                "the host states the active dimension, or every Edge tool stays hidden forever");

        const std::size_t StatedAt = Host.find("ParametricToolsApplied.ActiveDimension =");
        const std::size_t RecordAt = Host.find("ParametricTools.Record(");
        Require(StatedAt != std::string::npos && RecordAt != std::string::npos && StatedAt < RecordAt,
                "and states it BEFORE the panel records, or the panel filters on last frame's answer");

        Require(Host.find("ParametricToolsApplied.WorkplaneActivation = Sketch.Declared()") != std::string::npos,
                "the workplane standing is the sketch's, not a preset button's");

        // 🔴 THE SOURCE OF THE DIMENSION IS THE STANDING PICK. Read from the widget's mode instead and
        //    the catalogue would offer edge operations because the artist was LOOKING at Edge mode,
        //    rather than because an edge is actually held.
        Require(Host.find("SketchPick& Held = SketchSemanticSelection") != std::string::npos,
                "and the dimension is read from what is selected, not from what could be");

        // 🔴 CUT HAD NO TILE. The subject, the geometry and the apply arm all existed; no band listed it,
        //    so none of it was reachable.
        const std::string Panel =
            ReadWhole("Engine/SlateUI/Interface/ParametricTools/Source/ParametricToolsPanel.cpp");
        Require(!Panel.empty(), "the catalogue source is readable");
        Require(Panel.find("{ \"Cut\"") != std::string::npos,
                "Cut has a tile, not just an enumeration member");
        Require(Panel.find("ParametricToolSubject::Cut") != std::string::npos,
                "and the band maps its index onto the Cut subject");

        // 🔴 THE DIMENSION GATES, IT DOES NOT HIDE. `Presented` refuses a HIDDEN tool outright and
        //    `ShowGated` cannot bypass that, so a dimension filter answering `Hidden` removed all five
        //    Sketch Modify tools whenever nothing was selected -- and `VisibleCount` then fell to zero,
        //    which made the rail skip the entire band. The artist could not see the operations OR the
        //    band that holds them, and reaching the dimension that would reveal them required selecting
        //    an edge, which is the thing they had gone to the catalogue to find an operation for.
        const std::size_t GateAt = Panel.find("bool Gated(");
        const std::size_t HideAt = Panel.find("bool Hidden(");
        Require(GateAt != std::string::npos && HideAt != std::string::npos,
                "the catalogue states both filters");

        const std::string GateBody = Panel.substr(GateAt, HideAt > GateAt ? HideAt - GateAt : 0u);
        const std::size_t HideEnds = Panel.find("\n}", HideAt);
        const std::string HideBody = Panel.substr(HideAt, HideEnds > HideAt ? HideEnds - HideAt : 0u);

        Require(GateBody.find("DimensionAccepted") != std::string::npos,
                "the dimension answers Gated, so an inapplicable tool is dimmed and still visible");
        Require(HideBody.find("DimensionAccepted") == std::string::npos,
                "and does NOT answer Hidden, which would remove it and take its band with it");

        // 📝 `Raising` against a solid stays hidden on purpose: that is "never", not "not yet", so a
        //    dimmed tile would sit there forever with no action that could light it.
        Require(HideBody.find("Tool.Raising") != std::string::npos,
                "a raising tool on a solid stays hidden, because nothing could ever make it applicable");
    }

    std::printf("[SelectionModeProof] %u claims, %u failures\n", Claims, Failures);
    return Failures == 0u ? 0 : 1;
}
