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

    std::printf("[SelectionModeProof] %u claims, %u failures\n", Claims, Failures);
    return Failures == 0u ? 0 : 1;
}
