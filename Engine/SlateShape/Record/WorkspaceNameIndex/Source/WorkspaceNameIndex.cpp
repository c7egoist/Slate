//============================================================================================================================================
//                                                      WORKSPACENAMEINDEX.CPP
//============================================================================================================================================

#include "SlateShape/Record/WorkspaceNameIndex/Api/WorkspaceNameIndex.h"
#include "SlateShape/Record/WorkspaceRecordStructure/Api/WorkspaceRecordStructure.h"

namespace Slate
{

std::string WorkspaceNameIndex::Issue(WorkspaceRecordSubject Subject)
{
    switch (Subject)
    {
        case WorkspaceRecordSubject::Point:         return "Point_" + std::to_string(++PointCount);
        case WorkspaceRecordSubject::OpenCurve:     return "Line_" + std::to_string(++CurveCount);
        case WorkspaceRecordSubject::ClosedProfile: return "Profile_" + std::to_string(++ProfileCount);
        case WorkspaceRecordSubject::ThinSurface:   return "ThinSurface_" + std::to_string(++SurfaceCount);
        case WorkspaceRecordSubject::Solid:         return "Solid_" + std::to_string(++SolidCount);
        case WorkspaceRecordSubject::Dimension:     return "Dimension_" + std::to_string(++DimensionCount);
        case WorkspaceRecordSubject::Constraint:    return "Constraint_" + std::to_string(++ConstraintCount);
        case WorkspaceRecordSubject::Pattern:       return "Pattern_" + std::to_string(++PatternCount);
        case WorkspaceRecordSubject::Mirror:        return "Mirror_" + std::to_string(++MirrorCount);
        case WorkspaceRecordSubject::Folder:        return "Folder_" + std::to_string(++FolderCount);
        case WorkspaceRecordSubject::SubjectCount:  return "Unnamed_0";
    }
    return "Unnamed_0";
}

void WorkspaceNameIndex::Reclaim()
{
    PointCount = 0u;
    CurveCount = 0u;
    ProfileCount = 0u;
    SurfaceCount = 0u;
    SolidCount = 0u;
    DimensionCount = 0u;
    ConstraintCount = 0u;
    PatternCount = 0u;
    MirrorCount = 0u;
    FolderCount = 0u;
}

} // namespace Slate
