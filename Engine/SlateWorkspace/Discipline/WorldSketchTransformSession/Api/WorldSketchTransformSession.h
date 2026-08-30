//============================================================================================================================================
//                                                   WORLDSKETCHTRANSFORMSESSION.H
//============================================================================================================================================
// 🧩 Dragging a world-space sketch selection in true 3D. This is the interaction half that turns a picked
//    world point, edge, loop or control into a live move preview driven by camera rays and axis locks.

#pragma once

#include "SlateShape/World/WorldSketchEditing/Api/WorldSketchEditing.h"
#include "SlateWorkspace/Discipline/TransformSequence/Api/TransformSequence.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/ViewportProjection.h"

#include <vector>

namespace Slate
{

struct WorldSketchTransformSession
{
    TransformStanding Standing = {};

    bool AwaitingRelease = false;
    bool Changed = false;

    WorldPick Target = {};
    std::vector<WorldPlacementSubject> Placements = {};
    std::vector<SpatialPoint> Origins = {};

    SpatialPoint Pivot = {};
    SpatialPoint StartReference = {};
    SpatialDirection AxisDirection = { 1.0, 0.0, 0.0 };
    double PreviewValue = 0.0;

    TransformManner& Manner() { return Standing.Manner; }
    TransformManner Manner() const { return Standing.Manner; }
    bool& Engaged() { return Standing.Engaged; }
    bool Engaged() const { return Standing.Engaged; }
    bool& SlideAlongCurve() { return Standing.SlideAlongCurve; }
    bool SlideAlongCurve() const { return Standing.SlideAlongCurve; }
    TransformRestriction& Restriction() { return Standing.Restriction; }
    TransformRestriction Restriction() const { return Standing.Restriction; }
};

bool ResolveWorldTransformPlacements(const WorldSketchStructure& Declared,
                                     const WorldPick& Target,
                                     SpatialPoint& Pivot,
                                     std::vector<WorldPlacementSubject>& Placements);

SpatialDirection ResolveWorldCurveSlideDirection(const WorldSketchStructure& Declared,
                                                 WorldCurveName Curve,
                                                 const SpatialPoint& NearPosition);

void ApplyWorldTransformPlacements(WorldSketchStructure& Declared,
                                   const WorldSketchTransformSession& Session,
                                   const SpatialDirection& Offset);
void RestoreWorldTransformPlacements(WorldSketchStructure& Declared,
                                     const WorldSketchTransformSession& Session);
void ClearWorldSketchTransformSession(WorldSketchTransformSession& Session);

bool StartWorldSketchTransformSession(const WorldSketchStructure& Declared,
                                     const ResolvedCamera& Camera,
                                     const PlaneExtent& Extent,
                                     float PointerX,
                                     float PointerY,
                                     const WorldPick& Target,
                                     TransformRestriction Restriction,
                                     bool SlideAlongCurve,
                                     WorldSketchTransformSession& Session,
                                     bool MouseDriven = false);

void UpdateWorldSketchTransformSession(const ResolvedCamera& Camera,
                                      const PlaneExtent& Extent,
                                      float PointerX,
                                      float PointerY,
                                      WorldSketchStructure& Declared,
                                      WorldSketchTransformSession& Session);

void CommitWorldSketchTransformSession(WorldSketchTransformSession& Session);
void CancelWorldSketchTransformSession(WorldSketchStructure& Declared,
                                      WorldSketchTransformSession& Session);

} // namespace Slate
