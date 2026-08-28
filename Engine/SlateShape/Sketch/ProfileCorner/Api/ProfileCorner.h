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

/// 🧩 Which corner of which profile loop sits nearest a probe, for a curve the artist has selected.
/// note  🔴 A CORNER IS NOT A CURVE. `ApplyProfileCorner` rounds the junction of two curves in a
///        resolved loop, but the artist selects a CURVE and clicks near one of its ends. Something has to
///        turn the one into the other, and without it the corner solver -- 214 working lines of it -- had
///        no reachable caller in the whole tree.
struct ProfileCornerTarget
{
    ProfileNameInFeature Profile     = {};
    std::uint32_t        LoopIndex   = 0u;
    std::uint32_t        CornerIndex = 0u;
    SpatialPoint         Position    = {};   // [-] - where the corner actually is
};

/// 🧩 Finds the profile corner nearest `Probe` among the loops that traverse `Subject`.
/// in    Declared  [-] the sketch to search
/// in    Subject   [-] the curve the artist selected; only loops using it are considered
/// in    Probe     [-] where the artist clicked
/// out   Result    [-] refuses when the curve belongs to no resolved loop, which is the ordinary case for
///                     a loose line and is why the caller must have a non-profile path as well
/// note  ⚠️ The corner index is the one `ApplyProfileCorner` expects: the junction BEFORE traversal
///        entry `CornerIndex`, so index zero is the joint between the last curve and the first.
/// note  📝 Walks the loops of the newest profile that names the curve.
/// cost  🚩
/// tag   api, nonthrowing
Deliver<ProfileCornerTarget> ResolveProfileCornerNear(const SketchStructure& Declared,
                                                      SketchCurveName Subject,
                                                      const SpatialPoint& Probe);

Deliver<ProfileNameInFeature> ApplyProfileCorner(SketchStructure& Declared,
                                                 ProfileNameInFeature Subject,
                                                 std::uint32_t LoopIndex,
                                                 std::uint32_t CornerIndex,
                                                 double Radius,
                                                 bool Chamfer);

} // namespace Slate
