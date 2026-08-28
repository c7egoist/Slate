//============================================================================================================================================
//                                                         SKETCHPLACEMENT.CPP
//============================================================================================================================================

#include "SketchToolset/SketchTool/SketchPlacement/Api/SketchPlacement.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Slate
{

//------------------------------------------------------------------------------------------------------------------------
//                                                       DECLARATION
//------------------------------------------------------------------------------------------------------------------------

void SketchPlacement::Declare(SketchSubject Subject, PlacementMethod Method, bool Construction)
{
    // 🔴 A pair no geometry call can honour stands the tool down rather than holding a tool that can never
    //    complete. The retired table shipped exactly that: `DiameterCircle` sat in a branch storing one
    //    anchor while its commit required two, so the tool was held, consumed every press, and produced
    //    nothing for as long as the artist kept clicking.
    if (!AcceptedBy(Subject, Method))
    {
        Abandon();
        return;
    }

    // 🔴 Declaring what is already held keeps the anchors. The caller states the held tool every tick from
    //    whatever the artist last pressed, so restarting here would discard the first anchor of every
    //    two-anchor placement on the tick after it was taken.
    if (Subject == Placing && Method == PlacingMethod)
    {
        ConstructionDeclared = Construction;
        return;
    }

    Abandon();
    Placing              = Subject;
    PlacingMethod        = Method;
    ConstructionDeclared = Construction;
}

void SketchPlacement::Abandon()
{
    Placing              = SketchSubject::None;
    PlacingMethod        = PlacementMethod::Extent;
    ConstructionDeclared = false;
    HoverTaken           = false;
    HoverAt              = {};
    HoverSnap            = {};

    // 📝 `clear` rather than assigning `{}`: the placement is reused every time the artist draws another
    //    shape with the same tool, so keeping the reserved extent means the common case allocates once.
    Taken.clear();
    TakenPlacements.clear();

    // 📝 The side count is a property of the TOOL, not of one placement: an artist who sets an octagon
    //    expects the next octagon to be an octagon too. It is reset only when the tool itself changes.
}

bool SketchPlacement::Resolve(float Notches)
{
    // 🔴 Only a polygon has a resolution, so every other tool leaves the wheel to the camera. Returning
    //    false is what lets the host keep zooming while a line is being drawn.
    if (Placing != SketchSubject::Polygon)
        return false;

    const int Travel = static_cast<int>(Notches > 0.0f ? std::floor(Notches + 0.5f)
                                                       : std::ceil(Notches - 0.5f));
    if (Travel == 0)
        return false;

    const long long Wanted = static_cast<long long>(SideCount) + Travel;
    SideCount = static_cast<std::uint32_t>(std::clamp<long long>(Wanted,
                                                                 PolygonSideMinimum,
                                                                 PolygonSideMaximum));
    return true;
}

//------------------------------------------------------------------------------------------------------------------------
//                                                        THE POINTER
//------------------------------------------------------------------------------------------------------------------------

void SketchPlacement::Hover(const SpatialPoint& Position, const SketchSnapPlacement& Placement)
{
    if (!Standing())
        return;

    HoverAt    = Position;
    HoverSnap  = Placement;
    HoverTaken = true;
}

PlacementArrival SketchPlacement::Anchor(bool Terminating)
{
    // 🔴 An anchor is taken at the hover, so a caller that has not stated one this tick has nothing to
    //    anchor. Reporting `Ignored` leaves the contact unconsumed rather than placing an anchor at the
    //    stale position of the previous tick.
    if (!Standing() || !HoverTaken)
        return PlacementArrival::Ignored;

    const PlacementDeclaration Declared = DeclaredPlacement(Placing, PlacingMethod);

    // 🔴 A `Resolved` closure measures between features, so an unsnapped contact is not an anchor at all.
    //    Refusing it lets a dimension tool ignore empty space without the caller knowing dimensions are
    //    special.
    if (Declared.Closure == PlacementClosure::Resolved && !HoverSnap.Resolved())
        return PlacementArrival::Ignored;

    Taken.push_back(HoverAt);
    TakenPlacements.push_back(HoverSnap);

    const std::uint32_t Count = static_cast<std::uint32_t>(Taken.size());

    switch (Declared.Closure)
    {
        case PlacementClosure::Sufficient:
            // 📝 `>=` not `==`: a count reached exactly still completes, and a count somehow passed
            //    completes rather than running on forever.
            return Count >= Declared.Required ? PlacementArrival::Complete : PlacementArrival::Anchored;

        case PlacementClosure::Terminated:
            // 🔴 A terminated curve completes only when the artist says so AND enough anchors stand. A
            //    double-press on the second anchor of a four-anchor Hermite is not a curve.
            return (Terminating && Count >= Declared.Required) ? PlacementArrival::Complete
                                                               : PlacementArrival::Anchored;

        case PlacementClosure::Resolved:
            // 📝 Every anchor here is snapped — the unsnapped ones were refused above — so the count of
            //    anchors and the count of resolved anchors are the same number.
            return Count >= Declared.Required ? PlacementArrival::Complete : PlacementArrival::Anchored;

        case PlacementClosure::ClosureCount:
            break;
    }

    return PlacementArrival::Anchored;
}

//------------------------------------------------------------------------------------------------------------------------
//                                                          SEALING
//------------------------------------------------------------------------------------------------------------------------

SealedPlacement SketchPlacement::Seal()
{
    SealedPlacement Sealed = {};
    if (!Standing() || Taken.empty())
        return Sealed;

    Sealed.Subject      = Placing;
    Sealed.Method       = PlacingMethod;
    Sealed.Construction = ConstructionDeclared;

    // 🔴 The toggle travels with the placement. Deciding at commit time from a panel the commit
    //    cannot see is how the construction flag used to be lost.
    Sealed.ClosedProfile = ClosedProfileDeclared;

    // 🔴 The wheel-chosen side count travels with the sealed placement. The commit used to declare a
    //    hardcoded six sides, so a polygon was always a hexagon whatever the artist asked for.
    Sealed.Resolution   = SideCount;
    Sealed.Anchors      = std::move(Taken);
    Sealed.Placements   = std::move(TakenPlacements);

    // 🔴 The moved-from vectors are cleared, not left in whatever state the move produced, so the tool
    //    that continues to be held starts its next shape empty. The tool itself stays held: after drawing
    //    a line the artist still has the line tool.
    Taken.clear();
    TakenPlacements.clear();
    HoverTaken = false;

    return Sealed;
}

//------------------------------------------------------------------------------------------------------------------------
//                                                        REPORTING
//------------------------------------------------------------------------------------------------------------------------

std::uint32_t SketchPlacement::Remaining() const
{
    if (!Standing())
        return 0u;

    const PlacementDeclaration Declared = DeclaredPlacement(Placing, PlacingMethod);
    const std::uint32_t        Count    = static_cast<std::uint32_t>(Taken.size());

    return Count >= Declared.Required ? 0u : Declared.Required - Count;
}

//------------------------------------------------------------------------------------------------------------------------
//                                                     THE PREVIEW CURVE
//------------------------------------------------------------------------------------------------------------------------

CurveSpecification ResolvePlacementCurve(SketchSubject Subject,
                                         const std::vector<SpatialPoint>& Anchors,
                                         const SpatialPoint& Hover)
{
    // 📝 The hover is the anchor the artist has not committed to yet, so every subject below sees the
    //    same list the commit will see: the anchors taken, plus where the pointer is now.
    std::vector<SpatialPoint> Points = Anchors;
    Points.push_back(Hover);

    switch (Subject)
    {
        case SketchSubject::Line:
        case SketchSubject::Dimension:
            if (Points.size() >= 2u)
                return CurveSpecification::DeclareLine(Points.front(), Points.back());
            break;

        // 🔴 A polyline previews its LAST leg here; the finished legs are drawn by the plural, which
        //    returns one line per pair. Falling through to `default` left the tool with no preview at
        //    all, so an artist mid-polyline saw nothing follow the pointer.
        case SketchSubject::Polyline:
            if (Points.size() >= 2u)
                return CurveSpecification::DeclareLine(Points[Points.size() - 2u], Points.back());
            break;

        // 🔴 An elliptical arc previews as the ellipse it is being cut from until three anchors state
        //    the sweep, which is far better feedback than the nothing it previewed before.
        case SketchSubject::EllipticalArc:
            if (Points.size() >= 2u)
            {
                const SpatialDirection Span = Difference(Points[0], Points[1]);
                const double Major = std::sqrt(LengthSquared(Span));
                if (Major > 1.0e-6)
                {
                    EllipseCurve Round;
                    Round.Centre         = Points[0];
                    Round.Normal         = { 0.0, 1.0, 0.0 };
                    Round.MajorDirection = Normalize(Span);
                    Round.MajorRadius    = Major;
                    Round.MinorRadius    = Points.size() >= 3u
                        ? std::max(std::sqrt(LengthSquared(Difference(Points[0], Points[2]))), 1.0e-6)
                        : Major * 0.5;
                    return CurveSpecification::DeclareEllipse(Round);
                }
            }
            break;

        case SketchSubject::Arc:
            if (Points.size() >= 3u)
                return CurveSpecification::DeclareThreePointArc(Points[0], Points[1], Points[2]);
            if (Points.size() == 2u)
                return CurveSpecification::DeclareLine(Points[0], Points[1]);
            break;

        // 🔴 A Bezier's anchors ARE its control points, so the curve grows a degree with every click.
        case SketchSubject::Bezier:
            if (Points.size() >= 2u)
                return CurveSpecification::DeclareBezier(Points, { 0.0, 1.0 });
            break;

        // 🔴 THE THREE SUBJECTS THAT PREVIEWED AS NOTHING. Degree is clamped to what the control count
        //    can carry -- a cubic needs four points -- so the curve is drawn from the second click on
        //    rather than staying invisible until the last one.
        case SketchSubject::BasisSpline:
            if (Points.size() >= 2u)
            {
                BasisSplineCurve Spline;
                Spline.ControlPoints = Points;
                Spline.Degree = std::min<std::uint32_t>(3u, static_cast<std::uint32_t>(Points.size() - 1u));
                Spline.Periodic = false;
                return CurveSpecification::DeclareBasisSpline(Spline, { 0.0, 1.0 });
            }
            break;

        case SketchSubject::RationalSpline:
            if (Points.size() >= 2u)
            {
                RationalSplineCurve Spline;
                Spline.ControlPoints = Points;
                Spline.Weights.assign(Points.size(), 1.0);
                Spline.Degree = std::min<std::uint32_t>(3u, static_cast<std::uint32_t>(Points.size() - 1u));
                Spline.Periodic = false;
                return CurveSpecification::DeclareRationalSpline(Spline, { 0.0, 1.0 });
            }
            break;

        // 🔴 A HERMITE IS A CHAIN OF SPANS, and the chain is built by `ResolvePlacementCurves`.
        //    A single `CurveSpecification` cannot express more than one span, which is exactly why
        //    the shipped tool drew the first two points as a curve and left every later click as a
        //    bare point. This arm answers for the FIRST span only; callers that want the whole chain
        //    ask for the plural.
        case SketchSubject::Hermite:
            if (Points.size() >= 2u)
            {
                HermiteCurve Span;
                Span.StartPoint   = Points[0];
                Span.EndPoint     = Points[1];
                Span.StartTangent = Difference(Points[0], Points[1]);
                Span.EndTangent   = Span.StartTangent;
                return CurveSpecification::DeclareHermite(Span, { 0.0, 1.0 });
            }
            break;

        case SketchSubject::Circle:
        case SketchSubject::Polygon:
            if (Points.size() >= 2u)
            {
                const double Radius = std::sqrt(LengthSquared(Difference(Points[0], Points[1])));
                if (Radius > 1.0e-6)
                {
                    CircleCurve Round;
                    Round.Centre         = Points[0];
                    Round.Normal         = { 0.0, 1.0, 0.0 };
                    Round.StartDirection = Normalize(Difference(Points[0], Points[1]));
                    Round.Radius         = Radius;
                    return CurveSpecification::DeclareCircle(Round);
                }
            }
            break;

        // 🔴 THE ELLIPSE HAD NO ARM AND FELL THROUGH TO `default`, so it previewed as NOTHING while
        //    being dragged. What the artist saw was the COMMITTED shape appearing on release -- and
        //    because a committed ellipse is four quarter arcs whose fill needs a convex loop, an
        //    almost-closed outline with an open tip is exactly the failure that reads as "draws
        //    almost completely, but open at the tip". One ellipse curve previews closed by
        //    construction.
        case SketchSubject::Ellipse:
            if (Points.size() >= 2u)
            {
                const SpatialDirection Span = Difference(Points[0], Points[1]);
                const double Major = std::sqrt(LengthSquared(Span));
                if (Major > 1.0e-6)
                {
                    EllipseCurve Round;
                    Round.Centre         = Points[0];
                    Round.Normal         = { 0.0, 1.0, 0.0 };
                    Round.MajorDirection = Normalize(Span);
                    Round.MajorRadius    = Major;
                    // 📝 Matches the commit: a minor axis that has not been stated is half the major.
                    Round.MinorRadius    = Major * 0.5;
                    return CurveSpecification::DeclareEllipse(Round);
                }
            }
            break;

        // 📝 A rectangle and a slot preview as their diagonal until they are committed; drawing the
        //    four sides needs a profile, which only the commit declares.
        case SketchSubject::Rectangle:
        case SketchSubject::Slot:
            if (Points.size() >= 2u)
                return CurveSpecification::DeclareLine(Points.front(), Points.back());
            break;

        default:
            break;
    }

    return {};
}

void ResolvePlacementCurves(SketchSubject Subject,
                            const std::vector<SpatialPoint>& Anchors,
                            const SpatialPoint& Hover,
                            std::vector<CurveSpecification>& Delivered)
{
    Delivered.clear();

    std::vector<SpatialPoint> Points = Anchors;
    Points.push_back(Hover);

    // 🔴 ONLY A HERMITE IS A CHAIN. Every other subject is one curve, so the plural defers to the
    //    singular rather than duplicating twenty-two arms of the same table.
    // 🔴 A POLYLINE IS A CHAIN TOO. Every leg already taken must stay on screen while the next one
    //    follows the pointer -- previewing only the last leg would make the shape appear to be a
    //    single moving line rather than the run of segments it is.
    if (Subject == SketchSubject::Polyline && Points.size() >= 2u)
    {
        for (std::size_t Index = 0u; Index + 1u < Points.size(); ++Index)
            if (LengthSquared(Difference(Points[Index], Points[Index + 1u])) > 0.0)
                Delivered.push_back(CurveSpecification::DeclareLine(Points[Index], Points[Index + 1u]));
        return;
    }

    if (Subject != SketchSubject::Hermite || Points.size() < 3u)
    {
        const CurveSpecification Single = ResolvePlacementCurve(Subject, Anchors, Hover);
        if (Single.Declared())
            Delivered.push_back(Single);
        return;
    }

    // 🔴 CATMULL-ROM TANGENTS. Every anchor is a point the curve passes THROUGH, and the tangent at
    //    an interior point is half the vector between its neighbours -- so the spans meet with equal
    //    tangents and the chain is continuous and smooth. The end points reuse their only chord,
    //    which makes the first and last spans behave like the two-point case.
    //
    // 🔴 This is what makes the tool usable. Read as {start, end, tangent, tangent}, a Hermite spent
    //    FOUR clicks on ONE span and ignored every click after that -- the reported "renders the
    //    first 2 points as a curve, other places are just points". Now the Nth click adds the
    //    (N-1)th span and the whole chain redraws.
    for (std::size_t Index = 0u; Index + 1u < Points.size(); ++Index)
    {
        const SpatialPoint& From = Points[Index];
        const SpatialPoint& To   = Points[Index + 1u];

        const SpatialPoint& Before = Index == 0u ? From : Points[Index - 1u];
        const SpatialPoint& After  = Index + 2u < Points.size() ? Points[Index + 2u] : To;

        HermiteCurve Span;
        Span.StartPoint   = From;
        Span.EndPoint     = To;
        Span.StartTangent = Scaled(Difference(Before, To), 0.5);
        Span.EndTangent   = Scaled(Difference(From, After), 0.5);

        Delivered.push_back(CurveSpecification::DeclareHermite(Span, { 0.0, 1.0 }));
    }
}

} // namespace Slate
