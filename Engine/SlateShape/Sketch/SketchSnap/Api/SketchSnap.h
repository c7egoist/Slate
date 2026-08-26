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

SketchSnapPlacement ResolveNearestSnap(const SketchStructure& Declared,
                                       const SpatialPoint& Probe,
                                       double MaximumDistance,
                                       const SketchSnapMask& Accepted = {});

} // namespace Slate
