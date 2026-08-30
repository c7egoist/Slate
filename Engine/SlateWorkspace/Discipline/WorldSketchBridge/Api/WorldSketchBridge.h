//============================================================================================================================================
//                                                    WORLDSKETCHSKETCHBRIDGE.H
//============================================================================================================================================
// 🧩 Bridges the shipped sketch document to the world-sketch interaction and rendering path. Drawing still
//    writes the sketch structure for now; this unit mirrors that geometry into a world sketch, maps picks
//    between the two models, syncs transformed world geometry back into the sketch, and projects the
//    resulting true-3D curves through the world renderer.

#pragma once

#include "SlateShape/Sketch/SketchRenderingProjection/Api/SketchRenderingProjection.h"
#include "SlateShape/World/WorldSketchPicking/Api/WorldSketchPicking.h"
#include "SlateWorkspace/Discipline/SketchPicking/Api/SketchPicking.h"
#include "SlateWorkspace/Discipline/WorldSketchRenderingProjection/Api/WorldSketchRenderingProjection.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/DrawableScale.h"
#include "SlateWorkspace/Discipline/WorkplaneStanding/Api/WorkplaneStanding.h"
#include "SlateShape/Record/WorkspaceRevisionSequence/Api/WorkspaceRevisionSequence.h"
#include "SketchToolset/SketchTool/SketchPlacement/Api/SketchPlacement.h"

namespace Slate
{

struct WorldSketchLoopReference
{
    ProfileNameInFeature Profile = {};
    std::uint32_t ProfileLoopIndex = 0u;
};

struct WorldSketchMapping
{
    std::vector<WorldSketchLoopReference> Loops = {};
};

bool MirrorSketchIntoWorldSketch(const SketchStructure& Sketch,
                                WorldSketchStructure& Declared,
                                WorldSketchMapping& Mapping);

bool ApplyWorldSketchToSketch(const WorldSketchStructure& Declared,
                             SketchStructure& Sketch);

WorkspaceRecordName ResolveRecordForWorldLoop(const WorkspaceRecordStructure& Records,
                                              const WorldSketchMapping& Mapping,
                                              WorldLoopName Loop);

bool ResolveWorldPickForSketchPick(const SketchStructure& Sketch,
                                   const WorkspaceRecordStructure& Records,
                                   const WorldSketchStructure& Declared,
                                   const WorldSketchMapping& Mapping,
                                   const SketchPick& Selection,
                                   WorldPick& Resolved);

bool ResolveSketchPickForWorldPick(const SketchStructure& Sketch,
                                   const WorkspaceRecordStructure& Records,
                                   const WorldSketchMapping& Mapping,
                                   const WorldPick& Selection,
                                   SketchPick& Resolved);

Deliver<bool> ProjectWorldBackedSketchRendering(const SketchStructure& Sketch,
                                                const ResolvedCamera& Camera,
                                                const PlaneExtent& LogicalExtent,
                                                const DrawableScale& Drawable,
                                                WorkspaceCadPacket& Delivered,
                                                const WorldSketchRenderingStyle& Style = {},
                                                double ClosureTolerance = 0.01,
                                                double CoplanarTolerance = 0.01);

bool ProjectWorldPlacementPreview(const ResolvedCamera& Camera,
                                  const PlaneExtent& LogicalExtent,
                                  const DrawableScale& Drawable,
                                  const std::vector<CurveSpecification>& Geometry,
                                  const std::vector<SpatialPoint>& Anchors,
                                  const SpatialPoint& Hover,
                                  WorkspaceCadPacket& Delivered,
                                  const SketchRenderingStyle& Style = {});

bool CommitPlacementWorldBacked(const Workplane& ActiveWorkplane,
                                WorldSketchStructure& Declared,
                                WorldSketchMapping& Mapping,
                                WorkspaceNameIndex& Naming,
                                SketchStructure& Sketch,
                                WorkspaceRecordStructure& Records,
                                WorkspaceRevisionSequence& Revisions,
                                const SealedPlacement& Placed,
                                WorkspaceRecordName& SelectedRecord);

Deliver<bool> ProjectWorldBackedSketchRendering(const WorldSketchStructure& Declared,
                                                const ResolvedCamera& Camera,
                                                const PlaneExtent& LogicalExtent,
                                                const DrawableScale& Drawable,
                                                WorkspaceCadPacket& Delivered,
                                                const WorldSketchRenderingStyle& Style = {},
                                                double ClosureTolerance = 0.01,
                                                double CoplanarTolerance = 0.01);

} // namespace Slate
