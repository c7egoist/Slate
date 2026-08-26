//============================================================================================================================================
//                                                        SKETCHPOLYLINE.H
//============================================================================================================================================
// 🧩 Polyline evaluation of exact sketch curves for selection, snapping, booleans and visual proofing. The exact
//    sketch declarations remain authoritative; this is the shared approximation seam those consumers read.

#pragma once

#include "SlateShape/Geometry/CurveSpecification/Api/CurveSpecification.h"

#include <cstdint>
#include <vector>

namespace Slate
{

void AppendCurvePolyline(const CurveSpecification& Geometry,
                         std::vector<SpatialPoint>& Polyline,
                         std::uint32_t StepCount = 48u);

} // namespace Slate
