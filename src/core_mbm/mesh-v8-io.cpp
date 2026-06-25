#include "mesh-v8-io.h"
#include "mesh-io-primitives.h"

#include <cstring>

using namespace util::le_io;

namespace
{
    inline bool readColor(FILE *fp, mbm::COLOR &out)
    {
        return readF32LE(fp, out.r) &&
               readF32LE(fp, out.g) &&
               readF32LE(fp, out.b) &&
               readF32LE(fp, out.a);
    }

    inline bool writeColor(FILE *fp, const mbm::COLOR &in)
    {
        return writeF32LE(fp, in.r) &&
               writeF32LE(fp, in.g) &&
               writeF32LE(fp, in.b) &&
               writeF32LE(fp, in.a);
    }

    inline bool readMaterial(FILE *fp, util::MATERIAL &out)
    {
        return readColor(fp, out.Diffuse) &&
               readColor(fp, out.Ambient) &&
               readColor(fp, out.Specular) &&
               readColor(fp, out.Emissive) &&
               readF32LE(fp, out.Power);
    }

    inline bool writeMaterial(FILE *fp, const util::MATERIAL &in)
    {
        return writeColor(fp, in.Diffuse) &&
               writeColor(fp, in.Ambient) &&
               writeColor(fp, in.Specular) &&
               writeColor(fp, in.Emissive) &&
               writeF32LE(fp, in.Power);
    }
}

namespace util
{
    bool readHeaderV8(FILE *fp, util::HEADER &out)
    {
        int32_t version = 0;
        uint32_t magic = 0;
        int32_t reserved = 0;
        int32_t backBufferWidth = 0;
        int32_t backBufferHeight = 0;
        int32_t extraHeader = 0;
        if (!readBytes(fp, out.name, sizeof(out.name)) ||
            !readBytes(fp, out.typeApp, sizeof(out.typeApp)) ||
            !readI32LE(fp, version) ||
            !readU32LE(fp, magic) ||
            !readI32LE(fp, reserved) ||
            !readI32LE(fp, backBufferWidth) ||
            !readI32LE(fp, backBufferHeight) ||
            !readI32LE(fp, extraHeader))
        {
            return false;
        }
        out.version = version;
        out.magic = magic;
        out.reserved = reserved;
        out.backBufferWidth = backBufferWidth;
        out.backBufferHeight = backBufferHeight;
        out.extraHeader = extraHeader;
        return true;
    }

    bool writeHeaderV8(FILE *fp, const util::HEADER &in)
    {
        return writeBytes(fp, in.name, sizeof(in.name)) &&
               writeBytes(fp, in.typeApp, sizeof(in.typeApp)) &&
             writeI32LE(fp, in.version) &&
             writeU32LE(fp, in.magic) &&
             writeI32LE(fp, in.reserved) &&
             writeI32LE(fp, in.backBufferWidth) &&
             writeI32LE(fp, in.backBufferHeight) &&
             writeI32LE(fp, in.extraHeader);
    }

    bool readHeaderMeshV8(FILE *fp, util::HEADER_MESH &out)
    {
        int32_t totalAnimation = 0;
        int32_t totalFrames = 0;
        int32_t deprecatedTypePhysics = 0;
        int16_t hasNorText0 = 0;
        int16_t hasNorText1 = 0;
        return readMaterial(fp, out.material) &&
               readI32LE(fp, totalAnimation) &&
               readI32LE(fp, totalFrames) &&
               readI32LE(fp, deprecatedTypePhysics) &&
               readI16LE(fp, hasNorText0) &&
               readI16LE(fp, hasNorText1) &&
               readF32LE(fp, out.angleX) &&
               readF32LE(fp, out.angleY) &&
               readF32LE(fp, out.angleZ) &&
               readF32LE(fp, out.posX) &&
               readF32LE(fp, out.posY) &&
               readF32LE(fp, out.posZ) &&
               ((out.totalAnimation = totalAnimation),
                (out.totalFrames = totalFrames),
                (out.deprecated_typePhysics = deprecatedTypePhysics),
                (out.hasNorText[0] = hasNorText0),
                (out.hasNorText[1] = hasNorText1),
                true);
    }

    bool writeHeaderMeshV8(FILE *fp, const util::HEADER_MESH &in)
    {
        return writeMaterial(fp, in.material) &&
             writeI32LE(fp, in.totalAnimation) &&
             writeI32LE(fp, in.totalFrames) &&
             writeI32LE(fp, in.deprecated_typePhysics) &&
               writeI16LE(fp, in.hasNorText[0]) &&
               writeI16LE(fp, in.hasNorText[1]) &&
               writeF32LE(fp, in.angleX) &&
               writeF32LE(fp, in.angleY) &&
               writeF32LE(fp, in.angleZ) &&
               writeF32LE(fp, in.posX) &&
               writeF32LE(fp, in.posY) &&
               writeF32LE(fp, in.posZ);
    }

    bool readHeaderAnimationV8(FILE *fp, util::HEADER_ANIMATION &out)
    {
        int32_t initialFrame = 0;
        int32_t finalFrame = 0;
        int32_t typeAnimation = 0;
        return readBytes(fp, out.nameAnimation, sizeof(out.nameAnimation)) &&
               readI32LE(fp, initialFrame) &&
               readI32LE(fp, finalFrame) &&
               readF32LE(fp, out.timeBetweenFrame) &&
               readI32LE(fp, typeAnimation) &&
               readU16LE(fp, out.hasShaderEffect) &&
               readU16LE(fp, out.blendState) &&
               ((out.initialFrame = initialFrame),
                (out.finalFrame = finalFrame),
                (out.typeAnimation = typeAnimation),
                true);
    }

    bool writeHeaderAnimationV8(FILE *fp, const util::HEADER_ANIMATION &in)
    {
        return writeBytes(fp, in.nameAnimation, sizeof(in.nameAnimation)) &&
             writeI32LE(fp, in.initialFrame) &&
             writeI32LE(fp, in.finalFrame) &&
               writeF32LE(fp, in.timeBetweenFrame) &&
             writeI32LE(fp, in.typeAnimation) &&
               writeU16LE(fp, in.hasShaderEffect) &&
               writeU16LE(fp, in.blendState);
    }

    bool readHeaderInfoShaderStepV8(FILE *fp, util::HEADER_INFO_SHADER_STEP &out)
    {
        int32_t blendOperation = 0;
        return readI16LE(fp, out.lenNameShader) &&
               readI16LE(fp, out.lenTextureStage2) &&
               readI16LE(fp, out.sizeArrayVarInBytes) &&
               readI16LE(fp, out.typeAnimation) &&
               readI32LE(fp, blendOperation) &&
               readF32LE(fp, out.timeAnimation) &&
               ((out.blendOperation = blendOperation), true);
    }

    bool writeHeaderInfoShaderStepV8(FILE *fp, const util::HEADER_INFO_SHADER_STEP &in)
    {
        return writeI16LE(fp, in.lenNameShader) &&
               writeI16LE(fp, in.lenTextureStage2) &&
               writeI16LE(fp, in.sizeArrayVarInBytes) &&
               writeI16LE(fp, in.typeAnimation) &&
             writeI32LE(fp, in.blendOperation) &&
               writeF32LE(fp, in.timeAnimation);
    }

    bool readHeaderInfoShaderEffectV10(FILE *fp, int16_t &lenTextureAnimationEffect)
    {
        int16_t reserved0 = 0;
        int32_t reserved1 = 0;
        return readI16LE(fp, lenTextureAnimationEffect) &&
               readI16LE(fp, reserved0) &&
               readI32LE(fp, reserved1);
    }

    bool writeHeaderInfoShaderEffectV10(FILE *fp, const int16_t lenTextureAnimationEffect)
    {
        const int16_t reserved0 = 0;
        const int32_t reserved1 = 0;
        return writeI16LE(fp, lenTextureAnimationEffect) &&
               writeI16LE(fp, reserved0) &&
               writeI32LE(fp, reserved1);
    }

    bool readHeaderFrameV8(FILE *fp, util::HEADER_FRAME &out)
    {
        int32_t totalSubset = 0;
        int32_t sizeIndexBuffer = 0;
        int32_t sizeVertexBuffer = 0;
        int32_t stride = 0;
        if (!readI32LE(fp, totalSubset) ||
            !readI32LE(fp, sizeIndexBuffer) ||
            !readI32LE(fp, sizeVertexBuffer) ||
            !readI32LE(fp, stride) ||
            !readBytes(fp, out.typeBuffer, sizeof(out.typeBuffer)))
        {
            return false;
        }
        out.totalSubset = totalSubset;
        out.sizeIndexBuffer = sizeIndexBuffer;
        out.sizeVertexBuffer = sizeVertexBuffer;
        out.stride = stride;
        return true;
    }

    bool writeHeaderFrameV8(FILE *fp, const util::HEADER_FRAME &in)
    {
         return writeI32LE(fp, in.totalSubset) &&
             writeI32LE(fp, in.sizeIndexBuffer) &&
             writeI32LE(fp, in.sizeVertexBuffer) &&
             writeI32LE(fp, in.stride) &&
               writeBytes(fp, in.typeBuffer, sizeof(in.typeBuffer));
    }

    bool readHeaderDescSubsetV8(FILE *fp, util::HEADER_DESC_SUBSET &out)
    {
        int32_t vertexCount = 0;
        int32_t vertexStart = 0;
        int32_t indexStart = 0;
        int32_t indexCount = 0;
        if (!readBytes(fp, out.nameTexture, sizeof(out.nameTexture)) ||
            !readI32LE(fp, vertexCount) ||
            !readI32LE(fp, vertexStart) ||
            !readI32LE(fp, indexStart) ||
            !readI32LE(fp, indexCount) ||
            !readBytes(fp, out.alphaColor, sizeof(out.alphaColor)))
        {
            return false;
        }
        out.vertexCount = vertexCount;
        out.vertexStart = vertexStart;
        out.indexStart = indexStart;
        out.indexCount = indexCount;
        return true;
    }

    bool writeHeaderDescSubsetV8(FILE *fp, const util::HEADER_DESC_SUBSET &in)
    {
        return writeBytes(fp, in.nameTexture, sizeof(in.nameTexture)) &&
             writeI32LE(fp, in.vertexCount) &&
             writeI32LE(fp, in.vertexStart) &&
             writeI32LE(fp, in.indexStart) &&
             writeI32LE(fp, in.indexCount) &&
               writeBytes(fp, in.alphaColor, sizeof(in.alphaColor));
    }

    bool readHeaderDescSubsetV9(FILE *fp, util::HEADER_DESC_SUBSET &out)
    {
        uint16_t materialTextureSlotCount = 0;
        uint16_t reservedMaterialTextureSlots = 0;
        if (!readHeaderDescSubsetV8(fp, out) ||
            !readU16LE(fp, materialTextureSlotCount) ||
            !readU16LE(fp, reservedMaterialTextureSlots))
        {
            return false;
        }
        out.materialTextureSlotCount = materialTextureSlotCount;
        out.reservedMaterialTextureSlots = reservedMaterialTextureSlots;
        return true;
    }

    bool writeHeaderDescSubsetV9(FILE *fp, const util::HEADER_DESC_SUBSET &in)
    {
        return writeHeaderDescSubsetV8(fp, in) &&
               writeU16LE(fp, in.materialTextureSlotCount) &&
               writeU16LE(fp, in.reservedMaterialTextureSlots);
    }

    bool readMaterialTextureSlotHeaderV9(FILE *fp, util::MATERIAL_TEXTURE_SLOT_HEADER &out)
    {
        uint16_t type = 0;
        uint16_t reserved = 0;
        uint32_t payloadSizeInBytes = 0;
        if (!readU16LE(fp, type) ||
            !readU16LE(fp, reserved) ||
            !readU32LE(fp, payloadSizeInBytes) ||
            !readBytes(fp, out.nameTexture, sizeof(out.nameTexture)))
        {
            return false;
        }
        out.type = type;
        out.reserved = reserved;
        out.payloadSizeInBytes = payloadSizeInBytes;
        return true;
    }

    bool writeMaterialTextureSlotHeaderV9(FILE *fp, const util::MATERIAL_TEXTURE_SLOT_HEADER &in)
    {
        return writeU16LE(fp, in.type) &&
               writeU16LE(fp, in.reserved) &&
               writeU32LE(fp, in.payloadSizeInBytes) &&
               writeBytes(fp, in.nameTexture, sizeof(in.nameTexture));
    }

    bool readHeaderImgV8(FILE *fp, util::HEADER_IMG &out)
    {
        if (!readU32LE(fp, out.width) ||
            !readU32LE(fp, out.height) ||
            !readU16LE(fp, out.depth) ||
            !readU16LE(fp, out.channel) ||
            !readU32LE(fp, out.lenght) ||
            !readBytes(fp, out.alphaColor, sizeof(out.alphaColor)))
        {
            return false;
        }
        return true;
    }

    bool writeHeaderImgV8(FILE *fp, const util::HEADER_IMG &in)
    {
        return writeU32LE(fp, in.width) &&
               writeU32LE(fp, in.height) &&
               writeU16LE(fp, in.depth) &&
               writeU16LE(fp, in.channel) &&
               writeU32LE(fp, in.lenght) &&
               writeBytes(fp, in.alphaColor, sizeof(in.alphaColor));
    }

    namespace
    {
        constexpr int32_t kMaxExtraHeaderSize = 64 * 1024; // paths should never be bigger than this

        inline bool isValidExtraSize(const int32_t size)
        {
            return size >= 0 && size <= kMaxExtraHeaderSize;
        }
    }

    bool readExtraHeaderV8(FILE *fp, util::EXTRA_HEADER &out)
    {
        const auto startPos = std::ftell(fp);
        int32_t sizeExtraHeader = 0;
        if (!readBytes(fp, &out.type, sizeof(out.type)) ||
            !readI32LE(fp, sizeExtraHeader))
        {
            return false;
        }
        out.sizeExtraHeader = sizeExtraHeader;

        // Guard against garbage sizes (observed on Windows when reading old assets)
        if (!isValidExtraSize(sizeExtraHeader))
        {
            // Try legacy layout: type stored as 32 bits (struct write with padding)
            if (startPos >= 0 && std::fseek(fp, static_cast<long>(startPos), SEEK_SET) == 0)
            {
                int32_t legacyType = 0;
                if (!readI32LE(fp, legacyType) || !readI32LE(fp, sizeExtraHeader))
                {
                    return false;
                }
                out.type = static_cast<char>(legacyType & 0xFF);
                out.sizeExtraHeader = sizeExtraHeader;
            }
        }

        if (!isValidExtraSize(out.sizeExtraHeader))
        {
            return false;
        }
        return true;
    }

    bool writeExtraHeaderV8(FILE *fp, const util::EXTRA_HEADER &in)
    {
        return writeBytes(fp, &in.type, sizeof(in.type)) &&
               writeI32LE(fp, in.sizeExtraHeader);
    }

    bool readInfoDrawModeV8(FILE *fp, util::INFO_DRAW_MODE &out)
    {
        return readU32LE(fp, out.mode_draw) &&
               readU32LE(fp, out.mode_cull_face) &&
               readU32LE(fp, out.mode_front_face_direction);
    }

    bool writeInfoDrawModeV8(FILE *fp, const util::INFO_DRAW_MODE &in)
    {
        return writeU32LE(fp, in.mode_draw) &&
               writeU32LE(fp, in.mode_cull_face) &&
               writeU32LE(fp, in.mode_front_face_direction);
    }

    bool readDetailMeshV8(FILE *fp, util::DETAIL_MESH &out)
    {
        int32_t type = 0;
        int32_t totalBounding = 0;
        if (!readI32LE(fp, type) ||
            !readI32LE(fp, totalBounding))
        {
            return false;
        }
        out.type = type;
        out.totalBounding = totalBounding;
        return true;
    }

    bool writeDetailMeshV8(FILE *fp, const util::DETAIL_MESH &in)
    {
        return writeI32LE(fp, in.type) &&
               writeI32LE(fp, in.totalBounding);
    }

    bool readDetailHeaderFontV8(FILE *fp, util::DETAIL_HEADER_FONT &out)
    {
        return readU16LE(fp, out.sizeNameFonte) &&
               readU16LE(fp, out.totalDetailFont) &&
               readI16LE(fp, out.spaceXCharacter) &&
               readI16LE(fp, out.spaceYCharacter) &&
               readU16LE(fp, out.heightLetter);
    }

    bool writeDetailHeaderFontV8(FILE *fp, const util::DETAIL_HEADER_FONT &in)
    {
        return writeU16LE(fp, in.sizeNameFonte) &&
               writeU16LE(fp, in.totalDetailFont) &&
               writeI16LE(fp, in.spaceXCharacter) &&
               writeI16LE(fp, in.spaceYCharacter) &&
               writeU16LE(fp, in.heightLetter);
    }

    bool readDetailLetterV8(FILE *fp, util::DETAIL_LETTER &out)
    {
        return readBytes(fp, &out.letter, sizeof(out.letter)) &&
               readBytes(fp, &out.indexFrame, sizeof(out.indexFrame)) &&
               readU16LE(fp, out.widthLetter) &&
               readU16LE(fp, out.heightLetter);
    }

    bool writeDetailLetterV8(FILE *fp, const util::DETAIL_LETTER &in)
    {
        return writeBytes(fp, &in.letter, sizeof(in.letter)) &&
               writeBytes(fp, &in.indexFrame, sizeof(in.indexFrame)) &&
               writeU16LE(fp, in.widthLetter) &&
               writeU16LE(fp, in.heightLetter);
    }

    bool readStageParticleV8(FILE *fp, util::STAGE_PARTICLE &out)
    {
        return readF32LE(fp, out.minOffsetPosition.x) &&
               readF32LE(fp, out.minOffsetPosition.y) &&
               readF32LE(fp, out.minOffsetPosition.z) &&
               readF32LE(fp, out.maxOffsetPosition.x) &&
               readF32LE(fp, out.maxOffsetPosition.y) &&
               readF32LE(fp, out.maxOffsetPosition.z) &&
               readF32LE(fp, out.minDirection.x) &&
               readF32LE(fp, out.minDirection.y) &&
               readF32LE(fp, out.minDirection.z) &&
               readF32LE(fp, out.maxDirection.x) &&
               readF32LE(fp, out.maxDirection.y) &&
               readF32LE(fp, out.maxDirection.z) &&
               readF32LE(fp, out.minColor.x) &&
               readF32LE(fp, out.minColor.y) &&
               readF32LE(fp, out.minColor.z) &&
               readF32LE(fp, out.maxColor.x) &&
               readF32LE(fp, out.maxColor.y) &&
               readF32LE(fp, out.maxColor.z) &&
               readF32LE(fp, out.minSpeed) &&
               readF32LE(fp, out.maxSpeed) &&
               readF32LE(fp, out.minTimeLife) &&
               readF32LE(fp, out.maxTimeLife) &&
               readF32LE(fp, out.minSizeParticle) &&
               readF32LE(fp, out.maxSizeParticle) &&
               readF32LE(fp, out.ariseTime) &&
               readF32LE(fp, out.stageTime) &&
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

    bool writeStageParticleV8(FILE *fp, const util::STAGE_PARTICLE &in)
    {
        return writeF32LE(fp, in.minOffsetPosition.x) &&
               writeF32LE(fp, in.minOffsetPosition.y) &&
               writeF32LE(fp, in.minOffsetPosition.z) &&
               writeF32LE(fp, in.maxOffsetPosition.x) &&
               writeF32LE(fp, in.maxOffsetPosition.y) &&
               writeF32LE(fp, in.maxOffsetPosition.z) &&
               writeF32LE(fp, in.minDirection.x) &&
               writeF32LE(fp, in.minDirection.y) &&
               writeF32LE(fp, in.minDirection.z) &&
               writeF32LE(fp, in.maxDirection.x) &&
               writeF32LE(fp, in.maxDirection.y) &&
               writeF32LE(fp, in.maxDirection.z) &&
               writeF32LE(fp, in.minColor.x) &&
               writeF32LE(fp, in.minColor.y) &&
               writeF32LE(fp, in.minColor.z) &&
               writeF32LE(fp, in.maxColor.x) &&
               writeF32LE(fp, in.maxColor.y) &&
               writeF32LE(fp, in.maxColor.z) &&
               writeF32LE(fp, in.minSpeed) &&
               writeF32LE(fp, in.maxSpeed) &&
               writeF32LE(fp, in.minTimeLife) &&
               writeF32LE(fp, in.maxTimeLife) &&
               writeF32LE(fp, in.minSizeParticle) &&
               writeF32LE(fp, in.maxSizeParticle) &&
               writeF32LE(fp, in.ariseTime) &&
               writeF32LE(fp, in.stageTime) &&
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

    bool readFloat3ArrayV8(FILE *fp, float out[3])
    {
        return readF32LE(fp, out[0]) &&
               readF32LE(fp, out[1]) &&
               readF32LE(fp, out[2]);
    }

    bool writeFloat3ArrayV8(FILE *fp, const float in[3])
    {
        return writeF32LE(fp, in[0]) &&
               writeF32LE(fp, in[1]) &&
               writeF32LE(fp, in[2]);
    }

    bool readU16ArrayV8(FILE *fp, uint16_t *out, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!readU16LE(fp, out[i]))
                return false;
        }
        return true;
    }

    bool writeU16ArrayV8(FILE *fp, const uint16_t *in, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!writeU16LE(fp, in[i]))
                return false;
        }
        return true;
    }

    bool readFloatArrayV8(FILE *fp, float *out, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!readF32LE(fp, out[i]))
                return false;
        }
        return true;
    }

    bool writeFloatArrayV8(FILE *fp, const float *in, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!writeF32LE(fp, in[i]))
                return false;
        }
        return true;
    }

    bool readVec2V8(FILE *fp, mbm::VEC2 &out)
    {
        return readF32LE(fp, out.x) &&
               readF32LE(fp, out.y);
    }

    bool writeVec2V8(FILE *fp, const mbm::VEC2 &in)
    {
        return writeF32LE(fp, in.x) &&
               writeF32LE(fp, in.y);
    }

    bool readVec2ArrayV8(FILE *fp, mbm::VEC2 *out, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!readVec2V8(fp, out[i]))
                return false;
        }
        return true;
    }

    bool writeVec2ArrayV8(FILE *fp, const mbm::VEC2 *in, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!writeVec2V8(fp, in[i]))
                return false;
        }
        return true;
    }

    bool readVec3V8(FILE *fp, mbm::VEC3 &out)
    {
        return readF32LE(fp, out.x) &&
               readF32LE(fp, out.y) &&
               readF32LE(fp, out.z);
    }

    bool writeVec3V8(FILE *fp, const mbm::VEC3 &in)
    {
        return writeF32LE(fp, in.x) &&
               writeF32LE(fp, in.y) &&
               writeF32LE(fp, in.z);
    }

    bool readVec3ArrayV8(FILE *fp, mbm::VEC3 *out, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!readVec3V8(fp, out[i]))
                return false;
        }
        return true;
    }

    bool writeVec3ArrayV8(FILE *fp, const mbm::VEC3 *in, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!writeVec3V8(fp, in[i]))
                return false;
        }
        return true;
    }

    bool readCubeV8(FILE *fp, mbm::CUBE &out)
    {
        return readVec3V8(fp, out.halfDim) &&
               readVec3V8(fp, out.absCenter);
    }

    bool writeCubeV8(FILE *fp, const mbm::CUBE &in)
    {
        return writeVec3V8(fp, in.halfDim) &&
               writeVec3V8(fp, in.absCenter);
    }

    bool readSphereV8(FILE *fp, mbm::SPHERE &out)
    {
        return readF32LE(fp, out.ray) &&
               readF32LE(fp, out.absCenter[0]) &&
               readF32LE(fp, out.absCenter[1]) &&
               readF32LE(fp, out.absCenter[2]);
    }

    bool writeSphereV8(FILE *fp, const mbm::SPHERE &in)
    {
        return writeF32LE(fp, in.ray) &&
               writeF32LE(fp, in.absCenter[0]) &&
               writeF32LE(fp, in.absCenter[1]) &&
               writeF32LE(fp, in.absCenter[2]);
    }

    bool readCubeComplexV8(FILE *fp, mbm::CUBE_COMPLEX &out)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (!readF32LE(fp, out.p[i].x) ||
                !readF32LE(fp, out.p[i].y) ||
                !readF32LE(fp, out.p[i].z))
            {
                return false;
            }
        }
        return true;
    }

    bool writeCubeComplexV8(FILE *fp, const mbm::CUBE_COMPLEX &in)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (!writeF32LE(fp, in.p[i].x) ||
                !writeF32LE(fp, in.p[i].y) ||
                !writeF32LE(fp, in.p[i].z))
            {
                return false;
            }
        }
        return true;
    }

    bool readTriangleV8(FILE *fp, mbm::TRIANGLE &out)
    {
        return readVec3ArrayV8(fp, out.point, 3) &&
               readVec2V8(fp, out.position);
    }

    bool writeTriangleV8(FILE *fp, const mbm::TRIANGLE &in)
    {
        return writeVec3ArrayV8(fp, in.point, 3) &&
               writeVec2V8(fp, in.position);
    }

    bool readTriangleLegacyNoPosV8(FILE *fp, mbm::TRIANGLE &out)
    {
        out.position.x = 0.0f;
        out.position.y = 0.0f;
        return readVec3ArrayV8(fp, out.point, 3);
    }

    bool readBtileHeaderMapV8(FILE *fp, util::BTILE_HEADER_MAP &out)
    {
        uint32_t typeMap = 0;
        if (!readU32LE(fp, out.count_width_tile) ||
            !readU32LE(fp, out.count_height_tile) ||
            !readU32LE(fp, out.size_width_tile) ||
            !readU32LE(fp, out.size_height_tile) ||
            !readU32LE(fp, out.layerCount) ||
            !readU32LE(fp, out.countRawTiles) ||
            !readU32LE(fp, out.objectCount) ||
            !readU32LE(fp, out.propertyCount) ||
            !readU32LE(fp, typeMap) ||
            !readU32LE(fp, out.background) ||
            !readBytes(fp, out.background_texture, sizeof(out.background_texture)) ||
            !readBytes(fp, out.renderDirection, sizeof(out.renderDirection)))
        {
            return false;
        }
        out.typeMap = static_cast<util::BTILE_TYPE_MAP>(typeMap);
        return true;
    }

    bool writeBtileHeaderMapV8(FILE *fp, const util::BTILE_HEADER_MAP &in)
    {
        return writeU32LE(fp, in.count_width_tile) &&
               writeU32LE(fp, in.count_height_tile) &&
               writeU32LE(fp, in.size_width_tile) &&
               writeU32LE(fp, in.size_height_tile) &&
               writeU32LE(fp, in.layerCount) &&
               writeU32LE(fp, in.countRawTiles) &&
               writeU32LE(fp, in.objectCount) &&
               writeU32LE(fp, in.propertyCount) &&
               writeU32LE(fp, static_cast<uint32_t>(in.typeMap)) &&
               writeU32LE(fp, in.background) &&
               writeBytes(fp, in.background_texture, sizeof(in.background_texture)) &&
               writeBytes(fp, in.renderDirection, sizeof(in.renderDirection));
    }

    bool readBtileBrickInfoV8(FILE *fp, util::BTILE_BRICK_INFO &out)
    {
        return readU16LE(fp, out.index) &&
               readU16LE(fp, out.original_index) &&
               readU16LE(fp, out.rotation) &&
               readU16LE(fp, out.flipped);
    }

    bool writeBtileBrickInfoV8(FILE *fp, const util::BTILE_BRICK_INFO &in)
    {
        return writeU16LE(fp, in.index) &&
               writeU16LE(fp, in.original_index) &&
               writeU16LE(fp, in.rotation) &&
               writeU16LE(fp, in.flipped);
    }

    bool readBtileIndexTileV8(FILE *fp, util::BTILE_INDEX_TILE &out)
    {
        return readU32LE(fp, out.index) &&
               readF32LE(fp, out.x) &&
               readF32LE(fp, out.y);
    }

    bool writeBtileIndexTileV8(FILE *fp, const util::BTILE_INDEX_TILE &in)
    {
        return writeU32LE(fp, in.index) &&
               writeF32LE(fp, in.x) &&
               writeF32LE(fp, in.y);
    }

    bool readBtileDetailHeaderV8(FILE *fp, util::BTILE_DETAIL_HEADER &out)
    {
        return readU32LE(fp, out.totalObj) &&
               readU32LE(fp, out.totalProperties);
    }

    bool writeBtileDetailHeaderV8(FILE *fp, const util::BTILE_DETAIL_HEADER &in)
    {
        return writeU32LE(fp, in.totalObj) &&
               writeU32LE(fp, in.totalProperties);
    }

    bool readBtileObjHeaderV8(FILE *fp, util::BTILE_OBJ_HEADER &out)
    {
        return readU16LE(fp, out.sizeName) &&
               readU16LE(fp, out.type) &&
               readU16LE(fp, out.sizePoints);
    }

    bool writeBtileObjHeaderV8(FILE *fp, const util::BTILE_OBJ_HEADER &in)
    {
        return writeU16LE(fp, in.sizeName) &&
               writeU16LE(fp, in.type) &&
               writeU16LE(fp, in.sizePoints);
    }

    bool readBtilePropertyHeaderV8(FILE *fp, util::BTILE_PROPERTY_HEADER &out)
    {
        return readU16LE(fp, out.type) &&
               readU16LE(fp, out.nameLength) &&
               readU16LE(fp, out.valueLength) &&
               readU16LE(fp, out.ownerLength);
    }

    bool writeBtilePropertyHeaderV8(FILE *fp, const util::BTILE_PROPERTY_HEADER &in)
    {
        return writeU16LE(fp, in.type) &&
               writeU16LE(fp, in.nameLength) &&
               writeU16LE(fp, in.valueLength) &&
               writeU16LE(fp, in.ownerLength);
    }

    bool readBtileBrickInfoArrayV8(FILE *fp, util::BTILE_BRICK_INFO *out, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!readBtileBrickInfoV8(fp, out[i]))
                return false;
        }
        return true;
    }

    bool writeBtileBrickInfoArrayV8(FILE *fp, const util::BTILE_BRICK_INFO *in, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!writeBtileBrickInfoV8(fp, in[i]))
                return false;
        }
        return true;
    }

    bool readBtileIndexTileArrayV8(FILE *fp, util::BTILE_INDEX_TILE *out, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!readBtileIndexTileV8(fp, out[i]))
                return false;
        }
        return true;
    }

    bool writeBtileIndexTileArrayV8(FILE *fp, const util::BTILE_INDEX_TILE *in, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!writeBtileIndexTileV8(fp, in[i]))
                return false;
        }
        return true;
    }
}
