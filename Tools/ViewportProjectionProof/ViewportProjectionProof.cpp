//============================================================================================================================================
//                                                     VIEWPORTPROJECTIONPROOF.CPP
//============================================================================================================================================
// 🧩 Proves the projection lifted at step 10e, chiefly by ROUND-TRIPPING through it and its inverse.
//
// 🔴 A projection can only be checked properly against its own inverse. Screen coordinates are meaningless
//    on their own — no reader can say whether 412.7 is the right pixel — but "project this point, unproject
//    the result, get the point back" is checkable without knowing anything about the formula, and it fails
//    the moment either direction disagrees with the other. That pairing is what puts a placed point under
//    the cursor rather than a few pixels away from it, and it had never been tested.
//
// 📝 Every orientation is round-tripped in both projections, so a sign error in any one of the seven frames
//    is caught by the arm that uses it rather than by whichever test happened to look.
//
// ⚠️ The bases here are built BY HAND rather than read from a `SketchStructure`. That is deliberate: the
//    projection's behaviour depends on a basis, not on where the basis came from, and linking the whole
//    sketch kernel to obtain one would make this gate fail for reasons that have nothing to do with
//    projection. `ResolveSketchBasis` is the one function here not covered, and it is four lines.

#include "SlateWorkspace/Discipline/ViewportProjection/Api/CadProjection.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/ViewportProjection.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace Slate;

namespace
{

int Failures = 0;
int Checks   = 0;

void Claim(bool Held, const std::string& Stated)
{
    ++Checks;
    if (Held)
        return;
    ++Failures;
    std::printf("  FAIL  %s\n", Stated.c_str());
}

bool Near(double Left, double Right, double Tolerance = 1.0e-6)
{
    return std::fabs(Left - Right) <= Tolerance;
}

PlaneExtent Viewport()
{
    PlaneExtent Extent;
    Extent.MinimumX = 100.0f;
    Extent.MinimumY = 50.0f;
    Extent.MaximumX = 900.0f;
    Extent.MaximumY = 650.0f;
    return Extent;
}

/// The world plane, as an undeclared sketch resolves to.
SpatialBasis WorldPlane()
{
    return { {}, { 1.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0 }, { 0.0, 1.0, 0.0 } };
}

const ViewportOrientation EveryOrientation[] = {
    ViewportOrientation::Top,   ViewportOrientation::Bottom, ViewportOrientation::Front,
    ViewportOrientation::Back,  ViewportOrientation::Left,   ViewportOrientation::Right,
    ViewportOrientation::Isometric
};

//------------------------------------------------------------------------------------------------------------------------
//                                              1. THE PLANE AND ITS COORDINATES
//------------------------------------------------------------------------------------------------------------------------

void ProvePlane()
{
    std::printf("1. A planar coordinate pair and a spatial point name each other\n");

    const SpatialBasis Basis = WorldPlane();

    // 🔴 The plane round-trip, independent of any view. If this fails nothing above it can be trusted.
    for (double Along : { -50.0, -1.0, 0.0, 0.25, 17.5, 1000.0 })
        for (double Across : { -33.0, 0.0, 2.5, 400.0 })
        {
            const SpatialPoint Position = ResolvePlanarPoint(Basis, Along, Across);
            double BackAlong = 0.0, BackAcross = 0.0;
            ResolvePlaneCoordinates(Basis, Position, BackAlong, BackAcross);
            Claim(Near(BackAlong, Along) && Near(BackAcross, Across),
                  "(" + std::to_string(Along) + ", " + std::to_string(Across) + ") did not survive the plane round trip");
        }

    Claim(Near(ResolvePlanarPoint(Basis, 0.0, 0.0).Left, 0.0) &&
          Near(ResolvePlanarPoint(Basis, 0.0, 0.0).Up, 0.0) &&
          Near(ResolvePlanarPoint(Basis, 0.0, 0.0).Forward, 0.0),
          "the plane origin is the spatial origin");

    // ⚠️ A point off the plane measures by its projection; the normal distance is discarded.
    const SpatialPoint Lifted = Added(ResolvePlanarPoint(Basis, 3.0, 4.0), Scaled(Basis.Normal, 99.0));
    double LiftedAlong = 0.0, LiftedAcross = 0.0;
    ResolvePlaneCoordinates(Basis, Lifted, LiftedAlong, LiftedAcross);
    Claim(Near(LiftedAlong, 3.0) && Near(LiftedAcross, 4.0),
          "a point off the plane measures by its projection onto it");

    // 🔴 A skewed plane must be re-squared, not trusted.
    SpatialBasis Skewed = WorldPlane();
    Skewed.Along  = { 2.0, 0.0, 0.0 };
    Skewed.Normal = { 0.0, 5.0, 0.0 };
    const SpatialBasis Square = { Skewed.Origin, Normalize(Skewed.Along),
                                  Normalize(Cross(Normalize(Skewed.Normal), Normalize(Skewed.Along))),
                                  Normalize(Skewed.Normal) };
    Claim(Near(LengthSquared(Square.Along), 1.0) && Near(LengthSquared(Square.Across), 1.0),
          "a re-squared basis has unit directions");
    Claim(Near(Dot(Square.Along, Square.Across), 0.0) && Near(Dot(Square.Along, Square.Normal), 0.0),
          "a re-squared basis is orthogonal");
}

//------------------------------------------------------------------------------------------------------------------------
//                                            2. THE FRAME EACH ORIENTATION IMPLIES
//------------------------------------------------------------------------------------------------------------------------

void ProveFrames()
{
    std::printf("2. Every orientation resolves to a well-formed camera\n");

    const SpatialBasis Basis = WorldPlane();

    for (const ViewportOrientation Orientation : EveryOrientation)
        for (const bool Perspective : { false, true })
        {
            ViewportStanding View;
            ApplyViewportOrientation(View, Orientation, Perspective);

            const ViewFrame Frame = ResolveViewportFrame(Basis, View, Perspective);
            const std::string Where =
                std::string(OrientationText(Orientation)) + (Perspective ? " perspective" : " orthographic");

            // 🔴 A camera whose directions are not unit and mutually square projects a skewed image. This
            //    is where a collapsed frame shows up — the ±90° case that 89° exists to avoid.
            Claim(Near(LengthSquared(Frame.Right), 1.0, 1.0e-9), Where + ": right is not a unit direction");
            Claim(Near(LengthSquared(Frame.Up), 1.0, 1.0e-9), Where + ": up is not a unit direction");
            Claim(Near(LengthSquared(Frame.Forward), 1.0, 1.0e-9), Where + ": forward is not a unit direction");
            Claim(Near(Dot(Frame.Right, Frame.Up), 0.0, 1.0e-9), Where + ": right and up are not square");
            Claim(Near(Dot(Frame.Right, Frame.Forward), 0.0, 1.0e-9), Where + ": right and forward are not square");
            Claim(Near(Dot(Frame.Up, Frame.Forward), 0.0, 1.0e-9), Where + ": up and forward are not square");
        }

    // 📝 Top and bottom look opposite ways along the normal.
    ViewportStanding Above;
    ApplyViewportOrientation(Above, ViewportOrientation::Top, false);
    ViewportStanding Below;
    ApplyViewportOrientation(Below, ViewportOrientation::Bottom, false);
    const ViewFrame Down = ResolveViewportFrame(Basis, Above, false);
    const ViewFrame Up   = ResolveViewportFrame(Basis, Below, false);
    Claim(Near(Dot(Down.Forward, Up.Forward), -1.0, 1.0e-9), "top and bottom look opposite ways");
    Claim(Down.Eye.Up > 0.0 && Up.Eye.Up < 0.0, "each eye sits on its own side of the plane");
}

//------------------------------------------------------------------------------------------------------------------------
//                                           3. THE ORIENTATION CONTROL ITSELF
//------------------------------------------------------------------------------------------------------------------------

void ProveOrientation()
{
    std::printf("3. Naming an orientation sets the orbit, in perspective only\n");

    // 🔴 An orthographic view is decided by its orientation alone, so the orbit must survive untouched —
    //    that is what lets an artist visit Top and come back to the orbit they left.
    ViewportStanding Orthographic;
    Orthographic.OrbitYaw   = 123.0;
    Orthographic.OrbitPitch = 45.0;
    ApplyViewportOrientation(Orthographic, ViewportOrientation::Front, false);
    Claim(Orthographic.Orientation == ViewportOrientation::Front, "the orientation is recorded either way");
    Claim(Near(Orthographic.OrbitYaw, 123.0) && Near(Orthographic.OrbitPitch, 45.0),
          "an orthographic orientation must not disturb the orbit");

    ViewportStanding Perspective;
    ApplyViewportOrientation(Perspective, ViewportOrientation::Front, true);
    Claim(Near(Perspective.OrbitYaw, 0.0) && Near(Perspective.OrbitPitch, 0.0), "front orbits to 0, 0");
    ApplyViewportOrientation(Perspective, ViewportOrientation::Back, true);
    Claim(Near(Perspective.OrbitYaw, 180.0), "back orbits to 180");
    ApplyViewportOrientation(Perspective, ViewportOrientation::Left, true);
    Claim(Near(Perspective.OrbitYaw, -90.0), "left orbits to -90");
    ApplyViewportOrientation(Perspective, ViewportOrientation::Right, true);
    Claim(Near(Perspective.OrbitYaw, 90.0), "right orbits to 90");
    ApplyViewportOrientation(Perspective, ViewportOrientation::Isometric, true);
    Claim(Near(Perspective.OrbitYaw, 45.0) && Near(Perspective.OrbitPitch, 30.0), "isometric orbits to 45, 30");

    // ⚠️ THE 89-DEGREE RULE. At exactly ±90° the forward direction is parallel to the plane normal, so
    //    Cross(Forward, Normal) is nothing at all and the frame collapses. This pins the one degree of
    //    clearance, and section 2 proves the frame it produces is still square.
    ApplyViewportOrientation(Perspective, ViewportOrientation::Top, true);
    Claim(Near(Perspective.OrbitPitch, 89.0), "top orbits to 89, NOT 90 — at 90 the frame collapses");
    ApplyViewportOrientation(Perspective, ViewportOrientation::Bottom, true);
    Claim(Near(Perspective.OrbitPitch, -89.0), "bottom orbits to -89 for the same reason");

    Claim(std::string(OrientationText(ViewportOrientation::Isometric)) == "Perspective",
          "isometric reads as Perspective — the control the artist reached for");
    for (const ViewportOrientation Orientation : EveryOrientation)
        Claim(std::string(OrientationText(Orientation)).length() > 0u, "every orientation has a word");
}

//------------------------------------------------------------------------------------------------------------------------
//                                     4. PROJECT, THEN UNPROJECT, AND GET IT BACK
//------------------------------------------------------------------------------------------------------------------------

void ProveRoundTrip()
{
    std::printf("4. A point survives the trip to the screen and back\n");

    const SpatialBasis Basis  = WorldPlane();
    const PlaneExtent  Extent = Viewport();

    // 🔴 THE CENTRAL CLAIM OF THIS UNIT. Screen coordinates cannot be checked by inspection, but a point
    //    that comes back from the round trip proves the projection and its inverse agree — which is what
    //    puts a placed point under the cursor instead of near it.
    for (const ViewportOrientation Orientation : EveryOrientation)
        for (const bool Perspective : { false, true })
        {
            ViewportStanding View;
            ApplyViewportOrientation(View, Orientation, Perspective);

            const std::string Where =
                std::string(OrientationText(Orientation)) + (Perspective ? " perspective" : " orthographic");

            for (const double Along : { -20.0, 0.0, 7.5 })
                for (const double Across : { -12.0, 0.0, 31.0 })
                {
                    float ScreenX = 0.0f, ScreenY = 0.0f;
                    if (!ProjectViewportPoint(Basis, View, Perspective, Extent, Along, Across, ScreenX, ScreenY))
                        continue;   // 📝 behind a perspective eye; section 5 covers the refusal itself

                    SpatialPoint Back = {};
                    if (!ResolveViewportPlaneIntersection(Basis, View, Perspective, Extent,
                                                          ScreenX, ScreenY, Back))
                    {
                        // 🔴 NOT A DEFECT, and this proof asserted it was until the numbers said otherwise.
                        //    Front, Back, Left and Right look ALONG the sketch plane — their forward
                        //    direction is perpendicular to the plane normal — so every ray is parallel to
                        //    the plane and meets it nowhere. The projection still works (the plane is a
                        //    line on screen), but there is no inverse, and refusing is correct. Only views
                        //    that actually face the plane can round-trip.
                        Claim(std::fabs(Dot(ResolveViewportFrame(Basis, View, Perspective).Forward,
                                            Basis.Normal)) <= 1.0e-6,
                              Where + ": a view facing the plane failed to unproject a point it projected");
                        continue;
                    }

                    double BackAlong = 0.0, BackAcross = 0.0;
                    ResolvePlaneCoordinates(Basis, Back, BackAlong, BackAcross);

                    // 📝 A loose tolerance, because the trip runs through 32-bit screen coordinates. The
                    //    claim is that the point comes back to the same place, not to the same bits.
                    Claim(Near(BackAlong, Along, 0.02) && Near(BackAcross, Across, 0.02),
                          Where + ": (" + std::to_string(Along) + ", " + std::to_string(Across) +
                              ") came back as (" + std::to_string(BackAlong) + ", " +
                              std::to_string(BackAcross) + ")");
                }
        }

    // The centre of the viewport is the focus, in every orientation.
    for (const ViewportOrientation Orientation : EveryOrientation)
    {
        ViewportStanding View;
        ApplyViewportOrientation(View, Orientation, false);
        float ScreenX = 0.0f, ScreenY = 0.0f;
        Claim(ProjectViewportPoint(Basis, View, false, Extent, 0.0, 0.0, ScreenX, ScreenY),
              "the origin always projects in an orthographic view");
        Claim(Near(ScreenX, Extent.MinimumX + Extent.Width() * 0.5, 0.01) &&
              Near(ScreenY, Extent.MinimumY + Extent.Height() * 0.5, 0.01),
              std::string(OrientationText(Orientation)) + ": the focus is not at the centre of the viewport");
    }

    // 📝 Entering from a spatial point gives the same answer as entering from its coordinates.
    ViewportStanding View;
    ApplyViewportOrientation(View, ViewportOrientation::Isometric, true);
    float PlanarX = 0.0f, PlanarY = 0.0f, SpatialX = 0.0f, SpatialY = 0.0f;
    const bool Planar  = ProjectViewportPoint(Basis, View, true, Extent, 9.0, -4.0, PlanarX, PlanarY);
    const bool Spatial = ProjectSpatialPoint(Basis, View, true, Extent,
                                             ResolvePlanarPoint(Basis, 9.0, -4.0), SpatialX, SpatialY);
    Claim(Planar == Spatial && Near(PlanarX, SpatialX, 0.001) && Near(PlanarY, SpatialY, 0.001),
          "projecting a point and projecting its coordinates agree");

    // 🔴 A POINT OFF THE PLANE MUST NOT PROJECT TO ITS SHADOW ON IT. This went through
    //    `ResolvePlaneCoordinates` first, which discards the component along the normal, so a point fifty
    //    units up landed on exactly the pixel its ground shadow did. Everything that draws something
    //    standing OFF the sketch plane — a scene proxy, a solid, a gizmo arm along the normal — was
    //    drawing it flat, and no claim here covered the case because both sides of the comparison above
    //    lie on the plane by construction.
    for (const bool Projected : { false, true })
    {
        ViewportStanding Standing;
        ApplyViewportOrientation(Standing, ViewportOrientation::Isometric, Projected);

        const SpatialPoint OnPlane  = ResolvePlanarPoint(Basis, 9.0, -4.0);
        const SpatialPoint Above    = Added(OnPlane, Scaled(Basis.Normal, 50.0));
        const SpatialPoint Below    = Added(OnPlane, Scaled(Basis.Normal, -30.0));

        float FlatX = 0.0f, FlatY = 0.0f, HighX = 0.0f, HighY = 0.0f, LowX = 0.0f, LowY = 0.0f;
        const bool Flat = ProjectSpatialPoint(Basis, Standing, Projected, Extent, OnPlane, FlatX, FlatY);
        const bool High = ProjectSpatialPoint(Basis, Standing, Projected, Extent, Above,   HighX, HighY);
        const bool Low  = ProjectSpatialPoint(Basis, Standing, Projected, Extent, Below,   LowX,  LowY);

        Claim(Flat && High && Low, "a point above and below the plane both project");
        Claim(!Near(static_cast<double>(HighX), static_cast<double>(FlatX), 0.5) ||
              !Near(static_cast<double>(HighY), static_cast<double>(FlatY), 0.5),
              "a point standing off the plane does NOT land on its shadow");
        Claim(!Near(static_cast<double>(LowX), static_cast<double>(HighX), 0.5) ||
              !Near(static_cast<double>(LowY), static_cast<double>(HighY), 0.5),
              "...and above the plane is not the same place as below it");

        // 📝 On an isometric view the normal runs up the screen, so higher must draw higher — a smaller
        //    screen Y. This pins the SIGN, which a bare inequality would let through inverted.
        Claim(static_cast<double>(HighY) < static_cast<double>(FlatY),
              "higher off the plane draws higher up the display");
        Claim(static_cast<double>(LowY) > static_cast<double>(FlatY),
              "...and below the plane draws lower down");
    }
}

//------------------------------------------------------------------------------------------------------------------------
//                                              5. WHEN IT REFUSES, AND WHY
//------------------------------------------------------------------------------------------------------------------------

void ProveRefusals()
{
    std::printf("5. A point with no screen position is refused, not invented\n");

    const SpatialBasis Basis  = WorldPlane();
    const PlaneExtent  Extent = Viewport();

    // 🔴 A point behind a perspective eye must be refused. Dividing by a negative depth places it mirrored
    //    through the centre of the viewport, drawing geometry that is behind the artist.
    ViewportStanding Close;
    ApplyViewportOrientation(Close, ViewportOrientation::Front, true);
    Close.Distance = 10.0;
    float ScreenX = 0.0f, ScreenY = 0.0f;
    Claim(!ProjectViewportPoint(Basis, Close, true, Extent, 0.0, 100000.0, ScreenX, ScreenY),
          "a point far behind the eye must be refused");

    // An orthographic view has no eye to be behind, so it always projects.
    ViewportStanding Flat;
    ApplyViewportOrientation(Flat, ViewportOrientation::Top, false);
    Claim(ProjectViewportPoint(Basis, Flat, false, Extent, 0.0, 1.0e9, ScreenX, ScreenY),
          "an orthographic view projects any point, however distant");

    // ⚠️ A ray parallel to the plane never meets it. In a Front orthographic view the forward direction
    //    lies IN the sketch plane, so every ray is parallel and every position must refuse.
    ViewportStanding Edge;
    ApplyViewportOrientation(Edge, ViewportOrientation::Front, false);
    SpatialPoint Position = {};
    Claim(!ResolveViewportPlaneIntersection(Basis, Edge, false, Extent, 500.0f, 350.0f, Position),
          "a view looking along the plane cannot name a point on it");

    // Snap tolerance: floored at both ends, and never divides by a zero scale.
    ViewportStanding Zoomed;
    Zoomed.OrthoScale = 1000.0;
    Claim(ResolveSnapTolerance(Zoomed, false) >= 0.25, "an orthographic tolerance never falls below 0.25");
    Zoomed.OrthoScale = 0.0;
    const double Divided = ResolveSnapTolerance(Zoomed, false);
    Claim(Divided > 0.0 && std::isfinite(Divided), "a zero scale does not divide by zero");
    ViewportStanding Near2;
    Near2.Distance = 1.0;
    Claim(ResolveSnapTolerance(Near2, true) >= 2.0, "a perspective tolerance never falls below 2.0");
    ViewportStanding Far;
    Far.Distance = 1000.0;
    Claim(ResolveSnapTolerance(Far, true) > ResolveSnapTolerance(Near2, true),
          "a tolerance grows with the distance, so a snap stays the same size on screen");
    ViewportStanding Tight;
    Tight.OrthoScale = 100.0;
    ViewportStanding Loose;
    Loose.OrthoScale = 1.0;
    Claim(ResolveSnapTolerance(Loose, false) > ResolveSnapTolerance(Tight, false),
          "a zoomed-out orthographic view has a wider tolerance in plane units");
}

//------------------------------------------------------------------------------------------------------------------------
//                                        6. THE TWO DEFECTS THIS PROOF FOUND
//------------------------------------------------------------------------------------------------------------------------

void ProveRegressions()
{
    std::printf("6. The two shipped defects the round trip exposed stay fixed\n");

    const SpatialBasis Basis  = WorldPlane();
    const PlaneExtent  Extent = Viewport();

    // 🔴 DEFECT ONE — the ray-plane distance was negated. `Difference(A, B)` returns the direction FROM A
    //    TO B, so it already points from the eye towards the plane; the shipped `-Dot(...)` inverted every
    //    distance, an eye 240 units in FRONT of the plane resolved to -240, and it was refused as behind
    //    the viewer. Clicking anywhere in a perspective viewport could not place a point.
    //
    //    It survived because the orthographic arm cannot expose it: there the ray origin is built from the
    //    focus, which lies ON the sketch plane, so the numerator is exactly zero and the sign is invisible.
    ViewportStanding Perspective;
    ApplyViewportOrientation(Perspective, ViewportOrientation::Isometric, true);
    SpatialPoint Landed = {};
    Claim(ResolveViewportPlaneIntersection(Basis, Perspective, true, Extent,
                                           Extent.MinimumX + Extent.Width() * 0.5f,
                                           Extent.MinimumY + Extent.Height() * 0.5f, Landed),
          "the centre of a perspective viewport must name a point on the plane");
    double CentreAlong = 0.0, CentreAcross = 0.0;
    ResolvePlaneCoordinates(Basis, Landed, CentreAlong, CentreAcross);
    Claim(Near(CentreAlong, 0.0, 0.02) && Near(CentreAcross, 0.0, 0.02),
          "and that point is the focus");

    // 🔴 DEFECT TWO — an orthographic view was refused on a negative distance, but only a PERSPECTIVE view
    //    has an eye for something to be behind. An orthographic projection is parallel: its ray origin is
    //    a point on the projection plane and the ray runs both ways. Refusing on sign rejected half the
    //    viewport — invisible in the six axis-aligned views, whose origins sit on the sketch plane, and
    //    fatal in the isometric one, where the whole upper-left could not place a point.
    ViewportStanding Isometric;
    ApplyViewportOrientation(Isometric, ViewportOrientation::Isometric, false);
    std::uint32_t Named = 0u;
    for (float X = Extent.MinimumX + 20.0f; X < Extent.MaximumX; X += 80.0f)
        for (float Y = Extent.MinimumY + 20.0f; Y < Extent.MaximumY; Y += 80.0f)
        {
            SpatialPoint Where = {};
            if (ResolveViewportPlaneIntersection(Basis, Isometric, false, Extent, X, Y, Where))
                ++Named;
        }
    Claim(Named == 80u,
          "every position in an isometric orthographic viewport must name a point, " +
              std::to_string(Named) + " of 80 did");
}


//------------------------------------------------------------------------------------------------------------------------
//  §7  The shader agrees with the pointer
//------------------------------------------------------------------------------------------------------------------------
// 🔴 THE VIEWPORT PROJECTS EVERY POINT TWICE, BY TWO DIFFERENT ROUTES, AND THEY MUST LAND IN THE SAME
//    PLACE. `ProjectViewportPoint` runs on the processor and decides what the artist has clicked on;
//    `ResolveCadProjection` builds three rows the shader multiplies by, and decides where the line is
//    DRAWN. If they ever disagree the drawing appears somewhere the pointer is not — and every point is
//    off by the same amount, so the picture still looks plausible.
//
// ⚠️ Until step 10u this could not be written down: `ResolveCadProjection` was a definition inside
//    `ParametricSketchHost.cpp`, so nothing but that host could call it, and nothing at all could compare
//    the two answers. The two formulae have sat five hundred lines apart, maintained by hand, unchecked.
//
// 📝 The shader forms `Projection0 + u*Projection1 + v*Projection2` and divides x and y by w. This
//    reproduces exactly that arithmetic and demands the result match the point projection.

void ProjectThroughShaderRows(const WorkspaceCadProjection& Rows, double Along, double Across,
                              float& ScreenX, float& ScreenY)
{
    const double X = Rows.Projection0[0] + Along * Rows.Projection1[0] + Across * Rows.Projection2[0];
    const double Y = Rows.Projection0[1] + Along * Rows.Projection1[1] + Across * Rows.Projection2[1];
    const double W = Rows.Projection0[3] + Along * Rows.Projection1[3] + Across * Rows.Projection2[3];
    ScreenX = static_cast<float>(X / W);
    ScreenY = static_cast<float>(Y / W);
}

void ProveShaderMatchesPointer()
{
    std::printf("\n  §7  the shader rows and the pointer projection agree\n");

    const PlaneExtent  Extent = Viewport();
    const SpatialBasis Basis = WorldPlane();
    const DrawableScale Unscaled;   // 1:1 display; the scaled case is §7b below

    // ⚠️ THE CAMERA MUST BE PANNED OFF THE PLANE ORIGIN. Both default to (0,0,0), which makes the row
    //    describing the plane origin identically zero -- and a zero row hides every sign error in it.
    //    With the focus at the origin, negating the whole base row changed nothing and this section
    //    scored no failures against a deliberately broken build. The second focus below is what sees it.
    const SpatialPoint EveryFocus[] = { { 0.0, 0.0, 0.0 }, { 37.0, -18.0, 61.0 } };

    for (const ViewportOrientation Orientation : EveryOrientation)
    {
        for (const bool Perspective : { false, true })
        for (const SpatialPoint& Focus : EveryFocus)
        {
            ViewportStanding View;
            ApplyViewportOrientation(View, Orientation, Perspective);
            View.OrthoScale = 1.7;
            View.Focus = Focus;

            const WorkspaceCadProjection Rows =
                ResolveCadProjection(Basis, View, Perspective, Extent, Unscaled,
                                     static_cast<std::uint32_t>(Extent.Width()),
                                     static_cast<std::uint32_t>(Extent.Height()));

            unsigned Agreed = 0u;
            unsigned Compared = 0u;
            for (double Along = -300.0; Along <= 300.0; Along += 150.0)
            {
                for (double Across = -300.0; Across <= 300.0; Across += 150.0)
                {
                    float PointerX = 0.0f, PointerY = 0.0f;
                    if (!ProjectViewportPoint(Basis, View, Perspective, Extent, Along, Across, PointerX, PointerY))
                        continue;   // behind a perspective eye; the shader clips it too

                    float ShaderX = 0.0f, ShaderY = 0.0f;
                    ProjectThroughShaderRows(Rows, Along, Across, ShaderX, ShaderY);

                    ++Compared;
                    // 🚩 A tenth of a pixel. The two routes accumulate rounding differently -- one keeps
                    //    doubles to the end, the other stores floats in the rows -- but a real
                    //    disagreement is a whole pixel or more, never a fraction.
                    if (Near(ShaderX, PointerX, 0.1) && Near(ShaderY, PointerY, 0.1)) ++Agreed;
                }
            }

            Claim(Compared > 0u,
                  std::string("some point must project in ") + OrientationText(Orientation));
            Claim(Agreed == Compared,
                  std::string("shader rows must match the pointer in ") + OrientationText(Orientation) +
                      (Perspective ? " perspective, " : " orthographic, ") +
                      std::to_string(Agreed) + " of " + std::to_string(Compared) + " agreed");
        }
    }
}

// 🔴 §7b  THE SCALED DISPLAY. This is where the shipped placement defect lived (`e66b2c3`): the pointer
//    arrives in LOGICAL points and the shader divides by PHYSICAL pixels. On a 1:1 display the two are the
//    same number and every test passes; at 2x everything drawn is half a viewport out of place.
void ProveScaledDisplayPlacement()
{
    std::printf("\n  §7b the projection holds on a scaled display\n");

    const PlaneExtent Extent = Viewport();
    const SpatialBasis Basis = WorldPlane();

    DrawableScale Retina;
    Retina.Factor = 2.0f;

    ViewportStanding View;
    ApplyViewportOrientation(View, ViewportOrientation::Top, false);
    View.OrthoScale = 1.0;

    const PlaneExtent Physical = Retina.ToPhysical(Extent);
    const WorkspaceCadProjection Rows =
        ResolveCadProjection(Basis, View, false, Extent, Retina,
                             static_cast<std::uint32_t>(Physical.Width()),
                             static_cast<std::uint32_t>(Physical.Height()));

    // The plane origin must land at the physical centre of the viewport, not the logical one.
    float OriginX = 0.0f, OriginY = 0.0f;
    ProjectThroughShaderRows(Rows, 0.0, 0.0, OriginX, OriginY);

    const float PhysicalCentreX = Physical.MinimumX + Physical.Width() * 0.5f;
    const float PhysicalCentreY = Physical.MinimumY + Physical.Height() * 0.5f;
    Claim(Near(OriginX, PhysicalCentreX, 0.001) && Near(OriginY, PhysicalCentreY, 0.001),
          "the plane origin must land at the PHYSICAL viewport centre, at (" +
              std::to_string(OriginX) + ", " + std::to_string(OriginY) + ") not (" +
              std::to_string(PhysicalCentreX) + ", " + std::to_string(PhysicalCentreY) + ")");

    // 🔴 And the scale must double with the display, or geometry is drawn at half size in physical
    //    pixels -- which is the same defect wearing a different hat.
    float UnitX = 0.0f, UnitY = 0.0f;
    ProjectThroughShaderRows(Rows, 100.0, 0.0, UnitX, UnitY);

    DrawableScale Unscaled;
    const WorkspaceCadProjection Plain =
        ResolveCadProjection(Basis, View, false, Extent, Unscaled,
                             static_cast<std::uint32_t>(Extent.Width()),
                             static_cast<std::uint32_t>(Extent.Height()));
    float PlainOriginX = 0.0f, PlainOriginY = 0.0f, PlainUnitX = 0.0f, PlainUnitY = 0.0f;
    ProjectThroughShaderRows(Plain, 0.0, 0.0, PlainOriginX, PlainOriginY);
    ProjectThroughShaderRows(Plain, 100.0, 0.0, PlainUnitX, PlainUnitY);

    const double ScaledSpan = std::fabs(UnitX - OriginX);
    const double PlainSpan = std::fabs(PlainUnitX - PlainOriginX);
    Claim(Near(ScaledSpan, PlainSpan * 2.0, 0.001),
          "100 plane units must span twice as many physical pixels at 2x, " +
              std::to_string(ScaledSpan) + " against " + std::to_string(PlainSpan));
}

}   // namespace

//------------------------------------------------------------------------------------------------------------------------

int main()
{
    std::printf("\n=== ViewportProjection — the plane, the view, and the screen ===\n\n");

    ProvePlane();
    ProveFrames();
    ProveOrientation();
    ProveRoundTrip();
    ProveRefusals();
    ProveRegressions();
    ProveShaderMatchesPointer();
    ProveScaledDisplayPlacement();

    std::printf("\n%d claims, %d failures\n", Checks, Failures);
    std::printf(Failures == 0 ? "PROVEN\n\n" : "REFUTED\n\n");
    return Failures == 0 ? 0 : 1;
}
