/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT);                                                                                                     |
| Copyright (C); 2020      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software");, to deal in the Software without restriction, including without limitation       |
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

#ifndef TILED_HEADER_H
#define TILED_HEADER_H

#ifndef USE_EDITOR_FEATURES
#define USE_EDITOR_FEATURES
#endif

#include <core_mbm/device.h>
#include <core_mbm/shader-fx.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/physics.h>
#include <core_mbm/shader.h>
#include <render/line-mesh.h>

#include <vector>
#include <memory>

#include "tiled_map.h"

#include "layer.h"
#include "tile_set.h"

#define TILE_EDITOR_VERSION "1.0"

namespace mbm
{
    enum RENDER_WHAT
    {
        RENDER_MAP,
        RENDER_TILE_SET,
        RENDER_LAYER,
        RENDER_BRICK,
    };

    typedef std::map<std::string,std::shared_ptr<DYNAMIC_VAR>> PROPERTY;

    class TILE_EDITOR : public RENDERIZABLE, public COMMON_DEVICE, public ANIMATION_MANAGER
    {
    private:
        /* data */
    public:
        TILE_EDITOR(const SCENE *scene); // always 2dw
        ~TILE_EDITOR();

        const INFO_PHYSICS *getInfoPhysics() const override;
        const MESH_MBM *    getMesh() const        override;
        bool                isLoaded() const       override;
        FX*                 getFx() const          override;
        ANIMATION_MANAGER*  getAnimationManager()  override;

        void release();

        RENDER_WHAT render_what;
        uint32_t    index_render_what;
        VEC2 scale_map;
        VEC2 scale_tile;
        VEC2 scale_layer;
        VEC2 scale_brick;
        VEC2 cam_layer_pos;
        VEC2 cam_map_pos;
        VEC2 cam_tileset_pos;


        // Map Accessors
        inline uint32_t     getMapCountWidth() const                { return tileMap.count_width_tile; }
        inline void         setMapCountWidth(const uint32_t value)  { if(value > 0) tileMap.count_width_tile = value; }
        inline uint32_t     getMapCountHeight() const               { return tileMap.count_height_tile; }
        void                setMapCountHeight(const uint32_t value);

        inline uint32_t     getMapTileWidth() const                 { return tileMap.size_width_tile; }
        inline void         setMapTileWidth(const uint32_t value)   { if(value > 0) tileMap.size_width_tile = value; }
        inline uint32_t     getMapTileHeight() const                { return tileMap.size_height_tile; }
        inline void         setMapTileHeight(const uint32_t value)  { if(value > 0) tileMap.size_height_tile = value; }

        inline COLOR        getBackgroundColor() const              { return tileMap.background;}
        inline void         setBackgroundColor(COLOR & value)       { tileMap.background = value;}

        const char *        getBackgroundTexture() const;
        void                setBackgroundTexture(const char * name);

        inline PROPERTY &   getMapProperties()                      { return tileMap.properties;}
        PROPERTY &          getLayerProperties(const uint32_t index);
        void                setMapType(const char * type);
        const char*         getMapType() const;
        void                setDirectionMapRender(const bool left_to_right,const bool top_to_down);
        void                getDirectionMapRender(bool & left_to_right, bool & top_to_down)const;
        void                selectTiles(const VEC2 & start,const VEC2 & end);

        // TileSet
        bool                newTileSet( const char * tile_set_name,
                                        std::vector<std::string> & textures,
                                        const uint32_t width,
                                        const uint32_t height,
                                        const uint32_t space_x,
                                        const uint32_t space_y,
                                        const uint32_t margin_x,
                                        const uint32_t margin_y,
                                        const VEC2 * min_bound,
                                        const VEC2 * max_bound);

        uint32_t            getTileSetWidth(const uint32_t index) const;
        void                setTileSetWidth(const uint32_t index,const uint32_t value);
        uint32_t            getTileSetHeight(const uint32_t index) const;
        void                setTileSetHeight(const uint32_t index,const uint32_t value);
        const char*         getTileSetName(const uint32_t index) const;
        void                setTileSetName(const uint32_t index,const char* name);
        bool                existTileSet(const char* name) const;
        inline uint32_t     getTotalTileSet() const { return tileMap.tile_sets.size(); }
        void                setTileSetPreview(const char * fileNameTexture,
                                            const int32_t width,
                                            const int32_t height,
                                            const int32_t space_x,
                                            const int32_t space_y,
                                            const int32_t margin_x,
                                            const int32_t margin_y);
        uint32_t            getHowManyTile2Render() const;
        void                setHowManyTile2Render(const uint32_t value);

        // Bricks
        uint32_t            getTotalBricks(const char * filterTileSet) const;

        //this method works with absolute index when using filter (stored at vector). 
        // Use id (map) when no filter . (different place to search)
        std::shared_ptr<BRICK>      getBrick(const uint16_t index,const char * filterTileSet) const;
        uint16_t                    getBrickIDByTileID(const uint16_t index_tile) const;
        
        // Layer
        inline uint32_t             getTotalLayer() const { return tileMap.layers.size(); }
        bool                        isLayerVisible(const uint32_t index) const;
        COLOR                       getMinTintLayer(const uint32_t index) const;
        COLOR                       getMaxTintLayer(const uint32_t index) const;
        void                        setTintAnimTypeLayer(const uint32_t index,const TYPE_ANIMATION type);
        void                        setTintAnimTimeLayer(const uint32_t index,const float time);
        TYPE_ANIMATION              getTintAnimTypeLayer(const uint32_t index) const;
        float                       getTintAnimTimeLayer(const uint32_t index) const;
        const char *                getNamePsShaderLayer(const uint32_t index) const;
        VEC2                        getOffsetLayer(const uint32_t index) const;
        void                        setLayerVisible(const uint32_t index, bool value);
        void                        setMinTintLayer(const uint32_t index,   const COLOR & value);
        void                        setMaxTintLayer(const uint32_t index,   const COLOR & value);
        void                        setOffsetLayer(const uint32_t index, const VEC2 & value);
        bool                        existLayer(const uint32_t index) const;
        bool                        addLayer();
        void                        eraseLayer(const uint32_t index);
        void                        setOverBrick(const float x, const float y);
        uint32_t                    selectBrick(const int index,const bool unique);
        uint16_t                    getIndexTileIdOver(const float x, const float y) const;
        bool                        isBrickSelected(const uint16_t index) const;
        uint16_t                    getFirstSelectedBrick() const;
        inline void                 unselectBrick(const uint16_t index) {selectedBrick[index] = false;}
        inline const std::map<uint16_t,bool> & getSelectedBrickMap() const { return selectedBrick;}
        inline const uint16_t       getLastBrickOver() const { return iLastIndexBrickOver;}
        bool                        setBrick2Layer(uint16_t idBrick,uint32_t index_layer );
        void                        selectAllBricks();
        void                        invertSelectedBricks();
        void                        unselectAllBricks();
        void                        deleteSelectedBricks();
        uint16_t                    rotateSelectedBrick(const char * orientation);
        uint16_t                    flipSelectedBrick();
        void                        moveLayerUp(const uint32_t index_layer);
        void                        moveLayerDown(const uint32_t index_layer);
        void                        moveWholeLayerTo(const int32_t x,const int32_t y);
        void                        duplicateSelectedTiles(const float x, const float y);

        inline std::vector<std::shared_ptr<OBJ_TILE_MAP>>   &    getObjectsMap() {return tileMap.objects;}
        bool                        saveBinary(const char * fileName);
        bool                        loadBinary(const char * fileName);

        bool                        unDo();
        bool                        redDo();
        void                        addHistoric();
        void                        clearHistory();

    protected:
        TILED_MAP                   tileMap;
        VEC2                        position_aux_tileset;
        std::map<uint16_t,bool>     selectedBrick;
        std::map<uint16_t,VEC2>     positionSelectedBrick;
        uint16_t                    iLastIndexBrickOver;
        uint32_t                    id_texture_normal_brick, 
                                    id_texture_highlight_brick,
                                    id_texture_selected_brick;
        
        bool isOnFrustum()     override;
        bool render()          override;
        bool onRestoreDevice() override;
        void onStop()          override;

        bool renderMap(SHADER *shader);
        bool renderTileSet();
        bool renderLayer(const uint32_t index_layer, const bool enable_highlights,const VEC2 & scale_offset,const float transparency);
        bool renderBrickMap(const std::shared_ptr<LAYER> & layer,
                            const uint32_t i,
                            const uint32_t j,
                            const float width_tile,
                            const float height_tile,
                            const float initial_x,
                            const float initial_y, 
                            bool & updatedSelected, 
                            const bool enable_highlights,
                            const bool transparency);
        bool renderBrick();
        bool createAnim();

        bool renderEmptyBrick(SHADER *shader,bool highlight,bool selected);
        std::shared_ptr<BRICK> getBrickByOriginalId(const uint32_t original_id,const ROTATION_BRICK iRotation,const bool flipped) const;
        bool loadBufferGl(BUFFER_GL & whatBuffer);
        BUFFER_GL emptyBrick;
        BUFFER_GL backGroundMap;
        BUFFER_GL tileSetPreview;
        mbm::TEXTURE * textureTileSetPreview;
        mbm::LINE_MESH * line_tileSetPreview;
        std::vector<std::string> history_files;
        uint32_t index_history;
        uint32_t iTilesToRender;
        

    };
    
} 


#endif