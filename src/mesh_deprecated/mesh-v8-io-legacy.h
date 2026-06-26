#ifndef MESH_V8_IO_LEGACY_H
#define MESH_V8_IO_LEGACY_H

#include <cstdio>
#include <header-mesh.h>
#include <shapes.h>

namespace util
{
    bool readHeaderV8(FILE *fp, util::HEADER &out);
    bool writeHeaderV8(FILE *fp, const util::HEADER &in);

    bool readHeaderMeshV8(FILE *fp, util::HEADER_MESH &out);
    bool writeHeaderMeshV8(FILE *fp, const util::HEADER_MESH &in);

    bool readHeaderAnimationV8(FILE *fp, util::HEADER_ANIMATION &out);
    bool writeHeaderAnimationV8(FILE *fp, const util::HEADER_ANIMATION &in);

    bool readHeaderInfoShaderStepV8(FILE *fp, util::HEADER_INFO_SHADER_STEP &out);
    bool writeHeaderInfoShaderStepV8(FILE *fp, const util::HEADER_INFO_SHADER_STEP &in);
    bool readHeaderInfoShaderEffectV10(FILE *fp, int16_t &lenTextureAnimationEffect);
    bool writeHeaderInfoShaderEffectV10(FILE *fp, int16_t lenTextureAnimationEffect);

    bool readHeaderFrameV8(FILE *fp, util::HEADER_FRAME &out);
    bool writeHeaderFrameV8(FILE *fp, const util::HEADER_FRAME &in);

    bool readHeaderDescSubsetV8(FILE *fp, util::HEADER_DESC_SUBSET &out);
    bool writeHeaderDescSubsetV8(FILE *fp, const util::HEADER_DESC_SUBSET &in);
    bool readHeaderDescSubsetV9(FILE *fp, util::HEADER_DESC_SUBSET &out);
    bool writeHeaderDescSubsetV9(FILE *fp, const util::HEADER_DESC_SUBSET &in);

    bool readMaterialTextureSlotHeaderV9(FILE *fp, util::MATERIAL_TEXTURE_SLOT_HEADER &out);
    bool writeMaterialTextureSlotHeaderV9(FILE *fp, const util::MATERIAL_TEXTURE_SLOT_HEADER &in);

    bool readHeaderImgV8(FILE *fp, util::HEADER_IMG &out);
    bool writeHeaderImgV8(FILE *fp, const util::HEADER_IMG &in);

    bool readExtraHeaderV8(FILE *fp, util::EXTRA_HEADER &out);
    bool writeExtraHeaderV8(FILE *fp, const util::EXTRA_HEADER &in);

    bool readInfoDrawModeV8(FILE *fp, util::INFO_DRAW_MODE &out);
    bool writeInfoDrawModeV8(FILE *fp, const util::INFO_DRAW_MODE &in);

    bool readDetailMeshV8(FILE *fp, util::DETAIL_MESH &out);
    bool writeDetailMeshV8(FILE *fp, const util::DETAIL_MESH &in);

    bool readDetailHeaderFontV8(FILE *fp, util::DETAIL_HEADER_FONT &out);
    bool writeDetailHeaderFontV8(FILE *fp, const util::DETAIL_HEADER_FONT &in);

    bool readDetailLetterV8(FILE *fp, util::DETAIL_LETTER &out);
    bool writeDetailLetterV8(FILE *fp, const util::DETAIL_LETTER &in);

    bool readBtileHeaderMapV8(FILE *fp, util::BTILE_HEADER_MAP &out);
    bool writeBtileHeaderMapV8(FILE *fp, const util::BTILE_HEADER_MAP &in);

    bool readBtileBrickInfoV8(FILE *fp, util::BTILE_BRICK_INFO &out);
    bool writeBtileBrickInfoV8(FILE *fp, const util::BTILE_BRICK_INFO &in);

    bool readBtileIndexTileV8(FILE *fp, util::BTILE_INDEX_TILE &out);
    bool writeBtileIndexTileV8(FILE *fp, const util::BTILE_INDEX_TILE &in);

    bool readBtileDetailHeaderV8(FILE *fp, util::BTILE_DETAIL_HEADER &out);
    bool writeBtileDetailHeaderV8(FILE *fp, const util::BTILE_DETAIL_HEADER &in);

    bool readBtileObjHeaderV8(FILE *fp, util::BTILE_OBJ_HEADER &out);
    bool writeBtileObjHeaderV8(FILE *fp, const util::BTILE_OBJ_HEADER &in);

    bool readBtilePropertyHeaderV8(FILE *fp, util::BTILE_PROPERTY_HEADER &out);
    bool writeBtilePropertyHeaderV8(FILE *fp, const util::BTILE_PROPERTY_HEADER &in);

    bool readBtileBrickInfoArrayV8(FILE *fp, util::BTILE_BRICK_INFO *out, uint32_t count);
    bool writeBtileBrickInfoArrayV8(FILE *fp, const util::BTILE_BRICK_INFO *in, uint32_t count);

    bool readBtileIndexTileArrayV8(FILE *fp, util::BTILE_INDEX_TILE *out, uint32_t count);
    bool writeBtileIndexTileArrayV8(FILE *fp, const util::BTILE_INDEX_TILE *in, uint32_t count);

    bool readStageParticleV8(FILE *fp, util::STAGE_PARTICLE &out);
    bool writeStageParticleV8(FILE *fp, const util::STAGE_PARTICLE &in);

    bool readFloat3ArrayV8(FILE *fp, float out[3]);
    bool writeFloat3ArrayV8(FILE *fp, const float in[3]);

    bool readU16ArrayV8(FILE *fp, uint16_t *out, uint32_t count);
    bool writeU16ArrayV8(FILE *fp, const uint16_t *in, uint32_t count);

    bool readFloatArrayV8(FILE *fp, float *out, uint32_t count);
    bool writeFloatArrayV8(FILE *fp, const float *in, uint32_t count);

    bool readVec2V8(FILE *fp, mbm::VEC2 &out);
    bool writeVec2V8(FILE *fp, const mbm::VEC2 &in);

    bool readVec2ArrayV8(FILE *fp, mbm::VEC2 *out, uint32_t count);
    bool writeVec2ArrayV8(FILE *fp, const mbm::VEC2 *in, uint32_t count);

    bool readVec3V8(FILE *fp, mbm::VEC3 &out);
    bool writeVec3V8(FILE *fp, const mbm::VEC3 &in);

    bool readVec3ArrayV8(FILE *fp, mbm::VEC3 *out, uint32_t count);
    bool writeVec3ArrayV8(FILE *fp, const mbm::VEC3 *in, uint32_t count);

    bool readCubeV8(FILE *fp, mbm::CUBE &out);
    bool writeCubeV8(FILE *fp, const mbm::CUBE &in);

    bool readSphereV8(FILE *fp, mbm::SPHERE &out);
    bool writeSphereV8(FILE *fp, const mbm::SPHERE &in);

    bool readCubeComplexV8(FILE *fp, mbm::CUBE_COMPLEX &out);
    bool writeCubeComplexV8(FILE *fp, const mbm::CUBE_COMPLEX &in);

    bool readTriangleV8(FILE *fp, mbm::TRIANGLE &out);
    bool writeTriangleV8(FILE *fp, const mbm::TRIANGLE &in);
    bool readTriangleLegacyNoPosV8(FILE *fp, mbm::TRIANGLE &out);
}

#endif
