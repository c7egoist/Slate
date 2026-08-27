//============================================================================================================================================
//                                                         SKETCHPICKING.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/SketchPicking/Api/SketchPicking.h"

namespace Slate
{

bool ResolveSketchPointPosition(const SketchStructure& Sketch,
                                SketchPointName Subject,
                                SpatialPoint& Position)
{
    if (!Subject.Assigned())
        return false;

    // ⚠️ A point name PACKS its curve into the high bits. A name with no curve part belongs to no curve
    //    and cannot be located, so it is refused rather than searched for.
    const std::uint32_t CurveIndex = Subject.IssuedIndex >> 8u;
    if (CurveIndex == 0u)
        return false;

    std::vector<SketchPointPlacement> Points;
    if (!ResolveSketchPoints(Sketch, { CurveIndex }, Points))
        return false;

    for (const SketchPointPlacement& Current : Points)
        if (Current.Name.IssuedIndex == Subject.IssuedIndex)
        {
            Position = Current.Position;
            return true;
        }

    return false;
}

bool ProfileContainsCurve(const ProfileSpecification& Profile, SketchCurveName Curve)
{
    for (const ProfileLoop& Loop : Profile.HeldLoops())
        for (const ProfileCurveUse& Use : Loop.Traversal)
            if (Use.TraversedCurve.IssuedIndex == Curve.IssuedIndex)
                return true;
    return false;
}

WorkspaceRecordName ResolveRecordForPoint(const WorkspaceRecordStructure& Records, SketchPointName Point)
{
    // 🔴 REFUSES AN UNASSIGNED NAME. Without this the loop below matches the first record whose
    //    `SketchPoint` is also zero — which is EVERY record that carries no point at all, so a dimension
    //    or a folder would be handed back as the owner of a point that does not exist. The shipped code
    //    had no such guard and relied on every caller having a real point; the proof pins it instead.
    if (!Point.Assigned())
        return {};

    for (std::uint32_t Index = 1u; Index <= Records.DeclaredCount(); ++Index)
    {
        const WorkspaceRecord* Record = Records.Resolve({ Index });
        if (Record != nullptr && Record->SketchPoint.IssuedIndex == Point.IssuedIndex)
            return { Index };
    }

    // 📝 A point with no record of its own belongs to the curve that owns it.
    const std::uint32_t CurveIndex = Point.IssuedIndex >> 8u;
    if (CurveIndex != 0u)
        for (std::uint32_t Index = 1u; Index <= Records.DeclaredCount(); ++Index)
        {
            const WorkspaceRecord* Record = Records.Resolve({ Index });
            if (Record != nullptr && Record->SketchCurve.IssuedIndex == CurveIndex)
                return { Index };
        }

    return {};
}

WorkspaceRecordName ResolveRecordForCurve(const SketchStructure& Sketch,
                                          const WorkspaceRecordStructure& Records,
                                          SketchCurveName Curve)
{
    // 🔴 The same hazard as above: an unassigned curve would match every record that carries no curve.
    if (!Curve.Assigned())
        return {};

    // 🔴 A DIRECT RECORD WINS. Grabbing one edge of a rectangle should select the edge when the edge is a
    //    record in its own right, and the enclosing profile only when it is not.
    for (std::uint32_t Index = 1u; Index <= Records.DeclaredCount(); ++Index)
    {
        const WorkspaceRecord* Record = Records.Resolve({ Index });
        if (Record != nullptr && Record->SketchCurve.IssuedIndex == Curve.IssuedIndex)
            return { Index };
    }

    for (std::uint32_t Index = 1u; Index <= Records.DeclaredCount(); ++Index)
    {
        const WorkspaceRecord* Record = Records.Resolve({ Index });
        if (Record == nullptr || !Record->Profile.Assigned() ||
            Record->Profile.IssuedIndex > Sketch.Profiles().size())
            continue;
        if (ProfileContainsCurve(Sketch.Profiles()[Record->Profile.IssuedIndex - 1u], Curve))
            return { Index };
    }

    return {};
}

bool ResolveCurvePivot(const SketchStructure& Sketch, SketchCurveName Curve, SpatialPoint& Pivot)
{
    if (!Curve.Assigned() || Curve.IssuedIndex > Sketch.Curves().size())
        return false;

    const CurveSpecification& Geometry = Sketch.Curves()[Curve.IssuedIndex - 1u].Geometry;
    switch (Geometry.Subject())
    {
        case CurveSubject::Line:
        {
            const LineCurve& Line = Geometry.HeldLine();
            Pivot = { (Line.Origin.Left + Line.Terminus.Left) * 0.5,
                      (Line.Origin.Up + Line.Terminus.Up) * 0.5,
                      (Line.Origin.Forward + Line.Terminus.Forward) * 0.5 };
            return true;
        }
        case CurveSubject::CircularArc:
            Pivot = Geometry.HeldCircularArc().Centre;
            return true;
        case CurveSubject::Circle:
            Pivot = Geometry.HeldCircle().Centre;
            return true;
        case CurveSubject::EllipticalArc:
            Pivot = Geometry.HeldEllipticalArc().Centre;
            return true;
        case CurveSubject::Ellipse:
            Pivot = Geometry.HeldEllipse().Centre;
            return true;

        // 📝 The MIDDLE control point, not the average of them. Averaging drags the pivot towards wherever
        //    the controls bunch up, so it would shift under the artist as they edit the spline.
        case CurveSubject::Bezier:
            if (!Geometry.HeldBezier().ControlPoints.empty())
            {
                Pivot = Geometry.HeldBezier().ControlPoints[Geometry.HeldBezier().ControlPoints.size() / 2u];
                return true;
            }
            return false;
        case CurveSubject::BasisSpline:
            if (!Geometry.HeldBasisSpline().ControlPoints.empty())
            {
                Pivot = Geometry.HeldBasisSpline().ControlPoints[Geometry.HeldBasisSpline().ControlPoints.size() / 2u];
                return true;
            }
            return false;
        case CurveSubject::RationalSpline:
            if (!Geometry.HeldRationalSpline().ControlPoints.empty())
            {
                Pivot = Geometry.HeldRationalSpline().ControlPoints[Geometry.HeldRationalSpline().ControlPoints.size() / 2u];
                return true;
            }
            return false;
        case CurveSubject::Hermite:
        {
            const HermiteCurve& Hermite = Geometry.HeldHermite();
            Pivot = { (Hermite.StartPoint.Left + Hermite.EndPoint.Left) * 0.5,
                      (Hermite.StartPoint.Up + Hermite.EndPoint.Up) * 0.5,
                      (Hermite.StartPoint.Forward + Hermite.EndPoint.Forward) * 0.5 };
            return true;
        }

        // ⚠️ No default arm: a new curve subject must be given a pivot deliberately, not silently refused.
        case CurveSubject::SubjectCount:
            return false;
    }
    return false;
}

bool ResolveProfilePivot(const SketchStructure& Sketch, ProfileNameInFeature Profile, SpatialPoint& Pivot)
{
    if (!Profile.Assigned() || Profile.IssuedIndex > Sketch.Profiles().size())
        return false;

    const ProfileSpecification& Source = Sketch.Profiles()[Profile.IssuedIndex - 1u];
    std::uint32_t Count = 0u;
    Pivot = {};

    // ⚠️ The mean of the CURVES' pivots, not of their points. A curve cut into more segments than its
    //    neighbours would otherwise drag the profile's pivot towards itself.
    for (const ProfileLoop& Loop : Source.HeldLoops())
        for (const ProfileCurveUse& Use : Loop.Traversal)
        {
            SpatialPoint CurvePivot = {};
            if (!ResolveCurvePivot(Sketch, { Use.TraversedCurve.IssuedIndex }, CurvePivot))
                continue;
            Pivot = { Pivot.Left + CurvePivot.Left,
                      Pivot.Up + CurvePivot.Up,
                      Pivot.Forward + CurvePivot.Forward };
            ++Count;
        }

    if (Count == 0u)
        return false;

    const double Scale = 1.0 / static_cast<double>(Count);
    Pivot = { Pivot.Left * Scale, Pivot.Up * Scale, Pivot.Forward * Scale };
    return true;
}

void AppendPlacementUnique(std::vector<SketchPlacementSubject>& Placements,
                           const SketchPlacementSubject& Placement)
{
    // 🔴 Two curves meeting at a corner both report that corner. Collecting it twice would move it twice
    //    as far as the cursor travelled.
    for (const SketchPlacementSubject& Existing : Placements)
    {
        if (Placement.ControlPlacement == Existing.ControlPlacement)
        {
            if (!Placement.ControlPlacement && Placement.Point.IssuedIndex == Existing.Point.IssuedIndex)
                return;
            if (Placement.ControlPlacement && Placement.Control.IssuedIndex == Existing.Control.IssuedIndex)
                return;
        }
    }
    Placements.push_back(Placement);
}

void CollectCurvePlacements(const SketchStructure& Sketch,
                            SketchCurveName Curve,
                            std::vector<SketchPlacementSubject>& Placements)
{
    std::vector<SketchPointPlacement> Points;
    if (ResolveSketchPoints(Sketch, Curve, Points))
        for (const SketchPointPlacement& Point : Points)
            AppendPlacementUnique(Placements, { false, Point.Name, {}, Point.Position });

    std::vector<SketchControlPlacement> Controls;
    if (ResolveSketchControls(Sketch, Curve, Controls))
        for (const SketchControlPlacement& Control : Controls)
            AppendPlacementUnique(Placements, { true, {}, Control.Name, Control.Position });
}

void CollectProfilePlacements(const SketchStructure& Sketch,
                              ProfileNameInFeature Profile,
                              std::vector<SketchPlacementSubject>& Placements)
{
    if (!Profile.Assigned() || Profile.IssuedIndex > Sketch.Profiles().size())
        return;

    for (const ProfileLoop& Loop : Sketch.Profiles()[Profile.IssuedIndex - 1u].HeldLoops())
        for (const ProfileCurveUse& Use : Loop.Traversal)
            CollectCurvePlacements(Sketch, { Use.TraversedCurve.IssuedIndex }, Placements);
}

SketchPick ResolveSketchPick(const SketchStructure& Sketch,
                             const WorkspaceRecordStructure& Records,
                             const SpatialPoint& Probe,
                             double MaximumDistance)
{
    // 🔴 POINT FIRST. A curve passes through its own endpoints, so a search that compared distances would
    //    return the curve and the artist could never grab the end they were aiming at.
    SketchPointPlacement Point = {};
    double Distance = MaximumDistance;
    if (ResolveNearestSketchPoint(Sketch, Probe, MaximumDistance, Point, Distance))
    {
        SketchPick Pick = {};
        Pick.Subject  = SketchPickSubject::Point;
        Pick.Point    = Point.Name;
        Pick.Curve    = Point.SourceCurve;
        Pick.Position = Point.Position;
        Pick.Record   = ResolveRecordForPoint(Records, Point.Name);

        // ⚠️ Only returned once the directory can name it. A pick with no record is a selection nothing
        //    can act on, so the search falls through to the next kind instead.
        if (Pick.Record.Assigned())
            return Pick;
    }

    SketchControlPlacement Control = {};
    Distance = MaximumDistance;
    if (ResolveNearestSketchControl(Sketch, Probe, MaximumDistance, Control, Distance))
    {
        SketchPick Pick = {};
        Pick.Subject  = SketchPickSubject::Control;
        Pick.Control  = Control.Name;
        Pick.Curve    = Control.SourceCurve;
        Pick.Position = Control.Position;
        Pick.Record   = ResolveRecordForCurve(Sketch, Records, Control.SourceCurve);
        if (Pick.Record.Assigned())
            return Pick;
    }

    SketchCurveName Curve = {};
    Distance = MaximumDistance;
    if (ResolveNearestSketchCurve(Sketch, Probe, MaximumDistance, Curve, Distance))
    {
        SketchPick Pick = {};
        Pick.Subject = SketchPickSubject::Curve;
        Pick.Curve   = Curve;
        Pick.Record  = ResolveRecordForCurve(Sketch, Records, Curve);

        // 📝 A curve pick reports its PIVOT, not the point on it nearest the cursor: that is what a
        //    following rotate or scale turns about.
        ResolveCurvePivot(Sketch, Curve, Pick.Position);
        return Pick;
    }

    return {};
}

bool ResolvePickForRecord(const SketchStructure& Sketch,
                          const WorkspaceRecordStructure& Records,
                          WorkspaceRecordName Record,
                          SketchPick& Pick)
{
    Pick = {};
    const WorkspaceRecord* Held = Records.Resolve(Record);
    if (Held == nullptr)
        return false;

    Pick.Record = Record;
    switch (Held->Subject)
    {
        case WorkspaceRecordSubject::Point:
            Pick.Subject = SketchPickSubject::Point;
            Pick.Point   = Held->SketchPoint;
            return ResolveSketchPointPosition(Sketch, Held->SketchPoint, Pick.Position);

        case WorkspaceRecordSubject::OpenCurve:
            Pick.Subject = SketchPickSubject::Curve;
            Pick.Curve   = Held->SketchCurve;
            return ResolveCurvePivot(Sketch, Held->SketchCurve, Pick.Position);

        // 📝 A profile, a surface and a solid are all picked AS the record. There is no smaller thing to
        //    name: the artist selected the whole object in the outliner.
        case WorkspaceRecordSubject::ClosedProfile:
        case WorkspaceRecordSubject::ThinSurface:
        case WorkspaceRecordSubject::Solid:
            Pick.Subject = SketchPickSubject::Record;
            return ResolveProfilePivot(Sketch, Held->Profile, Pick.Position);

        // ⚠️ A dimension, a constraint, a folder and a pattern have no geometry to grab. Refusing is
        //    correct: selecting one in the outliner must not arm a transform.
        default:
            return false;
    }
}

}   // namespace Slate
