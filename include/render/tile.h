/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2018 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                            |
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

#ifndef TILE_RENDER_GLES_H
#define TILE_RENDER_GLES_H

#include <core_mbm/core-exports.h>
#include <core_mbm/device.h>
#include <core_mbm/shader-fx.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/header-mesh.h>
#include <core_mbm/physics.h>
#include <core_mbm/texture-manager.h>
#include <tuple>
#include <vector>


namespace mbm
{

    class TILE;

    class TILE_OBJ : public RENDERIZABLE
    {
    friend class TILE;
    public:
        API_IMPL TILE_OBJ(TILE* tileMap,MESH_MBM * pMesh, const uint32_t tileID,const uint32_t indexLayer);
        API_IMPL virtual~TILE_OBJ();
        API_IMPL virtual const INFO_PHYSICS *getInfoPhysics() const override;
        API_IMPL virtual const MESH_MBM *    getMesh() const override;
        API_IMPL virtual bool                isLoaded() const override;
        API_IMPL FX*                         getFx() const override;
        API_IMPL ANIMATION_MANAGER*          getAnimationManager() override;
        API_IMPL void                        setBrickID(const uint32_t _brickID);

    protected:
        uint32_t brickID;
        uint32_t index_layer;
        virtual bool isOnFrustum() override;
        virtual bool render() override;
        virtual bool onRestoreDevice() override;
        virtual void onStop()override;
        INFO_PHYSICS infoPhysics;
        TILE *      ptr_tileMap;
        MESH_MBM *  ptr_Mesh;
    };

    class TILE : public RENDERIZABLE, public COMMON_DEVICE, public ANIMATION_MANAGER
    {
    public:
        friend class PHYSICS_BOX2D;
        friend class RENDER_2_TEXTURE;
        API_IMPL TILE(const SCENE *scene, const bool _is3d, const bool _is2dScreen);
        API_IMPL virtual ~TILE();
        API_IMPL void release();
        API_IMPL bool load(const char * fileName);
        API_IMPL const char * getFileName();
        API_IMPL const util::BTILE_INFO *	getTileInfo() const;
        API_IMPL TILE_OBJ* buildTileRenderizable(const uint32_t tileID,const int indexLayer);
        API_IMPL void     setLayerVisible(const uint32_t index,const bool visible);
        API_IMPL uint32_t getTotalLayer()const;
        API_IMPL uint32_t disableRenderTile(const int x,const int y,const int indexLayer);
        API_IMPL uint32_t enableRenderTile (const int x,const int y,const int indexLayer);
        API_IMPL uint16_t getTileID(const float x, const float y,const uint32_t index_layer,uint16_t * brick_ID_found) const;
        API_IMPL std::vector<std::tuple<uint32_t,float,float>> getPositionsFromBrickID(const uint32_t brickID,const int indexLayer)const;// layer ,x, y
        API_IMPL void getTileSize(float & width, float & height)const;
        API_IMPL void getMapSize(float & width, float & height)const;
        API_IMPL bool getPositionFromTileID(const uint32_t tileID,float & x, float & y);
        API_IMPL void getNearPosition(float & x_in_out, float & y_in_out,const uint32_t indexLayer) const;
        API_IMPL bool setTileID(const int x,const int y,const uint32_t brickID,const uint32_t indexLayer);
        API_IMPL float getZLayer(const uint32_t indexLayer) const;
        API_IMPL void  setZLayer(const uint32_t indexLayer, const float z);
        API_IMPL FX*  getFx() const override;
        API_IMPL ANIMATION_MANAGER*  getAnimationManager() override;

    private:
        bool                    isOnFrustum() override;
        bool                    render() override;
        bool                    renderLayer(const uint32_t index_layer);
        bool                    onRestoreDevice() override;
        void                    onStop() override;
        const INFO_PHYSICS *	getInfoPhysics() const override;
        const MESH_MBM *        getMesh() const override;
        bool                    isLoaded() const override;
        inline bool             renderBrick(const util::BTILE_INFO * ptr_TileInfo, 
                                            const util::BTILE_INDEX_TILE * lsIndexTiles,
                                            const mbm::SHADER * shader,
                                            const unsigned int idTextureOverrideStage2,
                                            const uint32_t i, 
                                            const uint32_t j,
                                            const float offset_x,
                                            const float offset_y,
                                            const VEC3 * pos,
                                            const MATRIX *matrixPerspective) const;
        MESH_MBM *              mesh;
        std::vector<TILE_OBJ*>	lsTileObjs;
        std::vector<bool>		lsVisible;
    private:
        util::BTILE_INFO *		  clone_bTileInfo;
        mbm::TEXTURE *            backgroundTextureMap;
        BUFFER_GL                 backGroundMap;
        bool                      loadBufferBackGroundTexture();
    };
}

#endif
