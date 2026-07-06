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

#include <tile.h>
#include <cctype>
#include <core_mbm/texture-manager.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/util-interface.h>
#include <core_mbm/shader-var-cfg.h>
#include <core_mbm/header-mesh.h>
#include <core_mbm/scene.h>
#include <cmath>

namespace mbm
{

    TILE::TILE(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_TILE, _is3d && _is2dScreen == false, _is2dScreen)
        , clone_bTileInfo(nullptr)
        , backgroundTextureMap(nullptr)
    {
        this->mesh = nullptr;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->addRenderizable(this);
    }
    
    TILE::~TILE()
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->removeRenderizable(this);
        this->release();
    }
    
    void TILE::release()
    {
        for (auto* l : lsLayerRenderizables)
            delete l;
        lsLayerRenderizables.clear();
        this->releaseAnimation();
        this->lsVisible.clear();
        this->mesh                  = nullptr;
        this->setIndexAnimation(0);
        if(clone_bTileInfo)
            delete clone_bTileInfo;
        clone_bTileInfo = nullptr;
        backGroundMap.release();
        for (size_t i = 0; i < lsTileObjs.size(); i++)
        {
            TILE_OBJ * tile_obj = lsTileObjs[i];
            delete tile_obj;
        }
        lsTileObjs.clear();
    }

    
    bool TILE::load(const char *fileName)
    {
        if (this->mesh != nullptr)
            return true;
        this->mesh = MESH_MANAGER::getInstance()->load(fileName, this);
        if (this->mesh)
        {
            const util::TYPE_MESH expectedType = util::TYPE_MESH_TILE_MAP;
            const MeshLoadFinishResult result = this->populateAnimationsFromMesh(this->mesh, &expectedType, "tile");
            if (result == MeshLoadFinishResult::ANIMATION_FAILED)
            {
                this->release();
                return false;
            }
            else if (result != MeshLoadFinishResult::OK)
                return false;
            return finalizeTileLoad(fileName);
        }
        return false;
    }

    void TILE::loadAsync(const char *fileName, std::function<void(bool success)> callback)
    {
        if (this->mesh != nullptr)
        {
            if (callback)
                callback(true);
            return;
        }
        const std::string fileNameCopy(fileName);
        MESH_MANAGER::getInstance()->loadAsync(fileName, [this, fileNameCopy, callback](MESH_MBM *mesh, bool ok)
        {
            if (!ok || !mesh)
            {
                if (callback)
                    callback(false);
                return;
            }
            this->mesh = mesh;
            this->getPosition() += mesh->getPositionOffset();
            this->setAngle(mesh->getAngleDefault());
            const util::TYPE_MESH expectedType = util::TYPE_MESH_TILE_MAP;
            const MeshLoadFinishResult result = this->populateAnimationsFromMesh(this->mesh, &expectedType, "tile");
            if (result == MeshLoadFinishResult::ANIMATION_FAILED)
            {
                this->release();
                if (callback)
                    callback(false);
                return;
            }
            else if (result != MeshLoadFinishResult::OK)
            {
                if (callback)
                    callback(false);
                return;
            }
            if (callback)
                callback(finalizeTileLoad(fileNameCopy.c_str()));
        });
    }

    bool TILE::finalizeTileLoad(const char *fileName)
    {
        this->setInternalFileName(fileName);
        this->restartAnimation();
        const auto *ptr_TileInfo = this->mesh->getInfoTile();
        if (ptr_TileInfo)
        {
            INFO_PHYSICS &physicsInfo = this->mesh->getPhysicsInfo();
            CUBE *cube                = nullptr;
            const VEC3 &scale         = this->getScale();
            const float width_tile    = static_cast<float>(ptr_TileInfo->map.size_width_tile  * scale.x);
            const float height_tile   = static_cast<float>(ptr_TileInfo->map.size_height_tile * scale.y);
            const float width_map     = static_cast<float>(width_tile  * ptr_TileInfo->map.count_width_tile);
            const float height_map    = static_cast<float>(height_tile * ptr_TileInfo->map.count_height_tile);
            if (physicsInfo.lsCube.size() > 0)
            {
                cube = physicsInfo.lsCube[0];
                cube->halfDim.x = width_map  * 0.5f;
                cube->halfDim.y = height_map * 0.5f;
                cube->halfDim.z = 0;
            }
            else
            {
                cube = new CUBE(width_map, height_map, 0);
                physicsInfo.lsCube.push_back(cube);
            }
            if (ptr_TileInfo->map.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
            {
                cube->halfDim.y   += height_tile * 0.5f;
                cube->absCenter.x  = width_tile  * 0.5f;
            }
            if (ptr_TileInfo->map.background_texture[0])
            {
                backgroundTextureMap = TEXTURE_MANAGER::getInstance()->load(ptr_TileInfo->map.background_texture, true);
            }
            else if (ptr_TileInfo->map.background > 0)
            {
                char whatColor[20] = "";
                COLOR::getStringHexColorFromColor(ptr_TileInfo->map.background, whatColor, sizeof(whatColor));
                mbm::TEXTURE::EnablePixelPerfectTexture(true);
                backgroundTextureMap = TEXTURE_MANAGER::getInstance()->load(whatColor, true);
                mbm::TEXTURE::EnablePixelPerfectTexture(false);
            }
            if (backgroundTextureMap)
                loadBufferBackGroundTexture();
            lsVisible.resize(ptr_TileInfo->map.layerCount, true);
            for (uint32_t i = 0; i < ptr_TileInfo->map.layerCount; ++i)
                lsLayerRenderizables.push_back(new TILE_LAYER(this, i));
        }
        else
        {
            PRINT_IF_DEBUG("error on get ptr_TileInfo!!");
            return false;
        }
        this->updateAABB();
        return true;
    }

    bool TILE::loadBufferBackGroundTexture()
    {
        if(backGroundMap.isLoadedBuffer() == false)
        {
            VEC3 vertex[4];
            VEC2 uv[4];
            int indexStart = 0;
            int indexCount = 6;
            const float x  = 0.5f;
            const float y  = 0.5f;
            vertex[0].x = -x;
            vertex[0].y = -y;
            vertex[0].z = 0;

            vertex[1].x = -x;
            vertex[1].y = y;
            vertex[1].z = 0;

            vertex[2].x = x;
            vertex[2].y = -y;
            vertex[2].z = 0;

            vertex[3].x = x;
            vertex[3].y = y;
            vertex[3].z = 0;

            uv[0].x = 0;
            uv[0].y = 1;
            uv[1].x = 0;
            uv[1].y = 0;
            uv[2].x = 1;
            uv[2].y = 1;
            uv[3].x = 1;
            uv[3].y = 0;
            
            unsigned short int index[6]      = {0, 1, 2, 2, 1, 3};
            if(backGroundMap.loadBuffer(vertex, nullptr, uv, 4, index, 1, &indexStart, &indexCount, nullptr) == false)
            {
                ERROR_LOG("Error on load buffer for background texture [%s]",backgroundTextureMap ? backgroundTextureMap->getFileNameTexture() : "null");
                return false;
            }
            this->backGroundMap.setTextureByStage(backgroundTextureMap, 0, 0);
            this->backGroundMap.setTextureByStage(nullptr, 1, 0);
        }
        return true;
    }

    const char * TILE::getFileName()
    {
        if (this->mesh)
            return this->mesh->getFilenameMesh();
        return nullptr;
    }
    
    bool TILE::isOnFrustum()
    {
        if (this->mesh)
        {
            IS_ON_FRUSTUM verify(this);
            return verify.isOnFrustum(this->is3DObject(), this->is2dScreenObject());
        }
        return false;
    }

    bool TILE::render()
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->disableFilteringForPixelPerfect();
        
        const auto * ptr_TileInfo = this->getTileInfo();
        if(backgroundTextureMap)
        {
            auto * anim               = this->getAnimation(0);
            if(loadBufferBackGroundTexture() && anim)
            {
                VEC3 &position           = this->getPosition();
                VEC3 &angle              = this->getAngle();
                VEC3 &scale              = this->getScale();
                const float width_tile    = static_cast<float>(ptr_TileInfo->map.size_width_tile  * scale.x);
                const float height_tile   = static_cast<float>(ptr_TileInfo->map.size_height_tile * scale.y);
                const float width_map     = static_cast<float>(width_tile  * ptr_TileInfo->map.count_width_tile);
                const float height_map    = static_cast<float>(height_tile * ptr_TileInfo->map.count_height_tile);
                VEC3 backGround_scale(width_map,height_map,1.0f);
                VEC3 backPos(position);

                if(ptr_TileInfo->map.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
                {
                    backGround_scale.x  += width_tile  * 0.5f;
                    backGround_scale.y  += height_tile;
                    backPos.x += width_tile * 0.25f;
                }

                FX &fx = anim->getFx();
                MatrixTranslationRotationScale(&SHADER::modelView, &backPos, &angle, &backGround_scale);
                SHADER::updateMvpAndLightMatrices(device->getCamera().matrixView2d, device->getCamera().matrixPerspective2d);
                if (fx.shader.render(&this->backGroundMap, this) == false)
                {
                    device->enableFilteringAfterPixelPerfect();
                    return false;
                }
            }
        }
        device->enableFilteringAfterPixelPerfect();
        return true;
    }

    bool TILE::renderLayer(const uint32_t index_layer, const float z_value)
    {
        ANIMATION *anim            = getAnimation(index_layer);
        if(anim == nullptr)
        {
            ERROR_LOG("Tile should have one animation per layer. It does not have!");
            return false;
        }
        const auto * ptr_TileInfo  = this->getTileInfo();
        if(ptr_TileInfo == nullptr)
        {
            ERROR_LOG("Tile does not have information!");
            return false;
        }
        const util::BTILE_LAYER* layer = & ptr_TileInfo->layers[index_layer];
        VEC3 &position                 = this->getPosition();
        VEC3 &angle                    = this->getAngle();
        VEC3 &scale                    = this->getScale();
        const float offset_x           = layer->offset[0] * scale.x;
        const float offset_y           = layer->offset[1] * scale.y;

        // Build a local position carrying z_value so the parent's position.z is
        // never mutated — that would corrupt the sort key for the next frame.
        VEC3 renderPos(position.x, position.y, z_value);

        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->disableFilteringForPixelPerfect();

        anim->updateAnimation(device->delta, this, this->getOnEndAnimation(), this->getOnEndFx());
        FX &fx = anim->getFx();
        fx.shader.update();
        this->setBlendState(anim->getBlendState());
        fx.setBlendOp();

        TEXTURE* textureAnimationEffect = fx.textureAnimationEffect ? fx.textureAnimationEffect : nullptr;
        VEC3 thePosBrick(renderPos);
        const MATRIX *matrixPerspective = nullptr;
        const MATRIX *viewMatrix = nullptr;
        const CAMERA &camera = device->getCamera();
        if (this->is3DObject())
        {
            MatrixTranslationRotationScale(&SHADER::modelView, &renderPos, &angle, &scale);
            matrixPerspective = &camera.matrixPerspective;
            viewMatrix = &camera.matrixView;
        }
        else if(this->is2dScreenObject())
        {
            thePosBrick = VEC3(position.x * camera.scaleScreen2d.x,
                                    position.y * camera.scaleScreen2d.y, z_value);
            device->transformeScreen2dToWorld2d_scaled(position.x, position.y, thePosBrick);
            MatrixTranslationRotationScale(&SHADER::modelView, &thePosBrick, &angle, &scale);
            matrixPerspective = &camera.matrixPerspective2d;
            viewMatrix = &camera.matrixView2d;
        }
        else
        {
            MatrixTranslationRotationScale(&SHADER::modelView, &renderPos, &angle, &scale);
            matrixPerspective = &camera.matrixPerspective2d;
            viewMatrix = &camera.matrixView2d;
        }
        const bool render_left_to_right = ptr_TileInfo->map.renderDirection[0] == 1; // render_left_to_right == 1
        const bool render_top_to_down   = ptr_TileInfo->map.renderDirection[1] == 1; // render_top_to_down == 1

        if(ptr_TileInfo->map.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
        {
            if(render_left_to_right)
            {
                if(render_top_to_down)
                {
                    for (uint32_t j = ptr_TileInfo->map.count_height_tile; j > 0; j--)
                    {
                        for (uint32_t i = 0; i < ptr_TileInfo->map.count_width_tile; i++)
                        {
                            if (renderBrick(ptr_TileInfo,
                                layer->lsIndexTiles,
                                &fx.shader,
                                textureAnimationEffect,
                                i,
                                j - 1,
                                offset_x,
                                offset_y,
                                &thePosBrick,
                                viewMatrix,
                                matrixPerspective) == false)
                            {
                                device->enableFilteringAfterPixelPerfect();
                                return false;
                            }
                        }
                    }
                }
                else
                {
                    for (uint32_t j = 0; j < ptr_TileInfo->map.count_height_tile; j++)
                    {
                        for (uint32_t i = 0; i < ptr_TileInfo->map.count_width_tile; i++)
                        {
                            if (renderBrick(ptr_TileInfo,
                                layer->lsIndexTiles,
                                &fx.shader,
                                textureAnimationEffect,
                                i,
                                j,
                                offset_x,
                                offset_y,
                                &thePosBrick,
                                viewMatrix,
                                matrixPerspective) == false)
                            {
                                device->enableFilteringAfterPixelPerfect();
                                return false;
                            }
                        }
                    }
                }
            }
            else
            {
                if(render_top_to_down)
                {
                    for (uint32_t j = ptr_TileInfo->map.count_height_tile; j > 0; j--)
                    {
                        for (uint32_t i = ptr_TileInfo->map.count_width_tile; i > 0; i--)
                        {
                            if (renderBrick(ptr_TileInfo,
                                layer->lsIndexTiles,
                                &fx.shader,
                                textureAnimationEffect,
                                i - 1,
                                j - 1,
                                offset_x,
                                offset_y,
                                &thePosBrick,
                                viewMatrix,
                                matrixPerspective) == false)
                            {
                                device->enableFilteringAfterPixelPerfect();
                                return false;
                            }
                        }
                    }
                }
                else
                {
                    for (uint32_t j = 0; j < ptr_TileInfo->map.count_height_tile; j++)
                    {
                        for (uint32_t i = ptr_TileInfo->map.count_width_tile; i > 0; i--)
                        {
                            if (renderBrick(ptr_TileInfo,
                                layer->lsIndexTiles,
                                &fx.shader,
                                textureAnimationEffect,
                                i - 1,
                                j,
                                offset_x,
                                offset_y,
                                &thePosBrick,
                                viewMatrix,
                                matrixPerspective) == false)
                            {
                                device->enableFilteringAfterPixelPerfect();
                                return false;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            if(render_left_to_right)
            {
                if(render_top_to_down)
                {
                    for (uint32_t j = ptr_TileInfo->map.count_height_tile; j > 0; j--)
                    {
                        for (uint32_t i = 0; i < ptr_TileInfo->map.count_width_tile; i++)
                        {
                            if (renderBrick(ptr_TileInfo,
                                layer->lsIndexTiles,
                                &fx.shader,
                                textureAnimationEffect,
                                i,
                                j - 1,
                                offset_x,
                                offset_y,
                                &thePosBrick,
                                viewMatrix,
                                matrixPerspective) == false)
                            {
                                device->enableFilteringAfterPixelPerfect();
                                return false;
                            }
                        }
                    }
                }
                else
                {
                    for (uint32_t j = 0; j < ptr_TileInfo->map.count_height_tile; j++)
                    {
                        for (uint32_t i = 0; i < ptr_TileInfo->map.count_width_tile; i++)
                        {
                            if (renderBrick(ptr_TileInfo,
                                layer->lsIndexTiles,
                                &fx.shader,
                                textureAnimationEffect,
                                i,
                                j,
                                offset_x,
                                offset_y,
                                &thePosBrick,
                                viewMatrix,
                                matrixPerspective) == false)
                            {
                                device->enableFilteringAfterPixelPerfect();
                                return false;
                            }
                        }
                    }
                }
            }
            else
            {
                if(render_top_to_down)
                {
                    for (uint32_t j = ptr_TileInfo->map.count_height_tile; j > 0; j--)
                    {
                        for (uint32_t i = ptr_TileInfo->map.count_width_tile; i > 0; i--)
                        {
                            if (renderBrick(ptr_TileInfo,
                                layer->lsIndexTiles,
                                &fx.shader,
                                textureAnimationEffect,
                                i - 1,
                                j - 1,
                                offset_x,
                                offset_y,
                                &thePosBrick,
                                viewMatrix,
                                matrixPerspective) == false)
                            {
                                device->enableFilteringAfterPixelPerfect();
                                return false;
                            }
                        }
                    }
                }
                else
                {
                    for (uint32_t j = 0; j < ptr_TileInfo->map.count_height_tile; j++)
                    {
                        for (uint32_t i = ptr_TileInfo->map.count_width_tile; i > 0; i--)
                        {
                            if (renderBrick(ptr_TileInfo,
                                layer->lsIndexTiles,
                                &fx.shader,
                                textureAnimationEffect,
                                i - 1,
                                j,
                                offset_x,
                                offset_y,
                                &thePosBrick,
                                viewMatrix,
                                matrixPerspective) == false)
                            {
                                device->enableFilteringAfterPixelPerfect();
                                return false;
                            }
                        }
                    }
                }
            }
        }
        device->enableFilteringAfterPixelPerfect();
        return true;
    }

    inline bool TILE::renderBrick( const util::BTILE_INFO * ptr_TileInfo, 
                            const util::BTILE_INDEX_TILE * lsIndexTiles,
                            const mbm::SHADER * shader,
                            TEXTURE* textureAnimationEffect,
                            const uint32_t i, 
                            const uint32_t j,
                            const float offset_x,
                            const float offset_y,
                            const VEC3 * pos,
                            const MATRIX *viewMatrix,
                            const MATRIX *matrixPerspective) const
    {
        const uint32_t index     = j + (i  * ptr_TileInfo->map.count_height_tile);
        const auto * bTileIndex  = &lsIndexTiles[index];
        if(bTileIndex->index < ptr_TileInfo->map.countRawTiles)
        {
            const VEC3 &scale = this->getScale();
            SHADER::modelView._41 = (bTileIndex->x * scale.x) + offset_x + pos->x;
            SHADER::modelView._42 = (bTileIndex->y * scale.y) + offset_y + pos->y;
            SHADER::updateMvpAndLightMatrices(*viewMatrix, *matrixPerspective);
            BUFFER_MESH *frameBuffer = this->mesh->getBuffer(bTileIndex->index);
            mbm::bindTextureAnimationEffect(frameBuffer ? frameBuffer->getRenderBuffer() : nullptr, textureAnimationEffect);
            if (this->mesh->render(bTileIndex->index, shader, this) == false)
            {
                return false;
            }
        }
        return true;
    }
    
    bool TILE::onRestoreDevice()
    {
        // Preserve runtime z values (may have been changed via setZLayer)
        std::vector<float> savedLayerZ(lsLayerRenderizables.size());
        for (size_t i = 0; i < lsLayerRenderizables.size(); ++i)
            savedLayerZ[i] = lsLayerRenderizables[i]->getPosition().z;
        // Destroy layer renderizables; load() will recreate them
        for (auto* l : lsLayerRenderizables)
            delete l;
        lsLayerRenderizables.clear();

        this->mesh = nullptr;
        const char *internalFileName = this->getInternalFileName();
        if(this->load(internalFileName))
        {
            // Restore user-modified z values (load resets them from file data)
            for (size_t i = 0; i < lsLayerRenderizables.size() && i < savedLayerZ.size(); ++i)
                lsLayerRenderizables[i]->getPosition().z = savedLayerZ[i];
            #if defined DEBUG
            PRINT_INFO_IF_DEBUG( "Tile [%s] successfully restored", log_util::basename(internalFileName));
            #endif
            for( auto & tileObj : lsTileObjs)
            {
                tileObj->ptr_Mesh = this->mesh;
            }
            return true;
        }
        else
        {
            #if defined DEBUG
            PRINT_IF_DEBUG( "Failed to restore tile  [%s]", log_util::basename(internalFileName));
            #endif
            return false;
        }
        
    }
    
    const mbm::INFO_PHYSICS * TILE::getInfoPhysics() const
    {
        if(this->mesh)
            return &this->mesh->getPhysicsInfo();
        return nullptr;
    }
    
    const MESH_MBM * TILE::getMesh() const
    {
        return this->mesh;
    }
    
    bool TILE::isLoaded() const
    {
        return this->mesh != nullptr;
    }

    FVF_PROVIDE_BY_ENGINE TILE::getFvfFromBuffer() const noexcept
    {
        if (mesh)
        {
            BUFFER_MESH* buf = mesh->getBuffer(0);
            if (buf && buf->hasLoadedRenderBuffer())
                return buf->getRenderBuffer()->fvf;
        }
        return FVF_PROVIDE_BY_ENGINE::FVF_NONE;
    }

    const util::BTILE_INFO *	TILE::getTileInfo() const
    {
        if(this->mesh)
        {
            const util::BTILE_INFO * ptr_TileInfo = clone_bTileInfo ? clone_bTileInfo : mesh->getInfoTile();
            return ptr_TileInfo;
        }
        return nullptr;
    }

    void TILE::setLayerVisible(const unsigned int index, const bool visible)
    {
        if (index < lsVisible.size())
            lsVisible[index] = visible;
        if (index < lsLayerRenderizables.size())
            lsLayerRenderizables[index]->setEnableRender(visible);
    }

    unsigned int TILE::getTotalLayer()const
    {
        return static_cast<unsigned int>(lsVisible.size()); //we can trust on it
    }

    uint16_t TILE::getTileID(const float x, const float y,const uint32_t index_layer,uint16_t * brick_ID_found) const
    {
        const util::BTILE_INFO * ptr_cTileInfo = (this->mesh ? mesh->getInfoTile() : nullptr);
        if(ptr_cTileInfo == nullptr)
            return std::numeric_limits<uint16_t>::max();
        if(index_layer >= ptr_cTileInfo->map.layerCount)
            return std::numeric_limits<uint16_t>::max();
        mbm::DEVICE* device     = mbm::DEVICE::getInstance();
        const int total         = ptr_cTileInfo->map.count_width_tile * ptr_cTileInfo->map.count_height_tile;
        const auto * layer      = &ptr_cTileInfo->layers[index_layer];
        const VEC3 &scale       = this->getScale();
        VEC2 pos;
        device->transformeScreen2dToWorld2d_scaled(x,y,pos);
        pos.x                  -= layer->offset[0];
        pos.y                  -= layer->offset[1];
        const float width_tile  = static_cast<float>(ptr_cTileInfo->map.size_width_tile  * scale.x);
        const float height_tile = static_cast<float>(ptr_cTileInfo->map.size_height_tile * scale.y);
        const float width_map   = static_cast<float>(width_tile  * ptr_cTileInfo->map.count_width_tile);
        const float height_map  = static_cast<float>(height_tile * ptr_cTileInfo->map.count_height_tile);
        const float initial_x   = width_map  * -0.5f + (width_tile  );
        const float initial_y   = height_map * -0.5f + (height_tile );
        const float xMin        = initial_x - width_tile;
        const float yMin        = initial_y - height_tile;
        const float xMax        = initial_x + width_map - width_tile;
        const float yMax        = initial_y + height_map - height_tile;

        if(pos.x >= xMin && pos.y >= yMin && pos.x <= xMax && pos.y <= yMax)
        {
            pos.y       = std::ceil(pos.y / height_tile) * height_tile;
            const int j = static_cast<int>((pos.y - initial_y + (height_tile * 0.5f)) / height_tile);
            if(ptr_cTileInfo->map.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
            {
                float displacement = 0.0f;
                if(j % 2 == 0)
                    displacement = width_tile * 0.5f;
                pos.x = (std::ceil((pos.x - displacement) / width_tile)  * width_tile);
            }
            else
            {
                pos.x = std::ceil(pos.x / width_tile)  * width_tile;
            }

            const int i = static_cast<int>((pos.x - initial_x + (width_tile  * 0.5f)) / width_tile);
            const int index = j + (i * ptr_cTileInfo->map.count_height_tile);
            if(index >= 0 && index < total)
            {
                if(brick_ID_found)
                {
                    const util::BTILE_INDEX_TILE * indexTile = & layer->lsIndexTiles[index];
                    *brick_ID_found                          = indexTile->index;
                }
                return static_cast<uint16_t>(index);
            }
        }
        return std::numeric_limits<uint16_t>::max();
    }

    uint32_t TILE::disableRenderTile(const int x, const int y,const int indexLayer)
    {
        const util::BTILE_INFO * ptr_cTileInfo = (this->mesh ? mesh->getInfoTile() : nullptr);
        if(ptr_cTileInfo == nullptr)
            return std::numeric_limits<uint32_t>::max();
        if(clone_bTileInfo == nullptr)
            clone_bTileInfo = ptr_cTileInfo->clone();
        const auto iTotalTile = clone_bTileInfo->map.count_width_tile * clone_bTileInfo->map.count_height_tile;
        if (indexLayer < 0)
        {
            uint32_t atLeastOne = std::numeric_limits<uint32_t>::max();
            for (unsigned int i = 0; i < clone_bTileInfo->map.layerCount; ++i)
            {
                auto index = getTileID(static_cast<float>(x),static_cast<float>(y),i,nullptr);
                if(index < iTotalTile)
                {
                    util::BTILE_INDEX_TILE * indexTile = & clone_bTileInfo->layers[i].lsIndexTiles[index];
                    indexTile->index = std::numeric_limits<uint16_t>::max() - 1;
                    atLeastOne = index;
                }
            }
            return atLeastOne;
        }
        else if (indexLayer < static_cast<int>(clone_bTileInfo->map.layerCount))
        {
            auto index = getTileID(static_cast<float>(x),static_cast<float>(y),indexLayer,nullptr);
            if(index < iTotalTile)
            {
                util::BTILE_INDEX_TILE * indexTile = & clone_bTileInfo->layers[indexLayer].lsIndexTiles[index];
                indexTile->index = std::numeric_limits<uint16_t>::max() - 1;
                return index;
            }
        }
        return std::numeric_limits<uint32_t>::max();
    }

    uint32_t TILE::enableRenderTile (const int x,const int y,const int indexLayer)
    {
        const util::BTILE_INFO * that_cTileInfo = (this->mesh ? mesh->getInfoTile() : nullptr);
        if(that_cTileInfo == nullptr)
            return std::numeric_limits<uint32_t>::max();
        if(clone_bTileInfo == nullptr)
            clone_bTileInfo = that_cTileInfo->clone();
        util::BTILE_INFO * this_cTileInfo = clone_bTileInfo;
        const auto iTotalTile = clone_bTileInfo->map.count_width_tile * clone_bTileInfo->map.count_height_tile;
        if (indexLayer < 0)
        {
            uint32_t atLeastOne = std::numeric_limits<uint32_t>::max();
            for (unsigned int i = 0; i < clone_bTileInfo->map.layerCount; ++i)
            {
                const util::BTILE_LAYER * that_layer = &that_cTileInfo->layers[i];
                util::BTILE_LAYER * this_layer       = &this_cTileInfo->layers[i];
                auto index = getTileID(static_cast<float>(x),static_cast<float>(y),i,nullptr);
                if(index < iTotalTile)
                {
                    util::BTILE_INDEX_TILE * this_indexTile			= & this_layer->lsIndexTiles[index];
                    const util::BTILE_INDEX_TILE * that_indexTile	= & that_layer->lsIndexTiles[index];
                    this_indexTile->index                           = that_indexTile->index;
                    atLeastOne = index;
                }
            }
            return atLeastOne;
        }
        else if (indexLayer < static_cast<int>(clone_bTileInfo->map.layerCount))
        {
            auto index = getTileID(static_cast<float>(x),static_cast<float>(y),indexLayer,nullptr);
            if(index < iTotalTile)
            {
                const util::BTILE_LAYER * that_layer          = &that_cTileInfo->layers[indexLayer];
                util::BTILE_LAYER * this_layer                = &this_cTileInfo->layers[indexLayer];
                const util::BTILE_INDEX_TILE * that_indexTile = & that_layer->lsIndexTiles[index];
                util::BTILE_INDEX_TILE * this_indexTile		  = & this_layer->lsIndexTiles[index];
                this_indexTile->index                         = that_indexTile->index;
                return index;
            }
        }
        return std::numeric_limits<uint32_t>::max();
    }

    std::vector<std::tuple<uint32_t,float,float>> TILE::getPositionsFromBrickID(const uint32_t brickID,const int indexLayer)const
    {
        std::vector<std::tuple<uint32_t,float,float>> positionsIdOut;
        const auto * ptr_TileInfo = this->getTileInfo();
        if(ptr_TileInfo == nullptr)
            return positionsIdOut;
        const VEC3 &scale = this->getScale();
        
        if (indexLayer < 0)
        {
            for (unsigned int k = 0; k < ptr_TileInfo->map.layerCount; ++k)
            {
                const util::BTILE_LAYER * layer = &ptr_TileInfo->layers[k];
                for (uint32_t i = 0; i < ptr_TileInfo->map.count_width_tile; i++)
                {
                    for (uint32_t j = 0; j < ptr_TileInfo->map.count_height_tile; j++)
                    {
                        const uint32_t index = j + (i  * ptr_TileInfo->map.count_height_tile);
                        auto & bTileIndex    = layer->lsIndexTiles[index];
                        if(bTileIndex.index == brickID)
                        {
                            positionsIdOut.emplace_back(index,bTileIndex.x * scale.x,bTileIndex.y * scale.y );
                        }
                    }
                }
            }
        }
        else if (indexLayer < static_cast<int>(ptr_TileInfo->map.layerCount))
        {
            const util::BTILE_LAYER * layer = &ptr_TileInfo->layers[indexLayer];
            for (uint32_t i = 0; i < ptr_TileInfo->map.count_width_tile; i++)
            {
                for (uint32_t j = 0; j < ptr_TileInfo->map.count_height_tile; j++)
                {
                    const uint32_t index = j + (i  * ptr_TileInfo->map.count_height_tile);
                    auto & bTileIndex    = layer->lsIndexTiles[index];
                    if(bTileIndex.index == brickID)
                    {
                        positionsIdOut.emplace_back(index,bTileIndex.x * scale.x,bTileIndex.y * scale.y );
                    }
                }
            }
        }
        return positionsIdOut;
    }

    FX*  TILE::getFx()const
    {
        auto * anim = getAnimation();
        if (anim)
            return &anim->getFx();
        return nullptr;
    }

    ANIMATION_MANAGER*  TILE::getAnimationManager()
    {
        return this;
    }

    void TILE::getTileSize(float & width, float & height)const
    {
        const auto * ptr_TileInfo = this->getTileInfo();
        if(ptr_TileInfo)
        {
            const VEC3 &scale = this->getScale();
            width  = static_cast<float>(ptr_TileInfo->map.size_width_tile * scale.x);
            height = static_cast<float>(ptr_TileInfo->map.size_height_tile * scale.y);
        }
    }

    bool TILE::getPositionFromTileID(const uint32_t tileID, float & x, float & y)
    {
        const auto * ptr_TileInfo = this->getTileInfo();
        if(ptr_TileInfo && tileID < (ptr_TileInfo->map.count_height_tile * ptr_TileInfo->map.count_width_tile) && ptr_TileInfo->map.layerCount > 0)
        {
            const VEC3 &position = this->getPosition();
            const VEC3 &scale = this->getScale();
            x = (ptr_TileInfo->layers[0].lsIndexTiles[tileID].x * scale.x) + position.x;
            y = (ptr_TileInfo->layers[0].lsIndexTiles[tileID].y * scale.y) + position.y;
            return true;
        }
        return false;
    }

    void TILE::getNearPosition(float & x_in_out, float & y_in_out,const uint32_t indexLayer) const
    {
        const auto * ptr_TileInfo = this->getTileInfo();
        if(ptr_TileInfo && indexLayer <  ptr_TileInfo->map.layerCount)
        {
            const auto iTotalTile = ptr_TileInfo->map.count_width_tile * ptr_TileInfo->map.count_height_tile;
            uint16_t tile_ID_found = getTileID( x_in_out, y_in_out,indexLayer, nullptr);
            if(tile_ID_found < iTotalTile)
            {
                const util::BTILE_LAYER* layer = & ptr_TileInfo->layers[indexLayer];
                const VEC3 &position = this->getPosition();
                const VEC3 &scale = this->getScale();
                x_in_out = (layer->lsIndexTiles[tile_ID_found].x * scale.x) + position.x;
                y_in_out = (layer->lsIndexTiles[tile_ID_found].y * scale.y) + position.y;
            }
        }
    }

    void TILE::getMapSize(float & width, float & height)const
    {
        const auto * ptr_TileInfo = this->getTileInfo();
        if(ptr_TileInfo)
        {
            const VEC3 &scale = this->getScale();
            width  = static_cast<float>(ptr_TileInfo->map.size_width_tile  * scale.x * ptr_TileInfo->map.count_width_tile);
            height = static_cast<float>(ptr_TileInfo->map.size_height_tile * scale.y * ptr_TileInfo->map.count_height_tile);
            if(ptr_TileInfo->map.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
            {
                const float width_tile    = static_cast<float>(ptr_TileInfo->map.size_width_tile  * scale.x);
                const float height_tile   = static_cast<float>(ptr_TileInfo->map.size_height_tile * scale.y);
                width  += width_tile  * 0.5f;
                height += height_tile;
            }
        }
    }

    float TILE::getZLayer(const uint32_t indexLayer) const
    {
        if (indexLayer < lsLayerRenderizables.size())
            return lsLayerRenderizables[indexLayer]->getPosition().z;
        return 0.0f;
    }

    void  TILE::setZLayer(const uint32_t indexLayer, const float z)
    {
        if (indexLayer < lsLayerRenderizables.size())
            lsLayerRenderizables[indexLayer]->getPosition().z = z;
    }


    bool TILE::setTileID(const int x,const int y,const uint32_t brickID,const uint32_t indexLayer)
    {
        const util::BTILE_INFO * ptr_TileInfo = this->mesh ? mesh->getInfoTile() : nullptr;
        if(ptr_TileInfo == nullptr)
            return false;
        if(clone_bTileInfo == nullptr)
            clone_bTileInfo = ptr_TileInfo->clone();
        const auto iTotalTile = clone_bTileInfo->map.count_width_tile * clone_bTileInfo->map.count_height_tile;
        if(brickID >= clone_bTileInfo->map.countRawTiles)
            return false;
        uint16_t tile_ID_found = getTileID(static_cast<float>(x),static_cast<float>(y),indexLayer, nullptr);
        if(tile_ID_found < iTotalTile)
        {
            util::BTILE_INDEX_TILE * indexTile = & clone_bTileInfo->layers[indexLayer].lsIndexTiles[tile_ID_found];
            indexTile->index = brickID;
            return true;
        }
        return false;
    }

    TILE_OBJ * TILE::buildTileRenderizable(const uint32_t tileID,const int indexLayer)
    {
        if(this->isLoaded())
        {
            if(this->clone_bTileInfo == nullptr)
            {
                const auto * ptr_TileInfo = this->mesh->getInfoTile();
                if(ptr_TileInfo == nullptr)
                    return nullptr;
                this->clone_bTileInfo     = ptr_TileInfo->clone();
            }
            TILE_OBJ * tObj = new TILE_OBJ(this,this->mesh,tileID,indexLayer);
            lsTileObjs.push_back(tObj);
            return tObj;
        }
        return nullptr;
    }

    TILE_LAYER* TILE::getLayerRenderizable(uint32_t index) const noexcept
    {
        if (index < lsLayerRenderizables.size())
            return lsLayerRenderizables[index];
        return nullptr;
    }

    // -------------------------------------------------------------------------
    // TILE_LAYER implementation
    // -------------------------------------------------------------------------

    TILE_LAYER::TILE_LAYER(TILE* parent, uint32_t layerIndex)
    : RENDERIZABLE(parent->getIdScene(), TYPE_CLASS_TILE_LAYER,
                   parent->is3DObject(), parent->is2dScreenObject())
    , indexLayer(layerIndex)// indexLayer is stored here, immutable for the lifetime of this object
    , ptr_tileMap(parent)
    {
        // Use the layer's z offset if non-zero; otherwise leave at 0 so
        // DEVICE::addRenderizable auto-assigns a decreasing z value.
        const auto* ptr_TileInfo = parent->getTileInfo();
        if (ptr_TileInfo && layerIndex < ptr_TileInfo->map.layerCount)
        {
            const float layerZ = ptr_TileInfo->layers[layerIndex].offset[2];
            if (layerZ != 0.0f)
                this->getPosition().z = layerZ;
        }
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->addRenderizable(this);
    }

    TILE_LAYER::~TILE_LAYER()
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->removeRenderizable(this);
    }

    FVF_PROVIDE_BY_ENGINE TILE_LAYER::getFvfFromBuffer() const noexcept
    {
        return ptr_tileMap->getFvfFromBuffer();
    }

    const INFO_PHYSICS* TILE_LAYER::getInfoPhysics() const
    {
        return ptr_tileMap->getInfoPhysics();
    }

    const MESH_MBM* TILE_LAYER::getMesh() const
    {
        return ptr_tileMap->getMesh();
    }

    bool TILE_LAYER::isLoaded() const
    {
        return ptr_tileMap->isLoaded();
    }

    FX* TILE_LAYER::getFx() const
    {
        ANIMATION* anim = ptr_tileMap->getAnimation(indexLayer);
        if (anim)
            return &anim->getFx();
        return nullptr;
    }

    ANIMATION_MANAGER* TILE_LAYER::getAnimationManager()
    {
        return ptr_tileMap->getAnimationManager();
    }

    uint32_t TILE_LAYER::getLayerIndex() const noexcept
    {
        return indexLayer;
    }

    bool TILE_LAYER::isOnFrustum()
    {
        if (!ptr_tileMap->isRenderEnabled())
            return false;
        if (!ptr_tileMap->isLoaded())
            return false;
        // All layers share the same spatial extent as the parent TILE.
        IS_ON_FRUSTUM verify(ptr_tileMap);
        bool ret = verify.isOnFrustum(ptr_tileMap->is3DObject(), ptr_tileMap->is2dScreenObject());
        if (!ret)
        {
            // Update this layer's animation even when off-screen so timing stays correct.
            ANIMATION* anim = ptr_tileMap->getAnimation(indexLayer);
            if (anim)
            {
                mbm::DEVICE* device = mbm::DEVICE::getInstance();
                anim->updateAnimation(device->delta, ptr_tileMap,
                                      ptr_tileMap->getOnEndAnimation(),
                                      ptr_tileMap->getOnEndFx());
            }
        }
        return ret;
    }

    bool TILE_LAYER::render()
    {
        return ptr_tileMap->renderLayer(indexLayer, this->getPosition().z);
    }

    bool TILE_LAYER::onRestoreDevice()
    {
        return true; // The parent TILE handles mesh/texture restore.
    }

    // -------------------------------------------------------------------------

    TILE_OBJ::TILE_OBJ(TILE* tileMap, MESH_MBM * pMesh, const uint32_t tileID, const uint32_t indexLayer)
    : RENDERIZABLE(tileMap->getIdScene(), TYPE_CLASS_TILE_OBJ, tileMap->is3DObject() && tileMap->is2dScreenObject() == false, tileMap->is2dScreenObject()),
      brickID(std::numeric_limits<uint32_t>::max()),
      index_layer(indexLayer),
      ptr_tileMap(tileMap),
      ptr_Mesh(pMesh)
    {
        const auto * ptr_TileInfo  = pMesh->getInfoTile();
        auto * ptr_cTileInfo       = ptr_tileMap->getTileInfo();
        if(ptr_TileInfo && ptr_cTileInfo)
        {
            const int total = ptr_cTileInfo->map.count_width_tile * ptr_cTileInfo->map.count_height_tile;
            if(indexLayer < ptr_cTileInfo->map.layerCount)
            {
                const auto * layer_original = &ptr_TileInfo->layers[indexLayer];
                auto * layer_clone          = &ptr_cTileInfo->layers[indexLayer];
                if(tileID < static_cast<uint32_t>(total))
                {
                    VEC3 &position                   = this->getPosition();
                    VEC3 &scale                      = this->getScale();
                    const VEC3 &tileScale            = ptr_tileMap->getScale();
                    const auto * indexTile_original = & layer_original->lsIndexTiles[tileID];
                    auto * indexTile_clone          = & layer_clone->lsIndexTiles[tileID];
                    brickID                         = indexTile_original->index;
                    indexTile_clone->index          = std::numeric_limits<uint32_t>::max();
                    position.x                      = indexTile_clone->x * tileScale.x;
                    position.y                      = indexTile_clone->y * tileScale.y;
                    position.z                      = layer_original->offset[2];
                    scale.x                         = tileScale.x;
                    scale.y                         = tileScale.y;
                    scale.z                         = tileScale.z;
                    std::string  physic_name("physic-");
                    physic_name                    += std::to_string(brickID);

                    for(auto & tObj : ptr_TileInfo->lsObj)
                    {
                        if(tObj->name.compare(physic_name) == 0)
                        {
                            switch(tObj->type)
                            {
                                case util::BTILE_OBJ_TYPE_RECT:
                                {
                                    mbm::CUBE *  cube =  new mbm::CUBE();
                                    cube->absCenter.x = tObj->lsPoints[0]->x * scale.x;
                                    cube->absCenter.y = tObj->lsPoints[0]->y * scale.y;
                                    cube->halfDim.x   = tObj->lsPoints[1]->x * 0.5f * scale.x;
                                    cube->halfDim.y   = tObj->lsPoints[1]->y * 0.5f * scale.y;
                                    infoPhysics.lsCube.push_back(cube);
                                }
                                break;
                                case util::BTILE_OBJ_TYPE_CIRCLE:
                                {
                                    mbm::SPHERE* sphere  = new mbm::SPHERE();
                                    sphere->absCenter[0] = tObj->lsPoints[0]->x * scale.x;
                                    sphere->absCenter[1] = tObj->lsPoints[0]->y * scale.y;
                                    sphere->ray          = tObj->lsPoints[1]->x * scale.x;
                                    infoPhysics.lsSphere.push_back(sphere);
                                }
                                break;
                                case util::BTILE_OBJ_TYPE_TRIANGLE:
                                {
                                    mbm::TRIANGLE* triangle = new mbm::TRIANGLE();
                                    triangle->point[0].x    = tObj->lsPoints[0]->x * scale.x;
                                    triangle->point[0].y    = tObj->lsPoints[0]->y * scale.y;
                                    triangle->point[1].x    = tObj->lsPoints[1]->x * scale.x;
                                    triangle->point[1].y    = tObj->lsPoints[1]->y * scale.y;
                                    triangle->point[2].x    = tObj->lsPoints[2]->x * scale.x;
                                    triangle->point[2].y    = tObj->lsPoints[2]->y * scale.y;
                                    infoPhysics.lsTriangle.push_back(triangle);
                                }
                                break;
                                default:{}
                            }
                        }
                    }
                }
            }
        }
        auto * device = DEVICE::getInstance();
        device->addRenderizable(this);
        this->updateAABB();
    }

    TILE_OBJ::~TILE_OBJ()
    {
        auto * device = DEVICE::getInstance();
        device->removeRenderizable(this);
    }

    void TILE_OBJ::setBrickID(const uint32_t _brickID)
    {
        brickID = _brickID;
    }

    const INFO_PHYSICS * TILE_OBJ::getInfoPhysics() const
    {
        return &this->infoPhysics;
    }

    const MESH_MBM * TILE_OBJ::getMesh() const
    {
        return ptr_Mesh;
    }

    FVF_PROVIDE_BY_ENGINE TILE_OBJ::getFvfFromBuffer() const noexcept
    {
        if (ptr_Mesh)
        {
            BUFFER_MESH* buf = ptr_Mesh->getBuffer(0);
            if (buf && buf->hasLoadedRenderBuffer())
                return buf->getRenderBuffer()->fvf;
        }
        return FVF_PROVIDE_BY_ENGINE::FVF_NONE;
    }

    bool TILE_OBJ::isLoaded() const
    {
        return true;
    }

    FX * TILE_OBJ::getFx() const
    {
        auto anim = ptr_tileMap->getAnimation(index_layer);
        if(anim)
            return &anim->getFx();
        return nullptr;
    }

    ANIMATION_MANAGER * TILE_OBJ::getAnimationManager()
    {
        return ptr_tileMap->getAnimationManager();
    }

    bool TILE_OBJ::isOnFrustum() 
    {
        IS_ON_FRUSTUM verify(this);
        return verify.isOnFrustum(this->is3DObject(), this->is2dScreenObject());
    }

    bool TILE_OBJ::render()
    {
        auto * ptr_cTileInfo = ptr_tileMap->getTileInfo();
        auto anim            = ptr_tileMap->getAnimation(index_layer);
        if(anim && index_layer < ptr_cTileInfo->map.layerCount && brickID < ptr_cTileInfo->map.countRawTiles)
        {
            FX &fx = anim->getFx();
            fx.shader.update();
            this->setBlendState(anim->getBlendState());
            fx.setBlendOp();
            auto * device = DEVICE::getInstance();
            const CAMERA &camera = device->getCamera();
            VEC3 &position = this->getPosition();
            VEC3 &angle = this->getAngle();
            VEC3 &scale = this->getScale();
            
            if (this->is3DObject())
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &position, &angle, &scale);
                SHADER::updateMvpAndLightMatrices(camera.matrixView, camera.matrixPerspective);
            }
            else if (this->is2dScreenObject())
            {
                VEC3 positionScreen(position.x * camera.scaleScreen2d.x,
                                    position.y * camera.scaleScreen2d.y, position.z);
                device->transformeScreen2dToWorld2d_scaled(position.x, position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &angle, &scale);
                SHADER::updateMvpAndLightMatrices(camera.matrixView2d, camera.matrixPerspective2d);
            }
            else
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &position, &angle, &scale);
                SHADER::updateMvpAndLightMatrices(camera.matrixView2d, camera.matrixPerspective2d);
            }

            BUFFER_MESH *frameBuffer = this->ptr_Mesh->getBuffer(brickID);
            fx.bindTextureAnimationEffect(frameBuffer ? frameBuffer->getRenderBuffer() : nullptr);
            return this->ptr_Mesh->render(brickID, &fx.shader, this);
        }
        return false;
    }

    bool TILE_OBJ::onRestoreDevice() 
    {
        return true; //The parent will take care
    }

    //void TILE_OBJ::onStop()
    //{
    //    //do nothing
    //}
}
