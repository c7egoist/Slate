//============================================================================================================================================
//                                                    WORLDDRAFTSKETCHBRIDGE.H
//============================================================================================================================================
// 🧩 Bridges the shipped sketch document to the world-draft interaction and rendering path. Drawing still
//    writes the sketch structure for now; this unit mirrors that geometry into a world draft, maps picks
//    between the two models, syncs transformed world geometry back into the sketch, and projects the
//    resulting true-3D curves through the world renderer.

#pragma once

#include "SlateShape/Sketch/SketchRenderingProjection/Api/SketchRenderingProjection.h"
#include "SlateShape/World/WorldDraftPicking/Api/WorldDraftPicking.h"
#include "SlateShape/World/WorldDraftRenderingProjection/Api/WorldDraftRenderingProjection.h"
#include "SlateWorkspace/Discipline/SketchPicking/Api/SketchPicking.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/DrawableScale.h"
#include "SlateWorkspace/Discipline/WorkplaneStanding/Api/WorkplaneStanding.h"
#include "SlateShape/Record/WorkspaceRevisionSequence/Api/WorkspaceRevisionSequence.h"
#include "SketchToolset/SketchTool/SketchPlacement/Api/SketchPlacement.h"

namespace Slate
{

struct WorldDraftSketchLoopBinding
{
    ProfileNameInFeature Profile = {};
    std::uint32_t ProfileLoopIndex = 0u;
};

struct WorldDraftSketchMapping
{
    std::vector<WorldDraftSketchLoopBinding> Loops = {};
};

bool MirrorSketchIntoWorldDraft(const SketchStructure& Sketch,
                                WorldDraftStructure& Declared,
                                WorldDraftSketchMapping& Mapping);

bool ApplyWorldDraftToSketch(const WorldDraftStructure& Declared,
                             SketchStructure& Sketch);

WorkspaceRecordName ResolveRecordForWorldLoop(const WorkspaceRecordStructure& Records,
                                              const WorldDraftSketchMapping& Mapping,
                                              WorldLoopName Loop);

bool ResolveWorldPickForSketchPick(const SketchStructure& Sketch,
                                   const WorkspaceRecordStructure& Records,
                                   const WorldDraftStructure& Declared,
                                   const WorldDraftSketchMapping& Mapping,
                                   const SketchPick& Selection,
                                   WorldPick& Resolved);

bool ResolveSketchPickForWorldPick(const SketchStructure& Sketch,
                                   const WorkspaceRecordStructure& Records,
                                   const WorldDraftSketchMapping& Mapping,
                                   const WorldPick& Selection,
                                   SketchPick& Resolved);

Deliver<bool> ProjectWorldBackedSketchRendering(const SketchStructure& Sketch,
                                                const ResolvedCamera& Camera,
                                                const PlaneExtent& LogicalExtent,
                                                const DrawableScale& Drawable,
                                                WorkspaceCadPacket& Delivered,
                                                const WorldDraftRenderingStyle& Style = {},
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
                                WorldDraftStructure& Declared,
                                WorldDraftSketchMapping& Mapping,
                                WorkspaceNameIndex& Naming,
                                SketchStructure& Sketch,
                                WorkspaceRecordStructure& Records,
                                WorkspaceRevisionSequence& Revisions,
                                const SealedPlacement& Placed,
                                WorkspaceRecordName& SelectedRecord);

Deliver<bool> ProjectWorldBackedSketchRendering(const WorldDraftStructure& Declared,
                                                const ResolvedCamera& Camera,
                                                const PlaneExtent& LogicalExtent,
                                                const DrawableScale& Drawable,
                                                WorkspaceCadPacket& Delivered,
                                                const WorldDraftRenderingStyle& Style = {},
                                                double ClosureTolerance = 0.01,
                                                double CoplanarTolerance = 0.01);

} // namespace Slate
