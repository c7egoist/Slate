//============================================================================================================================================
//                                                 REVOLUTIONSPECIFICATION.CPP
//============================================================================================================================================

#include "SlateShape/Operation/RevolutionSpecification/Api/RevolutionSpecification.h"

namespace Slate
{

namespace
{
    double LengthSquared(const SpatialDirection& Direction)
    {
        return Direction.Left * Direction.Left
             + Direction.Up * Direction.Up
             + Direction.Forward * Direction.Forward;
    }
}

bool RevolutionSpecification::Declared() const
{
    return SourceProfile.Assigned()
        && LengthSquared(AxisDirection) > 0.0
        && SweepRadians != 0.0;
}

RevolutionDisposition EvaluateRevolution(const RevolutionSpecification& Declared)
{
    if (!Declared.SourceProfile.Assigned() && Declared.SweepRadians == 0.0)
        return RevolutionDisposition::NotRequested;
    if (!Declared.Declared())
        return RevolutionDisposition::InvalidSpecification;
    return RevolutionDisposition::ImplementationAbsent;
}

} // namespace Slate
