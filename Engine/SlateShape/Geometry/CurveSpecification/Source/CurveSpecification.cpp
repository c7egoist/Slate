//============================================================================================================================================
//                                                      CURVESPECIFICATION.CPP
//============================================================================================================================================

#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"

#include <cmath>

namespace Slate
{

namespace
{
    SpatialDirection RotateAroundAxis(const SpatialDirection& Subject,
                                      const SpatialDirection& Axis,
                                      double Radians)
    {
        const SpatialDirection UnitAxis = Normalize(Axis);
        const double Cosine = std::cos(Radians);
        const double Sine = std::sin(Radians);
        const SpatialDirection Parallel = Scaled(UnitAxis, Dot(UnitAxis, Subject));
        const SpatialDirection Perpendicular = Added(Subject, Scaled(Parallel, -1.0));
        const SpatialDirection Crossed = Cross(UnitAxis, Subject);
        return Added(Added(Scaled(Perpendicular, Cosine), Scaled(Crossed, Sine)), Parallel);
    }
}

CurveSpecification CurveSpecification::DeclareLine(const SpatialPoint& Origin,
                                                   const SpatialPoint& Terminus)
{
    CurveSpecification Declared;
    Declared.HeldSubject = CurveSubject::Line;
    Declared.HeldInterval = { 0.0, 1.0 };
    Declared.Line = { Origin, Terminus };
    return Declared;
}

CurveSpecification CurveSpecification::DeclareCircularArc(const CircularArcCurve& Declared,
                                                          const ParameterInterval& Interval)
{
    CurveSpecification Held;
    Held.HeldSubject = CurveSubject::CircularArc;
    Held.HeldInterval = Interval;
    Held.CircularArc = Declared;
    return Held;
}

CurveSpecification CurveSpecification::DeclareCircle(const CircleCurve& Declared)
{
    CurveSpecification Held;
    Held.HeldSubject = CurveSubject::Circle;
    Held.HeldInterval = { 0.0, 6.283185307179586 };
    Held.Circle = Declared;
    return Held;
}

CurveSpecification CurveSpecification::DeclareThreePointArc(const SpatialPoint& StartPoint,
                                                            const SpatialPoint& ThroughPoint,
                                                            const SpatialPoint& EndPoint)
{
    CurveSpecification Held;
    Held.HeldSubject = CurveSubject::CircularArc;
    Held.HeldInterval = { 0.0, 1.0 };

    const SpatialDirection First = Difference(ThroughPoint, StartPoint);
    const SpatialDirection Second = Difference(EndPoint, StartPoint);
    const SpatialDirection Normal = Normalize(Cross(First, Second));
    const double FirstLengthSquared = LengthSquared(First);
    const double SecondLengthSquared = LengthSquared(Second);
    const SpatialDirection Auxiliary = Cross(Normal, First);
    const double Denominator = 2.0 * LengthSquared(Cross(First, Second));

    if (Denominator > 0.0)
    {
        const SpatialDirection CentreOffset = Added(Scaled(Auxiliary, SecondLengthSquared),
                                                    Scaled(Cross(Second, Normal), FirstLengthSquared));
        const SpatialPoint Centre = Added(StartPoint, Scaled(CentreOffset, 1.0 / Denominator));
        const SpatialDirection StartDirection = Normalize(Difference(Centre, StartPoint));
        const SpatialDirection ThroughDirection = Normalize(Difference(Centre, ThroughPoint));
        const SpatialDirection EndDirection = Normalize(Difference(Centre, EndPoint));
        const double Radius = std::sqrt(LengthSquared(Difference(Centre, StartPoint)));
        double Sweep = std::acos(Dot(StartDirection, EndDirection));
        const double ThroughSweep = std::acos(Dot(StartDirection, ThroughDirection));
        if (ThroughSweep > Sweep)
            Sweep = 6.283185307179586 - Sweep;

        Held.CircularArc = { Centre, Normal, StartDirection, ThroughPoint, true, Radius, Sweep };
    }

    return Held;
}

CurveSpecification CurveSpecification::DeclareEllipticalArc(const EllipticalArcCurve& Declared,
                                                            const ParameterInterval& Interval)
{
    CurveSpecification Held;
    Held.HeldSubject = CurveSubject::EllipticalArc;
    Held.HeldInterval = Interval;
    Held.EllipticalArc = Declared;
    return Held;
}

CurveSpecification CurveSpecification::DeclareEllipse(const EllipseCurve& Declared)
{
    CurveSpecification Held;
    Held.HeldSubject = CurveSubject::Ellipse;
    Held.HeldInterval = { 0.0, 6.283185307179586 };
    Held.Ellipse = Declared;
    return Held;
}

CurveSpecification CurveSpecification::DeclareOval(const EllipseCurve& Declared)
{
    return DeclareEllipse(Declared);
}

CurveSpecification CurveSpecification::DeclareBezier(const std::vector<SpatialPoint>& ControlPoints,
                                                     const ParameterInterval& Interval)
{
    CurveSpecification Held;
    Held.HeldSubject = CurveSubject::Bezier;
    Held.HeldInterval = Interval;
    Held.Bezier.ControlPoints = ControlPoints;
    return Held;
}

CurveSpecification CurveSpecification::DeclareBasisSpline(const BasisSplineCurve& Declared,
                                                          const ParameterInterval& Interval)
{
    CurveSpecification Held;
    Held.HeldSubject = CurveSubject::BasisSpline;
    Held.HeldInterval = Interval;
    Held.BasisSpline = Declared;
    return Held;
}

CurveSpecification CurveSpecification::DeclareRationalSpline(const RationalSplineCurve& Declared,
                                                             const ParameterInterval& Interval)
{
    CurveSpecification Held;
    Held.HeldSubject = CurveSubject::RationalSpline;
    Held.HeldInterval = Interval;
    Held.RationalSpline = Declared;
    return Held;
}

CurveSpecification CurveSpecification::DeclareHermite(const HermiteCurve& Declared,
                                                      const ParameterInterval& Interval)
{
    CurveSpecification Held;
    Held.HeldSubject = CurveSubject::Hermite;
    Held.HeldInterval = Interval;
    Held.Hermite = Declared;
    return Held;
}

bool CurveSpecification::Declared() const
{
    if (!HeldInterval.Declared())
        return false;

    switch (HeldSubject)
    {
        case CurveSubject::Line:
            return Line.Origin.Left != Line.Terminus.Left
                || Line.Origin.Up != Line.Terminus.Up
                || Line.Origin.Forward != Line.Terminus.Forward;

        case CurveSubject::CircularArc:
            return CircularArc.Radius > 0.0
                && CircularArc.SweepRadians != 0.0
                && LengthSquared(CircularArc.Normal) > 0.0
                && LengthSquared(CircularArc.StartDirection) > 0.0;

        case CurveSubject::Circle:
            return Circle.Radius > 0.0
                && LengthSquared(Circle.Normal) > 0.0
                && LengthSquared(Circle.StartDirection) > 0.0;

        case CurveSubject::EllipticalArc:
            return EllipticalArc.MajorRadius > 0.0
                && EllipticalArc.MinorRadius > 0.0
                && EllipticalArc.SweepRadians != 0.0
                && LengthSquared(EllipticalArc.Normal) > 0.0
                && LengthSquared(EllipticalArc.MajorDirection) > 0.0;

        case CurveSubject::Ellipse:
            return Ellipse.MajorRadius > 0.0
                && Ellipse.MinorRadius > 0.0
                && LengthSquared(Ellipse.Normal) > 0.0
                && LengthSquared(Ellipse.MajorDirection) > 0.0;

        case CurveSubject::Bezier:
            return Bezier.ControlPoints.size() >= 2u;

        case CurveSubject::BasisSpline:
            return BasisSpline.ControlPoints.size() >= 2u
                && BasisSpline.Degree >= 1u
                && BasisSpline.Degree < BasisSpline.ControlPoints.size();

        case CurveSubject::RationalSpline:
            return RationalSpline.ControlPoints.size() >= 2u
                && RationalSpline.ControlPoints.size() == RationalSpline.Weights.size()
                && RationalSpline.Degree >= 1u
                && RationalSpline.Degree < RationalSpline.ControlPoints.size();

        case CurveSubject::Hermite:
            return (Hermite.StartPoint.Left != Hermite.EndPoint.Left
                 || Hermite.StartPoint.Up != Hermite.EndPoint.Up
                 || Hermite.StartPoint.Forward != Hermite.EndPoint.Forward)
                && LengthSquared(Hermite.StartTangent) > 0.0
                && LengthSquared(Hermite.EndTangent) > 0.0;

        case CurveSubject::SubjectCount:
            return false;
    }

    return false;
}

} // namespace Slate
