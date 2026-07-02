#ifndef MESH_V11_IO_H
#define MESH_V11_IO_H

// Section/TLV envelope read-write helpers for the v11 mesh format (docs/mesh-v11-format.md): the
// fixed file header, the generic section envelope (type/length/crc, optional per-section DEFLATE),
// and the section-payload field-by-field serializers built on top of them.

#include <cstdio>
#include <header-mesh.h>
#include <string>
#include <vector>
#include <functional>
#include "mesh-io-primitives.h" // util::MEM_CURSOR_V11

namespace util
{
    bool readFileHeaderV11(FILE *fp, util::FILE_HEADER_V11 &out);
    bool writeFileHeaderV11(FILE *fp, const util::FILE_HEADER_V11 &in);

    bool readSectionHeaderV11(FILE *fp, util::SECTION_HEADER_V11 &out);
    bool writeSectionHeaderV11(FILE *fp, const util::SECTION_HEADER_V11 &in);

    // Length-prefixed UTF-8 string: uint16 length, then `length` bytes, no null terminator
    // (format doc Sec. 5 - replaces fixed char[] name/path buffers). The read side only ever runs
    // against an already-decompressed section payload (see MEM_CURSOR_V11 in mesh-io-primitives.h)
    // - the write side still streams straight into the real on-disk FILE*.
    bool readStringV11(util::MEM_CURSOR_V11 &fp, std::string &out);
    bool writeStringV11(FILE *fp, const std::string &in);

    // Writes one section: compresses `payload` if header.compression requests it (falls back to
    // SECTION_COMPRESSION_NONE for an empty payload), fills in uncompressedLength/compressedLength/
    // crc32 on `header`, then writes header followed by the (possibly compressed) bytes.
    // Caller must have already set header.type and header.sectionVersion, and may pre-set
    // header.compression to request DEFLATE.
    bool writeSectionV11(FILE *fp, util::SECTION_HEADER_V11 &header, const uint8_t *payload, uint32_t payloadSize);

    // Reads a section header plus its payload, decompressing per header.compression and verifying
    // crc32 against the decompressed bytes. Returns false (including on crc32 mismatch) without
    // leaving the file position in a defined place - callers should treat that as a fatal read error.
    bool readSectionV11(FILE *fp, util::SECTION_HEADER_V11 &header, std::vector<uint8_t> &payloadOut);

    // Skips a section's payload bytes without reading or decompressing them - for section types the
    // caller does not understand/need, per the TLV envelope's "always skippable" guarantee.
    bool skipSectionPayloadV11(FILE *fp, const util::SECTION_HEADER_V11 &header);

    // Writes one section by streaming its payload directly into `fp` via `writePayload`, instead of
    // building it in a caller-owned buffer first. Reserves the 16-byte envelope, calls writePayload(fp),
    // measures what it wrote, rewinds and reads those bytes back into memory, then delegates to
    // writeSectionV11 to compress (if header.compression requests DEFLATE - same pre-set-by-caller
    // convention as writeSectionV11 itself), checksum, and write the real envelope + payload, and
    // finally truncates the file to the new end (compression can shrink the payload below what was
    // originally streamed to disk, so the stale uncompressed tail bytes must not linger). `header.type`/
    // `sectionVersion` must already be set by the caller. Used for every section so they share one
    // code path instead of bespoke ones per section type.
    bool writeSectionV11Streamed(FILE *fp, util::SECTION_HEADER_V11 &header,
                                 const std::function<bool(FILE*)> &writePayload);

    // Section-payload field-by-field serializers (docs/mesh-v11-format.md Sec. 6), following the
    // same little-endian, never-struct-blitted style as mesh-v8-io.cpp / the envelope helpers above.
    // Each writes directly into `fp` - meant to be called from inside a writeSectionV11Streamed
    // callback.
    bool writeFrameHeaderV11(FILE *fp, const util::FRAME_HEADER_V11 &in);
    bool writeTextureRefV11(FILE *fp, const util::TEXTURE_REF_V11 &in);
    bool writeSubsetDescV11(FILE *fp, const util::SUBSET_DESC_V11 &in);
    bool writeSubsetExtraSlotV11(FILE *fp, const util::SUBSET_EXTRA_SLOT_V11 &in);
    bool writeMaterialTransformV11(FILE *fp, const util::MATERIAL_TRANSFORM_V11 &in);

    // The read-side mirror of the serializers above. Read against an in-memory section payload
    // (MEM_CURSOR_V11) rather than FILE* - see readStringV11's comment above; the write side stays
    // FILE*-based.
    bool readFrameHeaderV11(util::MEM_CURSOR_V11 &fp, util::FRAME_HEADER_V11 &out);
    // Fails (returns false) on TEXTURE_REF_STORAGE_EMBEDDED_COMPRESSED - reserved, unread.
    bool readTextureRefV11(util::MEM_CURSOR_V11 &fp, util::TEXTURE_REF_V11 &out);
    bool readSubsetDescV11(util::MEM_CURSOR_V11 &fp, util::SUBSET_DESC_V11 &out);
    bool readSubsetExtraSlotV11(util::MEM_CURSOR_V11 &fp, util::SUBSET_EXTRA_SLOT_V11 &out);
    bool readMaterialTransformV11(util::MEM_CURSOR_V11 &fp, util::MATERIAL_TRANSFORM_V11 &out);

    // SECTION_ANIMATION payload serializers (docs/mesh-v11-format.md Sec. 6b).
    bool writeShaderVarV11(FILE *fp, const util::SHADER_VAR_V11 &in);
    bool writeShaderStepV11(FILE *fp, const util::SHADER_STEP_V11 &in);
    bool writeFxHeaderV11(FILE *fp, const util::FX_HEADER_V11 &in);
    bool writeAnimationHeaderV11(FILE *fp, const util::ANIMATION_HEADER_V11 &in);

    bool readShaderVarV11(util::MEM_CURSOR_V11 &fp, util::SHADER_VAR_V11 &out);
    bool readShaderStepV11(util::MEM_CURSOR_V11 &fp, util::SHADER_STEP_V11 &out);
    bool readFxHeaderV11(util::MEM_CURSOR_V11 &fp, util::FX_HEADER_V11 &out);
    bool readAnimationHeaderV11(util::MEM_CURSOR_V11 &fp, util::ANIMATION_HEADER_V11 &out);

    // SECTION_DETAIL_PARTICLE / SECTION_DETAIL_FONT payload serializers.
    bool writeStageParticleV11(FILE *fp, const util::STAGE_PARTICLE &in);
    bool readStageParticleV11(util::MEM_CURSOR_V11 &fp, util::STAGE_PARTICLE &out);

    bool writeFontDetailHeaderV11(FILE *fp, const util::FONT_DETAIL_HEADER_V11 &in);
    bool readFontDetailHeaderV11(util::MEM_CURSOR_V11 &fp, util::FONT_DETAIL_HEADER_V11 &out);

    bool writeDetailLetterV11(FILE *fp, const util::DETAIL_LETTER &in);
    bool readDetailLetterV11(util::MEM_CURSOR_V11 &fp, util::DETAIL_LETTER &out);

    // SECTION_DETAIL_TILE payload serializers.
    bool writeBtileIndexTileV11(FILE *fp, const util::BTILE_INDEX_TILE &in);
    bool readBtileIndexTileV11(util::MEM_CURSOR_V11 &fp, util::BTILE_INDEX_TILE &out);

    bool writeBtileBrickInfoV11(FILE *fp, const util::BTILE_BRICK_INFO &in);
    bool readBtileBrickInfoV11(util::MEM_CURSOR_V11 &fp, util::BTILE_BRICK_INFO &out);

    bool writeTileHeaderMapV11(FILE *fp, const util::TILE_HEADER_MAP_V11 &in);
    bool readTileHeaderMapV11(util::MEM_CURSOR_V11 &fp, util::TILE_HEADER_MAP_V11 &out);

    bool writeTileLayerHeaderV11(FILE *fp, const util::TILE_LAYER_HEADER_V11 &in);
    bool readTileLayerHeaderV11(util::MEM_CURSOR_V11 &fp, util::TILE_LAYER_HEADER_V11 &out);

    bool writeTileObjHeaderV11(FILE *fp, const util::TILE_OBJ_HEADER_V11 &in);
    bool readTileObjHeaderV11(util::MEM_CURSOR_V11 &fp, util::TILE_OBJ_HEADER_V11 &out);

    bool writeTilePropertyV11(FILE *fp, const util::TILE_PROPERTY_V11 &in);
    bool readTilePropertyV11(util::MEM_CURSOR_V11 &fp, util::TILE_PROPERTY_V11 &out);
}

#endif
