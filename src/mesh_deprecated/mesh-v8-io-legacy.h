#ifndef MESH_V8_IO_LEGACY_H
#define MESH_V8_IO_LEGACY_H

#include <cstdio>
#include <header-mesh.h>
#include <shapes.h>

// Legacy in-memory header (util::HEADER/EXTRA_HEADER) and the old per-field *_VERSION_MBM_HEADER
// version constants for the v1-v10 on-disk format (milestone 21 - relocated out of the shared
// core_mbm header-mesh.h once core_mbm's v11-only load/save path stopped needing them; only this
// offline mesh_deprecated importer still reads real v1-v10 files and branches on their version).
namespace util
{
    #define INITIAL_VERSION_MBM_HEADER     1
    #define SPRITE_INFO_VERSION_MBM_HEADER 2
    #define DETAIL_MESH_VERSION_MBM_HEADER 3
    #define SPACE_SHIP_VERSION_MBM_HEADER  4
    #define MODE_DRAW_VERSION_MBM_HEADER   5
    #define EXTRA_MBM_HEADER_PATH_TEXTURE  6
    #define NORMAL_OPTIONAL_VERSION_MBM_HEADER 7  // since v7: hasNorText[0] semantics changed
    #define STRONG_TYPES_VERSION_MBM_HEADER 8     // v8: new baseline for current mesh generation
    #define MATERIAL_TEXTURE_SLOT_VERSION_MBM_HEADER 9 // v9: per-subset typed material texture slots
    #define TEXTURE_ANIMATION_EFFECT_VERSION_MBM_HEADER 10 // v10: TextureAnimationEffect stored once per animation FX block

    // Legacy in-memory header version (util::HEADER::version) for the old v1-v10 on-disk format.
    // Unrelated to the v11 file format's own version field (FILE_HEADER_V11::formatVersion /
    // MBM_V11_FORMAT_VERSION, header-mesh.h) - this constant stopped advancing once v11 became the
    // only format being written.
    #define LEGACY_HEADER_VERSION     TEXTURE_ANIMATION_EFFECT_VERSION_MBM_HEADER

    #define MBM_HEADER_NAME_COMPARE_LENGTH 3
    #define MBM_HEADER_TYPE_APP_COMPARE_LENGTH 15
    #define MBM_HEADER_NAME_MBM "mbm"
    #define MBM_TYPE_APP_MESH_3D "Mesh 3d mbm"
    #define MBM_TYPE_APP_USER "User mbm"
    #define MBM_TYPE_APP_FONT "Font mbm"
    #define MBM_TYPE_APP_SPRITE "Sprite mbm"
    #define MBM_TYPE_APP_TILE "Tile mbm"
    #define MBM_TYPE_APP_SHAPE "Shape mbm"
    #define MBM_TYPE_APP_PARTICLE "Particle mbm"
    #define MBM_TYPE_APP_TEXTURE "Texture mbm"
    #define MBM_EXTRA_HEADER_TYPE_PATHS 1
    #define MBM_DEPRECATED_DETAIL_TYPE_SCRIPT 100
    #define MBM_DEPRECATED_DETAIL_TYPE_SHADER 101

    struct HEADER
    {
        char name[16];          // must be "mbm"
        char typeApp[16];       // "Mesh 3d mbm", "User mbm", "Font", "Particle", "Sprite mbm", "Tile mbm"
        int32_t version;            // legacy v1-v10 header version, see LEGACY_HEADER_VERSION
        uint32_t magic;             // must be 0x010203ff.
        int32_t reserved;           // reserved (Must be 0)
        int32_t backBufferWidth;    // Indica o tamanho da largura do back buffer em que o objeto foi criado
        int32_t backBufferHeight;   // Indica o tamanho da altura do back buffer em que o objeto foi criado
        int32_t extraHeader;        // Quando indica quantidade de estrutura EXTRA_HEADER logo apos este frame
        HEADER() noexcept;
    };

    struct EXTRA_HEADER //added since version 6
    {
        char type;           // 0 None, 1 = Paths
        int32_t sizeExtraHeader; // Tamanho extra (em bytes) logo apos este frame
        EXTRA_HEADER() noexcept;
    };

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
