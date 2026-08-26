# SlateAuthoring migration plan

## Context

The requested end state is one authoring product/library, **SlateAuthoring**, built from the existing SlateCad and SlateGeometry work. The current checkout contains only a one-line `README.md`, so this plan starts with an inventory and deliberately does not assume a language, UI framework, file format, or existing public API.

This document treats SlateGeometry as the geometry/domain foundation and SlateCad as the CAD tools and workflows that consume it. The names can remain as internal package names during migration, but SlateAuthoring should be the only product-facing name.

## Goals

- Give users one coherent authoring application/API instead of two separately coupled codebases.
- Keep geometry deterministic, testable, and independent of the UI/runtime.
- Preserve existing SlateCad behavior and SlateGeometry data wherever possible.
- Establish a versioned document model, command history, import/export boundary, and extension points.
- Allow incremental migration with working builds at every step.

## Non-goals for the first migration

- Redesigning every tool or changing the visual language at the same time.
- Changing geometry algorithms merely because files move.
- Introducing a cloud/backend requirement.
- Supporting every legacy format before the core authoring loop works.

## Target architecture

```text
SlateAuthoring app / host
├── authoring-ui       viewport, panels, tools, selection, input mapping
├── authoring-core     document state, commands, undo/redo, transactions, events
├── cad-domain         CAD entities, constraints, snapping, tool workflows
├── geometry           primitives, transforms, intersections, topology, tolerances
├── io                 versioned serialization and legacy import/export adapters
└── shared              IDs, units, diagnostics, math utilities, feature flags
```

Suggested dependency direction:

```text
UI → authoring-core → cad-domain → geometry
UI → io             → authoring-core / cad-domain / geometry
```

`geometry` must not import UI, application state, browser APIs, or CAD tool code. `authoring-core` should expose framework-neutral commands and state so a second UI or headless automation client remains possible.

## Target contracts to agree before porting code

### 1. Document model

Define and version a canonical `AuthoringDocument`, for example:

- `schemaVersion`
- document units and coordinate-system metadata
- stable entity/layer/constraint IDs
- geometry and transforms in model coordinates
- layers, visibility, styles, and selection-independent metadata
- constraints/relationships
- optional application metadata and migration history

Decide explicitly whether the first release is 2D only or also carries 3D entities. Do not encode screen pixels in the document model.

### 2. Geometry contract

Document the supported primitives and invariants: points/vectors, lines/segments, arcs/curves, polylines/polygons, transforms, bounding boxes, intersections, offsets/booleans, and topology. Define units, handedness, angle convention, equality rules, numerical tolerance, degenerate-input behavior, and error/result types.

Prefer immutable value objects or pure functions at this boundary. Every operation should state its tolerance policy rather than relying on hidden global state.

### 3. Command contract

All document mutations should go through serializable commands or transactions:

- `execute`, `undo`, and `redo`
- affected entity IDs
- validation and diagnostics
- merge/coalescing rules for drag operations
- deterministic replay for tests and future collaboration

The UI may request a command, but must not mutate geometry or document state directly.

### 4. Extension contract

Define registration points for tools, entity types, exporters, renderers, and commands. Keep the initial registry small; an explicit stable API is safer than exposing every internal class during the move.

## Phased execution

### Phase 0 — Inventory and freeze the contracts

1. Obtain the actual SlateCad and SlateGeometry source trees, package manifests, build commands, licenses, and test data.
2. Record the dependency graph and mark code as geometry, CAD domain, UI, persistence, platform glue, or dead/duplicate code.
3. List public exports, supported file formats, sample documents, known performance limits, and current bugs.
4. Capture baseline behavior with golden files, screenshots/render snapshots, and representative interaction flows.
5. Freeze new features briefly or use a feature branch/flag so the migration is not chasing moving APIs.

**Exit criteria:** an inventory, ownership map, baseline test command, compatibility matrix, and a short decision log for 2D/3D, units, runtime, and release compatibility.

### Phase 1 — Create the SlateAuthoring shell

1. Add the target workspace/package layout without moving implementation yet.
2. Add formatting, linting, type checking, unit tests, build/package scripts, and CI.
3. Add shared IDs, units, diagnostics, and feature-flag definitions.
4. Add dependency rules that prevent geometry from importing UI/application modules.
5. Publish or document a minimal internal API and keep legacy entry points compiling through adapters.

**Exit criteria:** a clean SlateAuthoring build with a smoke test and no behavior change for the legacy path.

### Phase 2 — Move and harden SlateGeometry

1. Port geometry modules in dependency order: math primitives → coordinate transforms → curves/solids → predicates/intersections → topology/booleans → spatial indexing.
2. Normalize naming, error handling, tolerance configuration, and serialization of geometry values.
3. Preserve old imports temporarily with deprecation shims rather than mass-editing every consumer at once.
4. Add unit, property-based, degeneracy, and round-trip tests. Include adversarial cases such as nearly parallel lines, zero-length segments, coincident edges, very large/small coordinates, and repeated transforms.
5. Benchmark the operations used by selection, snapping, rendering, and import.

**Exit criteria:** SlateAuthoring owns the canonical geometry implementation; old and new entry points produce equivalent results on the fixture corpus.

### Phase 3 — Move SlateCad into the authoring domain layer

1. Port entity definitions and constraints to the versioned document model.
2. Replace direct geometry/UI mutations with commands and transactions.
3. Port selection, snapping, construction tools, constraints, and validation one workflow at a time.
4. Implement undo/redo and command coalescing before migrating complex drag tools.
5. Add legacy-to-canonical and canonical-to-legacy adapters where compatibility is required.

**Exit criteria:** core CAD workflows run against `AuthoringDocument` and can be replayed headlessly without the UI.

### Phase 4 — Build the SlateAuthoring application shell

1. Port viewport/rendering and map input events to authoring commands.
2. Add document lifecycle: new/open/save/save-as, dirty state, recovery/autosave, and diagnostics.
3. Add tool activation, selection, layers/properties, snapping feedback, and undo/redo UI.
4. Keep rendering as a projection of document state; never make the render scene the source of truth.
5. Add import/export progress and actionable validation errors.

**Exit criteria:** a user can create, edit, undo, save, reload, and export a representative document without using a legacy path.

### Phase 5 — Compatibility cutover

1. Put the new authoring path behind a feature flag while it reaches parity.
2. Run old and new implementations against the same fixtures where feasible and diff document results, geometry, and rendered output.
3. Migrate consumers and examples to SlateAuthoring imports.
4. Publish migration notes and codemods/automated import replacements if external consumers exist.
5. Deprecate SlateCad/SlateGeometry product entry points; retain thin shims for one compatibility window.

**Exit criteria:** the new path is the default, legacy fixtures open or fail with a clear supported-format message, and telemetry/bug triage shows no unresolved blocker regressions.

### Phase 6 — Remove legacy code and release

1. Remove shims only after the announced compatibility window.
2. Delete duplicate algorithms and update ownership/documentation.
3. Tag a release with the document-schema version and upgrade instructions.
4. Maintain a migration tool for older documents independently of the UI.
5. Record post-migration performance and remaining follow-up work.

## Testing strategy

- **Geometry:** deterministic unit tests, property tests, fuzzed degenerate inputs, tolerance-boundary tests, and benchmark fixtures.
- **Documents:** schema validation, migration tests for every supported version, import/export round trips, stable IDs, and backward-compatibility fixtures.
- **Commands:** execute/undo/redo equivalence, transaction rollback, replay determinism, and command serialization.
- **Application:** viewport/tool smoke tests, keyboard/mouse flows, autosave/recovery, and visual snapshots for representative drawings.
- **Regression:** run SlateCad's existing fixtures before and after each migrated subsystem; compare both numerical output and user-visible behavior.
- **Quality gates:** no new direct UI-to-geometry mutations, no unversioned persisted schema changes, and no unexplained benchmark regression beyond an agreed threshold.

## Recommended first slices

Use vertical slices rather than moving entire directories in one change:

1. A minimal document containing one point/line and a versioned save/load round trip.
2. Select and move that entity through a command with undo/redo.
3. Add snapping/intersection using the migrated geometry package.
4. Add one representative CAD tool and its legacy fixture.
5. Add a second entity/tool only after the first slice passes in CI.

Each slice should leave the repository buildable and should include the adapter, tests, and documentation needed to replace the corresponding legacy path.

## Risks and mitigations

| Risk | Mitigation |
| --- | --- |
| Floating-point changes alter drawings | Freeze tolerance rules, use golden fixtures, and make coordinate conversions explicit. |
| UI and geometry are tightly coupled | Enforce dependency direction and move behavior behind commands. |
| Legacy IDs or file formats are lost | Stable-ID policy plus versioned import/export fixtures before porting. |
| Large drawings become slower | Baseline first; benchmark spatial indexes, rendering, selection, and booleans per phase. |
| A rename creates a flag day | Keep deprecated adapters and migrate consumers incrementally. |
| Scope expands into a rewrite | Preserve algorithms first; defer redesign until parity and measurements exist. |
| Hidden state makes replay unreliable | Centralize document state and require deterministic commands/transactions. |

## Decisions needed from the project owner

1. Where are the SlateCad and SlateGeometry source repositories/paths? They are not present in this checkout yet.
2. Is SlateAuthoring a desktop app, a reusable library, a web app, or all three?
3. Is the initial target 2D CAD, 3D CAD, or a 2D authoring layer with a future 3D extension?
4. Which legacy document/file formats must remain readable and writable?
5. Are external consumers relying on the current SlateCad or SlateGeometry APIs, and what compatibility window is required?
6. What are the target platforms, language/toolchain, and acceptable performance budgets?

## Definition of done

SlateAuthoring is the default product-facing entry point; its document schema is versioned; geometry is UI-independent; core edits are command-based and undoable; legacy documents have tested migrations; representative CAD workflows have parity coverage; CI checks build, types, tests, and dependency boundaries; and the old entry points are either supported by documented shims or intentionally removed in a versioned release.
