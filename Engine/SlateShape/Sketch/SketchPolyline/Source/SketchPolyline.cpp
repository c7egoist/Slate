//============================================================================================================================================
//                                                      SKETCHPOLYLINE.CPP
//============================================================================================================================================

#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"
#include "SlateShape/Sketch/SketchPolyline/Api/SketchPolyline.h"

#include <algorithm>
#include <cmath>

namespace Slate
{

namespace
{
    SpatialPoint EvaluateBezier(const std::vector<SpatialPoint>& ControlPoints,
                                double Parameter)
    {
        std::vector<SpatialPoint> Working = ControlPoints;
        for (std::size_t Width = Working.size(); Width > 1u; --Width)
        {
            for (std::size_t Index = 0u; Index + 1u < Width; ++Index)
            {
                Working[Index] = {
                    Working[Index].Left * (1.0 - Parameter) + Working[Index + 1u].Left * Parameter,
                    Working[Index].Up * (1.0 - Parameter) + Working[Index + 1u].Up * Parameter,
                    Working[Index].Forward * (1.0 - Parameter) + Working[Index + 1u].Forward * Parameter
                };
            }
        }
        return Working.front();
    }

    std::vector<double> BuildClampedKnots(std::uint32_t Degree,
                                          std::size_t ControlCount,
                                          bool Periodic)
    {
        const std::size_t KnotCount = ControlCount + Degree + 1u;
        std::vector<double> Knots(KnotCount, 0.0);
        if (Periodic)
        {
            for (std::size_t KnotIndex = 0u; KnotIndex < KnotCount; ++KnotIndex)
                Knots[KnotIndex] = static_cast<double>(KnotIndex);
            return Knots;
        }

        for (std::size_t KnotIndex = 0u; KnotIndex <= Degree; ++KnotIndex)
            Knots[KnotIndex] = 0.0;
        const std::size_t InteriorCount = ControlCount > Degree + 1u ? ControlCount - Degree - 1u : 0u;
        for (std::size_t InteriorIndex = 0u; InteriorIndex < InteriorCount; ++InteriorIndex)
            Knots[Degree + 1u + InteriorIndex] = static_cast<double>(InteriorIndex + 1u);
        const double Maximum = static_cast<double>(InteriorCount + 1u);
        for (std::size_t KnotIndex = KnotCount - Degree - 1u; KnotIndex < KnotCount; ++KnotIndex)
            Knots[KnotIndex] = Maximum;
        return Knots;
    }

    double BasisValue(std::size_t BasisIndex,
                      std::uint32_t Degree,
                      double Parameter,
                      const std::vector<double>& Knots)
    {
        if (Degree == 0u)
        {
            if ((Knots[BasisIndex] <= Parameter && Parameter < Knots[BasisIndex + 1u])
             || (Parameter == Knots.back() && BasisIndex + 1u == Knots.size() - 1u))
                return 1.0;
            return 0.0;
        }

        double Left = 0.0;
        const double LeftWidth = Knots[BasisIndex + Degree] - Knots[BasisIndex];
        if (LeftWidth > 0.0)
            Left = (Parameter - Knots[BasisIndex]) / LeftWidth * BasisValue(BasisIndex, Degree - 1u, Parameter, Knots);

        double Right = 0.0;
        const double RightWidth = Knots[BasisIndex + Degree + 1u] - Knots[BasisIndex + 1u];
        if (RightWidth > 0.0)
            Right = (Knots[BasisIndex + Degree + 1u] - Parameter) / RightWidth
                  * BasisValue(BasisIndex + 1u, Degree - 1u, Parameter, Knots);

        return Left + Right;
    }

    SpatialPoint EvaluateSpline(const std::vector<SpatialPoint>& ControlPoints,
                                const std::vector<double>* Weights,
                                std::uint32_t Degree,
                                bool Periodic,
                                double Parameter)
    {
        const std::vector<double> Knots = BuildClampedKnots(Degree, ControlPoints.size(), Periodic);
        const double Maximum = Knots[Knots.size() - Degree - 1u];
        const double Local = Parameter * Maximum;

        SpatialPoint Sum = {};
        double WeightSum = 0.0;
        for (std::size_t Index = 0u; Index < ControlPoints.size(); ++Index)
        {
            const double Basis = BasisValue(Index, Degree, Local, Knots);
            const double Weight = Weights != nullptr ? (*Weights)[Index] : 1.0;
            Sum.Left += ControlPoints[Index].Left * Basis * Weight;
            Sum.Up += ControlPoints[Index].Up * Basis * Weight;
            Sum.Forward += ControlPoints[Index].Forward * Basis * Weight;
            WeightSum += Basis * Weight;
        }

        if (WeightSum == 0.0)
            return ControlPoints.front();
        return { Sum.Left / WeightSum, Sum.Up / WeightSum, Sum.Forward / WeightSum };
    }

    SpatialPoint EvaluateHermite(const HermiteCurve& Curve,
                                 double Parameter)
    {
        const double Square = Parameter * Parameter;
        const double Cube = Square * Parameter;
        const double H00 = 2.0 * Cube - 3.0 * Square + 1.0;
        const double H10 = Cube - 2.0 * Square + Parameter;
        const double H01 = -2.0 * Cube + 3.0 * Square;
        const double H11 = Cube - Square;
        return {
            H00 * Curve.StartPoint.Left + H10 * Curve.StartTangent.Left + H01 * Curve.EndPoint.Left + H11 * Curve.EndTangent.Left,
            H00 * Curve.StartPoint.Up + H10 * Curve.StartTangent.Up + H01 * Curve.EndPoint.Up + H11 * Curve.EndTangent.Up,
            H00 * Curve.StartPoint.Forward + H10 * Curve.StartTangent.Forward + H01 * Curve.EndPoint.Forward + H11 * Curve.EndTangent.Forward
        };
    }
}

namespace
{
    /// 🔴 Steps for a circular sweep held to `CurveChordTolerance`. Inverting the sagitta
    ///    `t = R(1 - cos(θ/2))` gives `θ = 2·acos(1 - t/R)`, and the count is the sweep over that.
    std::uint32_t StepsForSweep(double Radius, double SweepRadians)
    {
        const double Sweep = std::fabs(SweepRadians);
        if (!(Radius > CurveChordTolerance) || !(Sweep > 0.0))
            return 0u;

        const double PerStep = 2.0 * std::acos(std::clamp(1.0 - CurveChordTolerance / Radius, -1.0, 1.0));
        if (!(PerStep > 1.0e-9))
            return CurveStepLimit;
        return static_cast<std::uint32_t>(std::ceil(Sweep / PerStep));
    }

    /// 🔴 A spline has no single radius, so the length of its control polygon stands in for arc length —
    ///    of which it is an upper bound. One chord per `8 × CurveChordTolerance` of that length.
    std::uint32_t StepsForControlPolygon(const std::vector<SpatialPoint>& ControlPoints)
    {
        if (ControlPoints.size() < 2u)
            return 0u;

        double Span = 0.0;
        for (std::size_t Index = 0u; Index + 1u < ControlPoints.size(); ++Index)
            Span += std::sqrt(LengthSquared(Difference(ControlPoints[Index], ControlPoints[Index + 1u])));

        return static_cast<std::uint32_t>(std::ceil(Span / (CurveChordTolerance * 8.0)));
    }
}

std::uint32_t ResolveCurveStepCount(const CurveSpecification& Geometry,
                                    std::uint32_t Floor)
{
    std::uint32_t Wanted = 0u;

    switch (Geometry.Subject())
    {
        // 📝 A line is exact at two points however long it is drawn; there is nothing to shrink.
        case CurveSubject::Line:
            return 2u;

        case CurveSubject::CircularArc:
            Wanted = StepsForSweep(Geometry.HeldCircularArc().Radius,
                                   Geometry.HeldCircularArc().SweepRadians);
            break;

        case CurveSubject::Circle:
            Wanted = StepsForSweep(Geometry.HeldCircle().Radius, 6.283185307179586);
            break;

        // 📝 The major radius is the worst case: curvature is highest at the ends of the major axis.
        case CurveSubject::EllipticalArc:
            Wanted = StepsForSweep(Geometry.HeldEllipticalArc().MajorRadius,
                                   Geometry.HeldEllipticalArc().SweepRadians);
            break;

        case CurveSubject::Ellipse:
            Wanted = StepsForSweep(Geometry.HeldEllipse().MajorRadius, 6.283185307179586);
            break;

        case CurveSubject::Bezier:
            Wanted = StepsForControlPolygon(Geometry.HeldBezier().ControlPoints);
            break;

        case CurveSubject::BasisSpline:
            Wanted = StepsForControlPolygon(Geometry.HeldBasisSpline().ControlPoints);
            break;

        case CurveSubject::RationalSpline:
            Wanted = StepsForControlPolygon(Geometry.HeldRationalSpline().ControlPoints);
            break;

        // 📝 A Hermite span is bounded by its endpoints and its two tangents, and a tangent may carry the
        //    curve well past the straight line between them, so all four contribute.
        case CurveSubject::Hermite:
        {
            const HermiteCurve& Hermite = Geometry.HeldHermite();
            Wanted = StepsForControlPolygon({ Hermite.StartPoint,
                                              Added(Hermite.StartPoint, Hermite.StartTangent),
                                              Added(Hermite.EndPoint, Hermite.EndTangent),
                                              Hermite.EndPoint });
            break;
        }

        case CurveSubject::SubjectCount:
        default:
            break;
    }

    return std::clamp(std::max(Wanted, Floor), 2u, CurveStepLimit);
}

void AppendCurvePolyline(const CurveSpecification& Geometry,
                         std::vector<SpatialPoint>& Polyline,
                         std::uint32_t StepCount)
{
    Polyline.clear();
    const std::uint32_t Steps = StepCount < 2u ? 2u : StepCount;

    switch (Geometry.Subject())
    {
        case CurveSubject::Line:
            Polyline.push_back(Geometry.HeldLine().Origin);
            Polyline.push_back(Geometry.HeldLine().Terminus);
            return;

        case CurveSubject::CircularArc:
        {
            const CircularArcCurve& Arc = Geometry.HeldCircularArc();
            const SpatialDirection StartDirection = Normalize(Arc.StartDirection);
            for (std::uint32_t StepIndex = 0u; StepIndex <= Steps; ++StepIndex)
            {
                const double Fraction = static_cast<double>(StepIndex) / static_cast<double>(Steps);
                const SpatialDirection CurrentDirection = RotateAroundAxis(StartDirection, Arc.Normal, Arc.SweepRadians * Fraction);
                Polyline.push_back(Added(Arc.Centre, Scaled(CurrentDirection, Arc.Radius)));
            }
            return;
        }

        case CurveSubject::Circle:
        {
            const CircleCurve& Circle = Geometry.HeldCircle();
            const SpatialDirection StartDirection = Normalize(Circle.StartDirection);
            for (std::uint32_t StepIndex = 0u; StepIndex <= Steps; ++StepIndex)
            {
                const double Fraction = static_cast<double>(StepIndex) / static_cast<double>(Steps);
                const SpatialDirection CurrentDirection = RotateAroundAxis(StartDirection, Circle.Normal, 6.283185307179586 * Fraction);
                Polyline.push_back(Added(Circle.Centre, Scaled(CurrentDirection, Circle.Radius)));
            }
            return;
        }

        case CurveSubject::EllipticalArc:
        {
            const EllipticalArcCurve& Arc = Geometry.HeldEllipticalArc();
            const SpatialDirection MajorDirection = Normalize(Arc.MajorDirection);
            const SpatialDirection MinorDirection = Normalize(Cross(Arc.Normal, MajorDirection));
            for (std::uint32_t StepIndex = 0u; StepIndex <= Steps; ++StepIndex)
            {
                const double Fraction = static_cast<double>(StepIndex) / static_cast<double>(Steps);
                const double Radians = Arc.StartRadians + Arc.SweepRadians * Fraction;
                Polyline.push_back({ Arc.Centre.Left + MajorDirection.Left * Arc.MajorRadius * std::cos(Radians) + MinorDirection.Left * Arc.MinorRadius * std::sin(Radians),
                                     Arc.Centre.Up + MajorDirection.Up * Arc.MajorRadius * std::cos(Radians) + MinorDirection.Up * Arc.MinorRadius * std::sin(Radians),
                                     Arc.Centre.Forward + MajorDirection.Forward * Arc.MajorRadius * std::cos(Radians) + MinorDirection.Forward * Arc.MinorRadius * std::sin(Radians) });
            }
            return;
        }

        case CurveSubject::Ellipse:
        {
            const EllipseCurve& Ellipse = Geometry.HeldEllipse();
            const SpatialDirection MajorDirection = Normalize(Ellipse.MajorDirection);
            const SpatialDirection MinorDirection = Normalize(Cross(Ellipse.Normal, MajorDirection));
            for (std::uint32_t StepIndex = 0u; StepIndex <= Steps; ++StepIndex)
            {
                const double Fraction = static_cast<double>(StepIndex) / static_cast<double>(Steps);
                const double Radians = 6.283185307179586 * Fraction;
                Polyline.push_back({ Ellipse.Centre.Left + MajorDirection.Left * Ellipse.MajorRadius * std::cos(Radians) + MinorDirection.Left * Ellipse.MinorRadius * std::sin(Radians),
                                     Ellipse.Centre.Up + MajorDirection.Up * Ellipse.MajorRadius * std::cos(Radians) + MinorDirection.Up * Ellipse.MinorRadius * std::sin(Radians),
                                     Ellipse.Centre.Forward + MajorDirection.Forward * Ellipse.MajorRadius * std::cos(Radians) + MinorDirection.Forward * Ellipse.MinorRadius * std::sin(Radians) });
            }
            return;
        }

        case CurveSubject::Bezier:
            for (std::uint32_t StepIndex = 0u; StepIndex <= Steps; ++StepIndex)
                Polyline.push_back(EvaluateBezier(Geometry.HeldBezier().ControlPoints,
                                                  static_cast<double>(StepIndex) / static_cast<double>(Steps)));
            return;

        case CurveSubject::BasisSpline:
            for (std::uint32_t StepIndex = 0u; StepIndex <= Steps; ++StepIndex)
                Polyline.push_back(EvaluateSpline(Geometry.HeldBasisSpline().ControlPoints,
                                                  nullptr,
                                                  Geometry.HeldBasisSpline().Degree,
                                                  Geometry.HeldBasisSpline().Periodic,
                                                  static_cast<double>(StepIndex) / static_cast<double>(Steps)));
            return;

        case CurveSubject::RationalSpline:
            for (std::uint32_t StepIndex = 0u; StepIndex <= Steps; ++StepIndex)
                Polyline.push_back(EvaluateSpline(Geometry.HeldRationalSpline().ControlPoints,
                                                  &Geometry.HeldRationalSpline().Weights,
                                                  Geometry.HeldRationalSpline().Degree,
                                                  Geometry.HeldRationalSpline().Periodic,
                                                  static_cast<double>(StepIndex) / static_cast<double>(Steps)));
            return;

        case CurveSubject::Hermite:
            for (std::uint32_t StepIndex = 0u; StepIndex <= Steps; ++StepIndex)
                Polyline.push_back(EvaluateHermite(Geometry.HeldHermite(),
                                                   static_cast<double>(StepIndex) / static_cast<double>(Steps)));
            return;

        case CurveSubject::SubjectCount:
            return;
    }
}

} // namespace Slate
