# Entity Composition Plan

## Decision

Slate entities should use composition over an inherited universal object. There is no `UObject` equivalent, no mandatory transform, no all-knowing base class, and no single reflection-heavy allocation type.

An entity is a stable identity in a scene registry. Components are independently versioned data attached to that identity. Systems operate on explicit component combinations. Editors submit commands against entity and component identifiers rather than retaining mutable implementation pointers.

## 1. The entity record

Every entity registry slot contains only the universally required information:

```text
EntityId       128-bit stable document identity
Generation     stale-handle protection for runtime slots
Name           authored display name
Tags           searchable classification
State          active, hidden, archived, pending removal
ComponentMap   type identifier -> component storage handle
```

`EntityId` survives save/load, duplication mapping, references, undo, and collaboration. A compact runtime handle `{slot, generation}` accelerates queries but is never serialized as identity.

Name and tags may later move into a metadata component if very large scenes benefit from sparse metadata. That is a storage decision, not an inheritance hierarchy.

## 2. Components

A component is plain authored or evaluated data with:

- a stable `ComponentTypeId`;
- a schema version;
- explicit ownership by one entity unless declared shareable;
- deterministic serialization and migration;
- no UI, renderer, allocator, or application-host dependency;
- no inheritance requirement;
- an optional evaluator supplied by a separate system.

Components do not call each other through hidden virtual methods. A system declares the component views it reads and writes. Cross-domain communication uses typed demands/events or immutable snapshots.

### Optional spatial placement

`TransformComponent` remains optional. It contains local translation, orientation, scale, and optional parent entity. An entity representing an audio asset, material, simulation profile, export preset, or research result needs no transform.

Suggested spatial family:

- `TransformComponent` — local authored transform and parent relation;
- `WorldTransformState` — evaluated frame-local result, not authored data;
- `BoundsComponent` — authored or evaluated local bounds;
- `VelocityComponent` — linear and angular motion state;
- `AttachmentComponent` — named socket/frame attachment when a transform parent alone is insufficient;
- `GeospatialFrameComponent` — large-world/CAD/geographic coordinate frame.

Parenting and component ownership are separate. Removing a transform must not delete the entity or its non-spatial components.

## 3. Placement workflow

The editor places content through commands, not by constructing an omnipotent object:

```text
CreateEntity(name, tags)
AttachComponent(entity, TransformComponent{...})
AttachComponent(entity, GeometryInstanceComponent{asset})
CommitTransaction()
```

A palette item is an `EntityRecipe`: a versioned declaration of components and initial values. Recipes can represent a camera, lamp, cloth sheet, water volume, vehicle engine, measurement marker, or CAD reference without introducing subclasses.

Placement tools resolve a recipe, preview a temporary transform, validate dependencies, and commit one undoable transaction. Runtime code sees only the resulting entity and components.

## 4. Component families

### Geometry and presentation

- `GeometryInstanceComponent`
- `CurveComponent`
- `SurfaceComponent`
- `ParametricBodyComponent`
- `MaterialAssignmentComponent`
- `VisibilityComponent`
- `RenderLayerComponent`
- `DecalComponent`
- `VolumePresentationComponent`

Geometry components refer to document assets through handles. They do not own Vulkan resources.

### Cameras and views

- base `CameraComponent` for projection, lens, clipping, and exposure association;
- `EditorCameraComponent` for editor-only movement preferences and bookmarks;
- `PlayerCameraComponent` for application-controlled view behaviour;
- `SpectatorCameraComponent` for unconstrained observation;
- `CameraOutputComponent` for a render target or viewport association.

Input interpretation stays in the application/editor layer. Camera data remains usable by CAD, rendering, simulation, and research tools.

### Light, sky, and environment

- `DirectionalLightComponent`
- `PointLightComponent`
- `AreaLightComponent`
- `SunComponent`
- `AtmosphereComponent`
- `FogVolumeComponent`
- `CloudVolumeComponent`
- `EnvironmentProbeComponent`
- `ExposureComponent`

The GPU atmosphere output is an adapter result. It is not a component and is not owned by UI code.

### Cloth and deformables

- `ClothComponent` — material and solver profile references;
- `ClothConstraintComponent` — pins, seams, and authored constraints;
- `SoftBodyComponent`;
- `DeformerComponent`;
- `CollisionShapeComponent`.

Simulation state lives in a solver-owned evaluated store. Authored components remain deterministic and undoable.

### Fluids, fire, clouds, mist, and fog

Avoid one enormous `FluidComponent`. Use a shared domain description plus specialised capability components:

- `SimulationVolumeComponent` — domain bounds and resolution policy;
- `FluidMaterialComponent` — density, viscosity, surface behaviour;
- `LiquidSourceComponent` and `LiquidDrainComponent`;
- `GasComponent` — gaseous transport properties;
- `CombustionComponent` — fuel, temperature, reaction parameters;
- `SmokeComponent`;
- `MistComponent`;
- `CloudComponent`;
- `FogComponent`;
- `FlowEmitterComponent`;
- `FlowColliderComponent`.

Water, fire, smoke, and clouds become recipes composed from these pieces. Solvers advertise supported component combinations rather than owning entity types.

### Vehicles and mechanical assemblies

Small components should remain small and reusable:

- `EngineComponent` — torque source and operating range;
- `TurbochargerComponent` — compressor/turbine map and shaft state;
- `TransmissionComponent`;
- `DifferentialComponent`;
- `WheelComponent`;
- `SuspensionComponent`;
- `FuelSystemComponent`;
- `ThermalComponent`;
- `BatteryComponent`;
- `ElectricMotorComponent`;
- `ControlInputComponent`;
- `MechanicalConnectionComponent` — explicit ports and ratios.

A vehicle is a recipe or assembly graph, not a `VehicleObject` subclass. The same engine and thermal components can serve research rigs, generators, CAD assemblies, and runtime vehicles.

### Physics

- `RigidBodyComponent`
- `MassPropertiesComponent`
- `CollisionShapeComponent`
- `JointComponent`
- `ForceFieldComponent`
- `PhysicalMaterialComponent`
- `SimulationParticipationComponent`

Physics publishes evaluated transform and velocity snapshots. It does not overwrite authored state without an explicit bake/accept command.

## 5. Audio architecture

Audio should be split by responsibility rather than “editor versus game” inheritance.

### Authored audio data

A future `SlateAudioDocument` family owns non-destructive source and edit descriptions:

- source media references;
- channel layouts and sample-rate metadata;
- regions, markers, loops, and annotations;
- clip graphs and non-destructive processing graphs;
- spectral-analysis caches;
- source records and research metadata.

### Scene/runtime audio

A future `SlateAudioScene` family supplies components such as:

- `AudioEmitterComponent`;
- `AudioListenerComponent`;
- `AudioZoneComponent`;
- `AudioRoutingComponent`;
- `AudioOcclusionComponent`;
- `AudioReactiveComponent`.

A runtime mixer consumes immutable scene snapshots. “Game audio” is one application profile of this runtime, not the name of the core module.

### Audio editor and research tools

A future `SlateAudioWorkbench` application/tool layer can provide waveform, spectrogram, phase, loudness, spatial field, transient, and feature visualisation; non-destructive editing; batch processing; live capture; comparison; annotation; and export. Research algorithms live behind analysis interfaces and write versioned result assets. They do not become mandatory dependencies of runtime playback.

Dependency direction:

```text
SlateAudioDocument <- SlateAudioAnalysis
SlateAudioDocument + SlateAudioScene <- runtime mixer adapter
SlateAudioDocument + SlateAudioAnalysis + SlateUI <- SlateAudioWorkbench
```

The runtime mixer never depends on the workbench.

## 6. Storage and evaluation

Use typed, chunked component stores. Each store owns one component type and can choose structure-of-arrays or array-of-structures storage. The registry maps entity handles to store rows.

A frame evaluation proceeds as:

1. freeze an authored revision;
2. identify dirty component families;
3. schedule systems from declared reads/writes;
4. produce evaluated state in a separate frame store;
5. publish immutable render, physics, audio, and UI snapshots;
6. retire old snapshots after consumers release their frame token.

This prevents renderer, physics, audio, and editor threads from sharing mutable entity objects.

## 7. References, events, and removal

Entity references use stable IDs in documents and generation-checked handles at runtime. Components declare whether a missing reference is optional, disables evaluation, or is a validation error.

Removal is staged:

1. mark entity pending removal;
2. publish typed removal notices to indexes and evaluators;
3. detach external runtime resources;
4. remove component rows;
5. increment the runtime slot generation;
6. preserve tombstone metadata when undo/collaboration requires it.

There is no destructor cascade through an object inheritance tree.

## 8. Undo, collaboration, and serialization

All authored edits are commands containing stable entity/component/property identifiers and before/after values. Commands group into transactions. Structural commands cover create, attach, detach, reparent, and remove.

Files store:

- entity stable ID and metadata;
- component type ID and schema version;
- component payload;
- external asset references;
- unknown component payloads for forward-compatible round trips.

Each component family owns migrations between schema versions. Binary module ABI versioning remains separate from document schema versioning.

## 9. Module direction

No new DLL should be created until these boundaries hold as static modules and tests.

```text
Foundation / SlateMath
        ↓
SlateComposition        entity IDs, registry interfaces, type IDs, commands, snapshots
        ↓
SlateSpatial            transform, bounds, attachments, coordinate frames
        ↓
Geometry | Physics | Environment | Mechanics | AudioScene | AudioDocument
        ↓
Evaluation adapters and SlateScene integration
        ↓
SlateVulkan / compute / audio backend adapters
        ↓
SlateUI and application workbenches
```

`SlateComposition` remains the preferred neutral name for the eventual shared component module. It must not absorb domain solvers merely because they consume entities.

## 10. Delivery sequence

1. Define `EntityId`, runtime handle, component type ID, and schema metadata.
2. Add a registry with typed fixed-capacity test stores.
3. Move transform ownership behind an optional spatial component view.
4. Add command-based create/attach/detach/remove and undo tests.
5. Add immutable scene snapshots and migrate renderer inputs.
6. Introduce entity recipes and the editor placement tool.
7. Migrate camera, light, atmosphere, geometry, and material families.
8. Add physics and mechanics through adapters.
9. Add audio document/scene foundations before building the Audio Workbench.
10. Consider DLL extraction only after ABI, lifetime, migration, and cross-module allocation tests pass.

## Non-goals

- no universal inherited object;
- no mandatory transform;
- no component-owned editor widgets;
- no component-owned Vulkan or audio-device resource;
- no string-based message bus for normal system interaction;
- no hidden mutation through global registries;
- no DLL extraction in the current increment.
