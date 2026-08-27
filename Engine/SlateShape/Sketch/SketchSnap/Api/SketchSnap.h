//============================================================================================================================================
//                                                           SKETCHSNAP.H
//============================================================================================================================================
// 🧩 CPU-side snapping over exact sketch declarations. Selection and snapping stay semantic on the CPU even when
//    later rendering moves to the GPU.

#pragma once

#include "SlateShape/Sketch/SketchSelection/Api/SketchSelection.h"

namespace Slate
{

enum class SketchSnapSubject : std::uint32_t
{
    None = 0u,
    Endpoint = 1u,
    Midpoint = 2u,
    Centre = 3u,
    Control = 4u,
    AlongCurve = 5u,
    Intersection = 6u,
    Grid = 7u,
    Perpendicular = 8u,
    Tangent = 9u,
    SubjectCount = 10u
};

struct SketchSnapMask
{
    bool EndpointAccepted = true;
    bool MidpointAccepted = true;
    bool CentreAccepted = true;
    bool ControlAccepted = true;
    bool AlongCurveAccepted = true;
    bool IntersectionAccepted = true;
    bool GridAccepted = true;
    bool PerpendicularAccepted = true;
    bool TangentAccepted = true;
};

struct SketchSnapPlacement
{
    SketchSnapSubject Subject = SketchSnapSubject::None;
    SketchCurveName SourceCurve = {};
    SketchPointName SketchPoint = {};
    SketchControlName SketchControl = {};
    SpatialPoint Position = {};
    double Distance = 0.0;

    bool Resolved() const { return Subject != SketchSnapSubject::None; }
};

/// 🧩 The nearest thing worth snapping to, or nothing within reach.
/// note 🔴 `GridAccepted` IS NOW HONOURED. The mask advertised it from the day it was written and this
///       function never produced a `Grid` placement, so every caller that wanted grid snapping had to
///       write its own — and the host did exactly that, against its own step and its own tolerance.
///       Geometry still wins: the grid is only consulted when nothing drawn is within reach, because a
///       grid line passes near everything and would otherwise beat the endpoint the artist aimed at.
/// note ⚠️ `GridStep` is in world units and is clamped to at least 1.0. A step of zero would round every
///       probe onto the plane's origin.
SketchSnapPlacement ResolveNearestSnap(const SketchStructure& Declared,
                                       const SpatialPoint& Probe,
                                       double MaximumDistance,
                                       const SketchSnapMask& Accepted = {},
                                       double GridStep = 10.0);

} // namespace Slate
