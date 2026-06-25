#ifndef MESH_V11_IO_H
#define MESH_V11_IO_H

// Section/TLV envelope read-write helpers for the v11 mesh format (docs/mesh-v11-format.md).
// Milestone 1 scope only: the fixed file header and the generic section envelope (type/length/crc,
// optional per-section DEFLATE). Section *payload* layouts (frame/subset/material/...) are not
// defined here - they land with the v11 writer/reader (milestones 3-4) on top of these helpers.

#include <cstdio>
#include <header-mesh.h>
#include <string>
#include <vector>

namespace util
{
    bool readFileHeaderV11(FILE *fp, util::FILE_HEADER_V11 &out);
    bool writeFileHeaderV11(FILE *fp, const util::FILE_HEADER_V11 &in);

    bool readSectionHeaderV11(FILE *fp, util::SECTION_HEADER_V11 &out);
    bool writeSectionHeaderV11(FILE *fp, const util::SECTION_HEADER_V11 &in);

    // Length-prefixed UTF-8 string: uint16 length, then `length` bytes, no null terminator
    // (format doc Sec. 5 - replaces fixed char[] name/path buffers).
    bool readStringV11(FILE *fp, std::string &out);
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
}

#endif
