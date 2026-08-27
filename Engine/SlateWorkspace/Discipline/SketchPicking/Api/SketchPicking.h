//============================================================================================================================================
//                                                          SKETCHPICKING.H
//============================================================================================================================================
// 🧩 What the artist just pointed at, and what moves when they drag it.
//
// 🔴 THE PRIORITY IS THE WHOLE DESIGN. A point, a spline control and a curve can all be within reach of one
//    cursor position, and picking the nearest of the three is wrong: a curve's body passes through every
//    one of its own endpoints, so distance alone would hand back the curve and the artist could never grab
//    the end they were aiming at. The order is POINT, then CONTROL, then CURVE — smallest target first —
//    and each is answered at its own full tolerance rather than competing on distance.
//
// 📝 Everything here is device-free. It reads the sketch and the record directory and answers in world
//    positions; where those land on screen belongs to `ViewportProjection`, and the tolerance the caller
//    passes is the only thing that knows about the view.

#pragma once

#include "SlateShape/Record/WorkspaceRecordStructure/Api/WorkspaceRecordStructure.h"
#include "SlateShape/Sketch/SketchSelection/Api/SketchSelection.h"
#include "SlateShape/Sketch/SketchSnap/Api/SketchSnap.h"
#include "SlateShape/Sketch/SketchStructure/Api/SketchStructure.h"

#include <cstdint>
#include <vector>

namespace Slate
{

/// 🧩 What kind of thing a pick landed on.
enum class SketchPickSubject : std::uint32_t
{
    None    = 0u,
    Point   = 1u,
    Control = 2u,
    Curve   = 3u,
    Record  = 4u
};

/// 🧩 One pick: what it is, which document record owns it, and where it sits in the world.
struct SketchPick
{
    SketchPickSubject   Subject  = SketchPickSubject::None;
    WorkspaceRecordName Record   = {};
    SketchPointName     Point    = {};
    SketchControlName   Control  = {};
    SketchCurveName     Curve    = {};
    SpatialPoint        Position = {};

    bool Standing() const { return Subject != SketchPickSubject::None; }
};

/// 🧩 One thing a transform will move.
struct SketchPlacementSubject
{
    bool              ControlPlacement = false;
    SketchPointName   Point            = {};
    SketchControlName Control          = {};
    SpatialPoint      Position         = {};
};

/// 🧩 Where a point sits, given the packed name a pick handed back.
/// note ⚠️ A sketch point name PACKS its curve into the high bits — `IssuedIndex >> 8` is the curve and the
///       low byte is the position along it. A name whose curve part is zero belongs to no curve and cannot
///       be resolved, which is why that is refused rather than searched for.
bool ResolveSketchPointPosition(const SketchStructure& Sketch,
                                SketchPointName Subject,
                                SpatialPoint& Position);

/// 🧩 Whether a profile is built from a particular curve.
bool ProfileContainsCurve(const ProfileSpecification& Profile, SketchCurveName Curve);

/// 🧩 The directory record that owns a sketch point.
/// note 🔴 An unassigned name resolves to nothing. Without that guard the search matches the first record
///       whose point is ALSO unassigned — which is every record carrying no point — so a folder or a
///       dimension would be handed back as the owner of a point that does not exist.
WorkspaceRecordName ResolveRecordForPoint(const WorkspaceRecordStructure& Records, SketchPointName Point);

/// 🧩 The directory record that owns a curve.
/// note 📝 A curve may be named directly by a record, or be one edge of a closed profile that is. The
///       direct record wins — grabbing an edge of a rectangle should select the edge if the edge is a
///       record in its own right, and the rectangle only if it is not.
WorkspaceRecordName ResolveRecordForCurve(const SketchStructure& Sketch,
                                          const WorkspaceRecordStructure& Records,
                                          SketchCurveName Curve);

/// 🧩 The point a curve rotates and scales about.
/// out   Named  [-]  false when the curve has no meaningful centre
/// note 📝 A centre for the shapes that have one, the midpoint for those that do not. A spline uses its
///       MIDDLE control point rather than an average: averaging drags the pivot towards wherever the
///       controls happen to bunch up, which moves under the artist as they edit.
bool ResolveCurvePivot(const SketchStructure& Sketch, SketchCurveName Curve, SpatialPoint& Pivot);

/// 🧩 The point a whole profile rotates and scales about.
/// note ⚠️ The mean of its curves' pivots, NOT the mean of its points. A curve subdivided into more
///       segments than its neighbours would otherwise drag the pivot towards itself.
bool ResolveProfilePivot(const SketchStructure& Sketch, ProfileNameInFeature Profile, SpatialPoint& Pivot);

/// 🧩 Adds a placement unless the same point or control is already listed.
/// note 🔴 Two curves meeting at a corner both report that corner. Moving it twice would move it twice as
///       far, so the same subject is never collected more than once.
void AppendPlacementUnique(std::vector<SketchPlacementSubject>& Placements,
                           const SketchPlacementSubject& Placement);

/// 🧩 Everything a curve owns that a transform can move: its points and its controls.
void CollectCurvePlacements(const SketchStructure& Sketch,
                            SketchCurveName Curve,
                            std::vector<SketchPlacementSubject>& Placements);

/// 🧩 The same for every curve a profile is built from.
void CollectProfilePlacements(const SketchStructure& Sketch,
                              ProfileNameInFeature Profile,
                              std::vector<SketchPlacementSubject>& Placements);

/// 🧩 What the artist pointed at.
/// in    Probe            [-]  where the pointer landed, already resolved onto the sketch plane
/// in    MaximumDistance  [-]  how far a candidate may be and still count, in plane units
/// note 🔴 POINT, THEN CONTROL, THEN CURVE — smallest target first, each at its own full tolerance. A
///       curve passes through its own endpoints, so competing on distance would make an endpoint
///       unreachable.
/// note ⚠️ A point or control that resolves to NO record is skipped and the search continues. Handing back
///       a pick the directory cannot name gives the artist a selection nothing can act on.
SketchPick ResolveSketchPick(const SketchStructure& Sketch,
                             const WorkspaceRecordStructure& Records,
                             const SpatialPoint& Probe,
                             double MaximumDistance);

/// 🧩 The pick a directory record corresponds to, for a selection made in the outliner rather than the
///    viewport.
bool ResolvePickForRecord(const SketchStructure& Sketch,
                          const WorkspaceRecordStructure& Records,
                          WorkspaceRecordName Record,
                          SketchPick& Pick);

}   // namespace Slate
