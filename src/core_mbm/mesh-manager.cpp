/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <mesh-manager.h>
#include <shader-var-cfg.h>
#include <texture-manager.h>
#include <renderizable.h>
#include <shader.h>
#include <util-interface.h>
#include <shapes.h>
#include <deprecated.h>
#include <cr-static-local.h>
#include <miniz-wrap/miniz-wrap.h>


#include <gles-debug.h>
#include <GLES2/gl2ext.h>

#include <cfloat>
#include <string>
#include <algorithm> // std::sort
#if defined USE_EDITOR_FEATURES
#include <map>
#endif

const bool is_mode_draw_valid(const uint32_t mode_draw)noexcept
{
    switch(mode_draw)
    {
        case GL_POINTS		   : return true;
        case GL_LINES		   : return true;
        case GL_LINE_LOOP	   : return true;
        case GL_LINE_STRIP	   : return true;
        case GL_TRIANGLES 	   : return true;
        case GL_TRIANGLE_STRIP : return true;
        case GL_TRIANGLE_FAN   : return true;
        default                : return false;
    }
}

const bool is_mode_cull_face_valid(const uint32_t mode_cull_face)noexcept
{
    switch(mode_cull_face)
    {
        case GL_FRONT		   : return true;
        case GL_BACK		   : return true;
        case GL_FRONT_AND_BACK : return true;
        default                : return false;
    }
}

const bool is_mode_front_face_direction_valid(const uint32_t mode_front_face_direction)noexcept
{
    switch(mode_front_face_direction)
    {
        case GL_CW   		   : return true;
        case GL_CCW  		   : return true;
        default                : return false;
    }
}

const bool is_any_mode_valid(const util::INFO_DRAW_MODE & info_mode,std::string & which_mode_is_invalid)noexcept
{
    if(is_mode_draw_valid(info_mode.mode_draw) == false)
    {
        which_mode_is_invalid = "mode draw:";
        which_mode_is_invalid += std::to_string(info_mode.mode_draw);
        return false;
    }
    if(is_mode_cull_face_valid(info_mode.mode_cull_face) == false)
    {
        which_mode_is_invalid = "mode cull face:";
        which_mode_is_invalid += std::to_string(info_mode.mode_cull_face);
        return false;
    }
    if(is_mode_front_face_direction_valid(info_mode.mode_front_face_direction) == false)
    {
        which_mode_is_invalid = "mode front face direction:";
        which_mode_is_invalid += std::to_string(info_mode.mode_front_face_direction);
        return false;
    }
    return true;
}

namespace mbm
{
    constexpr BUFFER_MESH::BUFFER_MESH() noexcept : pBufferGL(nullptr), subset(nullptr), totalSubset(0)
    {
    }
    
    BUFFER_MESH::~BUFFER_MESH()
    {
        release();
    }
    
    void BUFFER_MESH::release()
    {
        if (pBufferGL)
            delete pBufferGL;
        pBufferGL = nullptr;

        if (subset)
            delete[] subset;
        subset      = nullptr;
        totalSubset = 0;
    }

#if defined USE_EDITOR_FEATURES

    MESH_MBM_DEBUG::MESH_MBM_DEBUG() noexcept
    {
        positionOffset      = VEC3(0, 0, 0);
        angleDefault        = VEC3(0, 0, 0);
        coordTexFrame_0     = nullptr;
        sizeCoordTexFrame_0 = 0;
        typeMe              = util::TYPE_MESH_UNKNOWN;
        util::MATERIAL_GLES m;
        this->headerMesh.material      = m;
        this->headerMesh.hasNorText[0] = 0;
        this->headerMesh.hasNorText[1] = 1;
        zoomEditorSprite.x            = 1.0f;
        zoomEditorSprite.y            = 1.0f;
        extraInfo                     = nullptr;
    }

    MESH_MBM_DEBUG::~MESH_MBM_DEBUG()
    {
        this->release();
    }
    
    uint32_t MESH_MBM_DEBUG::addBuffer(const int stride )
    {
        if ((stride == 3 || stride == 2))
        {
            auto pBuffer = new util::BUFFER_MESH_DEBUG();
            pBuffer->headerFrame.stride      = stride;
            this->buffer.push_back(pBuffer);
            return static_cast<uint32_t>(this->buffer.size());
        }
        return 0;
    }
    
    uint32_t MESH_MBM_DEBUG::addSubset(uint32_t indexFrame)
    {
        if (indexFrame < static_cast<uint32_t>(this->buffer.size()))
        {
            this->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(indexFrame)]->subset.push_back(new util::SUBSET_DEBUG());
            return static_cast<uint32_t>(this->buffer[indexFrame]->subset.size());
        }
        return 0;
    }
    
    bool MESH_MBM_DEBUG::getInfo(util::HEADER_MESH &headerMeshMbmOut, util::TYPE_MESH &typeOut,
                              INFO_BOUND_FONT **datailFontOut, std::vector<util::STAGE_PARTICLE> & lsStageParticle)
    {
        if (this->buffer.size())
        {
            headerMeshMbmOut = this->headerMesh;
            typeOut          = this->typeMe;

            if(this->typeMe == util::TYPE_MESH_FONT)
            {
                *datailFontOut   = static_cast<INFO_BOUND_FONT *>(this->extraInfo);
            }
            lsStageParticle.clear();
            if(this->typeMe == util::TYPE_MESH_PARTICLE)
            {
                auto* lsParticleInfo = static_cast<std::vector<util::STAGE_PARTICLE*>*>(this->extraInfo);
                if(lsParticleInfo)
                {
                    for (auto & i : *lsParticleInfo)
                    {
                        lsStageParticle.push_back(*i);
                    }		
                }
            }
            return true;
        }
        return false;
    }

    std::string MESH_MBM_DEBUG::getExtension(const char* fileName)
    {
        std::string ret;
        if(fileName)
        {
            const std::string file(fileName);
            const std::string::size_type p = file.find_last_of('.');
            if(p != std::string::npos && (p+1) < file.size())
            {
                std::string newExt(file.substr(p + 1));
                const std::string::size_type l = newExt.size();
                for (std::string::size_type i=0; i < l; ++i )
                {
                    newExt[i] = static_cast<char>(toupper(newExt[i]));
                }
                ret = std::move(newExt);
            }
        }
        return ret;
    }

    const char* MESH_MBM_DEBUG::getValidExtension(const char* fileName,bool &isImage,bool &isMesh,bool &isUnknown)
    {
        if(fileName)
        {
            const std::string file(fileName);
            const std::string::size_type p = file.find_last_of('.');
            if(p != std::string::npos && (p+1) < file.size())
            {
                const char* ext = &fileName[p+1];
                std::string                  newExt(file.substr(p + 1));
                const std::string::size_type l = newExt.size();
                for (std::string::size_type i=0; i < l; ++i )
                {
                    newExt[i] = static_cast<char>(toupper(newExt[i]));
                }
                isImage = true;
                if(newExt.compare("PNG") == 0)
                    return ext;
                if(newExt.compare("BMP") == 0)
                    return ext;
                if(newExt.compare("TIF") == 0)
                    return ext;
                if(newExt.compare("JPEG") == 0)
                    return ext;
                if(newExt.compare("JPG") == 0)
                    return ext;
                if(newExt.compare("GIF") == 0)
                    return ext;
                if(newExt.compare("TIFF") == 0)
                    return ext;
                if(newExt.compare("UBERIMG") == 0)
                    return ext;
                isImage = false;
                isMesh = true;
                if(newExt.compare("SPT") == 0)
                    return ext;
                if(newExt.compare("MSH") == 0)
                    return ext;
                if(newExt.compare("FNT") == 0)
                    return ext;
                if(newExt.compare("PTL") == 0)
                    return ext;
                if(newExt.compare("TILE") == 0)
                    return ext;
#if defined USE_DEPRECATED_2_MINOR
                if(newExt.compare("MBM") == 0)
                    return ext;
#endif
                isMesh = false;
                isUnknown = true;
                return ext;
            }
        }
        return nullptr;
    }
    
    bool MESH_MBM_DEBUG::getInfo(const char *fileNamePath, util::HEADER_MESH &headerMeshMbmOut,util::INFO_DRAW_MODE & info_mode,
                              util::TYPE_MESH &typeOut, INFO_BOUND_FONT &datailFontOut, 
                              std::vector<util::STAGE_PARTICLE> & lsStageParticle)
    {
        bool isImage    = false;
        bool isMesh     = false;
        bool isUnknown  = false;
        const char* ext = getValidExtension(fileNamePath,isImage,isMesh,isUnknown);
        if(isUnknown)
        {
            typeOut = util::TYPE_MESH_UNKNOWN;
            return false;
        }
        if(ext && (isImage))
        {
            typeOut = util::TYPE_MESH_TEXTURE;
            datailFontOut.fontName = ext;
            const std::string::size_type l = datailFontOut.fontName.size();
            for (std::string::size_type i=0; i < l; ++i )
            {
                datailFontOut.fontName[i] = static_cast<char>(toupper(datailFontOut.fontName[i]));
            }
            util::HEADER_MESH tmp;
            headerMeshMbmOut = tmp;
            return true;    
        }
        if(!isMesh)
            return false;
        util::HEADER headerMbmOut;
        FILE *           fp = util::openFile(fileNamePath, "rb");
        if (!fp)
            return log_util::onFailed(fp,__FILE__, __LINE__, "Failed to open file [%s]", fileNamePath ? fileNamePath : "nullptr");
#if defined USE_DEPRECATED_2_MINOR
        deprecated_mbm::INFO_SPRITE deprectedInfoSprite; // version <=SPRITE_INFO_VERSION_MBM_HEADER
#endif
        fclose(fp);
        fp = nullptr;
        MINIZ minz;
        {
            char errorDesc[255]="";
            if (!minz.decompressFile(fileNamePath, util::getDecompressModelFileName(),errorDesc,sizeof(errorDesc)-1))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to uncompress file [%s]\n%s", fileNamePath,errorDesc);
        }
        fp = util::openFile(util::getDecompressModelFileName(), "rb");
        if (fp == nullptr)
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to open file [%s]", fileNamePath);
        // step 1: Verificação do header  MBM principal
        // -------------------------------------------------------------------------------
        if (!fread(&headerMbmOut, sizeof(util::HEADER), 1, fp))
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header file [%s]", fileNamePath);
        if (strncmp(headerMbmOut.name, "mbm", 3) == 0 &&
            (strncmp(headerMbmOut.typeApp, "Mesh 3d mbm", 15) == 0 || // Mesh 3d normal
             strncmp(headerMbmOut.typeApp, "User mbm", 15) == 0 ||    // user
             strncmp(headerMbmOut.typeApp, "Font mbm", 15) == 0 ||    // Font
             strncmp(headerMbmOut.typeApp, "Sprite mbm", 15) == 0 ||   // Sprite
             strncmp(headerMbmOut.typeApp, "Tile mbm", 15) == 0 ||   // binary Tile
             strncmp(headerMbmOut.typeApp, "Shape mbm", 15) == 0 ||   // shape
             strncmp(headerMbmOut.typeApp, "Particle mbm", 15) == 0)) // Particle
        {
            if (strncmp(headerMbmOut.typeApp, "Mesh 3d mbm", 15) == 0) // Mesh 3d normal
                typeOut = util::TYPE_MESH_3D;
            else if (strncmp(headerMbmOut.typeApp, "User mbm", 15) == 0) // special -- user
                typeOut = util::TYPE_MESH_USER;
            else if (strncmp(headerMbmOut.typeApp, "Font mbm", 15) == 0) // Font
                typeOut = util::TYPE_MESH_FONT;
            else if (strncmp(headerMbmOut.typeApp, "Sprite mbm", 15) == 0) // Sprite mbm
                typeOut = util::TYPE_MESH_SPRITE;
            else if (strncmp(headerMbmOut.typeApp, "Tile mbm", 15) == 0) // Tile mbm
                typeOut = util::TYPE_MESH_TILE_MAP;
            else if (strncmp(headerMbmOut.typeApp, "Particle mbm", 15) == 0) // Particle mbm
                typeOut = util::TYPE_MESH_PARTICLE;
            else if (strncmp(headerMbmOut.typeApp, "Shape mbm", 15) == 0) // Shape mbm
                typeOut = util::TYPE_MESH_SHAPE;
        }
        else
        {
            return log_util::onFailed(fp,__FILE__, __LINE__, "is not a mbm file!!\ntype of file: %s [%s]", headerMbmOut.typeApp,
                                     fileNamePath);
        }
        if (headerMbmOut.version < INITIAL_VERSION_MBM_HEADER || headerMbmOut.version > CURRENT_VERSION_MBM_HEADER)
            return log_util::onFailed(fp,__FILE__, __LINE__,"incompatible version [%s]\ncurrent version [%d] \nversion in file [%d]",fileNamePath, CURRENT_VERSION_MBM_HEADER, headerMbmOut.version);

        if(headerMbmOut.version >= MODE_DRAW_VERSION_MBM_HEADER)
        {
            if (!fread(&info_mode, sizeof(util::INFO_DRAW_MODE), 1, fp))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info INFO_DRAW_MODE [%s]", fileNamePath);
        }
        // step 2: --------------------------------------------------------------------------------------------------
        if (headerMbmOut.version >= DETAIL_MESH_VERSION_MBM_HEADER)
        {
            util::DETAIL_MESH detailInfo;
            if (headerMbmOut.version == DETAIL_MESH_VERSION_MBM_HEADER)
            {
                /* ************* DEPRECATED - Begin - old just here to compatibility ***************** */
                if (!fread(&detailInfo, sizeof(util::DETAIL_MESH), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info DETAIL_MESH [%s]", fileNamePath);
                if (detailInfo.type != 100 && detailInfo.type != 101) // script and shader until now
                    return log_util::onFailed(fp,__FILE__, __LINE__,"expected first DETAIL_MESH [%s] as size info extra information at version == DETAIL_MESH_VERSION_MBM_HEADER",fileNamePath);
                if (detailInfo.totalBounding)
                {
                    const int extraISize = detailInfo.totalBounding;
                    auto     _extraInfo    = new char[extraISize];
                    if (!fread(_extraInfo, static_cast<size_t>(extraISize), 1, fp))
                    {
                        delete[] _extraInfo;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read extra info [%s]", fileNamePath);
                    }
                    delete[] _extraInfo;
                }
                /* ************* End - old just here to compatibility ***************** */
            }
            
            if (!fread(&detailInfo, sizeof(util::DETAIL_MESH), 1, fp))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info DETAIL_MESH [%s]", fileNamePath);
            if (headerMbmOut.version == DETAIL_MESH_VERSION_MBM_HEADER)
            {
                if (detailInfo.type != 'H')
                    return log_util::onFailed(fp,__FILE__, __LINE__, "expected 'H' at DETAIL_MESH [%s]", fileNamePath);
            }
            else
            {
                if (detailInfo.type != 'P')
                    return log_util::onFailed(fp,__FILE__, __LINE__, "expected 'P' from Physics at DETAIL_MESH [%s]", fileNamePath);
            }
            for (int i = 0; i < detailInfo.totalBounding; )
            {
                util::DETAIL_MESH detail;
                if (!fread(&detail, sizeof(util::DETAIL_MESH), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read DETAIL_MESH [%s]", fileNamePath);
                switch (detail.type)
                {
                    case 1:
                    {
                        for(int j=0; j< detail.totalBounding; j++)
                        {
                            CUBE cube;
                            if (!fread(&cube, sizeof(CUBE), 1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 2:
                    {
                        for(int j=0; j< detail.totalBounding; j++)
                        {
                            SPHERE base;
                            if (!fread(&base, sizeof(SPHERE), 1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 3:
                    {
                        for(int j=0; j< detail.totalBounding; j++)
                        {
                            CUBE_COMPLEX complex;
                            if (!fread(&complex, sizeof(CUBE_COMPLEX), 1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 4:
                    {
                        //Introduced position to the triangle
                        if(headerMbmOut.version >= MODE_DRAW_VERSION_MBM_HEADER)
                        {
                            for(int j=0; j< detail.totalBounding; j++)
                            {
                                TRIANGLE triangle;
                                if (!fread(&triangle, sizeof(TRIANGLE), 1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                            }
                        }
                        else
                        {
                            for(int j=0; j< detail.totalBounding; j++)
                            {
                                TRIANGLE triangle;
                                if (!fread(&triangle, sizeof(TRIANGLE) - sizeof(VEC2), 1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                            }
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 5:
                    {
                        util::DETAIL_HEADER_FONT headerFont;
                        if (!fread(&headerFont, sizeof(util::DETAIL_HEADER_FONT), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_HEADER_FONT [%s]",
                                                     fileNamePath);
                        auto strNameFont = new char[headerFont.sizeNameFonte];
                        if (!fread(strNameFont, static_cast<size_t>(headerFont.sizeNameFonte), 1, fp))
                        {
                            delete [] strNameFont;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load font's name [%s]", fileNamePath);
                        }
                        datailFontOut.fontName        = strNameFont;
                        datailFontOut.heightLetter    = headerFont.heightLetter;
                        datailFontOut.spaceXCharacter = headerFont.spaceXCharacter;
                        datailFontOut.spaceYCharacter = headerFont.spaceYCharacter;
                        delete[] strNameFont;
                        for (int j = 0; j < headerFont.totalDetailFont; ++j)
                        {
                            auto detailFont = new util::DETAIL_LETTER();
                            if (!fread(detailFont, sizeof(util::DETAIL_LETTER), 1, fp))
                            {
                                delete detailFont;
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER [%s]", fileNamePath);
                            }
                            if (detailFont->indexFrame >= headerFont.totalDetailFont)
                            {
                                delete detailFont;
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER!! frame out of bound [%s]", fileNamePath);
                            }
                            datailFontOut.letter[detailFont->letter].detail = detailFont;
                        }
                        i += 1;
                    }
                    break;
                    case 6:
                    {
                        for (int j = 0; j< detail.totalBounding; j++)
                        {
                            util::STAGE_PARTICLE stage;
                            if (!fread(&stage, sizeof(util::STAGE_PARTICLE), 1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read stage particle [%s]", fileNamePath);
                            lsStageParticle.push_back(stage);
                        }
                        i += 1;
                    }
                    break;
                    case 7:
                    {
                        util::BTILE_INFO infoTileMap;
                        
                        if (!fread(&infoTileMap.map,  sizeof(util::BTILE_HEADER_MAP),1,fp))
                        {
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read from  detail tile!");
                        }

                        if(infoTileMap.map.layerCount != static_cast<uint32_t>(detail.totalBounding))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "expected layerCount == totalBounding. [%d]!=[%d]",infoTileMap.map.layerCount,detail.totalBounding);


                        infoTileMap.infoBrickEditor = new util::BTILE_BRICK_INFO[infoTileMap.map.countRawTiles];
                        if (!fread(infoTileMap.infoBrickEditor,  sizeof(util::BTILE_BRICK_INFO) * infoTileMap.map.countRawTiles,1,fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read brick info editor tile from  detail tile!");

                        const uint32_t tileCount = infoTileMap.map.count_height_tile * infoTileMap.map.count_width_tile;
                        infoTileMap.layers = new util::BTILE_LAYER[infoTileMap.map.layerCount];

                        for (uint32_t  j = 0; j < infoTileMap.map.layerCount; ++j)
                        {
                            if (!fread(infoTileMap.layers[j].offset,  sizeof(infoTileMap.layers[j].offset),1,fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read layer offset from tile!");

                            infoTileMap.layers[j].lsIndexTiles = new util::BTILE_INDEX_TILE[tileCount];

                            if (!fread(infoTileMap.layers[j].lsIndexTiles,  sizeof(util::BTILE_INDEX_TILE) * tileCount,1,fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read indexed tile from  detail tile!");
                        }

                        util::BTILE_DETAIL_HEADER detailObjsProperty;
                        if (!fread(&detailObjsProperty, sizeof(util::BTILE_DETAIL_HEADER),1,fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");

                        for(uint32_t j= 0 ; j < detailObjsProperty.totalObj; ++j)
                        {
                            util::BTILE_OBJ_HEADER objHeader;
                            if (!fread(&objHeader, sizeof(util::BTILE_OBJ_HEADER), 1,fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");
                            auto* obj = new util::BTILE_OBJ(static_cast<util::BTILE_OBJ_TYPE>(objHeader.type));
                            infoTileMap.lsObj.push_back(obj);
                            if(objHeader.sizeName > 0)
                            {
                                obj->name.resize(objHeader.sizeName);
                                auto * name = const_cast<char*>(obj->name.data());
                                if (!fread(name, objHeader.sizeName,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read name at detail tile header!");
                            }
                            for(uint32_t k= 0 ; k < objHeader.sizePoints; ++k)
                            {
                                auto* point = new mbm::VEC2();
                                obj->lsPoints.push_back(point);
                                if (!fread(point, sizeof(mbm::VEC2),1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");
                            }
                        }

                        for(uint32_t j= 0 ; j < detailObjsProperty.totalProperties; ++j)
                        {
                            util::BTILE_PROPERTY_HEADER propertyHeader;
                            if (!fread(&propertyHeader, sizeof(util::BTILE_PROPERTY_HEADER),1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail property tile header!");
                            auto* property = new util::BTILE_PROPERTY();
                            infoTileMap.lsProperty.push_back(property);
                            if(propertyHeader.nameLength > 0)
                            {
                                property->name.resize(propertyHeader.nameLength);
                                auto * name = const_cast<char*>(property->name.data());
                                if (!fread(name,propertyHeader.nameLength,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property name at detail tile header!");
                            }
                            if(propertyHeader.valueLength > 0)
                            {
                                property->value.resize(propertyHeader.valueLength);
                                auto * value = const_cast<char*>(property->value.data());
                                if (!fread(value,propertyHeader.valueLength,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property value at detail tile header!");
                            }
                            if(propertyHeader.ownerLength > 0)
                            {
                                property->owner.resize(propertyHeader.ownerLength);
                                auto * owner = const_cast<char*>(property->owner.data());
                                if (!fread(owner,propertyHeader.ownerLength,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property owner at detail tile header!");
                            }
                        }
                        i += 1;
                    }
                    break;
                    default:
                    {
                        return log_util::onFailed(fp,__FILE__, __LINE__, "unknown type bounding box [%d] [%s]", detail.type,
                                                 fileNamePath);
                    }
                }
            }
        }
#if defined USE_DEPRECATED_2_MINOR
        else
        {
            // step 2: --------------------------------------------------------------------------------------------------
            switch (typeOut)
            {
                case util::TYPE_MESH_3D:
                {
                    util::DETAIL_MESH detail;
                    // 2.1 Cubos -- Todos os boundings
                    // --------------------------------------------------------------------
                    if (!fread(&detail, sizeof(util::DETAIL_MESH), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load bounding box [%s]", fileNamePath);
                    if (detail.totalBounding)
                    {
                        switch (detail.type) // 1: Bounding box. 2: Esferico. 3: Cube poligono .
                        {
                            case 1:
                            {
                                for (int i = 0; i < detail.totalBounding; ++i)
                                {
                                    CUBE base;
                                    if (!fread(&base, sizeof(CUBE), 1, fp))
                                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]",
                                                                 fileNamePath);
                                    ;
                                }
                            }
                            break;
                            case 2:
                            {
                                for (int i = 0; i < detail.totalBounding; ++i)
                                {
                                    SPHERE base;
                                    if (!fread(&base, sizeof(SPHERE), 1, fp))
                                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]",
                                                                 fileNamePath);
                                }
                            }
                            break;
                            case 3:
                            {
                                for (int i = 0; i < detail.totalBounding; ++i)
                                {
                                    CUBE_COMPLEX complex;
                                    if (!fread(&complex, sizeof(CUBE_COMPLEX), 1, fp))
                                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]",
                                                                 fileNamePath);
                                    ;
                                }
                            }
                            break;
                            default: { return log_util::onFailed(fp,__FILE__, __LINE__, "bounding unknown [%s]", fileNamePath);
                            }
                        }
                    }
                }
                break;
                case util::TYPE_MESH_SPRITE:
                {
                    float zx, zy;
                    if (!deprectedInfoSprite.readBoundingSprite(fp, fileNamePath, &zx, &zy))
                        return false;
                }
                break;
                case util::TYPE_MESH_USER:
                {
                    // special -- user
                }
                break;
                case util::TYPE_MESH_FONT:
                {
                    util::DETAIL_HEADER_FONT headerFont;
                    if (!fread(&headerFont, sizeof(util::DETAIL_HEADER_FONT), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_HEADER_FONT [%s]", fileNamePath);
                    auto strNameFont = new char[headerFont.sizeNameFonte];
                    if (!fread(strNameFont, static_cast<size_t>(headerFont.sizeNameFonte), 1, fp))
                    {
                        delete [] strNameFont;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load font's name [%s]", fileNamePath);
                    }
                    datailFontOut.fontName        = strNameFont;
                    datailFontOut.heightLetter    = headerFont.heightLetter;
                    datailFontOut.spaceXCharacter = headerFont.spaceXCharacter;
                    datailFontOut.spaceYCharacter = headerFont.spaceYCharacter;
                    delete[] strNameFont;
                    for (int i = 0; i < headerFont.totalDetailFont; ++i)
                    {
                        auto detailFont = new util::DETAIL_LETTER();
                        if (!fread(detailFont, sizeof(util::DETAIL_LETTER), 1, fp))
                        {
                            delete detailFont;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER [%s]", fileNamePath);
                        }
                        if (detailFont->indexFrame >= headerFont.totalDetailFont)
                        {
                            delete detailFont;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER!! frame out of bound [%s]",
                                                     fileNamePath);
                        }
                        datailFontOut.letter[detailFont->letter].detail = detailFont;
                    }
                }
                break;
                default: {
                }
            }
        }
#else
        else
        {
            return log_util::onFailed(fp,__FILE__, __LINE__, "Imcompatible version [%d]", headerMbmOut.version);
        }
#endif
        // 3 headerMesh MBM -------------------------------------------------------------------------------
        if (!fread(&headerMeshMbmOut, sizeof(headerMeshMbmOut), 1, fp))
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read HEADER_MESH [%s]", fileNamePath);
        fclose(fp);
        fp = nullptr;
        return true;
    }
    
    util::TYPE_MESH MESH_MBM_DEBUG::getType() noexcept
    {
        if (this->buffer.size())
            return this->typeMe;
        return util::TYPE_MESH_UNKNOWN;
    }
    
    util::TYPE_MESH MESH_MBM_DEBUG::getType(const char *fileNamePath)
    {
        bool isImage    = false;
        bool isMesh     = false;
        bool isUnknown  = false;
        const char* ext = getValidExtension(fileNamePath,isImage,isMesh,isUnknown);
        if(ext == nullptr)
            return util::TYPE_MESH_UNKNOWN;
        if(isImage)
            return util::TYPE_MESH_TEXTURE;
        if(isUnknown)
            return util::TYPE_MESH_UNKNOWN;
        if(!isMesh)
            return util::TYPE_MESH_UNKNOWN;
        if(strcasecmp(ext,"SPT") == 0)
            return util::TYPE_MESH_SPRITE;
        if(strcasecmp(ext,"FNT") == 0)
            return util::TYPE_MESH_FONT;
        if(strcasecmp(ext,"MSH") == 0)
            return util::TYPE_MESH_3D;
        if(strcasecmp(ext,"PTL") == 0)
            return util::TYPE_MESH_PARTICLE;
        if (strcasecmp(ext, "TILE") == 0)
            return util::TYPE_MESH_TILE_MAP;
        util::TYPE_MESH typeOut = util::TYPE_MESH_UNKNOWN;
        util::HEADER    headerMbmOut;
        FILE *              fp = util::openFile(fileNamePath, "rb");
        if (fp == nullptr)
            return log_util::onFailed(fp,__FILE__, __LINE__, "Failed to open file [%s]", fileNamePath ? fileNamePath : "nullptr") ==
                           false
                       ? util::TYPE_MESH_UNKNOWN
                       : typeOut;
        fclose(fp);
        fp = nullptr;
        MINIZ minz;
        {
            char errorDesc[255]="";
            if (!minz.decompressFile(fileNamePath, util::getDecompressModelFileName(),errorDesc,sizeof(errorDesc)-1))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to uncompress file [%s]\n%s", fileNamePath,errorDesc) == false
                       ? util::TYPE_MESH_UNKNOWN
                       : typeOut;
        }
        
        fp = util::openFile(util::getDecompressModelFileName(), "rb");
        if (fp == nullptr)
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to open file [%s]", fileNamePath) == false
                       ? util::TYPE_MESH_UNKNOWN
                       : typeOut;
        // step 1: Verificação do header  MBM principal
        // -------------------------------------------------------------------------------
        if (!fread(&headerMbmOut, sizeof(util::HEADER), 1, fp))
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header file [%s]", fileNamePath) == false
                       ? util::TYPE_MESH_UNKNOWN
                       : typeOut;
        if (strncmp(headerMbmOut.name, "mbm", 3) == 0 &&
            (strncmp(headerMbmOut.typeApp, "Mesh 3d mbm", 15) == 0 || // Mesh 3d normal
             strncmp(headerMbmOut.typeApp, "User mbm", 15) == 0 ||    // user
             strncmp(headerMbmOut.typeApp, "Font mbm", 15) == 0 ||    // Font
             strncmp(headerMbmOut.typeApp, "Sprite mbm", 15) == 0 ||   // Sprite
             strncmp(headerMbmOut.typeApp, "Tile mbm", 15) == 0 ||   // Tile
             strncmp(headerMbmOut.typeApp, "Shape mbm", 15) == 0 ||   // Shape
             strncmp(headerMbmOut.typeApp, "Particle mbm", 15) == 0)) // Particle
        {
            if (strncmp(headerMbmOut.typeApp, "Mesh 3d mbm", 15) == 0) // Mesh 3d normal
                typeOut = util::TYPE_MESH_3D;
            else if (strncmp(headerMbmOut.typeApp, "User mbm", 15) == 0) // special -- user
                typeOut = util::TYPE_MESH_USER;
            else if (strncmp(headerMbmOut.typeApp, "Font mbm", 15) == 0) // Font
                typeOut = util::TYPE_MESH_FONT;
            else if (strncmp(headerMbmOut.typeApp, "Sprite mbm", 15) == 0) // Sprite mbm
                typeOut = util::TYPE_MESH_SPRITE;
            else if (strncmp(headerMbmOut.typeApp, "Particle mbm", 15) == 0) // Particle mbm
                typeOut = util::TYPE_MESH_PARTICLE;
            else if (strncmp(headerMbmOut.typeApp, "Tile mbm", 15) == 0) // Tile mbm
                typeOut = util::TYPE_MESH_TILE_MAP;
            else if (strncmp(headerMain.typeApp, "Shape mbm", 15) == 0) // Shape mbm
                typeMe = util::TYPE_MESH_SHAPE;
        }
        else
        {
            return log_util::onFailed(fp,__FILE__, __LINE__, "is not a mbm file!!\ntype of file: %s [%s]", headerMbmOut.typeApp,
                                     fileNamePath) == false
                       ? util::TYPE_MESH_UNKNOWN
                       : typeOut;
        }
        fclose(fp);
        fp = nullptr;
        return typeOut;
    }
    
    void MESH_MBM_DEBUG::calculateNormals()
    {
        headerMesh.totalFrames = static_cast<int>(this->buffer.size());
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            util::BUFFER_MESH_DEBUG *currentFrameBuffer = this->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(currentFrame)];
            auto *                   position           = reinterpret_cast<VEC3 *>(currentFrameBuffer->position);
            auto *                   normal             = reinterpret_cast<VEC3 *>(currentFrameBuffer->normal);
            const auto       sSub               = static_cast<uint32_t>(currentFrameBuffer->subset.size());
            if (currentFrameBuffer->indexBuffer == nullptr) // vertex
            {
                for (uint32_t indexSubset = 0; indexSubset < sSub; ++indexSubset)
                {
                    util::SUBSET_DEBUG *subset      = currentFrameBuffer->subset[indexSubset];
                    int                 countSubset = subset->vertexStart + subset->vertexCount;
                    countSubset -= (countSubset % 3);
                    for (int i = subset->vertexStart; (i + 3) < countSubset; i += 3)
                    {
                        VEC3      a, b, n;
                        const int index0 = i;
                        const int index1 = i + 1;
                        const int index2 = i + 2;

                        a.x = position[index1].x - position[index0].x;
                        a.y = position[index1].y - position[index0].y;
                        a.z = position[index1].z - position[index0].z;

                        b.x = position[index2].x - position[index0].x;
                        b.y = position[index2].y - position[index0].y;
                        b.z = position[index2].z - position[index0].z;

                        vec3Cross(&n, &a, &b);
                        vec3Normalize(&n, &n);

                        normal[index0] = n;
                        normal[index1] = n;
                        normal[index2] = n;
                    }
                }
            }
            else
            {
                for (uint32_t indexSubset = 0; indexSubset < sSub; ++indexSubset)
                {
                    util::SUBSET_DEBUG *subset           = currentFrameBuffer->subset[indexSubset];
                    int                 countIndexSubset = subset->vertexStart + subset->vertexCount;
                    countIndexSubset -= (countIndexSubset % 3);
                    for (int i = subset->indexStart; (i + 3) < countIndexSubset; i += 3)
                    {
                        VEC3      a, b, n;
                        const auto index0 = static_cast<int>(currentFrameBuffer->indexBuffer[i]);
                        const auto index1 = static_cast<int>(currentFrameBuffer->indexBuffer[i + 1]);
                        const auto index2 = static_cast<int>(currentFrameBuffer->indexBuffer[i + 2]);

                        a.x = position[index1].x - position[index0].x;
                        a.y = position[index1].y - position[index0].y;
                        a.z = position[index1].z - position[index0].z;

                        b.x = position[index2].x - position[index0].x;
                        b.y = position[index2].y - position[index0].y;
                        b.z = position[index2].z - position[index0].z;

                        vec3Cross(&n, &a, &b);
                        vec3Normalize(&n, &n);

                        normal[index0] = n;
                        normal[index1] = n;
                        normal[index2] = n;
                    }
                }
            }
        }
    }
    
    void MESH_MBM_DEBUG::calculateUV()
    {
        headerMesh.totalFrames = static_cast<int>(this->buffer.size());
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            util::BUFFER_MESH_DEBUG *currentFrameBuffer = this->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(currentFrame)];
            const auto  sSub   = static_cast<uint32_t>(currentFrameBuffer->subset.size());
            VEC2                     vMin(FLT_MAX, FLT_MAX);
            VEC2                     vMax(-FLT_MAX, -FLT_MAX);
            const auto *             pPosition = reinterpret_cast<VEC3 *>(currentFrameBuffer->position);
            auto *                   pUv       = reinterpret_cast<VEC2 *>(currentFrameBuffer->uv);
            for (uint32_t indexSubset = 0; indexSubset < sSub; ++indexSubset)
            {
                const util::SUBSET_DEBUG *subset = currentFrameBuffer->subset[indexSubset];
                int                 countSubset  = subset->vertexStart + subset->vertexCount;
                countSubset -= (countSubset % 3);
                for (int i = subset->vertexStart; i < countSubset; ++i)
                {
                    const VEC3 *position = &pPosition[i];
                    if (position->x > vMax.x)
                        vMax.x = position->x;
                    if (position->y > vMax.y)
                        vMax.y = position->y;

                    if (position->x < vMin.x)
                        vMin.x = position->x;
                    if (position->y < vMin.y)
                        vMin.y = position->y;
                }
            }
            const float tmp[2] = {(vMax.x - vMin.x),(vMax.y - vMin.y)};
            const float width  = (tmp[0] == 0.0f ? 1.0f : tmp[0]);
            const float height = (tmp[1] == 0.0f ? 1.0f : tmp[1]);
            vMin.x = -vMin.x;
            vMin.y = -vMin.y;
            for (uint32_t indexSubset = 0; indexSubset < sSub; ++indexSubset)
            {
                const util::SUBSET_DEBUG *subset = currentFrameBuffer->subset[indexSubset];
                const int           countSubset  = subset->vertexStart + subset->vertexCount;
                for (int i = subset->vertexStart; i < countSubset; ++i)
                {
                    const VEC3 *position = &pPosition[i];
                    VEC2 *uv       = &pUv[i];
                    uv->x          = (position->x + vMin.x) / width;
                    uv->y          = 1.0f - ((position->y + vMin.y) / height);
                }
            }
        }
    }
    
    bool MESH_MBM_DEBUG::saveDebug(const char *fileOut, const bool recalculateNormal, const bool recalculateUV, char *errorOut,const int lenErrorOut)
    {
        if (this->buffer.size() == 0)
            return false;
        FILE *file = nullptr;
        strncpy(headerMain.name, "mbm",sizeof(headerMain.name)-1);
        headerMesh.totalFrames  = static_cast<int>(this->buffer.size());
        headerMain.version     = CURRENT_VERSION_MBM_HEADER;
        headerMain.reserved    = 0;
        headerMain.extraHeader = 0;
        headerMain.magic       = 0x010203ff;
        switch (typeMe)
        {
            case util::TYPE_MESH_3D:        {strncpy(headerMain.typeApp, "Mesh 3d mbm",sizeof(headerMain.typeApp)-1);}
            break;
            case util::TYPE_MESH_SPRITE:    {strncpy(headerMain.typeApp, "Sprite mbm",sizeof(headerMain.typeApp)-1);}
            break;
            case util::TYPE_MESH_TILE_MAP:  {strncpy(headerMain.typeApp, "Tile mbm",sizeof(headerMain.typeApp)-1);}
            break;
            case util::TYPE_MESH_FONT:      {strncpy(headerMain.typeApp, "Font mbm",sizeof(headerMain.typeApp)-1);}
            break;
            case util::TYPE_MESH_PARTICLE:  {strncpy(headerMain.typeApp, "Particle mbm",sizeof(headerMain.typeApp)-1); }
            break;
            case util::TYPE_MESH_SHAPE:     {strncpy(headerMain.typeApp, "Shape mbm",sizeof(headerMain.typeApp)-1); }
            break;
            default:
            {
                bool allstride2 = true;
                for (auto & i : this->buffer)
                {
                    if (i->headerFrame.stride != 2)
                    {
                        allstride2 = false;
                        break;
                    }
                }
                if (allstride2)
                {
                    typeMe = util::TYPE_MESH_SPRITE;
                    strncpy(headerMain.typeApp, "Sprite mbm",sizeof(headerMain.typeApp)-1);
                }
                else
                {
                    typeMe = util::TYPE_MESH_3D;
                    strncpy(headerMain.typeApp, "Mesh 3d mbm",sizeof(headerMain.typeApp)-1);
                }
            }
        }
        if (errorOut)
        {
            if (!check(errorOut,lenErrorOut))
            {
                return log_util::onFailed(file,__FILE__, __LINE__, "error on check mesh to save.");
            }
        }
        else
        {
            char strError[255] = "";
            if (!check(strError,sizeof(strError)-1))
            {
                return log_util::onFailed(file,__FILE__, __LINE__, strError);
            }
        }
        if (recalculateNormal)
        {
            this->calculateNormals();
            this->headerMesh.hasNorText[0] = 1;
        }
        if (recalculateUV)
        {
            this->calculateUV();
            this->headerMesh.hasNorText[1] = 1;
        }
        {
            std::string which_mode;
            if(is_any_mode_valid(this->info_mode,which_mode) == false)
            {
                return log_util::onFailed(file,__FILE__, __LINE__, "Invalid mode %s detected:[%s]",which_mode.c_str(),fileOut);
            }
        }

        // 1 header MBM -------------------------------------------------------------------------------
        if (!util::saveToFileBinary(fileOut, &this->headerMain, sizeof(util::HEADER), nullptr, 0, &file))
            return log_util::onFailed(file,__FILE__, __LINE__, "Failed to save file [%s]", fileOut);

        if (!util::addToFileBinary(fileOut,&this->info_mode, sizeof(util::INFO_DRAW_MODE),&file))
            return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail INFO_DRAW_MODE [%s]", fileOut);

        int totalBounding = ((typeMe == util::TYPE_MESH_FONT) || (typeMe == util::TYPE_MESH_PARTICLE) || (typeMe == util::TYPE_MESH_TILE_MAP)) ? 1 : 0;
        totalBounding += static_cast<int>(this->infoPhysics.lsCube.size());
        totalBounding += static_cast<int>(this->infoPhysics.lsSphere.size());
        totalBounding += static_cast<int>(this->infoPhysics.lsCubeComplex.size());
        totalBounding += static_cast<int>(this->infoPhysics.lsTriangle.size());
        if (totalBounding == 0)
        {
            this->fillAtLeastOneBound();
            totalBounding = 1;
        }
        util::DETAIL_MESH detailHeader;
        detailHeader.totalBounding = totalBounding;
        detailHeader.type          = 'P'; //Physics
        if (!util::addToFileBinary(fileOut, &detailHeader, sizeof(util::DETAIL_MESH), &file))
            return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail header bounding box!!");

        if (this->infoPhysics.lsCube.size())
        {
            util::DETAIL_MESH detail;
            detail.totalBounding = static_cast<int>(this->infoPhysics.lsCube.size());
            detail.type          = 1;
            if (!util::addToFileBinary(fileOut, &detail, sizeof(util::DETAIL_MESH), &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail bounding box!!");
            for (int i = 0; i < detail.totalBounding; ++i)
            {
                CUBE *cube = this->infoPhysics.lsCube[static_cast<std::vector<CUBE*>::size_type>(i)];
                if (!util::addToFileBinary(fileOut, cube, sizeof(CUBE), &file))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to save bounding box!");
            }
        }
        if (this->infoPhysics.lsSphere.size())
        {
            util::DETAIL_MESH detail;
            detail.totalBounding = static_cast<int>(this->infoPhysics.lsSphere.size());
            detail.type          = 2;
            if (!util::addToFileBinary(fileOut, &detail, sizeof(util::DETAIL_MESH), &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail bounding box!!");
            for (int i = 0; i < detail.totalBounding; ++i)
            {
                SPHERE *sphere = this->infoPhysics.lsSphere[static_cast<std::vector<SPHERE*>::size_type>(i)];
                if (!util::addToFileBinary(fileOut, sphere, sizeof(SPHERE), &file))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to save bounding box!");
            }
        }
        if (this->infoPhysics.lsCubeComplex.size())
        {
            util::DETAIL_MESH detail;
            detail.totalBounding = static_cast<int>(this->infoPhysics.lsCubeComplex.size());
            detail.type          = 3;
            if (!util::addToFileBinary(fileOut, &detail, sizeof(util::DETAIL_MESH), &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail bounding box!!");
            for (int i = 0; i < detail.totalBounding; ++i)
            {
                CUBE_COMPLEX *complex = this->infoPhysics.lsCubeComplex[static_cast<std::vector<CUBE_COMPLEX*>::size_type>(i)];
                if (!util::addToFileBinary(fileOut, complex, sizeof(CUBE_COMPLEX), &file))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to save bounding box!");
            }
        }
        if (this->infoPhysics.lsTriangle.size())
        {
            util::DETAIL_MESH detail;
            detail.totalBounding = static_cast<int>(this->infoPhysics.lsTriangle.size());
            detail.type          = 4;
            if (!util::addToFileBinary(fileOut, &detail, sizeof(util::DETAIL_MESH), &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail bounding box!!");
            for (int i = 0; i < detail.totalBounding; ++i)
            {
                TRIANGLE *triangle = this->infoPhysics.lsTriangle[static_cast<std::vector<TRIANGLE*>::size_type>(i)];
                if (!util::addToFileBinary(fileOut, triangle, sizeof(TRIANGLE), &file))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to save bounding box!");
            }
        }
        if (typeMe == util::TYPE_MESH_FONT)
        {
            util::DETAIL_MESH detail;
            auto *infoFont = static_cast<INFO_BOUND_FONT *>(this->extraInfo);
            if(infoFont == nullptr)
              return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail font, infoFont is null!");
            detail.totalBounding = 1;
            detail.type          = 5;
            if (!util::addToFileBinary(fileOut, &detail, sizeof(util::DETAIL_MESH), &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail font!");
            util::DETAIL_HEADER_FONT headerFont;
            headerFont.totalDetailFont = 0;
            for (int i = 0; i < 255; ++i)
            {
                LETTER *letter = &infoFont->letter[static_cast<std::vector<LETTER*>::size_type>(i)];
                if (letter->detail)
                {
                    headerFont.totalDetailFont++;
                }
            }
            headerFont.sizeNameFonte   = static_cast<uint16_t>(infoFont->fontName.size() + 1);
            headerFont.spaceXCharacter = static_cast<char>(infoFont->spaceXCharacter);
            headerFont.spaceYCharacter = static_cast<char>(infoFont->spaceYCharacter);
            headerFont.heightLetter    = static_cast<uint16_t>(infoFont->heightLetter);
            if (!util::addToFileBinary(fileOut, &headerFont, sizeof(util::DETAIL_HEADER_FONT), &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to add header DETAIL_HEADER_FONT!!");

            if (!util::addToFileBinary(fileOut,infoFont->fontName.c_str(), infoFont->fontName.size() + 1, &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save font's name!!");

            int totalLetters = 0;
            for (int i = 0; i < 255; ++i)
            {
                LETTER *letter = &infoFont->letter[static_cast<std::vector<LETTER*>::size_type>(i)];
                if (letter->detail)
                {
                    util::DETAIL_LETTER *detailFont = letter->detail;
                    if (!util::addToFileBinary(fileOut, detailFont, sizeof(util::DETAIL_LETTER), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to include detail DETAIL_LETTER!!");
                    totalLetters++;
                }
            }
            if (totalLetters != this->headerMesh.totalFrames)
                return log_util::onFailed(
                    file, __FILE__,__LINE__, "failed to include detail DETAIL_LETTER!!\ntotal of frames different of letters!!!");
        }
        else if (typeMe == util::TYPE_MESH_PARTICLE)
        {
            util::DETAIL_MESH detail;
            const auto* lsParticleInfo = static_cast<const std::vector<util::STAGE_PARTICLE*>*>(this->extraInfo);
            if(lsParticleInfo == nullptr)
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to include detail particle!!");
            detail.totalBounding = static_cast<int>(lsParticleInfo->size());
            detail.type = 6;
            if (!util::addToFileBinary(fileOut, &detail, sizeof(util::DETAIL_MESH), &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail particle!");
            for (int i = 0; i < detail.totalBounding; ++i)
            {
                util::STAGE_PARTICLE* stage = lsParticleInfo->at(static_cast<std::vector<util::STAGE_PARTICLE*>::size_type>(i));
                if (!util::addToFileBinary(fileOut, stage, sizeof(util::STAGE_PARTICLE), &file))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to include detail particle!!");
            }
        }
        else if (typeMe == util::TYPE_MESH_TILE_MAP)
        {
            util::DETAIL_MESH detail;
            
            auto* infoTileMap = static_cast<util::BTILE_INFO*>(this->extraInfo);
            if(infoTileMap == nullptr)
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail tile!");
            detail.totalBounding = infoTileMap->map.layerCount;
            detail.type = 7;
            if (!util::addToFileBinary(fileOut, &detail, sizeof(util::DETAIL_MESH), &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail tile!");

            if (!util::addToFileBinary(fileOut, &infoTileMap->map , sizeof(util::BTILE_HEADER_MAP), &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail tile!");

            if (!util::addToFileBinary(fileOut, infoTileMap->infoBrickEditor, sizeof(util::BTILE_BRICK_INFO) * infoTileMap->map.countRawTiles, &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to include brick editor info tile!!");

            const uint32_t tileCount = infoTileMap->map.count_height_tile * infoTileMap->map.count_width_tile;
            
            for (uint32_t  i = 0; i < infoTileMap->map.layerCount; ++i)
            {
                const util::BTILE_INDEX_TILE* lsIndexTiles = infoTileMap->layers[i].lsIndexTiles;
                
                if (!util::addToFileBinary(fileOut, infoTileMap->layers[i].offset, sizeof(infoTileMap->layers[i].offset), &file))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to include offset of layer detail tile!!");

                if (!util::addToFileBinary(fileOut, lsIndexTiles, sizeof(util::BTILE_INDEX_TILE) * tileCount, &file))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to include index detail tile!!");
            }

            util::BTILE_DETAIL_HEADER detailObjsProperty;
            detailObjsProperty.totalObj			= static_cast<uint32_t>(infoTileMap->lsObj.size());
            detailObjsProperty.totalProperties	= static_cast<uint32_t>(infoTileMap->lsProperty.size());
            if (!util::addToFileBinary(fileOut, &detailObjsProperty, sizeof(util::BTILE_DETAIL_HEADER), &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail tile header!");

            for(uint32_t i= 0 ; i < detailObjsProperty.totalObj; ++i)
            {
                util::BTILE_OBJ* obj = infoTileMap->lsObj[i];
                util::BTILE_OBJ_HEADER objHeader(obj);
                if (!util::addToFileBinary(fileOut, &objHeader, sizeof(util::BTILE_OBJ_HEADER), &file))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail tile header!");
                if(obj->name.size() > 0)
                {
                    if (!util::addToFileBinary(fileOut, obj->name.c_str(), obj->name.size(), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to save name at detail tile header!");
                }
                for(auto & lsPoint : obj->lsPoints)
                {
                    if (!util::addToFileBinary(fileOut, lsPoint, sizeof(VEC2), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail tile header!");
                }
            }

            for(uint32_t i= 0 ; i < detailObjsProperty.totalProperties; ++i)
            {
                util::BTILE_PROPERTY* property = infoTileMap->lsProperty[i];
                util::BTILE_PROPERTY_HEADER propertyHeader(property);
                if (!util::addToFileBinary(fileOut, &propertyHeader, sizeof(util::BTILE_PROPERTY_HEADER), &file))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to save detail property tile header!");
                if(property->name.size() > 0)
                {
                    if (!util::addToFileBinary(fileOut, property->name.c_str(), property->name.size(), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to save property name at detail tile header!");
                }
                if(property->value.size() > 0)
                {
                    if (!util::addToFileBinary(fileOut, property->value.c_str(), property->value.size(), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to save property value at detail tile header!");
                }
                if(property->owner.size() > 0)
                {
                    if (!util::addToFileBinary(fileOut, property->owner.c_str(), property->owner.size(), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to save property owner at detail tile header!");
                }
            }
        }
        headerMesh.totalAnimation = static_cast<int>(this->infoAnimation.lsHeaderAnim.size());
        if (headerMesh.totalAnimation == 0)
        {
            auto infoHead = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            this->infoAnimation.lsHeaderAnim.push_back(infoHead);
            headerMesh.totalAnimation = static_cast<int>(this->infoAnimation.lsHeaderAnim.size());
            infoHead->headerAnim     = new util::HEADER_ANIMATION();
            strncpy(infoHead->headerAnim->nameAnimation, "default",sizeof(infoHead->headerAnim->nameAnimation)-1);
        }

        // 3 headerMesh MBM -------------------------------------------------------------------------------
        if (!util::addToFileBinary(fileOut, &headerMesh, sizeof(util::HEADER_MESH), &file))
            return log_util::onFailed(file,__FILE__, __LINE__, "failed to save header of file!!");

        // 4 header anim -- Todas as animações -----------------------------------------------------------
        if (!this->saveAnimationHeaders(fileOut, &file))
            return false;

        // Loop principal atraves de todos os frames deste arquivo -----------------------------------------------
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            util::BUFFER_MESH_DEBUG *currentFrameBuffer = this->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG *>::size_type>(currentFrame)];
            auto             totalSubset        = static_cast<uint32_t>(currentFrameBuffer->subset.size());
            // 5 Cada header Frame
            // --------------------------------------------------------------------------------------------------
            // Grava cada estrutura de frame para cada loop indicando os atributos do objeto
            // ----------------------------------------
            util::HEADER_FRAME *headerFrame = &currentFrameBuffer->headerFrame;

            const bool isIndexBuffer = currentFrameBuffer->indexBuffer != nullptr;

            // Verfifica se utiliza index buffer ou vertex buffer
            // -------------------------------------------------------------------
            if (!isIndexBuffer)
            {
                strncpy(headerFrame->typeBuffer, "VB",sizeof(headerFrame->typeBuffer)-1); // Opta por vertex buffer
                headerFrame->sizeIndexBuffer = 0;
            }
            else
            {
                strncpy(headerFrame->typeBuffer, "IB",sizeof(headerFrame->typeBuffer)-1); // Opta por index buffer
                uint32_t sIndex = 0;
                for (uint32_t i = 0; i < totalSubset; ++i)
                {
                    sIndex += static_cast<uint32_t>(currentFrameBuffer->subset[i]->indexCount);
                }
                headerFrame->sizeIndexBuffer = static_cast<int>(sIndex);
            }
            uint32_t sVertex = 0;
            for (uint32_t i = 0; i < totalSubset; ++i)
            {
                sVertex += static_cast<uint32_t>(currentFrameBuffer->subset[i]->vertexCount);
            }
            headerFrame->totalSubset      = static_cast<int>(totalSubset);
            headerFrame->sizeVertexBuffer = static_cast<int>(sVertex);
            if (!util::addToFileBinary(fileOut, headerFrame, sizeof(util::HEADER_FRAME), &file))
                return log_util::onFailed(file,__FILE__, __LINE__, "failed to add header of frame!");
            // 6 Todos os headers subset deste frame
            // -------------------------------------------------------------------------------
            for (int i = 0; i < headerFrame->totalSubset; ++i)
            {
                util::HEADER_DESC_SUBSET headerDescSubset;
                util::SUBSET_DEBUG *     pSubset = currentFrameBuffer->subset[static_cast<std::vector<util::SUBSET_DEBUG *>::size_type>(i)];
                if (pSubset->texture.size())
                {
                    std::vector<std::string> lsRet;
                    util::split(lsRet, pSubset->texture.c_str(), '\\');
                    const std::vector<std::string>::size_type s = lsRet.size();
                    if (s)
                    {
                        const std::string newOne(lsRet[s - 1]);
                        util::split(lsRet, newOne.c_str(), '/');
                        const std::vector<std::string>::size_type s2 = lsRet.size();
                        if (s2)
                            strncpy(headerDescSubset.nameTexture, lsRet[s2 - 1].c_str(), lsRet[s2 - 1].size());
                        else
                            strncpy(headerDescSubset.nameTexture, pSubset->texture.c_str(),sizeof(headerDescSubset.nameTexture) - 1);
                    }
                    else
                    {
                        strncpy(headerDescSubset.nameTexture, pSubset->texture.c_str(),sizeof(headerDescSubset.nameTexture) - 1);
                    }
                    if (typeMe == util::TYPE_MESH_FONT)
                    {
                        const std::string font_name_texture(headerDescSubset.nameTexture);
                        if(font_name_texture.find(".ttf") != std::string::npos)
                        {
                            return log_util::onFailed(file,__FILE__, __LINE__, "You must to load the font with 'save' flag enabled to save as png otherwise will not work...");
                        }
                    }
                }
                else
                    strncpy(headerDescSubset.nameTexture, "default",sizeof(headerDescSubset.nameTexture)-1);
                headerDescSubset.vertexStart = pSubset->vertexStart;
                headerDescSubset.indexStart  = pSubset->indexStart;
                headerDescSubset.vertexCount = pSubset->vertexCount;
                headerDescSubset.indexCount  = pSubset->indexCount;
                headerDescSubset.hasAlphaColor  = 1;

                if (!util::addToFileBinary(fileOut, &headerDescSubset, sizeof(util::HEADER_DESC_SUBSET), &file))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to add header of subset!");
            }

            // 6 index buffer se houver -----------------------------------------------------------------------------
            if (headerFrame->sizeIndexBuffer && strcmp(headerFrame->typeBuffer, "IB") == 0)
            {
                if (!util::addToFileBinary(fileOut, currentFrameBuffer->indexBuffer,
                                           static_cast<size_t>(headerFrame->sizeIndexBuffer) * sizeof(int16_t), &file))
                    return log_util::onFailed(file,__FILE__, __LINE__, "failed to add index buffer!!! !!");
            }
            // 6 Vertex buffer -----------------------------------------------------------------------------
            if (headerFrame->sizeVertexBuffer)
            {
                if (headerFrame->stride == 2)
                {
                    auto pPosition = new VEC2[headerFrame->sizeVertexBuffer];
                    auto *position  = reinterpret_cast<VEC3 *>(currentFrameBuffer->position);
                    for (int i = 0; i < headerFrame->sizeVertexBuffer; ++i)
                    {
                        pPosition[i].x = position[i].x;
                        pPosition[i].y = position[i].y;
                    }
                    if (!util::addToFileBinary(fileOut, pPosition, static_cast<size_t>(headerFrame->sizeVertexBuffer) * sizeof(VEC2), &file))
                    {
                        delete[] pPosition;
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to add vertex buffer");
                    }
                    delete[] pPosition;
                }
                else
                {
                    if (!util::addToFileBinary(fileOut, currentFrameBuffer->position,
                                               static_cast<size_t>(headerFrame->sizeVertexBuffer) * sizeof(VEC3), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to add vertex buffer");
                }
                if (headerMesh.hasNorText[0])
                {
                    if (!util::addToFileBinary(fileOut, currentFrameBuffer->normal,
                                               static_cast<size_t>(headerFrame->sizeVertexBuffer) * sizeof(VEC3), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to add vertex buffer");
                }
                if (headerMesh.hasNorText[1] == 1)
                {
                    if (!util::addToFileBinary(fileOut, currentFrameBuffer->uv,
                                               static_cast<size_t>(headerFrame->sizeVertexBuffer) * sizeof(VEC2), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to add vertex buffer");
                }
                else if (currentFrame == 0 && headerMesh.hasNorText[1] == 2)
                {
                    if (!util::addToFileBinary(fileOut, currentFrameBuffer->uv,
                                               static_cast<size_t>(headerFrame->sizeVertexBuffer) * sizeof(VEC2), &file))
                        return log_util::onFailed(file,__FILE__, __LINE__, "failed to add vertex buffer");
                }
            }
            else
                return log_util::onFailed(file,__FILE__, __LINE__, "total of vertex is zero");
        }
        if (file)
            fclose(file);
        file = nullptr;
        return this->compressFile(fileOut,errorOut,lenErrorOut);
    }

    bool MESH_MBM_DEBUG::loadDebug(const char *fileNamePath)
    {
        this->release();
        FILE *fp = util::openFile(fileNamePath, "rb");
        if (!fp)
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to open file [%s]", fileNamePath);
        MINIZ minz;
#if defined USE_DEPRECATED_2_MINOR
        deprecated_mbm::INFO_SPRITE deprectedInfoSprite; // version <=SPRITE_INFO_VERSION_MBM_HEADER
#endif
        fclose(fp);
        fp = nullptr;
        {
            char errorDesc[255]="";
            if (!minz.decompressFile(fileNamePath, util::getDecompressModelFileName(),errorDesc,sizeof(errorDesc)-1))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to uncompress file [%s]\n%s", fileNamePath,errorDesc);
        }
        
        fp = util::openFile(util::getDecompressModelFileName(), "rb");
        if (!fp)
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to open file [%s]", fileNamePath);
        fileName = fileNamePath;
        // step 1: Verificação do header  MBM principal
        // -------------------------------------------------------------------------------
        if (!fread(&headerMain, sizeof(util::HEADER), 1, fp))
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header file [%s]", fileNamePath);
        if (strncmp(headerMain.name, "mbm", 3) == 0 &&
            (strncmp(headerMain.typeApp, "Mesh 3d mbm", 15) == 0 || // Mesh 3d normal
             strncmp(headerMain.typeApp, "User mbm", 15) == 0 ||
             strncmp(headerMain.typeApp, "Font mbm", 15) == 0 ||  // Font
             strncmp(headerMain.typeApp, "Sprite mbm", 15) == 0 ||   // Sprite
             strncmp(headerMain.typeApp, "Tile mbm", 15) == 0 ||   // Tile
             strncmp(headerMain.typeApp, "Shape mbm", 15) == 0 ||   // Shape
             strncmp(headerMain.typeApp, "Particle mbm", 15) == 0)) // Particle
        {
            if (strncmp(headerMain.typeApp, "Mesh 3d mbm", 15) == 0) // Mesh 3d normal
                typeMe = util::TYPE_MESH_3D;
            else if (strncmp(headerMain.typeApp, "User mbm", 15) == 0)
                typeMe = util::TYPE_MESH_USER;
            else if (strncmp(headerMain.typeApp, "Font mbm", 15) == 0) // Font
                typeMe = util::TYPE_MESH_FONT;
            else if (strncmp(headerMain.typeApp, "Sprite mbm", 15) == 0) // Sprite mbm
                typeMe = util::TYPE_MESH_SPRITE;
            else if (strncmp(headerMain.typeApp, "Tile mbm", 15) == 0) // Tile mbm
                typeMe = util::TYPE_MESH_TILE_MAP;
            else if (strncmp(headerMain.typeApp, "Particle mbm", 15) == 0) // Particle mbm
                typeMe = util::TYPE_MESH_SPRITE;
            else if (strncmp(headerMain.typeApp, "Shape mbm", 15) == 0)
                typeMe = util::TYPE_MESH_SHAPE;
        }
        else
        {
            char strTemp[255];
            sprintf(strTemp, "[%s] is not a mbm file!!\ntype of file: %s", fileNamePath, headerMain.typeApp);
            return log_util::onFailed(fp,__FILE__, __LINE__, strTemp);
        }
        if (headerMain.version < INITIAL_VERSION_MBM_HEADER || headerMain.version > CURRENT_VERSION_MBM_HEADER)
            return log_util::onFailed(fp,__FILE__, __LINE__, "incompatible version [%s] version [%d]", fileNamePath,headerMain.version);

        if(headerMain.version >= MODE_DRAW_VERSION_MBM_HEADER)
        {
            if (!fread(&info_mode, sizeof(util::INFO_DRAW_MODE), 1, fp))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info INFO_DRAW_MODE [%s]", fileNamePath);
        }
        if(typeMe == util::TYPE_MESH_TILE_MAP)
        {
            mbm::TEXTURE::enableFilter(false);
        }
        else
        {
            mbm::TEXTURE::enableFilter(true);
        }
        // step 2: --------------------------------------------------------------------------------------------------
        if (headerMain.version >= DETAIL_MESH_VERSION_MBM_HEADER)
        {
            util::DETAIL_MESH detailInfo;
            if (headerMain.version == DETAIL_MESH_VERSION_MBM_HEADER)
            {
                /* ************* DEPRECATED - Begin - old just here to compatibility ***************** */
                if (!fread(&detailInfo, sizeof(util::DETAIL_MESH), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info DETAIL_MESH [%s]", fileNamePath);
                if (detailInfo.type != 100 && detailInfo.type != 101) // script and shader until now
                    return log_util::onFailed(fp,__FILE__, __LINE__,"expected first DETAIL_MESH [%s] as size info extra information at version == DETAIL_MESH_VERSION_MBM_HEADER",fileNamePath);
                if (detailInfo.totalBounding)
                {
                    const auto extraInfoSize = static_cast<uint32_t>(detailInfo.totalBounding);
                    auto * extra     = new char[extraInfoSize];
                    if (!fread(extra, static_cast<size_t>(extraInfoSize), 1, fp))
                    {
                        delete [] extra;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read old and deprected extra info [%s]", fileNamePath);
                    }
                    delete [] extra;
                }
                /* ************* End - old just here to compatibility ***************** */
            }
            
            if (!fread(&detailInfo, sizeof(util::DETAIL_MESH), 1, fp))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info DETAIL_MESH [%s]", fileNamePath);
            if (headerMain.version == DETAIL_MESH_VERSION_MBM_HEADER)
            {
                if (detailInfo.type != 'H')
                    return log_util::onFailed(fp,__FILE__, __LINE__, "expected 'H' at DETAIL_MESH [%s]", fileNamePath);
            }
            else
            {
                if (detailInfo.type != 'P')
                    return log_util::onFailed(fp,__FILE__, __LINE__, "expected 'P' from Physics at DETAIL_MESH [%s]", fileNamePath);
            }
            for (int i = 0; i < detailInfo.totalBounding; )
            {
                util::DETAIL_MESH detail;
                if (!fread(&detail, sizeof(util::DETAIL_MESH), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read DETAIL_MESH [%s]", fileNamePath);
                switch (detail.type)
                {
                    case 1:
                    {
                        for(int j=0; j< detail.totalBounding; j++)
                        {
                            auto cube = new CUBE();
                            this->infoPhysics.lsCube.push_back(cube);
                            if (!fread(cube, sizeof(CUBE), 1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 2:
                    {
                        for(int j=0; j< detail.totalBounding; j++)
                        {
                            auto base = new SPHERE();
                            this->infoPhysics.lsSphere.push_back(base);
                            if (!fread(base, sizeof(SPHERE), 1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 3:
                    {
                        for(int j=0; j< detail.totalBounding; j++)
                        {
                            auto complex = new CUBE_COMPLEX();
                            this->infoPhysics.lsCubeComplex.push_back(complex);
                            if (!fread(complex, sizeof(CUBE_COMPLEX), 1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 4:
                    {
                        //Introduced position to the triangle
                        if(headerMain.version >= MODE_DRAW_VERSION_MBM_HEADER)
                        {
                            for(int j=0; j< detail.totalBounding; j++)
                            {
                                auto triangle = new TRIANGLE();
                                this->infoPhysics.lsTriangle.push_back(triangle);
                                if (!fread(triangle, sizeof(TRIANGLE), 1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                            }
                        }
                        else
                        {
                            for(int j=0; j< detail.totalBounding; j++)
                            {
                                auto triangle = new TRIANGLE();
                                this->infoPhysics.lsTriangle.push_back(triangle);
                                if (!fread(triangle, sizeof(TRIANGLE) - sizeof(VEC2) , 1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                            }
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 5:
                    {
                        auto *infoFont = new INFO_BOUND_FONT();
                        this->extraInfo = infoFont;
                        util::DETAIL_HEADER_FONT headerFont;
                        if (!fread(&headerFont, sizeof(util::DETAIL_HEADER_FONT), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_HEADER_FONT [%s]",
                                                     fileNamePath);
                        auto strNameFont = new char[headerFont.sizeNameFonte];
                        if (!fread(strNameFont, static_cast<size_t>(headerFont.sizeNameFonte), 1, fp))
                        {
                            delete [] strNameFont;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load font's name [%s]", fileNamePath);
                        }
                        infoFont->fontName        = strNameFont;
                        infoFont->heightLetter    = headerFont.heightLetter;
                        infoFont->spaceXCharacter = headerFont.spaceXCharacter;
                        infoFont->spaceYCharacter = headerFont.spaceYCharacter;
                        delete[] strNameFont;
                        for (int j = 0; j < headerFont.totalDetailFont; ++j)
                        {
                            auto detailFont = new util::DETAIL_LETTER();
                            if (!fread(detailFont, sizeof(util::DETAIL_LETTER), 1, fp))
                            {
                                delete detailFont;
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER [%s]", fileNamePath);
                            }
                            if (detailFont->indexFrame >= headerFont.totalDetailFont)
                            {
                                delete detailFont;
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER!! frame out of bound [%s]", fileNamePath);
                            }
                            infoFont->letter[detailFont->letter].detail = detailFont;
                        }
                        i += 1;
                    }
                    break;
                    case 6:
                    {
                        auto* lsParticleInfo = new std::vector<util::STAGE_PARTICLE*>();
                        this->extraInfo = lsParticleInfo;
                        for (int j = 0; j< detail.totalBounding; j++)
                        {
                            auto stage = new util::STAGE_PARTICLE();
                            lsParticleInfo->push_back(stage);
                            if (!fread(stage, sizeof(util::STAGE_PARTICLE), 1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read stage particle [%s]", fileNamePath);
                        }
                        i += 1;
                    }
                    break;
                    case 7:
                    {
                        auto* infoTileMap = new util::BTILE_INFO();
                        this->extraInfo = infoTileMap;

                        if (!fread(&infoTileMap->map,  sizeof(util::BTILE_HEADER_MAP),1,fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read from  detail tile!");

                        if(infoTileMap->map.layerCount != static_cast<uint32_t>(detail.totalBounding))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "expected layerCount == totalBounding. [%d]!=[%d]",infoTileMap->map.layerCount,detail.totalBounding);


                        infoTileMap->infoBrickEditor = new util::BTILE_BRICK_INFO[infoTileMap->map.countRawTiles];
                        if (!fread(infoTileMap->infoBrickEditor,  sizeof(util::BTILE_BRICK_INFO) * infoTileMap->map.countRawTiles,1,fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read brick info editor tile from  detail tile!");

                        const uint32_t tileCount = infoTileMap->map.count_height_tile * infoTileMap->map.count_width_tile;
                        infoTileMap->layers = new util::BTILE_LAYER[infoTileMap->map.layerCount];

                        for (uint32_t  j = 0; j < infoTileMap->map.layerCount; ++j)
                        {
                            if (!fread(infoTileMap->layers[j].offset,  sizeof(infoTileMap->layers[j].offset),1,fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read layer offset from tile!");

                            infoTileMap->layers[j].lsIndexTiles = new util::BTILE_INDEX_TILE[tileCount];
                            if (!fread(infoTileMap->layers[j].lsIndexTiles,  sizeof(util::BTILE_INDEX_TILE) * tileCount,1,fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read indexed tile from  detail tile!");
                        }

                        util::BTILE_DETAIL_HEADER detailObjsProperty;
                        if (!fread(&detailObjsProperty, sizeof(util::BTILE_DETAIL_HEADER),1,fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");

                        for(uint32_t j= 0 ; j < detailObjsProperty.totalObj; ++j)
                        {
                            util::BTILE_OBJ_HEADER objHeader;
                            if (!fread(&objHeader, sizeof(util::BTILE_OBJ_HEADER), 1,fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");
                            auto* obj = new util::BTILE_OBJ(static_cast<util::BTILE_OBJ_TYPE>(objHeader.type));
                            infoTileMap->lsObj.push_back(obj);
                            if(objHeader.sizeName > 0)
                            {
                                obj->name.resize(objHeader.sizeName);
                                auto * name = const_cast<char*>(obj->name.data());
                                if (!fread(name, objHeader.sizeName,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read name at detail tile header!");
                            }
                            for(uint32_t k= 0 ; k < objHeader.sizePoints; ++k)
                            {
                                auto* point = new VEC2();
                                obj->lsPoints.push_back(point);
                                if (!fread(point, sizeof(VEC2),1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");
                            }
                        }

                        for(uint32_t j= 0 ; j < detailObjsProperty.totalProperties; ++j)
                        {
                            util::BTILE_PROPERTY_HEADER propertyHeader;
                            if (!fread(&propertyHeader, sizeof(util::BTILE_PROPERTY_HEADER),1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail property tile header!");
                            auto* property = new util::BTILE_PROPERTY(static_cast<util::BTILE_PROPERTY_TYPE>(propertyHeader.type));
                            infoTileMap->lsProperty.push_back(property);
                            if(propertyHeader.nameLength > 0)
                            {
                                property->name.resize(propertyHeader.nameLength);
                                auto * name = const_cast<char*>(property->name.data());
                                if (!fread(name,propertyHeader.nameLength,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property name at detail tile header!");
                            }
                            if(propertyHeader.valueLength > 0)
                            {
                                property->value.resize(propertyHeader.valueLength);
                                auto * value = const_cast<char*>(property->value.data());
                                if (!fread(value,propertyHeader.valueLength,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property value at detail tile header!");
                            }
                            if(propertyHeader.ownerLength > 0)
                            {
                                property->owner.resize(propertyHeader.ownerLength);
                                auto * owner = const_cast<char*>(property->owner.data());
                                if (!fread(owner,propertyHeader.ownerLength,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property owner at detail tile header!");
                            }
                        }

                        i += 1;
                    }
                    break;
                    default:
                    {
                        return log_util::onFailed(fp,__FILE__, __LINE__, "unknown type bounding box [%d] [%s]", detail.type,
                                                 fileNamePath);
                    }
                }
            }
        }
#if defined USE_DEPRECATED_2_MINOR
        else
        {
            // step 2: --------------------------------------------------------------------------------------------------
            switch (this->typeMe)
            {
                case util::TYPE_MESH_3D:
                {
                    util::DETAIL_MESH detail;
                    // 2.1 Cubos -- Todos os boundings
                    // --------------------------------------------------------------------
                    if (!fread(&detail, sizeof(util::DETAIL_MESH), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load bounding box [%s]", fileNamePath);
                    if (detail.totalBounding)
                    {
                        switch (detail.type) // 1: Bounding box. 2: Esferico. 3: Cube poligono .
                        {
                            case 1:
                            {
                                for (int i = 0; i < detail.totalBounding; ++i)
                                {
                                    auto base = new CUBE();
                                    this->infoPhysics.lsCube.push_back(base);
                                    if (!fread(base, sizeof(CUBE), 1, fp))
                                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]",
                                                                 fileNamePath);
                                    ;
                                }
                            }
                            break;
                            case 2:
                            {
                                for (int i = 0; i < detail.totalBounding; ++i)
                                {
                                    auto base = new SPHERE();
                                    this->infoPhysics.lsSphere.push_back(base);
                                    if (!fread(base, sizeof(SPHERE), 1, fp))
                                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]",
                                                                 fileNamePath);
                                }
                            }
                            break;
                            case 3:
                            {
                                for (int i = 0; i < detail.totalBounding; ++i)
                                {
                                    auto complex = new CUBE_COMPLEX();
                                    this->infoPhysics.lsCubeComplex.push_back(complex);
                                    if (!fread(complex, sizeof(CUBE_COMPLEX), 1, fp))
                                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]",
                                                                 fileNamePath);
                                    ;
                                }
                            }
                            break;
                            default: { return log_util::onFailed(fp,__FILE__, __LINE__, "bounding unknown [%s]", fileNamePath);
                            }
                        }
                    }
                }
                break;
                case util::TYPE_MESH_SPRITE:
                {
                    if (!deprectedInfoSprite.readBoundingSprite(fp, fileNamePath, &this->zoomEditorSprite.x,
                                                                &this->zoomEditorSprite.y))
                        return false;
                }
                break;
                case util::TYPE_MESH_USER:
                {
                    // special -- user
                }
                break;
                case util::TYPE_MESH_FONT:
                {
                    auto *   infoFont = new INFO_BOUND_FONT();
                    this->extraInfo = infoFont;
                    util::DETAIL_HEADER_FONT headerFont;
                    if (!fread(&headerFont, sizeof(util::DETAIL_HEADER_FONT), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_HEADER_FONT [%s]", fileNamePath);
                    auto strNameFont = new char[headerFont.sizeNameFonte];
                    if (!fread(strNameFont, static_cast<size_t>(headerFont.sizeNameFonte), 1, fp))
                    {
                        delete [] strNameFont;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load font's name [%s]", fileNamePath);
                    }
                    infoFont->fontName        = strNameFont;
                    infoFont->heightLetter    = headerFont.heightLetter;
                    infoFont->spaceXCharacter = headerFont.spaceXCharacter;
                    infoFont->spaceYCharacter = headerFont.spaceYCharacter;
                    delete[] strNameFont;
                    for (int i = 0; i < headerFont.totalDetailFont; ++i)
                    {
                        auto detailFont = new util::DETAIL_LETTER();
                        if (!fread(detailFont, sizeof(util::DETAIL_LETTER), 1, fp))
                        {
                            delete detailFont;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER [%s]", fileNamePath);
                        }
                        if (detailFont->indexFrame >= headerFont.totalDetailFont)
                        {
                            delete detailFont;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER!! frame out of bound [%s]",
                                                     fileNamePath);
                        }
                        infoFont->letter[detailFont->letter].detail = detailFont;
                    }
                }
                break;
                default: {
                }
            }
        }
#else
        else
        {
            return log_util::onFailed(fp,__FILE__, __LINE__, "Imcompatible version [%d]", headerMain.version);
        }
#endif
        // 3 headerMesh MBM -------------------------------------------------------------------------------
        if (!fread(&headerMesh, sizeof(util::HEADER_MESH), 1, fp))
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read HEADER_MESH [%s]", fileNamePath);
        if (headerMesh.totalAnimation == 0)
            return log_util::onFailed(fp,__FILE__, __LINE__, "there is no animation [%s]", fileNamePath);

        // 4 header anim -- Todas as animações -----------------------------------------------------------
        if (headerMain.version == INITIAL_VERSION_MBM_HEADER)
        {
#if defined USE_DEPRECATED_2_MINOR
            if (!deprecated_mbm::fillAnimation_1(fileNamePath, fp, &headerMesh, &this->infoAnimation))
                return false;
#else
            return log_util::onFailed(fp,__FILE__, __LINE__, "unexpected version [%s] Version [%d]", fileNamePath, headerMain.version);
#endif
        }
        else if (headerMain.version >= SPRITE_INFO_VERSION_MBM_HEADER)
        {
            if (!this->fillAnimation_2(fileNamePath, fp))
                return false;
        }
        else
        {
            return log_util::onFailed(fp,__FILE__, __LINE__, "unknown version [%s] Version [%d]", fileNamePath, headerMain.version);
        }
        if (this->headerMain.version == INITIAL_VERSION_MBM_HEADER)
            this->headerMain.version = SPRITE_INFO_VERSION_MBM_HEADER;
#if defined USE_DEPRECATED_2_MINOR
        if (this->headerMain.version <= SPRITE_INFO_VERSION_MBM_HEADER)
        {
            deprectedInfoSprite.fillOldPhysicsSprite_2(this->typeMe, this->infoAnimation, this->headerMesh.totalFrames);
        }
#endif
        // Loop principal atraves de todos os frames deste arquivo -----------------------------------------------
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            auto pBuffer = new util::BUFFER_MESH_DEBUG();
            this->buffer.push_back(pBuffer);
            // 5 Sequencia lógica dos frames --------------------------------------------------------------------------
            // Cada header Frame
            // --------------------------------------------------------------------------------------------------
            util::HEADER_FRAME *headerFrame = &pBuffer->headerFrame;
            if (!fread(headerFrame, sizeof(util::HEADER_FRAME), 1, fp))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header of frame [%s]", fileNamePath);

            // 6 Todos os headers subset deste frame
            // -------------------------------------------------------------------------------
            util::HEADER_DESC_SUBSET headerDescSubset;
            for (int i = 0; i < headerFrame->totalSubset; ++i)
            {
                auto pSubset = new util::SUBSET_DEBUG();
                pBuffer->subset.push_back(pSubset);
                if (!fread(&headerDescSubset, sizeof(util::HEADER_DESC_SUBSET), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header of subset [%s]", fileNamePath);
                pSubset->vertexStart = headerDescSubset.vertexStart;
                pSubset->indexStart  = headerDescSubset.indexStart;
                pSubset->indexCount  = headerDescSubset.indexCount;
                pSubset->vertexCount = headerDescSubset.vertexCount;
                if (strcmp(headerDescSubset.nameTexture, "default") != 0)
                {
                    char *pch = strchr(headerDescSubset.nameTexture, '#');
                    if (pch && pch[0] == '#' && pch[1] == 'u')
                    {
                        pch[0] = 0;
                        util::HEADER_IMG headerImg;
                        headerImg.lenght = 0;
                        if (!fread(&headerImg, sizeof(util::HEADER_IMG), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header image [%s]", fileNamePath);
                        if (headerImg.lenght == 0)
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header image [%s]", fileNamePath);
                        auto data = new uint8_t[headerImg.lenght];
                        if (!fread(data, static_cast<size_t>(headerImg.lenght), 1, fp))
                        {
                            delete [] data;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read image [%s]", fileNamePath);
                        }
                        uint32_t sizeOfImage = 0;
                        if (headerImg.channel != 4 && headerImg.channel != 3 && headerImg.channel != 0)
                        {
                            delete [] data;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read image! Channel != [3 || 4] [%s]",
                                                     fileNamePath);
                        }
                        headerImg.channel = headerImg.channel == 4 ? 4 : 3;
                        switch (headerImg.depth)
                        {
                            case 3:
                            {
                                sizeOfImage = 3 * headerImg.channel * headerImg.width * headerImg.height;
                                while (sizeOfImage % 8)
                                {
                                    sizeOfImage++;
                                }
                                sizeOfImage = sizeOfImage / 8;
                            }
                            break;
                            case 4:
                            {

                                sizeOfImage = 4 * headerImg.channel * headerImg.width * headerImg.height;
                                while (sizeOfImage % 8)
                                {
                                    sizeOfImage++;
                                }
                                sizeOfImage = sizeOfImage / 8;
                            }
                            break;
                            case 8: { sizeOfImage = headerImg.width * headerImg.height * headerImg.channel;}
                            break;
                            default: {delete [] data; return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read image [%s]", fileNamePath);}
                        }
                        MINIZ miniz;
                        if (miniz.decompressStream(data, headerImg.lenght, sizeOfImage))
                        {
                            delete[] data;
                            pSubset->texture = headerDescSubset.nameTexture;
                            if (!pSubset->texture.size())
                            {
#if defined _DEBUG
                                PRINT_IF_DEBUG( "error on create texture: %s \n Linha %d",
                                             fileName.c_str());
#endif
                            }
                        }
                        else
                        {
                            delete[] data;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to uncompress file [%s]", fileNamePath);
                        }
                    }
                    else
                    {
                        pch = strchr(headerDescSubset.nameTexture, '#');
                        if (pch && pch[0] == '#' && pch[1] == 'M') // Material apenas .. cor como string
                        {
                            pch = &pch[2];
                            //util::MATERIAL mat;
                            //memset(&mat, 0, sizeof(mat));
                            std::vector<std::string> result;
                            util::split(result, pch, '|');

                            if (result.size() == 5)
                            {
                                COLOR colorAculm(0.0f, 0.0f, 0.0f, 0.0f);
                                int        totalSum = 0;
                                for (auto & n : result)
                                {
                                    const char *strTemp   = n.c_str();
                                    //char        letter    = strTemp[0];
                                    const char *strNumber = &strTemp[1];
                                    COLOR  color     = static_cast<uint32_t>(strtol(strNumber, nullptr, 16));
                                    if (color.r > 0.0f || color.g > 0.0f || color.b > 0.0f)
                                    {
                                        colorAculm.r += color.r;
                                        colorAculm.g += color.g;
                                        colorAculm.b += color.b;
                                        totalSum++;
                                    }
                                    //switch (letter)
                                    //{
                                    //    case 'A': { mat.Ambient = color;
                                    //    }
                                    //    break;
                                    //    case 'D': { mat.Diffuse = color;
                                    //    }
                                    //    break;
                                    //    case 'E': { mat.Emissive = color;
                                    //    }
                                    //    break;
                                    //    case 'S': { mat.Specular = color;
                                    //    }
                                    //    break;
                                    //    case 'P': { mat.Power = static_cast<float>(atof(strNumber));}
                                    //    break;
                                    //    default: break;
                                    //}
                                }
                                if (totalSum)
                                { // potências de 2 (2,4,8,16,32,64,128,256, 512,1024)...
                                    /*colorAculm.r  =   colorAculm.r / (float)totalSum;
                                    colorAculm.g    =   colorAculm.g / (float)totalSum;
                                    colorAculm.b    =   colorAculm.b / (float)totalSum;
                                    uint8_t dataARGB[16];
                                    uint32_t dwR = colorAculm.r >= 1.0f ? 0xff : colorAculm.r <= 0.0f ? 0x00 :
                                    (uint32_t) (colorAculm.r * 255.0f + 0.5f);
                                    uint32_t dwG = colorAculm.g >= 1.0f ? 0xff : colorAculm.g <= 0.0f ? 0x00 :
                                    (uint32_t) (colorAculm.g * 255.0f + 0.5f);
                                    uint32_t dwB = colorAculm.b >= 1.0f ? 0xff : colorAculm.b <= 0.0f ? 0x00 :
                                    (uint32_t) (colorAculm.b * 255.0f + 0.5f);
                                    for(int pixel=0; pixel< 12; pixel+=4)
                                    {
                                        dataARGB[pixel]     =   (uint8_t)255;
                                        dataARGB[pixel+1]   =   (uint8_t)dwR;
                                        dataARGB[pixel+2]   =   (uint8_t)dwG;
                                        dataARGB[pixel+3]   =   (uint8_t)dwB;
                                    }*/
                                    pSubset->texture = headerDescSubset.nameTexture;
                                }
                            }
                            else
                            {
                                pSubset->texture.clear();
                            }
                        }
                        else if (pch && pch[0] == '#')//solid color  as texture
                        {
                            const char * nickName = pch;
                            mbm::TEXTURE_MANAGER * man = mbm::TEXTURE_MANAGER::getInstance();
                            if(man->existTexture(nickName) == false)
                            {
                                COLOR color;
                                const char* sColor = &pch[1]; 
                                int len = strlen(sColor);
                                if (len == 8)
                                {
                                    char alpha[3] = {0,0,0};
                                    alpha[0] = *sColor;
                                    sColor++;
                                    alpha[1] = *sColor;
                                    sColor++;
                                    const int n = strtol(sColor,nullptr, 16);
                                    color = COLOR(n);
                                    color.a = strtol(alpha, nullptr, 16) * 1.0f / 255.0f;
                                }
                                else if (len == 6)
                                {
                                    const int n = strtol(sColor, nullptr, 16);
                                    color = COLOR(n);
                                    color.a = 1.0f;
                                }
                                const uint32_t width = 4;
                                const uint32_t height = 4;
                                uint8_t pixel[4 * 4* 3];
                                uint8_t r = 0;
                                uint8_t g = 0;
                                uint8_t b = 0;
                                color.get(&r,&g,&b);
                                for (uint32_t j = 0; j < 4 * 4 * 3; j += 3)
                                {
                                    pixel[j] = r;
                                    pixel[j+1] = g;
                                    pixel[j+2] = b;
                                }
                            
                                TEXTURE *solidTexture = man->load(width, height, &pixel[0], nickName, 8,3);
                                if(solidTexture == nullptr)
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed on create solid texture [%s][%s]",nickName, fileNamePath);
                            }
                            pSubset->texture = headerDescSubset.nameTexture;
                        }
                        else
                        {
                            pSubset->texture = headerDescSubset.nameTexture;
                        }
                    }
                }
            }

            // 6.2 Vertex buffer e index buffer
            // -----------------------------------------------------------------------------------------
            if (headerFrame->sizeIndexBuffer && strcmp(headerFrame->typeBuffer, "IB") == 0)
            {
                auto indexArray = new uint16_t[headerFrame->sizeIndexBuffer];
                if (!fread(indexArray, sizeof(uint16_t) * static_cast<size_t>(headerFrame->sizeIndexBuffer), 1, fp))
                {
                    delete[] indexArray;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read index buffer of frame [%s]", fileNamePath);
                }
                VEC3 *pPosition = nullptr;
                VEC3 *pNormal   = nullptr;
                VEC2 *pTexture  = nullptr;
                if (!this->loadFromSplited(fp, headerFrame->sizeVertexBuffer, &pPosition, &pNormal, &pTexture,
                                           headerMesh.hasNorText, indexArray, headerFrame->sizeIndexBuffer,
                                           headerFrame->stride))
                {
                    delete[] indexArray;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex buffer of frame [%s]", fileNamePath);
                }
                pBuffer->position    = reinterpret_cast<float *>(pPosition);
                pBuffer->normal      = reinterpret_cast<float *>(pNormal);
                pBuffer->uv          = reinterpret_cast<float *>(pTexture);
                pBuffer->indexBuffer = indexArray;
#if defined USE_DEPRECATED_2_MINOR
                if (headerMain.version <= SPRITE_INFO_VERSION_MBM_HEADER)
                    deprectedInfoSprite.fillPhysicsSprite(pPosition, static_cast<uint32_t>(currentFrame), pBuffer->subset, this->typeMe,
                                                          this->infoPhysics.lsCube, this->infoPhysics.lsSphere,
                                                          this->infoPhysics.lsTriangle);
#endif
            }
            // 6.3 Vertex Buffer somente
            // ----------------------------------------------------------------------------------------------
            else if (strcmp(headerFrame->typeBuffer, "VB") == 0)
            {
                VEC3 *pPosition = nullptr;
                VEC3 *pNormal   = nullptr;
                VEC2 *pTexture  = nullptr;
                if (!this->loadFromSplited(fp, headerFrame->sizeVertexBuffer, &pPosition, &pNormal, &pTexture,
                                           headerMesh.hasNorText, nullptr, 0, headerFrame->stride))
                {
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex buffer of frame [%s]", fileNamePath);
                }

                pBuffer->position = reinterpret_cast<float *>(pPosition);
                pBuffer->normal   = reinterpret_cast<float *>(pNormal);
                pBuffer->uv       = reinterpret_cast<float *>(pTexture);
#if defined USE_DEPRECATED_2_MINOR
                if (headerMain.version <= SPRITE_INFO_VERSION_MBM_HEADER)
                    deprectedInfoSprite.fillPhysicsSprite(pPosition, static_cast<uint32_t>(currentFrame), pBuffer->subset, this->typeMe,
                                                          this->infoPhysics.lsCube, this->infoPhysics.lsSphere,
                                                          this->infoPhysics.lsTriangle);
#endif
            }
            else
            {
                return log_util::onFailed(fp,__FILE__, __LINE__, "unknown buffer type [%s]", fileNamePath);
            }
        }
        fclose(fp);
        fp             = nullptr;
        positionOffset = VEC3(headerMesh.posX, headerMesh.posY, headerMesh.posZ);
        angleDefault   = VEC3(headerMesh.angleX, headerMesh.angleY, headerMesh.angleZ);
        remove(util::getDecompressModelFileName());

        this->sizeCoordTexFrame_0 = 0;
        if (this->coordTexFrame_0)
            delete[] this->coordTexFrame_0;
        this->coordTexFrame_0 = nullptr;
        return true;
    }
    
    bool MESH_MBM_DEBUG::check(char *error,const int lenError)
    {
        if (this->buffer.size() == 0)
        {
            if (error)
                strncpy(error, "Empty buffer",lenError);
            return false;
        }
        for (std::vector<util::BUFFER_MESH_DEBUG *>::size_type i = 0; i < this->buffer.size(); ++i)
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->buffer[i];
            const std::vector<util::SUBSET_DEBUG *>::size_type s = bufferCurrent->subset.size();
            if (s == 0)
            {
                if (error)
                    snprintf(error,lenError, "Empty subset at frame [%d]",static_cast<int>(i));
                return false;
            }
            for (std::vector<util::SUBSET_DEBUG *>::size_type j = 0; j < s; ++j)
            {
                util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[j];
                if (pTmpSubset->vertexCount == 0)
                {
                    if (error)
                        snprintf(error,lenError, "vertex count [0] in subset [%d] at frame [%d]", static_cast<int>(j),static_cast<int>(i));
                    return false;
                }
                if (pTmpSubset->indexCount > 0 && bufferCurrent->indexBuffer == nullptr)
                {
                    pTmpSubset->indexCount = 0;
                }
                if (bufferCurrent->indexBuffer)
                {
                    if (pTmpSubset->indexCount == 0)
                    {
                        if (error)
                            snprintf(error,lenError,"there is index in buffer but 'index count' has [0] in subset [%d] at frame [%d]", static_cast<int>(j),static_cast<int>(i));
                        return false;
                    }
                }
            }
        }

        for (uint32_t i = 0; i < this->buffer.size(); ++i)
        {
            int                      iTotalVertex  = 0;
            int                      iTotalIndex   = 0;
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->buffer[i];
            const std::vector<util::BUFFER_MESH_DEBUG *>::size_type s = bufferCurrent->subset.size();
            for (std::vector<util::BUFFER_MESH_DEBUG *>::size_type j = 0; j < s; ++j)
            {
                util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[j];
                iTotalVertex += pTmpSubset->vertexCount;
                iTotalIndex += pTmpSubset->indexCount;
            }
            if (bufferCurrent->indexBuffer)
            {
                int rest = iTotalIndex % 3;
                if (rest)
                {
                    if((iTotalIndex - rest) >=3)
                    {
                        util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[s-1];
                        pTmpSubset->indexCount -= rest;
                        PRINT_IF_DEBUG("index buffer must be dividible by 3 (mode triangle list indexed)\nindex total [%d]\nDoing work around to work",iTotalIndex);
                    }
                    else
                    {
                        if (error)
                        sprintf(error,"index buffer must be dividible by 3 (mode triangle list indexed)\nindex total [%d]",iTotalIndex);
                        return false;
                    }
                }
            }
            else
            {
                int rest = iTotalVertex % 3;
                if (rest)
                {
                    if((iTotalVertex - rest) >=3)
                    {
                        util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[s-1];
                        pTmpSubset->vertexCount-= rest;
                        PRINT_IF_DEBUG("vertex buffer must be dividible by 3 (mode triangle list indexed)\nvertex total [%d]\n Doing work around to work",iTotalVertex);
                    }
                    else
                    {
                        if (error)
                            sprintf(error,"vertex buffer must be dividible by 3 (mode triangle list indexed)\nvertex total [%d]",iTotalVertex);
                        return false;
                    }
                }
            }
            for (uint32_t j = 0; j < s; ++j)
            {
                util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[j];
                if ((pTmpSubset->vertexStart + pTmpSubset->vertexCount) > iTotalVertex)
                {
                    if (error)
                        sprintf(error, "vertex start [%d] + vertex count [%d] = [%d] > total vertex [%d] in subset [%u] "
                                       "at frame [%u]",
                                pTmpSubset->vertexStart, pTmpSubset->vertexCount,
                                pTmpSubset->vertexStart + pTmpSubset->vertexCount, iTotalVertex, j, i);
                    return false;
                }
                if ((pTmpSubset->indexStart + pTmpSubset->indexCount) > iTotalIndex)
                {
                    if (error)
                        sprintf(
                            error,
                            "index start [%d] + index count [%d] = [%d] > total index [%d] in subset [%u] at frame [%u]",
                            pTmpSubset->indexStart, pTmpSubset->indexCount,
                            pTmpSubset->indexStart + pTmpSubset->indexCount, iTotalIndex, j, i);
                    return false;
                }
            }
            if (bufferCurrent->indexBuffer)
            {
                for (uint32_t j = 0; j < s; ++j)
                {
                    util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[j];
                    if (pTmpSubset->indexCount == 0)
                    {
                        if (error)
                            sprintf(error, "index length is [0] in subset [%u] at frame [%u]", j, i);
                        return false;
                    }
                    for (int k = 0; k < pTmpSubset->indexCount; ++k)
                    {
                        uint16_t index = bufferCurrent->indexBuffer[k];
                        if (index > iTotalVertex)
                        {
                            if (error)
                                sprintf(error, "index [%d] value [%u] invalid in subset [%u] at frame [%u]", k, index, j,
                                        i);
                            return false;
                        }
                    }
                }
            }
        }
        return true;
    }
    
    void MESH_MBM_DEBUG::centralizeFrame(const int indexFrame, const int indexSubset)
    {
        if (indexFrame < 0) //-1
        {
            for (uint32_t i = 0; i < this->buffer.size(); ++i)
            {
                centralizeFrame(static_cast<int>(i), indexSubset);
            }
        }
        else if (indexFrame < static_cast<int>(this->buffer.size()))
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->buffer[static_cast<std::vector<util::BUFFER_MESH_DEBUG*>::size_type>(indexFrame)];
            auto *                   pPosition     = reinterpret_cast<VEC3 *>(bufferCurrent->position);
            const auto       s             = static_cast<uint32_t>(bufferCurrent->subset.size());
            VEC3                     maxSize(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            VEC3                     minSize(FLT_MAX, FLT_MAX, FLT_MAX);
            if (indexSubset < 0)
            {
                for (uint32_t i = 0; i < s; ++i)
                {
                    const util::SUBSET_DEBUG * pTmpSubset = bufferCurrent->subset[static_cast<std::vector<util::SUBSET_DEBUG *>::size_type>(i)];
                    const auto        n          = static_cast<uint32_t>(pTmpSubset->vertexStart + pTmpSubset->vertexCount);
                    for (auto j = static_cast<uint32_t>(pTmpSubset->vertexStart); j < n; ++j)
                    {
                        VEC3 *pos = &pPosition[j];
                        if (pos->x < minSize.x)
                            minSize.x = pos->x;
                        if (pos->y < minSize.y)
                            minSize.y = pos->y;
                        if (pos->z < minSize.z)
                            minSize.z = pos->z;

                        if (pos->x > maxSize.x)
                            maxSize.x = pos->x;
                        if (pos->y > maxSize.y)
                            maxSize.y = pos->y;
                        if (pos->z > maxSize.z)
                            maxSize.z = pos->z;
                    }
                }
            }
            else if (indexSubset < static_cast<int>(s))
            {
                const util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[static_cast<std::vector<util::SUBSET_DEBUG *>::size_type>(indexSubset)];
                const auto        n          = static_cast<uint32_t>(pTmpSubset->vertexStart + pTmpSubset->vertexCount);
                for (auto j = static_cast<uint32_t>(pTmpSubset->vertexStart); j < n; ++j)
                {
                    VEC3 *pos = &pPosition[j];
                    if (pos->x < minSize.x)
                        minSize.x = pos->x;
                    if (pos->y < minSize.y)
                        minSize.y = pos->y;
                    if (pos->z < minSize.z)
                        minSize.z = pos->z;

                    if (pos->x > maxSize.x)
                        maxSize.x = pos->x;
                    if (pos->y > maxSize.y)
                        maxSize.y = pos->y;
                    if (pos->z > maxSize.z)
                        maxSize.z = pos->z;
                }
            }
            else
            {
                return;
            }
            VEC3 dist(maxSize - minSize);
            // O Seguinte calculo inibe uns pontos perdidos
            float xDif = maxSize.x < 0.0f ? -maxSize.x : maxSize.x;
            float yDif = maxSize.y < 0.0f ? -maxSize.y : maxSize.y;
            float zDif = maxSize.z < 0.0f ? -maxSize.z : maxSize.z;

            float xDiff = minSize.x < 0.0f ? -minSize.x : minSize.x;
            float yDiff = minSize.y < 0.0f ? -minSize.y : minSize.y;
            float zDiff = minSize.z < 0.0f ? -minSize.z : minSize.z;

            float xMin = xDiff < xDif ? xDiff : xDif;
            float xMax = xDiff > xDif ? xDiff : xDif;

            float yMin = yDiff < yDif ? yDiff : yDif;
            float yMax = yDiff > yDif ? yDiff : yDif;

            float zMin = zDiff < zDif ? zDiff : zDif;
            float zMax = zDiff > zDif ? zDiff : zDif;

            float xDiv = xMin / xMax;
            float yDiv = yMin / yMax;
            float zDiv = zMin / zMax;
            if (xDiv < 0.001f)
            {
                dist.x    = xMin;
                minSize.x = 0;
            }
            if (yDiv < 0.001f)
            {
                dist.y    = yMin;
                minSize.y = 0;
            }
            if (zDiv < 0.001f)
            {
                dist.z    = zMin;
                minSize.z = 0;
            }
            const VEC3 middle(dist.x * 0.5f, dist.y * 0.5f, dist.z * 0.5f);
            const VEC3 offset(minSize.x + middle.x, minSize.y + middle.y, minSize.z + middle.z);
            if (indexSubset < 0)
            {
                for (uint32_t i = 0; i < s; ++i)
                {
                    util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[i];
                    const auto  n          = static_cast<uint32_t >(pTmpSubset->vertexStart + pTmpSubset->vertexCount);
                    for (auto j = static_cast<uint32_t >(pTmpSubset->vertexStart); j < n; ++j)
                    {
                        VEC3 *pos = &pPosition[j];
                        pos->x -= offset.x;
                        pos->y -= offset.y;
                        pos->z -= offset.z;
                    }
                }
            }
            else if (indexSubset < static_cast<int>(s))
            {
                util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[static_cast<std::vector<util::SUBSET_DEBUG *>::size_type>(indexSubset)];
                const int  n          = pTmpSubset->vertexStart + pTmpSubset->vertexCount;
                for (int j = pTmpSubset->vertexStart; j < n; ++j)
                {
                    VEC3 *pos = &pPosition[j];
                    pos->x -= offset.x;
                    pos->y -= offset.y;
                    pos->z -= offset.z;
                }
            }
        }
    }
    
    bool MESH_MBM_DEBUG::addIndex(const uint32_t indexFrame, const uint32_t indexSubset,
                        const uint16_t *newIndexPart, const uint32_t sizeArrayNewIndexPart,
                        char *strErrorOut)
    {
        if (indexFrame < this->buffer.size() && indexSubset < this->buffer[indexFrame]->subset.size())
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = this->buffer[indexFrame];
            util::SUBSET_DEBUG *     pSubset       = bufferCurrent->subset[indexSubset];
            if (pSubset->vertexCount == 0)
            {
                if (strErrorOut)
                    sprintf(strErrorOut, "vertex count is zero [0] to subset [%u] at frame [%u]\nBefore set index you "
                                         "must set the vertex.",
                            indexSubset, indexFrame);
                return false;
            }
            for (uint32_t i = 0; i < sizeArrayNewIndexPart; ++i)
            {
                const uint16_t index = newIndexPart[i];
                if (index >= pSubset->vertexCount)
                {
                    if (strErrorOut)
                        sprintf(strErrorOut, "index [%u] value [%u] out of bound. max vertex [%d] for this subset", i,
                                index, pSubset->vertexCount);
                    return false;
                }
            }
            const std::vector<util::SUBSET_DEBUG *>::size_type  sizeSubset       = bufferCurrent->subset.size();
            uint32_t        indexCountTotal  = 0;
            uint32_t        indexCountAfter  = 0;
            uint32_t        indexCountBefore = 0;
            uint16_t *oldIndex         = bufferCurrent->indexBuffer;
            auto        oldSizeIndex     = static_cast<uint32_t>(pSubset->indexCount);
            for (uint32_t i = 0; i < sizeSubset; ++i)
            {
                util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[i];
                indexCountTotal += static_cast<uint32_t>(pTmpSubset->indexCount);
                if (i < indexSubset)
                {
                    indexCountBefore += static_cast<uint32_t>(pTmpSubset->indexCount);
                }
                if (i > indexSubset)
                {
                    indexCountAfter += static_cast<uint32_t>(pTmpSubset->indexCount);
                }
            }

            const uint32_t newSizeIndex = indexCountTotal - oldSizeIndex + sizeArrayNewIndexPart;

            if (oldIndex)
            {
                auto newIndex   = new unsigned short[newSizeIndex];
                bufferCurrent->indexBuffer = newIndex;
                if (indexCountBefore)
                {
                    memcpy(newIndex, oldIndex, sizeof(unsigned short) * static_cast<size_t>(indexCountBefore));
                }
                memcpy(&newIndex[indexCountBefore], newIndexPart, sizeof(unsigned short) * static_cast<size_t>(sizeArrayNewIndexPart));
                if (indexCountAfter)
                {
                    uint32_t s = indexCountBefore + sizeArrayNewIndexPart;
                    memcpy(&newIndex[s], oldIndex, sizeof(unsigned short) * static_cast<size_t>(indexCountAfter));
                }
                int diff = static_cast<int>(oldSizeIndex) - static_cast<int>(sizeArrayNewIndexPart);
                for (uint32_t i = (indexSubset + 1); i < sizeSubset; ++i)
                {
                    util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[std::vector<util::SUBSET_DEBUG *>::size_type(i)];
                    pTmpSubset->indexStart += diff;
                }

                for (int i = pSubset->indexStart; i < (pSubset->indexStart + static_cast<int>(sizeArrayNewIndexPart)); ++i)
                {
                    newIndex[i] +=  static_cast<unsigned short>(pSubset->vertexStart);
                }
                pSubset->indexCount = static_cast<int>(sizeArrayNewIndexPart);
                delete[] oldIndex;
            }
            else
            {
                pSubset->indexStart        = static_cast<int>(indexCountBefore);
                pSubset->indexCount        = static_cast<int>(sizeArrayNewIndexPart);
                bufferCurrent->indexBuffer = new uint16_t[newSizeIndex];
                memcpy(bufferCurrent->indexBuffer, newIndexPart, sizeof(uint16_t) * static_cast<size_t>(sizeArrayNewIndexPart));
                int diff = static_cast<int>(oldSizeIndex) - static_cast<int>(sizeArrayNewIndexPart);
                for (uint32_t i = (indexSubset + 1); i < sizeSubset; ++i)
                {
                    util::SUBSET_DEBUG *pTmpSubset = bufferCurrent->subset[i];
                    pTmpSubset->indexStart += diff;
                }
                for (int i = pSubset->indexStart; i < (pSubset->indexStart + static_cast<int>(sizeArrayNewIndexPart)); ++i)
                {
                    bufferCurrent->indexBuffer[i] += static_cast<unsigned short>(pSubset->vertexStart);
                }
            }
            // update
            uint32_t lastCountIndex = 0;

            for (auto & i : bufferCurrent->subset)
            {
                pSubset             = i;
                pSubset->indexStart = static_cast<int>(lastCountIndex);
                lastCountIndex += static_cast<uint32_t>(pSubset->indexCount);
            }
            return true;
        }
        else
        {
            if (strErrorOut)
            {
                const auto tSubset = static_cast<int>(indexFrame < this->buffer.size() ? this->buffer[indexFrame]->subset.size() : 0);
                sprintf(strErrorOut, "Out of bound[indexFrame(total %u),indexSubset(total %d)\n"
                                     "indexFrame %u indexSubset %u",
                        static_cast<uint32_t>(this->buffer.size()), tSubset, indexFrame, indexSubset);
            }
            return false;
        }
    }
    
    bool MESH_MBM_DEBUG::addVertex(const uint32_t indexFrame, const uint32_t indexSubset, const uint32_t totalVertex)
    {
        if (indexFrame < this->buffer.size() && indexSubset < this->buffer[indexFrame]->subset.size())
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent    = this->buffer[indexFrame];
            util::SUBSET_DEBUG *     pSubset          = nullptr;
            unsigned  int            vertexCountTotal = 0;
            unsigned  int            vertexEndSubset  = 0;

            for (std::vector<util::SUBSET_DEBUG *>::size_type i = 0; i < bufferCurrent->subset.size(); ++i)
            {
                pSubset = bufferCurrent->subset[i];
                vertexCountTotal += static_cast<uint32_t>(pSubset->vertexCount);
                if (i <= indexSubset)
                {
                    vertexEndSubset += static_cast<uint32_t>(pSubset->vertexCount);
                }
            }
            pSubset = bufferCurrent->subset[indexSubset];
            if (pSubset->indexCount)
            {
                PRINT_IF_DEBUG( "Warning! you are adding vertex to a subset [%d] that has index at "
                                                 "frame [%d]\n the index will be deleted.");
                if (bufferCurrent->indexBuffer)
                {
                    delete[] bufferCurrent->indexBuffer;
                    bufferCurrent->indexBuffer = nullptr;
                }
                for (auto & i : bufferCurrent->subset)
                {
                    pSubset             = i;
                    pSubset->indexCount = 0;
                    pSubset->indexStart = 0;
                }
            }
            auto *oldPosition = reinterpret_cast<VEC3 *>(bufferCurrent->position);
            auto *oldNormal   = reinterpret_cast<VEC3 *>(bufferCurrent->normal);
            auto *oldUv       = reinterpret_cast<VEC2 *>(bufferCurrent->uv);

            auto newPosition = new VEC3[vertexCountTotal + totalVertex];
            auto newNormal   = new VEC3[vertexCountTotal + totalVertex];
            auto newUv       = new VEC2[vertexCountTotal + totalVertex];

            if (vertexEndSubset)
            {
                if (oldPosition)
                    memcpy(static_cast<void*>(newPosition), static_cast<void*>(oldPosition), sizeof(VEC3) * static_cast<size_t>(vertexEndSubset));
                if (oldNormal)
                    memcpy(static_cast<void*>(newNormal), static_cast<void*>(oldNormal), sizeof(VEC3) * static_cast<size_t>(vertexEndSubset));
                if (oldUv)
                    memcpy(static_cast<void*>(newUv), static_cast<void*>(oldUv), sizeof(VEC2) * static_cast<size_t>(vertexEndSubset));
            }
            memset(static_cast<void*>(&newPosition[vertexEndSubset]), 0, sizeof(VEC3) * static_cast<size_t>(totalVertex)); // new vertex comes 0.0f
            memset(static_cast<void*>(&newNormal[vertexEndSubset]), 0, sizeof(VEC3) * static_cast<size_t>(totalVertex));   // new vertex comes 0.0f
            memset(static_cast<void*>(&newUv[vertexEndSubset]), 0, sizeof(VEC2) * static_cast<size_t>(totalVertex));       // new vertex comes 0.0f
            uint32_t lenLastVertex = vertexCountTotal - (vertexEndSubset);
            if (lenLastVertex > 0)
            {
                memcpy(static_cast<void*>(&newPosition[vertexEndSubset + totalVertex]), &oldPosition[vertexEndSubset],sizeof(VEC3) * static_cast<size_t>(lenLastVertex));
                memcpy(static_cast<void*>(&newNormal[vertexEndSubset + totalVertex]), &oldNormal[vertexEndSubset],sizeof(VEC3) * static_cast<size_t>(lenLastVertex));
                memcpy(static_cast<void*>(&newUv[vertexEndSubset + totalVertex]), &oldUv[vertexEndSubset], sizeof(VEC2) * static_cast<size_t>(lenLastVertex));
            }

            bufferCurrent->position = reinterpret_cast<float *>(newPosition);
            bufferCurrent->normal   = reinterpret_cast<float *>(newNormal);
            bufferCurrent->uv       = reinterpret_cast<float *>(newUv);

            if (oldPosition)
                delete[] oldPosition;
            if (oldNormal)
                delete[] oldNormal;
            if (oldUv)
                delete[] oldUv;

            pSubset->vertexCount += totalVertex;
            // update
            uint32_t lastCountVertex = 0;
            for (auto & i : bufferCurrent->subset)
            {
                pSubset              = i;
                pSubset->vertexStart = static_cast<int>(lastCountVertex);
                lastCountVertex += static_cast<uint32_t>(pSubset->vertexCount);
            }
            return true;
        }
        return false;
    }
    
    int MESH_MBM_DEBUG::addAnimation(const char *nameAnimation, const int initialFrame, const int finalFrame,
                           const float timeBetweenFrame, const int typeAnimation, char *errorOut)
    {
        if (this->buffer.size() == 0)
        {
            if (errorOut)
                sprintf(errorOut, "there is no frame ");
            return 0;
        }
        if (initialFrame < 0 || initialFrame >= static_cast<int>(this->buffer.size()))
        {
            if (errorOut)
                sprintf(errorOut, "initial frame [%d] out of range ->[%d]", initialFrame, static_cast<int>(this->buffer.size()));
            return 0;
        }
        if (finalFrame < 0 || finalFrame >= static_cast<int>(this->buffer.size()))
        {
            if (errorOut)
                sprintf(errorOut, "final frame [%d] out of range ->[%d]", finalFrame, static_cast<int>(this->buffer.size()));
            return 0;
        }
        if (typeAnimation < 0 || typeAnimation > 6)
        {
            if (errorOut)
                sprintf(errorOut, "type of animation [%d] out of range ->[0-6]", typeAnimation);
            return 0;
        }
        auto infoHead = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
        this->infoAnimation.lsHeaderAnim.push_back(infoHead);
        headerMesh.totalAnimation = static_cast<int>(this->infoAnimation.lsHeaderAnim.size());
        infoHead->headerAnim     = new util::HEADER_ANIMATION();
        if (nameAnimation)
            strncpy(infoHead->headerAnim->nameAnimation, nameAnimation, sizeof(infoHead->headerAnim->nameAnimation) - 1);
        else
            strncpy(infoHead->headerAnim->nameAnimation, "default",sizeof(infoHead->headerAnim->nameAnimation) - 1);
        infoHead->headerAnim->initialFrame     = initialFrame;
        infoHead->headerAnim->finalFrame       = finalFrame;
        infoHead->headerAnim->timeBetweenFrame = timeBetweenFrame <= 0.0f ? 0.0f : timeBetweenFrame;
        infoHead->headerAnim->typeAnimation    = typeAnimation;
        return headerMesh.totalAnimation;
    }

    bool MESH_MBM_DEBUG::updateAnimation(const uint32_t index, const char *nameAnimation, const int initialFrame, const int finalFrame,
                           const float timeBetweenFrame, const int typeAnimation, char *errorOut,const int lenError)
    {
        if (this->buffer.size() == 0)
        {
            if (errorOut)
                snprintf(errorOut,lenError, "there is no frame ");
            return false;
        }
        if(index >= this->infoAnimation.lsHeaderAnim.size())
        {
            if (errorOut)
                snprintf(errorOut, lenError,"index animation out of range. Total anim -> [%d] index -> [%d] ",static_cast<int>(this->infoAnimation.lsHeaderAnim.size()),static_cast<int>(index));
            return false;
        }
        if (initialFrame < 0 || initialFrame >= static_cast<int>(this->buffer.size()))
        {
            if (errorOut)
                snprintf(errorOut, lenError,"initial frame [%d] out of range ->[%d]", initialFrame, static_cast<int>(this->buffer.size()));
            return false;
        }
        if (finalFrame < 0 || finalFrame >= static_cast<int>(this->buffer.size()))
        {
            if (errorOut)
                snprintf(errorOut,lenError, "final frame [%d] out of range ->[%d]", finalFrame, static_cast<int>(this->buffer.size()));
            return false;
        }
        if (typeAnimation < 0 || typeAnimation > 6)
        {
            if (errorOut)
                snprintf(errorOut,lenError, "type of animation [%d] out of range ->[0-6]", typeAnimation);
            return false;
        }
        util::INFO_ANIMATION::INFO_HEADER_ANIM *infoHead = this->infoAnimation.lsHeaderAnim[index];
        if(infoHead->headerAnim == nullptr)
        {
            if (errorOut)
                strncpy(errorOut, "headerAnim null",lenError);
            return false;
        }
        if (nameAnimation)
            strncpy(infoHead->headerAnim->nameAnimation, nameAnimation, sizeof(infoHead->headerAnim->nameAnimation) - 1);
        else
            strncpy(infoHead->headerAnim->nameAnimation, "default",sizeof(infoHead->headerAnim->nameAnimation)-1);
        infoHead->headerAnim->initialFrame     = initialFrame;
        infoHead->headerAnim->finalFrame       = finalFrame;
        infoHead->headerAnim->timeBetweenFrame = timeBetweenFrame <= 0.0f ? 0.0f : timeBetweenFrame;
        infoHead->headerAnim->typeAnimation    = typeAnimation;
        return true;
    }

    const util::INFO_ANIMATION::INFO_HEADER_ANIM *MESH_MBM_DEBUG::getAnim(const uint32_t index)const
    {
        if(index < this->infoAnimation.lsHeaderAnim.size())
            return this->infoAnimation.lsHeaderAnim[index];
        return nullptr;
    }

    void MESH_MBM_DEBUG::deleteExtraInfo()
    {
        switch(typeMe)
        {
            case util::TYPE_MESH_FONT:
            {
                auto* infoFont = static_cast<mbm::INFO_BOUND_FONT*>(extraInfo);
                if(infoFont)
                    delete infoFont;
            }
            break;
            case util::TYPE_MESH_PARTICLE:
            {
                auto* lsParticleInfo = static_cast<std::vector<util::STAGE_PARTICLE*>*>(extraInfo);
                if(lsParticleInfo)
                {
                    for (auto stage : *lsParticleInfo)
                    {
                        delete stage;
                    }
                    lsParticleInfo->clear();
                    delete lsParticleInfo;
                }
            }
            break;
            case util::TYPE_MESH_TILE_MAP:
            {
                auto* infoTileMap = static_cast<util::BTILE_INFO*>(extraInfo);
                if(infoTileMap)
                    delete infoTileMap;
            }
            break;
                        case util::TYPE_MESH_SHAPE:
            {
                auto* infoShape = static_cast<util::DYNAMIC_SHAPE*>(extraInfo);
                if(infoShape)
                    delete infoShape;
            }
            break;
            default:
            {
                if (extraInfo)
                {
                    auto * charExtraInfo = static_cast<char*>(extraInfo);
                    delete[] charExtraInfo;
                }
            }
        }
        extraInfo           = nullptr;
    }
    
    void MESH_MBM_DEBUG::fixDefaultBoud()
    {
        if (this->infoPhysics.lsCube.size() == 0)
        {
            this->fillAtLeastOneBound();
            headerMesh.deprecated_typePhysics = 1;
        }
    }
    
    void MESH_MBM_DEBUG::release()
    {
        deleteExtraInfo();
        if (this->coordTexFrame_0)
            delete[] this->coordTexFrame_0;
        this->coordTexFrame_0 = nullptr;
        
        for (auto meshBuffer : this->buffer)
        {
            if (meshBuffer)
                delete meshBuffer;
            meshBuffer = nullptr;
        }
        buffer.clear();
        angleDefault        = VEC3(0, 0, 0);
        positionOffset      = VEC3(0, 0, 0);
        sizeCoordTexFrame_0 = 0;
        typeMe              = util::TYPE_MESH_UNKNOWN;
        memset(static_cast<void*>(&this->headerMain), 0, sizeof(this->headerMain));
        memset(static_cast<void*>(&this->headerMesh), 0, sizeof(this->headerMesh));
        zoomEditorSprite.x = 1.0f;
        zoomEditorSprite.y = 1.0f;
        util::MATERIAL_GLES m;
        this->headerMesh.material      = m;
        this->headerMesh.hasNorText[0] = 0;
        this->headerMesh.hasNorText[1] = 1;
        this->infoPhysics.release();
        this->infoAnimation.release();
    }

    void MESH_MBM_DEBUG::fillAtLeastOneBound()
    {
        headerMesh.deprecated_typePhysics = 1;
        auto base                       = new CUBE();
        this->infoPhysics.release();
        this->infoPhysics.lsCube.push_back(base);
        VEC3 vMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        VEC3 vMin(FLT_MAX, FLT_MAX, FLT_MAX);
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            util::BUFFER_MESH_DEBUG *currentFrameBuffer = this->buffer[std::vector<util::BUFFER_MESH_DEBUG *>::size_type(currentFrame)];
            auto *                   pPosition          = reinterpret_cast<VEC3 *>(currentFrameBuffer->position);
            for (auto pSubset : currentFrameBuffer->subset)
            {
                const int           t       = pSubset->vertexCount + pSubset->vertexStart;
                for (int i = pSubset->vertexStart; i < t; ++i)
                {
                    VEC3 *p = &pPosition[i];
                    if (p->x < vMin.x)
                        vMin.x = p->x;
                    if (p->y < vMin.y)
                        vMin.y = p->y;
                    if (p->z < vMin.z)
                        vMin.z = p->z;

                    if (p->x > vMax.x)
                        vMax.x = p->x;
                    if (p->y > vMax.y)
                        vMax.y = p->y;
                    if (p->z > vMax.z)
                        vMax.z = p->z;
                }
            }
        }
        base->halfDim.x   = (vMax.x - vMin.x) * 0.5f;
        base->halfDim.y   = (vMax.y - vMin.y) * 0.5f;
        base->halfDim.z   = (vMax.z - vMin.z) * 0.5f;
        base->absCenter.x = vMin.x + (base->halfDim.x);
        base->absCenter.y = vMin.y + (base->halfDim.y);
        base->absCenter.z = vMin.z + (base->halfDim.z);
    }
    
    bool MESH_MBM_DEBUG::fillAnimation_2(const char *fileNamePath, FILE *fp)
    {
        for (int i = 0; i < headerMesh.totalAnimation; ++i)
        {
            auto headerAnim = new util::HEADER_ANIMATION();
            if (!fread(headerAnim, sizeof(util::HEADER_ANIMATION), 1, fp))
            {
                delete headerAnim;
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load animation 's mesh [%s]", fileNamePath);
            }
            auto infoHead = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            this->infoAnimation.lsHeaderAnim.push_back(infoHead);
            infoHead->headerAnim = headerAnim;

            if(headerAnim->hasShaderEffect == 1)
            {
                infoHead->effetcShader = new util::INFO_FX();
                util::HEADER_INFO_SHADER_STEP headerPS_VS;
                if (!fread(&headerPS_VS, sizeof(util::HEADER_INFO_SHADER_STEP), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header step [%s]", fileNamePath);
                if(headerPS_VS.blendOperation != 0)
                    infoHead->effetcShader->blendOperation = headerPS_VS.blendOperation;
                else
                    infoHead->effetcShader->blendOperation = 1;
                if (headerPS_VS.lenNameShader) // do we have pixel shader?
                {
                    auto dataInfo =
                        new util::INFO_SHADER_DATA(headerPS_VS.sizeArrayVarInBytes, static_cast<short>(headerPS_VS.lenNameShader),
                                                    static_cast<short>(headerPS_VS.lenTextureStage2));
                    infoHead->effetcShader->blendOperation = headerPS_VS.blendOperation;
                    infoHead->effetcShader->dataPS     = (dataInfo);
                    dataInfo->typeAnimation    = headerPS_VS.typeAnimation;
                    dataInfo->timeAnimation    = headerPS_VS.timeAnimation;
                    if (!fread(dataInfo->fileNameShader, static_cast<size_t>(headerPS_VS.lenNameShader), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read shader's name [%s]", fileNamePath);

                    if (headerPS_VS.lenTextureStage2)
                    {
                        if (!fread(dataInfo->fileNameTextureStage2, static_cast<size_t>(headerPS_VS.lenTextureStage2), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read name's texture [%s]", fileNamePath);
                    }
                    if (headerPS_VS.sizeArrayVarInBytes)
                    {
                        if (!fread(dataInfo->typeVars, static_cast<size_t>(dataInfo->lenVars), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                        if (!fread(dataInfo->min, sizeof(float) * static_cast<size_t>(dataInfo->lenVars) * 4, 1,
                                   fp)) // sempre lemos de 4 em 4 floats
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                        if (!fread(dataInfo->max, sizeof(float) * static_cast<size_t>(dataInfo->lenVars) * 4, 1,
                                   fp)) // sempre lemos de 4 em 4 floats
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                    }
                }
                if (!fread(&headerPS_VS, sizeof(util::HEADER_INFO_SHADER_STEP), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header step [%s]", fileNamePath);
                if (headerPS_VS.lenNameShader) // temos vertex shader
                {
                    auto dataInfo =
                        new util::INFO_SHADER_DATA(headerPS_VS.sizeArrayVarInBytes, static_cast<short>(headerPS_VS.lenNameShader),
                                                    static_cast<short>(headerPS_VS.lenTextureStage2));
                    infoHead->effetcShader->blendOperation = headerPS_VS.blendOperation;
                    infoHead->effetcShader->dataVS     = (dataInfo);
                    dataInfo->typeAnimation    = headerPS_VS.typeAnimation;
                    dataInfo->timeAnimation    = headerPS_VS.timeAnimation;
                    if (!fread(dataInfo->fileNameShader, static_cast<size_t>(headerPS_VS.lenNameShader), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read shader's name [%s]", fileNamePath);

                    if (headerPS_VS.lenTextureStage2)
                    {
                        if (!fread(dataInfo->fileNameTextureStage2, static_cast<size_t>(headerPS_VS.lenTextureStage2), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read name's texture [%s]", fileNamePath);
                    }
                    if (headerPS_VS.sizeArrayVarInBytes)
                    {
                        if (!fread(dataInfo->typeVars, static_cast<size_t>(dataInfo->lenVars), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                        if (!fread(dataInfo->min, sizeof(float) * static_cast<size_t>(dataInfo->lenVars) * 4, 1,
                                   fp)) // sempre lemos de 4 em 4 floats
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                        if (!fread(dataInfo->max, sizeof(float) * static_cast<size_t>(dataInfo->lenVars) * 4, 1,
                                   fp)) // sempre lemos de 4 em 4 floats
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                    }
                }
            }
        }
        return true;
    }
    
    bool MESH_MBM_DEBUG::loadFromSplited(FILE *fp, const int sizeVertexBuffer, VEC3 **positionOut,
                                VEC3 **normalOut, VEC2 **textureOut, int16_t hasNorText[2],
                                uint16_t *indexArray, const int sizeArrayIndex, const int stride)
    {
        auto pPosition = new VEC3[sizeVertexBuffer];
        auto pNormal   = new VEC3[sizeVertexBuffer];
        auto pTexture  = new VEC2[sizeVertexBuffer];
        *positionOut            = pPosition;
        *normalOut              = pNormal;
        *textureOut             = pTexture;
        if (stride == 3)
        {
            if (!fread(pPosition, sizeof(VEC3) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
            {
                delete[] pPosition;
                delete[] pNormal;
                delete[] pTexture;
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex");
            }
            if (hasNorText[0])
            {
                if (!fread(pNormal, sizeof(VEC3) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex");
                }
            }
            else
            {
                if (indexArray && sizeArrayIndex)
                {
                    for (int i = 0; i < sizeArrayIndex; i += 3)
                    {
                        const int index0 = indexArray[i];
                        const int index1 = indexArray[i + 1];
                        const int index2 = indexArray[i + 2];
                        if (index0 >= sizeVertexBuffer || index1 >= sizeVertexBuffer || index2 >= sizeVertexBuffer)
                        {
                            delete[] pPosition;
                            delete[] pNormal;
                            delete[] pTexture;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "inconsistent index to normal");
                        }
                        VEC3 a(pPosition[index1] - pPosition[index0]);
                        VEC3 b(pPosition[index2] - pPosition[index1]);
                        vec3Cross(&pNormal[index0], &a, &b);
                        vec3Normalize(&pNormal[index0], &pNormal[index0]);
                        pNormal[index1] = pNormal[index0];
                        pNormal[index2] = pNormal[index0];
                    }
                }
                else
                {
                    for (int i = 0; i < sizeVertexBuffer; i += 3)
                    {
                        VEC3 a(pPosition[i + 1] - pPosition[i]);
                        VEC3 b(pPosition[i + 2] - pPosition[i + 1]);
                        vec3Cross(&pNormal[i], &a, &b);
                        vec3Normalize(&pNormal[i], &pNormal[i]);
                        pNormal[i + 1] = pNormal[i];
                        pNormal[i + 2] = pNormal[i];
                    }
                }
            }
            if (hasNorText[1] == 1) // As coordenadas estão presentes em cada frame
            {
                if (!fread(pTexture, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data uv");
                }
            }
            else if (hasNorText[1] == 2) // As coordenadas só estão presentes no primeiro frame
            {
                if (this->coordTexFrame_0) // Ja passou pelo primeiro frame, então só copia
                {
                    if (sizeVertexBuffer != this->sizeCoordTexFrame_0)
                        memset(static_cast<void*>(pTexture), 0, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer));
                    int safeCopy = std::min(sizeVertexBuffer, this->sizeCoordTexFrame_0);
                    memcpy(static_cast<void*>(pTexture), this->coordTexFrame_0, sizeof(VEC2) * static_cast<size_t>(safeCopy));
                }
                else //É o primeiro frame então guarda as coordenadas
                {
                    this->coordTexFrame_0 = new VEC2[sizeVertexBuffer];
                    this->sizeCoordTexFrame_0 = sizeVertexBuffer;
                    if (!fread(pTexture, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
                    {
                        delete[] pPosition;
                        delete[] pNormal;
                        delete[] pTexture;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data uv");
                    }
                    memcpy(static_cast<void*>(this->coordTexFrame_0), pTexture, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer));
                }
            }
            else
            {
                for (int i = 0, j = 0; i < sizeVertexBuffer; i += 3, ++j)
                {
                    if (j % 2)
                    {
                        pTexture[i].x = 0;
                        pTexture[i].y = 1;

                        pTexture[i + 1].x = 0;
                        pTexture[i + 1].y = 0;

                        pTexture[i + 2].x = 1;
                        pTexture[i + 2].y = 1;
                    }
                    else
                    {
                        pTexture[i].x = 1;
                        pTexture[i].y = 1;

                        pTexture[i + 1].x = 0;
                        pTexture[i + 1].y = 0;

                        pTexture[i + 2].x = 1;
                        pTexture[i + 2].y = 0;
                    }
                }
            }
            return true;
        }
        else if (stride == 2)
        {
            auto pStridePosition = new VEC2[sizeVertexBuffer];
            if (!fread(pStridePosition, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
            {
                delete[] pPosition;
                delete[] pNormal;
                delete[] pTexture;
                delete[] pStridePosition;
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex");
            }
            for (int i = 0; i < sizeVertexBuffer; ++i)
            {
                pPosition[i].x = pStridePosition[i].x;
                pPosition[i].y = pStridePosition[i].y;
                pPosition[i].z = 0.0f;
            }
            delete[] pStridePosition;
            pStridePosition = nullptr;
            if (hasNorText[0])
            {
                if (!fread(pNormal, sizeof(VEC3) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex");
                }
            }
            else
            {
                if (indexArray && sizeArrayIndex)
                {
                    for (int i = 0; i < sizeArrayIndex; i += 3)
                    {
                        const int index0 = indexArray[i];
                        const int index1 = indexArray[i + 1];
                        const int index2 = indexArray[i + 2];
                        if (index0 >= sizeVertexBuffer || index1 >= sizeVertexBuffer || index2 >= sizeVertexBuffer)
                        {
                            delete[] pPosition;
                            delete[] pNormal;
                            delete[] pTexture;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "inconsistent index to normal");
                        }
                        VEC3 a(pPosition[index1] - pPosition[index0]);
                        VEC3 b(pPosition[index2] - pPosition[index1]);
                        vec3Cross(&pNormal[index0], &a, &b);
                        vec3Normalize(&pNormal[index0], &pNormal[index0]);
                        pNormal[index1] = pNormal[index0];
                        pNormal[index2] = pNormal[index0];
                    }
                }
                else
                {
                    for (int i = 0; i < sizeVertexBuffer; i += 3)
                    {
                        VEC3 a(pPosition[i + 1] - pPosition[i]);
                        VEC3 b(pPosition[i + 2] - pPosition[i + 1]);
                        vec3Cross(&pNormal[i], &a, &b);
                        vec3Normalize(&pNormal[i], &pNormal[i]);
                        pNormal[i + 1] = pNormal[i];
                        pNormal[i + 2] = pNormal[i];
                    }
                }
            }
            if (hasNorText[1] == 1) // As coordenadas estão presentes em cada frame
            {
                if (!fread(pTexture, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data uv");
                }
            }
            else if (hasNorText[1] == 2) // As coordenadas só estão presentes no primeiro frame
            {
                if (this->coordTexFrame_0) // Ja passou pelo primeiro frame, então só copia
                {
                    if (sizeVertexBuffer != this->sizeCoordTexFrame_0)
                        memset(static_cast<void*>(pTexture), 0, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer));
                    int safeCopy = std::min(sizeVertexBuffer, this->sizeCoordTexFrame_0);
                    memcpy(static_cast<void*>(pTexture), this->coordTexFrame_0, sizeof(VEC2) * static_cast<size_t>(safeCopy));
                }
                else //First frame, keep the coordinates
                {
                    this->coordTexFrame_0 = new VEC2[sizeVertexBuffer];
                    this->sizeCoordTexFrame_0 = sizeVertexBuffer;
                    if (!fread(pTexture, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
                    {
                        delete[] pPosition;
                        delete[] pNormal;
                        delete[] pTexture;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data uv");
                    }
                    memcpy(static_cast<void*>(this->coordTexFrame_0), pTexture, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer));
                }
            }
            else
            {
                for (int i = 0, j = 0; i < sizeVertexBuffer; i += 3, ++j)
                {
                    if (j % 2)
                    {
                        pTexture[i].x = 0;
                        pTexture[i].y = 1;

                        pTexture[i + 1].x = 0;
                        pTexture[i + 1].y = 0;

                        pTexture[i + 2].x = 1;
                        pTexture[i + 2].y = 1;
                    }
                    else
                    {
                        pTexture[i].x = 1;
                        pTexture[i].y = 1;

                        pTexture[i + 1].x = 0;
                        pTexture[i + 1].y = 0;

                        pTexture[i + 2].x = 1;
                        pTexture[i + 2].y = 0;
                    }
                }
            }
            return true;
        }
        else
        {
            return log_util::onFailed(fp,__FILE__, __LINE__, "stride unknown. must be 2 or 3");
        }
    }
    
    bool MESH_MBM_DEBUG::saveAnimationHeaders(const char *fileOut, FILE **file)
    {
        // 4 header anim -- Todas as animações -----------------------------------------------------------
        for (int i = 0; i < this->headerMesh.totalAnimation; ++i)
        {
            util::INFO_ANIMATION::INFO_HEADER_ANIM * infoHead   = this->infoAnimation.lsHeaderAnim[ std::vector<util::INFO_ANIMATION::INFO_HEADER_ANIM *>::size_type(i)];
            util::HEADER_ANIMATION       headerAnim = *infoHead->headerAnim;
            if (headerAnim.hasShaderEffect == 0)
                headerAnim.hasShaderEffect = 1;
            if (!util::addToFileBinary(fileOut, &headerAnim, sizeof(util::HEADER_ANIMATION), file))
                return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add animation header", *file);
            if(infoHead->effetcShader)
            {
                bool hasPX = false;
                // Pixel Shader
                if (infoHead->effetcShader->dataPS)
                {
                    util::INFO_FX *      infoShaderStep = infoHead->effetcShader;
                    util::INFO_SHADER_DATA *     infoShader     = infoShaderStep->dataPS;
                    util::HEADER_INFO_SHADER_STEP headerPS_VS;
                    hasPX                     = true;
                    headerPS_VS.lenNameShader = static_cast<short>(strlen(infoShader->fileNameShader) + 1);

                    if (infoShader->fileNameTextureStage2)
                        headerPS_VS.lenTextureStage2 = static_cast<short>(strlen(infoShader->fileNameTextureStage2) + 1);
                    else
                        headerPS_VS.lenTextureStage2 = 0;
                    headerPS_VS.sizeArrayVarInBytes  = static_cast<short>(infoShader->lenVars * 4); //?
                    headerPS_VS.typeAnimation        = static_cast<short>(infoShader->typeAnimation);
                    headerPS_VS.blendOperation       = infoShaderStep->blendOperation;
                    headerPS_VS.timeAnimation        = infoShader->timeAnimation;
                    if (!util::addToFileBinary(fileOut, &headerPS_VS, sizeof(util::HEADER_INFO_SHADER_STEP), file))
                        return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add header shader to file");
                    if (headerPS_VS.lenNameShader)
                    {
                        if (!util::addToFileBinary(fileOut,infoShader->fileNameShader, static_cast<size_t>(headerPS_VS.lenNameShader),
                                                   file))
                            return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add name of pixel shader");
                    }
                    if (headerPS_VS.lenTextureStage2)
                    {
                        if (!util::addToFileBinary(fileOut,infoShader->fileNameTextureStage2,
                                                   static_cast<size_t>(headerPS_VS.lenTextureStage2), file))
                            return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add textures name stage 2");
                    }
                    if (headerPS_VS.sizeArrayVarInBytes)
                    {
                        std::vector<char> lsSizeVar;
                        lsSizeVar.reserve(infoShader->lenVars);
                        for (int j = 0; j < infoShader->lenVars; ++j)
                        {
                            lsSizeVar.push_back(static_cast<char>(infoShader->typeVars[j]));
                        }
                        if (!util::addToFileBinary(fileOut, &lsSizeVar[0], lsSizeVar.size(), file))
                            return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add var to file!!");
                        for (int j = 0; j < infoShader->lenVars; ++j)
                        {
                            float d[4];
                            memcpy(d,&infoShader->min[j*4],sizeof(d));
                            if (!util::addToFileBinary(fileOut, d, sizeof(float) * 4, file))
                                return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add var to file");
                        }
                        for (int j = 0; j < infoShader->lenVars; ++j)
                        {
                            float d[4];
                            memcpy(d,&infoShader->max[j*4],sizeof(d));
                            if (!util::addToFileBinary(fileOut, d, sizeof(float) * 4, file))
                                return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add var to file");
                        }
                    }
                }
                else
                {
                    util::HEADER_INFO_SHADER_STEP headerPS_VS;
                    headerPS_VS.blendOperation           = infoHead->effetcShader->blendOperation;
                    if (!util::addToFileBinary(fileOut, &headerPS_VS, sizeof(util::HEADER_INFO_SHADER_STEP), file))
                        return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add header shader to file");
                }
                // Vertex Shader
                if (infoHead->effetcShader->dataVS)
                {
                    util::INFO_FX *      infoShaderStep = infoHead->effetcShader;
                    util::INFO_SHADER_DATA *     infoShader     = infoShaderStep->dataVS;
                    util::HEADER_INFO_SHADER_STEP headerPS_VS;
                    headerPS_VS.lenNameShader = static_cast<short>(strlen(infoShader->fileNameShader) + 1);

                    if (infoShader->fileNameTextureStage2)
                        headerPS_VS.lenTextureStage2 = static_cast<short>(strlen(infoShader->fileNameTextureStage2) + 1);
                    else
                        headerPS_VS.lenTextureStage2 = 0;
                    if (headerPS_VS.lenTextureStage2 && hasPX)
                        headerPS_VS.lenTextureStage2 = 0; // ja foi acrescentada a textura no pixel shader
                    headerPS_VS.sizeArrayVarInBytes  = static_cast<short>(infoShader->lenVars * 4); //?
                    headerPS_VS.typeAnimation        = static_cast<short>(infoShader->typeAnimation);
                    headerPS_VS.blendOperation           = infoShaderStep->blendOperation;
                    headerPS_VS.timeAnimation        = infoShader->timeAnimation;
                    if (!util::addToFileBinary(fileOut, &headerPS_VS, sizeof(util::HEADER_INFO_SHADER_STEP), file))
                        return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add header shader to file");
                    if (headerPS_VS.lenNameShader)
                    {
                        if (!util::addToFileBinary(fileOut,infoShader->fileNameShader, static_cast<size_t>(headerPS_VS.lenNameShader),
                                                   file))
                            return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add name of shader to file");
                    }
                    if (headerPS_VS.lenTextureStage2)
                    {
                        if (!util::addToFileBinary(fileOut, infoShader->fileNameTextureStage2,
                                                   static_cast<size_t>(headerPS_VS.lenTextureStage2), file))
                            return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add textures name stage 2");
                    }
                    if (headerPS_VS.sizeArrayVarInBytes)
                    {
                        std::vector<char> lsSizeVar;
                        lsSizeVar.reserve(infoShader->lenVars);
                        for (int j = 0; j < infoShader->lenVars; ++j)
                        {
                            lsSizeVar.push_back(static_cast<char>(infoShader->typeVars[j]));
                        }
                        if (!util::addToFileBinary(fileOut, &lsSizeVar[0], lsSizeVar.size(), file))
                            return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add var to file!!");
                        for (int j = 0; j < infoShader->lenVars; ++j)
                        {
                            float d[4];
                            memcpy(d,&infoShader->min[j*4],sizeof(d));
                            if (!util::addToFileBinary(fileOut, d, sizeof(float) * 4, file))
                                return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add var to file");
                        }
                        for (int j = 0; j < infoShader->lenVars; ++j)
                        {
                            float d[4];
                            memcpy(d,&infoShader->max[j*4],sizeof(d));
                            if (!util::addToFileBinary(fileOut, d, sizeof(float) * 4, file))
                                return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add var to file");
                        }
                    }
                }
                else
                {
                    util::HEADER_INFO_SHADER_STEP headerPS_VS;
                    headerPS_VS.blendOperation           = infoHead->effetcShader->blendOperation;
                    if (!util::addToFileBinary(fileOut, &headerPS_VS, sizeof(util::HEADER_INFO_SHADER_STEP), file))
                        return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add header shader to file");
                }
            }
            if(infoHead->effetcShader == nullptr)
            {
                util::HEADER_INFO_SHADER_STEP headerPS_VS;
                if(i < static_cast<int>(lsBlendOperation.size()) && lsBlendOperation[i] != 0)
                    headerPS_VS.blendOperation           = lsBlendOperation[i];
                if (!util::addToFileBinary(fileOut, &headerPS_VS, sizeof(util::HEADER_INFO_SHADER_STEP), file))//pixel
                    return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add header shader to file");
                if (!util::addToFileBinary(fileOut, &headerPS_VS, sizeof(util::HEADER_INFO_SHADER_STEP), file))//vertex
                        return log_util::onFailed(*file,__FILE__, __LINE__, "failed to add header shader to file");
            }
        }
        return true;
    }
    
    bool MESH_MBM_DEBUG::compressFile(const char *fileNameIn, char *stringStatus,const int lenStatus)
    {
        if (!fileNameIn)
        {
            if (stringStatus)
                strncpy(stringStatus, "name of file empty",lenStatus);
            return false;
        }
        MINIZ  miniz;
        std::string fileNameTemp(fileNameIn);
        fileNameTemp += ".tmp";
        if (rename(fileNameIn, fileNameTemp.c_str()))
        {
            if (stringStatus)
                strncpy(stringStatus, "failed to rename file",lenStatus);
            return false;
        }
        if (miniz.compressFile(fileNameTemp.c_str(), fileNameIn))
        {
            remove(fileNameTemp.c_str());
            if (stringStatus && stringStatus[0] == 0)
                strncpy(stringStatus, "File successfully saved",lenStatus);
            return true;
        }
        else
        {
            rename(fileNameTemp.c_str(), fileNameIn);
            if (stringStatus)
                strncpy(stringStatus, "error on compress file",lenStatus);
            return false;
        }
    }

    #if defined ANDROID //ANDROID //TODO fix issue not found EGL lib on ANDOID 
        typedef void* (PFNGLMAPBUFFEROESPROC_TODO)       (GLenum target, GLenum access);
        typedef GLboolean (PFNGLUNMAPBUFFEROESPROC_TODO) (GLenum target);
    #endif

    bool MESH_MBM_DEBUG::loadDebugFromMemory(const MESH_MBM* meshMemory)
    {
        if(meshMemory == nullptr || meshMemory->isLoaded() == false)
            return log_util::onFailed(nullptr,__FILE__, __LINE__, "Mesh empty or not loaded...");
        auto *extensionString = (char*)glGetString(GL_EXTENSIONS);
        if (strstr(extensionString, "GL_OES_mapbuffer") == nullptr)
            return log_util::onFailed(nullptr,__FILE__, __LINE__, "extension [GL_OES_mapbuffer] not supported!");
        #if defined ANDROID //ANDROID //TODO fix issue not found EGL lib on ANDOID 
            PRINT_IF_DEBUG("loadDebugFromMemory is not working on ANDOID");
            PRINT_IF_DEBUG("TODO: fix issue not found EGL lib on ANDOID");
            PFNGLMAPBUFFEROESPROC_TODO   * glMapBufferOES   = nullptr;
            PFNGLUNMAPBUFFEROESPROC_TODO * glUnmapBufferOES = nullptr;
        #else //ANDROID //TODO fix issue not found EGL lib on ANDOID 
            auto glMapBufferOES     = (PFNGLMAPBUFFEROESPROC)eglGetProcAddress("glMapBufferOES");
            auto glUnmapBufferOES   = (PFNGLUNMAPBUFFEROESPROC)eglGetProcAddress("glUnmapBufferOES");
        #endif
        if(glMapBufferOES == nullptr)
            return log_util::onFailed(nullptr,__FILE__, __LINE__, "extension [glMapBufferOES] not supported!");
        if(glUnmapBufferOES == nullptr)
            return log_util::onFailed(nullptr,__FILE__, __LINE__, "extension [glUnmapBufferOES] not supported!");
        this->release();
        fileName = meshMemory->getFilenameMesh();
        // step 1: Verificação do header
        // -------------------------------------------------------------------------------
        switch (meshMemory->getTypeMesh())
        {
            case util::TYPE_MESH_3D:       strncpy(headerMain.typeApp,"Mesh 3d mbm",sizeof(headerMain.typeApp)-1);  break;
            case util::TYPE_MESH_SHAPE:    strncpy(headerMain.typeApp,"Shape mbm",sizeof(headerMain.typeApp)-1);    break;
            case util::TYPE_MESH_USER:     strncpy(headerMain.typeApp,"User mbm",sizeof(headerMain.typeApp)-1);     break;
            case util::TYPE_MESH_SPRITE:   strncpy(headerMain.typeApp,"Sprite mbm",sizeof(headerMain.typeApp)-1);   break;
            case util::TYPE_MESH_TILE_MAP: strncpy(headerMain.typeApp,"Tile mbm", sizeof(headerMain.typeApp) - 1);  break;
            case util::TYPE_MESH_FONT:     strncpy(headerMain.typeApp,"Font mbm",sizeof(headerMain.typeApp)-1);     break;
            case util::TYPE_MESH_PARTICLE: strncpy(headerMain.typeApp,"Particle mbm",sizeof(headerMain.typeApp)-1); break;
            default:
                return log_util::onFailed(nullptr,__FILE__, __LINE__, "Mesh invalid type");
            break;
        }
        strncpy(headerMain.name, "mbm",sizeof(headerMain.name)-1);
        headerMain.version = CURRENT_VERSION_MBM_HEADER;
        headerMain.magic = 0x010203ff;
        typeMe = meshMemory->getTypeMesh();
        // step 2: --------------------------------------------------------------------------------------------------
        for(auto pCube : meshMemory->infoPhysics.lsCube)
        {
            auto cube = new CUBE(pCube->halfDim,pCube->absCenter);
            this->infoPhysics.lsCube.push_back(cube);
        }
        for(auto pBase : meshMemory->infoPhysics.lsSphere)
        {
            auto base        = new SPHERE();
            base->absCenter[0] = pBase->absCenter[0];
            base->absCenter[1] = pBase->absCenter[1];
            base->absCenter[2] = pBase->absCenter[2];
            base->ray          = pBase->ray;
            this->infoPhysics.lsSphere.push_back(base);
        }
        for(auto pComplex : meshMemory->infoPhysics.lsCubeComplex)
        {
            auto complex = new CUBE_COMPLEX();
            for(int k=0; k< 8; k++)
                complex->p[k] = pComplex->p[k];
            this->infoPhysics.lsCubeComplex.push_back(complex);
        }
        for(auto pTriangle : meshMemory->infoPhysics.lsTriangle)
        {
            auto triangle = new TRIANGLE();
            triangle->point[0] = pTriangle->point[0];
            triangle->point[1] = pTriangle->point[1];
            triangle->point[2] = pTriangle->point[2];
            this->infoPhysics.lsTriangle.push_back(triangle);
        }
        if(meshMemory->getInfoFont() != nullptr)
        {
            const INFO_BOUND_FONT *   pMemoryInfoFont = meshMemory->getInfoFont();
            headerMain.backBufferHeight     = pMemoryInfoFont->heightLetter;
            this->extraInfo					= new INFO_BOUND_FONT();
            auto *   infoFont	= static_cast<INFO_BOUND_FONT*>(this->extraInfo);
            util::DETAIL_HEADER_FONT headerFont;
            infoFont->fontName          = pMemoryInfoFont->fontName;
            infoFont->heightLetter      = pMemoryInfoFont->heightLetter;
            infoFont->spaceXCharacter   = pMemoryInfoFont->spaceXCharacter;
            infoFont->spaceYCharacter   = pMemoryInfoFont->spaceYCharacter;
            for (std::vector<util::DETAIL_LETTER*>::size_type j = 0; j < 255; ++j)
            {
                const util::DETAIL_LETTER *pDetailFont =  pMemoryInfoFont->letter[j].detail;
                if(pMemoryInfoFont->letter[j].detail)
                {
                    auto detailFont = new util::DETAIL_LETTER();
                    detailFont->heightLetter    = pDetailFont->heightLetter;
                    detailFont->indexFrame      = pDetailFont->indexFrame;
                    detailFont->letter          = pDetailFont->letter;
                    detailFont->widthLetter     = pDetailFont->widthLetter;
                    infoFont->letter[j].detail  = detailFont;
                }
            }
        }
        if(meshMemory->getInfoParticle() != nullptr)
        {
            const std::vector<util::STAGE_PARTICLE*>* thatParticleInfo = meshMemory->getInfoParticle();
            auto* lsParticleInfo = new std::vector<util::STAGE_PARTICLE*>();
            this->extraInfo = lsParticleInfo;
            for (auto thatStage : *thatParticleInfo)
            {
                auto* stage = new util::STAGE_PARTICLE(thatStage);
                lsParticleInfo->push_back(stage);
            }
        }
        if(meshMemory->getInfoTile() != nullptr)
        {
            const util::BTILE_INFO* thatInfoTile = meshMemory->getInfoTile();
            this->extraInfo = thatInfoTile->clone();
        }
        headerMesh.totalAnimation = meshMemory->infoAnimation.lsHeaderAnim.size();
        for(int i=0; i< headerMesh.totalAnimation; ++i)
        {
            const util::INFO_ANIMATION::INFO_HEADER_ANIM* pInfoAnim    = meshMemory->infoAnimation.lsHeaderAnim[i];
            auto  infoHead										= new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            infoHead->headerAnim                                = new util::HEADER_ANIMATION();
            this->infoAnimation.lsHeaderAnim.push_back(infoHead);
            util::HEADER_ANIMATION *headerAnim  = infoHead->headerAnim;
            headerAnim->hasShaderEffect   = pInfoAnim->headerAnim->hasShaderEffect;
            headerAnim->blendState        = pInfoAnim->headerAnim->blendState;
            headerAnim->initialFrame      = pInfoAnim->headerAnim->initialFrame;
            headerAnim->finalFrame        = pInfoAnim->headerAnim->finalFrame;
            headerAnim->timeBetweenFrame  = pInfoAnim->headerAnim->timeBetweenFrame;
            headerAnim->typeAnimation     = pInfoAnim->headerAnim->typeAnimation;
            strncpy(headerAnim->nameAnimation,pInfoAnim->headerAnim->nameAnimation,sizeof(headerAnim->nameAnimation));
            headerAnim->hasShaderEffect             = (uint16_t)(infoHead->effetcShader ? 1 : 0);
            infoHead->headerAnim->blendState        = (uint16_t)headerAnim->blendState;
            //for(auto pInfoStepShader : pInfoAnim->lsStepEffetcShader)
            if(infoHead->effetcShader)
            {
                auto pInfoStepShader = pInfoAnim->effetcShader;
                //each step may has two shaders (PS and VS)
                auto infoStepShader  = new util::INFO_FX();
                infoHead->effetcShader = infoStepShader;
                infoStepShader->blendOperation = pInfoStepShader->blendOperation;
                
                if(pInfoStepShader->dataPS)
                {
                    infoStepShader->dataPS = new util::INFO_SHADER_DATA(
                        pInfoStepShader->dataPS->lenVars * 4,
                        strlen(pInfoStepShader->dataPS->fileNameShader) +1,
                        pInfoStepShader->dataPS->fileNameTextureStage2 ? strlen(pInfoStepShader->dataPS->fileNameTextureStage2) + 1 : 0);
                    strcpy(infoStepShader->dataPS->fileNameShader,pInfoStepShader->dataPS->fileNameShader);
                    if(infoStepShader->dataPS->fileNameTextureStage2)
                        strcpy(infoStepShader->dataPS->fileNameTextureStage2,pInfoStepShader->dataPS->fileNameTextureStage2);
                    infoStepShader->dataPS->timeAnimation = pInfoStepShader->dataPS->timeAnimation;
                    infoStepShader->dataPS->typeAnimation = pInfoStepShader->dataPS->typeAnimation;
                    for(int k=0; k < infoStepShader->dataPS->lenVars; ++k)
                    {
                        const int index = k * 4;
                        memcpy(&infoStepShader->dataPS->max[index],&pInfoStepShader->dataPS->max[index],sizeof(float) * 4);
                        memcpy(&infoStepShader->dataPS->min[index],&pInfoStepShader->dataPS->min[index],sizeof(float) * 4);
                        infoStepShader->dataPS->typeVars[k] = pInfoStepShader->dataPS->typeVars[k];
                    }
                }
                if(pInfoStepShader->dataVS)
                {
                    infoStepShader->dataVS = new util::INFO_SHADER_DATA(
                        pInfoStepShader->dataVS->lenVars * 4,
                        strlen(pInfoStepShader->dataVS->fileNameShader) +1,
                        pInfoStepShader->dataVS->fileNameTextureStage2 ? strlen(pInfoStepShader->dataVS->fileNameTextureStage2) + 1 : 0);
                    strcpy(infoStepShader->dataVS->fileNameShader,pInfoStepShader->dataVS->fileNameShader);
                    if(infoStepShader->dataVS->fileNameTextureStage2)
                        strcpy(infoStepShader->dataVS->fileNameTextureStage2,pInfoStepShader->dataVS->fileNameTextureStage2);
                    infoStepShader->dataVS->timeAnimation = pInfoStepShader->dataVS->timeAnimation;
                    infoStepShader->dataVS->typeAnimation = pInfoStepShader->dataVS->typeAnimation;
                    for(int k=0; k < infoStepShader->dataVS->lenVars; ++k)
                    {
                        const int index = k * 4;
                        memcpy(&infoStepShader->dataVS->max[index],&pInfoStepShader->dataVS->max[index],sizeof(float) * 4);
                        memcpy(&infoStepShader->dataVS->min[index],&pInfoStepShader->dataVS->min[index],sizeof(float) * 4);
                        infoStepShader->dataVS->typeVars[k] = pInfoStepShader->dataVS->typeVars[k];
                    }
                }
            }
        }
        
        headerMesh.totalFrames = meshMemory->getTotalFrame();
        std::map<int,float> lsLetterChangedValuesByLetterX;
        std::map<int,float> lsLetterChangedValuesByCurFrameX;
        std::map<int,float> lsLetterChangedValuesByLetterY;
        std::map<int,float> lsLetterChangedValuesByCurFrameY;
        if(meshMemory->getInfoFont() != nullptr)//TODO
        {
            const INFO_BOUND_FONT * pMemoryInfoFont = meshMemory->getInfoFont();
            const auto sL = static_cast<int>(sizeof(mbm::INFO_BOUND_FONT::letterDiffY) / sizeof(float));

            for(int i=0; i< sL; ++i)
            {
                if(pMemoryInfoFont->letterDiffY[i] != 0.0f)
                {
                    lsLetterChangedValuesByLetterY[i] = pMemoryInfoFont->letterDiffY[i];
                }
                if(pMemoryInfoFont->letterDiffX[i] != 0.0f)
                {
                    lsLetterChangedValuesByLetterX[i] = pMemoryInfoFont->letterDiffX[i];
                }
            }
            for(auto & j : pMemoryInfoFont->letter)
            {
                const util::DETAIL_LETTER *pDetailFont =  j.detail;
                if(pDetailFont)
                {
                    const float x = lsLetterChangedValuesByLetterX[pDetailFont->letter];
                    if(x != 0.0f)
                    {
                        lsLetterChangedValuesByCurFrameX[pDetailFont->indexFrame] = x;
                    }
                    const float y = lsLetterChangedValuesByLetterY[pDetailFont->letter];
                    if(y != 0.0f)
                    {
                        lsLetterChangedValuesByCurFrameY[pDetailFont->indexFrame] = y;
                    }
                }
            }
        }
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            auto pBuffer = new util::BUFFER_MESH_DEBUG();
            this->buffer.push_back(pBuffer);
            // 5 Sequencia lógica dos frames --------------------------------------------------------------------------
            // Cada header Frame
            // --------------------------------------------------------------------------------------------------
            util::HEADER_FRAME *headerFrame    = &pBuffer->headerFrame;
            const BUFFER_MESH* pBufferMesh     = meshMemory->getBuffer(currentFrame);
            const BUFFER_GL* pGl               = pBufferMesh->pBufferGL;
            if(pGl->isIndexBuffer)
            {
                strncpy(headerFrame->typeBuffer,"IB",sizeof(headerFrame->typeBuffer)-1);
                for(uint32_t i=0; i< pBufferMesh->pBufferGL->totalSubset; ++i)
                {
                    headerFrame->sizeIndexBuffer  += pBufferMesh->pBufferGL->indexCountIB[i];
                }
            }
            else
            {
                strncpy(headerFrame->typeBuffer,"VB",sizeof(headerFrame->typeBuffer)-1);
                for(uint32_t i=0; i< pBufferMesh->pBufferGL->totalSubset; ++i)
                {
                    headerFrame->sizeVertexBuffer  += pBufferMesh->pBufferGL->vertexCountVB[i];
                }
            }
            headerFrame->stride = 3;
            // 6 Todos os headers subset deste frame
            // -------------------------------------------------------------------------------
            headerFrame->totalSubset = pBufferMesh->pBufferGL->totalSubset;
            // 6.2 Vertex buffer e index buffer
            // -----------------------------------------------------------------------------------------
            if (headerFrame->sizeIndexBuffer && strcmp(headerFrame->typeBuffer, "IB") == 0)
            {
                pBuffer->indexBuffer = new uint16_t[headerFrame->sizeIndexBuffer];
                uint16_t acumulated = 0;
                for(uint32_t i=0; i< pBufferMesh->pBufferGL->totalSubset; ++i)
                {
                    auto pSubset = new util::SUBSET_DEBUG();
                    pBuffer->subset.push_back(pSubset);
                    uint16_t maxIndexSubset = 0;
                    pSubset->indexStart             = pGl->indexStartIB[i];
                    pSubset->indexCount             = pGl->indexCountIB[i];
                    pBuffer->subset[i]->indexStart  = pSubset->indexStart;
                    pBuffer->subset[i]->indexCount  = pSubset->indexCount;
                    GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pGl->vboIndexSubsetIB[i]);
                    auto *indexBuffer = static_cast<uint16_t*>(glMapBufferOES(GL_ELEMENT_ARRAY_BUFFER,GL_WRITE_ONLY_OES));
                    if(indexBuffer == nullptr)
                        return log_util::onFailed(nullptr,__FILE__, __LINE__, "Failed to get index at [glMapBufferOES] [%s]", meshMemory->getFilenameMesh());
                    for(int j=0; j< pSubset->indexCount; ++j)
                    {
                        const int index             = pSubset->indexStart + j;
                        pBuffer->indexBuffer[index] = indexBuffer[j];
                        maxIndexSubset = std::max(pBuffer->indexBuffer[index],maxIndexSubset);
                    }
                    glUnmapBufferOES(GL_ELEMENT_ARRAY_BUFFER);
                    uint16_t vertexCount  = maxIndexSubset + 1;
                    pSubset->vertexCount            = vertexCount;
                    pSubset->vertexStart            = acumulated;
                    acumulated                      += vertexCount;

                    if(pBufferMesh->subset[i].texture)
                    {
                        pSubset->texture = pBufferMesh->subset[i].texture->getFileNameTexture();
                    }
                }
                headerFrame->sizeVertexBuffer = (acumulated);
                const uint32_t totalUv    = (acumulated) * 2;
                pBuffer->position   = new float[headerFrame->sizeVertexBuffer * 3];
                pBuffer->normal     = new float[headerFrame->sizeVertexBuffer * 3];
                pBuffer->uv         = new float[totalUv];
            }
            // 6.3 Vertex Buffer somente
            // ----------------------------------------------------------------------------------------------
            else if (strcmp(headerFrame->typeBuffer, "VB") == 0)
            {
                const uint32_t totalVertex = (headerFrame->sizeVertexBuffer) * 3;
                const uint32_t totalNormal = (headerFrame->sizeVertexBuffer) * 3;
                const uint32_t totalUv     = (headerFrame->sizeVertexBuffer) * 2;
                pBuffer->position   = new float[totalVertex];
                pBuffer->normal     = new float[totalNormal];
                pBuffer->uv         = new float[totalUv];
            }
            else
            {
                return log_util::onFailed(nullptr,__FILE__, __LINE__, "unknown buffer type [%s]", meshMemory->getFilenameMesh());
            }
            bool is_dynamic_shape   = false;
            float *pPosition        = nullptr;
            float *pNormal          = nullptr;
            float *pTexture         = nullptr;
            const util::DYNAMIC_SHAPE* infoShape =  meshMemory->getInfoShape(); //maybe is dynamic shape
            if(infoShape && infoShape->dynamicVertex)
            {
                pPosition           = infoShape->dynamicVertex;
                pNormal             = infoShape->dynamicNormal;
                pTexture            = infoShape->dynamicNormal;
                is_dynamic_shape    = pPosition != nullptr && pNormal != nullptr && pTexture != nullptr;
                if(is_dynamic_shape == false)
                    return log_util::onFailed(nullptr,__FILE__, __LINE__, "Dynamic shape has nullptr [%s]", meshMemory->getFilenameMesh());
                if(headerFrame->sizeVertexBuffer != static_cast<int>(infoShape->size_vertex / 3))
                    return log_util::onFailed(nullptr,__FILE__, __LINE__, "Dynamic shape has inconsistent vertex buffer [%s] sizeVertexBuffer: [%d] size_vertex [%d] ", meshMemory->getFilenameMesh(),headerFrame->sizeVertexBuffer,infoShape->size_vertex);
                if(headerFrame->sizeVertexBuffer != static_cast<int>(infoShape->size_normal / 3))
                    return log_util::onFailed(nullptr,__FILE__, __LINE__, "Dynamic shape has inconsistent normal buffer [%s] sizeVertexBuffer: [%d] size_normal [%d] ", meshMemory->getFilenameMesh(),headerFrame->sizeVertexBuffer,infoShape->size_normal);
                if(headerFrame->sizeVertexBuffer != static_cast<int>(infoShape->size_uv / 2))
                    return log_util::onFailed(nullptr,__FILE__, __LINE__, "Dynamic shape has inconsistent uv buffer [%s] sizeVertexBuffer: [%d] size_uv [%d] ", meshMemory->getFilenameMesh(),headerFrame->sizeVertexBuffer,infoShape->size_uv);
            }

            if(is_dynamic_shape == false)
            {
                GLBindBuffer(GL_ARRAY_BUFFER,pGl->vboVertNorTexIB[0]);
                pPosition = static_cast<float*>(glMapBufferOES(GL_ARRAY_BUFFER,GL_WRITE_ONLY_OES));
            }
            if(pPosition == nullptr)
            {
                return log_util::onFailed(nullptr,__FILE__, __LINE__, "Failed to get position at [glMapBufferOES] [%s]", meshMemory->getFilenameMesh());
            }
            memcpy(pBuffer->position,pPosition,sizeof(float) * 3 * static_cast<size_t>(headerFrame->sizeVertexBuffer));
            if(is_dynamic_shape == false)
                glUnmapBufferOES(GL_ARRAY_BUFFER);

            if(meshMemory->getInfoFont() != nullptr)
            {
                const float letterDiffX = lsLetterChangedValuesByCurFrameX[currentFrame];
                const float letterDiffY = lsLetterChangedValuesByCurFrameY[currentFrame];
                const auto sL = static_cast<int>(sizeof(mbm::INFO_BOUND_FONT::letterDiffY) / sizeof(float));
                if(currentFrame < sL)
                {
                    if(letterDiffX != 0.0f)
                    {
                        const uint32_t ss = 3 * static_cast<size_t>(headerFrame->sizeVertexBuffer);
                        for(uint32_t ii = 0; ii < ss; ii+=3 )// [0 -> x, 1 -> y, 2 -> z] (first x coord == 0)
                        {
                            pBuffer->position[ii] += letterDiffX;
                        }
                    }
                    if(letterDiffY != 0.0f)
                    {
                        const uint32_t ss = 3 * static_cast<size_t>(headerFrame->sizeVertexBuffer);
                        for(uint32_t ii = 1; ii < ss; ii+=3 )// [0 -> x, 1 -> y, 2 -> z] (first y coord == 1)
                        {
                            pBuffer->position[ii] += letterDiffY;
                        }
                    }
                }
            }
            if(is_dynamic_shape == false)
            {
                GLBindBuffer(GL_ARRAY_BUFFER,pGl->vboVertNorTexIB[1]);
                pNormal   = static_cast<float*>(glMapBufferOES(GL_ARRAY_BUFFER,GL_WRITE_ONLY_OES));
            }
            if(pNormal == nullptr)
            {
                return log_util::onFailed(nullptr,__FILE__, __LINE__, "Failed to get normal at [glMapBufferOES] [%s]", meshMemory->getFilenameMesh());
            }
            memcpy(pBuffer->normal,pNormal,sizeof(float) * 3 * static_cast<size_t>(headerFrame->sizeVertexBuffer));
            if(is_dynamic_shape == false)
            {
                glUnmapBufferOES(GL_ARRAY_BUFFER);
                GLBindBuffer(GL_ARRAY_BUFFER,pGl->vboVertNorTexIB[2]);
                pTexture  = static_cast<float*>(glMapBufferOES(GL_ARRAY_BUFFER,GL_WRITE_ONLY_OES));
            }
            if(pTexture == nullptr)
            {
                const util::DYNAMIC_SHAPE* infoShape =  meshMemory->getInfoShape(); //maybe is dynamic shape
                if(infoShape == nullptr || infoShape->dynamicUV == nullptr)
                    return log_util::onFailed(nullptr,__FILE__, __LINE__, "Failed to get uv at [glMapBufferOES] [%s]", meshMemory->getFilenameMesh());
                
            }
            memcpy(pBuffer->uv,pTexture,sizeof(float) * 2 * static_cast<size_t>(headerFrame->sizeVertexBuffer));
            if(is_dynamic_shape == false)
                glUnmapBufferOES(GL_ARRAY_BUFFER);
        }
        positionOffset = VEC3(headerMesh.posX, headerMesh.posY, headerMesh.posZ);
        angleDefault   = VEC3(headerMesh.angleX, headerMesh.angleY, headerMesh.angleZ);
        this->sizeCoordTexFrame_0 = 0;
        if (this->coordTexFrame_0)
            delete[] this->coordTexFrame_0;
        this->coordTexFrame_0 = nullptr;
        return true;
    }

#endif

    BUFFER_MESH * MESH_MBM::getBuffer(const uint32_t index) const
    {
        if (index < this->totalFramesMesh && buffer)
            return &buffer[index];
        return nullptr;
    }
    
    TEXTURE * MESH_MBM::getTexture(const uint32_t indexFrame, const uint32_t indexSubset)
    {
        if (indexFrame < totalFramesMesh && buffer)
        {
            if (indexSubset < buffer[indexFrame].totalSubset)
                return buffer[indexFrame].subset[indexSubset].texture;
        }
        return nullptr;
    }
    
    bool MESH_MBM::setTexture(const uint32_t indexFrame, const uint32_t indexSubset, const char *fileNameTexture,
                           const bool hasAlpha)
    {
        if (indexFrame < totalFramesMesh && buffer)
        {
            if (indexSubset < buffer[indexFrame].totalSubset)
            {
                buffer[indexFrame].subset[indexSubset].texture =
                    TEXTURE_MANAGER::getInstance()->load(fileNameTexture, hasAlpha);
                if (buffer[indexFrame].pBufferGL && buffer[indexFrame].subset[indexSubset].texture)
                {
                    for (uint32_t i = 0; i < buffer[indexFrame].pBufferGL->totalSubset; ++i)
                    {
                        buffer[indexFrame].pBufferGL->idTexture0[i] =
                            buffer[indexFrame].subset[indexSubset].texture->idTexture;
                    }
                    return true;
                }
                return buffer[indexFrame].subset[indexSubset].texture != nullptr;
            }
        }
        return false;
    }
    
    const char * MESH_MBM::getFilenameMesh() const
    {
        return fileName.c_str();
    }
    
    MESH_MBM::~MESH_MBM()
    {
        release();
    }

    void MESH_MBM::deleteExtraInfo()
    {
        switch(typeMe)
        {
            case util::TYPE_MESH_FONT:
            {
                auto* infoFont = static_cast<mbm::INFO_BOUND_FONT*>(extraInfo);
                if(infoFont)
                    delete infoFont;
            }
            break;
            case util::TYPE_MESH_PARTICLE:
            {
                auto* lsParticleInfo = static_cast<std::vector<util::STAGE_PARTICLE*>*>(extraInfo);
                if(lsParticleInfo)
                {
                    for (auto stage : *lsParticleInfo)
                    {
                        delete stage;
                    }
                    lsParticleInfo->clear();
                    delete lsParticleInfo;
                }
            }
            break;
            case util::TYPE_MESH_TILE_MAP:
            {
                auto* infoTileMap = static_cast<util::BTILE_INFO*>(extraInfo);
                if(infoTileMap)
                    delete infoTileMap;
            }
            break;
            case util::TYPE_MESH_SHAPE:
            {
                auto* infoShape = static_cast<util::DYNAMIC_SHAPE*>(extraInfo);
                if(infoShape)
                    delete infoShape;
            }
            break;
            default:
            {
                if (extraInfo)
                {
                    auto * charExtraInfo = static_cast<char*>(extraInfo);
                    delete[] charExtraInfo;
                }
            }
        }
        extraInfo           = nullptr;
    }
    
    void MESH_MBM::release() //
    {
        deleteExtraInfo();
        if (buffer)
            delete[] buffer;
        buffer = nullptr;
        this->infoPhysics.release();
        this->infoAnimation.release();

        if (coordTexFrame_0)
            delete[] coordTexFrame_0;
        coordTexFrame_0 = nullptr;

        totalFramesMesh = 0;
        
        zoomEditorSprite.x  = 0;
        zoomEditorSprite.y  = 0;
        typeMe              = util::TYPE_MESH_UNKNOWN;
        hasNormTex[0]       = 0;
        hasNormTex[1]       = 0;
        depthUberImage      = 8;
        sizeCoordTexFrame_0 = 0;
    }
    
    bool MESH_MBM::isLoaded() const
    {
        return this->buffer != nullptr;
    }
    
    bool MESH_MBM::render(const uint32_t indexFrame,const SHADER *pShader, const uint32_t idTexture1)
    {
        if (indexFrame < totalFramesMesh && buffer)
        {
            buffer[indexFrame].pBufferGL->idTexture1 = idTexture1;
            return pShader->render(buffer[indexFrame].pBufferGL);
        }
        return false;
    }
    
    bool MESH_MBM::renderDynamic(const uint32_t indexFrame, SHADER *pShader, VEC3 *vertex, VEC3 *normal,
                                    VEC2 *uv, const uint32_t idTexture1)
    {
        if (indexFrame < totalFramesMesh && buffer)
        {
            buffer[indexFrame].pBufferGL->idTexture1 = idTexture1;

            return pShader->renderDynamic(buffer[indexFrame].pBufferGL, vertex, normal, uv);
        }
        return false;
    }
    
    /*const bool drawSubset(    const uint32_t          indexFrame,
                                    std::vector<uint32_t>   &lsIndexSubset,
                                    SHADER*             pShader,
                                    const uint32_t          idTexture1)//Renderiza o frame indicado retorna true se
    foi possivel renderizar
    {
        if(indexFrame < totalFramesMesh && buffer && lsIndexSubset.size())
        {
            for(uint32_t i=0, s = lsIndexSubset.size(); i< s; ++i)
            {
                const uint32_t index = lsIndexSubset[i];
                if(index < buffer[indexFrame].totalSubset)
                {
                    buffer[indexFrame].pBufferGL->idTexture1 = idTexture1;
                    if(!pShader->drawSubset(buffer[indexFrame].pBufferGL,index))
                        return false;
                }
            }
            return true;
        }
        return false;
    }*/
    
    util::TYPE_MESH MESH_MBM::getTypeMesh() const
    {
        return typeMe;
    }
    
    VEC2 MESH_MBM::getZoomEditorSprite() const
    {
        return this->zoomEditorSprite;
    }
    
    uint32_t MESH_MBM::getTotalFrame() const
    {
        return totalFramesMesh;
    }
    
    uint32_t MESH_MBM::getTotalSubset(const uint32_t indexFrame) const
    {
        if (indexFrame < totalFramesMesh && buffer)
            return buffer[indexFrame].totalSubset;
        return 0;
    }
    
    MESH_MBM::MESH_MBM() noexcept
    {
        buffer          = nullptr;
        extraInfo       = nullptr;
        totalFramesMesh = 0;
        
        coordTexFrame_0     = nullptr;
        sizeCoordTexFrame_0 = 0;

        zoomEditorSprite.x  = 0;
        zoomEditorSprite.y  = 0;
        typeMe              = util::TYPE_MESH_UNKNOWN;
        hasNormTex[0]       = 0;
        hasNormTex[1]       = 0;
        depthUberImage      = 8;
    }
    
    bool MESH_MBM::load(const char *fileNamePath)
    {
        this->release();
        util::HEADER       headerMain;
        util::HEADER_MESH  headerMesh;
        FILE *                 fp             = util::openFile(fileNamePath, "rb");
#if defined USE_DEPRECATED_2_MINOR
        deprecated_mbm::INFO_SPRITE deprectedInfoSprite; // version <=SPRITE_INFO_VERSION_MBM_HEADER
#endif
        if (!fp)
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to open file [%s]", fileNamePath);
        fclose(fp);
        fp = nullptr;
        {
            MINIZ minz;
            char errorDesc[255]="";
            if (!minz.decompressFile(fileNamePath, util::getDecompressModelFileName(),errorDesc,sizeof(errorDesc)-1))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to uncompress file [%s]\n%s", fileNamePath,errorDesc);
        }
        fp = util::openFile(util::getDecompressModelFileName(), "rb");
        if (!fp)
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to open file [%s]", fileNamePath);
        this->fileName = fileNamePath;
        // step 1: Verificação do header  principal
        // -------------------------------------------------------------------------------
        if (!fread(&headerMain, sizeof(util::HEADER), 1, fp))
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header file [%s]", fileNamePath);
        if (strncmp(headerMain.name, "mbm", 3) == 0 &&
            (strncmp(headerMain.typeApp, "Mesh 3d mbm", 15) == 0 || // Mesh 3d normal
             strncmp(headerMain.typeApp, "User mbm", 15) == 0 ||    // user
             strncmp(headerMain.typeApp, "Font mbm", 15) == 0 ||    // Font
             strncmp(headerMain.typeApp, "Sprite mbm", 15) == 0 ||   // Sprite
             strncmp(headerMain.typeApp, "Shape mbm", 15) == 0 ||   // Shape
             strncmp(headerMain.typeApp, "Tile mbm", 15) == 0 ||   // Tile
             strncmp(headerMain.typeApp, "Particle mbm", 15) == 0 )) // Particle
        {
            if (strncmp(headerMain.typeApp, "Mesh 3d mbm", 15) == 0) // Mesh 3d normal
                typeMe = util::TYPE_MESH_3D;
            else if (strncmp(headerMain.typeApp, "User mbm", 15) == 0) // special -- user
                typeMe = util::TYPE_MESH_USER;
            else if (strncmp(headerMain.typeApp, "Font mbm", 15) == 0) // Font
                typeMe = util::TYPE_MESH_FONT;
            else if (strncmp(headerMain.typeApp, "Sprite mbm", 15) == 0) // Sprite mbm
                typeMe = util::TYPE_MESH_SPRITE;
            else if (strncmp(headerMain.typeApp, "Tile mbm", 15) == 0) // Tile mbm
                typeMe = util::TYPE_MESH_TILE_MAP;
            else if (strncmp(headerMain.typeApp, "Particle mbm", 15) == 0) // Particle mbm
                typeMe = util::TYPE_MESH_PARTICLE;
            else if (strncmp(headerMain.typeApp, "Shape mbm", 15) == 0) // Shape
                typeMe = util::TYPE_MESH_SHAPE;
        }
        else
        {
            char strTemp[255];
            sprintf(strTemp, "[%s] is not a mbm file!!\ntype of file: %s", fileNamePath, headerMain.typeApp);
            return log_util::onFailed(fp,__FILE__, __LINE__, strTemp);
        }
        if (headerMain.version < INITIAL_VERSION_MBM_HEADER || headerMain.version > CURRENT_VERSION_MBM_HEADER)
            return log_util::onFailed(fp,__FILE__, __LINE__, "incompatible version [%s] version [%d]", fileNamePath,headerMain.version);

        if(headerMain.version >= MODE_DRAW_VERSION_MBM_HEADER)
        {
            if (!fread(&info_mode, sizeof(util::INFO_DRAW_MODE), 1, fp))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info INFO_DRAW_MODE [%s]", fileNamePath);

            std::string which_mode;
            if(is_any_mode_valid(this->info_mode,which_mode) == false)
            {
                return log_util::onFailed(fp,__FILE__, __LINE__, "Invalid mode %s detected:[%s]",which_mode.c_str(), fileNamePath);
            }
        }
        if(typeMe == util::TYPE_MESH_TILE_MAP)
        {
            mbm::TEXTURE::enableFilter(false);
        }
        else
        {
            mbm::TEXTURE::enableFilter(true);
        }
        // step 2: --------------------------------------------------------------------------------------------------
        if (headerMain.version >= DETAIL_MESH_VERSION_MBM_HEADER)
        {
            util::DETAIL_MESH detailInfo;
            if (headerMain.version == DETAIL_MESH_VERSION_MBM_HEADER)
            {
                /* ************* DEPRECATED - Begin - old just here to compatibility ***************** */
                if (!fread(&detailInfo, sizeof(util::DETAIL_MESH), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info DETAIL_MESH [%s]", fileNamePath);
                if (detailInfo.type != 100 && detailInfo.type != 101) // script and shader until now
                    return log_util::onFailed(fp,__FILE__, __LINE__,"expected first DETAIL_MESH [%s] as size info extra information at version == DETAIL_MESH_VERSION_MBM_HEADER",fileNamePath);
                if (detailInfo.totalBounding)
                {
                    const auto extraInfoSize = static_cast<uint32_t>(detailInfo.totalBounding);
                    auto * extra     = new char[extraInfoSize];
                    if (!fread(extra, static_cast<size_t>(extraInfoSize), 1, fp))
                    {
                        delete [] extra;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read old and deprected extra info [%s]", fileNamePath);
                    }
                    delete [] extra;
                }
                /* ************* End - old just here to compatibility ***************** */
            }

            if (!fread(&detailInfo, sizeof(util::DETAIL_MESH), 1, fp))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info DETAIL_MESH [%s]", fileNamePath);
            if (headerMain.version == DETAIL_MESH_VERSION_MBM_HEADER)
            {
                if (detailInfo.type != 'H')
                    return log_util::onFailed(fp,__FILE__, __LINE__, "expected 'H' at DETAIL_MESH [%s]", fileNamePath);
            }
            else
            {
                if (detailInfo.type != 'P')
                    return log_util::onFailed(fp,__FILE__, __LINE__, "expected 'P' from Physics at DETAIL_MESH [%s]", fileNamePath);
            }
            for (int i = 0; i < detailInfo.totalBounding; )
            {
                util::DETAIL_MESH detail;
                if (!fread(&detail, sizeof(util::DETAIL_MESH), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read DETAIL_MESH [%s]", fileNamePath);
                switch (detail.type)
                {
                    case 1:
                    {
                        for(int j=0; j< detail.totalBounding; j++)
                        {
                            auto cube = new CUBE();
                            this->infoPhysics.lsCube.push_back(cube);
                            if (!fread(cube, sizeof(CUBE), 1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 2:
                    {
                        for(int j=0; j< detail.totalBounding; j++)
                        {
                            auto base = new SPHERE();
                            this->infoPhysics.lsSphere.push_back(base);
                            if (!fread(base, sizeof(SPHERE), 1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 3:
                    {
                        for(int j=0; j< detail.totalBounding; j++)
                        {
                            auto complex = new CUBE_COMPLEX();
                            this->infoPhysics.lsCubeComplex.push_back(complex);
                            if (!fread(complex, sizeof(CUBE_COMPLEX), 1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 4:
                    {
                        //Introduced position to the triangle
                        if(headerMain.version >= MODE_DRAW_VERSION_MBM_HEADER)
                        {
                            for(int j=0; j< detail.totalBounding; j++)
                            {
                                auto triangle = new TRIANGLE();
                                this->infoPhysics.lsTriangle.push_back(triangle);
                                if (!fread(triangle, sizeof(TRIANGLE), 1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                            }
                        }
                        else
                        {
                            for(int j=0; j< detail.totalBounding; j++)
                            {
                                auto triangle = new TRIANGLE();
                                this->infoPhysics.lsTriangle.push_back(triangle);
                                if (!fread(triangle, sizeof(TRIANGLE) - sizeof(VEC2) , 1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                            }
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 5:
                    {
                        auto *   infoFont = new INFO_BOUND_FONT();
                        this->extraInfo = infoFont;
                        util::DETAIL_HEADER_FONT headerFont;
                        if (!fread(&headerFont, sizeof(util::DETAIL_HEADER_FONT), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_HEADER_FONT [%s]",
                                                     fileNamePath);
                        auto strNameFont = new char[headerFont.sizeNameFonte];
                        if (!fread(strNameFont, static_cast<size_t>(headerFont.sizeNameFonte), 1, fp))
                        {
                            delete [] strNameFont;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load font's name [%s]", fileNamePath);
                        }
                        infoFont->fontName        = strNameFont;
                        infoFont->heightLetter    = headerFont.heightLetter;
                        infoFont->spaceXCharacter = headerFont.spaceXCharacter;
                        infoFont->spaceYCharacter = headerFont.spaceYCharacter;
                        delete[] strNameFont;
                        for (int j = 0; j < headerFont.totalDetailFont; ++j)
                        {
                            auto detailFont = new util::DETAIL_LETTER();
                            if (!fread(detailFont, sizeof(util::DETAIL_LETTER), 1, fp))
                            {
                                delete detailFont;
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER [%s]", fileNamePath);
                            }
                            if (detailFont->indexFrame >= headerFont.totalDetailFont)
                            {
                                delete detailFont;
                                return log_util::onFailed(
                                    fp, __FILE__,__LINE__, "failed to load DETAIL_LETTER!! frame out of bound [%s]", fileNamePath);
                            }
                            infoFont->letter[detailFont->letter].detail = detailFont;
                        }
                        i += 1;
                    }
                    break;
                    case 6:
                    {
                        auto* lsParticleInfo = new std::vector<util::STAGE_PARTICLE*>();
                        this->extraInfo = lsParticleInfo;
                        for (int j = 0; j< detail.totalBounding; j++)
                        {
                            auto stage = new util::STAGE_PARTICLE();
                            lsParticleInfo->push_back(stage);
                            if (!fread(stage, sizeof(util::STAGE_PARTICLE), 1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read stage particle [%s]", fileNamePath);
                        }
                        i += 1;
                    }
                    break;
                    case 7:
                    {
                        auto* infoTileMap = new util::BTILE_INFO();
                        this->extraInfo = infoTileMap;

                        if (!fread(&infoTileMap->map,  sizeof(util::BTILE_HEADER_MAP),1,fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read from  detail tile!");

                        if(infoTileMap->map.layerCount != static_cast<uint32_t>(detail.totalBounding))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "expected layerCount == totalBounding. [%d]!=[%d]",infoTileMap->map.layerCount,detail.totalBounding);

                        infoTileMap->infoBrickEditor = new util::BTILE_BRICK_INFO[infoTileMap->map.countRawTiles];
                        if (!fread(infoTileMap->infoBrickEditor,  sizeof(util::BTILE_BRICK_INFO) * infoTileMap->map.countRawTiles,1,fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read brick infor editor tile from  detail tile!");

                        const uint32_t tileCount = infoTileMap->map.count_height_tile * infoTileMap->map.count_width_tile;
                        infoTileMap->layers      = new util::BTILE_LAYER[infoTileMap->map.layerCount];
                        
                        for (uint32_t  j = 0; j < infoTileMap->map.layerCount; ++j)
                        {
                            if (!fread(infoTileMap->layers[j].offset,  sizeof(infoTileMap->layers[j].offset),1,fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read layer offset from tile!");

                            infoTileMap->layers[j].lsIndexTiles = new util::BTILE_INDEX_TILE[tileCount];

                            if (!fread(infoTileMap->layers[j].lsIndexTiles,  sizeof(util::BTILE_INDEX_TILE) * tileCount,1,fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read indexed tile from  detail tile!");
                        }

                        util::BTILE_DETAIL_HEADER detailObjsProperty;
                        if (!fread(&detailObjsProperty, sizeof(util::BTILE_DETAIL_HEADER),1,fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");

                        for(uint32_t j= 0 ; j < detailObjsProperty.totalObj; ++j)
                        {
                            util::BTILE_OBJ_HEADER objHeader;
                            if (!fread(&objHeader, sizeof(util::BTILE_OBJ_HEADER), 1,fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");
                            auto* obj = new util::BTILE_OBJ(static_cast<util::BTILE_OBJ_TYPE>(objHeader.type));
                            infoTileMap->lsObj.push_back(obj);
                            if(objHeader.sizeName > 0)
                            {
                                obj->name.resize(objHeader.sizeName);
                                auto * name = const_cast<char*>(obj->name.data());
                                if (!fread(name, objHeader.sizeName,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read name at detail tile header!");
                            }
                            for(uint32_t k= 0 ; k < objHeader.sizePoints; ++k)
                            {
                                auto* point = new VEC2();
                                obj->lsPoints.push_back(point);
                                if (!fread(point, sizeof(VEC2),1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");
                            }
                        }

                        for(uint32_t j= 0 ; j < detailObjsProperty.totalProperties; ++j)
                        {
                            util::BTILE_PROPERTY_HEADER propertyHeader;
                            if (!fread(&propertyHeader, sizeof(util::BTILE_PROPERTY_HEADER),1, fp))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail property tile header!");
                            auto* property = new util::BTILE_PROPERTY(static_cast<const util::BTILE_PROPERTY_TYPE>(propertyHeader.type));
                            infoTileMap->lsProperty.push_back(property);
                            if(propertyHeader.nameLength > 0)
                            {
                                property->name.resize(propertyHeader.nameLength);
                                auto * name = const_cast<char*>(property->name.data());
                                if (!fread(name,propertyHeader.nameLength,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property name at detail tile header!");
                            }
                            if(propertyHeader.valueLength > 0)
                            {
                                property->value.resize(propertyHeader.valueLength);
                                auto * value = const_cast<char*>(property->value.data());
                                if (!fread(value,propertyHeader.valueLength,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property value at detail tile header!");
                            }
                            if(propertyHeader.ownerLength > 0)
                            {
                                property->owner.resize(propertyHeader.ownerLength);
                                auto * owner = const_cast<char*>(property->owner.data());
                                if (!fread(owner,propertyHeader.ownerLength,1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property owner at detail tile header!");
                            }
                        }

                        i += 1;
                    }
                    break;
                    default:
                    {
                        return log_util::onFailed(fp,__FILE__, __LINE__, "unknown type bounding box [%d] [%s]", detail.type,
                                                 fileNamePath);
                    }
                }
            }
        }
#if defined USE_DEPRECATED_2_MINOR
        else
        {
            // step 2: --------------------------------------------------------------------------------------------------
            switch (this->typeMe)
            {
                case util::TYPE_MESH_3D:
                {
                    util::DETAIL_MESH detail;
                    // 2.1 Cubos -- Todos os boundings
                    // --------------------------------------------------------------------
                    if (!fread(&detail, sizeof(util::DETAIL_MESH), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load bounding box [%s]", fileNamePath);
                    if (detail.totalBounding)
                    {
                        switch (detail.type) // 1: Bounding box. 2: Esferico. 3: Cube poligono .
                        {
                            case 1:
                            {
                                for (int i = 0; i < detail.totalBounding; ++i)
                                {
                                    auto base = new CUBE();
                                    this->infoPhysics.lsCube.push_back(base);
                                    if (!fread(base, sizeof(CUBE), 1, fp))
                                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]",
                                                                 fileNamePath);
                                    ;
                                }
                            }
                            break;
                            case 2:
                            {
                                for (int i = 0; i < detail.totalBounding; ++i)
                                {
                                    auto base = new SPHERE();
                                    this->infoPhysics.lsSphere.push_back(base);
                                    if (!fread(base, sizeof(SPHERE), 1, fp))
                                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]",
                                                                 fileNamePath);
                                }
                            }
                            break;
                            case 3:
                            {
                                for (int i = 0; i < detail.totalBounding; ++i)
                                {
                                    auto complex = new CUBE_COMPLEX();
                                    this->infoPhysics.lsCubeComplex.push_back(complex);
                                    if (!fread(complex, sizeof(CUBE_COMPLEX), 1, fp))
                                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]",
                                                                 fileNamePath);
                                    ;
                                }
                            }
                            break;
                            default: { return log_util::onFailed(fp,__FILE__, __LINE__, "bounding unknown [%s]", fileNamePath);
                            }
                        }
                    }
                }
                break;
                case util::TYPE_MESH_SPRITE:
                {
                    if (!deprectedInfoSprite.readBoundingSprite(fp, fileNamePath, &this->zoomEditorSprite.x,
                                                                &this->zoomEditorSprite.y))
                        return false;
                }
                break;
                case util::TYPE_MESH_USER:
                {
                    // special -- user
                }
                break;
                case util::TYPE_MESH_FONT:
                {
                    auto *   infoFont = new INFO_BOUND_FONT();
                    this->extraInfo = infoFont;
                    util::DETAIL_HEADER_FONT headerFont;
                    if (!fread(&headerFont, sizeof(util::DETAIL_HEADER_FONT), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_HEADER_FONT [%s]", fileNamePath);
                    auto strNameFont = new char[headerFont.sizeNameFonte];
                    if (!fread(strNameFont, static_cast<size_t>(headerFont.sizeNameFonte), 1, fp))
                    {
                        delete [] strNameFont;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load font's name [%s]", fileNamePath);
                    }
                    infoFont->fontName        = strNameFont;
                    infoFont->heightLetter    = headerFont.heightLetter;
                    infoFont->spaceXCharacter = headerFont.spaceXCharacter;
                    infoFont->spaceYCharacter = headerFont.spaceYCharacter;
                    delete[] strNameFont;
                    for (int i = 0; i < headerFont.totalDetailFont; ++i)
                    {
                        auto detailFont = new util::DETAIL_LETTER();
                        if (!fread(detailFont, sizeof(util::DETAIL_LETTER), 1, fp))
                        {
                            delete detailFont;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER [%s]", fileNamePath);
                        }
                        if (detailFont->indexFrame >= headerFont.totalDetailFont)
                        {
                            delete detailFont;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_LETTER!! frame out of bound [%s]",
                                                     fileNamePath);
                        }
                        infoFont->letter[detailFont->letter].detail = detailFont;
                    }
                }
                break;
                default: {
                }
            }
        }
#else
        else
        {
            return log_util::onFailed(fp,__FILE__, __LINE__, "Imcompatible version [%d]", headerMain.version);
        }
#endif

        // 3 headerMesh MBM -------------------------------------------------------------------------------
        if (!fread(&headerMesh, sizeof(util::HEADER_MESH), 1, fp))
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read HEADER_MESH [%s]", fileNamePath);
        this->hasNormTex[0]     = headerMesh.hasNorText[0];
        this->hasNormTex[1]     = headerMesh.hasNorText[1];
        this->material.Ambient  = headerMesh.material.Ambient;
        this->material.Diffuse  = headerMesh.material.Diffuse;
        this->material.Emissive = headerMesh.material.Emissive;
        this->material.Specular = headerMesh.material.Specular;
        this->material.Power    = headerMesh.material.Power;
        if (headerMesh.totalAnimation == 0)
            return log_util::onFailed(fp,__FILE__, __LINE__, "there is no animation [%s]", fileNamePath);

        // 4 header anim -- Todas as animações -----------------------------------------------------------
        if (headerMain.version == INITIAL_VERSION_MBM_HEADER)
        {
#if defined USE_DEPRECATED_2_MINOR
            if (!deprecated_mbm::fillAnimation_1(fileNamePath, fp, &headerMesh, &this->infoAnimation))
                return false;
#else
            return log_util::onFailed(fp,__FILE__, __LINE__, "unexpected version [%s] V[%d]", fileNamePath, headerMain.version);
#endif
        }
        else
        {
            if (!this->fillAnimation_2(headerMesh, fileNamePath, fp))
                return false;
        }
#if defined USE_DEPRECATED_2_MINOR
        if (headerMain.version <= SPRITE_INFO_VERSION_MBM_HEADER)
        {
            deprectedInfoSprite.fillOldPhysicsSprite_2(this->typeMe, this->infoAnimation, headerMesh.totalFrames);
        }
#endif
        this->buffer          = new BUFFER_MESH[headerMesh.totalFrames];
        this->totalFramesMesh = static_cast<uint32_t>(headerMesh.totalFrames);

        // Loop principal atraves de todos os frames deste arquivo -----------------------------------------------
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            std::vector<uint32_t>  lsIdTexture;
            std::vector<uint8_t> lsHasColorKeying;
            // 5 Sequencia lógica dos frames --------------------------------------------------------------------------
            // Cada header Frame
            // --------------------------------------------------------------------------------------------------
            util::HEADER_FRAME headerFrame;
            if (!fread(&headerFrame, sizeof(util::HEADER_FRAME), 1, fp))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header of frame [%s]", fileNamePath);

            // 6 Todos os headers subset deste frame
            // -------------------------------------------------------------------------------
            util::HEADER_DESC_SUBSET headerDescSubset;
            buffer[currentFrame].subset      = new util::SUBSET[headerFrame.totalSubset];
            buffer[currentFrame].totalSubset = static_cast<uint32_t>(headerFrame.totalSubset);
            for (int i = 0; i < headerFrame.totalSubset; ++i)
            {
                if (!fread(&headerDescSubset, sizeof(util::HEADER_DESC_SUBSET), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header of subset [%s]", fileNamePath);
                buffer[currentFrame].subset[i].vertexStart = headerDescSubset.vertexStart;
                buffer[currentFrame].subset[i].indexStart  = headerDescSubset.indexStart;
                buffer[currentFrame].subset[i].indexCount  = headerDescSubset.indexCount;
                buffer[currentFrame].subset[i].vertexCount = headerDescSubset.vertexCount;
                if (strcmp(headerDescSubset.nameTexture, "default") != 0)
                {
                    char *pch = strchr(headerDescSubset.nameTexture, '#');
                    if (pch && pch[0] == '#' && pch[1] == 'u')
                    {
                        pch[0] = 0;
                        util::HEADER_IMG headerImg;
                        headerImg.lenght = 0;
                        if (!fread(&headerImg, sizeof(util::HEADER_IMG), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header image [%s]", fileNamePath);
                        if (headerImg.lenght == 0)
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header image [%s]", fileNamePath);
                        auto data = new uint8_t[headerImg.lenght];
                        if (!fread(data, static_cast<size_t>(headerImg.lenght), 1, fp))
                        {
                            delete [] data;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read image [%s]", fileNamePath);
                        }
                        uint32_t sizeOfImage = 0;
                        this->depthUberImage     = static_cast<uint8_t>(headerImg.depth);
                        if (headerImg.channel != 4 && headerImg.channel != 3 && headerImg.channel != 0)
                        {
                            delete [] data;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read image! Channel != [3 || 4] [%s]",
                                                     fileNamePath);
                        }
                        headerImg.channel = headerImg.channel == 4 ? 4 : 3;
                        switch (headerImg.depth)
                        {
                            case 3:
                            {
                                sizeOfImage = 3 * headerImg.channel * headerImg.width * headerImg.height;
                                while (sizeOfImage % 8)
                                {
                                    sizeOfImage++;
                                }
                                sizeOfImage = sizeOfImage / 8;
                            }
                            break;
                            case 4:
                            {

                                sizeOfImage = 4 * headerImg.channel * headerImg.width * headerImg.height;
                                while (sizeOfImage % 8)
                                {
                                    sizeOfImage++;
                                }
                                sizeOfImage = sizeOfImage / 8;
                            }
                            break;
                            case 8: { sizeOfImage = headerImg.width * headerImg.height * headerImg.channel;}
                            break;
                            default: 
                            { 
                                delete [] data;
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read image [%s]", fileNamePath);
                            }
                        }
                        MINIZ miniz;
                        if (miniz.decompressStream(data, headerImg.lenght, sizeOfImage))
                        {
                            delete[] data;
                            TEXTURE_MANAGER *textureManager = TEXTURE_MANAGER::getInstance();
                            buffer[currentFrame].subset[i].texture = textureManager->load(
                                headerImg.width, headerImg.height, miniz.getDataStreamOut(), headerDescSubset.nameTexture,
                                headerImg.depth, headerImg.channel, headerImg.hasAlpha ? true : false);
                            if (!buffer[currentFrame].subset[i].texture)
                            {
                                lsIdTexture.push_back(0);
                                lsHasColorKeying.push_back(0);
#if defined _DEBUG
                                PRINT_IF_DEBUG( "error on creating texture: %s", fileName.c_str());
#endif
                            }
                            else
                            {
                                lsIdTexture.push_back(buffer[currentFrame].subset[i].texture->idTexture);
                                lsHasColorKeying.push_back(buffer[currentFrame].subset[i].texture->useAlphaChannel ? 1: 0);
                            }
                        }
                        else
                        {
                            delete[] data;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to uncompress file [%s]", fileNamePath);
                        }
                    }
                    else
                    {
                        pch = strchr(headerDescSubset.nameTexture, '#');
                        if (pch && pch[0] == '#' && pch[1] == 'M') // Material apenas .. cor como string
                        {
                            pch = &pch[2];
                            //util::MATERIAL mat;
                            //memset(&mat, 0, sizeof(mat));
                            std::vector<std::string> result;
                            util::split(result, pch, '|');

                            if (result.size() == 5)
                            {
                                COLOR colorAculm(0.0f, 0.0f, 0.0f, 0.0f);
                                int        totalSum = 0;
                                for (auto & n : result)
                                {
                                    const char *strTemp   = n.c_str();
                                    //char        letter    = strTemp[0];
                                    const char *strNumber = &strTemp[1];
                                    COLOR  color     = static_cast<uint32_t>(strtol(strNumber, nullptr, 16));
                                    if (color.r > 0.0f || color.g > 0.0f || color.b > 0.0f)
                                    {
                                        colorAculm.r += color.r;
                                        colorAculm.g += color.g;
                                        colorAculm.b += color.b;
                                        totalSum++;
                                    }
                                    //switch (letter)
                                    //{
                                    //    case 'A': { mat.Ambient = color;
                                    //    }
                                    //    break;
                                    //    case 'D': { mat.Diffuse = color;
                                    //    }
                                    //    break;
                                    //    case 'E': { mat.Emissive = color;
                                    //    }
                                    //    break;
                                    //    case 'S': { mat.Specular = color;
                                    //    }
                                    //    break;
                                    //    case 'P': { mat.Power = static_cast<float>(atof(strNumber));
                                    //    }
                                    //    break;
                                    //    default: break;
                                    //}
                                }
                                if (totalSum)
                                { // potências de 2 (2,4,8,16,32,64,128,256, 512,1024)...
                                    TEXTURE_MANAGER *textureManager = TEXTURE_MANAGER::getInstance();
                                    colorAculm.r = colorAculm.r / static_cast<float>(totalSum);
                                    colorAculm.g = colorAculm.g / static_cast<float>(totalSum);
                                    colorAculm.b = colorAculm.b / static_cast<float>(totalSum);
                                    uint8_t dataARGB[16];
                                    uint32_t  dwR =
                                        colorAculm.r >= 1.0f
                                            ? 0xff
                                            : colorAculm.r <= 0.0f ? 0x00 : static_cast<uint32_t>(colorAculm.r * 255.0f + 0.5f);
                                    uint32_t dwG =
                                        colorAculm.g >= 1.0f
                                            ? 0xff
                                            : colorAculm.g <= 0.0f ? 0x00 : static_cast<uint32_t>(colorAculm.g * 255.0f + 0.5f);
                                    uint32_t dwB =
                                        colorAculm.b >= 1.0f
                                            ? 0xff
                                            : colorAculm.b <= 0.0f ? 0x00 : static_cast<uint32_t>(colorAculm.b * 255.0f + 0.5f);
                                    for (int pixel = 0; pixel < 12; pixel += 4)
                                    {
                                        dataARGB[pixel]     = static_cast<uint8_t>(255);
                                        dataARGB[pixel + 1] = static_cast<uint8_t>(dwR);
                                        dataARGB[pixel + 2] = static_cast<uint8_t>(dwG);
                                        dataARGB[pixel + 3] = static_cast<uint8_t>(dwB);
                                    }
                                    buffer[currentFrame].subset[i].texture =
                                        textureManager->load(2, 2, dataARGB, headerDescSubset.nameTexture, 8, 4);
                                    if (buffer[currentFrame].subset[i].texture)
                                    {
                                        lsIdTexture.push_back(buffer[currentFrame].subset[i].texture->idTexture);
                                        lsHasColorKeying.push_back(
                                            buffer[currentFrame].subset[i].texture->useAlphaChannel ? 1 : 0);
                                    }
                                    else
                                    {
                                        lsIdTexture.push_back(0);
                                        lsHasColorKeying.push_back(0);
                                    }
                                }
                            }
                            else
                            {
                                buffer[currentFrame].subset[i].texture = nullptr;
                                lsIdTexture.push_back(0);
                                lsHasColorKeying.push_back(0);
                            }
                        }
                        else if(this->getInfoFont() != nullptr)
                        {
                            TEXTURE_MANAGER *textureManager = TEXTURE_MANAGER::getInstance();
                            const INFO_BOUND_FONT* pInfoFont = this->getInfoFont();
                            const size_t len = strlen(headerDescSubset.nameTexture);
                            if(len > 4 && strcasecmp(&headerDescSubset.nameTexture[len-4],".ttf")==0)
                            {
                                buffer[currentFrame].subset[i].texture = textureManager->loadTTF(headerDescSubset.nameTexture,nullptr,nullptr,pInfoFont->heightLetter,true);
                            }
                            else
                            {
                                buffer[currentFrame].subset[i].texture = textureManager->load(
#if defined USE_DEPRECATED_2_MINOR
                                    headerDescSubset.nameTexture, true);
                                    headerDescSubset.hasAlphaColor = 1;
#else
                                    headerDescSubset.nameTexture, headerDescSubset.hasAlphaColor ? true : false);
#endif
                            }
                            if (!buffer[currentFrame].subset[i].texture)
                            {
                                lsIdTexture.push_back(0);
                                lsHasColorKeying.push_back(0);
                            }
                            else
                            {
                                lsIdTexture.push_back(buffer[currentFrame].subset[i].texture->idTexture);
                                lsHasColorKeying.push_back(buffer[currentFrame].subset[i].texture->useAlphaChannel ? 1: 0);
                            }
                        }
                        else
                        {
                            TEXTURE_MANAGER *textureManager = TEXTURE_MANAGER::getInstance();
                            buffer[currentFrame].subset[i].texture = textureManager->load(
                                headerDescSubset.nameTexture, headerDescSubset.hasAlphaColor ? true : false);
                            if (!buffer[currentFrame].subset[i].texture)
                            {
                                lsIdTexture.push_back(0);
                                lsHasColorKeying.push_back(0);
                            }
                            else
                            {
                                lsIdTexture.push_back(buffer[currentFrame].subset[i].texture->idTexture);
                                lsHasColorKeying.push_back(buffer[currentFrame].subset[i].texture->useAlphaChannel ? 1: 0);
                            }
                        }
                    }
                }
            }

            // 6.2 Vertex buffer e index buffer
            // -----------------------------------------------------------------------------------------
            if (headerFrame.sizeIndexBuffer && strcmp(headerFrame.typeBuffer, "IB") == 0)
            {
                auto indexArray = new uint16_t[headerFrame.sizeIndexBuffer];
                if (!fread(indexArray, sizeof(uint16_t) * static_cast<size_t>(headerFrame.sizeIndexBuffer), 1, fp))
                {
                    delete[] indexArray;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read index buffer of frame [%s]", fileNamePath);
                }
                VEC3 *pPosition = nullptr;
                VEC3 *pNormal   = nullptr;
                VEC2 *pTexture  = nullptr;
                if (!this->loadFromSplited(fp, headerFrame.sizeVertexBuffer, &pPosition, &pNormal, &pTexture,
                                           headerMesh.hasNorText, indexArray, headerFrame.sizeIndexBuffer,
                                           headerFrame.stride))
                {
                    delete[] indexArray;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex buffer of frame [%s]", fileNamePath);
                }
                buffer[currentFrame].pBufferGL = new BUFFER_GL();
                /*if(this->is3d == false)
                {
                    //Não precisamos mais utilizar isto pois ja estamos gravando invertido no directx
                    this->invertMap(false,true,pTexture,headerFrame.sizeVertexBuffer);
                }*/
                auto indexStart = new int[buffer[currentFrame].totalSubset];
                auto indexCount = new int[buffer[currentFrame].totalSubset];
                for (int subIndex = 0; subIndex < headerFrame.totalSubset; ++subIndex)
                {
                    indexStart[subIndex] = buffer[currentFrame].subset[subIndex].indexStart;
                    indexCount[subIndex] = buffer[currentFrame].subset[subIndex].indexCount;
                }
                if (!buffer[currentFrame].pBufferGL->loadBuffer(pPosition, pNormal, pTexture,
                                                                static_cast<uint32_t>(headerFrame.sizeVertexBuffer), indexArray,
                                                                buffer[currentFrame].totalSubset, indexStart, indexCount,&this->info_mode))

                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    delete[] indexArray;
                    delete[] indexStart;
                    delete[] indexCount;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "error on load buffer bufferTriangleList [%s]", fileNamePath);
                }
#if defined USE_DEPRECATED_2_MINOR
                if (headerMain.version <= SPRITE_INFO_VERSION_MBM_HEADER)
                    deprectedInfoSprite.fillPhysicsSprite(pPosition, static_cast<uint32_t>(currentFrame), buffer[currentFrame].subset,
                                                          this->typeMe, this->infoPhysics.lsCube,
                                                          this->infoPhysics.lsSphere, this->infoPhysics.lsTriangle);
#endif
                delete[] pPosition;
                delete[] pNormal;
                delete[] pTexture;
                delete[] indexArray;
                delete[] indexStart;
                delete[] indexCount;
            }
            // 6.3 Vertex Buffer somente
            // ----------------------------------------------------------------------------------------------
            else if (strcmp(headerFrame.typeBuffer, "VB") == 0)
            {
                VEC3 *pPosition = nullptr;
                VEC3 *pNormal   = nullptr;
                VEC2 *pTexture  = nullptr;
                if (!this->loadFromSplited(fp, headerFrame.sizeVertexBuffer, &pPosition, &pNormal, &pTexture,
                                           headerMesh.hasNorText, nullptr, 0, headerFrame.stride))
                {
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex buffer of frame [%s]", fileNamePath);
                }
                buffer[currentFrame].pBufferGL = new BUFFER_GL();
                auto vertexStart               = new int[buffer[currentFrame].totalSubset];
                auto vertexCount               = new int[buffer[currentFrame].totalSubset];
                for (int subIndex = 0; subIndex < headerFrame.totalSubset; ++subIndex)
                {
                    vertexStart[subIndex] = buffer[currentFrame].subset[subIndex].vertexStart;
                    vertexCount[subIndex] = buffer[currentFrame].subset[subIndex].vertexCount;
                }
                if (!buffer[currentFrame].pBufferGL->loadBuffer(
                        pPosition, pNormal, pTexture, static_cast<uint32_t>(headerFrame.sizeVertexBuffer), buffer[currentFrame].totalSubset,
                        vertexStart, vertexCount,&this->info_mode))
                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "error on load buffer bufferTriangleList [%s]", fileNamePath);
                }
#if defined USE_DEPRECATED_2_MINOR
                if (headerMain.version <= SPRITE_INFO_VERSION_MBM_HEADER)
                    deprectedInfoSprite.fillPhysicsSprite(pPosition, static_cast<uint32_t>(currentFrame), buffer[currentFrame].subset,
                                                          this->typeMe, this->infoPhysics.lsCube,
                                                          this->infoPhysics.lsSphere, this->infoPhysics.lsTriangle);
#endif
                delete[] pPosition;
                delete[] pNormal;
                delete[] pTexture;
            }
            else
            {
                return log_util::onFailed(fp,__FILE__, __LINE__, "unknown buffer type [%s]", fileNamePath);
            }
            const std::vector<int>::size_type  totalIdTexture = ((buffer[currentFrame].pBufferGL->totalSubset > lsIdTexture.size())
                                                     ? lsIdTexture.size()
                                                     : buffer[currentFrame].pBufferGL->totalSubset);
            for (std::vector<int>::size_type i = 0; i < totalIdTexture; ++i)
            {
                buffer[currentFrame].pBufferGL->idTexture0[i] = lsIdTexture[i];
                buffer[currentFrame].pBufferGL->useAlpha[i]   = lsHasColorKeying[i];
            }
        }
        fclose(fp);
        fp             = nullptr;
        positionOffset = VEC3(headerMesh.posX, headerMesh.posY, headerMesh.posZ);
        angleDefault   = VEC3(headerMesh.angleX, headerMesh.angleY, headerMesh.angleZ);
        remove(util::getDecompressModelFileName());

        this->sizeCoordTexFrame_0 = 0;
        if (this->coordTexFrame_0)
            delete[] this->coordTexFrame_0;
        this->coordTexFrame_0 = nullptr;
        return true;
    }
    
    void MESH_MBM::invertMap(const bool u, const bool v, VEC2 *pTexture, const uint32_t arraySize)
    {
        float maxU = -FLT_MAX;
        float maxV = -FLT_MAX;
        float minU = FLT_MAX;
        float minV = FLT_MAX;
        for (uint32_t k = 0; k < arraySize; ++k)
        {
            if (pTexture[k].x > maxU)
                maxU = pTexture[k].x;
            if (pTexture[k].y > maxV)
                maxV = pTexture[k].y;

            if (pTexture[k].x < minU)
                minU = pTexture[k].x;
            if (pTexture[k].y < minV)
                minV = pTexture[k].y;
        }
        const float diffU = maxU - minU;
        const float diffV = maxV - minV;
        for (uint32_t k = 0; k < arraySize; ++k)
        {
            if (u)
            {
                float perc    = (pTexture[k].x - minU) / diffU;
                pTexture[k].x = ((1.0f - perc) * diffU) + minU;
            }
            if (v)
            {
                float perc    = (pTexture[k].y - minV) / diffV;
                pTexture[k].y = ((1.0f - perc) * diffV) + minV;
            }
        }
    }
    
    bool MESH_MBM::loadFromSplited(FILE *fp, const int sizeVertexBuffer, VEC3 **positionOut,
                                VEC3 **normalOut, VEC2 **textureOut, int16_t hasNorText[2],
                                uint16_t *indexArray, const int sizeArrayIndex, const int stride)
    {
        auto pPosition = new VEC3[sizeVertexBuffer];
        auto pNormal   = new VEC3[sizeVertexBuffer];
        auto pTexture  = new VEC2[sizeVertexBuffer];
        *positionOut            = pPosition;
        *normalOut              = pNormal;
        *textureOut             = pTexture;
        if (stride == 3)
        {
            if (!fread(pPosition, sizeof(VEC3) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
            {
                delete[] pPosition;
                delete[] pNormal;
                delete[] pTexture;
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex");
            }
            if (hasNorText[0])
            {
                if (!fread(pNormal, sizeof(VEC3) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex");
                }
            }
            else
            {
                if (indexArray && sizeArrayIndex)
                {
                    for (int i = 0; i < sizeArrayIndex; i += 3)
                    {
                        const int index0 = indexArray[i];
                        const int index1 = indexArray[i + 1];
                        const int index2 = indexArray[i + 2];
                        if (index0 >= sizeVertexBuffer || index1 >= sizeVertexBuffer || index2 >= sizeVertexBuffer)
                        {
                            delete[] pPosition;
                            delete[] pNormal;
                            delete[] pTexture;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "inconsistent index to normal");
                        }
                        VEC3 a(pPosition[index1] - pPosition[index0]);
                        VEC3 b(pPosition[index2] - pPosition[index1]);
                        vec3Cross(&pNormal[index0], &a, &b);
                        vec3Normalize(&pNormal[index0], &pNormal[index0]);
                        pNormal[index1] = pNormal[index0];
                        pNormal[index2] = pNormal[index0];
                    }
                }
                else
                {
                    for (int i = 0; i < sizeVertexBuffer; i += 3)
                    {
                        VEC3 a(pPosition[i + 1] - pPosition[i]);
                        VEC3 b(pPosition[i + 2] - pPosition[i + 1]);
                        vec3Cross(&pNormal[i], &a, &b);
                        vec3Normalize(&pNormal[i], &pNormal[i]);
                        pNormal[i + 1] = pNormal[i];
                        pNormal[i + 2] = pNormal[i];
                    }
                }
            }
            if (hasNorText[1] == 1) // As coordenadas estão presentes em cada frame
            {
                if (!fread(pTexture, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data uv");
                }
            }
            else if (hasNorText[1] == 2) // As coordenadas só estão presentes no primeiro frame
            {
                if (this->coordTexFrame_0) // Ja passou pelo primeiro frame, então só copia
                {
                    if (sizeVertexBuffer != this->sizeCoordTexFrame_0)
                        memset(static_cast<void*>(pTexture), 0, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer));
                    int safeCopy = std::min(sizeVertexBuffer, this->sizeCoordTexFrame_0);
                    memcpy(static_cast<void*>(pTexture), this->coordTexFrame_0, sizeof(VEC2) * static_cast<size_t>(safeCopy));
                }
                else //É o primeiro frame então guarda as coordenadas
                {
                    this->coordTexFrame_0 = new VEC2[sizeVertexBuffer];
                    this->sizeCoordTexFrame_0 = sizeVertexBuffer;
                    if (!fread(pTexture, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
                    {
                        delete[] pPosition;
                        delete[] pNormal;
                        delete[] pTexture;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data uv");
                    }
                    memcpy(static_cast<void*>(this->coordTexFrame_0), pTexture, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer));
                }
            }
            else
            {
                for (int i = 0, j = 0; i < sizeVertexBuffer; i += 3, ++j)
                {
                    if (j % 2)
                    {
                        pTexture[i].x = 0;
                        pTexture[i].y = 1;

                        pTexture[i + 1].x = 0;
                        pTexture[i + 1].y = 0;

                        pTexture[i + 2].x = 1;
                        pTexture[i + 2].y = 1;
                    }
                    else
                    {
                        pTexture[i].x = 1;
                        pTexture[i].y = 1;

                        pTexture[i + 1].x = 0;
                        pTexture[i + 1].y = 0;

                        pTexture[i + 2].x = 1;
                        pTexture[i + 2].y = 0;
                    }
                }
            }
            return true;
        }
        else if (stride == 2)
        {
            auto pStridePosition = new VEC2[sizeVertexBuffer];
            if (!fread(pStridePosition, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
            {
                delete[] pPosition;
                delete[] pNormal;
                delete[] pTexture;
                delete[] pStridePosition;
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex");
            }
            for (int i = 0; i < sizeVertexBuffer; ++i)
            {
                pPosition[i].x = pStridePosition[i].x;
                pPosition[i].y = pStridePosition[i].y;
                pPosition[i].z = 0.0f;
            }
            delete[] pStridePosition;
            pStridePosition = nullptr;
            if (hasNorText[0])
            {
                if (!fread(pNormal, sizeof(VEC3) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read vertex");
                }
            }
            else
            {
                if (indexArray && sizeArrayIndex)
                {
                    for (int i = 0; i < sizeArrayIndex; i += 3)
                    {
                        const int index0 = indexArray[i];
                        const int index1 = indexArray[i + 1];
                        const int index2 = indexArray[i + 2];
                        if (index0 >= sizeVertexBuffer || index1 >= sizeVertexBuffer || index2 >= sizeVertexBuffer)
                        {
                            delete[] pPosition;
                            delete[] pNormal;
                            delete[] pTexture;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "inconsistent index to normal");
                        }
                        VEC3 a(pPosition[index1] - pPosition[index0]);
                        VEC3 b(pPosition[index2] - pPosition[index1]);
                        vec3Cross(&pNormal[index0], &a, &b);
                        vec3Normalize(&pNormal[index0], &pNormal[index0]);
                        pNormal[index1] = pNormal[index0];
                        pNormal[index2] = pNormal[index0];
                    }
                }
                else
                {
                    for (int i = 0; i < sizeVertexBuffer; i += 3)
                    {
                        VEC3 a(pPosition[i + 1] - pPosition[i]);
                        VEC3 b(pPosition[i + 2] - pPosition[i + 1]);
                        vec3Cross(&pNormal[i], &a, &b);
                        vec3Normalize(&pNormal[i], &pNormal[i]);
                        pNormal[i + 1] = pNormal[i];
                        pNormal[i + 2] = pNormal[i];
                    }
                }
            }
            if (hasNorText[1] == 1) // As coordenadas estão presentes em cada frame
            {
                if (!fread(pTexture, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
                {
                    delete[] pPosition;
                    delete[] pNormal;
                    delete[] pTexture;
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data uv");
                }
            }
            else if (hasNorText[1] == 2) // As coordenadas só estão presentes no primeiro frame
            {
                if (this->coordTexFrame_0) // Ja passou pelo primeiro frame, então só copia
                {
                    if (sizeVertexBuffer != this->sizeCoordTexFrame_0) 
                        memset(static_cast<void*>(pTexture), 0, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer));
                    int safeCopy = std::min(sizeVertexBuffer, this->sizeCoordTexFrame_0);
                    memcpy(static_cast<void*>(pTexture), this->coordTexFrame_0, sizeof(VEC2) * static_cast<size_t>(safeCopy));
                }
                else //É o primeiro frame então guarda as coordenadas
                {
                    this->coordTexFrame_0 = new VEC2[sizeVertexBuffer];
                    this->sizeCoordTexFrame_0 = sizeVertexBuffer;
                    if (!fread(pTexture, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer), 1, fp))
                    {
                        delete[] pPosition;
                        delete[] pNormal;
                        delete[] pTexture;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data uv");
                    }
                    memcpy(static_cast<void*>(this->coordTexFrame_0), pTexture, sizeof(VEC2) * static_cast<size_t>(sizeVertexBuffer));
                }
            }
            else
            {
                for (int i = 0, j = 0; i < sizeVertexBuffer; i += 3, ++j)
                {
                    if (j % 2)
                    {
                        pTexture[i].x = 0;
                        pTexture[i].y = 1;

                        pTexture[i + 1].x = 0;
                        pTexture[i + 1].y = 0;

                        pTexture[i + 2].x = 1;
                        pTexture[i + 2].y = 1;
                    }
                    else
                    {
                        pTexture[i].x = 1;
                        pTexture[i].y = 1;

                        pTexture[i + 1].x = 0;
                        pTexture[i + 1].y = 0;

                        pTexture[i + 2].x = 1;
                        pTexture[i + 2].y = 0;
                    }
                }
            }
            return true;
        }
        else
        {
            return log_util::onFailed(fp,__FILE__, __LINE__, "stride unknown. must be 2 or 3");
        }
    }
    
    bool MESH_MBM::fillAnimation_2(util::HEADER_MESH &headerMesh, const char *fileNamePath, FILE *fp)
    {
        for (int i = 0; i < headerMesh.totalAnimation; ++i)
        {
            auto headerAnim = new util::HEADER_ANIMATION();
            if (!fread(headerAnim, sizeof(util::HEADER_ANIMATION), 1, fp))
            {
                delete headerAnim;
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load animation 's mesh [%s]", fileNamePath);
            }
            auto infoHead = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            this->infoAnimation.lsHeaderAnim.push_back(infoHead);
            infoHead->headerAnim = headerAnim;

            if(headerAnim->hasShaderEffect == 1)
            {
                infoHead->effetcShader = new util::INFO_FX();
                util::HEADER_INFO_SHADER_STEP headerPS_VS;
                if (!fread(&headerPS_VS, sizeof(util::HEADER_INFO_SHADER_STEP), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header step [%s]", fileNamePath);
                if(headerPS_VS.blendOperation != 0)
                    infoHead->effetcShader->blendOperation = headerPS_VS.blendOperation;
                else
                    infoHead->effetcShader->blendOperation = 1;
                if (headerPS_VS.lenNameShader) // temos pixel shader
                {
                    auto dataInfo = new util::INFO_SHADER_DATA(headerPS_VS.sizeArrayVarInBytes, static_cast<short>(headerPS_VS.lenNameShader),static_cast<short>(headerPS_VS.lenTextureStage2));
                    infoHead->effetcShader->blendOperation = headerPS_VS.blendOperation;
                    infoHead->effetcShader->dataPS     = (dataInfo);
                    dataInfo->typeAnimation    = headerPS_VS.typeAnimation;
                    dataInfo->timeAnimation    = headerPS_VS.timeAnimation;
                    if (!fread(dataInfo->fileNameShader, static_cast<size_t>(headerPS_VS.lenNameShader), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read shader's name [%s]", fileNamePath);

                    if (headerPS_VS.lenTextureStage2)
                    {
                        if (!fread(dataInfo->fileNameTextureStage2, static_cast<size_t>(headerPS_VS.lenTextureStage2), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read name's texture [%s]", fileNamePath);
                    }
                    if (headerPS_VS.sizeArrayVarInBytes)
                    {
                        if (!fread(dataInfo->typeVars, static_cast<size_t>(dataInfo->lenVars), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                        if (!fread(dataInfo->min, sizeof(float) * static_cast<size_t>(dataInfo->lenVars) * 4, 1,
                                   fp)) // sempre lemos de 4 em 4 floats
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                        if (!fread(dataInfo->max, sizeof(float) * static_cast<size_t>(dataInfo->lenVars) * 4, 1,
                                   fp)) // sempre lemos de 4 em 4 floats
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                    }
                }
                if (!fread(&headerPS_VS, sizeof(util::HEADER_INFO_SHADER_STEP), 1, fp))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read header step [%s]", fileNamePath);
                if (headerPS_VS.lenNameShader) // temos vertex shader
                {
                    auto dataInfo = new util::INFO_SHADER_DATA(headerPS_VS.sizeArrayVarInBytes, static_cast<short>(headerPS_VS.lenNameShader),static_cast<short>(headerPS_VS.lenTextureStage2));
                    infoHead->effetcShader->blendOperation = headerPS_VS.blendOperation;
                    infoHead->effetcShader->dataVS     = (dataInfo);
                    dataInfo->typeAnimation    = headerPS_VS.typeAnimation;
                    dataInfo->timeAnimation    = headerPS_VS.timeAnimation;
                    if (!fread(dataInfo->fileNameShader, static_cast<size_t>(headerPS_VS.lenNameShader), 1, fp))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read shader's name [%s]", fileNamePath);

                    if (headerPS_VS.lenTextureStage2)
                    {
                        if (!fread(dataInfo->fileNameTextureStage2, static_cast<size_t>(headerPS_VS.lenTextureStage2), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read name's texture [%s]", fileNamePath);
                    }
                    if (headerPS_VS.sizeArrayVarInBytes)
                    {
                        if (!fread(dataInfo->typeVars, static_cast<size_t>(dataInfo->lenVars), 1, fp))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                        if (!fread(dataInfo->min, sizeof(float) * static_cast<size_t>(dataInfo->lenVars) * 4, 1,
                                   fp)) // sempre lemos de 4 em 4 floats
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                        if (!fread(dataInfo->max, sizeof(float) * static_cast<size_t>(dataInfo->lenVars) * 4, 1,
                                   fp)) // sempre lemos de 4 em 4 floats
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read data shader [%s]", fileNamePath);
                    }
                }
            }
        }
        return true;
    }

    const INFO_BOUND_FONT* MESH_MBM::getInfoFont()const
    {
        if(this->typeMe == util::TYPE_MESH_FONT)
            return static_cast<INFO_BOUND_FONT*>(this->extraInfo);
        return nullptr;
    }

    const std::vector<util::STAGE_PARTICLE*>* MESH_MBM::getInfoParticle()const
    {
        if(this->typeMe == util::TYPE_MESH_PARTICLE)
            return static_cast<std::vector<util::STAGE_PARTICLE*>*>(this->extraInfo);
        return nullptr;
    }

    const util::BTILE_INFO* MESH_MBM::getInfoTile()const
    {
        if(this->typeMe == util::TYPE_MESH_TILE_MAP)
            return static_cast<util::BTILE_INFO*>(this->extraInfo);
        return nullptr;
    }

        API_IMPL const util::DYNAMIC_SHAPE* MESH_MBM::getInfoShape()const
        {
            if(this->typeMe == util::TYPE_MESH_SHAPE)
                 return static_cast<util::DYNAMIC_SHAPE*>(this->extraInfo);
            return nullptr;
        }

    MESH_MANAGER * MESH_MANAGER::getInstance()
    {
        if (instanceMeshManager == nullptr)
        {
            instanceMeshManager = new MESH_MANAGER();
        }
        return instanceMeshManager;
    }
    
    void MESH_MANAGER::release()
    {
        if (instanceMeshManager)
            delete instanceMeshManager;
        instanceMeshManager = nullptr;
    }
    
    void MESH_MANAGER::fakeRelease(const char* fileName)
    {
        const std::string fileNameBase = util::getBaseName(fileName);
        MESH_MBM* ptr = this->lsMeshes[fileNameBase];
        if(ptr)
        {
            this->lsFakeRelease.push_back(ptr);
            this->lsMeshes[fileNameBase] = nullptr;
        }
    }

    MESH_MBM * MESH_MANAGER::getIfExists(const char* fileName)
    {
        std::string fileNameBase = util::getBaseName(fileName);
        auto mesh = lsMeshes[fileNameBase];
        return mesh;
    }
    
    MESH_MBM * MESH_MANAGER::load(const char *fileName)
    {
        std::string fileNameBase = util::getBaseName(fileName);
        auto mesh = this->lsMeshes[fileNameBase];
        if(mesh)
            return mesh;
        mesh = new MESH_MBM();
        if (mesh->load(fileName))
        {
            lsMeshes[fileNameBase] = mesh;
            return mesh;
        }
        else
        {
            delete mesh;
#if defined _DEBUG
            PRINT_IF_DEBUG( "failed to load mesh");
#endif
            return nullptr;
        }
    }
    
    MESH_MBM * MESH_MANAGER::loadTrueTypeFont(const char *fileNameTtf, const float heightLetter, const short spaceWidth,
                                      const short spaceHeight,const bool saveTextureAsPng, TEXTURE ** texture_loaded)
    {
        if (fileNameTtf == nullptr)
        {
#if defined _DEBUG
            PRINT_IF_DEBUG( "filename null.");
#endif
            return nullptr;
        }

        auto fillvertexQuadTrueFont = [](VEC3 *_position, const float width, const float height, const float diffY) -> void
        {
            const float x  = width * 0.5f;
            const float y  = height * 0.5f;
            _position[0].x = -x;
            _position[0].y = -y - diffY;
            _position[0].z = 0;

            _position[1].x = -x;
            _position[1].y = y - diffY;
            _position[1].z = 0;

            _position[2].x = x;
            _position[2].y = -y - diffY;
            _position[2].z = 0;

            _position[3].x = x;
            _position[3].y = y - diffY;
            _position[3].z = 0;
        };

        std::string fileNameBase = util::getBaseName(fileNameTtf);
        char measure[255]="";
        snprintf(measure,sizeof(measure),"%0.2f|%d|%d#",heightLetter,spaceWidth,spaceHeight);
        std::string fileNameBaseSuppose(measure);
        fileNameBaseSuppose += fileNameBase;
        auto mesh = lsMeshes[fileNameBaseSuppose];
        if(mesh)
            return mesh;
        mesh = new MESH_MBM();
        std::vector<stbtt_aligned_quad *> lsStbFont;
        std::vector<VEC2>                 lsWidthLetter;

        TEXTURE *texture = TEXTURE_MANAGER::getInstance()->loadTTF(fileNameTtf, &lsStbFont, &lsWidthLetter, heightLetter,saveTextureAsPng);
        if (texture == nullptr || lsStbFont.size() < 30)
        {
            delete mesh;
            return nullptr;
        }
        if(texture_loaded != nullptr)
            *texture_loaded = texture;
        auto tTotalSTB = static_cast<uint32_t>(lsStbFont.size() - 30);
        VEC3         pPosition[4];
        VEC3         pNormal[4];
        VEC2         pTexture[4];

        for (auto & i : pNormal)
        {
            i.x = 0;
            i.y = 0;
            i.z = 1;
        }

        mesh->buffer                       = new BUFFER_MESH[tTotalSTB];
        mesh->totalFramesMesh              = tTotalSTB;
        uint16_t    indexQuad[6] = {0, 1, 2, 2, 1, 3};
        auto* infoFont          = new INFO_BOUND_FONT();
        mesh->extraInfo					   = infoFont;
        infoFont->spaceXCharacter          = spaceWidth;
        infoFont->spaceYCharacter          = spaceHeight;
        infoFont->heightLetter             = static_cast<unsigned short>(heightLetter);

        infoFont->fontName = fileNameTtf;
        std::size_t p      = infoFont->fontName.find_last_of(util::getCharDirSeparator());
        if (p != std::string::npos)
            infoFont->fontName.erase(0, p + 1);
        const float middleHeight = 'M' <= lsWidthLetter.size() ? lsWidthLetter['M'].y : 0;
        for (uint32_t i = 30, index = 0; i < lsStbFont.size(); ++i)
        {
            stbtt_aligned_quad *q = lsStbFont[i];
            if (q)
            {
                const float y  = lsWidthLetter[i].y;
                float       dy = (middleHeight - y) * 0.5f;
                switch (i)
                {
                    case 'g': dy = y * 0.27f; break;
                    case 'p': dy = y * 0.27f; break;
                    case 'q': dy = y * 0.27f; break;
                    case 'y': dy = y * 0.27f; break;
                    case '*': dy = 0; break;
                    case '-': dy = 0; break;
                    case '+': dy = 0; break;
                    case '=': dy = 0; break;
                    case '<': dy = 0; break;
                    case '>': dy = 0; break;
                    case ':': dy = 0; break;
                    case '|': dy = 0; break;
                    case '~': dy = 0; break;
                    case '\'':
                        dy = -dy;
                        break; //'
                    case 22:
                        dy = -dy;
                        break; //"
                    case '\"':
                        dy = -dy;
                        break; //"
                    case ';':
                        dy = y * 0.5f;
                        break; //;
                    case 162:
                        dy = y * 0.10f;
                        break; //¢
                    case 185:
                        dy = -dy;
                        break; //¹
                    case 186:
                        dy = -dy;
                        break; //º
                    case 187:
                        dy = 0;
                        break; //»
                    case 170:
                        dy = -dy;
                        break; //ª
                    case 171:
                        dy = 0;
                        break; //«
                    case 172:
                        dy *= -0.75f;
                        break; //¬
                    case 176:
                        dy = -dy;
                        break; //°
                    case 178:
                        dy = -dy;
                        break; //²
                    case 179:
                        dy = -dy;
                        break; //³
                    case 231:
                        dy = y * 0.27f;
                        break; //ç
                    case 199:
                        dy = y * 0.13f;
                        break; //Ç
                }
                fillvertexQuadTrueFont(pPosition, lsWidthLetter[i].x, y, dy);

                pTexture[0].x = q->s0;
                pTexture[0].y = q->t1;
                pTexture[1].x = q->s0;
                pTexture[1].y = q->t0;
                pTexture[2].x = q->s1;
                pTexture[2].y = q->t1;
                pTexture[3].x = q->s1;
                pTexture[3].y = q->t0;

                mesh->buffer[index].pBufferGL            = new BUFFER_GL();
                mesh->buffer[index].subset               = new util::SUBSET[1];
                mesh->buffer[index].totalSubset          = 1;
                mesh->buffer[index].subset[0].indexCount = 6;
                
                if (mesh->buffer[index].pBufferGL->loadBuffer(
                        pPosition, pNormal, pTexture, 4, indexQuad, mesh->buffer[index].totalSubset,
                        &mesh->buffer[index].subset[0].indexStart, &mesh->buffer[index].subset[0].indexCount,nullptr))
                {

                    mesh->buffer[index].subset[0].texture        = texture;
                    mesh->buffer[index].pBufferGL->idTexture0[0] = texture->idTexture;
                    mesh->buffer[index].pBufferGL->useAlpha[0]   = texture->useAlphaChannel ? 1 : 0;
                    infoFont->letter[i].detail                   = new util::DETAIL_LETTER();
                    infoFont->letter[i].detail->indexFrame       = static_cast<uint8_t>(index);
                    infoFont->letter[i].detail->widthLetter      = static_cast<uint8_t>(lsWidthLetter[i].x);
                    infoFont->letter[i].detail->heightLetter     = static_cast<uint8_t>(lsWidthLetter[i].y);
                    infoFont->letter[i].detail->letter           = static_cast<uint8_t>(i);
                    ++index;
                }
                else
                {

                    PRINT_IF_DEBUG( "error on load buffer bufferTriangleList [%s]", fileNameTtf);
                    delete mesh;
                    mesh = nullptr;
                    for(auto qq : lsStbFont)
                    {
                        if(qq)
                            delete qq;
                    }
                    break;
                }
            }
        }
        for (auto q : lsStbFont)
        {
            if (q)
                delete q;
        }
        if (mesh)
        {
            mesh->positionOffset                    = VEC3(0, 0, 0);
            mesh->angleDefault                      = VEC3(0, 0, 0);
            mesh->typeMe                            = util::TYPE_MESH_FONT;
            auto header = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            mesh->infoAnimation.lsHeaderAnim.push_back(header);
            header->headerAnim             = new util::HEADER_ANIMATION();
            header->headerAnim->hasShaderEffect = 1; // always will be 1
            lsMeshes[fileNameBaseSuppose] = mesh;
            const char *fontps                 = "font.ps";
            header->headerAnim->typeAnimation  = 1;
            mesh->hasNormTex[0] = 1;//has normal
            mesh->hasNormTex[1] = 1;//uv each frame
            strncpy(header->headerAnim->nameAnimation,"font-1",sizeof(header->headerAnim->nameAnimation)-1);
            auto effectFont = new util::INFO_FX();
            header->effetcShader = effectFont;
            effectFont->dataPS = new util::INFO_SHADER_DATA(4, static_cast<int>(strlen(fontps) + 1), 0);
            strcpy(effectFont->dataPS->fileNameShader, fontps);
            const float mmin[4]               = {1.0f, 1.0f, 0.0f, 0.0f};
            const float mmax[4]               = {1.0f, 1.0f, 0.0f, 0.0f};
            effectFont->dataPS->typeAnimation = 6; // recursive loop
            effectFont->dataPS->timeAnimation = 1; // seconds
            effectFont->dataPS->typeVars[0]   = VAR_COLOR_RGB;
            memcpy(effectFont->dataPS->min, mmin, sizeof(mmin));
            memcpy(effectFont->dataPS->max, mmax, sizeof(mmax));
            mesh->fileName = std::move(fileNameBaseSuppose);
            
        }
        return mesh;
    }
    
    MESH_MBM * MESH_MANAGER::load(const char *nickName, float *pPosition, float *pNormal, float *pTexture,
                          const uint32_t sizeVertexBuffer,const util::INFO_DRAW_MODE * info_mode)
    {
        const std::string fileNameBase = util::getBaseName(nickName);
        auto mesh = this->lsMeshes[fileNameBase];
        if(mesh)
            return mesh;
        mesh                                  = new MESH_MBM();
        mesh->buffer                          = new BUFFER_MESH[1];
        mesh->totalFramesMesh                 = 1;
        mesh->buffer[0].pBufferGL             = new BUFFER_GL();
        mesh->buffer[0].subset                = new util::SUBSET[1];
        mesh->buffer[0].totalSubset           = 1;
        mesh->buffer[0].subset[0].vertexCount = sizeVertexBuffer / 3;

        {
            std::string which_mode;
            if(info_mode && is_any_mode_valid(*info_mode,which_mode) == false)
            {
                PRINT_IF_DEBUG( "Invalid mode %s detected:[%s]", which_mode.c_str(),nickName);
                delete mesh;
                return nullptr;
            }
        }

        if (!mesh->buffer[0].pBufferGL->loadBuffer(
                reinterpret_cast<VEC3 *>(pPosition), reinterpret_cast<VEC3 *>(pNormal), reinterpret_cast<VEC2 *>(pTexture), sizeVertexBuffer / 3, mesh->buffer[0].totalSubset,
                &mesh->buffer[0].subset[0].vertexStart, &mesh->buffer[0].subset[0].vertexCount,info_mode))
        {
            PRINT_IF_DEBUG( "error on load buffer bufferTriangleList [%s]", nickName);
            delete mesh;
            return nullptr;
        }

        mesh->positionOffset = VEC3(0, 0, 0);
        mesh->angleDefault   = VEC3(0, 0, 0);
        mesh->typeMe         = util::TYPE_MESH_SHAPE;
        if(info_mode)
        {
            mesh->info_mode.mode_draw = info_mode->mode_draw;
            mesh->info_mode.mode_cull_face = info_mode->mode_cull_face;
            mesh->info_mode.mode_front_face_direction = info_mode->mode_front_face_direction;
        }
        lsMeshes[fileNameBase] = mesh;
        return mesh;
    }
    
    MESH_MBM * MESH_MANAGER::loadIndex(const char *nickName, float *pPosition, float *pNormal, float *pTexture,
                               const uint32_t sizeVertexBuffer, uint16_t *index,
                               const uint32_t sizeIndex,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        const std::string fileNameBase = util::getBaseName(nickName);
        auto mesh = this->lsMeshes[fileNameBase];
        if(mesh)
            return mesh;
        mesh                                  = new MESH_MBM();
        mesh->buffer                          = new BUFFER_MESH[1];
        mesh->totalFramesMesh                 = 1;
        mesh->buffer[0].pBufferGL             = new BUFFER_GL();
        mesh->buffer[0].subset                = new util::SUBSET[1];
        mesh->buffer[0].totalSubset           = 1;
        mesh->buffer[0].subset[0].vertexCount = sizeVertexBuffer / 3;
        mesh->buffer[0].subset[0].indexCount  = static_cast<int>(sizeIndex);

        std::string which_mode;
        if(info_draw_mode && is_any_mode_valid(*info_draw_mode,which_mode) == false)
        {
            PRINT_IF_DEBUG( "Invalid mode %s detected:[%s]", which_mode.c_str(),nickName);
            delete mesh;
            return nullptr;
        }

        if (!mesh->buffer[0].pBufferGL->loadBuffer(reinterpret_cast<VEC3*>(pPosition),reinterpret_cast<VEC3 *>(pNormal), reinterpret_cast<VEC2 *>(pTexture),
                                                   sizeVertexBuffer / 3, index, mesh->buffer[0].totalSubset,
                                                   &mesh->buffer[0].subset[0].indexStart,
                                                   &mesh->buffer[0].subset[0].indexCount,info_draw_mode))
        {
            PRINT_IF_DEBUG( "error on load buffer bufferTriangleList [%s]", nickName);
            delete mesh;
            return nullptr;
        }

        mesh->positionOffset = VEC3(0, 0, 0);
        mesh->angleDefault   = VEC3(0, 0, 0);
        mesh->typeMe         = util::TYPE_MESH_SHAPE;
        if(info_draw_mode)
        {
            mesh->info_mode.mode_draw = info_draw_mode->mode_draw;
            mesh->info_mode.mode_cull_face = info_draw_mode->mode_cull_face;
            mesh->info_mode.mode_front_face_direction = info_draw_mode->mode_front_face_direction;
        }
        lsMeshes[fileNameBase] = mesh;
        return mesh;
    }
    
    MESH_MBM * MESH_MANAGER::loadDynamicIndex(const char *nickName, const uint32_t sizeVertexBuffer,uint16_t *index, const uint32_t sizeIndex,const util::INFO_DRAW_MODE * info_draw_mode,const util::DYNAMIC_SHAPE & dynamic_shape_info)
    {
        const std::string fileNameBase = util::getBaseName(nickName);
        auto mesh = this->lsMeshes[fileNameBase];
        if (mesh == nullptr)
            mesh = new MESH_MBM();
        else
            mesh->release();
        mesh->buffer                          = new BUFFER_MESH[1];
        mesh->totalFramesMesh                 = 1;
        mesh->buffer[0].pBufferGL             = new BUFFER_GL();
        mesh->buffer[0].subset                = new util::SUBSET[1];
        mesh->buffer[0].totalSubset           = 1;
        mesh->buffer[0].subset[0].vertexCount = sizeVertexBuffer / 3;
        mesh->buffer[0].subset[0].indexCount  = static_cast<int>(sizeIndex);

        std::string which_mode;
        if(info_draw_mode && is_any_mode_valid(*info_draw_mode,which_mode) == false)
        {
            PRINT_IF_DEBUG( "Invalid mode %s detected:[%s]", which_mode.c_str(),nickName);
            delete mesh;
            return nullptr;
        }

        if (!mesh->buffer[0].pBufferGL->loadBufferDynamic(index, mesh->buffer[0].totalSubset,
                                                          &mesh->buffer[0].subset[0].indexStart,
                                                          &mesh->buffer[0].subset[0].indexCount,info_draw_mode))
        {
            PRINT_IF_DEBUG( "error on load buffer bufferTriangleList [%s]", nickName);
            delete mesh;
            return nullptr;
        }
        mesh->positionOffset = VEC3(0, 0, 0);
        mesh->angleDefault   = VEC3(0, 0, 0);
        mesh->typeMe         = util::TYPE_MESH_SHAPE;
        util::DYNAMIC_SHAPE * extra_info_shape = new util::DYNAMIC_SHAPE(dynamic_shape_info.dynamicVertex,dynamic_shape_info.dynamicNormal,dynamic_shape_info.dynamicUV,dynamic_shape_info.size_vertex,dynamic_shape_info.size_normal,dynamic_shape_info.size_uv);
        mesh->extraInfo      = extra_info_shape;
        mesh->fileName       = fileNameBase;
        if(info_draw_mode)
        {
            mesh->info_mode.mode_draw = info_draw_mode->mode_draw;
            mesh->info_mode.mode_cull_face = info_draw_mode->mode_cull_face;
            mesh->info_mode.mode_front_face_direction = info_draw_mode->mode_front_face_direction;
        }
        lsMeshes[fileNameBase] = mesh;
        return mesh;
    }

    MESH_MANAGER::~MESH_MANAGER()
    {
        std::unordered_map<std::string,MESH_MBM *>::const_iterator it;
        for(it = lsMeshes.cbegin(); it != lsMeshes.cend(); ++it)
        {
            MESH_MBM *ptr = it->second;
            if (ptr)
            {
                ptr->release();
                delete ptr;
                ptr = nullptr;
            }
        }
        for(auto ptr : this->lsFakeRelease)
        {
            if (ptr)
            {
                delete ptr;
            }
        }
        this->lsFakeRelease.clear();
    }

    const char * MESH_MANAGER::typeClassName(const util::TYPE_MESH type) noexcept
    {
        switch (type)
        {
            case util::TYPE_MESH_3D           : return "mesh";
            case util::TYPE_MESH_USER         : return "mesh user";
            case util::TYPE_MESH_SPRITE       : return "sprite";
            case util::TYPE_MESH_FONT         : return "font";
            case util::TYPE_MESH_TEXTURE      : return "texture";
            case util::TYPE_MESH_UNKNOWN      : return "unknown";
            case util::TYPE_MESH_SHAPE        : return "shape";
            case util::TYPE_MESH_PARTICLE     : return "particle";
            case util::TYPE_MESH_TILE_MAP     : return "tile map";
			default                           : return "unknown";
        }
    }
}

mbm::MESH_MANAGER *    mbm::MESH_MANAGER::instanceMeshManager        = nullptr;
