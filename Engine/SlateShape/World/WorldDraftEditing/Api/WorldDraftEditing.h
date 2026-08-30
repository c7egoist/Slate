//============================================================================================================================================
//                                                     WORLDDRAFTEDITING.H
//============================================================================================================================================
// 🧩 Edit and move verbs for the world-space draft. These amend the exact 3D declarations in place, and
//    when one picked edge or vertex belongs to a closed loop they move the connected world points too so
//    the same loop deforms rather than splitting open into duplicate geometry.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "SlateShape/World/WorldDraftPicking/Api/WorldDraftPicking.h"

#include <vector>

namespace Slate
{

struct WorldPlacementSubject
{
    bool ControlPlacement = false;
    WorldPointName Point = {};
    WorldControlName Control = {};
    SpatialPoint Position = {};
};

bool ResolveWorldDraftPointPosition(const WorldDraftStructure& Declared,
                                    WorldPointName Subject,
                                    SpatialPoint& Position);

void AppendWorldPlacementUnique(std::vector<WorldPlacementSubject>& Placements,
                                const WorldPlacementSubject& Placement);
void CollectWorldCurvePlacements(const WorldDraftStructure& Declared,
                                 WorldCurveName Curve,
                                 std::vector<WorldPlacementSubject>& Placements);
void CollectWorldLoopPlacements(const WorldDraftStructure& Declared,
                                WorldLoopName Loop,
                                std::vector<WorldPlacementSubject>& Placements);

Deliver<bool> EnforceWorldDraftPoint(WorldDraftStructure& Declared,
                                     WorldPointName Subject,
                                     const SpatialPoint& Position);
Deliver<bool> EnforceWorldDraftControl(WorldDraftStructure& Declared,
                                       WorldControlName Subject,
                                       const SpatialPoint& Position);

Deliver<bool> MoveWorldDraftPoint(WorldDraftStructure& Declared,
                                  WorldPointName Subject,
                                  const SpatialDirection& Offset);
Deliver<bool> MoveWorldDraftCurve(WorldDraftStructure& Declared,
                                  WorldCurveName Subject,
                                  const SpatialDirection& Offset);
Deliver<bool> MoveWorldDraftLoop(WorldDraftStructure& Declared,
                                 WorldLoopName Subject,
                                 const SpatialDirection& Offset);
Deliver<bool> MoveWorldDraftPick(WorldDraftStructure& Declared,
                                 const WorldPick& Subject,
                                 const SpatialDirection& Offset);

} // namespace Slate
