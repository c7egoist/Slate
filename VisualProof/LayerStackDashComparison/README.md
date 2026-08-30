# Layer-stack mask-entry visual corrections

## Scope

This record preserves the reviewed raster changes and their implementation state. The review images were
rendered at 1280 × 900 by the real `TexturePaintPanel` through `SceneDirectoryProof`, using `master` at
`127846d` as the visual baseline. The approved dash, count-band, and mask-label corrections were applied
after the working branch was synchronized to `master` at `3eb240e`.

## Approved dash correction

The attached mask entry currently combines two incompatible axis treatments:

- its horizontal outline uses marks that are 1 px thick across the Y axis;
- its vertical colour rail uses marks that are 3 px thick across the X axis;
- the rail also uses a different on/off period from the outline.

The approved correction gives every dash a 1 px cross-axis thickness and one shared 2 px on / 2 px off
rhythm:

| Orientation | Along-axis length | Cross-axis thickness | Period |
|-------------|------------------:|---------------------:|-------:|
| Horizontal  | 2 px              | 1 px                 | 4 px   |
| Vertical    | 2 px              | 1 px                 | 4 px   |

The axes remain different geometrically: a horizontal mark is 2 × 1 px, while a vertical mark is
1 × 2 px. This preserves direction while making their visual weight uniform.

- Baseline: [`before.png`](before.png)
- Approved raster: [`after.png`](after.png)
- Implemented on `3eb240e`: [`implemented-3eb240e.png`](implemented-3eb240e.png)

### Applied implementation

`TexturePaintPanel::RecordMaskRow` now:

1. Reuses `DashOn`, `DashStep`, and `DashWeight` for the mask-entry colour rail.
2. Advances the rail along Y by `DashStep` and clamps each mark to `DashOn`.
3. Uses `DashWeight` as the rail's X extent instead of `Scaled.LayerTagX`.
4. Keeps the layer row's solid 3 px colour tag unchanged; only the attached mask entry is affected.

The obsolete separate rail rhythm was removed from `ShellMetric`, preventing the two treatments from
drifting apart again.

## Proposed mask label correction

The current label is rendered as the tracked uppercase run `M A S K`. That treatment competes with the
row's chips and makes an ordinary item name read like four separate initials.

The proposed replacement is natural title case, `Mask`, with ordinary tracking. It keeps the existing
font size, position, colour, selection tint, icon, source metadata, and interaction behaviour.

- Proposed raster: [`mask-label-title-case.png`](mask-label-title-case.png)

The implementation now uses an ordinary title-case text run with zero additional tracking. Its font size,
position, colour, selection tint, icon, source metadata, and interaction behaviour remain unchanged.

## Proposed count-band removal

The `12 · 7m` chip is a summary: `12` is the number of layer rows and `7m` means seven attached masks.
It occupies a separate band between the panel chrome and the search tools, even though the panel chrome
already identifies the layer stack and the rows visibly expose their masks.

The proposed correction removes the complete count band rather than merely hiding its text and leaving
an empty strip. The search and filter tools move upward to begin directly beneath the panel header, and
the recovered height is returned to the layer list.

- Proposed raster: [`without-count-band.png`](without-count-band.png)

The implementation now starts the tools directly at the leaf body's top edge. `RecordStackHeader`, its
count/solo presentation, and the retired header-only interaction identities were removed rather than
retained as dead code.

## Proposed material naming and flattened export

The Layer Stack panel header should present the material's editable name instead of a generic fixed
caption. The standing name should be `Material` until the artist enters another value. Taking the name
opens an inline text field in the same header; Enter or loss of focus accepts the edit, while Escape
restores the prior name.

`ComponentSpecification` now exposes the reusable `EditableText` control. It records through
`RecordingSurface`, keeps only the active interaction copy, and writes caller-owned text only when Enter
accepts the edit. The same editor is used by magnitude readouts, where a double contact accepts arithmetic
such as `10 exp 5`, `10^5`, `(2 + 3) * 4`, and `3.5 * 320` before applying the declared domain.

The material name is document data, not panel-local display state. It must therefore be owned by the
texture-paint document/host context and supplied to the panel. The export request must copy the accepted
name so a later rename cannot alter an export already in flight. Invalid file-system characters should be
replaced only when deriving the file name; the visible material name should remain exactly as authored.

A right-aligned pill button should stand in the footer with the label `Export Flattened`. Activating it
should composite all currently visible layers, masks, blend modes, channel values, and opacity into one
image without destructively replacing the editable layer stack. The export dialog should propose the
material name as the output file stem.

The operation needs a dedicated request crossing from the panel to the host; the panel must not perform
image composition or file I/O. The host should report cancellation or failure without changing the
stack. No source implementation has been applied yet.

## Reusable editable text field

The reusable field foundation now stands in `ComponentSpecification`. It is not specific to the
layer-stack header and is intended to support all of these uses:

- editing the material name in the Texture Paint leaf header;
- renaming layer-stack folders;
- renaming ordinary layer-stack entries;
- renaming scene-directory entities;
- future single-line naming and property fields without another private text-input implementation.

The component must own presentation and editing behaviour, but not document data. Its caller supplies the
standing run, writable capacity, placeholder, enabled condition, and validation policy. The component
reports accepted, cancelled, and still-editing outcomes; the appropriate document or host applies an
accepted rename.

The first implementation covers double-contact activation, insertion, Backspace, Delete, Home, End,
arrow-key caret movement, Enter acceptance, Escape cancellation, invalid-expression feedback, and a
caller-owned accepted value. Selection ranges, clipboard/IME input, keyboard rename activation, and
horizontal scrolling remain requirements for the later naming integration.

Required completed and planned interaction behaviour:

- pointer activation and keyboard activation for rename actions;
- caret placement, selection, insertion, Backspace, Delete, Home, End, and arrow navigation;
- Enter accepts, Escape cancels, and configurable focus loss accepts or cancels;
- horizontal text scrolling when the run exceeds the visible field;
- empty-name and maximum-length handling declared by the caller;
- one active text editor at a time across the interface;
- IME and clipboard input routed through the interface exchange rather than panel-specific typed buffers;
- visible focus, selection, caret, disabled, invalid, and truncated states;
- stable control identity so moving a field between header, row, and property layouts does not lose its
  editing state.

The existing search fields demonstrate pointer capture and typed-run routing, but they are filter-specific
primitive recordings rather than a reusable text editor. Their useful behaviour should be moved behind
the shared component and the search fields should then consume the same component, proving reuse.

## Required reusable panel header and footer

`EditorPanel` already records the leaf header and footer, but they are private, fixed procedures rather
than standalone reusable components. The current header is centred on panel-subject selection, division,
and withdrawal. The current footer is centred on viewport/grid and related popup menus. They can be
extended, but texture-paint controls must not be hardcoded directly into those procedures.

The header and footer should become reusable chrome components with declarative composition slots. Each
slot can receive reusable controls such as text fields, pill buttons, icon buttons, readouts, separators,
and menu anchors. At minimum, both chrome surfaces need:

- leading, centre, and trailing placement slots;
- left-to-right and right-to-left packing with spacing and clipping;
- priority-based withdrawal when a leaf becomes narrow;
- reusable icon-button, pill-button, text-field, readout, separator, and menu-anchor declarations;
- stable interaction identities supplied by the owning leaf;
- disabled, hovered, pressed, focused, and disclosed states;
- tooltips and accessible labels independent of visible compact text;
- deferred menu recording so leaf content cannot paint over a popup;
- per-subject declarations without dependencies on Texture Paint, Scene Directory, or viewport data;
- preservation of the existing subject, divide, split, withdraw, grid, shading, and workspace actions.

The Texture Paint leaf would declare an editable material-name field in the header's naming slot and an
`Export Flattened` pill in the footer's trailing action slot. Layer rows and scene-directory rows would
reuse the same editable text-field component in their own layouts. Other leaves retain their current
chrome declarations unchanged.

The panel content reports intent through request slots. The reusable chrome only records controls and
returns interaction outcomes; it must not rename documents, flatten images, write files, split panel
structures, or own workspace state. No source implementation has been applied yet.
