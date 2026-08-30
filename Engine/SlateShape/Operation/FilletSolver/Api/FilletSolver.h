//============================================================================================================================================
//                                                           FILLETSOLVER.H
//============================================================================================================================================

#pragma once

#include <cstdint>
#include <vector>

namespace Slate
{

struct FilletSpecification
{
    std::vector<std::uint32_t> TraversedEdges = {};
    double Radius = 0.0;

    bool Declared() const { return !TraversedEdges.empty() && Radius > 0.0; }
};

} // namespace Slate
