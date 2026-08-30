//============================================================================================================================================
//                                                       RECORDDECLARATION.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/RecordDeclaration/Api/RecordDeclaration.h"

namespace Slate
{

WorkspaceRecordName ResolveCategoryFolder(const WorkspaceRecordStructure& Records,
                                          WorkspaceCategory Category)
{
    for (std::uint32_t Index = 1u; Index <= Records.DeclaredCount(); ++Index)
    {
        const WorkspaceRecord* Record = Records.Resolve({ Index });

        // ⚠️ `!ParentFolder.Assigned()` is what confines this to the TOP-LEVEL folder. A nested folder
        //    carrying the same category would otherwise capture records meant for the one at the root.
        if (Record != nullptr && Record->Subject == WorkspaceRecordSubject::Folder &&
            Record->FolderCategory == Category && !Record->ParentFolder.Assigned())
            return { Index };
    }
    return {};
}

WorkspaceRecordName DeclareWorkspaceCurve(WorkspaceNameIndex& Naming,
                                          WorkspaceRecordStructure& Records,
                                          SketchCurveName Curve,
                                          bool Construction)
{
    WorkspaceRecord Record = {};
    Record.Subject      = WorkspaceRecordSubject::OpenCurve;
    Record.ParentFolder = ResolveCategoryFolder(Records, WorkspaceCategory::Sketch);
    Record.Naming       = Construction
                        ? std::string("Construction ") + Naming.Issue(WorkspaceRecordSubject::OpenCurve)
                        : Naming.Issue(WorkspaceRecordSubject::OpenCurve);
    Record.SketchCurve  = Curve;
    Record.ConstructionSemantic = Construction;
    return Records.Declare(Record);
}

WorkspaceRecordName DeclareWorkspaceProfile(WorkspaceNameIndex& Naming,
                                            WorkspaceRecordStructure& Records,
                                            ProfileNameInFeature Profile)
{
    WorkspaceRecord Record = {};
    Record.Subject      = WorkspaceRecordSubject::ClosedProfile;
    Record.ParentFolder = ResolveCategoryFolder(Records, WorkspaceCategory::Sketch);
    Record.Naming       = Naming.Issue(WorkspaceRecordSubject::ClosedProfile);
    Record.Profile      = Profile;
    Record.ClosedSemantic          = true;
    Record.CappedExtrusionSemantic = true;
    return Records.Declare(Record);
}

WorkspaceRecordName DeclareWorkspaceDimension(WorkspaceNameIndex& Naming,
                                              WorkspaceRecordStructure& Records,
                                              DimensionName Dimension)
{
    WorkspaceRecord Record = {};
    Record.Subject      = WorkspaceRecordSubject::Dimension;
    Record.ParentFolder = ResolveCategoryFolder(Records, WorkspaceCategory::Annotation);
    Record.Naming       = Naming.Issue(WorkspaceRecordSubject::Dimension);
    Record.Dimension    = Dimension;
    return Records.Declare(Record);
}

WorkspaceRecordName DeclareWorkspaceConstraint(WorkspaceNameIndex& Naming,
                                               WorkspaceRecordStructure& Records,
                                               ConstraintName Constraint)
{
    WorkspaceRecord Record = {};
    Record.Subject      = WorkspaceRecordSubject::Constraint;
    Record.ParentFolder = ResolveCategoryFolder(Records, WorkspaceCategory::Annotation);
    Record.Naming       = Naming.Issue(WorkspaceRecordSubject::Constraint);
    Record.Constraint   = Constraint;
    return Records.Declare(Record);
}

WorkspaceRecordName DeclareWorkspacePoint(WorkspaceNameIndex& Naming,
                                          WorkspaceRecordStructure& Records,
                                          SketchPointName Point)
{
    WorkspaceRecord Record = {};
    Record.Subject      = WorkspaceRecordSubject::Point;
    Record.ParentFolder = ResolveCategoryFolder(Records, WorkspaceCategory::Sketch);
    Record.Naming       = Naming.Issue(WorkspaceRecordSubject::Point);
    Record.SketchPoint  = Point;
    return Records.Declare(Record);
}

WorkspaceRecordName AutoDeclareWorkspaceProfilesFromChains(WorkspaceNameIndex& Naming,
                                                           SketchStructure& Sketch,
                                                           WorkspaceRecordStructure& Records,
                                                           WorkspaceRevisionSequence& Revisions)
{
    const Deliver<std::vector<ProfileNameInFeature>> Profiles = AutoDeclareClosedAreaProfiles(Sketch, 0.05);
    if (!Profiles.Resolved || Profiles.Resolve().empty())
        return {};

    std::vector<WorkspaceRecordName> Written;
    for (ProfileNameInFeature Profile : Profiles.Resolve())
        Written.push_back(DeclareWorkspaceProfile(Naming, Records, Profile));

    // 🔴 ONE revision for all of them. Closing a rectangle declares a single area and the artist expects a
    //    single undo; sealing one revision per profile would take four presses to walk back one action.
    Revisions.Seal("Declared closed sketch areas", "Auto Create Profiles", Written,
                   Revisions.DeclaredCount() + 1u);

    return Written.empty() ? WorkspaceRecordName{} : Written.front();
}

}   // namespace Slate
