//============================================================================================================================================
//                                                   SKETCHSCENEDIRECTORYBRIDGE.H
//============================================================================================================================================
// 🧩 Host-side bridge from decoded workspace Codex scene entries to the shared Scene Directory rows.

#pragma once

#include "SlateDocument/Format/CodexInterchange/Api/WorkspaceCodex.h"
#include "SlateShape/Record/WorkspaceRecordStructure/Api/WorkspaceRecordStructure.h"
#include "SlateUI/Interface/SceneDirectoryPanel/Api/SceneDirectoryPanel.h"

#include <algorithm>
#include <cstdio>

namespace Slate
{

struct SketchSceneDirectoryStorage
{
    EntityRow Rows[SceneDirectoryContext::EntityLimit] = {};
    char Names[SceneDirectoryContext::EntityLimit][96] = {};
    char Tags[SceneDirectoryContext::EntityLimit][160] = {};
    std::uint32_t RowCount = 0u;
};

inline EntitySubject SketchSceneSubjectOf(CodexSceneSubject Subject)
{
    switch (Subject)
    {
        case CodexSceneSubject::Sun:        return EntitySubject::Sun;
        case CodexSceneSubject::Sky:        return EntitySubject::Sky;
        case CodexSceneSubject::Atmosphere: return EntitySubject::Actor;
        case CodexSceneSubject::Geometry:   return EntitySubject::Actor;
        default:                            return EntitySubject::Actor;
    }
}

inline const char* SketchSceneTagOf(const CodexSceneEntry& Entry)
{
    switch (Entry.Subject)
    {
        case CodexSceneSubject::Sun:        return "codex reference sun light";
        case CodexSceneSubject::Sky:        return "codex reference sky atmosphere";
        case CodexSceneSubject::Atmosphere: return "codex reference atmosphere settings";
        case CodexSceneSubject::Geometry:   return Entry.MaterialReference.c_str();
        default:                            return "codex reference";
    }
}

inline void SketchSceneAppendRow(SketchSceneDirectoryStorage& Storage,
                                 const char* Naming,
                                 EntitySubject Subject,
                                 std::uint32_t Depth,
                                 std::uint32_t Enclosing,
                                 std::uint32_t EnclosedCount,
                                 const char* Tagged,
                                 StableRowIdentity Identity)
{
    if (Storage.RowCount >= SceneDirectoryContext::EntityLimit)
        return;

    const std::uint32_t RowIndex = Storage.RowCount++;
    std::snprintf(Storage.Names[RowIndex], sizeof(Storage.Names[RowIndex]), "%s", Naming != nullptr ? Naming : "");
    std::snprintf(Storage.Tags[RowIndex], sizeof(Storage.Tags[RowIndex]), "%s", Tagged != nullptr ? Tagged : "");

    EntityRow& Row = Storage.Rows[RowIndex];
    Row = {};
    Row.Naming = Storage.Names[RowIndex];
    Row.Subject = Subject;
    Row.Depth = Depth;
    Row.Enclosing = Enclosing;
    Row.EnclosedCount = EnclosedCount;
    Row.Tagged = Storage.Tags[RowIndex];
    Row.Camera = CameraRole::Absent;
    Row.Identity = Identity;
}

inline std::uint32_t BridgeSketchSceneDirectory(const WorkspaceCodex& Workspace,
                                                SketchSceneDirectoryStorage& Storage)
{
    Storage = SketchSceneDirectoryStorage{};

    std::uint32_t LightingCount = 0u;
    std::uint32_t GeometryCount = 0u;
    for (const CodexSceneEntry& Entry : Workspace.Scene)
    {
        if (Entry.Subject == CodexSceneSubject::Geometry)
            ++GeometryCount;
        else
            ++LightingCount;
    }

    SketchSceneAppendRow(Storage,
                         Workspace.Naming.empty() ? "Workspace Scene" : Workspace.Naming.c_str(),
                         EntitySubject::Level, 0u, 0xFFFFFFFFu,
                         (LightingCount > 0u ? 1u : 0u) + (GeometryCount > 0u ? 1u : 0u),
                         "workspace codex scene", 6000u);

    std::uint32_t LightingRow = 0xFFFFFFFFu;
    if (LightingCount > 0u)
    {
        LightingRow = Storage.RowCount;
        SketchSceneAppendRow(Storage, "Lighting", EntitySubject::Grouping, 1u, 0u,
                             LightingCount, "codex lighting folder", 6001u);
        for (std::uint32_t Index = 0u; Index < Workspace.Scene.size(); ++Index)
        {
            const CodexSceneEntry& Entry = Workspace.Scene[Index];
            if (Entry.Subject == CodexSceneSubject::Geometry)
                continue;
            SketchSceneAppendRow(Storage, Entry.Naming.c_str(), SketchSceneSubjectOf(Entry.Subject), 2u,
                                 LightingRow, 0u, SketchSceneTagOf(Entry), 6100u + Index);
        }
    }

    std::uint32_t ObjectsRow = 0xFFFFFFFFu;
    if (GeometryCount > 0u)
    {
        ObjectsRow = Storage.RowCount;
        SketchSceneAppendRow(Storage, "Scene Objects", EntitySubject::Grouping, 1u, 0u,
                             GeometryCount, "codex geometry folder", 6002u);
        for (std::uint32_t Index = 0u; Index < Workspace.Scene.size(); ++Index)
        {
            const CodexSceneEntry& Entry = Workspace.Scene[Index];
            if (Entry.Subject != CodexSceneSubject::Geometry)
                continue;
            SketchSceneAppendRow(Storage, Entry.Naming.c_str(), EntitySubject::Actor, 2u,
                                 ObjectsRow, 0u, SketchSceneTagOf(Entry), 6200u + Index);
        }
    }

    return Storage.RowCount;
}

inline bool SketchSceneCadReferenced(WorkspaceRecordSubject Subject)
{
    return Subject == WorkspaceRecordSubject::ClosedProfile ||
           Subject == WorkspaceRecordSubject::ThinSurface ||
           Subject == WorkspaceRecordSubject::Solid;
}

inline void AppendSketchCadReferences(const WorkspaceRecordStructure& Records,
                                      SketchSceneDirectoryStorage& Storage)
{
    std::uint32_t ReferenceCount = 0u;
    for (std::uint32_t Index = 1u; Index <= Records.DeclaredCount(); ++Index)
    {
        const WorkspaceRecord* Record = Records.Resolve({ Index });
        if (Record != nullptr && Record->Visible && SketchSceneCadReferenced(Record->Subject))
            ++ReferenceCount;
    }
    if (ReferenceCount == 0u)
        return;

    if (Storage.RowCount == 0u)
    {
        SketchSceneAppendRow(Storage, "Sketch Scene", EntitySubject::Level, 0u, 0xFFFFFFFFu,
                             0u, "sketch scene", 6000u);
    }

    Storage.Rows[0u].EnclosedCount += 1u;
    const std::uint32_t Folder = Storage.RowCount;
    SketchSceneAppendRow(Storage, "CAD References", EntitySubject::Grouping, 1u, 0u,
                         ReferenceCount, "cad references folder", 6003u);

    for (std::uint32_t Index = 1u; Index <= Records.DeclaredCount(); ++Index)
    {
        const WorkspaceRecord* Record = Records.Resolve({ Index });
        if (Record == nullptr || !Record->Visible || !SketchSceneCadReferenced(Record->Subject))
            continue;

        const char* Tag = Record->Subject == WorkspaceRecordSubject::ClosedProfile ? "CAD reference closed profile" :
                          Record->Subject == WorkspaceRecordSubject::ThinSurface ? "CAD reference thin surface" :
                                                                                   "CAD reference solid";
        SketchSceneAppendRow(Storage, Record->Naming.c_str(), EntitySubject::Actor, 2u, Folder,
                             0u, Tag, 7000u + Index);
    }
}

inline void ApplySketchSceneEnvironment(const WorkspaceCodex& Workspace,
                                        SceneDirectoryContext& Applied)
{
    Applied.Environment.SunElevation = Workspace.Environment.SunElevation;
    Applied.Environment.SunAzimuth = Workspace.Environment.SunAzimuth;
    Applied.Environment.SunIntensity = Workspace.Environment.SunIntensity;
    Applied.Environment.SunTemperature = Workspace.Environment.SunTemperature;
    Applied.Environment.SkyIntensity = Workspace.Environment.SkyIntensity;
    Applied.Environment.AtmosphereDensity = Workspace.Environment.AtmosphereDensity;
    Applied.Environment.AtmosphereScaleHeight = Workspace.Environment.AtmosphereScaleHeight;
}

} // namespace Slate
