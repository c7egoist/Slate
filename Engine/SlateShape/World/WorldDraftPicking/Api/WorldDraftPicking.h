//============================================================================================================================================
//                                                     WORLDDRAFTPICKING.H
//============================================================================================================================================
// 🧩 CPU-side picking over the world-space draft authoring model. Selection now answers from exact 3D curves
//    and derived planar loops rather than from one global sketch plane, so overlapping planes can coexist in
//    one view and the nearer world-space candidate wins.

#pragma once

#include "SketchToolset/SketchTool/SelectionOptions/Api/SelectionOptions.h"
#include "SlateShape/World/WorldDraftStructure/Api/WorldDraftStructure.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/ViewportProjection.h"

#include <cstdint>
#include <vector>

namespace Slate
{

enum class WorldControlSubject : std::uint32_t
{
    Centre = 0u,
    Radius = 1u,
    MajorAxis = 2u,
    MinorAxis = 3u,
    Through = 4u,
    ControlPoint = 5u,
    StartTangent = 6u,
    EndTangent = 7u,
    SubjectCount = 8u
};

struct WorldPointName
{
    std::uint32_t IssuedIndex = 0u;
    bool Assigned() const { return IssuedIndex != 0u; }
};

struct WorldControlName
{
    std::uint32_t IssuedIndex = 0u;
    bool Assigned() const { return IssuedIndex != 0u; }
};

struct WorldPointPlacement
{
    WorldPointName Name = {};
    WorldCurveName SourceCurve = {};
    SpatialPoint Position = {};
};

struct WorldControlPlacement
{
    WorldControlName Name = {};
    WorldCurveName SourceCurve = {};
    WorldControlSubject Subject = WorldControlSubject::ControlPoint;
    std::uint32_t LocalIndex = 0u;
    SpatialPoint Position = {};
};

enum class WorldPickSubject : std::uint32_t
{
    None = 0u,
    Point = 1u,
    Control = 2u,
    Curve = 3u,
    Loop = 4u
};

struct WorldPick
{
    WorldPickSubject Subject = WorldPickSubject::None;
    WorldPointName Point = {};
    WorldControlName Control = {};
    WorldCurveName Curve = {};
    WorldLoopName Loop = {};
    SpatialPoint Position = {};

    bool Standing() const { return Subject != WorldPickSubject::None; }
};

bool ResolveWorldDraftPoints(const WorldDraftStructure& Declared,
                             WorldCurveName SourceCurve,
                             std::vector<WorldPointPlacement>& Resolved);
bool ResolveWorldDraftControls(const WorldDraftStructure& Declared,
                               WorldCurveName SourceCurve,
                               std::vector<WorldControlPlacement>& Resolved);

bool ResolveWorldCurvePivot(const WorldDraftStructure& Declared,
                            WorldCurveName Curve,
                            SpatialPoint& Pivot);
bool ResolveWorldLoopPivot(const WorldDraftStructure& Declared,
                           WorldLoopName Loop,
                           SpatialPoint& Pivot);

bool ResolveWorldDraftPick(const WorldDraftStructure& Declared,
                           const ResolvedCamera& Camera,
                           const PlaneExtent& Extent,
                           float ScreenX,
                           float ScreenY,
                           double MaximumDistancePixels,
                           WorldPick& Resolved);

bool ResolveWorldDraftPickForElement(const WorldDraftStructure& Declared,
                                     const ResolvedCamera& Camera,
                                     const PlaneExtent& Extent,
                                     float ScreenX,
                                     float ScreenY,
                                     double MaximumDistancePixels,
                                     SelectionElement Element,
                                     WorldPick& Resolved);

} // namespace Slate
