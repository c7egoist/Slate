//============================================================================================================================================
//                                                            DRAFTSOLVER.H
//============================================================================================================================================

#pragma once

#include <cstdint>

namespace Slate
{

struct DraftSpecification
{
    std::uint32_t SourceFace = 0u;
    double AngleRadians = 0.0;

    bool Declared() const { return SourceFace != 0u && AngleRadians != 0.0; }
};

} // namespace Slate
