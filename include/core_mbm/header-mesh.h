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

#if defined _WIN32
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
        //'CI' - OS/2 Color Icon (�cone colorido)
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
        // 1 - Bitmap monocrom�tico (preto e COR_BRANCO)
        // 4 - Bitmap De 16 cores
        // 8 - Bitmap De 256 cores
        // 16 - Bitmap De 16bits (high color)
        // 24 - Bitmap De 24bits (true color)
        // 32 - Bitmap De 32bits (true color)
        uint8_t compressed[4];
        // 0 - nenhuma (Tamb�m identificada Por BI_RGB)
        // 1 - RLE 8 bits/Pixel (Tamb�m identificada Por BI_RLE4)
        // 2 - RLE 4 bits/Pixel (Tamb�m identificada Por BI_RLE8)
        // 3 - Bitfields (Tamb�m identificada Por BI_BITFIELDS)
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

    struct API_IMPL MATERIAL_GLES
    {
        mbm::COLOR Diffuse;
        mbm::COLOR Ambient;
        mbm::COLOR Specular;
        mbm::COLOR Emissive;
        float      Power;
        constexpr MATERIAL_GLES() noexcept:
            Diffuse(1.0f,1.0f,1.0f,1.0f) ,
            Ambient(1.0f,1.0f,1.0f,1.0f) ,
            Specular(1.0f,1.0f,1.0f,1.0f) ,
            Emissive(0.0f,0.0f,0.0f,0.0f) ,
            Power(1.0f)
        {}
    };

    #define INITIAL_VERSION_MBM_HEADER     1
    #define SPRITE_INFO_VERSION_MBM_HEADER 2
    #define DETAIL_MESH_VERSION_MBM_HEADER 3
    #define SPACE_SHIP_VERSION_MBM_HEADER  4
    #define CURRENT_VERSION_MBM_HEADER     5

    #define MODE_DRAW_VERSION_MBM_HEADER   5

    // step 1:
    struct API_IMPL HEADER
    {
        char name[16];          // must be "mbm"
        char typeApp[16];       // "Mesh 3d mbm", "User mbm", "Font", "Particle", "Sprite mbm", "Tile mbm"
        int version;            // current CURRENT_VERSION_MBM_HEADER
        int magic;              // must be 0x010203ff.
        int reserved;           // reserved (Must be 0)
        int backBufferWidth;    // Indica o tamanho da largura do back buffer em que o objeto foi criado
        int backBufferHeight;   // Indica o tamanho da altura do back buffer em que o objeto foi criado
        int extraHeader;        // Quando indica o tamanho do Header extra (em bytes) logo apos este frame (utilizado em fontes e/ou/futuros).
        HEADER() noexcept;
        HEADER(const char *nameApp, const int versionNumber = 3)noexcept;
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
        int type; // 1 box, 2 sphere, 3 complex-cube, 4 triangle, 5 header-font,6 particle, 7 Tile, (deprecated 100 script generic, 101 shader)
        int totalBounding;
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
        MATERIAL_GLES material;              // Material aplicado nesta subset
        int          totalAnimation;         // Total de anima��o na mesh
        int          totalFrames;            // Total de frames para o arquivo. cada frame � dividido em uma ou mais subsets.
        int          deprecated_typePhysics; // not used anymore, 'deprecated' (just keep for compatibility,old typePhysics)
        int16_t    hasNorText[2];          // hasNorText[0]: Indica se ha normal (0: sera calculado as normais,1:tera normal em cada frame)
        // hasNorText[1]:textura (0: n�o tera textura, 1: tera textura em cada frame, 2: tera textura somente no frame 1, os outros ser�o copiados).
        float angleX, angleY, angleZ; 
        float posX, posY, posZ;       
    
        HEADER_MESH()noexcept;
    };


    struct API_IMPL HEADER_ANIMATION
    {
        char  nameAnimation[32];        // 32 bytes para o nome da anima��o (31 + null)
        int   initialFrame;             // Frame inicial para esta anima��o
        int   finalFrame;               // Frame final para esta anima��o
        float timeBetweenFrame;         // Tempo entre frames da anima��o
        int   typeAnimation;            // Tipo da anima��o
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
        int       blendOperation;       // Tipo de operacao blend nos steps
        float     timeAnimation;        // Tempo da anima��o
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

    struct API_IMPL INFO_FX
    {
        INFO_SHADER_DATA *dataPS;          // Data do pixel Shader
        INFO_SHADER_DATA *dataVS;          // Data do vertex Shader
        int                blendOperation;  // Tipo de operacao blend nos steps
        INFO_FX()noexcept;
        ~INFO_FX()noexcept;
    };

    struct INFO_ANIMATION
    {
        struct INFO_HEADER_ANIM
        {
            util::HEADER_ANIMATION *    headerAnim;         
            INFO_FX *			effetcShader; 
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
        int totalSubset;        // Total de subset para este frame
        int sizeIndexBuffer;    // Tamanho do Index buffer(se houver) deste frame.
        int sizeVertexBuffer;   // Tamanho do vertex buffer deste frame.
        int stride;             // 3 para x,y z ou 2 para x e y.
        char typeBuffer[4];     // Tipo do buffer VB par vertex buffer e IB para indexBuffer
        HEADER_FRAME()noexcept;
    };


    struct API_IMPL HEADER_DESC_SUBSET 
    {
        char nameTexture[64]; // 64 bytes para o nome da textura (63 + null) desta subset
        int  vertexCount;     // Total de vertex no subset
        int  vertexStart;     // Inicio do vertex
        int  indexStart;      // Inicio do �ndice
        int  indexCount;      // Total de indices
        union {
            struct
            {
                uint8_t alphaColor[4]; // onde o primeiro byte indica se existe color alpha e os demais s�o as cores.
                                             // (mantido por compatibilidade mas nao usamos mais colorKeying)
            };
            struct
            {
                uint8_t hasAlphaColor; // Flag indicando se existe alpha
                uint8_t r, g, b;       // Cor  color alpha
            };
        };
    
        HEADER_DESC_SUBSET()noexcept;
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
                uint8_t alphaColor[4]; // onde o primeiro byte indica se existe color alpha e os demais s�o as cores
                                             // (mantido por compatibilidade mas nao usamos mais colorKeying)
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
        int           vertexStart; // Inicial do vertex
        int           indexStart;  // Inicio do �ndice
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
        char  segmented;
        char  sizeMin2Max;
        char  revive;
        char  _operator;
        char  invert_red;
        char  invert_green;
        char  invert_blue;
        char  invert_alpha;

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
        unsigned int    background;
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

}

#if defined _WIN32
    #pragma warning(default : 4201) //nonstandard extension used : nameless struct/union
#endif 

#endif
