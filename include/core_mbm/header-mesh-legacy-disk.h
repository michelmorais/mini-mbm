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

#ifndef HEADER_MESH_LEGACY_DISK_H
#define HEADER_MESH_LEGACY_DISK_H

// Reference-only on-disk layouts for the legacy MESH_MBM formats (v1-v10). Nothing in core_mbm
// reads/writes these directly today (mesh-v8-io.cpp populates the *runtime* structs in
// header-mesh.h, e.g. util::HEADER_MESH, field-by-field - it never blits these). They exist purely
// to document the exact legacy byte layout for whoever builds the offline `mesh_deprecated`
// importer (docs/mesh-v11-plan.md, milestone 5) and were relocated out of header-mesh.h so that
// header (and anything that includes it, like mesh-manager.h) stops dragging in dead legacy
// reference structs just to use the small set of runtime types it actually needs
// (docs/mesh-v11-plan.md, milestone 2).

#include <stdint.h>
#include "header-mesh.h"

namespace util
{
    struct API_IMPL HEADER_DISK_V8
    {
        char name[16];
        char typeApp[16];
        int32_t version;
        uint32_t magic;
        int32_t reserved;
        int32_t backBufferWidth;
        int32_t backBufferHeight;
        int32_t extraHeader;
    };

    struct API_IMPL HEADER_MESH_DISK_V8
    {
        MATERIAL material;
        int32_t totalAnimation;
        int32_t totalFrames;
        int32_t deprecated_typePhysics;
        int16_t hasNorText[2];
        float angleX, angleY, angleZ;
        float posX, posY, posZ;
    };

    struct API_IMPL HEADER_ANIMATION_DISK_V8
    {
        char nameAnimation[32];
        int32_t initialFrame;
        int32_t finalFrame;
        float timeBetweenFrame;
        int32_t typeAnimation;
        uint16_t hasShaderEffect;
        uint16_t blendState;
    };

    struct API_IMPL HEADER_INFO_SHADER_STEP_DISK_V8
    {
        int16_t lenNameShader;
        int16_t lenTextureStage2;
        int16_t sizeArrayVarInBytes;
        int16_t typeAnimation;
        int32_t blendOperation;
        float timeAnimation;
    };

    struct API_IMPL HEADER_INFO_SHADER_EFFECT_DISK_V10
    {
        int16_t lenTextureAnimationEffect;
        int16_t reserved0;
        int32_t reserved1;
    };

    struct API_IMPL HEADER_FRAME_DISK_V8
    {
        int32_t totalSubset;
        int32_t sizeIndexBuffer;
        int32_t sizeVertexBuffer;
        int32_t stride;
        char typeBuffer[4];
    };

    struct API_IMPL HEADER_DESC_SUBSET_DISK_V8
    {
        char nameTexture[64];
        int32_t vertexCount;
        int32_t vertexStart;
        int32_t indexStart;
        int32_t indexCount;
        uint8_t alphaColor[4];
    };

    enum MATERIAL_TEXTURE_SLOT_TYPE : uint16_t
    {
        MATERIAL_TEXTURE_SLOT_NORMAL   = 1,
        MATERIAL_TEXTURE_SLOT_SPECULAR = 2,
        MATERIAL_TEXTURE_SLOT_EMISSIVE = 3,
        MATERIAL_TEXTURE_SLOT_MASK     = 4,
    };

    struct API_IMPL HEADER_DESC_SUBSET_DISK_V9
    {
        char nameTexture[64];
        int32_t vertexCount;
        int32_t vertexStart;
        int32_t indexStart;
        int32_t indexCount;
        uint8_t alphaColor[4];
        uint16_t materialTextureSlotCount;
        uint16_t reservedMaterialTextureSlots;
    };

    struct API_IMPL MATERIAL_TEXTURE_SLOT_HEADER_DISK_V9
    {
        uint16_t type;
        uint16_t reserved;
        uint32_t payloadSizeInBytes;
        char nameTexture[64];
    };

    struct API_IMPL HEADER_IMG_DISK_V8
    {
        uint32_t width;
        uint32_t height;
        uint16_t depth;
        uint16_t channel;
        uint32_t lenght;
        uint8_t alphaColor[4];
    };

    struct API_IMPL EXTRA_HEADER_DISK_V8
    {
        char type;
        int32_t sizeExtraHeader;
    };

    struct API_IMPL INFO_DRAW_MODE_DISK_V8
    {
        uint32_t mode_draw;
        uint32_t mode_cull_face;
        uint32_t mode_front_face_direction;
    };

    struct API_IMPL DETAIL_MESH_DISK_V8
    {
        int32_t type;
        int32_t totalBounding;
    };

    struct API_IMPL DETAIL_HEADER_FONT_DISK_V8
    {
        uint16_t sizeNameFonte;
        uint16_t totalDetailFont;
        int16_t spaceXCharacter;
        int16_t spaceYCharacter;
        uint16_t heightLetter;
    };

    struct API_IMPL DETAIL_LETTER_DISK_V8
    {
        uint8_t letter;
        uint8_t indexFrame;
        uint16_t widthLetter;
        uint16_t heightLetter;
    };

    struct API_IMPL BTILE_BRICK_INFO_DISK_V8
    {
        uint16_t index;
        uint16_t original_index;
        uint16_t rotation;
        uint16_t flipped;
    };

    struct API_IMPL BTILE_HEADER_MAP_DISK_V8
    {
        uint32_t count_width_tile;
        uint32_t count_height_tile;
        uint32_t size_width_tile;
        uint32_t size_height_tile;
        uint32_t layerCount;
        uint32_t countRawTiles;
        uint32_t objectCount;
        uint32_t propertyCount;
        uint32_t typeMap;
        uint32_t background;
        char background_texture[62];
        char renderDirection[2];
    };

    struct API_IMPL BTILE_INDEX_TILE_DISK_V8
    {
        uint32_t index;
        float x;
        float y;
    };

    struct API_IMPL BTILE_DETAIL_HEADER_DISK_V8
    {
        uint32_t totalObj;
        uint32_t totalProperties;
    };

    struct API_IMPL BTILE_OBJ_HEADER_DISK_V8
    {
        uint16_t sizeName;
        uint16_t type;
        uint16_t sizePoints;
    };

    struct API_IMPL BTILE_PROPERTY_HEADER_DISK_V8
    {
        uint16_t type;
        uint16_t nameLength;
        uint16_t valueLength;
        uint16_t ownerLength;
    };

    struct API_IMPL STAGE_PARTICLE_DISK_V8
    {
        float minOffsetPosition[3];
        float maxOffsetPosition[3];
        float minDirection[3];
        float maxDirection[3];
        float minColor[3];
        float maxColor[3];
        float minSpeed;
        float maxSpeed;
        float minTimeLife;
        float maxTimeLife;
        float minSizeParticle;
        float maxSizeParticle;
        float ariseTime;
        float stageTime;
        uint32_t totalParticle;
        uint8_t segmented;
        uint8_t sizeMin2Max;
        uint8_t revive;
        uint8_t _operator;
        uint8_t invert_red;
        uint8_t invert_green;
        uint8_t invert_blue;
        uint8_t invert_alpha;
    };
}

#endif
