//============================================================================================================================================
//                                                      SKETCHSTRUCTURE.CPP
//============================================================================================================================================

#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"
#include "SlateShape/Sketch/SketchStructure/Api/SketchStructure.h"

#include <cmath>

namespace Slate
{

namespace
{
    CurveName CurveReferenceOf(SketchCurveName Name)
    {
        return { Name.IssuedIndex };
    }
}

bool SketchPlane::Declared() const
{
    return LengthSquared(Normal) > 0.0 && LengthSquared(AlongDirection) > 0.0;
}

SketchCurveName SketchStructure::DeclareCurve(const CurveSpecification& Incoming)
{
    HeldCurves.push_back({ Incoming });
    return { static_cast<std::uint32_t>(HeldCurves.size()) };
}

ProfileNameInFeature SketchStructure::DeclareProfile(const ProfileSpecification& Incoming)
{
    HeldProfiles.push_back(Incoming);
    return { static_cast<std::uint32_t>(HeldProfiles.size()) };
}

ConstraintName SketchStructure::DeclareConstraint(const ConstraintSpecification& Incoming)
{
    HeldConstraints.push_back(Incoming);
    return { static_cast<std::uint32_t>(HeldConstraints.size()) };
}

DimensionName SketchStructure::DeclareDimension(const DimensionSpecification& Incoming)
{
    HeldDimensions.push_back(Incoming);
    return { static_cast<std::uint32_t>(HeldDimensions.size()) };
}

SketchCurveName SketchStructure::DeclareLine(const SpatialPoint& Origin, const SpatialPoint& Terminus)
{
    return DeclareCurve(CurveSpecification::DeclareLine(Origin, Terminus));
}

SketchCurveName SketchStructure::DeclareThreePointArc(const SpatialPoint& StartPoint,
                                                      const SpatialPoint& ThroughPoint,
                                                      const SpatialPoint& EndPoint)
{
    return DeclareCurve(CurveSpecification::DeclareThreePointArc(StartPoint, ThroughPoint, EndPoint));
}

SketchCurveName SketchStructure::DeclareCircle(const CircleCurve& Declared)
{
    return DeclareCurve(CurveSpecification::DeclareCircle(Declared));
}

SketchCurveName SketchStructure::DeclareEllipse(const EllipseCurve& Declared)
{
    return DeclareCurve(CurveSpecification::DeclareEllipse(Declared));
}

SketchCurveName SketchStructure::DeclareOval(const EllipseCurve& Declared)
{
    return DeclareCurve(CurveSpecification::DeclareOval(Declared));
}

SketchCurveName SketchStructure::DeclareBezier(const std::vector<SpatialPoint>& ControlPoints)
{
    return DeclareCurve(CurveSpecification::DeclareBezier(ControlPoints, { 0.0, 1.0 }));
}

SketchCurveName SketchStructure::DeclareBasisSpline(const BasisSplineCurve& Declared)
{
    return DeclareCurve(CurveSpecification::DeclareBasisSpline(Declared, { 0.0, 1.0 }));
}

SketchCurveName SketchStructure::DeclareRationalSpline(const RationalSplineCurve& Declared)
{
    return DeclareCurve(CurveSpecification::DeclareRationalSpline(Declared, { 0.0, 1.0 }));
}

SketchCurveName SketchStructure::DeclareHermite(const HermiteCurve& Declared)
{
    return DeclareCurve(CurveSpecification::DeclareHermite(Declared, { 0.0, 1.0 }));
}

Deliver<bool> SketchStructure::DeclarePolyline(const std::vector<SpatialPoint>& Positions,
                                               std::vector<SketchCurveName>& DeclaredCurves)
{
    DeclaredCurves.clear();
    if (Positions.size() < 2u)
        return Deliver<bool>::Refuse({ RefusalReason::ContentUnsupported, "a polyline requires at least two positions" });

    DeclaredCurves.reserve(Positions.size() - 1u);
    for (std::size_t PositionIndex = 0u; PositionIndex + 1u < Positions.size(); ++PositionIndex)
    {
        // 🔴 A ZERO-LENGTH SEGMENT IS NOT A LINE, AND ONE OF THEM BLANKED THE WHOLE SKETCH. Closing a
        //    polyline anchors the start point a second time, so the final pair was coincident and
        //    `DeclareLine` produced an UNDECLARED curve. `SketchStructure::Declared()` is all-or-
        //    nothing across every curve, and `ProjectSketchRendering` refuses outright on an
        //    undeclared sketch -- so closing a shape and pressing Enter made every shape already
        //    drawn disappear at once. Coincident neighbours are skipped rather than declared.
        const SpatialDirection Span = Difference(Positions[PositionIndex],
                                                 Positions[PositionIndex + 1u]);
        if (LengthSquared(Span) <= 0.0)
            continue;

        DeclaredCurves.push_back(DeclareLine(Positions[PositionIndex], Positions[PositionIndex + 1u]));
    }

    // ⚠️ Every pair coincident means the artist clicked one spot repeatedly; there is no polyline.
    if (DeclaredCurves.empty())
        return Deliver<bool>::Refuse({ RefusalReason::ContentUnsupported,
                                       "a polyline requires two distinct positions" });

    return Deliver<bool>::Result(true);
}

Deliver<ProfileNameInFeature> SketchStructure::DeclareCircleProfile(const CircleCurve& Declared)
{
    if (!PlaneStanding || !Plane.Declared())
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the sketch plane is not declared" });
    if (Declared.Radius <= 0.0)
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the circle requires a positive radius" });

    ProfileSpecification Profile;
    Profile.DeclarePlane({ Plane.Origin, Plane.Normal, Plane.AlongDirection });
    ProfileLoop Loop;
    Loop.Orientation = ProfileLoopOrientation::Outer;

    const SpatialDirection StartDirection = Normalize(Declared.StartDirection);
    const SpatialDirection QuarterDirection = Normalize(Cross(Declared.Normal, StartDirection));
    for (std::uint32_t QuarterIndex = 0u; QuarterIndex < 4u; ++QuarterIndex)
    {
        const double StartRadians = 1.5707963267948966 * static_cast<double>(QuarterIndex);
        const SpatialDirection QuarterStart = Added(Scaled(StartDirection, std::cos(StartRadians)),
                                                    Scaled(QuarterDirection, std::sin(StartRadians)));
        // ⚠️ EVERY FIELD, NAMED BY POSITION. `CircularArcCurve` carries a `ThroughPoint` and a
        //    `ThroughDeclared` between the start direction and the radius. A five-element brace list
        //    silently slid the radius into `ThroughPoint` and the sweep into `ThroughDeclared`, leaving
        //    the real radius at zero — so every circle profile in the application collapsed to its own
        //    centre. It still declared, still selected, still appeared in the outliner, and drew nothing.
        CircularArcCurve Quarter = {};
        Quarter.Centre         = Declared.Centre;
        Quarter.Normal         = Declared.Normal;
        Quarter.StartDirection = QuarterStart;
        Quarter.Radius         = Declared.Radius;
        Quarter.SweepRadians   = 1.5707963267948966;

        const SketchCurveName DeclaredCurve = DeclareCurve(
            CurveSpecification::DeclareCircularArc(Quarter, { 0.0, 1.0 }));
        Loop.Traversal.push_back({ CurveReferenceOf(DeclaredCurve), true });
    }

    Profile.DeclareLoop(Loop);
    return Deliver<ProfileNameInFeature>::Result(DeclareProfile(Profile));
}

Deliver<ProfileNameInFeature> SketchStructure::DeclareEllipseProfile(const EllipseCurve& Declared)
{
    if (!PlaneStanding || !Plane.Declared())
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the sketch plane is not declared" });
    if (Declared.MajorRadius <= 0.0 || Declared.MinorRadius <= 0.0)
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the ellipse requires positive axes" });

    ProfileSpecification Profile;
    Profile.DeclarePlane({ Plane.Origin, Plane.Normal, Plane.AlongDirection });
    ProfileLoop Loop;
    Loop.Orientation = ProfileLoopOrientation::Outer;

    for (std::uint32_t QuarterIndex = 0u; QuarterIndex < 4u; ++QuarterIndex)
    {
        const SketchCurveName DeclaredCurve = DeclareCurve(CurveSpecification::DeclareEllipticalArc(
            { Declared.Centre, Declared.Normal, Declared.MajorDirection,
              Declared.MajorRadius, Declared.MinorRadius,
              1.5707963267948966 * static_cast<double>(QuarterIndex),
              1.5707963267948966 },
            { 0.0, 1.0 }));
        Loop.Traversal.push_back({ CurveReferenceOf(DeclaredCurve), true });
    }

    Profile.DeclareLoop(Loop);
    return Deliver<ProfileNameInFeature>::Result(DeclareProfile(Profile));
}

Deliver<ProfileNameInFeature> SketchStructure::DeclareOvalProfile(const EllipseCurve& Declared)
{
    return DeclareEllipseProfile(Declared);
}

Deliver<ProfileNameInFeature> SketchStructure::DeclareRegularPolygon(const SpatialPoint& Centre,
                                                                     double Radius,
                                                                     std::uint32_t SideCount,
                                                                     const SpatialDirection& StartDirection)
{
    if (!PlaneStanding || !Plane.Declared())
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the sketch plane is not declared" });
    if (Radius <= 0.0 || SideCount < 3u)
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the polygon requires positive radius and three sides" });

    // 🔴 THE FIRST CORNER GOES WHERE THE ARTIST DRAGGED. It was pinned to the plane's own
    //    `AlongDirection`, so the committed polygon was rotated away from the one just previewed --
    //    the shape visibly turned on release. An unstated direction still falls back to the plane's
    //    axis, so a polygon declared from a script behaves as it always did.
    const SpatialDirection AlongDirection = LengthSquared(StartDirection) > 1.0e-12
                                          ? Normalize(StartDirection)
                                          : Normalize(Plane.AlongDirection);
    const SpatialDirection AcrossDirection = Normalize(Cross(Plane.Normal, AlongDirection));

    ProfileSpecification Profile;
    Profile.DeclarePlane({ Plane.Origin, Plane.Normal, Plane.AlongDirection });
    ProfileLoop Loop;
    Loop.Orientation = ProfileLoopOrientation::Outer;

    const double StepRadians = 6.283185307179586 / static_cast<double>(SideCount);
    std::vector<SpatialPoint> Corners;
    Corners.reserve(SideCount);
    for (std::uint32_t CornerIndex = 0u; CornerIndex < SideCount; ++CornerIndex)
    {
        const double AngleRadians = StepRadians * static_cast<double>(CornerIndex);
        const SpatialDirection Offset = {
            AlongDirection.Left * Radius * std::cos(AngleRadians) + AcrossDirection.Left * Radius * std::sin(AngleRadians),
            AlongDirection.Up * Radius * std::cos(AngleRadians) + AcrossDirection.Up * Radius * std::sin(AngleRadians),
            AlongDirection.Forward * Radius * std::cos(AngleRadians) + AcrossDirection.Forward * Radius * std::sin(AngleRadians)
        };
        Corners.push_back(Added(Centre, Offset));
    }

    for (std::uint32_t EdgeIndex = 0u; EdgeIndex < SideCount; ++EdgeIndex)
    {
        const std::uint32_t NextIndex = (EdgeIndex + 1u) % SideCount;
        const SketchCurveName DeclaredCurve = DeclareLine(Corners[EdgeIndex], Corners[NextIndex]);
        Loop.Traversal.push_back({ CurveReferenceOf(DeclaredCurve), true });
    }

    Profile.DeclareLoop(Loop);
    return Deliver<ProfileNameInFeature>::Result(DeclareProfile(Profile));
}

Deliver<ProfileNameInFeature> SketchStructure::DeclareSlot(const SpatialPoint& StartPoint,
                                                           const SpatialPoint& EndPoint,
                                                           double Radius)
{
    if (!PlaneStanding || !Plane.Declared())
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the sketch plane is not declared" });
    if (Radius <= 0.0)
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the slot requires a positive radius" });

    const SpatialDirection AxisOffset = { EndPoint.Left - StartPoint.Left,
                                          EndPoint.Up - StartPoint.Up,
                                          EndPoint.Forward - StartPoint.Forward };
    if (LengthSquared(AxisOffset) == 0.0)
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the slot endpoints must differ" });

    const SpatialDirection AxisDirection = Normalize(AxisOffset);
    const SpatialDirection SideDirection = Normalize(Cross(Plane.Normal, AxisDirection));
    const SpatialPoint StartUpper = Added(StartPoint, Scaled(SideDirection, Radius));
    const SpatialPoint EndUpper = Added(EndPoint, Scaled(SideDirection, Radius));
    const SpatialPoint StartLower = Added(StartPoint, Scaled(SideDirection, -Radius));
    const SpatialPoint EndLower = Added(EndPoint, Scaled(SideDirection, -Radius));

    const SketchCurveName Upper = DeclareLine(StartUpper, EndUpper);
    const SketchCurveName Lower = DeclareLine(EndLower, StartLower);

    // 🔴 THE CAPS BULGED INWARDS, WHICH IS WHY A SLOT DREW AS TWO CRESCENTS BITING INTO ITS OWN BODY.
    //    Each cap starts on one side of the axis and must reach the other side by going AROUND THE
    //    FAR END -- away from the slot. A positive sweep turns the start direction towards the
    //    other cap instead, so both semicircles were carved out of the rectangle rather than added
    //    to its ends: the shape closed, so nothing downstream complained, and it simply looked
    //    wrong. Reversing the sweep sends each cap the long way round, over the end it belongs to.
    //
    // 📝 `StepsForSweep` takes the magnitude, so a negative sweep tessellates exactly as densely.
    // ⚠️ The five-versus-seven field slide this once had is why every field is named rather than
    //    written as a brace list; a positional initialiser here silently gave both caps a zero
    //    radius.
    constexpr double HalfTurn = 3.141592653589793;

    CircularArcCurve StartCap = {};
    StartCap.Centre         = StartPoint;
    StartCap.Normal         = Plane.Normal;
    StartCap.StartDirection = Negated(SideDirection);
    StartCap.Radius         = Radius;
    StartCap.SweepRadians   = -HalfTurn;

    CircularArcCurve EndCap = {};
    EndCap.Centre         = EndPoint;
    EndCap.Normal         = Plane.Normal;
    EndCap.StartDirection = SideDirection;
    EndCap.Radius         = Radius;
    EndCap.SweepRadians   = -HalfTurn;

    const SketchCurveName StartArc = DeclareCurve(CurveSpecification::DeclareCircularArc(StartCap, { 0.0, 1.0 }));
    const SketchCurveName EndArc   = DeclareCurve(CurveSpecification::DeclareCircularArc(EndCap, { 0.0, 1.0 }));

    ProfileSpecification Profile;
    Profile.DeclarePlane({ Plane.Origin, Plane.Normal, Plane.AlongDirection });
    ProfileLoop Loop;
    Loop.Orientation = ProfileLoopOrientation::Outer;
    Loop.Traversal = {
        { CurveReferenceOf(Upper), true },
        { CurveReferenceOf(EndArc), true },
        { CurveReferenceOf(Lower), true },
        { CurveReferenceOf(StartArc), true }
    };
    Profile.DeclareLoop(Loop);

    return Deliver<ProfileNameInFeature>::Result(DeclareProfile(Profile));
}

bool SketchStructure::Declared() const
{
    if (!PlaneStanding || !Plane.Declared())
        return false;

    for (const DeclaredSketchCurve& Curve : HeldCurves)
        if (!Curve.Geometry.Declared())
            return false;

    for (const ProfileSpecification& Profile : HeldProfiles)
        if (!Profile.Declared())
            return false;

    for (const ConstraintSpecification& Constraint : HeldConstraints)
        if (!Constraint.Declared())
            return false;

    for (const DimensionSpecification& Dimension : HeldDimensions)
        if (!Dimension.Declared())
            return false;

    return true;
}

void SketchStructure::Reclaim()
{
    Plane = {};
    PlaneStanding = false;
    HeldCurves.clear();
    HeldProfiles.clear();
    HeldConstraints.clear();
    HeldDimensions.clear();
}

} // namespace Slate
