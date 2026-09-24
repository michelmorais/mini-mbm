# Future Features

This file tracks intentional follow-up work that should not be mixed into an active backend delivery.

## Mesh simplification follow-up

The current [Mesh Simplification](mesh-simplification.md) workflow is complete for production use.
Further work should be driven by real assets encountered during normal project development rather
than delaying the delivered workflow.

### Coplanar-region follow-up

The Coplanar delivery is **complete through M8 / 7.289.0**, with its
[final acceptance audit](mesh-coplanar-acceptance.md). The items below are optional
improvements driven by real assets, not blockers for this delivery.

The conservative first implementation was delivered in 7.280.0; see
[Mesh Simplification](mesh-simplification.md#coplanar-modes-72800) and the
[design/validation record](mesh-coplanar-optimization-plan.md).

Certified retriangulation of eligible regions with up to 16 holes is delivered in
7.283.0, with every boundary segment retained. Optional follow-ups:

- approximate contour reduction and broader boundary coordination; exact collinear
  boundary coordination is delivered as an opt-in feature in 7.284.0;
- non-affine attribute constraints, approximate varying-normal certificates and
  deformation certificates; exact affine raw-normal fields on exactly planar
  generic regions are supported since 7.285.0;
- broader certificates for uncertain/complex contacts and acceleration for heavily
  overlapping neighborhoods; strict coplanar separation is delivered in 7.286.0,
  an indexed projected-box broad phase in 7.287.0, full-projection separation of
  inclined obstacles in 7.288.0, and strict 3D separation with outward-rounded
  projection intervals in 7.289.0;
- larger approximate tolerances, additional backend parity and visual diagnostics.

### Visual diagnostics

- add a Mesh Debug overlay for open boundaries, high-error regions, elongated faces, disconnected
  components, and inter-subset clearance protections;
- let the user focus or frame a reported region without stripping skeletal weights;
- provide a copyable diagnostic summary with stable frame/subset identifiers;
- keep diagnostic generation explicit or cached so idle editor frames do not rescan geometry.

### Performance and progress

- reuse or incrementally update spatial acceleration structures across collapse passes;
- reduce temporary allocations and repeated candidate/triangle set construction;
- add a broad phase for deformation-sample clearance checks and avoid redundant pose evaluations;
- measure Debug and Release behavior on high-density static, skeletal, and layered meshes;
- preserve the existing simplifier progress, cancellation and atomic publication when adding new
  processing stages; add checkpoints and measure cancellation latency inside expensive stages.

## Explicit blend-state API

`BLEND_DISABLE` is a legacy and misleading name. Its established behavior on DirectX 9,
DirectX 11, OpenGL ES, and Metal use the engine's default alpha composition
(`SRC_ALPHA`, `INV_SRC_ALPHA`), not disabled blending.

After the DirectX 11 backend delivery:

- introduce an explicit name such as `BLEND_ALPHA` for the current behavior;
- retain `BLEND_DISABLE` as a deprecated compatibility alias during migration;
- add a separately named opaque/no-blend mode if the engine needs true disabled blending;
- audit Lua constants, mesh serialization, editors, plugins, and shipped games before changing
  any public enum exposure;
- do not renumber the existing serialized blend values.
