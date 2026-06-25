#include "mesh-v11-io.h"
#include "mesh-io-primitives.h"

#include <cstring>
#include <miniz-wrap/miniz-wrap.h>

using namespace util::le_io;

namespace util
{
    bool readFileHeaderV11(FILE *fp, util::FILE_HEADER_V11 &out)
    {
        char magic[4] = {0, 0, 0, 0};
        uint16_t formatVersion = 0;
        uint8_t typeMesh = 0;
        uint8_t reserved0 = 0;
        int32_t backBufferWidth = 0;
        int32_t backBufferHeight = 0;
        uint32_t sectionCount = 0;
        if (!readBytes(fp, magic, sizeof(magic)) ||
            !readU16LE(fp, formatVersion) ||
            !readBytes(fp, &typeMesh, sizeof(typeMesh)) ||
            !readBytes(fp, &reserved0, sizeof(reserved0)) ||
            !readI32LE(fp, backBufferWidth) ||
            !readI32LE(fp, backBufferHeight) ||
            !readU32LE(fp, sectionCount))
        {
            return false;
        }
        if (std::memcmp(magic, MBM_V11_MAGIC, sizeof(magic)) != 0)
            return false;

        std::memcpy(out.magic, magic, sizeof(out.magic));
        out.formatVersion = formatVersion;
        out.typeMesh = typeMesh;
        out.reserved0 = reserved0;
        out.backBufferWidth = backBufferWidth;
        out.backBufferHeight = backBufferHeight;
        out.sectionCount = sectionCount;
        return true;
    }

    bool writeFileHeaderV11(FILE *fp, const util::FILE_HEADER_V11 &in)
    {
        return writeBytes(fp, in.magic, sizeof(in.magic)) &&
               writeU16LE(fp, in.formatVersion) &&
               writeBytes(fp, &in.typeMesh, sizeof(in.typeMesh)) &&
               writeBytes(fp, &in.reserved0, sizeof(in.reserved0)) &&
               writeI32LE(fp, in.backBufferWidth) &&
               writeI32LE(fp, in.backBufferHeight) &&
               writeU32LE(fp, in.sectionCount);
    }

    bool readSectionHeaderV11(FILE *fp, util::SECTION_HEADER_V11 &out)
    {
        uint16_t type = 0;
        uint16_t sectionVersion = 0;
        uint8_t compression = 0;
        uint8_t reserved1[3] = {0, 0, 0};
        uint32_t uncompressedLength = 0;
        uint32_t compressedLength = 0;
        uint32_t crc32Value = 0;
        if (!readU16LE(fp, type) ||
            !readU16LE(fp, sectionVersion) ||
            !readBytes(fp, &compression, sizeof(compression)) ||
            !readBytes(fp, reserved1, sizeof(reserved1)) ||
            !readU32LE(fp, uncompressedLength) ||
            !readU32LE(fp, compressedLength) ||
            !readU32LE(fp, crc32Value))
        {
            return false;
        }
        out.type = type;
        out.sectionVersion = sectionVersion;
        out.compression = compression;
        std::memcpy(out.reserved1, reserved1, sizeof(out.reserved1));
        out.uncompressedLength = uncompressedLength;
        out.compressedLength = compressedLength;
        out.crc32Value = crc32Value;
        return true;
    }

    bool writeSectionHeaderV11(FILE *fp, const util::SECTION_HEADER_V11 &in)
    {
        return writeU16LE(fp, in.type) &&
               writeU16LE(fp, in.sectionVersion) &&
               writeBytes(fp, &in.compression, sizeof(in.compression)) &&
               writeBytes(fp, in.reserved1, sizeof(in.reserved1)) &&
               writeU32LE(fp, in.uncompressedLength) &&
               writeU32LE(fp, in.compressedLength) &&
               writeU32LE(fp, in.crc32Value);
    }

    bool readStringV11(FILE *fp, std::string &out)
    {
        uint16_t length = 0;
        if (!readU16LE(fp, length))
            return false;
        out.resize(length);
        if (length == 0)
            return true;
        return readBytes(fp, &out[0], length);
    }

    bool writeStringV11(FILE *fp, const std::string &in)
    {
        if (in.size() > 0xFFFFu)
            return false;
        const uint16_t length = static_cast<uint16_t>(in.size());
        if (!writeU16LE(fp, length))
            return false;
        if (length == 0)
            return true;
        return writeBytes(fp, in.data(), length);
    }

    bool writeSectionV11(FILE *fp, util::SECTION_HEADER_V11 &header, const uint8_t *payload, uint32_t payloadSize)
    {
        header.uncompressedLength = payloadSize;
        header.crc32Value = mbm::crc32Buffer(payload, payloadSize);

        if (header.compression == util::SECTION_COMPRESSION_DEFLATE && payloadSize > 0)
        {
            mbm::MINIZ miniz;
            if (!miniz.compressStream(const_cast<uint8_t *>(payload), payloadSize))
                return false;
            header.compressedLength = miniz.getSizeDataStreamOut();
            return writeSectionHeaderV11(fp, header) &&
                   writeBytes(fp, miniz.getDataStreamOut(), header.compressedLength);
        }

        header.compression = util::SECTION_COMPRESSION_NONE;
        header.compressedLength = payloadSize;
        return writeSectionHeaderV11(fp, header) &&
               writeBytes(fp, payload, payloadSize);
    }

    bool readSectionV11(FILE *fp, util::SECTION_HEADER_V11 &header, std::vector<uint8_t> &payloadOut)
    {
        if (!readSectionHeaderV11(fp, header))
            return false;

        payloadOut.resize(header.uncompressedLength);
        if (header.uncompressedLength == 0)
            return header.compressedLength == 0;

        if (header.compression == util::SECTION_COMPRESSION_DEFLATE)
        {
            std::vector<uint8_t> compressed(header.compressedLength);
            if (!readBytes(fp, compressed.data(), header.compressedLength))
                return false;
            mbm::MINIZ miniz;
            if (!miniz.decompressStream(compressed.data(), header.compressedLength, header.uncompressedLength))
                return false;
            std::memcpy(payloadOut.data(), miniz.getDataStreamOut(), header.uncompressedLength);
        }
        else
        {
            if (header.compressedLength != header.uncompressedLength ||
                !readBytes(fp, payloadOut.data(), header.uncompressedLength))
            {
                return false;
            }
        }

        return mbm::crc32Buffer(payloadOut.data(), header.uncompressedLength) == header.crc32Value;
    }

    bool skipSectionPayloadV11(FILE *fp, const util::SECTION_HEADER_V11 &header)
    {
        return std::fseek(fp, static_cast<long>(header.compressedLength), SEEK_CUR) == 0;
    }
}
