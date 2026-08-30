# Codex Interchange

## Purpose

Codex is Slate's one seekable binary document container. Every product profile uses the same `SLCD` byte
signature, version discipline, section index, integrity digest, revision sequence, and embedded-content route.
A profile changes the preferred authoring surface; it does not create a second binary format.

The first general workspace document is `WorkspaceCodex` with the `.codex` extension. A workspace may carry
complete specialized Codex documents without flattening their identity, version, revisions, or unknown sections.

## Product Profiles

- `WorkspaceCodex` — `.codex`: complete workspace, scene arrangement, environment, loaded assets, and revisions.
- `PigmentCodex` — `.pigment`: paint layers, masks, channels, and image references.
- `EnamelCodex` — `.enamel`: coating-oriented paint and surface finish authoring.
- `TextileCodex` — `.textile`: woven, knitted, and sheet textile content, including weave and fabric imagery.
- `GarmentCodex` — `.garment`: garment construction, seams, drape, and textile assignments.
- `CanvasCodex` — `.canvas`: heavy woven sheets, sails, tents, upholstery, and reinforcement zones.
- `WorldCodex` — `.world`: authored world arrangement and world-specific extension content.
- `TerrainCodex` — `.terrain`: elevation, terrain layers, and terrain-specific authored sources.
- `ImpulseCodex` — `.impulse`: physics simulation conditions, bodies, contacts, and simulation content.
- `SolidCodex` — `.solid`: manufactured solids and their precision-design children.
- `DraftingCodex` — `.draft`: technical views, dimensions, sheets, and annotations.
- `SketchCodex` — `.sketch`: constraint sketches and construction geometry.
- `AssemblyCodex` — `.assembly`: multi-part mechanical arrangement and relationships.
- `FabricationCodex` — `.fabrication`: cut, bend, and manufactured-form instructions.
- `MachiningCodex` — `.machining`: toolpath and machining-specific content.
- `DatumCodex` — `.datum`: engineering reference geometry and measurement bases.
- `InvoluteCodex` — `.involute`: gears and precision-curve content.
- `ProfileCodex` — `.profile`: section/profile-driven geometry.
- `SectionCodex` — `.section`: architectural and engineering sectional content.

## Binary Arrangement

A Codex stream starts with a fixed preamble, followed by independently addressable payload sections, then a
section index and completion record. Section positions are absolute byte positions, allowing a reader to seek
only the content needed for its current authoring surface.

The initial preamble carries:

- `SLCD` signature.
- Major and minor compatibility figures.
- Product profile.
- Preamble byte extent.
- Current index position and byte extent.
- Document identity.
- Current revision identity.
- Index digest.

The index carries one fixed-width entry for each payload section:

- Section code.
- Per-section schema version.
- Payload byte position and byte extent.
- Payload digest.
- Introducing revision identity.

A completion record repeats the current index position, index extent, and digest. A later append-safe writer will
write changed sections, then a new index, then a completion record, and only then restate the preamble. Opening
recovers the latest complete record if an interrupted save left the preamble behind it.

## Content Preservation

A reader retains every unrecognized section exactly. A product that cannot edit garment, terrain, machining, or
simulation content must preserve it when saving the surrounding workspace. Unknown content is never silently
removed because another authoring surface opened the document.

Embedded child Codex content is retained as whole child streams. A linked child records its document identity,
location, and digest. A packed child is a linked child copied into its owner for handoff while keeping its own
identity.

## Ownership Direction

`WorkspaceCodex` may carry all specialized profiles. `WorldCodex` may carry `TerrainCodex`, solids, and surface
content. `PigmentCodex` may carry or reference `TextileCodex` and `EnamelCodex`. `GarmentCodex` and
`CanvasCodex` may reference textile content.

`SolidCodex` may carry or reference `SketchCodex`, `DatumCodex`, `ProfileCodex`, `SectionCodex`,
`InvoluteCodex`, `AssemblyCodex`, `FabricationCodex`, and `MachiningCodex`. A focused precision document does
not become a catch-all owner for unrelated precision documents; this keeps dependencies directed and prevents
cycles.

## First Workspace Proof

`WhiteTeaService.codex` is the first general workspace proof. It will carry Sun, Sky, Atmosphere, five authored
tea-service geometry assets, one shared `WhiteDielectric.pigment` material source, authoritative rendering
packets, and revisions. The five assets are Utah Teapot, Teacup, Saucer, Sugar Bowl, and Milk Jug.

No synthetic scene rows or substitute geometry enter this proof. Loading registers the retained environment rows
and the five decoded geometry entries into the actual Scene Directory. The first render uses one shared white
dielectric material and transparent radiance outside resolved geometry, leaving the dynamic atmosphere visible.

## Delivery Sequence

1. ✔️ Deliver the in-memory Codex codec: preamble, payload sections, index, completion record, validation, and
   unknown-section retention.
2. ✔️ Deliver profile-matched stream writing and recovery through `.new` and `.prior` complete-stream candidates.
   Append-only section replacement follows once changed-section writing reaches the file route.
3. ◐ Deliver workspace, environment, material-reference, and nested-Codex sections. The typed workspace
   interchange carries independent Sun/Sky/Atmosphere figures, persisted scene placements, material references,
   and complete embedded child Codex streams without importing SlateUI vocabulary. A focused `PINF` typed
   pigment section now persists the shared fixed-white dielectric source without introducing unapproved image,
   layer, transparency, or advanced-material processing. Linked-child records follow in this stage.
4. ◐ Deliver the precise tea-service binary geometry and UV content inside `WhiteTeaService.codex`. Editable
   source topology must stay out of `EngineContent`; activation validates the binary workspace without resolving
   back to OBJ files. The canonical historical Utah Teapot source must replace the explicitly documented temporary
   demonstration teapot before final geometry residency lands.
5. Connect decoded geometry to authoritative rendering residency, fixed-white radiance, and actual viewport proof.
