# Two-Dimensional CAD Completion Plan

## Goal

Drive the sketch / curve / profile editor to roughly ninety percent completion before returning to deeper three-dimensional work.

## Standing foundation

The CAD branch already carries:

- exact sketch primitive declarations;
- exact profile declarations;
- sketch point / control selection;
- sketch point / control edit verbs;
- loop-span selection groundwork;
- occurrence placement declarations;
- CPU-side topology and occurrence selection seams;
- profile resolution;
- exact extrusion intake.

## Remaining implementation order

### Batch A — authoring precision and primitive creation

1. Add a dedicated sketch snapping subsystem.
2. Add a mouse-driven primitive creation workflow.

#### Snap targets

- endpoint;
- midpoint;
- centre;
- control point;
- along curve.

#### Creation modes

- line;
- polyline;
- three-point arc;
- circle;
- ellipse / oval;
- regular polygon;
- slot;
- Bezier;
- basis spline;
- rational spline;
- Hermite.

### Batch B — core curve/profile editing

1. Implement two-dimensional boolean execution.
2. Implement two-dimensional fillet and chamfer.

### Batch C — profile restructuring

1. Implement trim and cut.
2. Implement join and open/close loop management.
3. Implement duplicate, mirror, array and inset.

### Batch D — CAD semantics

1. Add dimensions that can drive their targets.
2. Add real constraint solving.

### Batch E — bridge to three-dimensional work

1. Finalise open-chain vs closed-profile semantics.
2. Finalise thin-surface / solidify readiness.
3. Finalise loft-section readiness.

## Current implementation target

Implement Batch A first:

- snapping;
- mouse-driven primitive creation.

These are the biggest remaining two-dimensional usability gaps because the branch can already declare and edit exact sketch geometry, but a user still cannot author it naturally through one creation flow.
