# Naming and Placement Audit

**Status:** implemented on `arena/01a02e51-slate` after the audit was approved  
**Implementation validation:** naming gate clean; dependency partition clean; all 140 Engine translation units and both proof tools accepted by the Linux syntax build  
**Audit date:** 2026-08-23  
**Baseline:** working branch `arena/01a02e51-slate` at `4cc2d72`, containing the latest `master` commit `3eb240e` as its base  
**Scope:** first-party files under `Engine/`; vendor packages are excluded

## Implementation note

The approved migration has now been applied. The camera foundation was made even narrower than the proposed dependency sketch: `SlateScene` consumes only the neutral `Foundation/CameraFoundation.h` and therefore declares no engine-unit dependency. The combined GPU pass took the documented lower-risk route and is now `WorkspaceOverlayPass`; its generated shader stems are `WorkspaceOverlayVertex` and `WorkspaceOverlayFragment`, and its former numeric mode interface is a named `WorkspaceOverlayDraw` guarantee.

## Executive decision

The audit confirms that `Ceiling`, `Ordinal`, and `Choice` are broad naming families rather than isolated mistakes. They should be retired with semantic replacements, not a blind global substitution:

- **`Ceiling`** usually means a count or safety maximum and should normally become **`Limit`**. Use **`Extent`** only for a measured size. Use `MaximumX`, `MaximumY`, `ByteBudget`, `DepthLimit`, and similar precise names where those are the actual meanings.
- **`Ordinal`** overwhelmingly means a zero-based array/vector position and should normally become **`Index`**. Use `Number`, `Rank`, `Order`, or `SequenceNumber` only where the value has that different meaning.
- **`Choice`** conflates the available options with the currently selected value. Use **`Options`** for candidates and **`Selection`** for the selected result.
- Plain **`Construct`** methods conceal what is being established. Rename them by result: `ConstructPanel`, `ConstructPass`, `ConstructIndex`, `ConstructSurface`, `ConstructPartition`, `DeriveFrustum`, `ReserveStorage`, and so on.
- **`InteractionIndex`** is not an index of interactions. It is a generational registry of controls plus global pointer capture, popup disclosure, release, and fade state. **`ControlIndex`** is the recommended replacement.
- The reusable **`CameraComponent`** is currently split across a generic-looking Application header and an EditorHost implementation. That prevents reuse at the compiled-library boundary. Move the host-independent camera foundation to a new **`SlateScene` static library**; keep `EditorCameraComponent`, `PlayerCameraComponent`, and `SpectatorCameraComponent` as distinct specialisations.
- The grid is hidden in mode 3 of the generic `OverlayFragment.slang` shader and therefore lowers to the misleading artifact `OverlayFragment.spv`. Give the analytic grid an explicit **`GroundGridPass`**, `GroundGridVertex.slang`, and `GroundGridFragment.slang` identity, or at minimum rename the combined pass to `WorkspaceOverlayPass` and replace numeric modes with named kinds.

## Audit method and counts

Counts below are case-sensitive identifier-substring occurrences in first-party `Engine/` files. “Identifier families” means distinct alphanumeric/underscore spellings containing the term. Comments are included because comments and API prose teach the vocabulary that later code repeats.

| Term | Occurrences | Matching lines | Files | Identifier families |
|---|---:|---:|---:|---:|
| `Ceiling` | 1,015 | 945 | 142 | 117 |
| `Ordinal` | 6,277 | 4,602 | 239 | 174 |
| `Choice` | 72 | 40 | 7 | 5 |
| `Boundary` | 101 | 91 | 12 | 9 |
| `Binding` | 26 | 21 | 3 | 7 |
| `Region` | 14 | 13 | 3 | 3 |
| plain `Construct(` | 206 | — | 105 | 52 declarations/definitions |

The wider repository also contains the words in scripts, instructions, and historical material. Those should not be included in a production rename tally. Vendor-owned API spellings such as `VkDescriptorSetLayoutBinding`, `dstBinding`, `pBindings`, `GetContentRegionAvail`, and PowerShell's `CmdletBinding` cannot be renamed and are not Slate naming violations.

A repeatable search is:

```bash
rg -n 'Ceiling|Ordinal|Choice|Boundary|Binding|Region' Engine \
  --glob '!ExternalPackages/**'
rg -n -F 'Construct(' Engine --glob '!ExternalPackages/**'
```

## 1. Retire `Ceiling`

### Why it is weak

`Ceiling` is being asked to represent at least five different concepts:

1. a fixed count capacity;
2. a configurable safety maximum;
3. an upper coordinate;
4. a byte/memory budget;
5. an enum or loop terminal.

Those meanings are not interchangeable. In particular, **extent is a size, while a ceiling/maximum coordinate is an endpoint**. Replacing every occurrence with `Extent` would preserve the ambiguity and sometimes make the name mathematically false.

### Replacement rule

| Actual meaning | Replacement form | Example |
|---|---|---|
| maximum allowed count | `<Subject>Limit` | `TextureLayerLimit` |
| fixed storage slots | `<Subject>Capacity` | `ControlCapacity` |
| measured width/height/size | `<Subject>Extent` | `DisplayExtentX` |
| upper coordinate | `<Subject>MaximumX/Y` | `NamingMaximumX` |
| byte allowance | `<Subject>ByteBudget` | `SurfaceByteBudget` |
| nesting guard | `<Subject>DepthLimit` | `EnclosureDepthLimit` |
| attempt guard | `AttemptLimit` / `IterationLimit` | `IterationLimit` |
| terminal condition | `LimitReached` | replaces `CeilingReached` |

### High-volume families and recommended names

| Existing family | Recommended name |
|---|---|
| `LayerStackCeiling` | `LayerStackLimit` |
| `TextureLayerCeiling` | `TextureLayerLimit` |
| `TextureChannelCeiling` | `TextureChannelLimit` |
| `RecordCeiling` | `RecordLimit` or `RecordCapacity` |
| `EntityCeiling` | `EntityLimit` or `EntityCapacity` |
| `RowCeiling` | `RowLimit` |
| `NamingCeiling` | `NamingMaximumX` when it is the right edge; otherwise `NameLengthLimit` |
| `CountCeiling` | replace with the domain noun, e.g. `LayerLimit`; do not preserve generic `Count` |
| `AccentCeiling` | `AccentLimit` |
| `RevisionCeiling` | `RevisionLimit` or `RevisionRetentionLimit` |
| `TriangleCeiling` | `TriangleLimit` / `TriangleCapacity` |
| `CardCeiling` | `CardLimit` |
| `RetentionCeiling`, `RetainedCeiling` | `RetentionLimit`, `RetainedLimit` |
| `DisplayExtentCeiling` | `DisplayExtentLimit`; if it is a coordinate, `DisplayMaximumX/Y` |
| `WorkspaceCeiling` | `WorkspaceLimit` |
| `IterationCeiling` | `IterationLimit` |
| `SubdivisionCeiling` | `SubdivisionLimit` |
| `SlotCeiling` | `SlotCapacity` for allocated storage, otherwise `SlotLimit` |
| `PathCeiling` | `PathLengthLimit` or `PathCapacity` |
| `EditableRunCeiling` | `EditableRunLimit` |
| `FaceCeiling` | `FaceLimit` / `FaceCapacity` |
| `EnclosureDepthCeiling` | `EnclosureDepthLimit` |
| `ByteCeiling` | `ByteBudget` |
| `CeilingReached` | `LimitReached` |

### Main migration areas

Start with public guarantees and constants, then call sites:

- `Engine/SlateUI/Interface/TexturePaintPanel/`
- `Engine/SlateUI/Interface/LayerStackPanel/`
- `Engine/SlateUI/Interface/ThemeInterchange/`
- `Engine/SlateUI/Interface/LayerStackSpecification/`
- `Engine/SlateUI/Interface/SceneDirectoryPanel/`
- `Engine/SlateDocument/` capacity guarantees
- `Engine/SlateCompute/` storage and geometry limits

## 2. Retire `Ordinal`

### Why it is weak

An ordinal answers “first, second, third.” Most Slate values named `Ordinal` instead answer “which zero-based slot in this container?” That is an **index**. The current spelling makes array access, stable identity, display order, and sequence numbering look like the same concept.

### Replacement rule

| Actual meaning | Replacement form |
|---|---|
| zero-based container position | `<Subject>Index` |
| generational identity slot | `SlotIndex` |
| user-visible 1-based label | `<Subject>Number` |
| monotonic issue/record number | `<Subject>SequenceNumber` |
| sorted precedence | `<Subject>Rank` or `<Subject>Order` |
| missing-index sentinel | `NoIndex` / `InvalidIndex` |
| count | `<Subject>Count`; never `Ordinal` |

### High-volume families and recommended names

| Existing family | Recommended name |
|---|---|
| bare `Ordinal` loop variable | domain name such as `RowIndex`, `LayerIndex`, or `VertexIndex` |
| `SlotOrdinal` | `SlotIndex` |
| `RecordOrdinal` | `RecordIndex`; use `RecordNumber` only if stable outside the container |
| `FaceOrdinal` | `FaceIndex` |
| `RecordingOrdinal` | `RecordingIndex` or `RecordingSequenceNumber`, according to storage semantics |
| `CellOrdinal` | `CellIndex` |
| `PartitionOrdinal` | `PartitionIndex` |
| `CornerOrdinal`, `CornerOrdinals` | `CornerIndex`, `CornerIndices` |
| `SpanOrdinal` | `SpanIndex` |
| `VertexOrdinal` | `VertexIndex` |
| `TargetOrdinal` | `TargetIndex` |
| `SourceOrdinal` | `SourceIndex` |
| `AxisOrdinal` | `AxisIndex`; prefer an `Axis` enum where feasible |
| `PlacementOrdinal` | `PlacementIndex` |
| `ImageOrdinal` | `ImageIndex` |
| `LevelOrdinal` | `LevelIndex` or `LevelNumber` |
| `TakenOrdinal` | `TakenIndex` |
| `SliderOrdinal` | `SliderIndex` |
| `TriangleOrdinal` | `TriangleIndex` |
| `AbsentOrdinal` | `NoIndex` |
| `ReservationOrdinal` | `ReservationIndex` |
| `LeafOrdinal` | `LeafIndex` |
| `HoveredOrdinal` | `HoveredIndex` |
| `PanelOrdinal` | `PanelIndex` |
| `ControlOrdinal` | `ControlIndex` |
| `ModuleOrdinal` | `ModuleIndex` |
| `ChannelOrdinal` | `ChannelIndex` |
| `MaterialOrdinal` | `MaterialIndex` |
| `BrushOrdinal` | `BrushIndex` |
| `TileOrdinal` | `TileIndex` |

Plural forms must follow the same rule: `CornerOrdinals` becomes `CornerIndices`, not `CornerIndexs`. Methods such as `OrdinalOf` should become `IndexOf`. Shader input variables can be renamed to `VertexIndex` while preserving the required vendor semantic `SV_VertexID`.

### Main migration areas

The greatest concentrations are in `TexturePaintPanel`, `ControlCentrePanel`, `LayerStackPanel`, `SceneDirectoryPanel`, `EditorPanel`, `OcclusionScheduler`, `TopologyConditioning`, `ChartPartition`, `SceneStructure`, and the validation hosts.

Migrate identity guarantees early. `Identity::SlotOrdinal` is the vocabulary source for many downstream declarations; renaming it to `SlotIndex` will prevent new `Ordinal` names from being copied.

## 3. Retire `Choice`

### Why it is weak

`Choice` currently names both lists of candidates and the selected candidate. That forces readers to inspect the type and use site to know whether a value is input, state, or result.

There are only five first-party identifier families, so this can be a small, complete migration:

| Existing | Recommended |
|---|---|
| `ExportChoices` | `ExportOptions` when it is the candidate list; `ExportSelection` when it is selected state |
| `TransferChoices` | `TransferOptions` / `TransferSelection` |
| `SegmentedChoice` | `SegmentedSelection` |
| `SettingChoice` | `SettingSelection` |
| bare `Choice` | `Option`, `Selection`, or the domain value, e.g. `Format` |

For carousel UI, prefer explicit terms such as `FormatOptions`, `SelectedFormat`, `ResolutionOptions`, `SelectedResolution`, `PresetOptions`, and `SelectedPreset`.

## 4. Replace plain `Construct`

### Finding

`Construct(` appears 206 times in 105 Engine files. There are 52 first-party method declarations/definitions with the plain name. C++ constructors such as `CameraComponent()` are language-defined names and are **not** part of this finding.

A one-word lifecycle method is especially unhelpful because current implementations perform unrelated operations: reserve GPU memory, create pipelines, register controls, derive frustum planes, populate rows, attach borrowed services, and assemble panels.

### Naming policy

Use the verb that describes the postcondition:

- **`Construct<Subject>`** when the method establishes the named owning mechanism;
- **`Attach<Subject>`** when it stores borrowed dependencies;
- **`Reserve<Subject>`** when it allocates fixed storage;
- **`Derive<Subject>`** when it computes a result from supplied values;
- **`Populate<Subject>`** when it fills an existing collection;
- **`Open<Subject>`** when it starts an external/device session.

Do not replace `Construct` with a new generic synonym such as `Initialize`, `Setup`, `Build`, or `Create`; those preserve the same ambiguity.

### Suggested API rename map

| Current owner | Suggested method |
|---|---|
| `AtmospherePresentationSurface::Construct` | `ConstructAtmosphereSurface` |
| `SpanSpace::Construct` | `ConstructSpanSpace` |
| `ShaderCodec::Construct` | `AttachShaderStreams` |
| `ProgramIndex::Construct` | `ConstructProgramIndex` |
| `OverlayPass::Construct` | `ConstructOverlayPass`; after grid split, `ConstructGroundGridPass` |
| `HostLifecycle::Construct` | `ConstructHost` |
| `HardwareMetrics::Construct` | `InspectHardware` |
| `DisplayScheduler::Construct` | `ConstructDisplayScheduler` |
| `DiagnosticExtension::Construct` | `AttachDiagnostics` |
| `DescriptorIndex::Construct` | `ConstructDescriptorIndex` |
| `CycleScheduler::Construct` | `ConstructCycleScheduler` |
| `CommandSequence::Construct` | `ConstructCommandSequence` |
| `ByteSpace::Construct` | `ReserveByteSpace` |
| `AttachmentIndex::Construct` | `ConstructAttachmentIndex` |
| `WorkspacePanel::Construct` | `ConstructWorkspacePanel` |
| `ViewportSequence::Construct` | `ConstructViewportSequence` |
| `TexturePaintPanel::Construct` | `ConstructTexturePaintPanel` |
| `SceneDirectoryPanel::Construct` | `ConstructSceneDirectoryPanel` |
| `PanelStructure::Construct` | `ConstructPanelPartition` |
| `LayerStackPanel::Construct` | `ConstructLayerStackPanel` |
| `InterfaceExchange::Construct` | `AttachInterface` |
| `InteractionIndex::Construct` | `AttachMotion` (after class rename: `ControlIndex::AttachMotion`) |
| `FacetPanel::Construct` | `ConstructFacetPanel` |
| `EditorPanel::Construct` | `ConstructEditorPanel` |
| `EditorLeafPanels::Construct` | `ConstructLeafPanels` |
| `DrawerSpace::Construct` | `ConstructDrawerSpace` |
| `ControlCentrePanel::Construct` | `ConstructControlCentrePanel` |
| `ContentBrowserPanel::Construct` | `ConstructContentBrowserPanel` |
| `ComponentSpecification::Construct` | `ConstructComponents` |
| `ControlPanel::Construct` | `ConstructControlPanel` |
| `WorkSequence::Construct` | `ConstructWorkerSequence` |
| `SpatialSubdivision::Construct(topology...)` | `ConstructSubdivision` |
| `SpatialSubdivision::Construct()` internal form | `ConstructNodes` or the exact recursive subject |
| `FrustumSpace::Construct` | `DeriveFrustumPlanes` |
| `RowSequence::Construct` | `PopulateVisibleRows` |
| `VisibilityIndex::Construct` | `ConstructVisibilityIndex` |
| `DepthReduction::Construct` | `ConstructDepthReduction` |
| `TileSpace::Construct` | `ReserveTileSpace` |
| `SurfaceTileSpace::Construct` | `ConstructSurfaceTiles` |
| `SurfaceDepot::Construct` | `ReserveSurfaceStorage` |
| `StrokeSpace::Construct` | `ConstructStrokeSpace` |
| `ReflectanceIntegrator::Construct` | `ConstructReflectanceLookup` |
| `PreviewProjection::Construct` | `ConstructPreviewProjection` |
| `EmissionSequence::Construct` | `ConstructEmissionSequence` |
| `AtmosphereIntegrator::Construct` | `ConstructAtmosphereLookups` |
| `AnalyticProjection::Construct` | `ConstructAnalyticProjection` |

The same operation should use the same name at declaration, definition, call site, diagnostics, and proof names. This rename should be done subsystem-by-subsystem rather than as one unreviewable global patch.

## 5. `InteractionIndex` audit

### What it actually does

`Engine/SlateUI/Interface/InteractionIndex/` implements:

- generational registration and resolution of every live `ControlIdentity`;
- one global pointer grab and the grabbed control part;
- release state and drag origin/initial reading;
- one disclosed popup;
- hover and take fade handles delegated to `MotionIntegrator`;
- fixed host-wide control capacity.

### Why the current name is odd

An “interaction index” suggests a searchable collection of interaction records or a map from interaction IDs to records. The class does not store historical interactions and callers do not query interactions. It registers **controls** and coordinates their live state. “Interaction” describes the broad topic; it does not identify the indexed subject.

### Recommendation

Rename the folder, class, include path, and type to **`ControlIndex`**:

```text
Engine/SlateUI/Interface/ControlIndex/
class ControlIndex
```

Use `Controls` for ordinary variables and parameters. Avoid the current `Interaction` aliases because they introduce a second metaphor for the same object.

`ControlIndex` is preferred over:

- `InteractionManager` — `Manager` is vague and retired;
- `InteractionRegistry` — still indexes the wrong noun and understates arbitration;
- `ControlState` — too broad and does not signal identity lookup;
- `ControlInteraction` — vivid, but not the repository's established container role.

If the object later becomes too broad, split rather than lengthen the name: keep identity/fades in `ControlIndex` and extract pointer/popup exclusivity into a focused arbitration component. That is not necessary for the naming-only migration.

Related member suggestions:

| Existing | Suggested |
|---|---|
| `ControlPose::HoverOrdinal` | `HoverIndex` |
| `ControlPose::TakeOrdinal` | `TakeIndex` |
| `Slot()` | `ResolveIndex()` |
| `RegisteredSlots` | `RegisteredCount` |
| `Disclose` / `Withdraw` | retain; these are already semantic |

## 6. `CameraComponent` placement

### Current defect

The public base declaration is:

```text
Engine/Application/CameraComponent/Api/CameraComponent.h
```

but its implementation is:

```text
Engine/Application/EditorHost/Source/CameraComponent.cpp
```

`Application` is an executable unit, and that source is compiled as part of the EditorHost subject. A Player or Spectator host cannot consume the compiled base camera implementation as a reusable engine library without duplicating source ownership or linking the application executable. The current header also imports `SlateUI/.../RecordingSurface.h` to obtain `CameraCondition`, making a supposedly host-independent camera depend on the UI layer.

### Recommended ownership

Create a new host-independent scene/runtime unit:

```text
Engine/SlateScene/
  Module.toml
  Scene/CameraComponent/Api/CameraComponent.h
  Scene/CameraComponent/Source/CameraComponent.cpp
  Scene/EditorCameraComponent/Api/EditorCameraComponent.h
  Scene/PlayerCameraComponent/Api/PlayerCameraComponent.h
  Scene/SpectatorCameraComponent/Api/SpectatorCameraComponent.h
```

Recommended product initially:

```toml
[unit]
name    = "SlateScene"
product = "StaticLibrary"
subject = [ "Scene" ]

[requires]
unit = [ "SlateMath", "SlateDocument" ]
```

A static library matches every current reusable engine unit and avoids imposing a DLL ABI before one is needed. Make it a dynamic library only if independent deployment/hot reload is a real requirement; that would also require an explicit exported ABI, ownership rules across the binary seam, versioning, and allocator compatibility. A DLL is not required merely to make code reusable.

### Dependency correction

Move `CameraCondition` out of `RecordingSurface.h` into a neutral camera/input guarantee that `SlateScene` can include without depending on `SlateUI`. The base should receive a host-translated condition and know nothing about ImGui, GLFW, Vulkan, panels, bookmarks, or editor commands.

### Type responsibilities

- **`CameraComponent`** — pose, projection-facing camera state, positional lag law, common movement primitives.
- **`EditorCameraComponent`** — editor fly navigation, focus/orbit behavior, editor constraints.
- **`PlayerCameraComponent`** — gameplay ownership, pawn/character following, gameplay constraints.
- **`SpectatorCameraComponent`** — detached/free-fly navigation and spectator-specific limits.

This preserves the requested base plus distinct camera types and deliberately avoids the rejected name `CameraRig`. Camera bookmarks remain editor data/services and should not move into the reusable base.

If adding a new unit is deferred, the least-wrong interim placement is `SlateDocument/Document/CameraComponent` only when camera pose is genuinely scene/document state. That is still preferable to `Application`, but `SlateScene` is cleaner because runtime behavior is not merely document content.

### Dependency direction

The intended direction is:

```text
SlateMath <- SlateDocument <- SlateScene <- Application hosts
                       ^            ^
                    scene data   camera behavior

SlateUI ------------------------------> Application composition only
```

`SlateScene` must not depend on `SlateUI`, `SlateVulkan`, or an Application host.

## 7. Overlay/grid shader and SPIR-V naming

### Finding

There is no source-controlled grid `.spv` file. The build script lowers each `.slang` source by stem to:

```text
<OutputRoot>/Shader/SlateVulkan/<Stem>.spv
```

The current source files are:

```text
Engine/SlateVulkan/Device/OverlayPass/Shader/OverlayVertex.slang
Engine/SlateVulkan/Device/OverlayPass/Shader/OverlayFragment.slang
```

They therefore become `OverlayVertex.spv` and `OverlayFragment.spv`, which `OverlayPass.cpp` resolves by those stems.

The naming problem is semantic: `OverlayFragment.slang` is not merely a generic overlay fragment. Its numeric **mode 3** performs the complete analytic ground-grid ray/plane intersection, minor and major lattice coverage, dot mode, horizon fade, and XYZ axis rendering. This substantial feature is hidden behind a generic artifact name and an unexplained integer.

### Preferred recommendation: split by rendered responsibility

```text
Engine/SlateVulkan/Device/GroundGridPass/
  Api/GroundGridPass.h
  Source/GroundGridPass.cpp
  Shader/GroundGridVertex.slang
  Shader/GroundGridFragment.slang
```

Generated artifacts then become self-describing:

```text
Shader/SlateVulkan/GroundGridVertex.spv
Shader/SlateVulkan/GroundGridFragment.spv
```

Keep gizmo/line/dot primitives in a focused overlay pass. If their responsibilities differ enough, use `GizmoPass`, `GizmoVertex.slang`, and `GizmoFragment.slang` rather than one generic overlay mechanism.

### Lower-risk recommendation: retain one pipeline but name it honestly

If a pass split would add undesirable draw submissions, rename the combined mechanism to:

```text
WorkspaceOverlayPass
WorkspaceOverlayVertex.slang
WorkspaceOverlayFragment.slang
```

and replace `Mode == 3u` with a named shader/CPU guarantee such as `OverlayDraw::GroundGrid`. This is less precise than a split, but it makes the generated SPIR-V discoverable and removes the magic-number guarantee.

### Related shared names

`Engine/Shared/OverlayGeometry.slang.h` and `OverlayTransform.slang.h` can remain generic only if they are genuinely shared by several overlay kinds. Grid-only functions/constants should move to an explicitly named `GroundGrid...` shared header. The CPU comment in `SceneDirectoryPanel` should then refer to `GroundGridFragment.slang`, not “overlay mode 3.”

## 8. Other retired vocabulary found

The broader naming rules also retire `Boundary`, `Binding`, `Region`, `Tree`, `Cadence`, `Submission`, `Footprint`, and `Vacancy`. First-party Engine results are:

- `Cadence`, `Submission`, `Footprint`, `Tree`, and `Vacancy`: no current first-party Engine identifiers containing these exact forms.
- `Binding`: 26 occurrences, but many are required Vulkan field/type spellings. Slate-owned `ColourBinding(s)` should become `ColourAssignment(s)` or `ColourSlots`, according to use.
- `Region`: 14 occurrences. `MembershipRegion` should become a semantic shape/domain name. `GetContentRegionAvail` is an ImGui API and must remain at the vendor edge.
- `Boundary`: 101 occurrences. Geometry should use `Contour`, `Perimeter`, `Edge`, or `Loop`; UI split limits should use `SplitLimit` or `ResizeLimit`; architecture seams should use `Seam` or `Edge`.

Representative replacement map:

| Existing | Suggested |
|---|---|
| `BoundaryLoop` | `ContourLoop` |
| `DeferredBoundary` | `DeferredSplit` or `DeferredDivider`, according to UI role |
| `BoundaryTouched` | `DividerTouched` / `EdgeTouched` |
| `BoundaryPresent` | `DividerPresent` / `EdgePresent` |
| `BoundaryHeld` | `DividerHeld` |
| `WithinBoundary` | `WithinExtent` |
| `MembershipRegion` | `MembershipDomain` or the actual shape name |
| `ColourBindings` | `ColourAssignments` |

Do not rewrite vendor declarations merely to make search output empty. The rule is to isolate vendor vocabulary at the seam and keep Slate-owned names semantic.

## 9. Migration order for a future implementation session

No implementation is included in this audit. A safe future sequence is:

1. Add a naming verification script with first-party scope and an explicit vendor allowlist.
2. Rename `Choice` families; this is the smallest complete migration.
3. Rename public identity vocabulary from `Ordinal` to `Index`, then migrate subsystem call sites.
4. Rename `Ceiling` constants by meaning, reviewing capacity versus limit versus extent at each declaration.
5. Rename plain `Construct` APIs one subsystem at a time.
6. Rename `InteractionIndex` to `ControlIndex` after its dependent UI APIs have the new index vocabulary.
7. Extract `CameraCondition` to a neutral guarantee and establish `SlateScene`; then move camera declarations and implementation together.
8. Split or rename the overlay/grid pass and update shader stem resolution as one atomic change.
9. Run a final first-party naming gate while preserving required vendor spellings.

Each step should compile and run its focused proofs before the next vocabulary family begins. Do not combine the entire audit into one mechanical rename patch: several terms require semantic decisions that cannot be made safely by search-and-replace.

## 10. Shared-foundation vocabulary

The former broad legal metaphor for cross-unit headers has been retired. **Foundation** now names the shared, dependency-free location, while each header names the value it actually supplies:

| Previous role | Replacement |
|---|---|
| combined CPU/shader declarations | `Foundation/Combination.h` |
| outcomes and refusals | `Foundation/DeliveryOutcome.h` |
| generational identities | `Foundation/Identity.h` |
| numerical precision guarantees | `Foundation/PrecisionGuarantee.h` |
| numerical tolerances | `Foundation/NumericTolerance.h` |
| compiler/platform declarations | `Foundation/Toolchain.h` |
| neutral camera input | `Foundation/CameraCondition.h` |
| scene-directory shared declarations | `SceneDirectorySpecification.h` |
| texture-paint shared declarations | `TexturePaintSpecification.h` |

`Foundation` is preferred because these headers are the dependency floor used by several units; it does not imply negotiation, legal terms, or a runtime interface. The naming gate now prevents the retired spelling from returning to first-party Engine source or paths.
