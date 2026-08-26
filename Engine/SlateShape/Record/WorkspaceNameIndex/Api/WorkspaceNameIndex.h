//============================================================================================================================================
//                                                        WORKSPACENAMEINDEX.H
//============================================================================================================================================
// 🧩 Monotonic default naming for committed parametric workspace records. Auto-generated names advance by
//    subject and never renumber after deletion, so history and references remain readable.

#pragma once

#include <cstdint>
#include <string>

namespace Slate
{

enum class WorkspaceRecordSubject : std::uint32_t;

class WorkspaceNameIndex
{
public:
    std::string Issue(WorkspaceRecordSubject Subject);
    void Reclaim();

private:
    std::uint32_t PointCount = 0u;
    std::uint32_t CurveCount = 0u;
    std::uint32_t ProfileCount = 0u;
    std::uint32_t SurfaceCount = 0u;
    std::uint32_t SolidCount = 0u;
    std::uint32_t DimensionCount = 0u;
    std::uint32_t ConstraintCount = 0u;
    std::uint32_t PatternCount = 0u;
    std::uint32_t MirrorCount = 0u;
    std::uint32_t FolderCount = 0u;
};

} // namespace Slate
