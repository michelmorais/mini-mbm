# Mesh v11 Binary Format — v1 (Milestone 0 closed 2026-06-25)

Companion to `docs/mesh-v11-plan.md`, Milestone 0. This layout is locked: the four open questions
below are resolved, so field names/sizes here are final for the v11.0 implementation (milestones
1-5). Anything still genuinely open (per-blob compression default, thread pool ownership, class
naming) lives in `mesh-v11-plan.md`'s Open Questions and belongs to later milestones, not this one.

Serialization style follows the existing `mesh-v8-io.cpp` convention: every multi-byte field is
read/written field-by-field through explicit little-endian helpers, never struct-blitted. That
choice is already proven 32/64-bit-safe in this codebase and carries forward unchanged.

## 1. Why a new envelope, not just a new version number

The current `util::HEADER` (`header-mesh.h:379`) puts `magic` 36 bytes into the file, behind two
16-byte string fields (`name`, `typeApp`). A v11 reader would have to trust those string fields are
even meaningful before it can find the byte that tells it "this is not a file I understand." v11
flips that: the file's own type/version self-identifies in the first handful of bytes, before
anything else is trusted.

## 2. Fixed file header

```cpp
struct FILE_HEADER_V11
{
    char     magic[4];        // ASCII "MBM1" — distinct from any legacy magic, checked first, always
    uint16_t formatVersion;   // 11. Independent of `magic` so a future v11.1 layout tweak can bump
                               // this without changing the magic bytes.
    uint8_t  typeMesh;        // util::TYPE_MESH value directly (no more typeApp string matching /
                               // get_type_app_from_mesh_type table)
    uint8_t  reserved0;       // must be 0
    int32_t  backBufferWidth; // kept: editor/authoring-time coordinate reference
    int32_t  backBufferHeight;
    uint32_t sectionCount;    // number of SECTION_HEADER_V11 blocks that follow, back-to-back
};
// 20 bytes, fixed size, no dependency on any later section.
```

Dropping the 16+16 byte `name`/`typeApp` strings and the string-comparison dispatch
(`get_type_app_from_mesh_type` / `get_mesh_type_from_type_app` / `MBM_TYPE_APP_*` constants) removes
a whole category of "what if the string doesn't match exactly" failure mode — `typeMesh` is just the
enum value, checked once.

## 3. Section envelope (TLV)

Every section, known or not, has the same fixed-size envelope so an unknown `type` can always be
skipped by seeking `compressedLength` bytes forward — no need to even understand or decompress it.

```cpp
struct SECTION_HEADER_V11
{
    uint16_t type;               // SECTION_TYPE
    uint16_t sectionVersion;     // per-section-type version; lets one section's payload evolve
                                  // without bumping formatVersion/magic for everything else
    uint8_t  compression;        // 0 = NONE, 1 = DEFLATE
    uint8_t  reserved1[3];       // must be 0
    uint32_t uncompressedLength;
    uint32_t compressedLength;   // == uncompressedLength when compression == NONE
    uint32_t crc32Value;          // mz_crc32() of the *uncompressed* payload, wrapped as
                                  // mbm::crc32Buffer() in miniz-wrap.h — no new dependency. Named
                                  // crc32Value, not crc32: miniz.h #defines crc32 to mz_crc32 for
                                  // zlib-API compatibility, which mangles a field literally named
                                  // crc32 in any translation unit that includes both headers (found
                                  // while implementing milestone 1).
};
// 16 bytes, followed immediately by `compressedLength` bytes of payload.
```

`crc32Value` is a deliberate addition over the current format, which has no integrity check of its
own beyond whatever zlib/miniz validates implicitly during whole-file decompression. Since v11 makes
compression optional and per-blob, that implicit check goes away for uncompressed sections —
`crc32Value` replaces it cheaply and also catches truncated/corrupted files earlier and with a
clearer error than a downstream parse failure would.

## 4. Section types

```cpp
enum SECTION_TYPE : uint16_t
{
    SECTION_MATERIAL_TRANSFORM = 1,   // material + angle/pos + draw mode (replaces HEADER_MESH + INFO_DRAW_MODE)
    SECTION_ANIMATION          = 2,   // repeated: one per animation, in order, including its FX block
    SECTION_FRAME_STATIC       = 10,  // repeated: one per frame, in order
    SECTION_FRAME_SKINNED      = 11,  // reserved type id — no v11.0 writer ever emits this
    SECTION_DETAIL_PHYSICS     = 20,  // cube / sphere / cube-complex / triangle bounding volumes
    SECTION_DETAIL_FONT        = 21,
    SECTION_DETAIL_PARTICLE    = 22,
    SECTION_DETAIL_TILE        = 23,
    SECTION_EXTRA_PATHS        = 30,  // replaces legacy EXTRA_HEADER type==1 path-registration hint
};
```

Sections of repeated kinds (`SECTION_ANIMATION`, `SECTION_FRAME_STATIC`) appear back-to-back in
ascending index order; nothing in the envelope encodes "which animation/frame index is this," the
order in the file is the index, same as today. `SECTION_DETAIL_*` presence is implied by `typeMesh`
in the file header (a font mesh has `SECTION_DETAIL_FONT`, a particle mesh has
`SECTION_DETAIL_PARTICLE`, etc.) exactly like today's `DETAIL_MESH.type` dispatch
(`mesh-manager.cpp:905-998`), just moved into the TLV envelope instead of an inline tag.

`SECTION_FRAME_SKINNED` is reserved *now* (the numeric id is spoken for) specifically so that when
bones ship later, they get a new section type, not a new file-format version. No v11.0 code path
reads or writes it.

## 5. Variable-length strings — replacing fixed char buffers

Today: `nameTexture[64]` (63 usable chars), `nameAnimation[32]` (31 usable chars). Both are silent
truncation hazards for deep paths or long names. v11 replaces every path/name field with:

```cpp
// on disk: uint16_t length, then `length` bytes of UTF-8, no null terminator stored
```

This is strictly more flexible and, for the common case of short names, no larger on disk than the
fixed buffers were.

## 6. `SECTION_FRAME_STATIC` payload

```cpp
struct FRAME_HEADER_V11
{
    uint32_t totalSubset;
    uint32_t vertexCount;
    uint8_t  indexWidth;     // 16 or 32 — see §7
    uint8_t  hasNormal;      // bool
    uint8_t  hasUv;          // bool
    uint8_t  uvSource;       // 0 = OWN (this frame stores its own UVs)
                              // 1 = SHARED_WITH_FRAME_0 (reuse frame 0's UV array; replaces today's
                              //     HAS_TEX_FIRST_FRAME global mode with an explicit per-frame flag)
    uint32_t indexCount;     // in indices, not bytes
};
// then: vertexCount * VEC3 position
//       vertexCount * VEC3 normal      (only if hasNormal)
//       vertexCount * VEC2 uv          (only if hasUv and uvSource == OWN)
//       indexCount  * (uint16 or uint32, per indexWidth)
//       totalSubset * SUBSET_DESC_V11
```

Keeping the "share UV with frame 0" option (today's `HAS_TEX_FIRST_FRAME`) as an explicit per-frame
flag rather than dropping it: sprite/font/particle atlases very commonly share one UV layout across
every frame, so this is a real, still-useful space saving, not legacy cruft — it just becomes
explicit instead of an implicit file-wide mode.

```cpp
struct SUBSET_DESC_V11
{
    TEXTURE_REF_V11 primaryTexture;   // implicit role TEXTURE_ROLE_DIFFUSE, always present
    int32_t  vertexCount, vertexStart, indexStart, indexCount;
    uint8_t  alphaColor[4];
    uint16_t extraSlotCount;
    // followed by extraSlotCount * { uint8_t role; TEXTURE_REF_V11 texture; }
    // `role` is an mbm::TEXTURE_ROLE value (shader.h), restricted here to
    // TEXTURE_ROLE_NORMAL / _SPECULAR / _EMISSIVE / _MASK — see docs/mesh-v11-plan.md §2.
    // TEXTURE_ROLE_ANIMATION_EFFECT never appears here; it belongs to SECTION_ANIMATION's FX block.
};

struct TEXTURE_REF_V11
{
    uint8_t storage; // 0 = PATH_REFERENCE, 1 = EMBEDDED_COMPRESSED
    // PATH_REFERENCE:      length-prefixed path string (§5)
    // EMBEDDED_COMPRESSED: width, height, depth, channel, hasAlpha, alphaColor[3],
    //                      uncompressedLength, compressedLength, then compressed bytes
    //                      (same DEFLATE codec as today's embedded textures, via miniz)
};
```

`mbm::TEXTURE_ROLE` is reused by value, not re-encoded — see `docs/mesh-v11-plan.md`, "Scope Decision
2." This is the one piece of this proposal that reaches outside `header-mesh.h` into `shader.h`, on
purpose: one enum, one definition.

## 7. Index width (§6 `indexWidth`)

Per-frame, not per-file: `16` is the default a writer should choose unless the frame's vertex count
exceeds 65535 or the developer explicitly opted into 32-bit indices for that mesh. Most sprite/font/
particle frames are a handful of quads and stay at 16-bit; dense 3D frames can opt into 32-bit only
where actually needed, keeping the common case small.

## 8. What stays out of this proposal on purpose

- `SECTION_FRAME_SKINNED` payload layout — not designed yet, only the type id is reserved
  (`docs/mesh-v11-plan.md` milestone 9).
- Vertex quantization (compact normal/UV encodings) — listed as a future optimization in
  `mesh-v11-plan.md`, not part of the v11.0 layout lock.
- `SECTION_DETAIL_*` payload bytes are intentionally not redesigned here — they can keep today's
  v8 field layout (`DETAIL_HEADER_FONT_DISK_V8`, `STAGE_PARTICLE_DISK_V8`, `BTILE_*_DISK_V8`, the
  physics shape structs), just moved inside the new TLV envelope instead of the old inline
  `DETAIL_MESH.type` dispatch. No reason to redesign payloads that aren't part of the problems this
  break is solving.

## Milestone 0 Decisions (resolved 2026-06-25)

1. **Magic stays 4 bytes (`"MBM1"`), no build/tooling stamp.** Self-identification only needs to
   answer "do I understand this file," not "who/what produced it." `formatVersion` (uint16)
   immediately follows the magic, giving 6 bytes of self-description before anything size-dependent
   is trusted — that's enough. Provenance/build metadata, if ever wanted, belongs in a section
   payload (a future `SECTION_BUILD_INFO`) where it can evolve independently, not baked into the one
   fixed field every reader must hardcode forever.
2. **`crc32Value` is always written, for every section, regardless of compression.** It's one `mz_crc32`
   pass on top of I/O the loader is already paying for, and it buys a single uniform validation path
   in the reader instead of a "verify only if compressed" branch. It also catches truncation/
   corruption in *uncompressed* sections — exactly the gap left open once whole-file compression's
   implicit zlib check goes away.
3. **No explicit index field on repeated sections.** `SECTION_ANIMATION` / `SECTION_FRAME_STATIC`
   keep file-order-as-index, unchanged from today's format. No real use case for sparse/reordered
   animations or frames exists today. `sectionVersion` is the designed escape hatch — an explicit
   index can be added later as a new section version if that need ever materializes, without
   spending bytes on every file now for a hypothetical.
4. **`reserved0` / `reserved1` stay reserve-and-zero.** Same reasoning as #3: no concrete present use,
   and `sectionVersion` / new section types are the mechanism for adding fields later. Baking a guess
   (e.g. a compression-policy flag) into padding now risks locking in the wrong shape before there's a
   real requirement driving it.

These four, plus the `TEXTURE_ROLE`-mapping location decided in `docs/mesh-v11-plan.md` (Scope
Decision 4), close Milestone 0. Implementation (`mesh-v11-io.cpp`, milestone 1) can proceed against
this layout as written.
