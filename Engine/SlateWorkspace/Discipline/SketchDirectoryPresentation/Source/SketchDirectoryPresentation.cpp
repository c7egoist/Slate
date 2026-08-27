//============================================================================================================================================
//                                                 SKETCHDIRECTORYPRESENTATION.CPP
//============================================================================================================================================

#include "SlateWorkspace/Discipline/SketchDirectoryPresentation/Api/SketchDirectoryPresentation.h"

#include <algorithm>
#include <string>

namespace Slate
{

void ClearInspectorBridge(ParametricWorkspaceBridgeStorage& Bridge)
{
    Bridge.Property = ParametricPropertyPresentation{};
    Bridge.PropertyNaming.clear();
    Bridge.PropertySecondary.clear();
    for (std::string& Run : Bridge.PropertyCaptions) Run.clear();
    for (std::string& Run : Bridge.PropertyValues)   Run.clear();
    for (std::string& Run : Bridge.PropertyTrails)   Run.clear();
    Bridge.RevisionRows.clear();
    Bridge.RevisionBacking.clear();
}

void SeedParametricWorkspace(WorkspaceNameIndex& Naming,
                             SketchStructure& Sketch,
                             WorkspaceRecordStructure& Records,
                             WorkspaceRevisionSequence& Revisions)
{
    // Parametric sketch starts empty. The default grid/basis is supplied by
    // ResolveSketchBasis until the artist creates real CAD records.
    static_cast<void>(Naming);
    static_cast<void>(Sketch);
    static_cast<void>(Records);
    static_cast<void>(Revisions);
}

void SeatParametricContext(const WorkspaceDirectoryProjection& Directory,
                           ParametricWorkspaceContext& Applied,
                           bool& Seeded)
{
    const std::uint32_t RowCount = std::min<std::uint32_t>(
        static_cast<std::uint32_t>(Directory.Rows.size()), ParametricWorkspaceContext::RowLimit);

    for (std::uint32_t Index = RowCount; Index < ParametricWorkspaceContext::RowLimit; ++Index)
    {
        Applied.RowExpanded[Index] = false;
        Applied.RowSelected[Index] = false;
    }

    if (RowCount == 0u)
    {
        Applied.RowTaken = 0u;
        Applied.RowSelectionAnchor = 0u;
        return;
    }

    if (!Seeded)
    {
        for (std::uint32_t Index = 0u; Index < ParametricWorkspaceContext::RowLimit; ++Index)
            Applied.RowSelected[Index] = false;

        for (std::uint32_t Index = 0u; Index < RowCount; ++Index)
            if (Directory.Rows[Index].Subject == WorkspaceRecordSubject::Folder)
                Applied.RowExpanded[Index] = true;

        const std::uint32_t Initial = InitialRowIn(Directory);
        Applied.RowTaken = Initial;
        Applied.RowSelectionAnchor = Initial;
        Applied.RowSelected[Initial] = true;
        Seeded = true;
        return;
    }

    if (Applied.RowTaken >= RowCount || !AnyRowSelected(Applied, RowCount))
    {
        for (std::uint32_t Index = 0u; Index < RowCount; ++Index)
            Applied.RowSelected[Index] = false;

        const std::uint32_t Initial = InitialRowIn(Directory);
        Applied.RowTaken = Initial;
        Applied.RowSelectionAnchor = Initial;
        Applied.RowSelected[Initial] = true;
    }
}

Deliver<bool> SynchroniseParametricPresentation(const WorkspaceRecordStructure& Records,
                                                const WorkspaceRevisionSequence& Revisions,
                                                WorkspaceDirectoryProjection& Directory,
                                                ParametricWorkspaceBridgeStorage& Bridge,
                                                ParametricWorkspaceContext& Applied,
                                                WorkspaceRecordName& PendingSelection,
                                                bool& Seeded)
{
    ProjectWorkspaceDirectory(Records, Directory);

    const Deliver<bool> DirectoryBridge = BridgeParametricDirectory(Directory, Bridge);
    if (!DirectoryBridge.Resolved)
        return DirectoryBridge;

    SeatParametricContext(Directory, Applied, Seeded);

    if (PendingSelection.Assigned())
    {
        const Deliver<std::uint32_t> Row = ResolveWorkspaceDirectoryRow(Directory, PendingSelection);
        if (Row.Resolved && Row.Resolve() < ParametricWorkspaceContext::RowLimit)
        {
            for (std::uint32_t Index = 0u; Index < ParametricWorkspaceContext::RowLimit; ++Index)
                Applied.RowSelected[Index] = false;
            Applied.RowTaken = Row.Resolve();
            Applied.RowSelectionAnchor = Row.Resolve();
            Applied.RowSelected[Row.Resolve()] = true;
        }
        PendingSelection = {};
    }

    const std::uint32_t RowCount = static_cast<std::uint32_t>(Directory.Rows.size());
    if (Applied.RowTaken >= RowCount || Directory.Rows.empty() ||
        Directory.Rows[Applied.RowTaken].Role != WorkspaceDirectoryRowRole::Record)
    {
        ClearInspectorBridge(Bridge);
        return Deliver<bool>::Result(true);
    }

    const WorkspaceRecordName Selected = Directory.Rows[Applied.RowTaken].Record;
    const Deliver<WorkspacePropertyProjection> Property =
        ProjectWorkspaceProperty(Records, Revisions, Selected);
    if (!Property.Resolved)
        return Deliver<bool>::Refuse(Property.Error);

    return BridgeParametricInspector(Records, Selected, Property.Resolve(), Revisions, Bridge);
}

}   // namespace Slate
