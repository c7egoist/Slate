//============================================================================================================================================
//                                                   SKETCHRENDERINGPROJECTION.H
//============================================================================================================================================
// 🧩 CPU-side projection from exact sketch / workspace declarations into the bounded CAD drawing record the
//    dedicated parametric workspace pass consumes. Exact declarations remain authoritative; the packet is
//    presentational only.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "Shared/WorkspaceCadPacket.slang.h"
#include "SlateShape/Record/WorkspaceRecordStructure/Api/WorkspaceRecordStructure.h"
#include "SlateShape/Sketch/SketchStructure/Api/SketchStructure.h"

namespace Slate
{

struct SketchRenderingStyle
{
    Unsigned32 SketchCurveColour = PackWorkspaceCadColour(96u, 165u, 250u, 255u);
    Unsigned32 ConstructionCurveColour = PackWorkspaceCadColour(148u, 163u, 184u, 188u);
    Unsigned32 ProfileCurveColour = PackWorkspaceCadColour(224u, 231u, 255u, 255u);
    Unsigned32 SurfaceCurveColour = PackWorkspaceCadColour(52u, 211u, 153u, 255u);
    Unsigned32 SolidCurveColour = PackWorkspaceCadColour(16u, 185u, 129u, 255u);
    Unsigned32 ProfileFillColour = PackWorkspaceCadColour(96u, 165u, 250u, 52u);
    Unsigned32 SurfaceFillColour = PackWorkspaceCadColour(52u, 211u, 153u, 60u);
    Unsigned32 SolidFillColour = PackWorkspaceCadColour(16u, 185u, 129u, 84u);
    Unsigned32 PointColour = PackWorkspaceCadColour(244u, 244u, 245u, 255u);
    Unsigned32 ConstraintColour = PackWorkspaceCadColour(168u, 85u, 247u, 255u);
    Unsigned32 DimensionColour = PackWorkspaceCadColour(245u, 158u, 11u, 255u);
    Real32 CurveThickness = 1.6f;
    Real32 ConstructionThickness = 1.1f;
    Real32 ProfileThickness = 1.8f;
    Real32 PointRadius = 4.0f;
    Real32 ConstraintRadius = 4.5f;
    Real32 DimensionRadius = 5.0f;
    Unsigned32 CurveSteps = 48u;
};

Deliver<bool> ProjectSketchRendering(const SketchStructure& Sketch,
                                     const WorkspaceRecordStructure& Records,
                                     WorkspaceCadPacket& Delivered,
                                     const SketchRenderingStyle& Style = {});

} // namespace Slate
