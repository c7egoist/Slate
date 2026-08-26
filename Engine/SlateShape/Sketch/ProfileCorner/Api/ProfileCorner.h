//============================================================================================================================================
//                                                           PROFILECORNER.H
//============================================================================================================================================
// 🧩 Two-dimensional fillet and chamfer over resolved profile loops. This stage operates on line-only loops and
//    emits a fresh profile declaration, which is enough to make later trim/cut/inset work on the same authority.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "SlateShape/Sketch/SketchStructure/Api/SketchStructure.h"

namespace Slate
{

enum class CornerDisposition : std::uint32_t
{
    NotRequested = 0u,
    InvalidSpecification = 1u,
    UnsupportedGeometry = 2u,
    Produced = 3u
};

CornerDisposition EvaluateProfileCorner(const SketchStructure& Declared,
                                        ProfileNameInFeature Subject,
                                        std::uint32_t LoopIndex,
                                        std::uint32_t CornerIndex,
                                        double Radius,
                                        bool Chamfer);

Deliver<ProfileNameInFeature> ApplyProfileCorner(SketchStructure& Declared,
                                                 ProfileNameInFeature Subject,
                                                 std::uint32_t LoopIndex,
                                                 std::uint32_t CornerIndex,
                                                 double Radius,
                                                 bool Chamfer);

} // namespace Slate
