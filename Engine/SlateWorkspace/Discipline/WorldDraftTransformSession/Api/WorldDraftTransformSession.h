//============================================================================================================================================
//                                                   WORLDDRAFTTRANSFORMSESSION.H
//============================================================================================================================================
// 🧩 Dragging a world-space draft selection in true 3D. This is the interaction half that turns a picked
//    world point, edge, loop or control into a live move preview driven by camera rays and axis locks.

#pragma once

#include "SlateShape/World/WorldDraftEditing/Api/WorldDraftEditing.h"
#include "SlateWorkspace/Discipline/TransformSequence/Api/TransformSequence.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/ViewportProjection.h"

#include <vector>

namespace Slate
{

struct WorldDraftTransformSession
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

bool ResolveWorldTransformPlacements(const WorldDraftStructure& Declared,
                                     const WorldPick& Target,
                                     SpatialPoint& Pivot,
                                     std::vector<WorldPlacementSubject>& Placements);

SpatialDirection ResolveWorldCurveSlideDirection(const WorldDraftStructure& Declared,
                                                 WorldCurveName Curve,
                                                 const SpatialPoint& NearPosition);

void ApplyWorldTransformPlacements(WorldDraftStructure& Declared,
                                   const WorldDraftTransformSession& Session,
                                   const SpatialDirection& Offset);
void RestoreWorldTransformPlacements(WorldDraftStructure& Declared,
                                     const WorldDraftTransformSession& Session);
void ClearWorldDraftTransformSession(WorldDraftTransformSession& Session);

bool StartWorldDraftTransformSession(const WorldDraftStructure& Declared,
                                     const ResolvedCamera& Camera,
                                     const PlaneExtent& Extent,
                                     float PointerX,
                                     float PointerY,
                                     const WorldPick& Target,
                                     TransformRestriction Restriction,
                                     bool SlideAlongCurve,
                                     WorldDraftTransformSession& Session,
                                     bool MouseDriven = false);

void UpdateWorldDraftTransformSession(const ResolvedCamera& Camera,
                                      const PlaneExtent& Extent,
                                      float PointerX,
                                      float PointerY,
                                      WorldDraftStructure& Declared,
                                      WorldDraftTransformSession& Session);

void CommitWorldDraftTransformSession(WorldDraftTransformSession& Session);
void CancelWorldDraftTransformSession(WorldDraftStructure& Declared,
                                      WorldDraftTransformSession& Session);

} // namespace Slate
