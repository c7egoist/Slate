//============================================================================================================================================
//                                                          SKETCHBASIS.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/ViewportProjection/Api/SketchBasis.h"

namespace Slate
{

SpatialBasis ResolveSketchBasis(const SketchStructure& Sketch)
{
    if (!Sketch.Declared())
        return { {}, { 1.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0 }, { 0.0, 1.0, 0.0 } };

    const SketchPlane& Plane = Sketch.HeldPlane();
    const SpatialDirection Along  = Normalize(Plane.AlongDirection);
    const SpatialDirection Normal = Normalize(Plane.Normal);
    const SpatialDirection Across = Normalize(Cross(Normal, Along));
    return { Plane.Origin, Along, Across, Normal };
}

}   // namespace Slate
