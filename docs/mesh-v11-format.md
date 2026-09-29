# Mesh v11 Binary Format

Reference for the v11 mesh binary layout used by `core_mbm`. The format is fully implemented and
locked. §8 summarizes format invariants and implementation limits. Extensions are
tracked in [Future Features](future-features.md#mesh-format-and-loader-follow-up).

Serialization style follows the existing `mesh-v8-io.cpp` convention: every multi-byte field is
read/written field-by-field through explicit little-endian helpers, never struct-blitted. That
choice is already proven 32/64-bit-safe in this codebase and carries forward unchanged.

## 1. File identification

V11 identifies the file with its magic and version at the start of the fixed
header, before parsing variable data. Multi-byte fields use explicit little-endian
serialization; disk records do not depend on host pointer size or struct padding.

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
                                  // crc32 in any translation unit that includes both headers.
};
// 16 bytes, followed immediately by `compressedLength` bytes of payload.
```

`crc32Value` validates the uncompressed payload independently of compression.
Uncompressed sections receive the same integrity check as compressed sections;
loaders reject a checksum mismatch before interpreting payload fields.

## 4. Section types

```cpp
enum SECTION_TYPE : uint16_t
{
    SECTION_MATERIAL_TRANSFORM = 1,   // material + angle/pos + draw mode (replaces HEADER_MESH + INFO_DRAW_MODE)
    SECTION_ANIMATION          = 2,   // repeated: one per animation, in order, including its FX block
    SECTION_FRAME_STATIC       = 10,  // repeated: one per frame, in order
    SECTION_ARTICULATED_PARTS  = 12,  // optional rigid-part identities, pivots, and hierarchy metadata
    SECTION_ARTICULATED_ANIMATION = 13, // optional rigid/articulated animation clips and tracks
    SECTION_NORMAL_MAP_TANGENTS = 14, // optional prepared tangent batches, one section per frame
    SECTION_NORMAL_MAP_MATERIALS = 15, // optional per-frame/subset convention and strength
    SECTION_DETAIL_PHYSICS     = 20,  // cube / sphere / cube-complex / triangle bounding volumes
    SECTION_DETAIL_FONT        = 21,
    SECTION_DETAIL_PARTICLE    = 22,
    SECTION_DETAIL_TILE        = 23,
    SECTION_EXTRA_PATHS        = 30,  // replaces legacy EXTRA_HEADER type==1 path-registration hint
    SECTION_SKELETAL_SKELETON  = 41,
    SECTION_SKELETAL_WEIGHTS   = 42,
    SECTION_SKELETAL_ANIMATION = 43,
    SECTION_AUTOPLAY_ANIMATION = 44,
};
```

Numeric types **11** and **40** are retired exploratory skeletal payloads documented historically
in Secs. 6e and 6g. They are no longer `SECTION_TYPE` members, recognized by either loader, emitted
by the writer, or represented by public payload structs/APIs.

Note on unrecognized section types: despite what an earlier draft of this doc claimed, an
unrecognized `type` is **not** actually a safe no-op for every reader — only the lightweight
`MESH_MBM_DEBUG::getInfo` file-probe genuinely skips unknown types. Both real content loaders
(`parse_v11_intermediate`, the shared runtime/`MESH_MBM` path, and `MESH_MBM_DEBUG::loadV11`, the
editor path) hard-fail on a `type` they don't have an explicit branch for. Every section type above
therefore needs explicit handling in both of those functions before it's safe to write to disk. The
autoplay section is read by both loaders and retained in their runtime/editor mesh records.

Sections of repeated kinds (`SECTION_ANIMATION`, `SECTION_FRAME_STATIC`) appear back-to-back in
ascending index order; nothing in the envelope encodes "which animation/frame index is this," the
order in the file is the index, same as today.

`MESH_MBM_DEBUG::loadDebugFromMemory()` is a runtime/editor extraction path, not another v11 file
reader. On DirectX 11 it copies GPU vertex/index buffers into staging resources, maps them for CPU
read, deinterleaves position/normal/UV data, and preserves the runtime subset ranges. Indexed
frames recover `sizeVertexBuffer` from the single global `BUFFER_GL` vertex count; they must not
sum per-subset maximum indices because subsets may share the same global vertices. The automated
DX11 fixture compares the extracted Crate frame byte-for-byte with `MESH_MBM_DEBUG::loadV11()`.

`SECTION_DETAIL_PHYSICS` is the one section every mesh type gets — the writer always emits it (a
mesh with no explicit bounding shapes gets one synthesized so the section is never missing), *not*
conditioned on `typeMesh`. Only `SECTION_DETAIL_FONT`/`_PARTICLE`/`_TILE` presence is implied by
`typeMesh` (a font mesh has `SECTION_DETAIL_FONT`, a particle mesh has `SECTION_DETAIL_PARTICLE`,
a tile map has `SECTION_DETAIL_TILE`, and no other mesh type has any of the three) — the same
`DETAIL_MESH.type` dispatch this replaces (`read_detail_mesh_section`, `mesh-manager.cpp`) only ever
carries physics bounding volumes now; FONT/PARTICLE/TILE detail data moved to their own top-level
sections, it's never nested inside `SECTION_DETAIL_PHYSICS`.

Numeric section types 11 and 40 are unsupported and rejected. Runtime skeletal
animation uses canonical sections 41–43; type 42 is the supported skeletal-weight
representation.

### Canonical skeletal-runtime section types

The following values are present in `SECTION_TYPE`; all three have explicit read/validate support
in both real loaders, `MESH_MBM_DEBUG::saveV11` round-trips canonical data, and the FBX importer
produces them:

```cpp
SECTION_SKELETAL_SKELETON  = 41,
SECTION_SKELETAL_WEIGHTS   = 42,
SECTION_SKELETAL_ANIMATION = 43,
```

The skeleton section currently writes version 3 (versions 1 and 2 remain explicitly readable);
weights and animation remain version 1. They form the skeletal family consumed by runtime animation.

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
    uint8_t  indexWidth;     // supported value: 16 — see §7
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
//       indexCount  * uint16 (supported indexWidth == 16)
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
                                       // source texture.
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

### Optional `SECTION_NORMAL_MAP_TANGENTS` (14), section version 1

This optional section stores CPU-prepared normal-map geometry references without
changing `SECTION_FRAME_STATIC` or the author's vertex/index arrays. At most one
section may reference each zero-based frame index. Section order is independent
of the referenced frame's position in the file. Absence is valid.

The payload is written field-by-field, little endian:

```text
uint32 frameIndex
uint32 preparationRevision = 1
uint64 sourceSignature
uint32 unusableTriangleCount
uint32 batchCount                 // nonzero
repeat batchCount:
    uint32 subsetIndex            // zero-based; nondecreasing across batches
    uint32 vertexCount            // 1..65536
    uint32 indexCount             // nonzero, divisible by 3
    repeat vertexCount:
        uint32 sourceVertexIndex  // index into the original frame
        float32 tangentX, tangentY, tangentZ, tangentSign
    repeat indexCount:
        uint16 batchLocalIndex
```

The fixed payload prefix is 24 bytes. Each batch has a 12-byte prefix, 20 bytes
per vertex and two bytes per local index. A subset may span multiple batches to
stay within the 16-bit index limit. Triangles retain source draw order; strips and
fans are expanded with their winding preserved. Positions, normals, UVs and skin
influences remain in their existing source records; the source-vertex mapping
allows their later transfer to render buffers without changing authoring indices.

Tangents must be finite and approximately unit length (squared-length tolerance
0.002), orthogonal to their normalized source normal within 0.002, and have sign
`+1` or `-1`. The sole disabled-basis representation is `(0,0,0,0)` and must cover
all three corners of its triangle. `unusableTriangleCount` must equal the number
of these triangles. Every prepared vertex must be referenced. Batch triangles
must cover exactly the requested source subsets and reference the same source
corners in the same order.

`sourceSignature` is FNV-1a 64-bit (offset 14695981039346656037, multiplier
1099511628211) over little-endian uint32 scalar words: preparation revision;
position count and XYZ float bits; normal count and XYZ float bits; UV count and
XY float bits; subset count; then each subset's topology (`0` triangles, `1` strip,
`2` fan), requested flag (0/1), source-index count and source indices. Unrequested
subsets have zero source-index count. The resolved UVs are used, including UVs
shared from frame zero. The signature detects source changes; the section CRC and
structural checks remain independently required. This is not an authenticity hash.

Both runtime and authoring readers reject duplicates, unsupported revisions,
non-finite source data, bad lengths/references, signature mismatches and trailing
bytes. Payload lengths are checked before allocating batch arrays. Runtime parsing
and validation also run on the asynchronous CPU worker; no graphics context is
needed. Valid stored tangents are retained without calling MikkTSpace.

`saveV11` prepares subsets with a nonempty normal-map slot. Previously prepared
subsets may remain prepared after removing the map. Without a map or retained
preparation, no tangent section is emitted. Missing normals/UVs and point/line
topologies do not emit a surface basis. Saving reuses validated preparation and
regenerates it when source data changes; runtime loading generates missing bases
only for materials that need them. The section can use the usual NONE/DEFLATE
envelope compression. Material properties are stored separately in section 15.
Static 3D lighting consumes the prepared basis on OpenGL ES, DirectX 9 SM3,
DirectX 11 and Metal; see [Lighting](light.md#material-texture-slots).

Explicit authoring preparation (`MESH_MBM_DEBUG::prepareNormalMap`, C++/Lua) can
retain a prepared subset without a normal texture. Its preserve/generate/import
policies publish the same section-14 representation; there is no additional binary
layout for the policy. Imported data is provided per expanded triangle corner,
validated, split/remapped privately and serialized exactly like generated data.
A successfully prepared no-map subset is retained by subsequent save/load. This
does not make tangents mandatory for other subsets or assets.

### Optional `SECTION_NORMAL_MAP_MATERIALS` (15), section version 1

At most one section per asset. It is independent of tangent sections and texture
slots. All values are little-endian, serialized field by field, without padding:

| Field | Type | Meaning |
|---|---|---|
| entryCount | u32 | Positive count, bounded by the payload size |
| frameIndex | u32 | Zero-based source frame index, repeated per entry |
| subsetIndex | u32 | Zero-based source subset index |
| sourceConvention | u32 | 0 = +Y; 1 = -Y; other values rejected |
| strength | f32 | Finite and non-negative; 0 = no detail, 1 = original strength |

Size is exactly `4 + 16 * entryCount` bytes. Duplicate sections/keys, unsupported
versions, out-of-range references, invalid values, truncation and trailing bytes
are rejected. References are checked after all frame sections have been read,
so section ordering does not affect validity. NONE/DEFLATE compression and CRC
validation use the ordinary section envelope. The MSH format remains version 11.

Missing entries mean +Y and strength 1, without assigning any texture or requiring
tangents. New assets using defaults emit no section; setting a subset back to both
defaults removes its explicit entry. Explicit default records in input files are
valid. Non-default settings can be saved even when no normal texture is assigned.
Removing a texture does not discard settings. Frame/subset copying, removal and
reordering preserve associations; merging subsets with different settings is rejected.

The stored convention describes the source pixels. Loading, saving and changing
settings never invert texture pixels, change the tangent signature, or invalidate
prepared tangents. Supported static 3D lighting decodes the map, applies the Y
sign once, scales tangential XY by strength and safely normalizes (using
proportional bounded coefficients for large strengths). Strength zero disables
the detail. These settings do not affect the existing 2dw lighting equations.
Storage and validated serialization live in `private/normal-map-asset.*`; both
runtime loaders and the authoring loader use the same material payload parser.

## 6b. `SECTION_ANIMATION` payload

One `SECTION_ANIMATION` section per animation, in file order (no explicit index — see §8 format invariants).

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

## 6e. Unsupported section type 11

Numeric type 11 is not accepted by loaders or emitted by writers. Use canonical
skeletal sections 41–43 for runtime skeletal assets.

## 6f. `SECTION_ARTICULATED_PARTS` and `SECTION_ARTICULATED_ANIMATION` payloads

These are optional rigid/articulated-animation sections. They are omitted when the corresponding
data does not exist, so old meshes without them remain valid and continue through the existing
static/frame-animation path. Both the runtime and editor loaders parse them.

`SECTION_ARTICULATED_PARTS` is one bundled section with `sectionVersion` `1` and a `uint32_t
partCount`, followed by that many records. Other section versions are rejected. Every `partId` is
nonzero and globally unique within the asset, and a frame/subset occurrence can have at most one
Part. Part names are editable labels and may repeat:

```cpp
struct ARTICULATED_PART_V11
{
    uint64_t partId;
    uint32_t frameIndex;
    uint32_t subsetIndex;
    uint64_t parentPartId; // 0 means no parent; nonzero parent must belong to the same frame
    // name: length-prefixed string (§5)
    float pivotX, pivotY, pivotZ;
    float pivotQX, pivotQY, pivotQZ, pivotQW;
};
```

`SECTION_ARTICULATED_ANIMATION` is one bundled section with a `uint32_t clipCount`. The current
writer and readers use `sectionVersion` `1`; other versions are rejected. This is the first public
layout for the optional section, so Euler authoring data, easing, Bezier controls, and clip
composition mode are all present from version 1. Each clip contains a length-prefixed name,
`float duration`, `float speed`, `int32_t defaultPriority`, a `uint8_t loop`, a
`uint8_t blendMode`, and a
`uint32_t trackCount`. Each track contains `uint64_t partId`, a `uint8_t` channel mask,
`uint32_t keyCount`, and key records. Keys store a `float time`, position (`x/y/z`), quaternion
rotation (`x/y/z/w`), authored Euler rotation in degrees (`x/y/z`), a `uint8_t hasRotationEuler`
flag, an easing mode byte, four Bezier control-point floats (`x1/y1/x2/y2`), and scale (`x/y/z`).
When the flag is present, runtime interpolation uses
the authored Euler values and converts the result to a quaternion. The easing mode on a key controls
the segment from that key to the next one: `0` Linear, `1` Ease In, `2` Ease Out, `3` Ease In Out,
`4` Smoothstep, and `5` Cubic Bezier. Cubic Bezier
uses normalized time on X and normalized interpolation progress on Y. X control values must remain
within `0..1`; Y may leave that range to produce overshoot.

Clip blend modes are `0` (`ARTICULATED_BLEND_ABSOLUTE`) and `1`
(`ARTICULATED_BLEND_ADDITIVE`). Absolute clips establish the base pose through per-channel
priority/start-order resolution. Additive clips are then composed over that pose: position is an
offset from zero, rotation is a quaternion delta from identity, and scale is a multiplier from one.
Runtime playback weight and fade progress are instance state and are not stored in this section.

## 6g. Unsupported section type 40

Numeric type 40 is not accepted by loaders or emitted by writers. Canonical type
42 stores skeletal vertex influences against frame-0 geometry and a bone palette.

## 6h. Canonical skeletal-runtime persistence

This section fixes the byte-level contract. Readers for types 41–43 are implemented, including
shared `skeletonId`, frame-0 topology, palette, coverage, clip/track/key, and presence invariants.
`MESH_MBM_DEBUG::saveV11` validates and emits an existing canonical 41–42–43 group in canonical
order and includes it in `sectionCount`; it never promotes legacy editor data implicitly. The
direct Blender/FBX path creates types 41 and 42 directly from armature bind matrices and vertex
groups, and type 43 from sampled parent-relative pose matrices. An armature import writes one REST
bind-geometry frame; it does not duplicate sampled poses as static geometry.
Every integer and float uses the existing V11 little-endian field serializers; records are written
field-by-field and never struct-blitted. Strings use the length-prefixed UTF-8 encoding from §5.
Each payload is protected by its ordinary `SECTION_HEADER_V11` CRC over uncompressed bytes.

### Shared identity and presence rules

- `skeletonId` and every `boneId`/`clipId` are nonzero `uint64_t` values.
- The three sections carry the same `skeletonId`; a mismatch is a fatal asset error.
- At most one section of each of the three types may occur in a file.
- Weights or animation require the skeleton section. A skeleton may exist without either.
- If present, canonical order is skeleton, weights, animation. Readers must resolve by type rather
  than relying on order because the current loader stages all payloads before dispatch.
- Unknown section versions are fatal. A future version must receive explicit reader support.
- `FILE_HEADER_V11.sectionCount` includes each emitted section normally; the V11 file magic and
  `formatVersion` remain unchanged because section type/version provide the compatibility boundary.

### `SECTION_SKELETAL_SKELETON = 41`, version 3

```text
uint64 skeletonId
uint32 boneCount                  // at least 1 in version 1
repeat boneCount times, in parent-before-child compiled order:
    uint64 boneId
    uint64 parentBoneId          // 0 only for a root
    string name                  // required; unique within the skeleton in v1
    float32 translation[3]       // parent-relative bind-local
    float32 rotation[4]          // normalized quaternion x,y,z,w
    float32 scale[3]
    float32 radius               // authoring/display metadata
    float32 length               // authoring head-to-tail length metadata
    // version 2 only:
    float32 tailOffset[3]        // explicit tail joint in this bone's local bind space
    uint8 hasExplicitTail        // exactly 0 or 1
    // version 3 only:
    uint8 connectedToParent      // exactly 0 or 1; roots must use 0
```

IDs, not names or array positions, define identity and hierarchy. Every nonzero `parentBoneId` must
refer to an earlier record. All numeric fields must be finite; quaternion, scale, hierarchy,
local→global reconstruction, and inverse-bind validation use the documented numerical policy.
Global bind and inverse-global-bind matrices are derived and are not persisted.

Version 1 ends after `length`. It remains readable and receives the non-authoritative fallback
`tailOffset=(0,length,0)`, `hasExplicitTail=0`. Version 2 ends after `hasExplicitTail` and defaults
`connectedToParent=0`; version 3 is always written. A version-3 transform
joint may also deliberately use `hasExplicitTail=0`. Imported bones and explicitly authored bone
segments set it to `1`. Bind TRS remains authoritative for hierarchy and skinning,
while the local tail offset is authoritative for Bone Editor geometry and can follow animated poses.
`connectedToParent=1` additionally states that this bone's head and its parent's tail are one
authoring joint and must move together; hierarchy alone does not imply this constraint.

### `SECTION_SKELETAL_WEIGHTS = 42`, version 1

```text
uint64 skeletonId
uint32 frameIndex                // v1 requires 0 (SECTION_FRAME_STATIC frame 1)
uint32 vertexCount               // must equal that frame's vertexCount
uint32 paletteCount              // 0..65535 entries; largest valid uint16 index is 65534
uint64 paletteBoneId[paletteCount]
repeat vertexCount times:
    uint16 paletteIndex[4]       // 0xFFFF = unused
    float32 weight[4]
```

Palette bone IDs must be nonzero, unique, and present in the associated skeleton. Used indices must
be in range; unused slots have weight exactly zero; effective weights are finite, nonnegative, and
sum to one within tolerance. The v1 runtime section does not preserve partial/envelope-fallback
semantics: conversion from legacy editor weights must resolve or explicitly reject every uncovered
vertex rather than silently inventing an influence. Four influences remain fixed for the initial
GPU contract, while `uint16` removes the legacy name palette's 254-entry ceiling.

The Skeletal Animation Editor mutates an existing type-42 section transactionally through stable
bone IDs. Its Lua-facing names are lookup labels only: unknown or duplicate bones, invalid sums,
and incomplete canonical assets are rejected without creating a legacy section or changing the
previous vertex record.

### `SECTION_SKELETAL_ANIMATION = 43`, version 1

```text
uint64 skeletonId
uint32 clipCount
repeat clipCount times:
    uint64 clipId
    string name                  // required; unique within this section in v1
    float32 duration
    uint8 loop
    uint8 reserved[3]            // zero
    uint32 trackCount
    repeat trackCount times:
        uint64 boneId
        uint8 channelMask        // bit 0=T, bit 1=R, bit 2=S; nonzero
        uint8 reserved[3]        // zero
        uint32 keyCount          // at least 1
        repeat keyCount times:
            float32 time
            float32 translation[3]
            float32 rotation[4]  // quaternion x,y,z,w
            float32 scale[3]
            uint8 easing         // 0 linear, 1 in, 2 out, 3 in-out, 4 smoothstep, 5 cubic Bézier
            uint8 reserved[3]    // zero
            float32 bezierX1, bezierY1, bezierX2, bezierY2
```

Track targets use `boneId`; one clip may contain at most one track per bone. Key times are finite,
strictly increasing by more than `1e-6`, and within `[0,duration]`. Only channels selected by the
mask affect sampling; absent channels/tracks use bind-local values. Quaternions are the functional
rotation representation and use antipodal sign correction during interpolation. No Euler intent,
player state, blend priority, fade, timeline selection, or backend data is persisted in version 1.

### `SECTION_AUTOPLAY_ANIMATION = 44`, version 1

```text
uint8 kind                        // 1=frame, 2=articulated, 3=skeletal
string name                       // required; selects one animation in the corresponding family
```

The section is optional and appears at most once. A newly loaded `MESH` starts the selected frame,
articulated, or skeletal clip. `SPRITE` starts frame and articulated selections. Playback state
remains per instance. The editor stores the selected animation kind and name here without changing
the existing animation payloads. An unset selection is represented by omitting the section.

### Reader compatibility

Loaders require explicit support for each section type and version; unsupported
sections are rejected rather than silently ignored. This applies to canonical
skeletal sections 41–43 and autoplay section 44.

There is deliberately no legacy skeletal writer mode. Readers, writers, structs, enum members,
storage, and public Mesh Debug APIs for retired numeric types 11 and 40 have been removed. A file
carrying only those exploratory sections is rejected, not converted during ordinary loading. Its
source FBX must be imported again to produce sections 41–43. A static-only writer remains valid for
genuinely static meshes, but it must not disguise a skeletal asset by dropping its skeleton or
animation merely to target an older binary.

## 7. Index width (§6 `indexWidth`)

`indexWidth` is stored per frame. Mini MBM writes `16` and rejects other values
in runtime and authoring loaders. The field does not enable 32-bit mesh indices.

This is an intentional scope limit aligned with the engine's lightweight goal:
32-bit indices will not be implemented. Assets requiring larger indexed geometry
must be divided into supported meshes or simplified. A 16-bit index addresses
values 0 through 65535; this is a vertex-addressing range, not a triangle-count
limit. Individual editing operations may enforce stricter vertex-count limits.
Normal-map preparation retains its existing private 16-bit batch partitioning;
that does not introduce 32-bit source-mesh support.

## 8. Format invariants and implementation limits

- The magic is four bytes (`MBM1`), followed by `formatVersion`. No build/tooling
  stamp is embedded in the fixed header.
- Every section carries a CRC of its uncompressed payload, including uncompressed
  and empty payloads. Compression does not change this validation requirement.
- Repeated animation and static-frame sections use file order as their index.
- Reserved fields are written as zero. Section types/versions describe extensions.
- Detail payloads retain their specified disk layouts inside the TLV envelope.
- The runtime/editor geometry path supports 16-bit indices only. Wider indices
  are intentionally outside engine scope, not an unimplemented format feature.

Format/editor extensions and additional loader validation are tracked in
[Future Features](future-features.md#mesh-format-and-loader-follow-up).

### Optional relative texture export

`MESH_MBM_DEBUG::saveV11` accepts a final optional
`relativeTextures=false` argument. When enabled, primary/extra material texture
references keep their basename instead of being expanded by the asset search
paths. The binary layout is unchanged; the existing 63-byte writer limit still
applies. The image-mesh portable exporter limits generated PNG basenames to
63 bytes and writes them alongside the mesh. This flag alone does not copy files.
