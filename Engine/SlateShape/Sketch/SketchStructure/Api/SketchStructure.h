//============================================================================================================================================
//                                                        SKETCHSTRUCTURE.H
//============================================================================================================================================
// 🧩 One exact sketch declaration set — plane, curves, profiles, and constraints. This structure is 2D authoring
//    authority only; it does not own any tessellated preview or render resources.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"
#include "SlateShape/Geometry/ProfileSpecification/Api/ProfileSpecification.h"
#include "SlateShape/Sketch/ConstraintSpecification/Api/ConstraintSpecification.h"
#include "SlateShape/Sketch/DimensionSpecification/Api/DimensionSpecification.h"

#include <vector>

namespace Slate
{

struct SketchPlane
{
    SpatialPoint Origin = {};
    SpatialDirection Normal = {};
    SpatialDirection AlongDirection = {};
    bool Declared() const;
};

struct DeclaredSketchCurve
{
    CurveSpecification Geometry = {};
};

class SketchStructure
{
public:
    void DeclarePlane(const SketchPlane& Incoming) { Plane = Incoming; PlaneStanding = true; }
    SketchCurveName DeclareCurve(const CurveSpecification& Incoming);
    ProfileNameInFeature DeclareProfile(const ProfileSpecification& Incoming);
    ConstraintName DeclareConstraint(const ConstraintSpecification& Incoming);
    DimensionName DeclareDimension(const DimensionSpecification& Incoming);

    SketchCurveName DeclareLine(const SpatialPoint& Origin, const SpatialPoint& Terminus);
    SketchCurveName DeclareThreePointArc(const SpatialPoint& StartPoint,
                                         const SpatialPoint& ThroughPoint,
                                         const SpatialPoint& EndPoint);
    SketchCurveName DeclareCircle(const CircleCurve& Declared);
    SketchCurveName DeclareEllipse(const EllipseCurve& Declared);
    SketchCurveName DeclareOval(const EllipseCurve& Declared);
    SketchCurveName DeclareBezier(const std::vector<SpatialPoint>& ControlPoints);
    SketchCurveName DeclareBasisSpline(const BasisSplineCurve& Declared);
    SketchCurveName DeclareRationalSpline(const RationalSplineCurve& Declared);
    SketchCurveName DeclareHermite(const HermiteCurve& Declared);

    Deliver<bool> DeclarePolyline(const std::vector<SpatialPoint>& Positions,
                                  std::vector<SketchCurveName>& DeclaredCurves);
    Deliver<ProfileNameInFeature> DeclareCircleProfile(const CircleCurve& Declared);
    Deliver<ProfileNameInFeature> DeclareEllipseProfile(const EllipseCurve& Declared);
    Deliver<ProfileNameInFeature> DeclareOvalProfile(const EllipseCurve& Declared);
    /// in  StartDirection  [-]  where the FIRST corner sits, so the polygon follows the drag
    /// note 🔴 A zero-length direction keeps the old behaviour of starting on the plane's own axis.
    ///       Without this the first corner always lay along `AlongDirection`, so the committed
    ///       polygon was rotated away from the one the artist had just dragged out and previewed.
    Deliver<ProfileNameInFeature> DeclareRegularPolygon(const SpatialPoint& Centre,
                                                        double Radius,
                                                        std::uint32_t SideCount,
                                                        const SpatialDirection& StartDirection = {});
    Deliver<ProfileNameInFeature> DeclareSlot(const SpatialPoint& StartPoint,
                                              const SpatialPoint& EndPoint,
                                              double Radius);

    const SketchPlane& HeldPlane() const { return Plane; }

    /// 🧩 Whether a plane has been declared and is usable -- the ONE thing drawing genuinely requires.
    /// note 🔴 Distinct from `Declared()`, which is all-or-nothing across every curve, profile and
    ///       constraint. Refusing to draw on that made a single malformed curve blank the entire
    ///       viewport; a renderer needs the coordinate frame, not a clean bill of health.
    /// cost ✔️
    /// tag  api, nonallocating, nonthrowing
    bool PlaneDeclared() const { return PlaneStanding && Plane.Declared(); }
    const std::vector<DeclaredSketchCurve>& Curves() const { return HeldCurves; }
    std::vector<DeclaredSketchCurve>& Curves() { return HeldCurves; }
    const std::vector<ProfileSpecification>& Profiles() const { return HeldProfiles; }
    const std::vector<ConstraintSpecification>& Constraints() const { return HeldConstraints; }
    const std::vector<DimensionSpecification>& Dimensions() const { return HeldDimensions; }
    std::vector<DimensionSpecification>& Dimensions() { return HeldDimensions; }
    bool Declared() const;
    void Reclaim();

private:
    SketchPlane Plane = {};
    bool PlaneStanding = false;
    std::vector<DeclaredSketchCurve> HeldCurves = {};
    std::vector<ProfileSpecification> HeldProfiles = {};
    std::vector<ConstraintSpecification> HeldConstraints = {};
    std::vector<DimensionSpecification> HeldDimensions = {};
};

} // namespace Slate
