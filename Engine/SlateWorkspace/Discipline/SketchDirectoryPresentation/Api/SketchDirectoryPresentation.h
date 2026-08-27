//============================================================================================================================================
//                                                  SKETCHDIRECTORYPRESENTATION.H
//============================================================================================================================================
// 🧩 Turning the parametric document into what the sketch directory panel shows: the row list, the
//    inspector, the revision history and which row is picked.
//
// 📝 The last behaviour to leave `ParametricSketchHost`. Four functions that were only ever called from
//    `main()` and could only be exercised by launching the application and clicking.

#pragma once

#include "Foundation/DeliveryGuarantee.h"
#include "Application/Api/ParametricWorkspaceBridge.h"
#include "SlateShape/Record/WorkspaceDirectoryProjection/Api/WorkspaceDirectoryProjection.h"
#include "SlateShape/Sketch/SketchStructure/Api/SketchStructure.h"
#include "SlateUI/Interface/ParametricWorkspace/Api/ParametricWorkspaceSpecification.h"
#include "SlateWorkspace/Discipline/ToolAvailability/Api/ToolAvailability.h"

namespace Slate
{

/// 🧩 Empties the inspector, so a stale property panel cannot outlive the record it described.
void ClearInspectorBridge(ParametricWorkspaceBridgeStorage& Bridge);

/// 🧩 What a brand-new parametric workspace starts with, which is nothing.
/// note 📝 Deliberately empty: `ResolveSketchBasis` supplies the ground plane until the artist makes a
///       real record, so seeding geometry here would put something in the document nobody asked for.
void SeedParametricWorkspace(WorkspaceNameIndex& Naming,
                             SketchStructure& Sketch,
                             WorkspaceRecordStructure& Records,
                             WorkspaceRevisionSequence& Revisions);

/// 🧩 Seats the directory's rows into the panel context, and clears the rows beyond them.
/// note 🔴 THE ROWS PAST THE END MUST BE CLEARED, not left. `RowSelected[RowLimit] = { true }` initialises
///       only element zero, and a shrinking directory otherwise leaves the previous run's ticks behind on
///       rows that no longer name anything.
void SeatParametricContext(const WorkspaceDirectoryProjection& Directory,
                           ParametricWorkspaceContext& Applied,
                           bool& Seeded);

/// 🧩 Brings the whole sketch directory presentation up to date after the records change.
/// note 📝 Directory, inspector, revision list and selection in one call, because they are only ever
///       correct together — a directory rebuilt without reseating the context shows rows that select the
///       wrong record.
Deliver<bool> SynchroniseParametricPresentation(const WorkspaceRecordStructure& Records,
                                                const WorkspaceRevisionSequence& Revisions,
                                                WorkspaceDirectoryProjection& Directory,
                                                ParametricWorkspaceBridgeStorage& Bridge,
                                                ParametricWorkspaceContext& Applied,
                                                WorkspaceRecordName& PendingSelection,
                                                bool& Seeded);

}   // namespace Slate
