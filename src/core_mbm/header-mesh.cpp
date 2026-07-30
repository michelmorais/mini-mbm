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

#include <header-mesh.h>
#include <util-interface.h>
#include <cstring>
#include <utility>
#include <draw-compatibility.h>

namespace mbm
{
class TEXTURE;
};

namespace util
{

    uint32_t __HEADER_BMP::getAsUintFromCharPointer(uint8_t *adress)
    {
        uint32_t x;
        x = adress[3];
        x <<= 8;
        x |= adress[2];
        x <<= 8;
        x |= adress[1];
        x <<= 8;
        x |= adress[0];
        return x;
    }

	INFO_DRAW_MODE::INFO_DRAW_MODE()noexcept:
            mode_draw(util::MODE_DRAW_TRIANGLES),
            mode_cull_face(util::CULL_BACK),//GL_FRONT, GL_BACK, GL_FRONT_AND_BACK
            mode_front_face_direction(util::CW)// GL_CW, GL_CCW
	{
	}

    DETAIL_MESH::DETAIL_MESH() noexcept
    {
        type          = 1;
        totalBounding = 0;
    }

    DETAIL_HEADER_FONT::DETAIL_HEADER_FONT() noexcept
    {
        sizeNameFonte   = 0;
        totalDetailFont = 0;
        spaceXCharacter = 0;
        spaceYCharacter = 0;
        heightLetter    = 0;
    }

    DETAIL_LETTER::DETAIL_LETTER() noexcept
    {
        letter       = 0;
        indexFrame   = 0;
        widthLetter  = 0;
        heightLetter = 0;
    }

    HEADER_MESH::HEADER_MESH() noexcept
    {
        totalAnimation         = 0;
        totalFrames            = 0;
        angleX                 = 0;
        angleY                 = 0;
        angleZ                 = 0;
        posX                   = 0;
        posY                   = 0;
        posZ                   = 0;
        deprecated_typePhysics = 0;
        hasNorText[0]          = HAS_NOR_NO;
        hasNorText[1]          = HAS_TEX_EACH_FRAME;
    }

    HEADER_ANIMATION::HEADER_ANIMATION() noexcept
    {
        memset(nameAnimation, 0, sizeof(nameAnimation));
        initialFrame     = 0;
        finalFrame       = 0;
        timeBetweenFrame = 0.0f;
        typeAnimation    = 0;
        hasShaderEffect  = 0;
        blendState       = 0;
    }

    HEADER_INFO_SHADER_STEP::HEADER_INFO_SHADER_STEP() noexcept
    {
        lenNameShader       = 0;
        lenTextureStage2    = 0;
        sizeArrayVarInBytes = 0;
        typeAnimation       = 0;
        blendOperation      = 1;
        timeAnimation       = 0.0f;
    }

    INFO_SHADER_DATA::INFO_SHADER_DATA(const int sizeArrayInBytes, const int sizeFileNameShader, const int sizeFileNameTextureStage2)
    {
        this->min                   = nullptr;
        this->max                   = nullptr;
        this->fileNameTextureStage2 = nullptr;
        this->fileNameShader        = nullptr;
        this->timeAnimation         = 0.0f;
        this->typeAnimation         = 0;
        if (sizeArrayInBytes)
            this->lenVars = sizeArrayInBytes / 4;
        else
            this->lenVars = 0;
        if (this->lenVars)
        {
            this->min = new float[this->lenVars * 4];
            this->max = new float[this->lenVars * 4];
        }
        if (this->lenVars)
            this->typeVars = new char[lenVars];
        else
            this->typeVars = nullptr;
        if (sizeFileNameShader)
            this->fileNameShader = new char[sizeFileNameShader];
        if (sizeFileNameTextureStage2)
            this->fileNameTextureStage2 = new char[sizeFileNameTextureStage2];
    }
    
    INFO_SHADER_DATA::~INFO_SHADER_DATA()noexcept
    {
        if (this->min)
            delete[] this->min;
        if (this->max)
            delete[] this->max;
        if (this->fileNameShader)
            delete[] this->fileNameShader;
        if (this->fileNameTextureStage2)
            delete[] this->fileNameTextureStage2;
        if (this->typeVars)
            delete[] this->typeVars;

        this->min                   = nullptr;
        this->max                   = nullptr;
        this->typeVars              = nullptr;
        this->fileNameTextureStage2 = nullptr;
    }

    INFO_FX::INFO_FX() noexcept
    {
        dataPS                         = nullptr;
        dataVS                         = nullptr;
        fileNameTextureAnimationEffect = nullptr;
        blendOperation                 = 0;
    }
    
    INFO_FX::~INFO_FX()noexcept
    {
        if (dataPS)
            delete dataPS;
        dataPS = nullptr;

        if (dataVS)
            delete dataVS;
        dataVS = nullptr;

        if (fileNameTextureAnimationEffect)
            delete[] fileNameTextureAnimationEffect;
        fileNameTextureAnimationEffect = nullptr;
    }

    void INFO_FX::setTextureAnimationEffectFileName(const char *fileName) noexcept
    {
        if (fileNameTextureAnimationEffect)
            delete[] fileNameTextureAnimationEffect;
        fileNameTextureAnimationEffect = nullptr;
        if (fileName && fileName[0])
        {
            const size_t len = strlen(fileName);
            fileNameTextureAnimationEffect = new char[len + 1];
            memcpy(fileNameTextureAnimationEffect, fileName, len + 1);
        }
    }

    const char *INFO_FX::getTextureAnimationEffectFileName() const noexcept
    {
        return fileNameTextureAnimationEffect;
    }

    bool INFO_FX::normalizeLegacyTextureAnimationEffectPaths() noexcept
    {
        const char *psTexture = dataPS ? dataPS->fileNameTextureStage2 : nullptr;
        const char *vsTexture = dataVS ? dataVS->fileNameTextureStage2 : nullptr;
        if (psTexture && psTexture[0] == 0)
            psTexture = nullptr;
        if (vsTexture && vsTexture[0] == 0)
            vsTexture = nullptr;
        if (psTexture && vsTexture && strcmp(psTexture, vsTexture) != 0)
            return false;
        const char *textureAnimationEffect = psTexture ? psTexture : vsTexture;
        this->setTextureAnimationEffectFileName(textureAnimationEffect);
        return true;
    }

    INFO_ANIMATION::INFO_HEADER_ANIM::INFO_HEADER_ANIM() noexcept
    {
        headerAnim = nullptr;
		effectShader = nullptr;
    }
        
    INFO_ANIMATION::INFO_HEADER_ANIM::~INFO_HEADER_ANIM()
    {
        if (headerAnim)
            delete headerAnim;

        if (effectShader)
            delete effectShader;
    }
        
    INFO_ANIMATION::INFO_ANIMATION() noexcept
    = default;
    
    INFO_ANIMATION::~INFO_ANIMATION()
    {
        this->release();
    }
    
    void INFO_ANIMATION::release()
    {
        for (auto & i : this->lsHeaderAnim)
        {
            INFO_HEADER_ANIM *dataInfo = i;
            if (dataInfo)
                delete dataInfo;
            i = nullptr;
        }
        this->lsHeaderAnim.clear();
    }

    HEADER_FRAME::HEADER_FRAME() noexcept
    {
        totalSubset      = 0;
        sizeIndexBuffer  = 0;
        sizeVertexBuffer = 0;
        stride           = 3;
        strncpy(typeBuffer, "?B",sizeof(typeBuffer)-1);
    }

    HEADER_DESC_SUBSET::HEADER_DESC_SUBSET() noexcept
    {
        memset(nameTexture, 0, sizeof(nameTexture));
        vertexCount = 0;
        indexStart  = 0;
        vertexStart = 0;
        indexCount  = 0;
        memset(alphaColor, 0, sizeof(alphaColor));
        materialTextureSlotCount = 0;
        reservedMaterialTextureSlots = 0;
    }

    MATERIAL_TEXTURE_SLOT_HEADER::MATERIAL_TEXTURE_SLOT_HEADER() noexcept
    {
        type = 0;
        reserved = 0;
        payloadSizeInBytes = 0;
        memset(nameTexture, 0, sizeof(nameTexture));
    }

    MATERIAL_TEXTURE_SLOT_DEBUG::MATERIAL_TEXTURE_SLOT_DEBUG() noexcept
    {
        type = 0;
    }

    SUBSET_DEBUG::SUBSET_DEBUG() noexcept
    {
        vertexStart = 0;
        indexStart  = 0;
        vertexCount = 0;
        indexCount  = 0;
    }

    BUFFER_MESH_DEBUG::BUFFER_MESH_DEBUG() noexcept
    {
        position    = nullptr;
        normal      = nullptr;
        uv          = nullptr;
        indexBuffer = nullptr;
    }
    
    BUFFER_MESH_DEBUG::~BUFFER_MESH_DEBUG()
    {
        release();
    }
    
    void BUFFER_MESH_DEBUG::release()
    {
        if (position)
            delete[] position;
        position = nullptr;

        if (normal)
            delete[] normal;
        normal = nullptr;

        if (uv)
            delete[] uv;
        uv = nullptr;

        if (indexBuffer)
            delete[] indexBuffer;
        indexBuffer = nullptr;

        for (auto pSubset : subset)
        {
            if (pSubset)
                delete pSubset;
        }
        subset.clear();
    }

    SUBSET::SUBSET() noexcept
    {
        vertexStart = 0;
        indexStart  = 0;
        vertexCount = 0;
        indexCount  = 0;
        texture     = nullptr;
    }

    STAGE_PARTICLE::STAGE_PARTICLE()noexcept :
          minOffsetPosition(-10, -10, -10)
        , maxOffsetPosition(10, 10, 10)
        , minDirection(-10, -10, -10)
        , maxDirection(10, 10, 10)
        , minColor(0.0f, 0.0f, 0.0f)
        , maxColor(1.0f, 1.0f, 1.0f)
        , minSpeed(3.0f)
        , maxSpeed(50.0f)
        , minTimeLife(0.01f)
        , maxTimeLife(7.0f)
        , minSizeParticle(5.0f)
        , maxSizeParticle(50.0f)
        , ariseTime(1.0f)
        , stageTime(5.0f)
        , totalParticle(0)
        , segmented(0)
        , sizeMin2Max(0)
        , revive(1)
        , _operator('*')
        , invert_red(0)
        , invert_green(0)
        , invert_blue(0)
        , invert_alpha(0)
    {
    }
    
    STAGE_PARTICLE::STAGE_PARTICLE(const STAGE_PARTICLE* other) noexcept:
        minOffsetPosition(other->minOffsetPosition)
        , maxOffsetPosition(other->maxOffsetPosition)
        , minDirection(other->minDirection)
        , maxDirection(other->maxDirection)
        , minColor(other->minColor)
        , maxColor(other->maxColor)
        , minSpeed(other->minSpeed)
        , maxSpeed(other->maxSpeed)
        , minTimeLife(other->minTimeLife)
        , maxTimeLife(other->maxTimeLife)
        , minSizeParticle(other->minSizeParticle)
        , maxSizeParticle(other->maxSizeParticle)
        , ariseTime(other->ariseTime)
        , stageTime(other->stageTime)
        , totalParticle(other->totalParticle)
        , segmented(other->segmented)
        , sizeMin2Max(other->sizeMin2Max)
        , revive(other->revive)
        , _operator(other->_operator)
        , invert_red(other->invert_red)
        , invert_green(other->invert_green)
        , invert_blue(other->invert_blue)
        , invert_alpha(other->invert_alpha)
    {
    }

	BTILE_INDEX_TILE::BTILE_INDEX_TILE()noexcept :
	index(0),x(0.0f), y(0.0f)
	{

	}

	BTILE_LAYER::BTILE_LAYER()noexcept :
		lsIndexTiles(nullptr)
	{
        offset[0] = 0;
        offset[1] = 0;
        offset[2] = 0; //z
    }

	BTILE_LAYER::~BTILE_LAYER()noexcept
	{
		if(lsIndexTiles)
			delete [] lsIndexTiles;
		lsIndexTiles = nullptr;
	}

    BTILE_BRICK_INFO::BTILE_BRICK_INFO() noexcept:
        index(0),original_index(0),rotation(0),flipped(0)
    {
    }

	BTILE_HEADER_MAP::BTILE_HEADER_MAP()noexcept:
	  count_width_tile(0)
	, count_height_tile(0)
	, size_width_tile(0)
	, size_height_tile(0)
    , layerCount(0)
    , countRawTiles(0)
    , objectCount(0)
    , propertyCount(0)
    , typeMap(BTILE_TYPE_ORIENTATION_ORTHOGONAL)
	, background(0)
	, background_texture("")
	{
        renderDirection[0] = 0;
        renderDirection[1] = 0;
    }

	BTILE_PROPERTY::BTILE_PROPERTY()noexcept:
		type(BTILE_PROPERTY_TYPE_UNKNOWN)
	{}

	BTILE_PROPERTY::BTILE_PROPERTY(const BTILE_PROPERTY & other)noexcept:
		owner(other.owner),
		name(other.name),
		value(other.value),
		type(other.type)
	{}

	BTILE_PROPERTY::BTILE_PROPERTY(const BTILE_PROPERTY_TYPE Type)noexcept:
		type(Type)
	{}

	BTILE_PROPERTY_HEADER::BTILE_PROPERTY_HEADER()noexcept:
		type(0),
		nameLength(0),
		valueLength(0),
		ownerLength(0)
	{}

	BTILE_PROPERTY_HEADER::BTILE_PROPERTY_HEADER(const BTILE_PROPERTY * property)noexcept:
		type(property->type),
		nameLength(static_cast<uint16_t>(property->name.size())),
		valueLength(static_cast<uint16_t>(property->value.size())),
		ownerLength(static_cast<uint16_t>(property->owner.size()))
	{}

	BTILE_OBJ::BTILE_OBJ()noexcept :type(BTILE_OBJ_TYPE_UNKNOWN)
	{}

	BTILE_OBJ::BTILE_OBJ(BTILE_OBJ_TYPE Type)noexcept :type(Type)
	{}

	BTILE_OBJ::BTILE_OBJ(BTILE_OBJ_TYPE Type,std::string  Name)noexcept :type(Type),name(std::move(Name))
	{}

	BTILE_OBJ::BTILE_OBJ(const BTILE_OBJ & other)noexcept :type(other.type),name(other.name)
	{
		for (auto p : other.lsPoints)
		{
			auto* point = new mbm::VEC2(p->x,p->y);
			lsPoints.push_back(point);
		}
	}

	BTILE_OBJ::~BTILE_OBJ() noexcept
	{
		for (auto point : lsPoints)
		{
			delete point;
		}
		lsPoints.clear();
	}

	BTILE_DETAIL_HEADER::BTILE_DETAIL_HEADER()noexcept:
		totalObj(0),
		totalProperties(0)
	{}

	BTILE_OBJ_HEADER::BTILE_OBJ_HEADER()noexcept:
		sizeName(0),
		type(0),
		sizePoints(0)
	{}
	BTILE_OBJ_HEADER::BTILE_OBJ_HEADER(const BTILE_OBJ * obj)noexcept:
		sizeName(static_cast<uint16_t>(obj->name.size())),
		type(static_cast<uint16_t>(obj->type)),
		sizePoints(static_cast<uint16_t>(obj->lsPoints.size()))
	{}
	
	BTILE_INFO::BTILE_INFO()noexcept:
		layers(nullptr),
        infoBrickEditor(nullptr)
	{}

	BTILE_INFO::~BTILE_INFO()noexcept
	{
		if(layers)
			delete [] layers;
		layers = nullptr;
        if(infoBrickEditor)
            delete [] infoBrickEditor;
        infoBrickEditor = nullptr;
		for (auto obj : lsObj)
		{
			delete obj;
		}
		lsObj.clear();

		for (auto property : lsProperty)
		{
			delete property;
		}
		lsProperty.clear();
	}

	BTILE_INFO* BTILE_INFO::clone()const
	{
		auto* that = new BTILE_INFO();
		that->map = map;
		that->layers = new BTILE_LAYER[map.layerCount];
		uint32_t tileCount  = map.count_height_tile * map.count_width_tile;
		for (uint32_t i = 0; i < map.layerCount; ++i)
		{
			const BTILE_LAYER* this_layer = &layers[i];
			BTILE_LAYER* that_layer = &that->layers[i];
			that_layer->lsIndexTiles = new util::BTILE_INDEX_TILE[tileCount];
			const util::BTILE_INDEX_TILE * lsIndexTiles = this_layer->lsIndexTiles;
			for (uint32_t  j = 0; j < tileCount; ++j)
			{
				util::BTILE_INDEX_TILE *	that_Index_Tile = &that_layer->lsIndexTiles[j];
				const util::BTILE_INDEX_TILE *	this_Index_Tile = &lsIndexTiles[j];
				that_Index_Tile->index = this_Index_Tile->index;
				that_Index_Tile->x     = this_Index_Tile->x;
				that_Index_Tile->y     = this_Index_Tile->y;
			}
		}

        that->infoBrickEditor = new BTILE_BRICK_INFO[map.countRawTiles];
        for(uint32_t i = 0; i < map.countRawTiles; ++i)
        {
            that->infoBrickEditor[i] = infoBrickEditor[i];
        }
		for (auto that_obj : lsObj)
		{
			auto* obj = new util::BTILE_OBJ(*that_obj);
			that->lsObj.push_back(obj);
		}

		for (auto that_property : lsProperty)
		{
			auto* property = new util::BTILE_PROPERTY(*that_property);
			that->lsProperty.push_back(property);
		}
		return that;
	}

        DYNAMIC_SHAPE::DYNAMIC_SHAPE(float * vertex,float * normal, float *uv, const unsigned int sv, const unsigned int sn, const unsigned int suv)noexcept:
          dynamicVertex(vertex),dynamicNormal(normal),dynamicUV(uv),size_vertex(sv),size_normal(sn),size_uv(suv)
        {}

    FILE_HEADER_V11::FILE_HEADER_V11() noexcept
    {
        std::memcpy(magic, MBM_V11_MAGIC, sizeof(magic));
        formatVersion    = MBM_V11_FORMAT_VERSION;
        typeMesh         = 0;
        reserved0        = 0;
        backBufferWidth  = 0;
        backBufferHeight = 0;
        sectionCount     = 0;
    }

    SECTION_HEADER_V11::SECTION_HEADER_V11() noexcept
    {
        type               = 0;
        sectionVersion     = 0;
        compression        = SECTION_COMPRESSION_NONE;
        reserved1[0] = reserved1[1] = reserved1[2] = 0;
        uncompressedLength = 0;
        compressedLength   = 0;
        crc32Value         = 0;
    }

    FRAME_HEADER_V11::FRAME_HEADER_V11() noexcept
    {
        totalSubset = 0;
        vertexCount = 0;
        indexWidth  = 16;
        hasNormal   = 0;
        hasUv       = 0;
        uvSource    = 0;
        indexCount  = 0;
    }

    TEXTURE_REF_V11::TEXTURE_REF_V11() noexcept
    {
        storage = TEXTURE_REF_STORAGE_PATH;
    }

    SUBSET_DESC_V11::SUBSET_DESC_V11() noexcept
    {
        vertexCount = 0;
        vertexStart = 0;
        indexStart  = 0;
        indexCount  = 0;
        alphaColor[0] = alphaColor[1] = alphaColor[2] = alphaColor[3] = 0;
        extraSlotCount = 0;
    }

    SUBSET_EXTRA_SLOT_V11::SUBSET_EXTRA_SLOT_V11() noexcept
    {
        role = 0;
    }

    MATERIAL_TRANSFORM_V11::MATERIAL_TRANSFORM_V11() noexcept
    {
        angleX = angleY = angleZ = 0.0f;
        posX = posY = posZ = 0.0f;
        mode_draw = 0;
        mode_cull_face = 0;
        mode_front_face_direction = 0;
    }

    SHADER_VAR_V11::SHADER_VAR_V11() noexcept
    {
        typeVar = 0;
        min[0] = min[1] = min[2] = min[3] = 0.0f;
        max[0] = max[1] = max[2] = max[3] = 0.0f;
    }

    SHADER_STEP_V11::SHADER_STEP_V11() noexcept
    {
        timeAnimation = 0.0f;
        typeAnimation = 0;
        varCount      = 0;
    }

    FX_HEADER_V11::FX_HEADER_V11() noexcept
    {
        blendOperation = 1;
        hasFxTexture   = 0;
        hasPS          = 0;
        hasVS          = 0;
    }

    ANIMATION_HEADER_V11::ANIMATION_HEADER_V11() noexcept
    {
        initialFrame     = 0;
        finalFrame       = 0;
        timeBetweenFrame = 0.0f;
        typeAnimation    = 0;
        blendState       = 0;
        hasFx            = 0;
    }

    ARTICULATED_PARTS_HEADER_V11::ARTICULATED_PARTS_HEADER_V11() noexcept
    {
        partCount = 0;
    }

    ARTICULATED_PART_V11::ARTICULATED_PART_V11() noexcept
    {
        partId = 0;
        frameIndex = 0;
        subsetIndex = 0;
        parentPartId = 0;
        pivotX = pivotY = pivotZ = 0.0f;
        pivotQX = pivotQY = pivotQZ = 0.0f;
        pivotQW = 1.0f;
    }

    ARTICULATED_ANIMATION_HEADER_V11::ARTICULATED_ANIMATION_HEADER_V11() noexcept
    {
        clipCount = 0;
    }

    ARTICULATED_CLIP_V11::ARTICULATED_CLIP_V11() noexcept
    {
        duration = 0.0f;
        speed = 1.0f;
        defaultPriority = 0;
        loop = 1;
    }

    ARTICULATED_TRACK_V11::ARTICULATED_TRACK_V11() noexcept
    {
        partId = 0;
        channelMask = 0;
        keyCount = 0;
    }

    ARTICULATED_KEY_V11::ARTICULATED_KEY_V11() noexcept
    {
        time = 0.0f;
        positionX = positionY = positionZ = 0.0f;
        rotationX = rotationY = rotationZ = 0.0f;
        rotationW = 1.0f;
        rotationEulerX = rotationEulerY = rotationEulerZ = 0.0f;
        hasRotationEuler = 0;
        scaleX = scaleY = scaleZ = 1.0f;
    }

    FONT_DETAIL_HEADER_V11::FONT_DETAIL_HEADER_V11() noexcept
    {
        spaceXCharacter = 0;
        spaceYCharacter = 0;
        heightLetter    = 0;
        letterCount     = 0;
    }

    TILE_HEADER_MAP_V11::TILE_HEADER_MAP_V11() noexcept
    {
        count_width_tile           = 0;
        count_height_tile          = 0;
        size_width_tile            = 0;
        size_height_tile           = 0;
        layerCount                 = 0;
        countRawTiles              = 0;
        objectCount                = 0;
        propertyCount              = 0;
        typeMap                    = 0;
        background                 = 0;
        renderDirectionLeftToRight = 0;
        renderDirectionTopToDown   = 0;
    }

    TILE_LAYER_HEADER_V11::TILE_LAYER_HEADER_V11() noexcept
    {
        offsetX = 0;
        offsetY = 0;
        offsetZ = 0;
    }

    TILE_OBJ_HEADER_V11::TILE_OBJ_HEADER_V11() noexcept
    {
        type       = 0;
        pointCount = 0;
    }

    TILE_PROPERTY_V11::TILE_PROPERTY_V11() noexcept
    {
        type = 0;
    }

    SKELETON_HEADER_V11::SKELETON_HEADER_V11() noexcept
    {
        jointCount = 0;
    }

    SKELETON_BONE_V11::SKELETON_BONE_V11() noexcept
    {
        x = y = z = 0.0f;
        radius     = 0.0f;
        rotX = rotY = rotZ = 0.0f;
        scaleX = scaleY = scaleZ = 1.0f;
        length = 0.0f;
    }

    VERTEX_SKIN_WEIGHTS_HEADER_V11::VERTEX_SKIN_WEIGHTS_HEADER_V11() noexcept
    {
        paletteCount = 0;
        vertexCount  = 0;
    }

    VERTEX_BONE_WEIGHT_V11::VERTEX_BONE_WEIGHT_V11() noexcept
    {
        paletteIndex[0] = paletteIndex[1] = paletteIndex[2] = paletteIndex[3] = 0xFF;
        weight[0] = weight[1] = weight[2] = weight[3] = 0.0f;
    }

}
