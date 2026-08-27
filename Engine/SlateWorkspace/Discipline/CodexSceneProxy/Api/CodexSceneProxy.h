//============================================================================================================================================
//                                                         CODEXSCENEPROXY.H
//============================================================================================================================================
// 🧩 Showing an imported scene inside a sketch: the directory rows it becomes, the box drawn in its place,
//    and picking one of those boxes with the pointer.
//
// 🔴 ALL THREE HOSTS DREW CODEX SCENE PROXIES, AND ALL THREE PROJECTED THE POINTS THEMSELVES.
//    `ParametricSketchHost` had `RecordCodexSceneProxy`, `EditorHost` has `RecordWorkspaceCodexProxy` and
//    `ProjectWorkspaceCodexPoint`, `PaintHost` has `ProjectPaintScenePoint`. Three copies of one idea, each
//    with its own projection, which is how they came to disagree about where a box belongs.
//
// 🔴 THE OTHER HALF CAME OUT OF `Application/Api/SketchSceneDirectoryBridge.h`, a header two hosts included
//    and nothing else could reach. `Bridge` named the file rather than the behaviour; what it actually does
//    is turn a codex into directory rows. That header is deleted at step 11.
//
// 📝 A proxy is a STAND-IN, not the mesh. The box states where an imported thing is and how big it is so
//    the artist can sketch against it; the mesh itself is drawn by the renderer.

#pragma once

#include "SlateDocument/Format/CodexInterchange/Api/WorkspaceCodex.h"
#include "SlateShape/Record/WorkspaceRecordStructure/Api/WorkspaceRecordStructure.h"
#include "SlateUI/Interface/InterfaceExchange/Api/RecordingSurface.h"
#include "SlateUI/Interface/SceneDirectoryPanel/Api/SceneDirectoryPanel.h"
#include "SlateWorkspace/Discipline/ViewportProjection/Api/ViewportProjection.h"

namespace Slate
{

//------------------------------------------------------------------------------------------------------------------------
//                                            THE ROWS A CODEX BECOMES
//------------------------------------------------------------------------------------------------------------------------

/// 🧩 The directory rows an imported scene is shown as, with their names and tags held alongside.
/// note ⚠️ Fixed capacity, matching the panel's own `EntityLimit`. The names and tags are held here rather
///       than in the rows because the panel's row type keeps pointers into them.
struct SceneDirectoryRows
{
    EntityRow Rows[SceneDirectoryContext::EntityLimit] = {};
    char Names[SceneDirectoryContext::EntityLimit][96] = {};
    char Tags[SceneDirectoryContext::EntityLimit][160] = {};
    std::uint32_t RowCount = 0u;
};

//------------------------------------------------------------------------------------------------------------------------
//                                          BUILDING THEM, AND DRAWING THEM
//------------------------------------------------------------------------------------------------------------------------

EntitySubject SceneEntrySubject(CodexSceneSubject Subject);

const char* SceneEntryTag(const CodexSceneEntry& Entry);

void AppendSceneDirectoryRow(SceneDirectoryRows& Storage,
                             const char* Naming,
                             EntitySubject Subject,
                             std::uint32_t Depth,
                             std::uint32_t Enclosing,
                             std::uint32_t EnclosedCount,
                             const char* Tagged,
                             StableRowIdentity Identity);

std::uint32_t BuildSceneDirectoryRows(const WorkspaceCodex& Workspace,
                                      SceneDirectoryRows& Storage);

bool CadReferenced(WorkspaceRecordSubject Subject);

void AppendCadReferenceRows(const WorkspaceRecordStructure& Records,
                            SceneDirectoryRows& Storage);

void ApplySceneEnvironment(const WorkspaceCodex& Workspace,
                           SceneDirectoryContext& Applied);

SpatialPoint CodexScenePosition(const CodexSceneEntry& Entry);

void ResolveCodexProxyExtent(const CodexSceneEntry& Entry,
                             double& HalfX,
                             double& HalfY,
                             double& HalfZ);

bool ResolveSelectedSceneMeshPivot(const WorkspaceCodex& Scene,
                                   bool SceneStanding,
                                   const SceneDirectoryRows& Storage,
                                   const SceneDirectoryContext& Applied,
                                   SpatialPoint& Pivot);

bool SelectSceneMeshAtPointer(const PlaneExtent& Extent,
                              const PointerCondition& Pointer,
                              const SpatialBasis& Basis,
                              const ViewportStanding& View,
                              bool Perspective,
                              const WorkspaceCodex& Scene,
                              bool SceneStanding,
                              const SceneDirectoryRows& Storage,
                              SceneDirectoryContext& Applied);

ThemeToken CodexMaterialToken(const WorkspaceCodex& Scene,
                              const CodexSceneEntry& Entry,
                              double Alpha,
                              std::uint32_t Fallback);

void RecordCodexSceneProxy(RecordingSurface& Surface,
                           const PlaneExtent& Extent,
                           const SpatialBasis& Basis,
                           const ViewportStanding& View,
                           bool Perspective,
                           const WorkspaceCodex& Scene,
                           bool SceneStanding,
                           const SceneDirectoryRows& Storage,
                           const SceneDirectoryContext& Applied);

void SeedSceneDirectoryTransformsFromCodex(const WorkspaceCodex& Scene,
                                           const SceneDirectoryRows& Storage,
                                           SceneDirectoryContext& Applied);

void SynchroniseCodexTransformsFromSceneDirectory(WorkspaceCodex& Scene,
                                                  const SceneDirectoryRows& Storage,
                                                  const SceneDirectoryContext& Applied,
                                                  bool SceneStanding);

}   // namespace Slate
