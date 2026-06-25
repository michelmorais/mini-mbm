#include <mesh-manager.h>
#include "mesh-manager-impl.h"
#include <deprecated.h>
#include <util-interface.h>
#include "mesh-v8-io.h"

namespace mbm
{
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
    bool MESH_MBM_DEBUG::loadDebugLegacyDetailStep(FILE *fp, const char *fileNamePath, deprecated_mbm::INFO_SPRITE &deprecatedInfoSprite)
    {
        switch (this->typeMe)
        {
            case util::TYPE_MESH_3D:
            {
                util::DETAIL_MESH detail;
                if (!util::readDetailMeshV8(fp, detail))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load bounding box [%s]", fileNamePath);
                if (detail.totalBounding)
                {
                    switch (detail.type)
                    {
                        case 1:
                        {
                            for (int i = 0; i < detail.totalBounding; ++i)
                            {
                                auto base = new CUBE();
                                this->infoPhysics.lsCube.push_back(base);
                                if (!util::readCubeV8(fp, *base))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]", fileNamePath);
                            }
                        }
                        break;
                        case 2:
                        {
                            for (int i = 0; i < detail.totalBounding; ++i)
                            {
                                auto base = new SPHERE();
                                this->infoPhysics.lsSphere.push_back(base);
                                if (!util::readSphereV8(fp, *base))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]", fileNamePath);
                            }
                        }
                        break;
                        case 3:
                        {
                            for (int i = 0; i < detail.totalBounding; ++i)
                            {
                                auto complex = new CUBE_COMPLEX();
                                this->infoPhysics.lsCubeComplex.push_back(complex);
                                if (!util::readCubeComplexV8(fp, *complex))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]", fileNamePath);
                            }
                        }
                        break;
                        default:
                            return log_util::onFailed(fp,__FILE__, __LINE__, "bounding unknown [%s]", fileNamePath);
                    }
                }
            }
            break;
            case util::TYPE_MESH_SPRITE:
            {
                if (!deprecatedInfoSprite.readBoundingSprite(fp, fileNamePath, &this->zoomEditorSprite.x, &this->zoomEditorSprite.y))
                    return false;
            }
            break;
            case util::TYPE_MESH_USER:
            {
            }
            break;
            case util::TYPE_MESH_FONT:
            {
                auto *infoFont = new INFO_BOUND_FONT();
                this->extraInfo = infoFont;
                util::DETAIL_HEADER_FONT headerFont;
                if (!util::readDetailHeaderFontV8(fp, headerFont))
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
                    if (!util::readDetailLetterV8(fp, *detailFont))
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
            }
            break;
            default:
                break;
        }
        return true;
    }

    bool MESH_MBM_DEBUG::loadDebugLegacyAnimationStep(FILE *fp, const char *fileNamePath, deprecated_mbm::INFO_SPRITE &deprecatedInfoSprite)
    {
        if (this->headerMain.version == INITIAL_VERSION_MBM_HEADER)
        {
            if (!deprecated_mbm::fillAnimation_1(fileNamePath, fp, &this->headerMesh, &this->infoAnimation))
                return false;
        }
        else if (this->headerMain.version >= SPRITE_INFO_VERSION_MBM_HEADER)
        {
            if (!this->fillAnimation_2(fileNamePath, fp))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read animation[%s]", fileNamePath);
        }
        else
        {
            return log_util::onFailed(fp,__FILE__, __LINE__, "unknown version [%s] Version [%d]", fileNamePath, this->headerMain.version);
        }

        if (this->headerMain.version == INITIAL_VERSION_MBM_HEADER)
            this->headerMain.version = SPRITE_INFO_VERSION_MBM_HEADER;
        if (this->headerMain.version <= SPRITE_INFO_VERSION_MBM_HEADER)
            deprecatedInfoSprite.fillOldPhysicsSprite_2(this->typeMe, this->infoAnimation, this->headerMesh.totalFrames);

        return true;
    }

    void MESH_MBM_DEBUG::fillDebugLegacyPhysicsIfNeeded(const int fileVersion, deprecated_mbm::INFO_SPRITE &deprecatedInfoSprite,
                                                        VEC3 *position, const uint32_t currentFrame,
                                                        std::vector<util::SUBSET_DEBUG *> &subsetArray)
    {
        if (fileVersion <= SPRITE_INFO_VERSION_MBM_HEADER)
        {
            deprecatedInfoSprite.fillPhysicsSprite(position, currentFrame, subsetArray, this->typeMe,
                                                   this->infoPhysics.lsCube, this->infoPhysics.lsSphere,
                                                   this->infoPhysics.lsTriangle);
        }
    }

    bool MESH_MBM::loadLegacyDetailStep(FILE *fp, const char *fileNamePath, const util::HEADER &headerMain,
                                        deprecated_mbm::INFO_SPRITE &deprecatedInfoSprite)
    {
        switch (this->typeMe)
        {
            case util::TYPE_MESH_3D:
            {
                util::DETAIL_MESH detail;
                if (!util::readDetailMeshV8(fp, detail))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load bounding box [%s]", fileNamePath);
                if (detail.totalBounding)
                {
                    switch (detail.type)
                    {
                        case 1:
                        {
                            for (int i = 0; i < detail.totalBounding; ++i)
                            {
                                auto base = new CUBE();
                                this->infoPhysics.lsCube.push_back(base);
                                if (!util::readCubeV8(fp, *base))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]", fileNamePath);
                            }
                        }
                        break;
                        case 2:
                        {
                            for (int i = 0; i < detail.totalBounding; ++i)
                            {
                                auto base = new SPHERE();
                                this->infoPhysics.lsSphere.push_back(base);
                                if (!util::readSphereV8(fp, *base))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]", fileNamePath);
                            }
                        }
                        break;
                        case 3:
                        {
                            for (int i = 0; i < detail.totalBounding; ++i)
                            {
                                auto complex = new CUBE_COMPLEX();
                                this->infoPhysics.lsCubeComplex.push_back(complex);
                                if (!util::readCubeComplexV8(fp, *complex))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]", fileNamePath);
                            }
                        }
                        break;
                        default:
                            return log_util::onFailed(fp,__FILE__, __LINE__, "bounding unknown [%s]", fileNamePath);
                    }
                }
            }
            break;
            case util::TYPE_MESH_SPRITE:
            {
                if (!deprecatedInfoSprite.readBoundingSprite(fp, fileNamePath, &this->zoomEditorSprite.x, &this->zoomEditorSprite.y))
                    return false;
            }
            break;
            case util::TYPE_MESH_USER:
            {
            }
            break;
            case util::TYPE_MESH_FONT:
            {
                auto *infoFont = new INFO_BOUND_FONT();
                this->extraInfo = infoFont;
                util::DETAIL_HEADER_FONT headerFont;
                if (!util::readDetailHeaderFontV8(fp, headerFont))
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
                    if (!util::readDetailLetterV8(fp, *detailFont))
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
            }
            break;
            default:
                break;
        }
        (void)headerMain;
        return true;
    }

    bool MESH_MBM::loadLegacyAnimationStep(FILE *fp, const char *fileNamePath, const util::HEADER &headerMain,
                                           util::HEADER_MESH &headerMesh, deprecated_mbm::INFO_SPRITE &deprecatedInfoSprite)
    {
        if (headerMain.version == INITIAL_VERSION_MBM_HEADER)
        {
            if (!deprecated_mbm::fillAnimation_1(fileNamePath, fp, &headerMesh, &this->infoAnimation))
                return false;
        }
        else
        {
            if (!this->fillAnimation_2(headerMesh, headerMain.version, fileNamePath, fp))
                return false;
        }

        if (headerMain.version <= SPRITE_INFO_VERSION_MBM_HEADER)
            deprecatedInfoSprite.fillOldPhysicsSprite_2(this->typeMe, this->infoAnimation, headerMesh.totalFrames);

        return true;
    }

    void MESH_MBM::fillLegacyPhysicsIfNeeded(const int fileVersion, deprecated_mbm::INFO_SPRITE &deprecatedInfoSprite,
                                             VEC3 *position, const uint32_t currentFrame, util::SUBSET *subsetArray)
    {
        if (fileVersion <= SPRITE_INFO_VERSION_MBM_HEADER)
        {
            deprecatedInfoSprite.fillPhysicsSprite(position, currentFrame, subsetArray, this->typeMe,
                                                   this->infoPhysics.lsCube, this->infoPhysics.lsSphere,
                                                   this->infoPhysics.lsTriangle);
        }
    }

    bool MESH_MBM_DEBUG::getInfoLegacyCompat(FILE *fp, const char *fileNamePath, const util::HEADER &headerMbmOut,
                                             util::HEADER_MESH &headerMeshMbmOut, util::INFO_DRAW_MODE &info_mode,
                                             util::TYPE_MESH &typeOut, INFO_BOUND_FONT &datailFontOut,
                                             std::vector<util::STAGE_PARTICLE> &lsStageParticle, int *versionOut)
    {
        deprecated_mbm::INFO_SPRITE deprecatedInfoSprite;
        for (int i = 0; i < headerMbmOut.extraHeader; i++)
        {
            util::EXTRA_HEADER extra;
            if (!util::readExtraHeaderV8(fp, extra))
                return log_util::onFailed(fp, __FILE__, __LINE__, "failed to read info EXTRA_HEADER [%s]", fileNamePath);
            if (extra.type == 1)
            {
                std::string path(extra.sizeExtraHeader + 1, 0);
                if (!fread(&path[0], extra.sizeExtraHeader, 1, fp))
                    return log_util::onFailed(fp, __FILE__, __LINE__, "Failed to read string from EXTRA_HEADER [%s] size -> [%d]", fileNamePath, extra.sizeExtraHeader);
            }
            else
            {
                return log_util::onFailed(fp, __FILE__, __LINE__, "Unsuported type of EXTRA_HEADER [%s] -> type [%d]", fileNamePath, extra.type);
            }
        }

        if (headerMbmOut.version >= MODE_DRAW_VERSION_MBM_HEADER)
        {
            if (!util::readInfoDrawModeV8(fp, info_mode))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info INFO_DRAW_MODE [%s]", fileNamePath);
        }

        if (headerMbmOut.version >= DETAIL_MESH_VERSION_MBM_HEADER)
        {
            util::DETAIL_MESH detailInfo;
            if (headerMbmOut.version == DETAIL_MESH_VERSION_MBM_HEADER)
            {
                if (!util::readDetailMeshV8(fp, detailInfo))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info DETAIL_MESH [%s]", fileNamePath);
                if (detailInfo.type != 100 && detailInfo.type != 101)
                    return log_util::onFailed(fp,__FILE__, __LINE__,"expected first DETAIL_MESH [%s] as size info extra information at version == DETAIL_MESH_VERSION_MBM_HEADER",fileNamePath);
                if (detailInfo.totalBounding)
                {
                    const int extraISize = detailInfo.totalBounding;
                    auto extraInfo = new char[extraISize];
                    if (!fread(extraInfo, static_cast<size_t>(extraISize), 1, fp))
                    {
                        delete[] extraInfo;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read extra info [%s]", fileNamePath);
                    }
                    delete[] extraInfo;
                }
            }

            if (!util::readDetailMeshV8(fp, detailInfo))
                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read info DETAIL_MESH [%s]", fileNamePath);
            if (headerMbmOut.version == DETAIL_MESH_VERSION_MBM_HEADER)
            {
                if (detailInfo.type != 'H')
                    return log_util::onFailed(fp,__FILE__, __LINE__, "expected 'H' at DETAIL_MESH [%s]", fileNamePath);
            }
            else if (detailInfo.type != 'P')
            {
                return log_util::onFailed(fp,__FILE__, __LINE__, "expected 'P' from Physics at DETAIL_MESH [%s]", fileNamePath);
            }

            for (int i = 0; i < detailInfo.totalBounding; )
            {
                util::DETAIL_MESH detail;
                if (!util::readDetailMeshV8(fp, detail))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read DETAIL_MESH [%s]", fileNamePath);
                switch (detail.type)
                {
                    case 1:
                    {
                        for (int j = 0; j < detail.totalBounding; j++)
                        {
                            CUBE cube;
                            if (!util::readCubeV8(fp, cube))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 2:
                    {
                        for (int j = 0; j < detail.totalBounding; j++)
                        {
                            SPHERE sphere;
                            if (!util::readSphereV8(fp, sphere))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 3:
                    {
                        for (int j = 0; j < detail.totalBounding; j++)
                        {
                            CUBE_COMPLEX complex;
                            if (!util::readCubeComplexV8(fp, complex))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 4:
                    {
                        if (headerMbmOut.version >= MODE_DRAW_VERSION_MBM_HEADER)
                        {
                            for (int j = 0; j < detail.totalBounding; j++)
                            {
                                TRIANGLE triangle;
                                if (!util::readTriangleV8(fp, triangle))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                            }
                        }
                        else
                        {
                            for (int j = 0; j < detail.totalBounding; j++)
                            {
                                TRIANGLE triangle;
                                if (!util::readTriangleLegacyNoPosV8(fp, triangle))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
                            }
                        }
                        i += detail.totalBounding;
                    }
                    break;
                    case 5:
                    {
                        util::DETAIL_HEADER_FONT headerFont;
                        if (!util::readDetailHeaderFontV8(fp, headerFont))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_HEADER_FONT [%s]", fileNamePath);
                        auto strNameFont = new char[headerFont.sizeNameFonte];
                        if (!fread(strNameFont, static_cast<size_t>(headerFont.sizeNameFonte), 1, fp))
                        {
                            delete[] strNameFont;
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load font's name [%s]", fileNamePath);
                        }
                        datailFontOut.fontName = strNameFont;
                        datailFontOut.heightLetter = headerFont.heightLetter;
                        datailFontOut.spaceXCharacter = headerFont.spaceXCharacter;
                        datailFontOut.spaceYCharacter = headerFont.spaceYCharacter;
                        delete[] strNameFont;
                        for (int j = 0; j < headerFont.totalDetailFont; ++j)
                        {
                            auto detailFont = new util::DETAIL_LETTER();
                            if (!util::readDetailLetterV8(fp, *detailFont))
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
                        for (int j = 0; j < detail.totalBounding; j++)
                        {
                            util::STAGE_PARTICLE stage;
                            if (!util::readStageParticleV8(fp, stage))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read stage particle [%s]", fileNamePath);
                            lsStageParticle.push_back(stage);
                        }
                        i += 1;
                    }
                    break;
                    case 7:
                    {
                        util::BTILE_INFO infoTileMap;
                        if (!util::readBtileHeaderMapV8(fp, infoTileMap.map))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read from  detail tile!");
                        if (infoTileMap.map.layerCount != static_cast<uint32_t>(detail.totalBounding))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "expected layerCount == totalBounding. [%d]!=[%d]", infoTileMap.map.layerCount, detail.totalBounding);

                        infoTileMap.infoBrickEditor = new util::BTILE_BRICK_INFO[infoTileMap.map.countRawTiles];
                        if (!util::readBtileBrickInfoArrayV8(fp, infoTileMap.infoBrickEditor, infoTileMap.map.countRawTiles))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read brick info editor tile from  detail tile!");

                        const uint32_t tileCount = infoTileMap.map.count_height_tile * infoTileMap.map.count_width_tile;
                        infoTileMap.layers = new util::BTILE_LAYER[infoTileMap.map.layerCount];
                        for (uint32_t j = 0; j < infoTileMap.map.layerCount; ++j)
                        {
                            if (!util::readFloat3ArrayV8(fp, infoTileMap.layers[j].offset))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read layer offset from tile!");
                            infoTileMap.layers[j].lsIndexTiles = new util::BTILE_INDEX_TILE[tileCount];
                            if (!util::readBtileIndexTileArrayV8(fp, infoTileMap.layers[j].lsIndexTiles, tileCount))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read indexed tile from  detail tile!");
                        }

                        util::BTILE_DETAIL_HEADER detailObjsProperty;
                        if (!util::readBtileDetailHeaderV8(fp, detailObjsProperty))
                            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");

                        for (uint32_t j = 0; j < detailObjsProperty.totalObj; ++j)
                        {
                            util::BTILE_OBJ_HEADER objHeader;
                            if (!util::readBtileObjHeaderV8(fp, objHeader))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");
                            auto* obj = new util::BTILE_OBJ(static_cast<util::BTILE_OBJ_TYPE>(objHeader.type));
                            infoTileMap.lsObj.push_back(obj);
                            if (objHeader.sizeName > 0)
                            {
                                obj->name.resize(objHeader.sizeName);
                                auto* name = const_cast<char*>(obj->name.data());
                                if (!fread(name, objHeader.sizeName, 1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read name at detail tile header!");
                            }
                            for (uint32_t k = 0; k < objHeader.sizePoints; ++k)
                            {
                                auto* point = new mbm::VEC2();
                                obj->lsPoints.push_back(point);
                                if (!util::readVec2V8(fp, *point))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail tile header!");
                            }
                        }

                        for (uint32_t j = 0; j < detailObjsProperty.totalProperties; ++j)
                        {
                            util::BTILE_PROPERTY_HEADER propertyHeader;
                            if (!util::readBtilePropertyHeaderV8(fp, propertyHeader))
                                return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read detail property tile header!");
                            auto* property = new util::BTILE_PROPERTY();
                            infoTileMap.lsProperty.push_back(property);
                            if (propertyHeader.nameLength > 0)
                            {
                                property->name.resize(propertyHeader.nameLength);
                                auto* name = const_cast<char*>(property->name.data());
                                if (!fread(name, propertyHeader.nameLength, 1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property name at detail tile header!");
                            }
                            if (propertyHeader.valueLength > 0)
                            {
                                property->value.resize(propertyHeader.valueLength);
                                auto* value = const_cast<char*>(property->value.data());
                                if (!fread(value, propertyHeader.valueLength, 1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property value at detail tile header!");
                            }
                            if (propertyHeader.ownerLength > 0)
                            {
                                property->owner.resize(propertyHeader.ownerLength);
                                auto* owner = const_cast<char*>(property->owner.data());
                                if (!fread(owner, propertyHeader.ownerLength, 1, fp))
                                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read property owner at detail tile header!");
                            }
                        }
                        i += 1;
                    }
                    break;
                    default:
                        return log_util::onFailed(fp,__FILE__, __LINE__, "unknown type bounding box [%d] [%s]", detail.type, fileNamePath);
                }
            }
        }
        else
        {
            switch (typeOut)
            {
                case util::TYPE_MESH_3D:
                {
                    util::DETAIL_MESH detail;
                    if (!util::readDetailMeshV8(fp, detail))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load bounding box [%s]", fileNamePath);
                    if (detail.totalBounding)
                    {
                        switch (detail.type)
                        {
                            case 1:
                            {
                                for (int i = 0; i < detail.totalBounding; ++i)
                                {
                                    CUBE base;
                                    if (!util::readCubeV8(fp, base))
                                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]", fileNamePath);
                                }
                            }
                            break;
                            case 2:
                            {
                                for (int i = 0; i < detail.totalBounding; ++i)
                                {
                                    SPHERE base;
                                    if (!util::readSphereV8(fp, base))
                                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]", fileNamePath);
                                }
                            }
                            break;
                            case 3:
                            {
                                for (int i = 0; i < detail.totalBounding; ++i)
                                {
                                    CUBE_COMPLEX complex;
                                    if (!util::readCubeComplexV8(fp, complex))
                                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding [%s]", fileNamePath);
                                }
                            }
                            break;
                            default:
                                return log_util::onFailed(fp,__FILE__, __LINE__, "bounding unknown [%s]", fileNamePath);
                        }
                    }
                }
                break;
                case util::TYPE_MESH_SPRITE:
                {
                    float zx = 0.0f;
                    float zy = 0.0f;
                    if (!deprecatedInfoSprite.readBoundingSprite(fp, fileNamePath, &zx, &zy))
                        return false;
                }
                break;
                case util::TYPE_MESH_USER:
                    break;
                case util::TYPE_MESH_FONT:
                {
                    util::DETAIL_HEADER_FONT headerFont;
                    if (!util::readDetailHeaderFontV8(fp, headerFont))
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load DETAIL_HEADER_FONT [%s]", fileNamePath);
                    auto strNameFont = new char[headerFont.sizeNameFonte];
                    if (!fread(strNameFont, static_cast<size_t>(headerFont.sizeNameFonte), 1, fp))
                    {
                        delete[] strNameFont;
                        return log_util::onFailed(fp,__FILE__, __LINE__, "failed to load font's name [%s]", fileNamePath);
                    }
                    datailFontOut.fontName = strNameFont;
                    datailFontOut.heightLetter = headerFont.heightLetter;
                    datailFontOut.spaceXCharacter = headerFont.spaceXCharacter;
                    datailFontOut.spaceYCharacter = headerFont.spaceYCharacter;
                    delete[] strNameFont;
                    for (int i = 0; i < headerFont.totalDetailFont; ++i)
                    {
                        auto detailFont = new util::DETAIL_LETTER();
                        if (!util::readDetailLetterV8(fp, *detailFont))
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
                }
                break;
                default:
                    break;
            }
        }

        if (!util::readHeaderMeshV8(fp, headerMeshMbmOut))
            return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read HEADER_MESH [%s]", fileNamePath);

        fclose(fp);
        if (versionOut)
            *versionOut = headerMbmOut.version;
        return true;
    }
#endif

    bool MESH_MBM_DEBUG::readDebugTriangleDetailCompat(FILE *fp, const char *fileNamePath, const int totalBounding, const int fileVersion)
    {
        if (fileVersion >= MODE_DRAW_VERSION_MBM_HEADER)
        {
            for (int j = 0; j < totalBounding; j++)
            {
                auto triangle = new TRIANGLE();
                this->impl->infoPhysics.lsTriangle.push_back(triangle);
                if (!util::readTriangleV8(fp, *triangle))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
            }
        }
        else
        {
            for (int j = 0; j < totalBounding; j++)
            {
                auto triangle = new TRIANGLE();
                this->impl->infoPhysics.lsTriangle.push_back(triangle);
                if (!util::readTriangleLegacyNoPosV8(fp, *triangle))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
            }
        }
        return true;
    }

    bool MESH_MBM::readTriangleDetailCompat(FILE *fp, const char *fileNamePath, const int totalBounding, const int fileVersion)
    {
        if (fileVersion >= MODE_DRAW_VERSION_MBM_HEADER)
        {
            for (int j = 0; j < totalBounding; j++)
            {
                auto triangle = new TRIANGLE();
                this->impl->infoPhysics.lsTriangle.push_back(triangle);
                if (!util::readTriangleV8(fp, *triangle))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
            }
        }
        else
        {
            for (int j = 0; j < totalBounding; j++)
            {
                auto triangle = new TRIANGLE();
                this->impl->infoPhysics.lsTriangle.push_back(triangle);
                if (!util::readTriangleLegacyNoPosV8(fp, *triangle))
                    return log_util::onFailed(fp,__FILE__, __LINE__, "failed to read bounding box [%s]", fileNamePath);
            }
        }
        return true;
    }

    bool MESH_MBM_DEBUG::loadDebugLegacyCompat(const char *fileNamePath)
    {
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
        return this->loadDebugImpl(fileNamePath, false);
#else
        (void)fileNamePath;
        return false;
#endif
    }

    bool MESH_MBM::loadLegacyCompat(const char *fileNamePath, RENDERIZABLE *renderizable)
    {
#if defined(MBM_ENABLE_MESH_LEGACY_V7)
        return this->loadImpl(fileNamePath, false, renderizable);
#else
        (void)fileNamePath;
        (void)renderizable;
        return false;
#endif
    }
}
