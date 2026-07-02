#include "mesh-v11-io.h"
#include "mesh-io-primitives.h"

#include <cstring>
#include <miniz-wrap/miniz-wrap.h>

#if defined _WIN32
    #include <io.h>
#else
    #include <unistd.h>
#endif

using namespace util::le_io;

namespace
{
    // Shrinks fp's underlying file to its current write position - needed after
    // util::writeSectionV11Streamed compresses a payload that was originally streamed to disk
    // uncompressed, leaving stale tail bytes past the new (shorter) logical end.
    bool truncateFileToCurrentPosition(FILE *fp)
    {
        if (std::fflush(fp) != 0)
            return false;
        const long pos = std::ftell(fp);
        if (pos < 0)
            return false;
#if defined _WIN32
        return _chsize(_fileno(fp), pos) == 0;
#else
        return ftruncate(fileno(fp), pos) == 0;
#endif
    }
}

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

    bool readStringV11(util::MEM_CURSOR_V11 &fp, std::string &out)
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
            return header.compressedLength == 0 && header.crc32Value == mbm::crc32Buffer(nullptr, 0);

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

        // header.compression carries whatever the caller pre-set (NONE/DEFLATE) - writeSectionV11
        // honors it, compressing payloadBytes first if requested, and recomputes
        // uncompressedLength/compressedLength/crc32Value itself.
        if (std::fseek(fp, headerPos, SEEK_SET) != 0 ||
            !writeSectionV11(fp, header, payloadBytes.data(), payloadSize))
        {
            return false;
        }

        // Compression can make the just-written payload shorter than what was originally streamed
        // to disk - drop the stale uncompressed tail bytes left over past the new logical end.
        return truncateFileToCurrentPosition(fp);
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

    bool readFrameHeaderV11(util::MEM_CURSOR_V11 &fp, util::FRAME_HEADER_V11 &out)
    {
        return readU32LE(fp, out.totalSubset) &&
               readU32LE(fp, out.vertexCount) &&
               readBytes(fp, &out.indexWidth, sizeof(out.indexWidth)) &&
               readBytes(fp, &out.hasNormal, sizeof(out.hasNormal)) &&
               readBytes(fp, &out.hasUv, sizeof(out.hasUv)) &&
               readBytes(fp, &out.uvSource, sizeof(out.uvSource)) &&
               readU32LE(fp, out.indexCount);
    }

    bool readTextureRefV11(util::MEM_CURSOR_V11 &fp, util::TEXTURE_REF_V11 &out)
    {
        if (!readBytes(fp, &out.storage, sizeof(out.storage)))
            return false;
        if (out.storage == util::TEXTURE_REF_STORAGE_PATH)
            return readStringV11(fp, out.path);
        return false; // EMBEDDED_COMPRESSED is reserved, not implemented (milestone 4 scope)
    }

    bool readSubsetDescV11(util::MEM_CURSOR_V11 &fp, util::SUBSET_DESC_V11 &out)
    {
        return readTextureRefV11(fp, out.primaryTexture) &&
               readI32LE(fp, out.vertexCount) &&
               readI32LE(fp, out.vertexStart) &&
               readI32LE(fp, out.indexStart) &&
               readI32LE(fp, out.indexCount) &&
               readBytes(fp, out.alphaColor, sizeof(out.alphaColor)) &&
               readU16LE(fp, out.extraSlotCount);
    }

    bool readSubsetExtraSlotV11(util::MEM_CURSOR_V11 &fp, util::SUBSET_EXTRA_SLOT_V11 &out)
    {
        return readBytes(fp, &out.role, sizeof(out.role)) &&
               readTextureRefV11(fp, out.texture);
    }

    bool readMaterialTransformV11(util::MEM_CURSOR_V11 &fp, util::MATERIAL_TRANSFORM_V11 &out)
    {
        const auto readColor = [&fp](mbm::COLOR &c) noexcept
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

    bool writeShaderVarV11(FILE *fp, const util::SHADER_VAR_V11 &in)
    {
        return writeBytes(fp, &in.typeVar, sizeof(in.typeVar)) &&
               writeF32LE(fp, in.min[0]) && writeF32LE(fp, in.min[1]) &&
               writeF32LE(fp, in.min[2]) && writeF32LE(fp, in.min[3]) &&
               writeF32LE(fp, in.max[0]) && writeF32LE(fp, in.max[1]) &&
               writeF32LE(fp, in.max[2]) && writeF32LE(fp, in.max[3]);
    }

    bool writeShaderStepV11(FILE *fp, const util::SHADER_STEP_V11 &in)
    {
        if (!writeStringV11(fp, in.name) ||
            !writeF32LE(fp, in.timeAnimation) ||
            !writeI32LE(fp, in.typeAnimation) ||
            !writeU16LE(fp, in.varCount))
        {
            return false;
        }
        return true;
    }

    bool writeFxHeaderV11(FILE *fp, const util::FX_HEADER_V11 &in)
    {
        if (!writeI32LE(fp, in.blendOperation) ||
            !writeBytes(fp, &in.hasFxTexture, sizeof(in.hasFxTexture)))
        {
            return false;
        }
        if (in.hasFxTexture && !writeTextureRefV11(fp, in.fxTexture))
            return false;
        if (!writeBytes(fp, &in.hasPS, sizeof(in.hasPS)))
            return false;
        if (in.hasPS && !writeShaderStepV11(fp, in.ps))
            return false;
        if (!writeBytes(fp, &in.hasVS, sizeof(in.hasVS)))
            return false;
        if (in.hasVS && !writeShaderStepV11(fp, in.vs))
            return false;
        return true;
    }

    bool writeAnimationHeaderV11(FILE *fp, const util::ANIMATION_HEADER_V11 &in)
    {
        return writeStringV11(fp, in.name) &&
               writeI32LE(fp, in.initialFrame) &&
               writeI32LE(fp, in.finalFrame) &&
               writeF32LE(fp, in.timeBetweenFrame) &&
               writeI32LE(fp, in.typeAnimation) &&
               writeU16LE(fp, in.blendState) &&
               writeBytes(fp, &in.hasFx, sizeof(in.hasFx));
    }

    bool readShaderVarV11(util::MEM_CURSOR_V11 &fp, util::SHADER_VAR_V11 &out)
    {
        return readBytes(fp, &out.typeVar, sizeof(out.typeVar)) &&
               readF32LE(fp, out.min[0]) && readF32LE(fp, out.min[1]) &&
               readF32LE(fp, out.min[2]) && readF32LE(fp, out.min[3]) &&
               readF32LE(fp, out.max[0]) && readF32LE(fp, out.max[1]) &&
               readF32LE(fp, out.max[2]) && readF32LE(fp, out.max[3]);
    }

    bool readShaderStepV11(util::MEM_CURSOR_V11 &fp, util::SHADER_STEP_V11 &out)
    {
        return readStringV11(fp, out.name) &&
               readF32LE(fp, out.timeAnimation) &&
               readI32LE(fp, out.typeAnimation) &&
               readU16LE(fp, out.varCount);
    }

    bool readFxHeaderV11(util::MEM_CURSOR_V11 &fp, util::FX_HEADER_V11 &out)
    {
        if (!readI32LE(fp, out.blendOperation) ||
            !readBytes(fp, &out.hasFxTexture, sizeof(out.hasFxTexture)))
        {
            return false;
        }
        if (out.hasFxTexture && !readTextureRefV11(fp, out.fxTexture))
            return false;
        if (!readBytes(fp, &out.hasPS, sizeof(out.hasPS)))
            return false;
        if (out.hasPS && !readShaderStepV11(fp, out.ps))
            return false;
        if (!readBytes(fp, &out.hasVS, sizeof(out.hasVS)))
            return false;
        if (out.hasVS && !readShaderStepV11(fp, out.vs))
            return false;
        return true;
    }

    bool readAnimationHeaderV11(util::MEM_CURSOR_V11 &fp, util::ANIMATION_HEADER_V11 &out)
    {
        return readStringV11(fp, out.name) &&
               readI32LE(fp, out.initialFrame) &&
               readI32LE(fp, out.finalFrame) &&
               readF32LE(fp, out.timeBetweenFrame) &&
               readI32LE(fp, out.typeAnimation) &&
               readU16LE(fp, out.blendState) &&
               readBytes(fp, &out.hasFx, sizeof(out.hasFx));
    }

    bool writeStageParticleV11(FILE *fp, const util::STAGE_PARTICLE &in)
    {
        return writeF32LE(fp, in.minOffsetPosition.x) && writeF32LE(fp, in.minOffsetPosition.y) &&
               writeF32LE(fp, in.minOffsetPosition.z) &&
               writeF32LE(fp, in.maxOffsetPosition.x) && writeF32LE(fp, in.maxOffsetPosition.y) &&
               writeF32LE(fp, in.maxOffsetPosition.z) &&
               writeF32LE(fp, in.minDirection.x) && writeF32LE(fp, in.minDirection.y) &&
               writeF32LE(fp, in.minDirection.z) &&
               writeF32LE(fp, in.maxDirection.x) && writeF32LE(fp, in.maxDirection.y) &&
               writeF32LE(fp, in.maxDirection.z) &&
               writeF32LE(fp, in.minColor.x) && writeF32LE(fp, in.minColor.y) &&
               writeF32LE(fp, in.minColor.z) &&
               writeF32LE(fp, in.maxColor.x) && writeF32LE(fp, in.maxColor.y) &&
               writeF32LE(fp, in.maxColor.z) &&
               writeF32LE(fp, in.minSpeed) && writeF32LE(fp, in.maxSpeed) &&
               writeF32LE(fp, in.minTimeLife) && writeF32LE(fp, in.maxTimeLife) &&
               writeF32LE(fp, in.minSizeParticle) && writeF32LE(fp, in.maxSizeParticle) &&
               writeF32LE(fp, in.ariseTime) && writeF32LE(fp, in.stageTime) &&
               writeU32LE(fp, in.totalParticle) &&
               writeBytes(fp, &in.segmented, sizeof(in.segmented)) &&
               writeBytes(fp, &in.sizeMin2Max, sizeof(in.sizeMin2Max)) &&
               writeBytes(fp, &in.revive, sizeof(in.revive)) &&
               writeBytes(fp, &in._operator, sizeof(in._operator)) &&
               writeBytes(fp, &in.invert_red, sizeof(in.invert_red)) &&
               writeBytes(fp, &in.invert_green, sizeof(in.invert_green)) &&
               writeBytes(fp, &in.invert_blue, sizeof(in.invert_blue)) &&
               writeBytes(fp, &in.invert_alpha, sizeof(in.invert_alpha));
    }

    bool readStageParticleV11(util::MEM_CURSOR_V11 &fp, util::STAGE_PARTICLE &out)
    {
        return readF32LE(fp, out.minOffsetPosition.x) && readF32LE(fp, out.minOffsetPosition.y) &&
               readF32LE(fp, out.minOffsetPosition.z) &&
               readF32LE(fp, out.maxOffsetPosition.x) && readF32LE(fp, out.maxOffsetPosition.y) &&
               readF32LE(fp, out.maxOffsetPosition.z) &&
               readF32LE(fp, out.minDirection.x) && readF32LE(fp, out.minDirection.y) &&
               readF32LE(fp, out.minDirection.z) &&
               readF32LE(fp, out.maxDirection.x) && readF32LE(fp, out.maxDirection.y) &&
               readF32LE(fp, out.maxDirection.z) &&
               readF32LE(fp, out.minColor.x) && readF32LE(fp, out.minColor.y) &&
               readF32LE(fp, out.minColor.z) &&
               readF32LE(fp, out.maxColor.x) && readF32LE(fp, out.maxColor.y) &&
               readF32LE(fp, out.maxColor.z) &&
               readF32LE(fp, out.minSpeed) && readF32LE(fp, out.maxSpeed) &&
               readF32LE(fp, out.minTimeLife) && readF32LE(fp, out.maxTimeLife) &&
               readF32LE(fp, out.minSizeParticle) && readF32LE(fp, out.maxSizeParticle) &&
               readF32LE(fp, out.ariseTime) && readF32LE(fp, out.stageTime) &&
               readU32LE(fp, out.totalParticle) &&
               readBytes(fp, &out.segmented, sizeof(out.segmented)) &&
               readBytes(fp, &out.sizeMin2Max, sizeof(out.sizeMin2Max)) &&
               readBytes(fp, &out.revive, sizeof(out.revive)) &&
               readBytes(fp, &out._operator, sizeof(out._operator)) &&
               readBytes(fp, &out.invert_red, sizeof(out.invert_red)) &&
               readBytes(fp, &out.invert_green, sizeof(out.invert_green)) &&
               readBytes(fp, &out.invert_blue, sizeof(out.invert_blue)) &&
               readBytes(fp, &out.invert_alpha, sizeof(out.invert_alpha));
    }

    bool writeFontDetailHeaderV11(FILE *fp, const util::FONT_DETAIL_HEADER_V11 &in)
    {
        return writeStringV11(fp, in.name) &&
               writeI16LE(fp, in.spaceXCharacter) &&
               writeI16LE(fp, in.spaceYCharacter) &&
               writeU16LE(fp, in.heightLetter) &&
               writeU16LE(fp, in.letterCount);
    }

    bool readFontDetailHeaderV11(util::MEM_CURSOR_V11 &fp, util::FONT_DETAIL_HEADER_V11 &out)
    {
        return readStringV11(fp, out.name) &&
               readI16LE(fp, out.spaceXCharacter) &&
               readI16LE(fp, out.spaceYCharacter) &&
               readU16LE(fp, out.heightLetter) &&
               readU16LE(fp, out.letterCount);
    }

    bool writeDetailLetterV11(FILE *fp, const util::DETAIL_LETTER &in)
    {
        return writeBytes(fp, &in.letter, sizeof(in.letter)) &&
               writeBytes(fp, &in.indexFrame, sizeof(in.indexFrame)) &&
               writeU16LE(fp, in.widthLetter) &&
               writeU16LE(fp, in.heightLetter);
    }

    bool readDetailLetterV11(util::MEM_CURSOR_V11 &fp, util::DETAIL_LETTER &out)
    {
        return readBytes(fp, &out.letter, sizeof(out.letter)) &&
               readBytes(fp, &out.indexFrame, sizeof(out.indexFrame)) &&
               readU16LE(fp, out.widthLetter) &&
               readU16LE(fp, out.heightLetter);
    }

    bool writeBtileIndexTileV11(FILE *fp, const util::BTILE_INDEX_TILE &in)
    {
        return writeU32LE(fp, in.index) && writeF32LE(fp, in.x) && writeF32LE(fp, in.y);
    }

    bool readBtileIndexTileV11(util::MEM_CURSOR_V11 &fp, util::BTILE_INDEX_TILE &out)
    {
        return readU32LE(fp, out.index) && readF32LE(fp, out.x) && readF32LE(fp, out.y);
    }

    bool writeBtileBrickInfoV11(FILE *fp, const util::BTILE_BRICK_INFO &in)
    {
        return writeU16LE(fp, in.index) && writeU16LE(fp, in.original_index) &&
               writeU16LE(fp, in.rotation) && writeU16LE(fp, in.flipped);
    }

    bool readBtileBrickInfoV11(util::MEM_CURSOR_V11 &fp, util::BTILE_BRICK_INFO &out)
    {
        return readU16LE(fp, out.index) && readU16LE(fp, out.original_index) &&
               readU16LE(fp, out.rotation) && readU16LE(fp, out.flipped);
    }

    bool writeTileHeaderMapV11(FILE *fp, const util::TILE_HEADER_MAP_V11 &in)
    {
        return writeU32LE(fp, in.count_width_tile) &&
               writeU32LE(fp, in.count_height_tile) &&
               writeU32LE(fp, in.size_width_tile) &&
               writeU32LE(fp, in.size_height_tile) &&
               writeU32LE(fp, in.layerCount) &&
               writeU32LE(fp, in.countRawTiles) &&
               writeU32LE(fp, in.objectCount) &&
               writeU32LE(fp, in.propertyCount) &&
               writeU32LE(fp, in.typeMap) &&
               writeU32LE(fp, in.background) &&
               writeStringV11(fp, in.backgroundTexture) &&
               writeBytes(fp, &in.renderDirectionLeftToRight, sizeof(in.renderDirectionLeftToRight)) &&
               writeBytes(fp, &in.renderDirectionTopToDown, sizeof(in.renderDirectionTopToDown));
    }

    bool readTileHeaderMapV11(util::MEM_CURSOR_V11 &fp, util::TILE_HEADER_MAP_V11 &out)
    {
        return readU32LE(fp, out.count_width_tile) &&
               readU32LE(fp, out.count_height_tile) &&
               readU32LE(fp, out.size_width_tile) &&
               readU32LE(fp, out.size_height_tile) &&
               readU32LE(fp, out.layerCount) &&
               readU32LE(fp, out.countRawTiles) &&
               readU32LE(fp, out.objectCount) &&
               readU32LE(fp, out.propertyCount) &&
               readU32LE(fp, out.typeMap) &&
               readU32LE(fp, out.background) &&
               readStringV11(fp, out.backgroundTexture) &&
               readBytes(fp, &out.renderDirectionLeftToRight, sizeof(out.renderDirectionLeftToRight)) &&
               readBytes(fp, &out.renderDirectionTopToDown, sizeof(out.renderDirectionTopToDown));
    }

    bool writeTileLayerHeaderV11(FILE *fp, const util::TILE_LAYER_HEADER_V11 &in)
    {
        return writeF32LE(fp, in.offsetX) && writeF32LE(fp, in.offsetY) && writeF32LE(fp, in.offsetZ);
    }

    bool readTileLayerHeaderV11(util::MEM_CURSOR_V11 &fp, util::TILE_LAYER_HEADER_V11 &out)
    {
        return readF32LE(fp, out.offsetX) && readF32LE(fp, out.offsetY) && readF32LE(fp, out.offsetZ);
    }

    bool writeTileObjHeaderV11(FILE *fp, const util::TILE_OBJ_HEADER_V11 &in)
    {
        return writeStringV11(fp, in.name) && writeU16LE(fp, in.type) && writeU16LE(fp, in.pointCount);
    }

    bool readTileObjHeaderV11(util::MEM_CURSOR_V11 &fp, util::TILE_OBJ_HEADER_V11 &out)
    {
        return readStringV11(fp, out.name) && readU16LE(fp, out.type) && readU16LE(fp, out.pointCount);
    }

    bool writeTilePropertyV11(FILE *fp, const util::TILE_PROPERTY_V11 &in)
    {
        return writeStringV11(fp, in.owner) && writeStringV11(fp, in.name) &&
               writeStringV11(fp, in.value) && writeU16LE(fp, in.type);
    }

    bool readTilePropertyV11(util::MEM_CURSOR_V11 &fp, util::TILE_PROPERTY_V11 &out)
    {
        return readStringV11(fp, out.owner) && readStringV11(fp, out.name) &&
               readStringV11(fp, out.value) && readU16LE(fp, out.type);
    }
}
