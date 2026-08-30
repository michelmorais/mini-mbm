# Future Features

This file tracks intentional follow-up work that should not be mixed into an active backend delivery.

## Mesh simplification follow-up

The current [Mesh Simplification](mesh-simplification.md) workflow is complete for production use.
Further work should be driven by real assets encountered during normal project development rather
than delaying the delivered workflow.

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
- expose progress and cancellation for long editor operations without allowing partial commits.

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
