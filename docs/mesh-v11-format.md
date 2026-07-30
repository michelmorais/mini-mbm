# Mesh v11 Binary Format

Reference for the v11 mesh binary layout used by `core_mbm`. The format is fully implemented and
locked. §8 lists what is deliberately out of scope for v11.0; `## Future Work` below covers
backlogged items that do not change the on-disk layout.

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
    SECTION_FRAME_SKINNED      = 11,  // bundled joint hierarchy, see Sec. 6e — diagnostic/editor
                                       // round-trip only, never consulted by rendering
    SECTION_ARTICULATED_PARTS  = 12,  // optional rigid-part identities, pivots, and future hierarchy metadata
    SECTION_ARTICULATED_ANIMATION = 13, // optional rigid/articulated animation clips and tracks
    SECTION_DETAIL_PHYSICS     = 20,  // cube / sphere / cube-complex / triangle bounding volumes
    SECTION_DETAIL_FONT        = 21,
    SECTION_DETAIL_PARTICLE    = 22,
    SECTION_DETAIL_TILE        = 23,
    SECTION_EXTRA_PATHS        = 30,  // replaces legacy EXTRA_HEADER type==1 path-registration hint
    SECTION_VERTEX_SKIN_WEIGHTS = 40, // bundled per-vertex bone weight palette, see Sec. 6g —
                                       // editor/diagnostic + FBX re-export round-trip only, same
                                       // scope as SECTION_FRAME_SKINNED (no GPU/CPU skinning
                                       // consumer exists in this engine)
};
```

Note on unrecognized section types: despite what an earlier draft of this doc claimed, an
unrecognized `type` is **not** actually a safe no-op for every reader — only the lightweight
`MESH_MBM_DEBUG::getInfo` file-probe genuinely skips unknown types. Both real content loaders
(`parse_v11_intermediate`, the shared runtime/`MESH_MBM` path, and `MESH_MBM_DEBUG::loadV11`, the
editor path) hard-fail on a `type` they don't have an explicit branch for. Every section type above
therefore needs explicit (if only parse-and-discard) handling in both of those functions before it's
safe to write to disk — see `SECTION_VERTEX_SKIN_WEIGHTS`'s own rollout in Sec. 6g for the concrete
pattern this implies (a shared parse function, one real consumer, one "parsed but intentionally
unused" consumer).

Sections of repeated kinds (`SECTION_ANIMATION`, `SECTION_FRAME_STATIC`) appear back-to-back in
ascending index order; nothing in the envelope encodes "which animation/frame index is this," the
order in the file is the index, same as today.

`SECTION_DETAIL_PHYSICS` is the one section every mesh type gets — the writer always emits it (a
mesh with no explicit bounding shapes gets one synthesized so the section is never missing), *not*
conditioned on `typeMesh`. Only `SECTION_DETAIL_FONT`/`_PARTICLE`/`_TILE` presence is implied by
`typeMesh` (a font mesh has `SECTION_DETAIL_FONT`, a particle mesh has `SECTION_DETAIL_PARTICLE`,
a tile map has `SECTION_DETAIL_TILE`, and no other mesh type has any of the three) — the same
`DETAIL_MESH.type` dispatch this replaces (`read_detail_mesh_section`, `mesh-manager.cpp`) only ever
carries physics bounding volumes now; FONT/PARTICLE/TILE detail data moved to their own top-level
sections in milestones 12/13, it's never nested inside `SECTION_DETAIL_PHYSICS`.

`SECTION_FRAME_SKINNED` (Sec. 6e) persists a joint hierarchy — one optional section per mesh,
independent of `SECTION_FRAME_STATIC` geometry (a mesh can have real frame data with no skeleton,
a skeleton with no special geometry origin, or both, e.g. a skeleton fitted onto a mesh imported
from elsewhere). It is not runtime skeletal animation — the engine has no GPU/CPU skinning
anywhere — purely a diagnostic round-trip mechanism for `editor/mesh_debug.lua`'s Bones node.

`SECTION_VERTEX_SKIN_WEIGHTS` (Sec. 6g) persists real per-vertex bone weights — also one optional
section per mesh, same "diagnostic/editor + FBX re-export round-trip only" scope as
`SECTION_FRAME_SKINNED`, not consumed by any renderer. Unlike the skeleton section, it's tied to a
specific `SECTION_FRAME_STATIC` frame's own vertex topology (frame 1, always) rather than being
independent of geometry — skin weights only mean anything relative to one specific vertex layout.

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
    uint8_t  alphaColor[4];           // byte 0: hasAlpha flag consumed by TEXTURE_MANAGER::load on
                                       // reload (forces the primary texture's alpha channel on/off);
                                       // bytes 1-3 are an unused legacy color-key remnant. The v11.0
                                       // writer has no real per-subset source for this today - it
                                       // always emits {1,0,0,0} (SUBSET_DEBUG carries no alpha-color
                                       // state), so every v11-saved subset currently reloads with its
                                       // primary texture's alpha channel forced on regardless of the
                                       // source texture. Not a legacy-compat field to preserve as-is;
                                       // a future milestone could wire a real per-subset value through
                                       // if that forced-on behavior ever needs to be an author choice.
    uint16_t extraSlotCount;
    // followed by extraSlotCount * { uint8_t role; TEXTURE_REF_V11 texture; }
    // `role` is an mbm::TEXTURE_ROLE value (shader.h), restricted here to
    // TEXTURE_ROLE_NORMAL / _SPECULAR / _EMISSIVE / _MASK.
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

`mbm::TEXTURE_ROLE` is reused by value, not re-encoded. This is the one piece of this format that
reaches into the runtime's `shader.h` — one enum, one definition, no parallel per-format copy.

## 6b. `SECTION_ANIMATION` payload

One `SECTION_ANIMATION` section per animation, in file order (no explicit index — see Milestone 0
Decision 3).

```cpp
struct ANIMATION_HEADER_V11
{
    // name: length-prefixed string (§5), replaces today's nameAnimation[32]
    int32_t  initialFrame;
    int32_t  finalFrame;
    float    timeBetweenFrame;
    int32_t  typeAnimation;
    uint16_t blendState;
    uint8_t  hasFx;          // bool - if 1, an FX_HEADER_V11 follows immediately
};

struct FX_HEADER_V11   // only on disk when ANIMATION_HEADER_V11.hasFx == 1
{
    int32_t blendOperation;
    uint8_t hasFxTexture;    // bool
    // fxTexture: TEXTURE_REF_V11 (§6), only if hasFxTexture - role TEXTURE_ROLE_ANIMATION_EFFECT
    uint8_t hasPS;           // bool
    // ps: SHADER_STEP_V11, only if hasPS
    uint8_t hasVS;           // bool
    // vs: SHADER_STEP_V11, only if hasVS
};

struct SHADER_STEP_V11
{
    // name: length-prefixed string (§5) - a shader name reference (built-in or custom), e.g.
    //       "transparent.ps". Resolved by name at load time, not embedded source.
    float    timeAnimation;
    int32_t  typeAnimation;  // 0-6, shader-level playback type
    uint16_t varCount;       // followed by varCount * SHADER_VAR_V11
};

struct SHADER_VAR_V11
{
    uint8_t typeVar;  // mbm::TYPE_VAR_SHADER (shader-var-cfg.h)
    float   min[4];
    float   max[4];
};
```

A second texture slot per shader step (legacy `lenTextureStage2`) is not modeled here — no real-world
file has ever been observed using it, and the v11.0 writer/reader reject it with a clear error rather
than silently dropping it if a future file ever does.

## 6c. `SECTION_DETAIL_PARTICLE` / `SECTION_DETAIL_FONT` payloads

One section of each, present only when the file's `typeMesh` matches (a particle mesh has exactly one
`SECTION_DETAIL_PARTICLE`, a font mesh exactly one `SECTION_DETAIL_FONT`) - unlike `SECTION_ANIMATION`,
these are not repeated per-item; all stages/letters are bundled into the single section, the same way
`SECTION_DETAIL_PHYSICS` already bundles multiple bounding shapes. Both keep today's v8 field layout
verbatim (per §8 below) aside from the name string, which uses the standard length-prefixed encoding
(§5) instead of the old fixed/length-prefixed-byte-buffer scheme.

```cpp
// SECTION_DETAIL_PARTICLE payload
struct
{
    uint16_t stageCount;
    // followed by stageCount STAGE_PARTICLE entries (util::STAGE_PARTICLE, header-mesh.h):
    //   minOffsetPosition, maxOffsetPosition, minDirection, maxDirection, minColor, maxColor (VEC3 each),
    //   minSpeed, maxSpeed, minTimeLife, maxTimeLife, minSizeParticle, maxSizeParticle, ariseTime,
    //   stageTime (float each), totalParticle (uint32_t), segmented, sizeMin2Max, revive, _operator,
    //   invert_red, invert_green, invert_blue, invert_alpha (uint8_t each) - same field order as
    //   today's writeStageParticleV8/readStageParticleV8.
};

// SECTION_DETAIL_FONT payload
struct FONT_DETAIL_HEADER_V11
{
    // name: length-prefixed string (§5) - replaces today's sizeNameFonte-prefixed buffer
    int16_t  spaceXCharacter;
    int16_t  spaceYCharacter;
    uint16_t heightLetter;
    uint16_t letterCount;    // followed by letterCount DETAIL_LETTER entries
};
// each DETAIL_LETTER entry (util::DETAIL_LETTER, header-mesh.h): letter (uint8_t, ascii code),
// indexFrame (uint8_t, frame index in SECTION_FRAME_STATIC), widthLetter, heightLetter (uint16_t
// each) - same field order as today's writeDetailLetterV8/readDetailLetterV8. Glyph *geometry* is
// not part of this payload - it already lives as ordinary frames in SECTION_FRAME_STATIC, indexed by
// indexFrame. `letterDiffX`/`letterDiffY` (runtime-only fields on INFO_BOUND_FONT) are never part of
// any on-disk format and are not written here either.
```

## 6d. `SECTION_DETAIL_TILE` payload

One section, present only for a `TYPE_MESH_TILE_MAP` file - same bundling convention as §6c. Reuses
today's v8 field layout for every struct that already had one (`BTILE_HEADER_MAP`, `BTILE_BRICK_INFO`,
`BTILE_INDEX_TILE`, `BTILE_OBJ`/`BTILE_PROPERTY` headers), with two deliberate deviations: the fixed
`background_texture[62]` buffer becomes a length-prefixed string (§5, same treatment every other v11
string field already got), and each layer gains a `float offset[3]` header that **the legacy v8
format never persisted at all** - confirmed by reading every legacy BTILE read/write function; the
editor (`tile_editor.cpp`) already populates and consumes this field at runtime and already has a
fallback for files where it's missing (`offset[2]` defaulting to an index-based stacking order), so a
fresh v11 writer/reader should persist it properly rather than carry the gap forward. Brick
*geometry* is not part of this payload either - each distinct brick is already one ordinary frame in
SECTION_FRAME_STATIC (`BTILE_HEADER_MAP.countRawTiles == ` total frame count), same relationship
font glyphs have to their frames.

```cpp
struct TILE_HEADER_MAP_V11
{
    uint32_t count_width_tile, count_height_tile;   // grid dimensions, in tiles
    uint32_t size_width_tile, size_height_tile;      // pixel size per tile
    uint32_t layerCount;
    uint32_t countRawTiles;     // followed by countRawTiles BTILE_BRICK_INFO entries
    uint32_t objectCount;       // derived from lsObj.size() at write time, not trusted from memory
    uint32_t propertyCount;     // derived from lsProperty.size() at write time
    uint32_t typeMap;           // util::BTILE_TYPE_MAP (orthogonal/isometric/staggered/hexagonal)
    uint32_t background;        // color, or 0
    // backgroundTexture: length-prefixed string (§5) - replaces today's background_texture[62]
    uint8_t  renderDirectionLeftToRight;
    uint8_t  renderDirectionTopToDown;
};
// followed by countRawTiles BTILE_BRICK_INFO entries (util::BTILE_BRICK_INFO, header-mesh.h):
//   index, original_index, rotation, flipped (uint16_t each) - same field order as today's
//   writeBtileBrickInfoV8/readBtileBrickInfoV8.

// then, for each of layerCount layers:
struct TILE_LAYER_HEADER_V11
{
    float offsetX, offsetY, offsetZ;   // see deviation note above - not in any legacy format
};
// followed by (count_width_tile * count_height_tile) BTILE_INDEX_TILE entries (util::BTILE_INDEX_TILE):
//   index (uint32_t, brick id for this cell), x, y (float, world position) - same field order as
//   today's writeBtileIndexTileV8/readBtileIndexTileV8.

// then, objectCount entries:
struct TILE_OBJ_HEADER_V11
{
    // name: length-prefixed string (§5)
    uint16_t type;        // util::BTILE_OBJ_TYPE (rect/circle/triangle/point/polyline)
    uint16_t pointCount;  // followed by pointCount raw (float x, float y) pairs
};

// then, propertyCount entries:
struct TILE_PROPERTY_V11
{
    // owner, name, value: length-prefixed strings (§5)
    uint16_t type;   // util::BTILE_PROPERTY_TYPE (bool/color/float/file/int/string)
};
```

## 6e. `SECTION_FRAME_SKINNED` payload

One optional section per mesh — present only when the editor (`editor/mesh_debug.lua`'s Bones
node, via `meshDebug:addBone(...)`) has explicitly added a skeleton. Independent of `typeMesh` and
independent of whether this mesh's `SECTION_FRAME_STATIC` geometry came from a hand-authored
skeleton or an ordinary Blender import — a skeleton can be fitted onto any existing mesh's geometry.
**Diagnostic/editor round-trip only — never consulted by rendering** (this engine has no GPU/CPU
skinning anywhere).

Bundled like `SECTION_DETAIL_TILE` (§6d): one section, an internal count prefix, followed by a
flat run of entries — not repeated-per-item like `SECTION_ANIMATION`, since there is exactly one
skeleton per file, not N.

`sectionVersion` (§3) distinguishes two on-disk layouts. **`1`** (legacy): only the first 6 fields
below (`name`..`radius`) are present — a reader defaults `rotX=rotY=rotZ=0`, `scaleX=scaleY=scaleZ=1`,
`length=0` (the struct's own constructor defaults). **`2`** (current writer, always emitted): all 13
fields are present. The two layouts share a byte-identical 6-field prefix — reading is "read 6
fields, then if `sectionVersion >= 2` read 7 more," nothing else branches.

```cpp
struct SKELETON_HEADER_V11
{
    uint16_t jointCount;  // count of SKELETON_BONE_V11 entries that follow, in parent-before-child order
};

// then, jointCount entries:
struct SKELETON_BONE_V11
{
    // name, parentName: length-prefixed strings (§5). parentName == "" marks the root bone;
    // otherwise it must equal the `name` of a SKELETON_BONE_V11 already emitted earlier in this
    // same section (root-first order) — a reader rejects the file if a parentName doesn't resolve
    // to an already-seen bone, rather than silently accepting a dangling/forward reference.
    float x, y, z;   // bone position, same coordinate convention as the caller's mesh
    float radius;    // authoring-time bone radius (envelope/gizmo marker size) — orthogonal to
                      // rotation/scale/length below

    // sectionVersion 2 only:
    float rotX, rotY, rotZ;      // bone orientation, Euler degrees, same non-parent-relative
                                  // world/armature-space convention as x,y,z above. Engine's own
                                  // X-then-Y-then-Z composition order (MatrixRotationX/Y/Z,
                                  // src/core_mbm/primitives.cpp), matching editor/mesh_debug.lua's
                                  // rotateX/Y/Z and MESH_MBM_DEBUG::rotateFrame exactly.
    float scaleX, scaleY, scaleZ; // bone-local scale, default 1,1,1
    float length;                 // bone extent along its own local +Y axis (Blender's own
                                  // head→tail convention): tail = head + Rotate(rotX,rotY,rotZ)
                                  // applied to Vector(0, length, 0). `0` means "no orientation
                                  // data available" (a sectionVersion 1 file, or a synthesized/
                                  // hand-authored bone with no Blender-import provenance) —
                                  // consumers needing a tail direction should fall back to
                                  // inferring it from position topology in that case, not trust
                                  // rotX/Y/Z.
};
```

Same "no explicit index field" convention as every other repeated/bundled section (§4, Milestone 0
Decision 3, §8 below): bone identity is by `name`, not by array position.

## 6f. `SECTION_ARTICULATED_PARTS` and `SECTION_ARTICULATED_ANIMATION` payloads

These are optional rigid/articulated-animation sections. They are omitted when the corresponding
data does not exist, so old meshes without them remain valid and continue through the existing
static/frame-animation path. Both the runtime and editor loaders parse them.

`SECTION_ARTICULATED_PARTS` is one bundled section with a `uint32_t partCount`, followed by that
many records:

```cpp
struct ARTICULATED_PART_V11
{
    uint64_t partId;
    uint32_t frameIndex;
    uint32_t subsetIndex;
    uint64_t parentPartId; // 0 means no parent; composition is reserved for a later milestone
    // name: length-prefixed string (§5)
    float pivotX, pivotY, pivotZ;
    float pivotQX, pivotQY, pivotQZ, pivotQW;
};
```

`SECTION_ARTICULATED_ANIMATION` is one bundled section with a `uint32_t clipCount`. Its current
`sectionVersion` is `2`; other versions are rejected. Each clip contains a length-prefixed name,
`float duration`, `float speed`, `int32_t defaultPriority`, a `uint8_t loop`, and a
`uint32_t trackCount`. Each track contains `uint64_t partId`, a `uint8_t` channel mask,
`uint32_t keyCount`, and key records. Keys store a `float time`, position (`x/y/z`), quaternion
rotation (`x/y/z/w`), authored Euler rotation in degrees (`x/y/z`), a `uint8_t hasRotationEuler`
flag, and scale (`x/y/z`). When the flag is present, runtime interpolation uses the authored Euler
values and converts the result to a quaternion.

## 6g. `SECTION_VERTEX_SKIN_WEIGHTS` payload

One optional section per mesh — present only when a Blender-imported source object had real
`vertex_groups` and `--include-bones` was set (`editor/blender_mesh_export.py`'s
`extract_vertex_skin_weights` capture pass), or when hand-set via
`meshDebug:setVertexWeight(...)`. **Diagnostic/editor + FBX re-export round-trip only — never
consulted by rendering** (same "no GPU/CPU skinning anywhere" scope as `SECTION_FRAME_SKINNED`,
§6e). Real motivation: `editor/mesh_debug.lua`'s "Export to FBX" previously had no choice but to
*invent* new weights from scratch via Blender's `ARMATURE_ENVELOPE` geometric approximation for
every export, because the format had nowhere to keep a mesh's own originally-authored weights past
import. This section is what closes that gap — when present, export uses it directly and skips
envelope binding entirely.

Bundled like `SECTION_FRAME_SKINNED` (§6e): one section, an internal count prefix, followed by a
flat run of entries. **Tied to `SECTION_FRAME_STATIC` frame 1's own vertex topology specifically**
(`vertexCount` below must equal frame 1's own `FRAME_HEADER_V11.vertexCount`) — skin weights are a
bind-pose property, they don't vary per animation frame (only bone *transforms* would, and this
engine doesn't apply those anywhere), so there is no reason to repeat this data per frame, and no
defined meaning for any frame other than frame 1.

Bones are referenced by a small per-section name **palette**, not by raw index into
`SECTION_FRAME_SKINNED`'s own bone array. This is deliberate: that array can be resorted/renamed/
have entries removed later (`MESH_MBM_DEBUG::updateBone`/`removeBone`/`addBone`), which would
silently invalidate a raw index but leaves a name-based reference either still correct or a clean
"unknown bone" lookup miss — never a silent wrong-bone reference.

`sectionVersion` is always `1` today (no legacy layout to branch on yet).

```cpp
struct VERTEX_SKIN_WEIGHTS_HEADER_V11
{
    uint32_t paletteCount;  // unique bone names referenced by any vertex
    uint32_t vertexCount;   // must equal SECTION_FRAME_STATIC frame 1's own vertexCount
};

// then, paletteCount length-prefixed strings (§5): the bone-name palette, in first-referenced order

// then, vertexCount entries (frame 1's own vertex order):
struct VERTEX_BONE_WEIGHT_V11
{
    uint8_t paletteIndex[4]; // index into the palette above; 0xFF = unused slot
    float   weight[4];       // unused slot weight = 0.0f; used slots should sum to ~1.0
                              // (not enforced on read — a caller that wrote unnormalized weights
                              // gets them back exactly as given)
};
```

Fixed at 4 influences per vertex — matches this codebase's own pre-existing convention
(`blender_mesh_skeleton_export.py`'s `vertex_group_limit_total(4)` + `vertex_group_normalize_all`,
already applied to its `ARMATURE_ENVELOPE` fallback weights before this section existed, and now
also applied on the *import* side when capturing real weights, for the same reason).

Size cost: 4×(1-byte index + 4-byte weight) = 20 bytes/vertex + a negligible one-time palette (tens
of short strings). Not amplified by triangle sharing — `SECTION_FRAME_STATIC` is an indexed buffer
(one entry per unique vertex, §6), so this is one weight entry per existing vertex entry, exactly
like normal/UV data already is; the only "duplication" that occurs is the same one position/normal/
UV already have at a genuine UV-seam or hard-normal edge, where one Blender vertex legitimately
becomes several separate entries in this format.

**Rollout note**: `parse_v11_intermediate` (the shared runtime/`MESH_MBM` load path) parses this
section into a scratch field of the shared intermediate struct that `finishLoadFromIntermediate`
never reads — the same "parsed but intentionally unused by `MESH_MBM`" pattern
`SECTION_FRAME_SKINNED` already established — purely so a game/runtime load of a mesh carrying this
section still succeeds. `MESH_MBM_DEBUG::loadV11` has its own separate read loop that actually
stores the data for editing/re-export. An *older*, already-compiled engine binary with no branch
for type `40` in either loader will still hard-fail on a file carrying this section (see the note
in §4) — accepted as consistent with `SECTION_FRAME_SKINNED`'s own original rollout, not treated as
a regression to fix retroactively.

## 7. Index width (§6 `indexWidth`)

Per-frame, not per-file: `16` is the default a writer should choose unless the frame's vertex count
exceeds 65535 or the developer explicitly opted into 32-bit indices for that mesh. Most sprite/font/
particle frames are a handful of quads and stay at 16-bit; dense 3D frames can opt into 32-bit only
where actually needed, keeping the common case small.

## 8. What stays out of this proposal on purpose

- Vertex quantization (compact normal/UV encodings) — future optimization, not part of the v11.0
  layout lock.
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

These four close Milestone 0. Implementation (`mesh-v11-io.cpp`, milestone 1) can proceed against
this layout as written.

## Future Work

Backlogged items that do not change the on-disk layout:

- **32-bit index support** (`indexWidth == 32` in `FRAME_HEADER_V11` §6): the format field is
  already defined and read/written correctly. The C++ implementation is all `uint16_t` throughout —
  GPU upload/draw (`GL_UNSIGNED_SHORT`/`D3DFMT_INDEX16`) and the entire in-memory editing API
  (`MESH_MBM_DEBUG::addVertex`/`addIndex`/`mergeBuffer`, ~30 sites in `mesh-manager.cpp`). All three
  layers must change together; the editing layer is the bulk of the work.

- **Shader-effect editor** (partially done): FX *texture* get/set is fully wired —
  `MESH_MBM_DEBUG::getAnimationEffectTexture`/`setAnimationEffectTexture`, Lua bindings
  `getFxTexture`/`setFxTexture` in `mesh-debug-lua.cpp`, and UI rows in both the Animations node
  and the Texture node's stage-1 branch in `mesh_debug.lua` (milestone 19).
  Still missing in the editor (no C++ methods on `MESH_MBM_DEBUG`, no Lua bindings, no UI):
  - PS/VS shader name (`INFO_FX::dataPS/dataVS->fileNameShader`, e.g. `"transparent.ps"`)
  - PS/VS animation type and time (`dataPS->typeAnimation`, `dataPS->timeAnimation`)
  - Blend operation (`INFO_FX::blendOperation`)
  - Shader vars (min/max per variable): var names are **not** stored in `INFO_SHADER_DATA` — only
    the runtime compiled `BASE_SHADER` knows them — so editing vars requires a "set shader name →
    compile → read uniform names → show named rows" workflow, making it the most complex piece.

- **Milestone 22 dynamic-test gap**: `renderizable:loadAsync()` Lua bindings were never exercised
  against a live engine — the implementing session's Xvfb/GL environment hung before any frame ran
  (confirmed pre-existing, not caused by those changes). With a working `DISPLAY=:1` setup, run:
  one genuinely-async case per renderizable type, a GC-safety test (drop all Lua refs +
  `collectgarbage()` before load completes), and a failure-path case (missing/wrong-type file). See
  `src/lua-wrap/render-table/*-lua.cpp` and `src/render/*.h`/`.cpp`.

- **`TEXTURE_MANAGER::loadAsync`**: no async primitive for plain texture loading. `PARTICLE`/
  `BACKGROUND`'s texture-only `loadAsync` sub-paths stay synchronous-but-callback-shaped by design.
  A real implementation would mirror `MESH_MANAGER::loadAsync`'s worker-thread decode + main-thread
  GPU-finish design.
