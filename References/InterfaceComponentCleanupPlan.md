# Interface component inventory and cleanup plan

## Goal

Make SlateUI a composition of small mechanisms and domain panels rather than a set of deep panel hierarchies that each redraw the same controls. A component receives caller-owned state, interaction identity, geometry, and theme roles; it reports user intent. Domain panels decide what the intent means.

This is the migration plan and progress record. The validation-host cleanup and dead-prototype retirement are complete. Control Centre UI Scaling drives `ViewportSequence` in EditorHost and PaintHost. Interface geometry antialiasing is typed and functional; font, viewport, and future SVG tessellation quality remain separate typed domains. Dropdown consolidation is complete: the duplicate `DropdownCard` path is removed, declarations have their own header, marked selections use trailing state dots, and filter menus use a plain mode. Content Browser now delegates its deferred tooltip timing, placement, wrapping, appearance, and recording to the shared component mechanism. Scene Directory and Texture Paint now share sliding-page state, stable row identity, selection, visible-tree occupancy, and domain-specific drop policies. Control Centre travel and SVG generation remain planned.

## Cleanup completed before this plan

`InterfaceValidationHost` no longer includes, constructs, advances, records, or resets:

- `ControlCentrePanel`;
- `ContentBrowserPanel` (the Asset Browser drawer implementation);
- `LayerStackPanel`;
- the former tab-summoned fullscreen inspector/outliner/texture-stack prototype.

The actual runtime implementations were preserved:

- `EditorHost` and `PaintHost` still own and use `ControlCentrePanel` and `ContentBrowserPanel`;
- `EditorHost` still owns and uses `TexturePaintPanel`;

The unused `GlobalShellPanel` source was removed because no runtime host used it and its required behaviour already has standing owners. Interface Validation now concentrates on reusable controls, tooltips, menus, tree rows, revision rows, facets, and the existing editor-panel fixture.

## Current module inventory

### Foundations and rendering seams

| Module | Current responsibility | Keep / cleanup direction |
|---|---|---|
| `InterfaceExchange` | Device-facing interface lifetime, input, windows, docking, and frame recording seam | Keep low-level; never add domain widgets here. |
| `RecordingSurface` | Draw primitives, clipping, text, images, layers, and pointer/display samples | Keep as the only rendering vocabulary consumed by components. |
| `ControlIndex` | Stable identities, hover/taken/disclosure state, capture, and arbitration | Keep; split no visual styling into it. |
| `MotionIntegrator` | Springs and eased interpolants | Keep; carousel/fold components reference declared motion profiles instead of reimplementing time arithmetic. |
| `GestureSequence` | Contact phases, travel, and gesture tolerance | Keep; make tree/list dragging consume it instead of duplicating thresholds. |
| `RedrawScheduler` | Quiet/recolour/rearrange/rerecord invalidation | Keep. |
| `RasterCodec` | Raster loading/encoding support for UI imagery | Keep separate from controls. |

### Theme, typography, and symbols

| Module | Current responsibility | Keep / cleanup direction |
|---|---|---|
| `AppearanceSpecification` | `ThemeProfile`, colour roles, metrics, typography, corners, and layout | Remains the single resolved theme source. Break the 900-line public declaration into coherent theme-role headers later. |
| `ThemeSpecification` | Theme/accent declarations and theme resolution | Keep. |
| `ThemeInterchange` | Persist and recover theme selections | Keep outside widgets. |
| `ThemeReceiver` | A virtual theme receiver that currently has no evident standing implementation | Audit and remove if still unused after migrations; do not build a new inheritance hierarchy around it. |
| `SymbolSpecification` | Shared icon geometry and semantic symbol names | Keep one roster. |
| `TextComponent` | Typography samples and controls | Keep, but separate showcase declarations from reusable text layout. |
| `FontLoader` | Font discovery, atlas preparation, and loading | Keep as infrastructure, not panel state. |

### Base controls presently exposed by `ComponentSpecification`

- card enclosure;
- selection field and its deferred option menu;
- reusable editable text field;
- numeric magnitude field with expression input;
- XYZ/vector numeric row;
- rotation ruler;
- switch track and toggle row;
- subset/check row;
- magnitude stop selector;
- tooltip trigger and deferred tooltip card.

`ComponentSpecification.cpp` is currently about 2,250 lines. These are valid reusable controls, but the implementation has become a single compilation and ownership bucket.

### Additional controls presently exposed by `ControlPanel`

- switch toggle;
- segmented selection;
- tab strip;
- two-page carousel sample;
- collapsible card;
- legacy dropdown card;
- colour picker;
- outline disclosure animation and outline row;
- revision row.

There is substantial overlap with `ComponentSpecification`, especially switches and dropdowns. `ControlPanel` should not remain a second all-purpose component hierarchy.

### Composition and shell mechanisms

| Module | Current responsibility | Keep / cleanup direction |
|---|---|---|
| `DrawerSpace` | North/south drawers, springs, tongues, and capture exclusions | Keep as composition infrastructure. |
| `WorkspacePanel` | Workspace chrome and vacant state | Keep. |
| `WorkspaceIndex` | Caller-owned workspace roster | Keep model separate from presentation. |
| `PanelStructure` | Split-tree data for editor leaves | Keep as composition model. |
| `EditorPanel` / `EditorLeafPanels` | Split panels, leaf headers/footers, menus, and resizing | Keep, then separate leaf chrome from domain footer actions. |
| `ViewportSequence` | Assembles interface, theme, drawers, and frame sequencing | Keep as host-level composition, not a widget library. |

### Reusable or semi-reusable panels

| Module | Current responsibility | Direction |
|---|---|---|
| `FacetPanel` | Coloured filter facets and filter dropdown | Retain; migrate its dropdown to the shared plain-filter mode. |
| `ContentBrowserPanel` | Sources, search, asset lattice, inspector, scrolling | Runtime panel used by `EditorHost` and `PaintHost`; retain and migrate its internal repeated controls. |
| `ControlCentrePanel` | Settings pages, theme controls, sliders, font rails, page travel | Runtime panel used by `EditorHost` and `PaintHost`; retain, but replace private carousel/slider implementations with shared mechanisms. |
| `SceneDirectoryPanel` | Scene tree, selection, details, camera bookmarks, transfer pages, sky/gizmo UI | Standing editor implementation; retain. |
| `TexturePaintPanel` | Texture layer tree, selection, masks, property cards, export pages | Standing editor texture-stack implementation; retain. |
| `LayerStackPanel` | Former validation-only layer tree, property panels, revisions, and operations | Removed as dead code after reference scans confirmed that no runtime host or proof owned it. |

### Domain data specifications

| Module | Current responsibility | Direction |
|---|---|---|
| `SceneDirectorySpecification` | Entity rows, entity kinds, camera roles, environment UI state | Keep domain-specific. |
| `TexturePaintSpecification` | Texture-layer rows, channels, generators, and requests | Keep domain-specific. |
| `LayerStackSpecification` | Former validation-only layer arrangement and snapshot model | Removed with its sole consumer, `LayerStackPanel`. This is unrelated to the document-level revision service. |
| `ShortcutSpecification` | Shortcut presets and chords | Keep independent of visual components. |

## Main duplication found

### 1. Sliding pages and carousels

At least four independent forms exist:

- `ControlPanel::CarouselPages`;
- `ControlCentrePanel` page travel and font rails;
- `SceneDirectoryPanel` directory/details and transfer-format travel;
- `TexturePaintPanel` stack/properties/export travel.

They separately retain current page, previous page, direction, eased motion, clipping, and translated extents. This should become one mechanism with caller-provided page drawing.

### 2. History and revision presentation

- `ControlPanel::RevisionRow` is a small view primitive.
- The removed validation-only Layer Stack owned a complete revision pane, editable cards, and a private arrangement snapshot ring.
- Scene Directory contains remnants and comments from its removed revision feed.

History presentation, undo storage, and domain mutations are different concerns and should not be one panel feature.

### 3. Trees, folders, and entry operations

`SceneDirectoryPanel`, `TexturePaintPanel`, and the removed validation-only Layer Stack each implemented combinations of:

- flattened tree traversal;
- disclosure animation;
- selected sets and range anchors;
- subtree extent calculation;
- drag placement;
- moving into folders;
- add/remove/duplicate/group operations;
- inline rename and text focus;
- filtering and visible-row derivation;
- row icons, presence, locks, masks, and domain badges.

The visual and interaction mechanics overlap. The domain rules do not. Scene dragging remains parenting/folder placement, while texture-layer movement remains ordered compositing. A shared tree must therefore emit intent and call a domain policy; it must not force both domains into one data model.

### 4. Dropdowns

Two principal implementations exist:

- `ComponentSpecification::SelectionField`: the desired compact field shape, colours, background, caption/value layout, and chevron cell;
- `ControlPanel::DropdownCard`: the shading-style option behaviour with a visible selected marker/dot.

Several panels then add their own popup and filtering variants.

### 5. Tooltips

`ComponentSpecification` already has a deferred tooltip component with wrapping, light/dark variants, animation, and correct overlay ordering. Some large panels still carry local tooltip strings, coordinates, sizing, and drawing. Those local implementations should be removed after their call sites adopt the shared tooltip.

### 6. Themes and local visual constants

`ThemeProfile` is already the intended source, but large panels still contain local colour structures, copied metrics, and static pixel values. `ContentBrowserMetric`, `LayerStackMetric`, and `ShellMetric` overlap with global layout roles, while some panels copy theme colours through `Reapply` and others retain a borrowed `ThemeProfile`.

The result is not one reliable theme per component; it is a mixture of borrowed roles, copied roles, and private constants.

## UI scale, antialiasing, and vector symbols

### UI scale

Display DPI and artist-selected UI scale are separate inputs and multiply exactly once. The Control Centre's `UI Scaling` value must drive the resolved `ThemeProfile`; it must not remain a decorative percentage.

Rules:

- accepted artist range is 75–200%;
- every component metric, corner, icon extent, hit target, spacing, and text size derives from the resolved appearance;
- no panel reads the Control Centre configuration directly;
- panels that cache derived metrics must expose one explicit theme/metric reseat until those caches are removed;
- changing UI scale must not change document coordinates, viewport camera units, texture resolution, or render resolution;
- display DPI changes and artist scale changes use the same appearance-resolution path;
- Interface Validation needs 75%, 100%, 150%, and 200% layout proofs at narrow and wide extents.

The first implementation increment connects this preference through `ViewportSequence`; later cleanup removes local fixed pixels that still prevent complete scaling.

### Antialiasing

The existing Control Centre `Antialiasing` value is currently presentation-only and its page labels it as font antialiasing. It must be replaced by typed preferences with clear scopes:

1. **Interface geometry antialiasing** — lines and filled vector geometry recorded by the interface backend.
2. **Font rasterisation** — font atlas sampling/rasterisation profile; changing it triggers an atlas rebuild between frames.
3. **Viewport antialiasing** — TSAA/MSAA/postprocess policy owned by the renderer, not SlateUI.
4. **SVG tessellation quality** — curve flattening tolerance for generated or runtime vector geometry.

Do not use one integer named `Antialiasing` to control all four. The Control Centre may present them together, but each writes a typed preference to its owning subsystem. `None` must remain an explicit accessibility/performance choice, while the normal interface default uses antialiased lines and fills.

### SVG symbol library

`References/Icons.svg` is currently an HTML/JavaScript icon gallery that emits 36 standalone 120×120 SVGs; it is not yet a directly consumable SVG sprite. `EngineContent/GraphicArchives` contains separate crest SVG assets. External-package SVGs are examples/vendor assets and must never be imported into Slate's symbol roster accidentally.

The SVG sources should be authoritative and generated C++ should be derived output:

```text
authored SVG packs + symbol manifest
                ↓
      build-time SVG normaliser
                ↓
 validated paths, transforms, paint layers, view boxes
                ↓
 generated C++ vector archive + diagnostics
                ↓
 SymbolSpecification lookup
                ↓
 RecordingSurface vector draw
```

#### Pack organisation

Keep one stable semantic `SymbolSubject` roster and organise authored artwork into packs rather than separate widget APIs:

- General and workspace;
- Navigation;
- CAD and drafting;
- Geometry and topology;
- Sculpting;
- Texturing and materials;
- Lighting and rendering;
- Animation and rigging;
- Physics and simulation;
- Audio and measurement;
- Layer/composition operations.

A subject has one semantic identity regardless of pack file, theme, or where it is drawn. Panels request `FolderClosed`, `RigidCollide`, or `ConstraintDimension`; they never request a filename or icon-set brand.

#### Appearance variants

Each symbol may provide named paint layers rather than three unrelated drawings:

- **Monotone** maps every visible layer to the requested foreground role;
- **Duotone** maps primary and secondary layers to two theme roles and retains declared opacity;
- **Coloured** maps semantic layers to theme-controlled colour roles, never hard-coded product colours unless the artwork explicitly requires them.

Stroke weight and nominal icon size are independent preferences. The generator preserves stroke/fill intent, joins, caps, fill rule, and view box, then normalises geometry into the engine's canonical coordinate space.

#### Conversion tool

Add a deterministic build tool, not hand transcription:

- accept individual SVG files or extracted gallery entries;
- reject scripts, external URLs, text, filters, unsupported masks, and unbounded recursion;
- expand transforms and `<use>` references;
- support path, line, polyline, polygon, circle, ellipse, and rounded rectangle primitives;
- preserve primary/secondary/semantic layer names from the manifest;
- flatten cubic/quadratic curves at a declared quality tolerance;
- emit generated C++ arrays, source hashes, bounds, and diagnostics;
- produce identical output on Windows and Linux;
- never rewrite the authored SVG;
- fail naming verification when two subjects or pack entries collide.

The generated archive should be split by pack so changing one CAD icon does not rebuild all interface artwork. A small index maps `SymbolSubject` to pack and generated entry.

#### Placeholder policy

Missing artwork must not silently draw a generic image glyph. During migration:

- explicitly declared temporary SVG artwork may render and is reported as temporary in the manifest;
- a subject with no SVG entry renders nothing in release builds and a diagnostic missing-symbol box only in validation/debug builds;
- call sites must not request `PlaceholderMark` as ordinary decoration;
- remove the current fallback substitution only after the manifest identifies every standing call site, so deleting a fallback cannot silently erase a required action;
- Interface Validation presents missing, monotone, duotone, coloured, disabled, hovered, and selected states.

This policy removes fake completeness while keeping missing artwork observable during development.

## Target component architecture

Use composition, not a base-widget inheritance tree:

```text
host-owned domain state
        ↓
domain panel / domain policy
        ↓
shared component mechanism + declaration + theme roles
        ↓
ControlIndex + MotionIntegrator + GestureSequence
        ↓
RecordingSurface
```

A component should have four clear inputs:

1. geometry;
2. declaration/data view;
3. caller-owned mutable state or emitted intent;
4. theme roles.

It should not own document data, know about EditorHost/PaintHost, or silently perform a domain mutation.

## Proposed shared components

### `SlidingPages`

Own only transition state and geometry:

- current and departing page;
- travel direction;
- eased motion identity;
- clipping and translated page extents;
- direct page selection and previous/next requests;
- interruption policy when a second page is requested mid-transition.

Use it for Scene Directory, Texture Paint, Control Centre, and transfer/export rails. The page contents remain callbacks or explicit record passes owned by each panel.

### `TreeView`

Shared view and interaction state:

- stable row IDs rather than storage positions;
- visible flattened run generated from parent relationships;
- animated disclosure affecting descendant occupancy and clipping;
- persistent multiselect set plus range anchor;
- keyboard/pointer focus;
- drop-before, drop-after, and drop-into intent;
- inline rename state using the reusable editable text field;
- empty state, filtering, scrolling, and shared tooltip declarations.

Domain policies decide whether each intent is legal:

- `SceneTreePolicy`: reparent entities or add them to folders; no arbitrary compositing reorder semantics.
- `TextureStackPolicy`: reorder layers and subtrees, move into folders, preserve compositing order, and enforce mask rules.

### `RevisionList`

Presentation only:

- revision rows;
- optional details disclosure;
- scrolling;
- selection;
- tooltip support.

Undo/redo remains a domain service. Do not make one generic snapshot type large enough to hold every domain.

### `Dropdown`

One interaction implementation and one field appearance, with explicit presentation modes:

1. **Marked selection** — use the current Selection Field head/background/colours and the shading dropdown’s option behaviour. Each option shows `entry name` plus a trailing dot/marker whose colour indicates selected versus unselected.
2. **Plain filter** — same head, menu, hover, keyboard, dismissal, and animation, but no selection marker because filters do not require one.

Both modes share:

- outside-click and Escape dismissal;
- keyboard traversal and acceptance;
- deferred recording;
- compact-width support;
- one selected value model;
- disabled/empty states;
- theme roles.

After migration, remove `DropdownCard`; do not retain two dropdown implementations under different styling classes.

### `Tooltip`

Keep the existing deferred tooltip implementation, then expose a small declaration usable by icon buttons, tree rows, truncated labels, disabled controls, and menu entries. Panels supply text and anchor only. Tooltip placement, wrapping, timing, and drawing stay shared.

### `EditableField`

Retain the existing reusable editor and expression parser. Add adapters rather than copies for:

- ordinary text;
- names and paths;
- numeric values;
- XYZ groups;
- rename-in-place.

Keyboard ownership and selection highlighting remain in one implementation.

### Smaller visual mechanisms

- `Switch` — merge the duplicate switch/toggle drawing paths.
- `DisclosureCard` — one animated body-clipping implementation.
- `SearchField` — editable field plus clear/focus shortcut and filter intent.
- `ScrollBar` — one geometry and drag implementation.
- `IconButton` / `PillButton` — one hit, disabled, hover, tooltip, and theme path.
- `SelectionSet` — shared persistent membership and anchor mechanics, independent of tree storage.

## Theme cleanup

Each shared component gets one role bundle inside the resolved theme, for example:

- `DropdownTheme`;
- `TreeTheme`;
- `TooltipTheme`;
- `FieldTheme`;
- `CarouselMotionTheme`;
- `PanelChromeTheme`.

Rules:

1. No component defines product-specific literal colours in its source.
2. Domain panels may select semantic accents but do not restate component geometry.
3. Prefer borrowing the current `ThemeProfile`; use `Reapply` only where a component must cache derived values and document why.
4. Metrics are resolved once from display scale, artist scale, and density.
5. Component state contains no theme values.
6. Validation renders every component in every supported theme/density state.

## Header strategy

Separate headers are a good idea **when each header represents one coherent reusable component**. One header per individual helper function would make navigation and compilation worse.

Recommended structure:

```text
Interface/Components/
  Dropdown/Api/Dropdown.h
  EditableField/Api/EditableField.h
  Tooltip/Api/Tooltip.h
  TreeView/Api/TreeView.h
  SlidingPages/Api/SlidingPages.h
  DisclosureCard/Api/DisclosureCard.h
  SelectionSet/Api/SelectionSet.h
```

Each public header should contain declarations, small data views, and results only. Put drawing and interaction code in its source. Keep domain models under their panels/specifications. During migration, preserve forwarding includes briefly so hosts can move one call site at a time; remove those forwarding paths at the end.

Do not create class inheritance such as `Widget → SelectableWidget → AnimatedSelectableWidget → Dropdown`. These controls share services through composition, not identity.

## Interface Validation after cleanup

Interface Validation should become a component catalogue and behavioural gate, not a second application. Organise it into sections:

1. fields: text, numeric expression, vector, ruler;
2. selection: marked dropdown, plain filter dropdown, segments, stops;
3. toggles: switch, subset/check row;
4. overlays: menus and shared tooltips;
5. structure: disclosure card, tree view, multiselect, rename, drop placement;
6. navigation: tabs and sliding pages;
7. history presentation: revision list only;
8. colour and typography controls;
9. responsive, density, theme, keyboard, and disabled-state matrix.

Do not seed full content-browser libraries, full texture stacks, full scene directories, or settings applications in this host. Runtime panels get focused proof harnesses of their own.

## Migration order

### Phase 1 — freeze and prove

- Add focused before rasters for dropdowns, the Scene Directory tree, the Texture Paint tree, and each carousel.
- Add interaction assertions for selection, rename, disclosure, drag, outside-click dismissal, Escape, and interrupted carousel travel.
- Record current control-capacity and motion-capacity demand per panel.

### Phase 2 — scale and preference plumbing

- Connect UI Scaling to `ViewportSequence` and the resolved appearance.
- Inventory and remove local unscaled pixels panel by panel.
- Split interface geometry, font, viewport, and SVG antialiasing preferences.
- Route each typed preference to its owning subsystem.
- Add scale and antialiasing validation matrices.

### Phase 3 — dropdown and tooltip consolidation

- Introduce the shared Dropdown with marked-selection and plain-filter modes.
- Match the existing Selection Field shape and colours.
- Match shading option markers and selected/unselected dot colours.
- Migrate Facet filtering first, then Scene Directory, Texture Paint, EditorPanel, Content Browser, and Control Centre.
- Replace local panel tooltip drawing with the existing shared Tooltip. Content Browser migration complete.
- Delete `DropdownCard` only after no call site remains.

### Phase 4 — sliding pages

- Introduce `SlidingPages` with deterministic geometry tests. Complete.
- Migrate the smallest two-page inspector sample first. Complete: `ControlPanel::CarouselPages` now delegates page placement.
- Migrate Scene Directory and Texture Paint. Complete: both now use one shared strip state, eased travel, and page placement.
- Migrate Control Centre page travel and its specialised rails last.
- Remove private previous/current/direction/motion implementations after each owner is migrated. Complete for Scene Directory and Texture Paint.

### Phase 5 — shared selection and tree mechanics

- Extract stable row IDs, `SelectionSet`, visible-tree traversal, and disclosure occupancy. Complete in `TreeMechanics`.
- Migrate Scene Directory using `SceneTreePolicy` without changing its parenting-only drag rule. Complete.
- Migrate Texture Paint using `TextureStackPolicy` without weakening compositing-order and mask rules. Complete.

### Phase 6 — history separation

- Separate `RevisionList` presentation from `RevisionSequence` storage.
- Keep domain-specific undo state outside UI panels.
- Remove stale Scene Directory revision remnants.
- Reuse one revision row/card visual implementation where history is actually required.

### Phase 7 — SVG symbol generation

- Extract the 36 authored gallery entries into deterministic standalone SVG sources and a semantic manifest.
- Implement the normaliser and generated pack output with source hashes and diagnostics.
- Add monotone first, then duotone and coloured layer mapping.
- Migrate standing symbol call sites pack by pack.
- Remove ordinary `PlaceholderMark` requests and then disable fallback substitution.
- Connect icon appearance, weight, size, and SVG tessellation quality to typed preferences.

### Phase 8 — split giant files by responsibility

Priority sources currently exceeding roughly 2,000 lines:

1. `TexturePaintPanel.cpp`;
2. `SceneDirectoryPanel.cpp`;
3. `ComponentSpecification.cpp`.

Split by coherent responsibility after shared mechanisms exist. Do not merely move identical private code into several new files.

### Phase 9 — theme normalization and dead-code sweep

- Move remaining local colours and metrics into semantic theme roles.
- Remove obsolete copied structs and `Reapply` paths.
- Remove compatibility forwarding headers.
- Re-run symbol/reference scans and delete only modules with no runtime host, proof, or test owner.

## Acceptance gates for every migration

- Same runtime behaviour or an explicitly approved improvement.
- Actual-panel before and after rasters.
- Keyboard and pointer paths both tested where applicable.
- Persistent multiselect and range anchor preserved.
- Folder disclosure animates descendant occupancy, not just the chevron.
- Scene and texture drag policies remain distinct.
- No duplicate theme constants or dropdown implementation added.
- No increase in control/ease capacity without a written reason.
- Interface Validation contains reusable components only, not copied applications.
- Full naming verification and sandbox build pass.
