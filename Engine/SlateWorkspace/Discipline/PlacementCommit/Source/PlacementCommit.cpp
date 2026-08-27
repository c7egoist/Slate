//============================================================================================================================================
//                                                       PLACEMENTCOMMIT.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/PlacementCommit/Api/PlacementCommit.h"

#include "SlateShape/Sketch/ConstraintSolver/Api/ConstraintSolver.h"

#include <cmath>
#include <string>
#include <vector>

namespace Slate
{

namespace
{

constexpr double CommitPi = 3.14159265358979323846;

//------------------------------------------------------------------------------------------------------------------------
//                                                    SHARED BY THE ARMS
//------------------------------------------------------------------------------------------------------------------------

ReferenceSpecification ReferenceFromPoint(SketchPointName Point)
{
    ReferenceSpecification Reference = {};
    Reference.Subject = ReferenceSubject::SketchPoint;
    Reference.SketchPoint = Point;
    return Reference;
}

ReferenceSpecification ReferenceFromCurve(SketchCurveName Curve)
{
    ReferenceSpecification Reference = {};
    Reference.Subject = ReferenceSubject::SketchCurve;
    Reference.SketchCurve = Curve;
    return Reference;
}


SketchPointName EncodePlacedPointName(SketchCurveName Curve, std::uint32_t LocalIndex)
{
    return { (Curve.IssuedIndex << 8u) | ((LocalIndex + 1u) & 0xFFu) };
}

ReferenceSpecification ReferenceFromSnap(const SketchSnapPlacement& Snap)
{
    ReferenceSpecification Reference = {};
    if (Snap.SketchPoint.Assigned())
    {
        Reference.Subject = ReferenceSubject::SketchPoint;
        Reference.SketchPoint = Snap.SketchPoint;
    }
    else if (Snap.SketchControl.Assigned())
    {
        Reference.Subject = ReferenceSubject::SketchControl;
        Reference.SketchControl = Snap.SketchControl;
    }
    else if (Snap.SourceCurve.Assigned())
    {
        Reference.Subject = ReferenceSubject::SketchCurve;
        Reference.SketchCurve = Snap.SourceCurve;
    }
    return Reference;
}


bool ResolveThreePointCircle(const SpatialPoint& A,
                             const SpatialPoint& B,
                             const SpatialPoint& C,
                             SpatialPoint& Centre,
                             double& Radius)
{
    const double D = 2.0 * (A.Left * (B.Forward - C.Forward) + B.Left * (C.Forward - A.Forward) + C.Left * (A.Forward - B.Forward));
    if (std::abs(D) <= 1.0e-9)
        return false;
    const double A2 = A.Left * A.Left + A.Forward * A.Forward;
    const double B2 = B.Left * B.Left + B.Forward * B.Forward;
    const double C2 = C.Left * C.Left + C.Forward * C.Forward;
    Centre.Left = (A2 * (B.Forward - C.Forward) + B2 * (C.Forward - A.Forward) + C2 * (A.Forward - B.Forward)) / D;
    Centre.Forward = (A2 * (C.Left - B.Left) + B2 * (A.Left - C.Left) + C2 * (B.Left - A.Left)) / D;
    Centre.Up = A.Up;
    Radius = std::sqrt(LengthSquared(Difference(Centre, A)));
    return Radius > 1.0e-6;
}

void AddLineAutoConstraints(WorkspaceNameIndex& Naming,
                            SketchStructure& Sketch,
                            WorkspaceRecordStructure& Records,
                            PlacementJournal& Revisions,
                            SketchCurveName Curve,
                            const SpatialPoint& StartPoint,
                            const SpatialPoint& EndPoint,
                            const SketchSnapPlacement* StartSnap,
                            const SketchSnapPlacement* EndSnap,
                            std::vector<WorkspaceRecordName>& Written)
{
    const SpatialBasis Basis = ResolveSketchBasis(Sketch);
    double StartAlong = 0.0, StartAcross = 0.0, EndAlong = 0.0, EndAcross = 0.0;
    ResolvePlaneCoordinates(Basis, StartPoint, StartAlong, StartAcross);
    ResolvePlaneCoordinates(Basis, EndPoint, EndAlong, EndAcross);
    const double Span = std::sqrt((EndAlong - StartAlong) * (EndAlong - StartAlong) +
                                  (EndAcross - StartAcross) * (EndAcross - StartAcross));
    if (Span > 1.0e-6)
    {
        ConstraintSpecification Axis = {};
        Axis.Primary = ReferenceFromCurve(Curve);
        if (std::fabs(EndAcross - StartAcross) <= std::max(Span * 0.015, 0.05))
        {
            Axis.Subject = ConstraintSubject::Horizontal;
            SealConstraintRecord(Naming, Records, Revisions, Sketch, Axis, Written);
        }
        else if (std::fabs(EndAlong - StartAlong) <= std::max(Span * 0.015, 0.05))
        {
            Axis.Subject = ConstraintSubject::Vertical;
            SealConstraintRecord(Naming, Records, Revisions, Sketch, Axis, Written);
        }
    }

    const SketchPointName StartNamed = EncodePlacedPointName(Curve, 0u);
    const SketchPointName EndNamed = EncodePlacedPointName(Curve, 1u);
    const SketchSnapPlacement* Snaps[2] = { StartSnap, EndSnap };
    const SketchPointName NewPoints[2] = { StartNamed, EndNamed };
    for (std::uint32_t Index = 0u; Index < 2u; ++Index)
    {
        if (Snaps[Index] == nullptr || !Snaps[Index]->SketchPoint.Assigned())
            continue;
        ConstraintSpecification Coincident = {};
        Coincident.Subject = ConstraintSubject::Coincident;
        Coincident.Primary = ReferenceFromPoint(NewPoints[Index]);
        Coincident.Secondary = ReferenceFromPoint(Snaps[Index]->SketchPoint);
        SealConstraintRecord(Naming, Records, Revisions, Sketch, Coincident, Written);
    }
}

Deliver<WorkspaceRecordName> DeclareLine(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const SketchCurveName Curve = Sketch.DeclareLine(Placed.Anchors[0], Placed.Anchors[1]);
        const WorkspaceRecordName Record = DeclareWorkspaceCurve(Naming, Records, Curve, Placed.Construction);
        std::vector<WorkspaceRecordName> Written = { Record };
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming),
                       Placed.Construction ? "Create Construction Curve" : "Create Curve", { Record },
                       Revisions.DeclaredCount() + 1u);
        AddLineAutoConstraints(Naming, Sketch, Records, Revisions, Curve,
                               Placed.Anchors[0], Placed.Anchors[1],
                               Placed.Placements.size() > 0u ? &Placed.Placements[0] : nullptr,
                               Placed.Placements.size() > 1u ? &Placed.Placements[1] : nullptr,
                               Written);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclarePoint(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const SpatialPoint Tip = Added(Placed.Anchors.back(), Scaled(Normalize(Sketch.HeldPlane().AlongDirection), 0.001));
        const SketchCurveName Curve = Sketch.DeclareLine(Placed.Anchors.back(), Tip);
        const WorkspaceRecordName Record = DeclareWorkspacePoint(Naming, Records, EncodePlacedPointName(Curve, 0u));
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Point", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclarePolyline(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        std::vector<SketchCurveName> Curves;
        const Deliver<bool> Declared = Sketch.DeclarePolyline(Placed.Anchors, Curves);
        if (!Declared.Resolved)
            return Deliver<WorkspaceRecordName>::Refuse(Declared.Error);

        std::vector<WorkspaceRecordName> RecordsWritten;
        RecordsWritten.reserve(Curves.size());
        for (std::uint32_t Index = 0u; Index < Curves.size(); ++Index)
        {
            SketchCurveName Curve = Curves[Index];
            RecordsWritten.push_back(DeclareWorkspaceCurve(Naming, Records, Curve, Placed.Construction));
            AddLineAutoConstraints(Naming, Sketch, Records, Revisions, Curve,
                                   Placed.Anchors[Index], Placed.Anchors[Index + 1u],
                                   Placed.Placements.size() > Index ? &Placed.Placements[Index] : nullptr,
                                   Placed.Placements.size() > Index + 1u ? &Placed.Placements[Index + 1u] : nullptr,
                                   RecordsWritten);
        }
        Revisions.Seal("Declared polyline", Placed.Construction ? "Create Construction Polyline" : "Create Polyline",
                       RecordsWritten, Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(RecordsWritten.empty() ? WorkspaceRecordName{} : RecordsWritten.front());
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareThreePointArc(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        if (!ArcReady(Placed.Anchors[0], Placed.Anchors[1], Placed.Anchors[2]))
            return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                          "the arc points are collinear" });
        const SketchCurveName Curve = Sketch.DeclareThreePointArc(Placed.Anchors[0], Placed.Anchors[1], Placed.Anchors[2]);
        const WorkspaceRecordName Record = DeclareWorkspaceCurve(Naming, Records, Curve, Placed.Construction);
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming),
                       Placed.Construction ? "Create Construction Arc" : "Create Arc", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareCentredArc(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const SpatialPoint Centre = Placed.Anchors[0];
        const SpatialPoint Start = Placed.Anchors[1];
        const SpatialPoint End = Placed.Anchors[2];
        const double Radius = std::sqrt(LengthSquared(Difference(Centre, Start)));
        if (Radius <= 1.0e-6)
            return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported, "the arc radius is too small" });
        const double A0 = std::atan2(Start.Forward - Centre.Forward, Start.Left - Centre.Left);
        const double A1 = std::atan2(End.Forward - Centre.Forward, End.Left - Centre.Left);
        double Sweep = A1 - A0;
        if (Sweep <= 0.0)
            Sweep += 2.0 * CommitPi;
        const CircularArcCurve Arc = { Centre, Sketch.HeldPlane().Normal, Normalize(Difference(Centre, Start)), {}, false, Radius, Sweep };
        const SketchCurveName Curve = Sketch.DeclareCurve(CurveSpecification::DeclareCircularArc(Arc, { 0.0, 1.0 }));
        const WorkspaceRecordName Record = DeclareWorkspaceCurve(Naming, Records, Curve, Placed.Construction);
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming),
                       Placed.Method == PlacementMethod::Tangent ? "Create Tangent Arc" : "Create Centred Arc", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareEllipticalArc(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const SpatialPoint Centre = Placed.Anchors[0];
        const double Major = std::max(std::abs(Placed.Anchors[1].Left - Centre.Left), 1.0e-6);
        const double Minor = std::max(std::abs(Placed.Anchors[1].Forward - Centre.Forward), Major * 0.5);
        const double EndAngle = std::atan2((Placed.Anchors[2].Forward - Centre.Forward) / Minor,
                                           (Placed.Anchors[2].Left - Centre.Left) / Major);
        const EllipticalArcCurve Arc = { Centre, Sketch.HeldPlane().Normal, Sketch.HeldPlane().AlongDirection,
                                         Major, Minor, 0.0, EndAngle <= 0.0 ? EndAngle + 2.0 * CommitPi : EndAngle };
        const SketchCurveName Curve = Sketch.DeclareCurve(CurveSpecification::DeclareEllipticalArc(Arc, { 0.0, 1.0 }));
        const WorkspaceRecordName Record = DeclareWorkspaceCurve(Naming, Records, Curve, Placed.Construction);
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Elliptical Arc", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareBasisSpline(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        BasisSplineCurve Spline;
        Spline.ControlPoints = Placed.Anchors;
        Spline.Degree = std::min<std::uint32_t>(3u, static_cast<std::uint32_t>(Spline.ControlPoints.size() - 1u));
        Spline.Periodic = false;
        const SketchCurveName Curve = Sketch.DeclareBasisSpline(Spline);
        const WorkspaceRecordName Record = DeclareWorkspaceCurve(Naming, Records, Curve, Placed.Construction);
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Basis Spline", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareRationalSpline(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        RationalSplineCurve Spline;
        Spline.ControlPoints = Placed.Anchors;
        Spline.Weights.assign(Placed.Anchors.size(), 1.0);
        Spline.Degree = std::min<std::uint32_t>(3u, static_cast<std::uint32_t>(Spline.ControlPoints.size() - 1u));
        Spline.Periodic = false;
        const SketchCurveName Curve = Sketch.DeclareRationalSpline(Spline);
        const WorkspaceRecordName Record = DeclareWorkspaceCurve(Naming, Records, Curve, Placed.Construction);
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create NURBS Curve", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareHermite(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        HermiteCurve CurveData;
        CurveData.StartPoint = Placed.Anchors[0];
        CurveData.EndPoint = Placed.Anchors[1];
        CurveData.StartTangent = Difference(Placed.Anchors[0], Placed.Anchors[2]);
        CurveData.EndTangent = Difference(Placed.Anchors[1], Placed.Anchors[3]);
        const SketchCurveName Curve = Sketch.DeclareHermite(CurveData);
        const WorkspaceRecordName Record = DeclareWorkspaceCurve(Naming, Records, Curve, Placed.Construction);
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Hermite Curve", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareDiameterCircle(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const SpatialPoint A = Placed.Anchors[0];
        const SpatialPoint B = Placed.Anchors[1];
        const SpatialPoint Centre = { (A.Left + B.Left) * 0.5, (A.Up + B.Up) * 0.5, (A.Forward + B.Forward) * 0.5 };
        const double Radius = std::sqrt(LengthSquared(Difference(Centre, A)));
        if (Radius <= 1.0e-6)
            return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported, "the circle radius is too small" });
        const CircleCurve Circle = { Centre, Sketch.HeldPlane().Normal, Normalize(Difference(Centre, A)), Radius };
        const Deliver<ProfileNameInFeature> Profile = Placed.Construction ? Deliver<ProfileNameInFeature>::Refuse({ RefusalReason::ContentUnsupported, "construction circle" })
                                                                         : Sketch.DeclareCircleProfile(Circle);
        if (Profile.Resolved)
        {
            const WorkspaceRecordName Record = DeclareWorkspaceProfile(Naming, Records, Profile.Resolve());
            Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Diameter Circle", { Record },
                           Revisions.DeclaredCount() + 1u);
            return Deliver<WorkspaceRecordName>::Result(Record);
        }
        const SketchCurveName Curve = Sketch.DeclareCircle(Circle);
        const WorkspaceRecordName Record = DeclareWorkspaceCurve(Naming, Records, Curve, true);
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Construction Diameter Circle", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareThreePointCircle(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        SpatialPoint Centre = {};
        double Radius = 0.0;
        if (!ResolveThreePointCircle(Placed.Anchors[0], Placed.Anchors[1], Placed.Anchors[2], Centre, Radius))
            return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported, "the circle points are collinear" });
        const CircleCurve Circle = { Centre, Sketch.HeldPlane().Normal, Normalize(Difference(Centre, Placed.Anchors[0])), Radius };
        const Deliver<ProfileNameInFeature> Profile = Sketch.DeclareCircleProfile(Circle);
        if (!Profile.Resolved)
            return Deliver<WorkspaceRecordName>::Refuse(Profile.Error);
        const WorkspaceRecordName Record = DeclareWorkspaceProfile(Naming, Records, Profile.Resolve());
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Three Point Circle", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclarePolygon(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const double Radius = std::sqrt(LengthSquared(Difference(Placed.Anchors[0], Placed.Anchors[1])));
        const Deliver<ProfileNameInFeature> Profile = Sketch.DeclareRegularPolygon(Placed.Anchors[0], Radius, 6u);
        if (!Profile.Resolved)
            return Deliver<WorkspaceRecordName>::Refuse(Profile.Error);
        const WorkspaceRecordName Record = DeclareWorkspaceProfile(Naming, Records, Profile.Resolve());
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Polygon", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareSlot(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const double Radius = std::sqrt(LengthSquared(Difference(Placed.Anchors[1], Placed.Anchors[2])));
        const Deliver<ProfileNameInFeature> Profile = Sketch.DeclareSlot(Placed.Anchors[0], Placed.Anchors[1], Radius);
        if (!Profile.Resolved)
            return Deliver<WorkspaceRecordName>::Refuse(Profile.Error);
        const WorkspaceRecordName Record = DeclareWorkspaceProfile(Naming, Records, Profile.Resolve());
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Slot", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareThreePointRectangle(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const SpatialPoint A = Placed.Anchors[0];
        const SpatialPoint B = Placed.Anchors[1];
        const SpatialPoint C = Placed.Anchors[2];
        const SpatialPoint D = Added(A, Difference(B, C));
        ProfileSpecification Profile;
        Profile.DeclarePlane({ Sketch.HeldPlane().Origin, Sketch.HeldPlane().Normal, Sketch.HeldPlane().AlongDirection });
        ProfileLoop Loop;
        Loop.Orientation = ProfileLoopOrientation::Outer;
        const SketchCurveName AB = Sketch.DeclareLine(A, B);
        const SketchCurveName BC = Sketch.DeclareLine(B, C);
        const SketchCurveName CD = Sketch.DeclareLine(C, D);
        const SketchCurveName DA = Sketch.DeclareLine(D, A);
        Loop.Traversal = { { { AB.IssuedIndex }, true }, { { BC.IssuedIndex }, true }, { { CD.IssuedIndex }, true }, { { DA.IssuedIndex }, true } };
        Profile.DeclareLoop(Loop);
        const WorkspaceRecordName Record = DeclareWorkspaceProfile(Naming, Records, Sketch.DeclareProfile(Profile));
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Three Point Rectangle", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareDimension(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const ReferenceSpecification Primary = ReferenceFromSnap(Placed.Placements[0]);
        const ReferenceSpecification Secondary = ReferenceFromSnap(Placed.Placements[1]);
        if (!Primary.Declared() || !Secondary.Declared())
            return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                          "linear dimensions require two snapped sketch references" });
        DimensionSpecification Dimension = {};
        Dimension.Subject = DimensionSubject::Aligned;
        Dimension.Primary = Primary;
        Dimension.Secondary = Secondary;
        Dimension.Target = std::sqrt(LengthSquared(Difference(Placed.Anchors[0], Placed.Anchors[1])));
        const DimensionName DimensionNamed = Sketch.DeclareDimension(Dimension);
        const WorkspaceRecordName Record = DeclareWorkspaceDimension(Naming, Records, DimensionNamed);
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Dimension", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareEllipse(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const SpatialBasis Basis = ResolveSketchBasis(Sketch);
        double CentreAlong = 0.0, CentreAcross = 0.0, HoverAlong = 0.0, HoverAcross = 0.0;
        ResolvePlaneCoordinates(Basis, Placed.Anchors[0], CentreAlong, CentreAcross);
        ResolvePlaneCoordinates(Basis, Placed.Anchors.back(), HoverAlong, HoverAcross);
        const double Major = std::fabs(HoverAlong - CentreAlong);
        double Minor = std::fabs(HoverAcross - CentreAcross);
        if (Major <= 1.0e-6)
            return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                          "the ellipse major radius is too small" });
        if (Minor <= 1.0e-6)
            Minor = Major * 0.5;
        const EllipseCurve Ellipse = { Placed.Anchors[0], Sketch.HeldPlane().Normal, Sketch.HeldPlane().AlongDirection, Major, Minor };
        if (Placed.Construction)
        {
            const SketchCurveName Curve = Sketch.DeclareEllipse(Ellipse);
            const WorkspaceRecordName Record = DeclareWorkspaceCurve(Naming, Records, Curve, true);
            Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Construction Ellipse", { Record },
                           Revisions.DeclaredCount() + 1u);
            return Deliver<WorkspaceRecordName>::Result(Record);
        }
        const Deliver<ProfileNameInFeature> Profile = Sketch.DeclareEllipseProfile(Ellipse);
        if (!Profile.Resolved)
            return Deliver<WorkspaceRecordName>::Refuse(Profile.Error);
        const WorkspaceRecordName Record = DeclareWorkspaceProfile(Naming, Records, Profile.Resolve());
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Ellipse Profile", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareBezier(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const SketchCurveName Curve = Sketch.DeclareBezier(Placed.Anchors);
        const WorkspaceRecordName Record = DeclareWorkspaceCurve(Naming, Records, Curve, Placed.Construction);
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming),
                       Placed.Construction ? "Create Construction Bezier" : "Create Bezier", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareCentreRadiusCircle(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const SpatialDirection Radius = Difference(Placed.Anchors[0], Placed.Anchors.back());
        const double RadiusLength = std::sqrt(LengthSquared(Radius));
        if (RadiusLength <= 1.0e-6)
            return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                          "the circle radius is too small" });

        const CircleCurve Circle = { Placed.Anchors[0], Sketch.HeldPlane().Normal, Normalize(Radius), RadiusLength };
        if (Placed.Construction)
        {
            const SketchCurveName Curve = Sketch.DeclareCircle(Circle);
            const WorkspaceRecordName Record = DeclareWorkspaceCurve(Naming, Records, Curve, true);
            Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming),
                           "Create Construction Circle", { Record },
                           Revisions.DeclaredCount() + 1u);
            return Deliver<WorkspaceRecordName>::Result(Record);
        }

        const Deliver<ProfileNameInFeature> Profile = Sketch.DeclareCircleProfile(Circle);
        if (!Profile.Resolved)
            return Deliver<WorkspaceRecordName>::Refuse(Profile.Error);
        const WorkspaceRecordName Record = DeclareWorkspaceProfile(Naming, Records, Profile.Resolve());
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Profile", { Record },
                       Revisions.DeclaredCount() + 1u);
        DimensionSpecification RadiusDimension = {};
        RadiusDimension.Subject = DimensionSubject::Radius;
        RadiusDimension.Primary.Subject = ReferenceSubject::Profile;
        RadiusDimension.Primary.Profile = Profile.Resolve();
        RadiusDimension.Target = RadiusLength;
        if (RadiusDimension.Declared())
        {
            const DimensionName DimensionNamed = Sketch.DeclareDimension(RadiusDimension);
            const WorkspaceRecordName DimensionRecord = DeclareWorkspaceDimension(Naming, Records, DimensionNamed);
            Revisions.Seal("Declared " + std::string(Records.Resolve(DimensionRecord)->Naming), "Create Radius Dimension", { DimensionRecord },
                           Revisions.DeclaredCount() + 1u);
        }
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareCentredRectangle(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const SpatialPoint Centre = Placed.Anchors[0];
        const SpatialDirection Span = Difference(Centre, Placed.Anchors.back());
        const SpatialPoint A = Added(Centre, Scaled(Span, -1.0));
        const SpatialPoint C = Placed.Anchors.back();
        const SpatialPoint B = { C.Left, A.Up, A.Forward };
        const SpatialPoint D = { A.Left, A.Up, C.Forward };
        ProfileSpecification Profile;
        Profile.DeclarePlane({ Sketch.HeldPlane().Origin, Sketch.HeldPlane().Normal, Sketch.HeldPlane().AlongDirection });
        ProfileLoop Loop;
        Loop.Orientation = ProfileLoopOrientation::Outer;
        const SketchCurveName AB = Sketch.DeclareLine(A, B);
        const SketchCurveName BC = Sketch.DeclareLine(B, C);
        const SketchCurveName CD = Sketch.DeclareLine(C, D);
        const SketchCurveName DA = Sketch.DeclareLine(D, A);
        Loop.Traversal = { { { AB.IssuedIndex }, true }, { { BC.IssuedIndex }, true }, { { CD.IssuedIndex }, true }, { { DA.IssuedIndex }, true } };
        Profile.DeclareLoop(Loop);
        const WorkspaceRecordName Record = DeclareWorkspaceProfile(Naming, Records, Sketch.DeclareProfile(Profile));
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Center Rectangle", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

Deliver<WorkspaceRecordName> DeclareExtentRectangle(WorkspaceNameIndex& Naming,
                                    SketchStructure& Sketch,
                                    WorkspaceRecordStructure& Records,
                                    PlacementJournal& Revisions,
                                    const SealedPlacement& Placed)
{
        const SpatialPoint A = Placed.Anchors[0];
        const SpatialPoint C = Placed.Anchors.back();
        const SpatialPoint B = { C.Left, A.Up, A.Forward };
        const SpatialPoint D = { A.Left, A.Up, C.Forward };

        ProfileSpecification Profile;
        Profile.DeclarePlane({ Sketch.HeldPlane().Origin, Sketch.HeldPlane().Normal, Sketch.HeldPlane().AlongDirection });
        ProfileLoop Loop;
        Loop.Orientation = ProfileLoopOrientation::Outer;
        const SketchCurveName AB = Sketch.DeclareLine(A, B);
        const SketchCurveName BC = Sketch.DeclareLine(B, C);
        const SketchCurveName CD = Sketch.DeclareLine(C, D);
        const SketchCurveName DA = Sketch.DeclareLine(D, A);
        if (Placed.Construction)
        {
            const WorkspaceRecordName First = DeclareWorkspaceCurve(Naming, Records, AB, true);
            const WorkspaceRecordName Second = DeclareWorkspaceCurve(Naming, Records, BC, true);
            const WorkspaceRecordName Third = DeclareWorkspaceCurve(Naming, Records, CD, true);
            const WorkspaceRecordName Fourth = DeclareWorkspaceCurve(Naming, Records, DA, true);
            Revisions.Seal("Declared construction rectangle", "Create Construction Rectangle",
                           { First, Second, Third, Fourth }, Revisions.DeclaredCount() + 1u);
            return Deliver<WorkspaceRecordName>::Result(First);
        }
        Loop.Traversal = { { { AB.IssuedIndex }, true }, { { BC.IssuedIndex }, true },
                           { { CD.IssuedIndex }, true }, { { DA.IssuedIndex }, true } };
        Profile.DeclareLoop(Loop);
        const ProfileNameInFeature ProfileName = Sketch.DeclareProfile(Profile);
        const WorkspaceRecordName Record = DeclareWorkspaceProfile(Naming, Records, ProfileName);
        Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Profile", { Record },
                       Revisions.DeclaredCount() + 1u);
        return Deliver<WorkspaceRecordName>::Result(Record);
    return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                  "the placement does not describe a shape" });
}

/// 🧩 The dispatch table: which declarer answers which pair.
/// note 🔴 BOTH AXES, ALWAYS. A row matching only the subject is what let a centred circle be built as a
///       centre-and-radius circle and left the centred-arc code unreachable.
struct CommitRow
{
    SketchSubject   Subject;
    PlacementMethod Method;
    std::uint32_t   Required;
    Deliver<WorkspaceRecordName> (*Declare)(WorkspaceNameIndex&,
                                            SketchStructure&,
                                            WorkspaceRecordStructure&,
                                            PlacementJournal&,
                                            const SealedPlacement&);
};

constexpr CommitRow CommitTable[] =
{
    { SketchSubject::Point,          PlacementMethod::Extent,     1u, DeclarePoint },
    { SketchSubject::Line,           PlacementMethod::Extent,     2u, DeclareLine },
    { SketchSubject::Polyline,       PlacementMethod::Extent,     2u, DeclarePolyline },
    { SketchSubject::Bezier,         PlacementMethod::Extent,     2u, DeclareBezier },
    { SketchSubject::BasisSpline,    PlacementMethod::Extent,     3u, DeclareBasisSpline },
    { SketchSubject::RationalSpline, PlacementMethod::Extent,     3u, DeclareRationalSpline },
    { SketchSubject::Hermite,        PlacementMethod::Extent,     4u, DeclareHermite },
    { SketchSubject::Dimension,      PlacementMethod::Extent,     2u, DeclareDimension },
    { SketchSubject::Slot,           PlacementMethod::Extent,     3u, DeclareSlot },

    // 🔴 The arc arms the host could never reach. `Centred` and `Tangent` both take a centre, a start
    //    and an end; `ThreePoint` and the bare method take three points ON the arc. Those are different
    //    shapes from the same three anchors, and the host built the second for all four.
    { SketchSubject::Arc,            PlacementMethod::Extent,     3u, DeclareThreePointArc },
    { SketchSubject::Arc,            PlacementMethod::ThreePoint, 3u, DeclareThreePointArc },
    { SketchSubject::Arc,            PlacementMethod::Centred,    3u, DeclareCentredArc },
    { SketchSubject::Arc,            PlacementMethod::Tangent,    3u, DeclareCentredArc },

    { SketchSubject::EllipticalArc,  PlacementMethod::Extent,     3u, DeclareEllipticalArc },
    { SketchSubject::EllipticalArc,  PlacementMethod::ThreePoint, 3u, DeclareEllipticalArc },
    { SketchSubject::EllipticalArc,  PlacementMethod::Centred,    3u, DeclareEllipticalArc },

    { SketchSubject::Circle,         PlacementMethod::Extent,     2u, DeclareCentreRadiusCircle },
    { SketchSubject::Circle,         PlacementMethod::Centred,    2u, DeclareCentreRadiusCircle },
    { SketchSubject::Circle,         PlacementMethod::Diameter,   2u, DeclareDiameterCircle },
    { SketchSubject::Circle,         PlacementMethod::ThreePoint, 3u, DeclareThreePointCircle },

    { SketchSubject::Ellipse,        PlacementMethod::Extent,     2u, DeclareEllipse },
    { SketchSubject::Ellipse,        PlacementMethod::Centred,    2u, DeclareEllipse },
    { SketchSubject::Ellipse,        PlacementMethod::Diameter,   2u, DeclareEllipse },

    { SketchSubject::Rectangle,      PlacementMethod::Extent,     2u, DeclareExtentRectangle },
    { SketchSubject::Rectangle,      PlacementMethod::Centred,    2u, DeclareCentredRectangle },
    { SketchSubject::Rectangle,      PlacementMethod::ThreePoint, 3u, DeclareThreePointRectangle },

    { SketchSubject::Polygon,        PlacementMethod::Centred,    2u, DeclarePolygon },
};

const CommitRow* ResolveCommitRow(SketchSubject Subject, PlacementMethod Method)
{
    for (const CommitRow& Row : CommitTable)
        if (Row.Subject == Subject && Row.Method == Method)
            return &Row;
    return nullptr;
}

}   // namespace

void SealConstraintRecord(WorkspaceNameIndex& Naming,
                          WorkspaceRecordStructure& Records,
                          PlacementJournal& Revisions,
                          SketchStructure& Sketch,
                          const ConstraintSpecification& Constraint,
                          std::vector<WorkspaceRecordName>& Written)
{
    if (!Constraint.Declared())
        return;
    const ConstraintName Named = Sketch.DeclareConstraint(Constraint);
    Discard(ApplyConstraint(Sketch, Named));
    const WorkspaceRecordName Record = DeclareWorkspaceConstraint(Naming, Records, Named);
    Written.push_back(Record);
    Revisions.Seal("Declared " + std::string(Records.Resolve(Record)->Naming), "Create Constraint", { Record },
                   Revisions.DeclaredCount() + 1u);
}

bool ArcReady(const SpatialPoint& StartPoint,
                   const SpatialPoint& ThroughPoint,
                   const SpatialPoint& EndPoint)
{
    const SpatialDirection First = Difference(StartPoint, ThroughPoint);
    const SpatialDirection Second = Difference(StartPoint, EndPoint);
    return LengthSquared(First) > 1.0e-8
        && LengthSquared(Second) > 1.0e-8
        && LengthSquared(Cross(First, Second)) > 1.0e-8;
}

bool CommitSupported(SketchSubject Subject, PlacementMethod Method)
{
    return ResolveCommitRow(Subject, Method) != nullptr;
}

Deliver<WorkspaceRecordName> CommitPlacement(WorkspaceNameIndex& Naming,
                                             SketchStructure& Sketch,
                                             WorkspaceRecordStructure& Records,
                                             WorkspaceRevisionSequence& Revisions,
                                             const SealedPlacement& Placed)
{
    const CommitRow* Row = ResolveCommitRow(Placed.Subject, Placed.Method);
    if (Row == nullptr)
        return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                      "that shape cannot be placed that way" });

    // ⚠️ Counted here, once, rather than repeated in every arm. A dimension is measured by its resolved
    //    placements rather than by bare anchors, which is the one exception.
    const std::size_t Standing = Placed.Subject == SketchSubject::Dimension ? Placed.Placements.size()
                                                                            : Placed.Anchors.size();
    if (Standing < Row->Required)
        return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                      "the placement has too few anchors" });

    // 🔴 One journal per placement, closed once. Every arm below writes into it — including the
    //    auto-constraints a line brings with it and the radius dimension a circle declares for itself —
    //    and the whole placement becomes a single step in the history.
    PlacementJournal Journal(Revisions);
    const Deliver<WorkspaceRecordName> Made = Row->Declare(Naming, Sketch, Records, Journal, Placed);
    if (!Made.Resolved)
        return Made;

    Journal.Close();
    return Made;
}

Deliver<WorkspaceRecordName> CommitConstraint(WorkspaceNameIndex& Naming,
                                              SketchStructure& Sketch,
                                              WorkspaceRecordStructure& Records,
                                              WorkspaceRevisionSequence& Revisions,
                                              const ConstraintSpecification& Constraint)
{
    if (!Constraint.Declared())
        return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                      "the constraint names nothing to relate" });

    std::vector<WorkspaceRecordName> Written;
    {
        PlacementJournal Journal(Revisions);
        SealConstraintRecord(Naming, Records, Journal, Sketch, Constraint, Written);
        Journal.Close();
    }

    if (Written.empty())
        return Deliver<WorkspaceRecordName>::Refuse({ RefusalReason::ContentUnsupported,
                                                      "the constraint wrote no record" });

    return Deliver<WorkspaceRecordName>::Result(Written.front());
}

}   // namespace Slate
