//============================================================================================================================================
//                                                          CHAMFERSOLVER.H
//============================================================================================================================================

#pragma once

#include <cstdint>
#include <vector>

namespace Slate
{

struct ChamferSpecification
{
    std::vector<std::uint32_t> TraversedEdges = {};
    double Distance = 0.0;

    bool Declared() const { return !TraversedEdges.empty() && Distance > 0.0; }
};

} // namespace Slate
