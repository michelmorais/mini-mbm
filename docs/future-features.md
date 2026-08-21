# Future Features

This file tracks intentional follow-up work that should not be mixed into an active backend delivery.

## Explicit blend-state API

`BLEND_DISABLE` is a legacy and misleading name. Its established behavior on DirectX 9,
OpenGL ES, and DirectX 11 is the engine's default alpha composition
(`SRC_ALPHA`, `INV_SRC_ALPHA`), not disabled blending.

After the DirectX 11 backend delivery:

- introduce an explicit name such as `BLEND_ALPHA` for the current behavior;
- retain `BLEND_DISABLE` as a deprecated compatibility alias during migration;
- add a separately named opaque/no-blend mode if the engine needs true disabled blending;
- audit Lua constants, mesh serialization, editors, plugins, and shipped games before changing
  any public enum exposure;
- do not renumber the existing serialized blend values.
