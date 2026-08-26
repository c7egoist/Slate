//============================================================================================================================================
//                                                         SKETCHEDITING.H
//============================================================================================================================================
// 🧩 Edit verbs for sketch points and controls. Selection resolves on the CPU; these verbs then amend the exact
//    sketch declarations in place so later profile solving and feature recompute read the same authority.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "SlateFeature/Sketch/SketchStructure/Api/SketchStructure.h"
#include "SlateFeature/Sketch/SketchSelection/Api/SketchSelection.h"

namespace Slate
{

Deliver<bool> EnforceSketchPoint(SketchStructure& Declared,
                                 SketchPointName Subject,
                                 const SpatialPoint& Position);
Deliver<bool> EnforceSketchControl(SketchStructure& Declared,
                                   SketchControlName Subject,
                                   const SpatialPoint& Position);

} // namespace Slate
