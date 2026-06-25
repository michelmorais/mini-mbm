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

    #define CURRENT_VERSION_MBM_HEADER     TEXTURE_ANIMATION_EFFECT_VERSION_MBM_HEADER

    /* hasNorText[0] (normals) */
    #define HAS_NOR_NO           0  /* no normals */
    #define HAS_NOR_IN_FILE      1  /* normals stored in file */
    #define HAS_NOR_CALCULATE    2  /* calculate normals from geometry */

    /* hasNorText[1] (UV/texture) */
    #define HAS_TEX_NO           0  /* no texture */
    #define HAS_TEX_EACH_FRAME   1  /* texture in each frame */
    #define HAS_TEX_FIRST_FRAME  2  /* texture only in first frame, others copy */

    #define MBM_ERROR_DESCRIPTION_BUFFER_SIZE 255
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
    #define MBM_DETAIL_TYPE_CUBE 1
    #define MBM_DETAIL_TYPE_SPHERE 2
    #define MBM_DETAIL_TYPE_CUBE_COMPLEX 3
    #define MBM_DETAIL_TYPE_TRIANGLE 4
    #define MBM_DETAIL_TYPE_FONT 5
    #define MBM_DETAIL_TYPE_PARTICLE 6
    #define MBM_DETAIL_TYPE_TILE 7

    // Legacy v1-v10 on-disk reference structs (HEADER_DISK_V8, HEADER_MESH_DISK_V8, ...,
    // STAGE_PARTICLE_DISK_V8) moved to header-mesh-legacy-disk.h - they document old byte layouts
    // for the future mesh_deprecated importer (docs/mesh-v11-plan.md milestone 5) but nothing in
    // core_mbm reads/writes them, so this header no longer needs to carry them.

    // step 1:
    struct API_IMPL HEADER
    {
        char name[16];          // must be "mbm"
        char typeApp[16];       // "Mesh 3d mbm", "User mbm", "Font", "Particle", "Sprite mbm", "Tile mbm"
        int32_t version;            // current CURRENT_VERSION_MBM_HEADER
        uint32_t magic;             // must be 0x010203ff.
        int32_t reserved;           // reserved (Must be 0)
        int32_t backBufferWidth;    // Indica o tamanho da largura do back buffer em que o objeto foi criado
        int32_t backBufferHeight;   // Indica o tamanho da altura do back buffer em que o objeto foi criado
        int32_t extraHeader;        // Quando indica quantidade de estrutura EXTRA_HEADER logo apos este frame
        HEADER() noexcept;
        HEADER(const char *nameApp, const int32_t versionNumber = 3)noexcept;
    };

    struct API_IMPL EXTRA_HEADER //added since version 6
    {
        char type;           // 0 None, 1 = Paths
        int32_t sizeExtraHeader; // Tamanho extra (em bytes) logo apos este frame
        EXTRA_HEADER() noexcept;
    };

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
    // Mesh v11 format (docs/mesh-v11-format.md) - section/TLV envelope. Layout locked, milestone 0
    // closed 2026-06-25 (docs/mesh-v11-plan.md). Section payload structs (frame/subset/material...)
    // are not defined yet - they land with the v11 writer/reader (milestones 3-4).
    // -----------------------------------------------------------------------------------------

    #define MBM_V11_MAGIC "MBM1"
    #define MBM_V11_FORMAT_VERSION 11

    enum SECTION_TYPE : uint16_t
    {
        SECTION_MATERIAL_TRANSFORM = 1,  // material + angle/pos + draw mode (replaces HEADER_MESH + INFO_DRAW_MODE)
        SECTION_ANIMATION          = 2,  // repeated: one per animation, in order, including its FX block
        SECTION_FRAME_STATIC       = 10, // repeated: one per frame, in order
        SECTION_FRAME_SKINNED      = 11, // reserved id only - no v11.0 writer ever emits this
        SECTION_DETAIL_PHYSICS     = 20,
        SECTION_DETAIL_FONT        = 21,
        SECTION_DETAIL_PARTICLE    = 22,
        SECTION_DETAIL_TILE        = 23,
        SECTION_EXTRA_PATHS        = 30, // replaces legacy EXTRA_HEADER type==1 path-registration hint
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

    // -----------------------------------------------------------------------------------------
    // Mesh v11 section payloads (docs/mesh-v11-format.md Sec. 6) - milestone 3 (core slice: material
    // transform, static frames, physics detail, extra paths). Animation/FX and the font/particle/tile
    // detail payloads are not covered here yet.
    // -----------------------------------------------------------------------------------------

    struct API_IMPL FRAME_HEADER_V11
    {
        uint32_t totalSubset;
        uint32_t vertexCount;
        uint8_t  indexWidth;   // 16 or 32 (only 16 emitted by the milestone-3 writer)
        uint8_t  hasNormal;    // bool
        uint8_t  hasUv;        // bool
        uint8_t  uvSource;     // 0 = OWN, 1 = SHARED_WITH_FRAME_0
        uint32_t indexCount;   // in indices, not bytes
        FRAME_HEADER_V11() noexcept;
    };

    enum TEXTURE_REF_STORAGE_V11 : uint8_t
    {
        TEXTURE_REF_STORAGE_PATH               = 0,
        TEXTURE_REF_STORAGE_EMBEDDED_COMPRESSED = 1, // reserved - no milestone-3 writer emits this
    };

    struct API_IMPL TEXTURE_REF_V11
    {
        uint8_t     storage; // TEXTURE_REF_STORAGE_V11
        std::string path;    // meaningful only when storage == TEXTURE_REF_STORAGE_PATH
        TEXTURE_REF_V11() noexcept;
    };

    struct API_IMPL SUBSET_DESC_V11
    {
        TEXTURE_REF_V11 primaryTexture; // implicit role TEXTURE_ROLE_DIFFUSE, always present
        int32_t  vertexCount;
        int32_t  vertexStart;
        int32_t  indexStart;
        int32_t  indexCount;
        uint8_t  alphaColor[4];
        uint16_t extraSlotCount; // caller must set before writing; followed on disk by
                                  // extraSlotCount * SUBSET_EXTRA_SLOT_V11
        SUBSET_DESC_V11() noexcept;
    };

    struct API_IMPL SUBSET_EXTRA_SLOT_V11
    {
        uint8_t         role; // an mbm::TEXTURE_ROLE value (NORMAL/SPECULAR/EMISSIVE/MASK only)
        TEXTURE_REF_V11 texture;
        SUBSET_EXTRA_SLOT_V11() noexcept;
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

}

#if (defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32))
    #pragma warning(default : 4201) //nonstandard extension used : nameless struct/union
#endif 

#endif
