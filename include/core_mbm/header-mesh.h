/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef HEADER_MESH_H
#define HEADER_MESH_H

#include <stdint.h>
#include <vector>
#include <string>
#include "primitives.h"
#include "core-exports.h"

#if (defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32))
    #pragma warning(disable : 4201) //nonstandard extension used : nameless struct/union
#endif

namespace mbm
{
    class TEXTURE;
}

namespace util
{

    struct __HEADER_BMP
    {
        uint8_t identy[2]; //'BM' - Windows 3.1x, 95, NT, ...
        //'BA' - OS/2 Bitmap Array (matriz Bitmap_True_Color_24_Bits)
        //'CI' - OS/2 Color Icon (color icon)
        //'CP' - OS/2 Color Pointer (Ponteiro colorido)
        //'IC' - OS/2 Icone
        //'PT' - OS/2 Ponteiro
        uint8_t length[4];
        uint8_t reserved[4];
        uint8_t offSet[4];
        uint8_t sizeHeader[4]; // 28h - Windows 3.1x, 95, NT, 0Ch - OS/2 1.x, F0h - OS/2 2.x
        uint8_t width[4];
        uint8_t height[4];
        uint8_t plane[2];
        uint8_t bitsPerPixels[2];
        // 1 - Monochrome bitmap (black and white)
        // 4 - Bitmap De 16 cores
        // 8 - Bitmap De 256 cores
        // 16 - Bitmap De 16bits (high color)
        // 24 - Bitmap De 24bits (true color)
        // 32 - Bitmap De 32bits (true color)
        uint8_t compressed[4];
        // 0 - none (Also identified by BI_RGB)
        // 1 - RLE 8 bits/Pixel (Also identified by BI_RLE4)
        // 2 - RLE 4 bits/Pixel (Also identified by BI_RLE8)
        // 3 - Bitfields (Also identified by BI_BITFIELDS)
        uint8_t sizeDataArea[4];
        uint8_t resH[4];
        uint8_t resV[4];
        uint8_t colors[4];
        uint8_t importantsColors[4];
    
        uint32_t getAsUintFromCharPointer(uint8_t *adress);
    };


    enum TYPE_MESH : char
    {
        TYPE_MESH_3D,
        TYPE_MESH_USER,
        TYPE_MESH_SPRITE,
        TYPE_MESH_FONT,
        TYPE_MESH_TEXTURE,
        TYPE_MESH_UNKNOWN,
        TYPE_MESH_SHAPE,
        TYPE_MESH_PARTICLE,
        TYPE_MESH_TILE_MAP,
    };

    struct API_IMPL MATERIAL
    {
        mbm::COLOR Diffuse;
        mbm::COLOR Ambient;
        mbm::COLOR Specular;
        mbm::COLOR Emissive;
        float      Power;
        constexpr MATERIAL() noexcept:
            Diffuse(1.0f,1.0f,1.0f,1.0f) ,
            Ambient(1.0f,1.0f,1.0f,1.0f) ,
            Specular(1.0f,1.0f,1.0f,1.0f) ,
            Emissive(0.0f,0.0f,0.0f,0.0f) ,
            Power(1.0f)
        {}
    };

    using MATERIAL_GLES = MATERIAL;

    /* hasNorText[0] (normals) */
    #define HAS_NOR_NO           0  /* no normals */
    #define HAS_NOR_IN_FILE      1  /* normals stored in file */
    #define HAS_NOR_CALCULATE    2  /* calculate normals from geometry */

    /* hasNorText[1] (UV/texture) */
    #define HAS_TEX_NO           0  /* no texture */
    #define HAS_TEX_EACH_FRAME   1  /* texture in each frame */
    #define HAS_TEX_FIRST_FRAME  2  /* texture only in first frame, others copy */

    #define MBM_ERROR_DESCRIPTION_BUFFER_SIZE 255
    #define MBM_DETAIL_TYPE_CUBE 1
    #define MBM_DETAIL_TYPE_SPHERE 2
    #define MBM_DETAIL_TYPE_CUBE_COMPLEX 3
    #define MBM_DETAIL_TYPE_TRIANGLE 4
    #define MBM_DETAIL_TYPE_FONT 5
    #define MBM_DETAIL_TYPE_PARTICLE 6
    #define MBM_DETAIL_TYPE_TILE 7

    struct API_IMPL INFO_DRAW_MODE //added since version 5
    {
        uint32_t mode_draw; //default (GL_TRIANGLES), mode: GL_POINTS, GL_LINES, GL_LINE_LOOP, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN
        uint32_t mode_cull_face;//GL_FRONT, GL_BACK,GL_FRONT_AND_BACK
        uint32_t mode_front_face_direction; //GL_CW, GL_CCW
        INFO_DRAW_MODE()noexcept;
    };


    struct API_IMPL DETAIL_MESH
    {
        int32_t type; // 1 box, 2 sphere, 3 complex-cube, 4 triangle, 5 header-font,6 particle, 7 Tile, (deprecated 100 script generic, 101 shader)
        int32_t totalBounding;
        DETAIL_MESH() noexcept;
    };


    struct API_IMPL DETAIL_HEADER_FONT // Header font mbm
    {
        uint16_t sizeNameFonte;   // Tamanho do nome da fonte com nullptr terminator
        uint16_t totalDetailFont; // Total de detalhes da fonte (frames - cada letra)
        int16_t  spaceXCharacter; // Espaco entre caracter desta fonte
        int16_t  spaceYCharacter; // Espaco entre caracter desta fonte
        uint16_t heightLetter;    // Altura de cada letra (mesma altura para todas)
    
        DETAIL_HEADER_FONT()noexcept;
    };

    struct API_IMPL DETAIL_LETTER // Detail letter
    {
        uint8_t      letter;       // Letra
        uint8_t      indexFrame;   // Indice do frame na mesh para ser renderizado
        uint16_t widthLetter;  // Largura da fonte (em pixel)
        uint16_t heightLetter; // Altura da fonte (em pixel)
        DETAIL_LETTER()noexcept;
    };


    struct API_IMPL HEADER_MESH // Header principal para objetos 3d MBM
    {
        MATERIAL material;              // Material aplicado nesta subset
        int32_t      totalAnimation;         // Total animations in mesh
        int32_t      totalFrames;            // Total frames for the file. Each frame is divided into one or more subsets.
        int32_t      deprecated_typePhysics; // not used anymore, 'deprecated' (just keep for compatibility,old typePhysics)
        int16_t    hasNorText[2];          // hasNorText[0]: HAS_NOR_NO, HAS_NOR_IN_FILE, HAS_NOR_CALCULATE. hasNorText[1]: HAS_TEX_NO, HAS_TEX_EACH_FRAME, HAS_TEX_FIRST_FRAME
        float angleX, angleY, angleZ; 
        float posX, posY, posZ;       
    
        HEADER_MESH()noexcept;
    };


    struct API_IMPL HEADER_ANIMATION
    {
        char  nameAnimation[32];        // 32 bytes for animation name (31 + null)
        int32_t   initialFrame;             // Initial frame for this animation
        int32_t   finalFrame;               // Final frame for this animation
        float timeBetweenFrame;         // Time between animation frames
        int32_t   typeAnimation;            // Animation type
        uint16_t hasShaderEffect;// 1 if has and 0 if do not has. previously steps shader (old lenMusicFileName mini mbm 1.0), Now must be 1
        uint16_t blendState;  //Blend state for each animation
    
        HEADER_ANIMATION()noexcept;
        HEADER_ANIMATION(HEADER_ANIMATION &) = default;
    };

    struct API_IMPL HEADER_INFO_SHADER_STEP
    {
        int16_t lenNameShader;        // Efeito shader (Pixel ou vertex). 0 nenhum. Indica o tamanho da string + nullptr do arquivo
                                        // Pixel shader ou vertex shader.
        int16_t lenTextureStage2;     // Quando ha textura no segundo estagio 2 para este step
        int16_t sizeArrayVarInBytes;  // Tamanho do array das variaveis do Shader em bytes
        int16_t typeAnimation;        // 0 - 6
        int32_t   blendOperation;       // Tipo de operacao blend nos steps
        float     timeAnimation;        // Animation time
        HEADER_INFO_SHADER_STEP()noexcept;
    };

    struct API_IMPL INFO_SHADER_DATA
    {
    
        char *    fileNameShader;
        char *    fileNameTextureStage2;
        char *    typeVars;
        float *   min;
        float *   max;
        int       lenVars;
        float     timeAnimation;
        int16_t typeAnimation; // 0 - 6
        INFO_SHADER_DATA(const int sizeArrayInBytes, const int sizeFileNameShader, const int sizeFileNameTextureStage2);
    
        ~INFO_SHADER_DATA()noexcept;
    };

    struct INFO_FX
    {
        INFO_SHADER_DATA *dataPS;          // Data do pixel Shader
        INFO_SHADER_DATA *dataVS;          // Data do vertex Shader
        char *             fileNameTextureAnimationEffect; // Canonical animation-level FX texture path
        int                blendOperation;  // Tipo de operacao blend nos steps
        API_IMPL INFO_FX()noexcept;
        API_IMPL ~INFO_FX()noexcept;
        API_IMPL void setTextureAnimationEffectFileName(const char *fileName) noexcept;
        API_IMPL const char *getTextureAnimationEffectFileName() const noexcept;
        API_IMPL bool normalizeLegacyTextureAnimationEffectPaths() noexcept;
    };

    struct INFO_ANIMATION
    {
        struct INFO_HEADER_ANIM
        {
            util::HEADER_ANIMATION *    headerAnim;         
            INFO_FX *			effectShader; 
            API_IMPL INFO_HEADER_ANIM()noexcept;
            API_IMPL ~INFO_HEADER_ANIM();
        };
        std::vector<INFO_HEADER_ANIM *> lsHeaderAnim;//each info for each animation
        API_IMPL INFO_ANIMATION()noexcept;
        API_IMPL ~INFO_ANIMATION();
        API_IMPL void release();
    };


    struct API_IMPL HEADER_FRAME 
    {
        int32_t totalSubset;        // Total de subset para este frame
        int32_t sizeIndexBuffer;    // Tamanho do Index buffer(se houver) deste frame.
        int32_t sizeVertexBuffer;   // Tamanho do vertex buffer deste frame.
        int32_t stride;             // 3 para x,y z ou 2 para x e y.
        char typeBuffer[4];     // Tipo do buffer VB par vertex buffer e IB para indexBuffer
        HEADER_FRAME()noexcept;
    };


    struct API_IMPL HEADER_DESC_SUBSET 
    {
        char nameTexture[64]; // 64 bytes para o nome da textura (63 + null) desta subset
        int32_t  vertexCount;     // Total de vertex no subset
        int32_t  vertexStart;     // Inicio do vertex
        int32_t  indexStart;      // Index start
        int32_t  indexCount;      // Total de indices
        union {
            struct
            {
                uint8_t alphaColor[4]; // First byte indicates if alpha color exists; remaining bytes are the colors.
                                             // (kept for compatibility but colorKeying no longer used)
            };
            struct
            {
                uint8_t hasAlphaColor; // Flag indicando se existe alpha
                uint8_t r, g, b;       // Cor  color alpha
            };
        };
        uint16_t materialTextureSlotCount;
        uint16_t reservedMaterialTextureSlots;
    
        HEADER_DESC_SUBSET()noexcept;
    };

    struct MATERIAL_TEXTURE_SLOT_HEADER
    {
        uint16_t type;
        uint16_t reserved;
        uint32_t payloadSizeInBytes;
        char nameTexture[64];

        API_IMPL MATERIAL_TEXTURE_SLOT_HEADER()noexcept;
    };

    // Legacy in-memory editor slot-id numbering (1-4) for MATERIAL_TEXTURE_SLOT_DEBUG::type below -
    // unrelated to mbm::TEXTURE_ROLE (the v11 on-disk role byte); see
    // legacyMaterialSlotTypeToTextureRole/textureRoleToLegacyMaterialSlotType in mesh-manager.cpp.
    enum MATERIAL_TEXTURE_SLOT_TYPE : uint16_t
    {
        MATERIAL_TEXTURE_SLOT_NORMAL   = 1,
        MATERIAL_TEXTURE_SLOT_SPECULAR = 2,
        MATERIAL_TEXTURE_SLOT_EMISSIVE = 3,
        MATERIAL_TEXTURE_SLOT_MASK     = 4,
    };

    struct MATERIAL_TEXTURE_SLOT_DEBUG
    {
        uint16_t    type;
        std::string texture;

        API_IMPL MATERIAL_TEXTURE_SLOT_DEBUG()noexcept;
    };

    struct API_IMPL HEADER_IMG
    {
        uint32_t       width;
        uint32_t       height;
        uint16_t depth;   // 8, 4 ou 3
        uint16_t channel; // 3 ou 4
        uint32_t       lenght;  // Tamanho comprimido
        union {
            struct
            {
                uint8_t alphaColor[4]; // First byte indicates if alpha color exists; remaining bytes are the colors
                                             // (kept for compatibility but colorKeying no longer used)
            };
            struct
            {
                uint8_t hasAlpha; // Flag indicando se existe alpha na textura
                uint8_t r, g, b;  // Cor  color alpha
            };
        };
    };


    struct SUBSET_DEBUG
    {
        std::string texture;
        std::vector<MATERIAL_TEXTURE_SLOT_DEBUG> materialTextureSlots;
        int         vertexStart; 
        int         indexStart;  
        int         vertexCount; 
        int         indexCount;  
        API_IMPL SUBSET_DEBUG()noexcept;
    };

    struct BUFFER_MESH_DEBUG
    {
        float *                     position;
        float *                     normal;
        float *                     uv;
        uint16_t *        indexBuffer;
        std::vector<SUBSET_DEBUG *> subset;
        util::HEADER_FRAME          headerFrame;
        API_IMPL BUFFER_MESH_DEBUG()noexcept;
        API_IMPL virtual ~BUFFER_MESH_DEBUG();
        API_IMPL void release();
    };


    struct SUBSET
    {
        mbm::TEXTURE *texture;
        std::vector<MATERIAL_TEXTURE_SLOT_HEADER> materialTextureSlotHeaders;
        std::vector<mbm::TEXTURE*>                materialTextures;
        int           vertexStart; // Inicial do vertex
        int           indexStart;  // Index start
        int           vertexCount; // Total de vertex no subset
        int           indexCount;  // Total de index no subset
        API_IMPL SUBSET()noexcept;
    };

    struct API_IMPL STAGE_PARTICLE
    {
        mbm::VEC3  minOffsetPosition;
        mbm::VEC3  maxOffsetPosition;
        mbm::VEC3  minDirection;
        mbm::VEC3  maxDirection;
        mbm::VEC3  minColor;
        mbm::VEC3  maxColor;
        float minSpeed;
        float maxSpeed;
        float minTimeLife; // seconds
        float maxTimeLife; // seconds
        float minSizeParticle;
        float maxSizeParticle;
        float ariseTime;
        float stageTime;
        uint32_t totalParticle;
        uint8_t  segmented;
        uint8_t  sizeMin2Max;
        uint8_t  revive;
        uint8_t  _operator;
        uint8_t  invert_red;
        uint8_t  invert_green;
        uint8_t  invert_blue;
        uint8_t  invert_alpha;

        STAGE_PARTICLE()noexcept;
        STAGE_PARTICLE(const STAGE_PARTICLE* other)noexcept;
    };

    struct API_IMPL BTILE_INDEX_TILE
    {
        uint32_t index;
        float x,y;
        BTILE_INDEX_TILE() noexcept;
        ~BTILE_INDEX_TILE()noexcept = default;
    };

    struct BTILE_LAYER
    {
        BTILE_INDEX_TILE* lsIndexTiles;
        float offset[3]; // plus z
        API_IMPL BTILE_LAYER()noexcept;
        API_IMPL ~BTILE_LAYER()noexcept;
    };

    enum BTILE_TYPE_MAP : uint32_t
    {
        BTILE_TYPE_ORIENTATION_ORTHOGONAL,
        BTILE_TYPE_ORIENTATION_ISOMETRIC,
        BTILE_TYPE_ORIENTATION_STAGGERED,
        BTILE_TYPE_ORIENTATION_HEXAGONAL,
    };

    struct API_IMPL BTILE_BRICK_INFO
    {
        BTILE_BRICK_INFO() noexcept;
        ~BTILE_BRICK_INFO()noexcept = default;
        uint16_t index;
        uint16_t original_index;
        uint16_t rotation;
        uint16_t flipped;
    };

    struct API_IMPL BTILE_HEADER_MAP
    {
        BTILE_HEADER_MAP()noexcept;
        ~BTILE_HEADER_MAP()noexcept = default;

        uint32_t        count_width_tile;
        uint32_t        count_height_tile;
        uint32_t        size_width_tile;
        uint32_t        size_height_tile;
        uint32_t        layerCount;
        uint32_t        countRawTiles;//total distinct tiles as fame buffer
        uint32_t        objectCount;
        uint32_t        propertyCount;
        BTILE_TYPE_MAP  typeMap;
        uint32_t        background;
        char            background_texture[62];
        char            renderDirection[2];
    };

    enum BTILE_OBJ_TYPE : uint16_t
    {
        BTILE_OBJ_TYPE_UNKNOWN,
        BTILE_OBJ_TYPE_RECT,
        BTILE_OBJ_TYPE_CIRCLE,
        BTILE_OBJ_TYPE_TRIANGLE,
        BTILE_OBJ_TYPE_POINT,
        BTILE_OBJ_TYPE_POLYLINE,
    };

    enum BTILE_PROPERTY_TYPE : uint16_t
    {
        BTILE_PROPERTY_TYPE_UNKNOWN,
        BTILE_PROPERTY_TYPE_BOOL,
        BTILE_PROPERTY_TYPE_COLOR,
        BTILE_PROPERTY_TYPE_FLOAT,
        BTILE_PROPERTY_TYPE_FILE,
        BTILE_PROPERTY_TYPE_INT,
        BTILE_PROPERTY_TYPE_STRING,
    };

    struct BTILE_PROPERTY
    {
        API_IMPL BTILE_PROPERTY()noexcept;
        API_IMPL BTILE_PROPERTY(const BTILE_PROPERTY & other )noexcept;
        API_IMPL BTILE_PROPERTY(const BTILE_PROPERTY_TYPE Type)noexcept;
        std::string owner;
        std::string name;
        std::string value;
        BTILE_PROPERTY_TYPE type;
    };
    
    struct API_IMPL BTILE_PROPERTY_HEADER
    {
        BTILE_PROPERTY_HEADER()noexcept;
        BTILE_PROPERTY_HEADER(const BTILE_PROPERTY * property)noexcept;
        uint16_t type;
        uint16_t nameLength;
        uint16_t valueLength;
        uint16_t ownerLength;
    };

    struct BTILE_OBJ
    {
        API_IMPL BTILE_OBJ()noexcept;
        API_IMPL BTILE_OBJ(const BTILE_OBJ & other)noexcept;
        API_IMPL BTILE_OBJ(const BTILE_OBJ_TYPE Type)noexcept;
        API_IMPL BTILE_OBJ(const BTILE_OBJ_TYPE Type,std::string  Name)noexcept;
        API_IMPL ~BTILE_OBJ()noexcept;
        BTILE_OBJ_TYPE				type;
        std::string					name;
                std::vector<mbm::VEC2*>     lsPoints;
    };

    struct API_IMPL BTILE_DETAIL_HEADER
    {
        BTILE_DETAIL_HEADER()noexcept;
        uint32_t totalObj;
        uint32_t totalProperties;
    };

    struct API_IMPL BTILE_OBJ_HEADER
    {
        BTILE_OBJ_HEADER()noexcept;
        BTILE_OBJ_HEADER(const BTILE_OBJ * obj)noexcept;
        uint16_t	sizeName;
        uint16_t	type;
        uint16_t	sizePoints;
    };

    struct BTILE_INFO
    {
        BTILE_HEADER_MAP			map;
        BTILE_LAYER*				layers;
        BTILE_BRICK_INFO*           infoBrickEditor;
        std::vector<BTILE_OBJ*>		lsObj;
        std::vector<BTILE_PROPERTY*> lsProperty;
        API_IMPL BTILE_INFO* clone()const;
        API_IMPL BTILE_INFO() noexcept;
        API_IMPL ~BTILE_INFO() noexcept;
    };

    struct API_IMPL DYNAMIC_SHAPE //for dynamic shape
    {
        float * dynamicVertex;
        float * dynamicNormal;
        float * dynamicUV;
        const unsigned int  size_vertex;
        const unsigned int  size_normal;
        const unsigned int  size_uv;
        DYNAMIC_SHAPE(float * vertex,float * normal, float *uv, const unsigned int sv, const unsigned int sn, const unsigned int suv) noexcept;
        ~DYNAMIC_SHAPE() noexcept = default;
    };

    // -----------------------------------------------------------------------------------------
    // Mesh v11 format (docs/mesh-v11-format.md) - section/TLV envelope.
    // -----------------------------------------------------------------------------------------

    #define MBM_V11_MAGIC "MBM1"
    #define MBM_V11_FORMAT_VERSION 11

    enum SECTION_TYPE : uint16_t
    {
        SECTION_MATERIAL_TRANSFORM = 1,  // material + angle/pos + draw mode (replaces HEADER_MESH + INFO_DRAW_MODE)
        SECTION_ANIMATION          = 2,  // repeated: one per animation, in order, including its FX block
        SECTION_FRAME_STATIC       = 10, // repeated: one per frame, in order
        SECTION_FRAME_SKINNED      = 11, // bundled joint hierarchy for editor/mesh_debug.lua's
                                          // Bones node round-trip diagnostic - never runtime skinning
        SECTION_ARTICULATED_PARTS  = 12, // optional rigid-part identities, pivots, and future hierarchy metadata
        SECTION_ARTICULATED_ANIMATION = 13, // optional rigid/articulated animation clips and tracks
        SECTION_DETAIL_PHYSICS     = 20,
        SECTION_DETAIL_FONT        = 21,
        SECTION_DETAIL_PARTICLE    = 22,
        SECTION_DETAIL_TILE        = 23,
        SECTION_EXTRA_PATHS        = 30, // replaces legacy EXTRA_HEADER type==1 path-registration hint
        SECTION_VERTEX_SKIN_WEIGHTS = 40, // bundled per-vertex bone weight palette, one entry per
                                          // SECTION_FRAME_STATIC frame-1 vertex - editor/diagnostic +
                                          // FBX re-export round-trip data only, same scope as
                                          // SECTION_FRAME_SKINNED (the engine has no GPU/CPU skinning
                                          // anywhere). See VERTEX_SKIN_WEIGHTS_HEADER_V11/
                                          // VERTEX_BONE_WEIGHT_V11 below and docs/mesh-v11-format.md
                                          // Sec. 6f. NOTE: unlike the comment that used to sit here,
                                          // an unrecognized section type is NOT actually a safe no-op
                                          // for parse_v11_intermediate or MESH_MBM_DEBUG::loadV11 (both
                                          // hard-fail on an unknown `type` - only the lightweight
                                          // MESH_MBM_DEBUG::getInfo probe truly skips unknown types) -
                                          // this section type has explicit handling in both real
                                          // loaders precisely because of that.
    };

    enum SECTION_COMPRESSION : uint8_t
    {
        SECTION_COMPRESSION_NONE    = 0,
        SECTION_COMPRESSION_DEFLATE = 1,
    };

    struct API_IMPL FILE_HEADER_V11
    {
        char     magic[4];        // MBM_V11_MAGIC ("MBM1"), checked first, before anything else is trusted
        uint16_t formatVersion;   // MBM_V11_FORMAT_VERSION, independent of `magic`
        uint8_t  typeMesh;        // util::TYPE_MESH value directly
        uint8_t  reserved0;       // must be 0
        int32_t  backBufferWidth;
        int32_t  backBufferHeight;
        uint32_t sectionCount;    // number of SECTION_HEADER_V11 blocks that follow, back-to-back
        FILE_HEADER_V11() noexcept;
    };

    struct API_IMPL SECTION_HEADER_V11
    {
        uint16_t type;               // SECTION_TYPE
        uint16_t sectionVersion;     // per-section-type version
        uint8_t  compression;        // SECTION_COMPRESSION
        uint8_t  reserved1[3];       // must be 0
        uint32_t uncompressedLength;
        uint32_t compressedLength;   // == uncompressedLength when compression == SECTION_COMPRESSION_NONE
        uint32_t crc32Value;         // mz_crc32() of the *uncompressed* payload, always written.
                                      // Named crc32Value, not crc32: miniz.h #defines crc32 to
                                      // mz_crc32 for zlib-API compat, which mangles a field literally
                                      // named crc32 in any TU that includes both headers.
        SECTION_HEADER_V11() noexcept;
    };

    // Non-owning cursor over an already-in-memory byte buffer - typically a v11 section's
    // decompressed payload. Lets the v11/v8 payload-level readers (mesh-v11-io.h/mesh-v8-io.h)
    // consume bytes without staging them through a real OS temp file first (superseded an earlier
    // tmpfile()-per-section design). Declared here (not in the internal mesh-io-primitives.h where
    // its read primitives live) because
    // MESH_MBM_DEBUG's class declaration (mesh-manager.h, a public header) needs the type visible.
    // Read-only: nothing ever serializes a payload back out of memory through this type, only into
    // a real FILE* via writeSectionV11Streamed.
    struct MEM_CURSOR_V11
    {
        const uint8_t *data;
        size_t         size;
        size_t         pos;
    };

    // -----------------------------------------------------------------------------------------
    // Mesh v11 section payloads (docs/mesh-v11-format.md Sec. 6): material transform, static frames,
    // physics detail, extra paths, animation/FX, and font/particle/tile detail.
    // -----------------------------------------------------------------------------------------

    struct API_IMPL FRAME_HEADER_V11
    {
        uint32_t totalSubset;
        uint32_t vertexCount;
        uint8_t  indexWidth;   // 16 or 32 (only 16 ever emitted by saveV11)
        uint8_t  hasNormal;    // bool
        uint8_t  hasUv;        // bool
        uint8_t  uvSource;     // 0 = OWN, 1 = SHARED_WITH_FRAME_0
        uint32_t indexCount;   // in indices, not bytes
        FRAME_HEADER_V11() noexcept;
    };

    enum TEXTURE_REF_STORAGE_V11 : uint8_t
    {
        TEXTURE_REF_STORAGE_PATH               = 0,
        TEXTURE_REF_STORAGE_EMBEDDED_COMPRESSED = 1, // reserved - saveV11 never emits this
    };

    // Not struct-level API_IMPL: a std::string member (directly or via a nested V11 struct) would
    // need its own dll-interface (MSVC C4251) to be exported wholesale. These are plain in-process
    // data carriers - only the constructor symbol needs to cross the DLL boundary, like
    // MATERIAL_TEXTURE_SLOT_DEBUG above.
    struct TEXTURE_REF_V11
    {
        uint8_t     storage; // TEXTURE_REF_STORAGE_V11
        std::string path;    // meaningful only when storage == TEXTURE_REF_STORAGE_PATH
        API_IMPL TEXTURE_REF_V11() noexcept;
    };

    struct SUBSET_DESC_V11
    {
        TEXTURE_REF_V11 primaryTexture; // implicit role TEXTURE_ROLE_DIFFUSE, always present
        int32_t  vertexCount;
        int32_t  vertexStart;
        int32_t  indexStart;
        int32_t  indexCount;
        uint8_t  alphaColor[4];
        uint16_t extraSlotCount; // caller must set before writing; followed on disk by
                                  // extraSlotCount * SUBSET_EXTRA_SLOT_V11
        API_IMPL SUBSET_DESC_V11() noexcept;
    };

    struct SUBSET_EXTRA_SLOT_V11
    {
        uint8_t         role; // an mbm::TEXTURE_ROLE value (NORMAL/SPECULAR/EMISSIVE/MASK only)
        TEXTURE_REF_V11 texture;
        API_IMPL SUBSET_EXTRA_SLOT_V11() noexcept;
    };

    struct API_IMPL MATERIAL_TRANSFORM_V11 // payload for SECTION_MATERIAL_TRANSFORM
    {
        MATERIAL material;
        float    angleX, angleY, angleZ;
        float    posX, posY, posZ;
        uint32_t mode_draw;
        uint32_t mode_cull_face;
        uint32_t mode_front_face_direction;
        MATERIAL_TRANSFORM_V11() noexcept;
    };

    struct API_IMPL SHADER_VAR_V11
    {
        uint8_t typeVar; // mbm::TYPE_VAR_SHADER
        float   min[4];
        float   max[4];
        SHADER_VAR_V11() noexcept;
    };

    struct SHADER_STEP_V11
    {
        std::string name;          // shader name reference (built-in or custom), e.g. "transparent.ps"
        float       timeAnimation;
        int32_t     typeAnimation; // 0-6, shader-level playback type
        uint16_t    varCount;      // caller must set before writing; followed on disk by
                                     // varCount * SHADER_VAR_V11
        API_IMPL SHADER_STEP_V11() noexcept;
    };

    struct FX_HEADER_V11 // payload for ANIMATION_HEADER_V11.hasFx == 1
    {
        int32_t         blendOperation;
        uint8_t         hasFxTexture; // bool
        TEXTURE_REF_V11 fxTexture;    // meaningful only if hasFxTexture - role TEXTURE_ROLE_ANIMATION_EFFECT
        uint8_t         hasPS;        // bool
        SHADER_STEP_V11 ps;           // meaningful only if hasPS
        uint8_t         hasVS;        // bool
        SHADER_STEP_V11 vs;           // meaningful only if hasVS
        API_IMPL FX_HEADER_V11() noexcept;
    };

    struct ANIMATION_HEADER_V11 // payload for SECTION_ANIMATION
    {
        std::string name;            // replaces today's nameAnimation[32]
        int32_t     initialFrame;
        int32_t     finalFrame;
        float       timeBetweenFrame;
        int32_t     typeAnimation;
        uint16_t    blendState;
        uint8_t     hasFx;           // bool - if 1, an FX_HEADER_V11 follows
        API_IMPL ANIMATION_HEADER_V11() noexcept;
    };

    struct ARTICULATED_PARTS_HEADER_V11 // payload header for SECTION_ARTICULATED_PARTS
    {
        uint32_t partCount;
        API_IMPL ARTICULATED_PARTS_HEADER_V11() noexcept;
    };

    struct ARTICULATED_PART_V11 // one frame/subset occurrence; partId may be shared across frames
    {
        uint64_t    partId;
        uint32_t    frameIndex;
        uint32_t    subsetIndex;
        uint64_t    parentPartId; // 0 means no parent; parent must belong to the same frame
        std::string name;
        float       pivotX, pivotY, pivotZ;
        float       pivotQX, pivotQY, pivotQZ, pivotQW;
        API_IMPL ARTICULATED_PART_V11() noexcept;
    };

    struct ARTICULATED_ANIMATION_HEADER_V11 // payload header for SECTION_ARTICULATED_ANIMATION
    {
        uint32_t clipCount;
        API_IMPL ARTICULATED_ANIMATION_HEADER_V11() noexcept;
    };

    struct ARTICULATED_CLIP_V11
    {
        std::string name;
        float       duration;
        float       speed;
        int32_t     defaultPriority;
        uint8_t     loop;
        API_IMPL ARTICULATED_CLIP_V11() noexcept;
    };

    enum ARTICULATED_CHANNEL_MASK_V11 : uint8_t
    {
        ARTICULATED_CHANNEL_POSITION = 1,
        ARTICULATED_CHANNEL_ROTATION = 2,
        ARTICULATED_CHANNEL_SCALE    = 4,
    };

    struct ARTICULATED_TRACK_V11
    {
        uint64_t partId;
        uint8_t  channelMask;
        uint32_t keyCount;
        API_IMPL ARTICULATED_TRACK_V11() noexcept;
    };

    struct ARTICULATED_KEY_V11
    {
        float time;
        float positionX, positionY, positionZ;
        float rotationX, rotationY, rotationZ, rotationW;
        // Authoring rotation in degrees. Quaternion fields remain the runtime representation.
        float rotationEulerX, rotationEulerY, rotationEulerZ;
        uint8_t hasRotationEuler;
        float scaleX, scaleY, scaleZ;
        API_IMPL ARTICULATED_KEY_V11() noexcept;
    };

    struct FONT_DETAIL_HEADER_V11 // payload header for SECTION_DETAIL_FONT
    {
        std::string name;            // replaces today's sizeNameFonte-prefixed buffer
        int16_t     spaceXCharacter;
        int16_t     spaceYCharacter;
        uint16_t    heightLetter;
        uint16_t    letterCount;     // count of DETAIL_LETTER-shaped entries that follow
        API_IMPL FONT_DETAIL_HEADER_V11() noexcept;
    };

    struct TILE_HEADER_MAP_V11 // payload header for SECTION_DETAIL_TILE
    {
        uint32_t    count_width_tile;
        uint32_t    count_height_tile;
        uint32_t    size_width_tile;
        uint32_t    size_height_tile;
        uint32_t    layerCount;
        uint32_t    countRawTiles;     // count of BTILE_BRICK_INFO-shaped entries that follow
        uint32_t    objectCount;       // count of TILE_OBJ_HEADER_V11 entries (after all layers)
        uint32_t    propertyCount;     // count of TILE_PROPERTY_V11 entries (after all objects)
        uint32_t    typeMap;           // util::BTILE_TYPE_MAP
        uint32_t    background;
        std::string backgroundTexture; // replaces today's fixed background_texture[62]
        uint8_t     renderDirectionLeftToRight;
        uint8_t     renderDirectionTopToDown;
        API_IMPL TILE_HEADER_MAP_V11() noexcept;
    };

    struct API_IMPL TILE_LAYER_HEADER_V11 // one per layer, followed by its BTILE_INDEX_TILE array
    {
        float offsetX;
        float offsetY;
        float offsetZ;
        TILE_LAYER_HEADER_V11() noexcept;
    };

    struct TILE_OBJ_HEADER_V11 // one per BTILE_OBJ, followed by pointCount (x,y) float pairs
    {
        std::string name;
        uint16_t    type;       // util::BTILE_OBJ_TYPE
        uint16_t    pointCount;
        API_IMPL TILE_OBJ_HEADER_V11() noexcept;
    };

    struct TILE_PROPERTY_V11 // one per BTILE_PROPERTY
    {
        std::string owner;
        std::string name;
        std::string value;
        uint16_t    type;       // util::BTILE_PROPERTY_TYPE
        API_IMPL TILE_PROPERTY_V11() noexcept;
    };

    // -----------------------------------------------------------------------------------------
    // SECTION_FRAME_SKINNED payload (docs/mesh-v11-format.md Sec. 6e) - NOT runtime skeletal
    // animation (the engine has no GPU/CPU skinning anywhere). This is exclusively a diagnostic
    // round-trip mechanism for editor/mesh_debug.lua's Bones node: one optional joint hierarchy
    // per mesh file, independent of and coexisting with any SECTION_FRAME_STATIC geometry the
    // same file already has. Bundled like SECTION_DETAIL_TILE (one section, internal count
    // prefix), not repeated-per-item like SECTION_ANIMATION - there is exactly one skeleton per
    // file.
    // -----------------------------------------------------------------------------------------

    struct API_IMPL SKELETON_HEADER_V11 // payload header for SECTION_FRAME_SKINNED
    {
        uint16_t jointCount; // count of SKELETON_BONE_V11 entries that follow, in parent-before-child order
        SKELETON_HEADER_V11() noexcept;
    };

    // sectionVersion 1: only the first 6 fields below (name..radius) are present on disk.
    // sectionVersion 2: all 13 fields are present. readSkeletonBoneV11 is handed the section's own
    // sectionVersion and defaults rotX/Y/Z=0, scaleX/Y/Z=1, length=0 (this struct's own ctor
    // defaults) when reading a v1 file - see docs/mesh-v11-format.md Sec. 6e.
    struct SKELETON_BONE_V11 // one per bone; parentName empty ("") marks the root
    {
        std::string name;
        std::string parentName; // must equal the `name` of a SKELETON_BONE_V11 already emitted
                                 // earlier in this same section (root-first order) - empty = root bone
        float       x, y, z;    // bone position, same coordinate convention as the caller's mesh
        float       radius;     // authoring-time bone radius (envelope/gizmo marker size) -
                                 // orthogonal to rotation/scale/length below
        float       rotX, rotY, rotZ; // bone orientation, Euler degrees, same non-parent-relative
                                 // world/armature-space convention as x,y,z above. Engine's own
                                 // X-then-Y-then-Z composition order (MatrixRotationX/Y/Z,
                                 // src/core_mbm/primitives.cpp), matching mesh_debug.lua's
                                 // rotateX/Y/Z and MESH_MBM_DEBUG::rotateFrame exactly.
        float       scaleX, scaleY, scaleZ; // bone-local scale, default 1,1,1
        float       length;     // bone extent along its own local +Y axis (Blender's own
                                 // head->tail convention): tail = head + Rotate(rotX,rotY,rotZ)
                                 // applied to Vector(0, length, 0). 0 means "no orientation data
                                 // available" (v1 file, or a synthesized/hand-authored bone) -
                                 // consumers needing a tail direction should fall back to inferring
                                 // it from position topology in that case, not trust rotX/Y/Z.
        API_IMPL SKELETON_BONE_V11() noexcept;
    };

    // -----------------------------------------------------------------------------------------
    // SECTION_VERTEX_SKIN_WEIGHTS payload (docs/mesh-v11-format.md Sec. 6f) - NOT runtime skeletal
    // animation (the engine has no GPU/CPU skinning anywhere), same scope as SECTION_FRAME_SKINNED:
    // a diagnostic/editor round-trip mechanism, here specifically so editor/mesh_debug.lua's
    // "Export to FBX" can write the mesh's REAL, originally-authored per-vertex bone weights
    // instead of inventing new ones from scratch via Blender's ARMATURE_ENVELOPE geometric
    // approximation. Bundled like SECTION_FRAME_SKINNED (one section, internal count prefix), not
    // repeated-per-frame - skin weights are a bind-pose property (they don't vary per animation
    // frame, only bone transforms would, and this engine doesn't even apply those), so this
    // section's vertexCount must equal SECTION_FRAME_STATIC frame 1's own FRAME_HEADER_V11.vertexCount;
    // it has nothing to say about any other frame's geometry.
    //
    // Bones are referenced by a small per-section name palette (VERTEX_SKIN_WEIGHTS_HEADER_V11's
    // own bone-name list, written once), not by raw index into SECTION_FRAME_SKINNED's own bone
    // array - that array can be resorted/renamed/have entries removed later via
    // updateBone/removeBone/addBone, which would silently invalidate a raw index but leaves a
    // name-based reference correct (or gives a clean "unknown bone" lookup miss instead of quietly
    // pointing at the wrong bone).
    // -----------------------------------------------------------------------------------------

    struct API_IMPL VERTEX_SKIN_WEIGHTS_HEADER_V11 // payload header for SECTION_VERTEX_SKIN_WEIGHTS
    {
        uint32_t paletteCount; // count of bone-name strings that follow (unique bones referenced by any vertex)
        uint32_t vertexCount;  // count of VERTEX_BONE_WEIGHT_V11 entries that follow; must equal
                                // SECTION_FRAME_STATIC frame 1's own vertexCount
        VERTEX_SKIN_WEIGHTS_HEADER_V11() noexcept;
    };

    // One per vertex (frame-1 vertex order), fixed at 4 influences - matches this codebase's own
    // existing convention (blender_mesh_skeleton_export.py's vertex_group_limit_total(4) +
    // vertex_group_normalize_all, already applied to its envelope-weight fallback path before this
    // section existed). An unused slot has paletteIndex == 0xFF and weight == 0.0f; used slots'
    // weights should sum to ~1.0 (not enforced on read - a caller that wrote unnormalized weights
    // gets them back exactly as given).
    struct VERTEX_BONE_WEIGHT_V11
    {
        uint8_t paletteIndex[4]; // index into this section's own VERTEX_SKIN_WEIGHTS_HEADER_V11 palette; 0xFF = unused slot
        float   weight[4];
        API_IMPL VERTEX_BONE_WEIGHT_V11() noexcept;
    };

}

#if (defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32))
    #pragma warning(default : 4201) //nonstandard extension used : nameless struct/union
#endif

#endif
