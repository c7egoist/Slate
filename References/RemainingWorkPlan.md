# Remaining Work — the six items, planned

Written after measuring each item rather than trusting the summary table. Three of the six were
not what the table said they were, and one of those changes the order.

---

## What the survey changed

| # | Item | The table said | Measurement says |
|---|---|---|---|
| 1 | `MaterialLayerStackBridge.h` | 175 lines, delete it | 175 lines, 7 functions, **1 host + 1 gate** depend on it. As described. |
| 2 | Delete `PaintHost` / `ParametricSketchHost` | Step 11 | As described — both already at 0 definitions, so this is deletion, not extraction. |
| 3 | `Paint` → `Texture` sweep | 527 uses | **690**, and they are not one kind of thing. Needs a split. |
| 4 | Workplane tool + placement bug | Not started, design is mine | **Almost entirely built already.** See below. |
| 5 | `DeviceOffering` / `InterfaceAttachment` | Deferred | As described. |
| 6 | `UnitScale` boundary | Untested | As described — and it is the only item that can silently corrupt a drawing. |

### Item 4 is not a design task

`ApplyWorkplaneTool` is **already written** in `SketchInteraction`, complete with the screen-space
case (`ResolvePlacedWorkplane`), the catalogue write, the directory record and a sealed revision.
`ParametricToolSubject::Workplane` already exists as enumerator `1u`. The plane sign defect in
`ResolveViewportPlaneIntersection` was already fixed at step 10e.

**What is missing is one call.** `ApplyWorkplaneTool` is declared in the header, defined in the
source, and invoked from nowhere:

```
=== is ApplyWorkplaneTool CALLED? ===
Engine/SlateWorkspace/Discipline/SketchInteraction/Api/SketchInteraction.h:63:bool ApplyWorkplaneTool(
```

One hit — the declaration. The host calls `DriveDrawingWithModifiers` and never the workplane tool,
so selecting the tool does nothing. This is why the feature is invisible despite being built.

> **A function that nothing calls is indistinguishable from a function that does not work.** It
> compiles, it is covered by two proofs, and the artist cannot reach it. The gates never noticed
> because no gate asserts that a tool subject is *dispatched* — only that it exists.

### Item 3 is three different jobs wearing one name

690 hits, split by what breaking them would cost:

| Kind | Count | Risk |
|---|---|---|
| Identifiers (`TexturePaintApplied`, `PaintedContent`, …) | ~600 | Compiler catches every miss |
| Directories and files (`TexturePaintPanel/`, `MaterialPaintExchange/`, `PaintHost/`) | 9 paths | Include paths, `Module.toml`, gates |
| String literals (`"PaintStroke"`, `"Paint Layer"`, `"Paint"`) | ~40 | **Some are transaction labels and UI text** |

The literals were checked against the document format: `WorkspaceCodex` writes binary fields, not
names, so **no rename here can invalidate a saved document.** `"PaintStroke"` is a revision label
compared by string in three places — it must change in all three or none.

---

## Order, and why

Item 4 first: it is the only item a person can see, and the fix is one call site.
Then 6, because an untested 1000× factor is the only remaining item that can corrupt a drawing
silently. Then 1 and 2, which are deletions that shrink the surface the sweep has to cover. Then 3
last and mechanically, when there is least code left to rename. Item 5 stays deferred and I say so
plainly at the end rather than half-doing it.

| Step | Item | Work |
|---|---|---|
| 1 | 4 | Dispatch `ApplyWorkplaneTool` from the host; prove a placed plane is adopted and existing curves do not move |
| 2 | 6 | Put a claim either side of the metre/millimetre boundary; sabotage both |
| 3 | 1 | Delete `MaterialLayerStackBridge.h` into the unit that owns the transformation |
| 4 | 2 | Delete `PaintHost` and `ParametricSketchHost` |
| 5 | 3 | Sweep `Paint` → `Texture`: identifiers, then paths, then literals, each with a totality check |
| 6 | 5 | Stated as deferred, with the reason |

Every step ends the same way: 33/33 gates, `make sequence` clears the unit graph, and any new
claim is sabotaged before it is believed.
