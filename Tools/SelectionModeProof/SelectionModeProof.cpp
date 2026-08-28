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

#include <cstdio>

namespace
{

std::uint32_t Claims = 0u;
std::uint32_t Failures = 0u;

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

    std::printf("[SelectionModeProof] %u claims, %u failures\n", Claims, Failures);
    return Failures == 0u ? 0 : 1;
}
