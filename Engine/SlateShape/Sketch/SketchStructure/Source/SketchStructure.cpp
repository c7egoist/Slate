//============================================================================================================================================
//                                                      SKETCHSTRUCTURE.CPP
//============================================================================================================================================

#include "SlateShape/Sketch/SketchStructure/Api/SketchStructure.h"

#include <cmath>

namespace Slate
{

namespace
{
    double LengthSquared(const SpatialDirection& Direction)
    {
        return Direction.Left * Direction.Left
             + Direction.Up * Direction.Up
             + Direction.Forward * Direction.Forward;
    }

    SpatialDirection Normalize(const SpatialDirection& Direction)
    {
        const double Length = std::sqrt(LengthSquared(Direction));
        return { Direction.Left / Length, Direction.Up / Length, Direction.Forward / Length };
    }

    SpatialDirection Negated(const SpatialDirection& Direction)
    {
        return { -Direction.Left, -Direction.Up, -Direction.Forward };
    }

    SpatialDirection Cross(const SpatialDirection& LeftDirection,
                           const SpatialDirection& RightDirection)
    {
        return {
            LeftDirection.Up * RightDirection.Forward - LeftDirection.Forward * RightDirection.Up,
            LeftDirection.Forward * RightDirection.Left - LeftDirection.Left * RightDirection.Forward,
            LeftDirection.Left * RightDirection.Up - LeftDirection.Up * RightDirection.Left
        };
    }

    SpatialDirection Scaled(const SpatialDirection& Direction,
                            double Amount)
    {
        return { Direction.Left * Amount, Direction.Up * Amount, Direction.Forward * Amount };
    }

    SpatialDirection Added(const SpatialDirection& LeftDirection,
                           const SpatialDirection& RightDirection)
    {
        return { LeftDirection.Left + RightDirection.Left,
                 LeftDirection.Up + RightDirection.Up,
                 LeftDirection.Forward + RightDirection.Forward };
    }

    SpatialPoint Added(const SpatialPoint& Position,
                       const SpatialDirection& Offset)
    {
        return { Position.Left + Offset.Left,
                 Position.Up + Offset.Up,
                 Position.Forward + Offset.Forward };
    }

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
        DeclaredCurves.push_back(DeclareLine(Positions[PositionIndex], Positions[PositionIndex + 1u]));

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
        const SketchCurveName DeclaredCurve = DeclareCurve(CurveSpecification::DeclareCircularArc(
            { Declared.Centre, Declared.Normal, QuarterStart, Declared.Radius, 1.5707963267948966 },
            { 0.0, 1.0 }));
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
                                                                     std::uint32_t SideCount)
{
    if (!PlaneStanding || !Plane.Declared())
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the sketch plane is not declared" });
    if (Radius <= 0.0 || SideCount < 3u)
        return Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "the polygon requires positive radius and three sides" });

    const SpatialDirection AlongDirection = Normalize(Plane.AlongDirection);
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
    const SketchCurveName StartArc = DeclareCurve(CurveSpecification::DeclareCircularArc(
        { StartPoint, Plane.Normal, Negated(SideDirection), Radius, 3.141592653589793 },
        { 0.0, 1.0 }));
    const SketchCurveName EndArc = DeclareCurve(CurveSpecification::DeclareCircularArc(
        { EndPoint, Plane.Normal, SideDirection, Radius, 3.141592653589793 },
        { 0.0, 1.0 }));

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
