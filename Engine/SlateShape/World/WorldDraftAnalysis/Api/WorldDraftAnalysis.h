//============================================================================================================================================
//                                                     WORLDDRAFTANALYSIS.H
//============================================================================================================================================
// 🧩 Derived loop analysis for the world-space sketch replacement path. Closed loops stay as topology even when
//    they are no longer planar; planarity is derived afterwards to decide whether a fill, profile handoff, or
//    later solid operation is honest.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "SlateShape/Geometry/ProfileSpecification/Api/ProfileSpecification.h"
#include "SlateShape/World/WorldDraftStructure/Api/WorldDraftStructure.h"

#include <cstdint>
#include <vector>

namespace Slate
{

enum class WorldLoopIssueSubject : std::uint32_t
{
    MissingCurve = 0u,
    Gap = 1u,
    OpenLoop = 2u,
    NonCoplanar = 3u,
    DegeneratePlane = 4u
};

struct WorldLoopIssue
{
    WorldLoopName Loop = {};
    WorldLoopIssueSubject Subject = WorldLoopIssueSubject::MissingCurve;
    WorldCurveName FirstCurve = {};
    WorldCurveName SecondCurve = {};
    SpatialPoint Primary = {};
    SpatialPoint Secondary = {};
    double Distance = 0.0;
};

struct WorldLoopAnalysisRecord
{
    WorldLoopName Loop = {};
    std::vector<SpatialPoint> Outline = {};
    bool Closed = false;
    bool Coplanar = false;
    bool FillEligible = false;
    WorldPlacementFrame SupportFrame = {};
    double MaximumDeviation = 0.0;
};

struct WorldDraftAnalysis
{
    std::vector<WorldLoopAnalysisRecord> Loops = {};
    std::vector<WorldLoopIssue> Issues = {};
};

WorldDraftAnalysis AnalyzeWorldDraft(const WorldDraftStructure& Declared,
                                     std::uint32_t StepFloor = 48u,
                                     double ClosureTolerance = 0.01,
                                     double CoplanarTolerance = 0.01);

Deliver<ProfileSpecification> ResolvePlanarWorldLoopProfile(const WorldDraftStructure& Declared,
                                                            WorldLoopName Subject,
                                                            std::uint32_t StepFloor = 48u,
                                                            double ClosureTolerance = 0.01,
                                                            double CoplanarTolerance = 0.01);

} // namespace Slate
