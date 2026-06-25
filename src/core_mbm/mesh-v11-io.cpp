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

    bool writeSectionV11Streamed(FILE *fp, util::SECTION_HEADER_V11 &header,
                                 const std::function<bool(FILE*)> &writePayload)
    {
        const long headerPos = std::ftell(fp);
        if (headerPos < 0)
            return false;

        const util::SECTION_HEADER_V11 placeholder;
        if (!writeSectionHeaderV11(fp, placeholder))
            return false;

        const long payloadPos = std::ftell(fp);
        if (payloadPos < 0 || !writePayload(fp))
            return false;

        const long payloadEnd = std::ftell(fp);
        if (payloadEnd < 0 || payloadEnd < payloadPos)
            return false;
        const uint32_t payloadSize = static_cast<uint32_t>(payloadEnd - payloadPos);

        std::vector<uint8_t> payloadBytes(payloadSize);
        if (payloadSize > 0)
        {
            if (std::fseek(fp, payloadPos, SEEK_SET) != 0 ||
                !readBytes(fp, payloadBytes.data(), payloadSize))
            {
                return false;
            }
        }

        header.compression = util::SECTION_COMPRESSION_NONE;
        header.uncompressedLength = payloadSize;
        header.compressedLength = payloadSize;
        header.crc32Value = mbm::crc32Buffer(payloadBytes.data(), payloadSize);

        if (std::fseek(fp, headerPos, SEEK_SET) != 0 ||
            !writeSectionHeaderV11(fp, header) ||
            std::fseek(fp, payloadEnd, SEEK_SET) != 0)
        {
            return false;
        }
        return true;
    }

    bool writeFrameHeaderV11(FILE *fp, const util::FRAME_HEADER_V11 &in)
    {
        return writeU32LE(fp, in.totalSubset) &&
               writeU32LE(fp, in.vertexCount) &&
               writeBytes(fp, &in.indexWidth, sizeof(in.indexWidth)) &&
               writeBytes(fp, &in.hasNormal, sizeof(in.hasNormal)) &&
               writeBytes(fp, &in.hasUv, sizeof(in.hasUv)) &&
               writeBytes(fp, &in.uvSource, sizeof(in.uvSource)) &&
               writeU32LE(fp, in.indexCount);
    }

    bool writeTextureRefV11(FILE *fp, const util::TEXTURE_REF_V11 &in)
    {
        if (!writeBytes(fp, &in.storage, sizeof(in.storage)))
            return false;
        if (in.storage == util::TEXTURE_REF_STORAGE_PATH)
            return writeStringV11(fp, in.path);
        return false; // EMBEDDED_COMPRESSED is reserved, not implemented (milestone 3 scope)
    }

    bool writeSubsetDescV11(FILE *fp, const util::SUBSET_DESC_V11 &in)
    {
        return writeTextureRefV11(fp, in.primaryTexture) &&
               writeI32LE(fp, in.vertexCount) &&
               writeI32LE(fp, in.vertexStart) &&
               writeI32LE(fp, in.indexStart) &&
               writeI32LE(fp, in.indexCount) &&
               writeBytes(fp, in.alphaColor, sizeof(in.alphaColor)) &&
               writeU16LE(fp, in.extraSlotCount);
    }

    bool writeSubsetExtraSlotV11(FILE *fp, const util::SUBSET_EXTRA_SLOT_V11 &in)
    {
        return writeBytes(fp, &in.role, sizeof(in.role)) &&
               writeTextureRefV11(fp, in.texture);
    }

    bool writeMaterialTransformV11(FILE *fp, const util::MATERIAL_TRANSFORM_V11 &in)
    {
        const auto writeColor = [fp](const mbm::COLOR &c) noexcept
        {
            return writeF32LE(fp, c.r) && writeF32LE(fp, c.g) && writeF32LE(fp, c.b) && writeF32LE(fp, c.a);
        };
        return writeColor(in.material.Diffuse) &&
               writeColor(in.material.Ambient) &&
               writeColor(in.material.Specular) &&
               writeColor(in.material.Emissive) &&
               writeF32LE(fp, in.material.Power) &&
               writeF32LE(fp, in.angleX) && writeF32LE(fp, in.angleY) && writeF32LE(fp, in.angleZ) &&
               writeF32LE(fp, in.posX) && writeF32LE(fp, in.posY) && writeF32LE(fp, in.posZ) &&
               writeU32LE(fp, in.mode_draw) &&
               writeU32LE(fp, in.mode_cull_face) &&
               writeU32LE(fp, in.mode_front_face_direction);
    }

    bool readFrameHeaderV11(FILE *fp, util::FRAME_HEADER_V11 &out)
    {
        return readU32LE(fp, out.totalSubset) &&
               readU32LE(fp, out.vertexCount) &&
               readBytes(fp, &out.indexWidth, sizeof(out.indexWidth)) &&
               readBytes(fp, &out.hasNormal, sizeof(out.hasNormal)) &&
               readBytes(fp, &out.hasUv, sizeof(out.hasUv)) &&
               readBytes(fp, &out.uvSource, sizeof(out.uvSource)) &&
               readU32LE(fp, out.indexCount);
    }

    bool readTextureRefV11(FILE *fp, util::TEXTURE_REF_V11 &out)
    {
        if (!readBytes(fp, &out.storage, sizeof(out.storage)))
            return false;
        if (out.storage == util::TEXTURE_REF_STORAGE_PATH)
            return readStringV11(fp, out.path);
        return false; // EMBEDDED_COMPRESSED is reserved, not implemented (milestone 4 scope)
    }

    bool readSubsetDescV11(FILE *fp, util::SUBSET_DESC_V11 &out)
    {
        return readTextureRefV11(fp, out.primaryTexture) &&
               readI32LE(fp, out.vertexCount) &&
               readI32LE(fp, out.vertexStart) &&
               readI32LE(fp, out.indexStart) &&
               readI32LE(fp, out.indexCount) &&
               readBytes(fp, out.alphaColor, sizeof(out.alphaColor)) &&
               readU16LE(fp, out.extraSlotCount);
    }

    bool readSubsetExtraSlotV11(FILE *fp, util::SUBSET_EXTRA_SLOT_V11 &out)
    {
        return readBytes(fp, &out.role, sizeof(out.role)) &&
               readTextureRefV11(fp, out.texture);
    }

    bool readMaterialTransformV11(FILE *fp, util::MATERIAL_TRANSFORM_V11 &out)
    {
        const auto readColor = [fp](mbm::COLOR &c) noexcept
        {
            return readF32LE(fp, c.r) && readF32LE(fp, c.g) && readF32LE(fp, c.b) && readF32LE(fp, c.a);
        };
        return readColor(out.material.Diffuse) &&
               readColor(out.material.Ambient) &&
               readColor(out.material.Specular) &&
               readColor(out.material.Emissive) &&
               readF32LE(fp, out.material.Power) &&
               readF32LE(fp, out.angleX) && readF32LE(fp, out.angleY) && readF32LE(fp, out.angleZ) &&
               readF32LE(fp, out.posX) && readF32LE(fp, out.posY) && readF32LE(fp, out.posZ) &&
               readU32LE(fp, out.mode_draw) &&
               readU32LE(fp, out.mode_cull_face) &&
               readU32LE(fp, out.mode_front_face_direction);
    }
}
