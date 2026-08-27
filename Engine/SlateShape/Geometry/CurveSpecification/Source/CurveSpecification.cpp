//============================================================================================================================================
//                                                      CURVESPECIFICATION.CPP
//============================================================================================================================================

#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"

#include <cmath>

namespace Slate
{

namespace
{
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

    // ⚠️ `Difference(A, B)` runs FROM A TO B, so the start point comes SECOND to get a direction
    //    pointing away from it. Reading it the other way negates both chords, which flips the computed
    //    centre through the start point.
    const SpatialDirection First  = Difference(StartPoint, ThroughPoint);
    const SpatialDirection Second = Difference(StartPoint, EndPoint);

    const SpatialDirection Perpendicular = Cross(First, Second);
    const double Denominator = 2.0 * LengthSquared(Perpendicular);

    if (Denominator > 0.0)
    {
        // 🔴 THE NORMAL IN THE NUMERATOR MUST BE THE SAME ONE THE DENOMINATOR IS BUILT FROM.
        //    The circumcentre is (|Second|^2 (First x P) + |First|^2 (P x Second)) / 2|P|^2 with the
        //    UNNORMALISED P = First x Second. Using a unit normal in the numerator while dividing by
        //    |P|^2 leaves a stray factor of |P| — here 1200 — and collapses the arc to a radius of 0.02
        //    about a centre 0.02 from the start point. It still declared, still drew, and was invisible.
        const double FirstLengthSquared  = LengthSquared(First);
        const double SecondLengthSquared = LengthSquared(Second);

        const SpatialDirection CentreOffset = Added(Scaled(Cross(Second, Perpendicular), FirstLengthSquared),
                                                    Scaled(Cross(Perpendicular, First), SecondLengthSquared));
        const SpatialPoint Centre = Added(StartPoint, Scaled(CentreOffset, 1.0 / Denominator));
        const SpatialDirection Normal = Normalize(Perpendicular);
        const SpatialDirection StartDirection = Normalize(Difference(Centre, StartPoint));
        const SpatialDirection ThroughDirection = Normalize(Difference(Centre, ThroughPoint));
        const SpatialDirection EndDirection = Normalize(Difference(Centre, EndPoint));
        const double Radius = std::sqrt(LengthSquared(Difference(Centre, StartPoint)));
        // ⚠️ `std::acos` cannot tell a turn one way from a turn the other — it only ever answers 0..pi.
        //    The middle point says which way round the arc goes: if it does NOT lie on the short way from
        //    start to end, the arc is the long way round. Clamped because a dot product of exactly
        //    ±1.0000000001 from rounding makes `acos` return NaN, and a NaN sweep draws nothing at all.
        const auto AngleBetween = [](const SpatialDirection& From, const SpatialDirection& To)
        {
            double Cosine = Dot(From, To);
            Cosine = Cosine < -1.0 ? -1.0 : (Cosine > 1.0 ? 1.0 : Cosine);
            return std::acos(Cosine);
        };

        double Sweep = AngleBetween(StartDirection, EndDirection);
        const double ThroughSweep = AngleBetween(StartDirection, ThroughDirection);
        const double ThroughToEnd  = AngleBetween(ThroughDirection, EndDirection);
        if (ThroughSweep > Sweep || ThroughToEnd > Sweep)
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
