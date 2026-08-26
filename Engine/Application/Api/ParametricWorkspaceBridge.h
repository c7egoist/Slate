//============================================================================================================================================
//                                                     PARAMETRICWORKSPACEBRIDGE.H
//============================================================================================================================================
// 🧩 Host-side bridge from SlateFeature's exact CAD workspace projections to SlateUI's parametric-workspace
//    presentation guarantee. The bridge lives in Application on purpose: the host is the only layer allowed to
//    see both the exact backend and the UI guarantee at once.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "SlateFeature/Feature/WorkspaceDirectoryProjection/Api/WorkspaceDirectoryProjection.h"
#include "SlateFeature/Feature/WorkspacePropertyProjection/Api/WorkspacePropertyProjection.h"
#include "SlateFeature/Feature/WorkspaceRevisionSequence/Api/WorkspaceRevisionSequence.h"
#include "SlateUI/Interface/ParametricWorkspace/Api/ParametricWorkspaceSpecification.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Slate
{

struct ParametricWorkspaceBridgeStorage
{
    struct DirectoryText
    {
        std::string Naming = {};
        std::string Tagged = {};
    };

    struct RevisionText
    {
        std::string Description = {};
        std::string Operation = {};
        std::string Affected = {};
        std::string SealedAt = {};
    };

    std::vector<ParametricDirectoryRow> DirectoryRows = {};
    std::vector<DirectoryText> DirectoryBacking = {};

    ParametricPropertyPresentation Property = {};
    std::string PropertyNaming = {};
    std::string PropertySecondary = {};
    std::array<std::string, ParametricPropertyPresentation::FieldLimit> PropertyCaptions = {};
    std::array<std::string, ParametricPropertyPresentation::FieldLimit> PropertyValues = {};
    std::array<std::string, ParametricPropertyPresentation::FieldLimit> PropertyTrails = {};

    std::vector<ParametricRevisionRow> RevisionRows = {};
    std::vector<RevisionText> RevisionBacking = {};

    void Reclaim()
    {
        DirectoryRows.clear();
        DirectoryBacking.clear();
        Property = ParametricPropertyPresentation{};
        PropertyNaming.clear();
        PropertySecondary.clear();
        for (std::string& Run : PropertyCaptions) Run.clear();
        for (std::string& Run : PropertyValues)   Run.clear();
        for (std::string& Run : PropertyTrails)   Run.clear();
        RevisionRows.clear();
        RevisionBacking.clear();
    }
};

inline ParametricCategory BridgeParametricCategory(WorkspaceCategory Category)
{
    switch (Category)
    {
        case WorkspaceCategory::Sketch:      return ParametricCategory::Sketch;
        case WorkspaceCategory::Geometry:    return ParametricCategory::Geometry;
        case WorkspaceCategory::Annotation:  return ParametricCategory::Annotation;
        case WorkspaceCategory::Operation:   return ParametricCategory::Operation;
        case WorkspaceCategory::Folder:
        case WorkspaceCategory::CategoryCount:
            return ParametricCategory::Sketch;
    }
    return ParametricCategory::Sketch;
}

inline ParametricRowSubject BridgeParametricRowSubject(const WorkspaceDirectoryRow& Source)
{
    if (Source.Role == WorkspaceDirectoryRowRole::CategoryRoot)
        return ParametricRowSubject::CategoryRoot;

    switch (Source.Subject)
    {
        case WorkspaceRecordSubject::Folder:        return ParametricRowSubject::Folder;
        case WorkspaceRecordSubject::Point:         return ParametricRowSubject::Point;
        case WorkspaceRecordSubject::OpenCurve:     return ParametricRowSubject::OpenCurve;
        case WorkspaceRecordSubject::ClosedProfile: return ParametricRowSubject::ClosedProfile;
        case WorkspaceRecordSubject::ThinSurface:   return ParametricRowSubject::ThinSurface;
        case WorkspaceRecordSubject::Solid:         return ParametricRowSubject::Solid;
        case WorkspaceRecordSubject::Dimension:     return ParametricRowSubject::Dimension;
        case WorkspaceRecordSubject::Constraint:    return ParametricRowSubject::Constraint;
        case WorkspaceRecordSubject::Pattern:       return ParametricRowSubject::Pattern;
        case WorkspaceRecordSubject::Mirror:        return ParametricRowSubject::Mirror;
        case WorkspaceRecordSubject::SubjectCount:
            return ParametricRowSubject::Folder;
    }
    return ParametricRowSubject::Folder;
}

inline const char* BridgeAffirmation(bool Reading)
{
    return Reading ? "Yes" : "No";
}

inline std::string BridgeIdentityRun(const char* Stem, std::uint32_t Index)
{
    return std::string(Stem) + "_" + std::to_string(Index);
}

inline std::string BridgeRecordDisplay(const WorkspaceRecordStructure& Records, WorkspaceRecordName Subject)
{
    if (!Subject.Assigned())
        return "Root";

    const WorkspaceRecord* Held = Records.Resolve(Subject);
    if (Held != nullptr && !Held->Naming.empty())
        return Held->Naming;

    return BridgeIdentityRun("Record", Subject.IssuedIndex);
}

inline std::string BridgeAffectedRun(const WorkspaceRecordStructure& Records,
                                     const std::vector<WorkspaceRecordName>& Affected)
{
    if (Affected.empty())
        return "None";

    std::string Joined = {};
    for (std::uint32_t Index = 0u; Index < Affected.size(); ++Index)
    {
        if (Index > 0u)
            Joined += ", ";
        Joined += BridgeRecordDisplay(Records, Affected[Index]);
    }
    return Joined;
}

inline std::string BridgeSubjectReference(const WorkspaceRecord& Subject)
{
    switch (Subject.Subject)
    {
        case WorkspaceRecordSubject::Point:
            return Subject.SketchPoint.Assigned()
                 ? BridgeIdentityRun("SketchPoint", Subject.SketchPoint.IssuedIndex) : "";
        case WorkspaceRecordSubject::OpenCurve:
            return Subject.SketchCurve.Assigned()
                 ? BridgeIdentityRun("SketchCurve", Subject.SketchCurve.IssuedIndex) : "";
        case WorkspaceRecordSubject::ClosedProfile:
        case WorkspaceRecordSubject::ThinSurface:
            return Subject.Profile.Assigned()
                 ? BridgeIdentityRun("Profile", Subject.Profile.IssuedIndex) : "";
        case WorkspaceRecordSubject::Solid:
            return Subject.Solid.Assigned()
                 ? BridgeIdentityRun("Solid", Subject.Solid.IssuedIndex) : "";
        case WorkspaceRecordSubject::Dimension:
            return Subject.Dimension.Assigned()
                 ? BridgeIdentityRun("Dimension", Subject.Dimension.IssuedIndex) : "";
        case WorkspaceRecordSubject::Constraint:
            return Subject.Constraint.Assigned()
                 ? BridgeIdentityRun("Constraint", Subject.Constraint.IssuedIndex) : "";
        case WorkspaceRecordSubject::Pattern:
        case WorkspaceRecordSubject::Mirror:
            return Subject.Feature.Assigned()
                 ? BridgeIdentityRun("Feature", Subject.Feature.IssuedIndex) : "";
        case WorkspaceRecordSubject::Folder:
        case WorkspaceRecordSubject::SubjectCount:
            return "";
    }
    return "";
}

inline Deliver<bool> BridgeParametricDirectory(const WorkspaceDirectoryProjection& Source,
                                               ParametricWorkspaceBridgeStorage& Delivered)
{
    Delivered.DirectoryRows.clear();
    Delivered.DirectoryBacking.clear();

    if (Source.Rows.size() > ParametricWorkspaceContext::RowLimit)
    {
        return Deliver<bool>::Refuse({ RefusalReason::ExtentExhausted,
                                       "the parametric workspace row limit would be exceeded" });
    }

    Delivered.DirectoryRows.reserve(Source.Rows.size());
    Delivered.DirectoryBacking.reserve(Source.Rows.size());

    for (const WorkspaceDirectoryRow& Row : Source.Rows)
    {
        Delivered.DirectoryBacking.push_back({ Row.Naming != nullptr ? Row.Naming : "",
                                               Row.Tagged != nullptr ? Row.Tagged : "" });
        const ParametricWorkspaceBridgeStorage::DirectoryText& Backing = Delivered.DirectoryBacking.back();

        ParametricDirectoryRow Presented = {};
        Presented.Naming         = Backing.Naming.c_str();
        Presented.Subject        = BridgeParametricRowSubject(Row);
        Presented.Category       = BridgeParametricCategory(Row.Category);
        Presented.Depth          = Row.Depth;
        Presented.Enclosing      = Row.Enclosing;
        Presented.EnclosedCount  = Row.EnclosedCount;
        Presented.Tagged         = Backing.Tagged.c_str();
        Presented.Identity       = Row.StableIdentity;
        Presented.Visible        = Row.Visible;
        Presented.Locked         = Row.Locked;
        Presented.ClosedSemantic = Row.ClosedSemantic;
        Presented.AutoNamed      = Row.AutoNamed;

        Delivered.DirectoryRows.push_back(Presented);
    }

    return Deliver<bool>::Result(true);
}

inline Deliver<bool> BridgeParametricInspector(const WorkspaceRecordStructure& Records,
                                               WorkspaceRecordName SubjectName,
                                               const WorkspacePropertyProjection& Source,
                                               const WorkspaceRevisionSequence& Revisions,
                                               ParametricWorkspaceBridgeStorage& Delivered)
{
    Delivered.Property = ParametricPropertyPresentation{};
    Delivered.PropertyNaming.clear();
    Delivered.PropertySecondary.clear();
    for (std::string& Run : Delivered.PropertyCaptions) Run.clear();
    for (std::string& Run : Delivered.PropertyValues)   Run.clear();
    for (std::string& Run : Delivered.PropertyTrails)   Run.clear();
    Delivered.RevisionRows.clear();
    Delivered.RevisionBacking.clear();

    if (!SubjectName.Assigned())
    {
        return Deliver<bool>::Refuse({ RefusalReason::ContentUnsupported,
                                       "the selected workspace record is not assigned" });
    }

    if (Source.Subject == nullptr)
    {
        return Deliver<bool>::Refuse({ RefusalReason::ContentUnsupported,
                                       "the selected workspace record is absent from the property projection" });
    }

    if (Source.RevisionSet.size() > ParametricWorkspaceContext::RevisionLimit)
    {
        return Deliver<bool>::Refuse({ RefusalReason::ExtentExhausted,
                                       "the parametric revision row limit would be exceeded" });
    }

    const WorkspaceRecord& Held = *Source.Subject;
    const ParametricCategory Category = BridgeParametricCategory(PresentedCategoryOfRecord(Held));
    const ParametricRowSubject Subject = BridgeParametricRowSubject(
        WorkspaceDirectoryRow{ WorkspaceDirectoryRowRole::Record, PresentedCategoryOfRecord(Held), SubjectName,
                               Held.Subject, 0u, 0xFFFFFFFFu, 0u, 0u, nullptr, nullptr,
                               Held.Visible, Held.Locked, Held.ClosedSemantic, Held.AutoNamed });

    Delivered.PropertyNaming = Held.Naming;
    Delivered.PropertySecondary = std::string(ParametricCategoryText(Category))
                                + " • " + ParametricRowText(Subject);

    Delivered.Property.Naming         = Delivered.PropertyNaming.c_str();
    Delivered.Property.Secondary      = Delivered.PropertySecondary.c_str();
    Delivered.Property.Subject        = Subject;
    Delivered.Property.Category       = Category;
    Delivered.Property.Identity       = static_cast<StableRowIdentity>(SubjectName.IssuedIndex);
    Delivered.Property.Visible        = Held.Visible;
    Delivered.Property.Locked         = Held.Locked;
    Delivered.Property.ClosedSemantic = Held.ClosedSemantic;
    Delivered.Property.CappedExtrusionSemantic = Held.CappedExtrusionSemantic;
    Delivered.Property.AutoNamed      = Held.AutoNamed;

    std::uint32_t Field = 0u;
    auto Append = [&](const char* Caption, const std::string& Value, const std::string& Trail = std::string{})
    {
        if (Field >= ParametricPropertyPresentation::FieldLimit)
            return;
        Delivered.PropertyCaptions[Field] = Caption != nullptr ? Caption : "";
        Delivered.PropertyValues[Field] = Value;
        Delivered.PropertyTrails[Field] = Trail;
        Delivered.Property.Fields[Field].Caption = Delivered.PropertyCaptions[Field].c_str();
        Delivered.Property.Fields[Field].Value = Delivered.PropertyValues[Field].c_str();
        Delivered.Property.Fields[Field].Secondary = Delivered.PropertyTrails[Field].c_str();
        ++Field;
    };

    Append("Record", BridgeIdentityRun("Record", SubjectName.IssuedIndex));
    Append("Category", ParametricCategoryText(Category));
    Append("Subject", ParametricRowText(Subject));
    Append("Visible", BridgeAffirmation(Held.Visible));
    Append("Locked", BridgeAffirmation(Held.Locked));
    Append("Auto Name", BridgeAffirmation(Held.AutoNamed));
    if (Held.ClosedSemantic)
    {
        Append("Curve Closure", "Closed loop", "outline only");
        Append("Extrude Result", Held.CappedExtrusionSemantic ? "Solid" : "Walls only",
               Held.CappedExtrusionSemantic ? "top/bottom caps" : "no caps");
        Append("Extrude Caps", Held.CappedExtrusionSemantic ? "On" : "Off", "click toggle");
    }
    Append("Parent Folder", BridgeRecordDisplay(Records, Held.ParentFolder));

    const std::string Reference = BridgeSubjectReference(Held);
    if (!Reference.empty())
        Append("Reference", Reference);

    if (Held.Feature.Assigned() &&
        Held.Subject != WorkspaceRecordSubject::Pattern &&
        Held.Subject != WorkspaceRecordSubject::Mirror)
    {
        Append("Feature", BridgeIdentityRun("Feature", Held.Feature.IssuedIndex));
    }

    Delivered.Property.FieldCount = Field;

    Delivered.RevisionRows.reserve(Source.RevisionSet.size());
    Delivered.RevisionBacking.reserve(Source.RevisionSet.size());

    for (WorkspaceRevisionName RevisionName : Source.RevisionSet)
    {
        const WorkspaceRevision* Revision = Revisions.Resolve(RevisionName);
        if (Revision == nullptr)
        {
            return Deliver<bool>::Refuse({ RefusalReason::IdentityStale,
                                           "a projected workspace revision is no longer declared" });
        }

        Delivered.RevisionBacking.push_back({ Revision->Description,
                                              Revision->Operation,
                                              BridgeAffectedRun(Records, Revision->Affected),
                                              std::string("Step ") + std::to_string(Revision->SealedAt) });
        const ParametricWorkspaceBridgeStorage::RevisionText& Backing = Delivered.RevisionBacking.back();

        ParametricRevisionRow Presented = {};
        Presented.Description = Backing.Description.c_str();
        Presented.Operation   = Backing.Operation.c_str();
        Presented.Affected    = Backing.Affected.c_str();
        Presented.SealedAt    = Backing.SealedAt.c_str();
        Presented.Identity    = static_cast<StableRowIdentity>(RevisionName.IssuedIndex);

        Delivered.RevisionRows.push_back(Presented);
    }

    return Deliver<bool>::Result(true);
}

} // namespace Slate
