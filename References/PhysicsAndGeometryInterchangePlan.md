# Physics and Geometry Interchange Plan

## Scope

This document reserves two neutral integration seams without implementing either class:

- **PhysicsInterchange** — Slate-authored simulation data to Jolt Physics or a custom solver, and evaluated state back to Slate.
- **GeometryInterchange** — file codecs and external geometry representations to Slate’s editable topology, render meshes, and scene recipes.

Neither seam owns the scene registry, editor UI, renderer, file picker, or solver. Both translate explicitly between stable Slate data and replaceable backends.

## What already exists

Slate already has three relevant foundations:

1. `FileInterchange` provides platform-level file access.
2. `AssetInterchange` accepts already-decoded topology and images, validates them, applies declared unit scale once, and registers them without silently repairing source data.
3. The Scene Import/Export UI already presents common format choices.

What does **not** yet exist is the full codec layer that parses FBX, glTF/GLB, OBJ, USD/USDZ, DAE, STL, PLY, or Alembic into a complete decoded scene. `AssetInterchange` explicitly receives decoded topology; it is not itself an FBX/glTF/USD parser. The format carousel is authored UI, not proof that each codec is implemented.

## PhysicsInterchange

### Purpose

`PhysicsInterchange` translates between backend-neutral Slate simulation descriptions and one selected physics backend. Jolt is the first adapter, while custom Slate physics and research solvers remain first-class alternatives.

No Jolt type may cross the interchange API. A scene component must not contain `JPH::BodyID`, Jolt allocators, Jolt vectors, or Jolt callbacks. Those remain inside the Jolt adapter.

### Authored input

The interchange reads immutable authored snapshots containing stable entity IDs and components such as:

- transform and world frame;
- rigid body mode: static, kinematic, or dynamic;
- mass, centre of mass, and inertia policy;
- collision shapes and local shape transforms;
- physical material;
- collision layers and masks;
- joints and constraints;
- velocity and initial sleep state;
- force fields;
- cloth, soft-body, fluid, or custom-solver participation;
- simulation quality and deterministic-step preferences.

Units are explicit: metres, kilograms, seconds, radians, Newtons, and Pascal-based material quantities. Axis and handedness conversion occurs once at the adapter edge and is tested with known poses.

### Backend-neutral shape description

Use a closed shape description independent of a solver:

- box, sphere, capsule, cylinder, tapered capsule;
- convex hull;
- triangle mesh;
- height field;
- compound shape;
- signed-distance or implicit shape where supported;
- custom shape payload identified by a stable provider ID.

The shape description preserves source geometry identity so cooking results can be cached by content hash. Runtime cooked objects belong to the backend adapter, not the document.

### Backend adapter shape

A backend adapter provides operations equivalent to:

```text
CreateWorld(settings) -> WorldHandle
Synchronise(authoredSnapshot, changeSet)
Step(world, fixedDelta, stepCount)
ReadEvaluatedState(world) -> PhysicsSnapshot
Query(world, queryDescription) -> QueryResults
DrainEvents(world) -> PhysicsEvents
DestroyWorld(world)
```

Handles are generation checked and private to the adapter. Slate entity IDs are maintained in an adapter-owned lookup table.

### Jolt adapter

The Jolt adapter is responsible for:

- Jolt allocator and job-system setup;
- object and broad-phase layer mapping;
- body and shape creation;
- shape cooking/cache ownership;
- activation and sleep policy;
- contact and trigger listener translation;
- fixed-step scheduling;
- character/vehicle features when explicitly enabled;
- conversion between Slate and Jolt numeric types;
- reclaiming every Jolt object before Jolt shutdown.

Jolt callbacks write bounded event records. They never mutate Slate entities directly from physics worker threads.

### Custom physics adapters

Custom solvers implement the same interchange surface. They may support only a subset such as particles, cloth, fluids, robotics, FEM, or research constraints. Every adapter publishes a capability description:

- supported body and shape families;
- deterministic/replay support;
- CPU/GPU execution;
- cloth/fluid/soft-body support;
- query types;
- maximum scale and precision;
- serialization and warm-start support.

Unsupported authored features are reported before stepping. They are never silently dropped.

### Mixed backends

One simulation island has one authoritative backend. Different islands may use different backends, but cross-backend interaction requires an explicit bridge system. A bridge exchanges bounded proxies, forces, or sampled boundary conditions; it does not share backend object pointers.

Examples:

- Jolt rigid bodies drive collision proxies for a custom fluid solver.
- A cloth solver reads animated/Jolt collider snapshots and publishes cloth vertices.
- A research vehicle tyre solver exchanges forces with a Jolt chassis at fixed synchronization points.

### Evaluated output

The physics snapshot contains stable entity IDs and evaluated values:

- world transform;
- linear and angular velocity;
- sleep/activation state;
- contact/trigger events;
- joint state and break events;
- optional solver diagnostics.

Evaluated state is separate from authored transform data. The editor can preview it, record it, or explicitly bake selected values through an undoable command.

### Determinism and replay

Record:

- backend identity and version;
- adapter version;
- fixed timestep;
- substep count;
- random seeds;
- authored revision/hash;
- capability settings;
- external inputs by tick.

A replay must fail clearly when the selected backend cannot satisfy the recorded deterministic mode.

### Dependency direction

```text
SlateComposition + SlateSpatial + authored physics components
                         ↓
                 PhysicsInterchange
                    ↙           ↘
          Jolt adapter       custom adapters
                    ↘           ↙
              evaluated PhysicsSnapshot
                         ↓
       scene, renderer, audio, and editor consumers
```

The core scene and components do not depend on Jolt.

## GeometryInterchange

The Phase 2 workspace, GPU geometry, selection, gizmo, base-material layer, and texture-evaluation delivery is expanded in `GeometryWorkspaceAndMaterialProcessingPlan.md`. This section remains the neutral interchange contract.

### Purpose

`GeometryInterchange` converts decoded external scenes and geometry into representations suited to editing, rendering, simulation, and round-trip export while preserving source meaning.

It should sit above file codecs and below scene/entity recipe creation.

### Codec layer

Each codec has a narrow responsibility:

```text
bytes + import options -> DecodedScene + diagnostics
DecodedScene + export options -> bytes + diagnostics
```

Likely adapters:

- glTF/GLB;
- OBJ/MTL;
- FBX;
- USD/USDZ;
- Collada/DAE;
- STL;
- PLY;
- Alembic;
- future CAD/B-Rep formats through separate geometry providers.

Codec adapters preserve unsupported named data in diagnostics or retained extension payloads. They do not weld, triangulate, recalculate normals, or discard properties unless the authored import options explicitly request that operation.

### Decoded scene

A decoded scene is broader than the existing `DecodedTopology` and includes:

- named nodes and parent relationships;
- transforms and coordinate frames;
- meshes and polygon groups;
- points, lines, polygons, and subdivision data;
- normals, tangents, UV sets, colours, and arbitrary vertex/corner properties;
- materials and texture references;
- cameras and lights;
- skeletons, joints, inverse-bind matrices, and skin weights;
- animation clips and property tracks;
- custom properties and namespaces;
- units, axis conventions, handedness, and source record;
- retained extension payloads where safe round-trip preservation is possible.

### Geometry representations

No single mesh layout serves every use. GeometryInterchange should produce or derive several explicit representations.

#### Polygon soup

The faithful intake form:

- source positions;
- corner-index runs;
- face groups;
- source-order attributes;
- no assumed manifoldness.

This is the closest representation to imported files and is suitable for validation and diagnostics.

#### Half-edge topology

The editable topology form:

- vertex, directed half-edge, edge, loop, and face records;
- stable topology element IDs;
- explicit boundary loops;
- non-manifold diagnostics or an extension representation;
- per-vertex, per-edge, per-corner, and per-face attribute stores;
- reversible mapping back to source element IDs.

Half-edge construction is a deliberate derivation from polygon soup. It reports duplicate directed edges, non-manifold edges, bow-tie vertices, invalid loops, and inconsistent winding. Repair is a separate authored operation.

#### Indexed render mesh

The renderer form:

- compact vertex/index buffers;
- explicitly triangulated faces;
- split vertices where corner attributes differ;
- material draw ranges;
- meshlets and acceleration data as derived caches;
- source and half-edge mapping retained for picking.

The render mesh is disposable and reproducible. It is not the editing authority.

#### Simulation mesh

Physics, cloth, and fluids may derive:

- collision triangle meshes;
- convex decompositions;
- tetrahedral volumes;
- cloth particle/constraint graphs;
- signed-distance fields;
- fluid boundaries.

These belong to backend caches keyed by authored geometry revision.

### Explicit operations

Geometry operations are separate, named stages:

- coordinate-system conversion;
- unit conversion;
- triangulation;
- normal generation;
- tangent generation;
- vertex welding;
- degenerate removal;
- manifold repair;
- decimation;
- subdivision;
- UV generation;
- colour-space conversion for vertex colours.

Each stage records parameters and diagnostics. Import remains faithful unless the user enables a stage in the transfer dialogue.

### Triangulation

Triangulation remains a boolean import/export option in the Scene Transfer UI because it is a single explicit operation. The chosen triangulator and its version should still be included in the source record when triangulation is enabled.

Half-edge faces remain polygons. Triangles are derived for render/simulation consumers rather than replacing editable polygon faces by default.

### Vertex colours

Vertex colours need more than a boolean:

- include/exclude;
- replace, multiply, or ignore existing colours;
- source, sRGB, or linear interpretation;
- vertex versus corner storage preservation;
- named colour-set selection when a format provides several sets.

### Materials

Material intake options include:

- create new materials;
- reuse matching authored materials;
- retain links to source material identities;
- relative, copied, or embedded texture handling;
- unsupported shader-node diagnostics;
- deterministic texture and material naming.

### Custom properties

Custom-property intake includes:

- all, supported-only, or none;
- preserve or flatten namespaces;
- retained source type information;
- collision-safe naming;
- diagnostics for values that cannot be represented.

### Armatures and animation

Armature intake includes:

- include/exclude skeletons;
- primary and secondary bone-axis conversion;
- leaf-bone policy;
- deform-bones-only filtering;
- stable joint IDs;
- inverse-bind matrices;
- skin weight normalization policy;
- animation range selection and curve resampling.

Skeleton, skin, and animation data remain separate from polygon topology but share decoded-scene node identities.

### Entity recipe creation

After successful geometry intake, a scene builder creates entity recipes rather than backend objects:

```text
Entity: imported node
  optional TransformComponent
  optional GeometryInstanceComponent
  optional MaterialAssignmentComponent
  optional SkeletonComponent / SkinComponent
  source record component
```

Nothing is registered if validation fails partway through the transaction.

### Dependency direction

```text
FileInterchange
      ↓
format codec adapters
      ↓
DecodedScene / polygon soup
      ↓
GeometryInterchange
   ↙       ↓        ↘
half-edge render mesh simulation derivations
   ↘       ↓        ↙
AssetInterchange + entity recipe transaction
```

File codecs do not depend on the editor. Half-edge editing does not depend on Vulkan. Render and physics caches do not become document authority.

## Naming rule

The previously rejected naming term is not used for any type, module, file, path, or architectural layer. The neutral terms are `Interchange`, `Adapter`, `Description`, `Snapshot`, `Recipe`, and `Capability`.

## Delivery order

1. Expand decoded topology into `DecodedScene` without implementing every codec.
2. Define stable geometry element IDs and source mappings.
3. Implement polygon-soup validation.
4. Implement half-edge derivation with non-manifold diagnostics and no automatic repair.
5. Implement indexed render-mesh derivation and picking mappings.
6. Add glTF/GLB and OBJ codec adapters first; add FBX/USD through isolated providers.
7. Add skeleton, skin, animation, material, vertex-colour, and custom-property payloads.
8. Define backend-neutral physics descriptions and snapshots.
9. Implement the Jolt adapter behind PhysicsInterchange.
10. Add custom-solver capability registration and one minimal reference adapter.
11. Add cache keys, deterministic replay metadata, and cross-backend bridge tests.
12. Consider DLL extraction only after static boundaries and ownership tests hold.
