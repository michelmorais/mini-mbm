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


#include "tile_editor.hpp"
#include <limits>
#include <core_mbm/util-interface.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/shader-var-cfg.h>
#include <core_mbm/dynamic-var.h>
#include <algorithm>

template< typename T >
struct array_deleter
{
  void operator ()( T const * p)
  { 
    delete[] p; 
  }
};

#if defined _WIN32
    namespace std
    {
        float clamp(const float value,const float v_min, const float v_max)
        {
            float ret = value;
            if(ret > v_max)
                ret = v_max;
            if(ret < v_min)
                ret = v_min;
            return ret;
        }

        
        uint32_t clamp(const uint32_t value,const uint32_t v_min, const uint32_t v_max)
        {
            uint32_t ret = value;
            if(ret > v_max)
                ret = v_max;
            if(ret < v_min)
                ret = v_min;
            return ret;
        }
    }
#endif

namespace mbm
{
    TILE_EDITOR::TILE_EDITOR(const SCENE *scene)
    : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_TILE, false,false)
    {
        render_what                = RENDER_MAP;
        index_render_what          = 0;
        iTilesToRender             = 0;
        scale_map                  = VEC2(1,1);
        scale_tile                 = VEC2(1,1);
        scale_layer                = VEC2(1,1);
        scale_brick                = VEC2(1,1);
        id_texture_normal_brick    = 0;
        id_texture_highlight_brick = 0;
        id_texture_selected_brick  = 0;
        index_history              = 0;
        tileMap.background_texture = nullptr;
        textureTileSetPreview      = nullptr;
        line_tileSetPreview        = nullptr;
        iLastIndexBrickOver        = std::numeric_limits<uint16_t>::max()-1;
        this->device->addRenderizable(this);
    }
    
    TILE_EDITOR::~TILE_EDITOR()
    {
        this->device->removeRenderizable(this);
        this->release();
        this->clearHistory();
    }

    void TILE_EDITOR::release()
    {
        tileMap.release();
        emptyBrick.release();
        backGroundMap.release();
        tileSetPreview.release();
        index_history                 = 0;
        textureTileSetPreview      = nullptr;
        tileMap.background_texture = nullptr;
        tileMap.background = COLOR(0.0f,0.0f,0.0f,0.0f);
        if(line_tileSetPreview)
            delete line_tileSetPreview;
        line_tileSetPreview = nullptr;
        iTilesToRender      = 0;
    }

    const INFO_PHYSICS *TILE_EDITOR::getInfoPhysics() const
    {
        return nullptr;
    }

    const MESH_MBM *    TILE_EDITOR::getMesh() const
    {
        return nullptr;
    }

    bool                TILE_EDITOR::isLoaded() const
    {
        return false;
    }

    FX*                 TILE_EDITOR::getFx() const
    {
        return nullptr;
    }

    ANIMATION_MANAGER*  TILE_EDITOR::getAnimationManager()
    {
        return this;
    }

    const char * TILE_EDITOR::getBackgroundTexture() const
    { 
        return tileMap.background_texture ? tileMap.background_texture->getFileNameTexture() : nullptr;
    }

    void TILE_EDITOR::setBackgroundTexture(const char * name)        
    {   if(name) 
        {
            mbm::TEXTURE::enableFilter(true);
            mbm::TEXTURE * texture     = mbm::TEXTURE_MANAGER::getInstance()->load(name,true);
            if(texture)
                tileMap.background_texture = texture;
        }
        else
        {
            tileMap.background_texture = nullptr;
        }
    }

    uint32_t     TILE_EDITOR::getTileSetWidth(const uint32_t index) const
    {
        if(index < tileMap.tile_sets.size())
            return tileMap.tile_sets[index]->tile_width;
        return 0;
    }

    void         TILE_EDITOR::setTileSetWidth(const uint32_t index,const uint32_t value)      
    {   
        if(index < tileMap.tile_sets.size())
        {
            auto & tilset      = tileMap.tile_sets[index];
            tilset->tile_width = value;
            for(auto & brick : tilset->bricks )
            {
                brick->backup();
                brick->width = value;
                brick->build(nullptr,brick->uv);
            }
        }
    }

    uint32_t     TILE_EDITOR::getTileSetHeight(const uint32_t index) const
    { 
        if(index < tileMap.tile_sets.size())
            return tileMap.tile_sets[index]->tile_height;
        return 0;
    }

    void         TILE_EDITOR::setTileSetHeight(const uint32_t index,const uint32_t value)
    {
        if(index < tileMap.tile_sets.size())
        {
            auto & tilset       = tileMap.tile_sets[index];
            tilset->tile_height = value;
            for(auto & brick : tilset->bricks )
            {
                brick->backup();
                brick->height = value;
                brick->build(nullptr,brick->uv);
            }
        }
    }

    const char*         TILE_EDITOR::getTileSetName(const uint32_t index) const
    {
        if(index < tileMap.tile_sets.size())
            return tileMap.tile_sets[index]->name.c_str();
        return 0;
    }
    void                TILE_EDITOR::setTileSetName(const uint32_t index,const char* name)
    {
        if(name && index < tileMap.tile_sets.size())
        {
            if(existTileSet(name) == false)
                tileMap.tile_sets[index]->name = name;
        }
    }

    bool                TILE_EDITOR::existTileSet(const char* name) const
    {
        for(auto & tile : tileMap.tile_sets)
        {
            if(tile->name.compare(name) == 0)
                return true;
        }
        return false;
    }

    uint32_t            TILE_EDITOR::getTotalBricks(const char * filterTileSet) const
    { 
        if(filterTileSet == nullptr)
            return tileMap.bricks.size();
        std::shared_ptr<TILED_SET> tileset;
        for(auto & ts : this->tileMap.tile_sets)
        {
            if(ts->name.compare(filterTileSet) == 0)
            {
                tileset = ts;
            }
        }
        if(tileset)
        {
            return tileset->bricks.size();
        }
        return 0;
    }

    bool         TILE_EDITOR::isLayerVisible(const uint32_t index) const
    {
        if(index < tileMap.layers.size())
            return tileMap.layers[index]->visible;
        return false;
    }

    COLOR        TILE_EDITOR::getMinTintLayer(const uint32_t index) const
    {
        if(index < tileMap.layers.size())
            return tileMap.layers[index]->tint_min;
        return COLOR();
    }

    COLOR        TILE_EDITOR::getMaxTintLayer(const uint32_t index) const
    {
        if(index < tileMap.layers.size())
            return tileMap.layers[index]->tint_max;
        return COLOR();
    }

    void   TILE_EDITOR::setTintAnimTypeLayer(const uint32_t index,const TYPE_ANIMATION type)
    {
        if(index < tileMap.layers.size())
        {
            auto & layer = tileMap.layers[index];
            layer->typeTint = type;
            layer->fx.setTypePS(layer->typeTint);
            layer->fx.shader.update();
            layer->fx.fxPS->restart();
        }
    }

    void   TILE_EDITOR::setTintAnimTimeLayer(const uint32_t index,const float time)
    {
        if(index < tileMap.layers.size())
        {
            auto & layer = tileMap.layers[index];
            layer->fTimeTint = time;
            layer->fx.setTimePS(layer->fTimeTint);
            layer->fx.shader.update();
            layer->fx.fxPS->restart();
        }
    }

    TYPE_ANIMATION  TILE_EDITOR::getTintAnimTypeLayer(const uint32_t index) const
    {
        if(index < tileMap.layers.size())
            return tileMap.layers[index]->typeTint;
        return TYPE_ANIMATION_PAUSED;
    }
    float  TILE_EDITOR::getTintAnimTimeLayer(const uint32_t index) const
    {
        if(index < tileMap.layers.size())
            return tileMap.layers[index]->fTimeTint;
        return 0;
    }

    const char * TILE_EDITOR::getNamePsShaderLayer(const uint32_t index) const 
    {
        if(index < tileMap.layers.size())
        {
            const auto & layer = tileMap.layers[index];
            if(layer->fx.fxPS->ptrCurrentShader)
                return layer->fx.fxPS->ptrCurrentShader->fileName.c_str();
        }
        return "null";
    }

    void TILE_EDITOR::setMapCountHeight(const uint32_t value)
    {
        if(value > 0)
        {
            const uint32_t count_height_tile_original   = tileMap.count_height_tile;
            const int32_t diff        = static_cast<int32_t>(value - tileMap.count_height_tile);
            tileMap.count_height_tile = value;
            const uint32_t total      = tileMap.count_width_tile * tileMap.count_height_tile;
            for(uint32_t index_layer  = 0; index_layer <  tileMap.layers.size(); ++index_layer)
            {
                auto & layer        = tileMap.layers[index_layer];
                auto brick_original = layer->bricks;
                if(diff > 0)
                {
                    if(layer->bricks.size() !=  total)
                    {
                        layer->bricks.resize(total);
                    }
                }
                for (uint32_t j = 0; j < tileMap.count_height_tile; j++)
                {
                    for (uint32_t i = 0; i < tileMap.count_width_tile; i++)
                    {
                        const uint32_t index = j + (i  * tileMap.count_height_tile);
                        if(j >= count_height_tile_original)
                            layer->bricks[index] = nullptr;
                        else
                        {
                            const uint32_t index_original = j + (i  * count_height_tile_original);
                            auto & brick = brick_original[index_original];
                            layer->bricks[index] = brick;
                        }
                    }
                }
            }
        }
     }


     void TILE_EDITOR::moveWholeLayerTo(const int32_t x,const int32_t y)
     {
        if(x != 0)
        {
            for(uint32_t index_layer  = 0; index_layer <  tileMap.layers.size(); ++index_layer)
            {
                auto & layer        = tileMap.layers[index_layer];
                auto brick_original = layer->bricks;
                for (uint32_t j = 0; j < tileMap.count_height_tile; j++)
                {
                    for (uint32_t i = 0; i < tileMap.count_width_tile; i++)
                    {
                        const uint32_t index_original = j + (i  * tileMap.count_height_tile);
                        const int32_t index_next = i + (-x);
                        if(index_next >= static_cast<int32_t>(tileMap.count_width_tile) || index_next < 0)
                        {
                            layer->bricks[index_original] = nullptr;
                        }
                        else
                        {
                            const uint32_t i_next = j + (index_next  * tileMap.count_height_tile);
                            auto & brick = brick_original[i_next];
                            layer->bricks[index_original] = brick;
                        }
                    }
                }
            }
        }
        if(y != 0)
        {
            for(uint32_t index_layer  = 0; index_layer <  tileMap.layers.size(); ++index_layer)
            {
                auto & layer        = tileMap.layers[index_layer];
                auto brick_original = layer->bricks;
                for (uint32_t j = 0; j < tileMap.count_height_tile; j++)
                {
                    for (uint32_t i = 0; i < tileMap.count_width_tile; i++)
                    {
                        const uint32_t index_original = j + (i  * tileMap.count_height_tile);
                        const int32_t index_next = j + (-y);
                        if(index_next >= static_cast<int32_t>(tileMap.count_height_tile) || index_next < 0)
                        {
                            layer->bricks[index_original] = nullptr;
                        }
                        else
                        {
                            const uint32_t i_next = index_next + (i  * tileMap.count_height_tile);
                            auto & brick = brick_original[i_next];
                            layer->bricks[index_original] = brick;
                        }
                    }
                }
            }
        }
     }


    VEC2         TILE_EDITOR::getOffsetLayer(const uint32_t index) const
    {
        if(index < tileMap.layers.size())
        {
            const auto & layer = tileMap.layers[index];
            return VEC2(layer->offset.x,layer->offset.y);
        }
        return VEC2();
    }

    void         TILE_EDITOR::setLayerVisible(const uint32_t index, bool value)
    {
        if(index < tileMap.layers.size())
        {
            auto & layer = tileMap.layers[index];
            layer->visible = value;
        }
    }

    void         TILE_EDITOR::setMinTintLayer(const uint32_t index,   const COLOR & value)
    {
        if(index < tileMap.layers.size())
        {
            auto & layer = tileMap.layers[index];
            layer->tint_min = value;
            const float tint_min[4] = {layer->tint_min.r,layer->tint_min.g,layer->tint_min.b, 1.0f };
            const float tint_max[4] = {layer->tint_max.r,layer->tint_max.g,layer->tint_max.b, 1.0f };
            layer->fx.setMinVarPShader("color", tint_min);
            layer->fx.setMaxVarPShader("color", tint_max);
            layer->fx.setVarPShader("color",    tint_min);
            layer->fx.shader.update();
            layer->fx.fxPS->restart();
        }
    }

    void         TILE_EDITOR::setMaxTintLayer(const uint32_t index,   const COLOR & value)
    {
        if(index < tileMap.layers.size())
        {
            auto & layer = tileMap.layers[index];
            layer->tint_max = value;
            const float tint_min[4] = {layer->tint_min.r,layer->tint_min.g,layer->tint_min.b, 1.0f };
            const float tint_max[4] = {layer->tint_max.r,layer->tint_max.g,layer->tint_max.b, 1.0f };
            layer->fx.setMinVarPShader("color", tint_min);
            layer->fx.setMaxVarPShader("color", tint_max);
            layer->fx.setVarPShader("color",    tint_min);
            layer->fx.shader.update();
            layer->fx.fxPS->restart();
        }
    }

    void         TILE_EDITOR::setOffsetLayer(const uint32_t index, const VEC2 & value)
    {
        if(index < tileMap.layers.size())
        {
            auto & layer = tileMap.layers[index];
            layer->offset.x = value.x;
            layer->offset.y = value.y;
        }
    }

    bool TILE_EDITOR::existLayer(const uint32_t index) const
    {
        if(index < tileMap.layers.size())
            return true;
        return false;
    }

    bool         TILE_EDITOR::addLayer()
    {
        if(tileMap.layers.size() == 0 && tileMap.bricks.size() > 0)
        {
            const auto brick = tileMap.bricks.begin();
            this->tileMap.size_width_tile  = brick->second->width; // initial suggested value to map
            this->tileMap.size_height_tile = brick->second->height;// initial suggested value to map
        }
        auto layer = std::make_shared<mbm::LAYER>();
        const uint32_t total    = tileMap.count_width_tile * tileMap.count_height_tile;
        if(layer->bricks.size() !=  total)
        {
            layer->bricks.resize(total);
        }
        if(layer->createFx() == false)
            return false;
        layer->offset.z =    (tileMap.layers.size() + 1) * -0.1f;
        tileMap.layers.emplace_back(layer);
        return true;
    }

    void         TILE_EDITOR::eraseLayer(const uint32_t index)
    {
        if(index < tileMap.layers.size())
        {
            tileMap.layers.erase(tileMap.layers.begin() + index);
            for(uint32_t i=0; i < tileMap.layers.size(); ++i)
            {
                auto & layer    = tileMap.layers[i];
                layer->offset.z = (i + 1) * -0.1f;
            }
        }
    }

    PROPERTY &          TILE_EDITOR::getLayerProperties(const uint32_t index)
    {
        if(index < tileMap.layers.size())
        {
            auto & layer = tileMap.layers[index];
            return layer->properties;
        }
        throw("Layer does not exist");
    }

    void TILE_EDITOR::setMapType(const char * type)
    {
        if(type)
        {
            if(strcmp(type,"ORTHOGONAL") == 0 )
            {
                tileMap.typeMap = util::BTILE_TYPE_ORIENTATION_ORTHOGONAL;
            }
            else if(strcmp(type,"ISOMETRIC") == 0 )
            {
                tileMap.typeMap = util::BTILE_TYPE_ORIENTATION_ISOMETRIC;
            }
            else
            {
                INFO_LOG("Not implemented map type [%s]",type);
            }
        }
    }

    const char* TILE_EDITOR::getMapType() const
    {
        if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ORTHOGONAL)
            return "ORTHOGONAL";
        if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
            return "ISOMETRIC";
        INFO_LOG("Not implemented map type [%d] returning ORTHOGONAL type",tileMap.typeMap);
        return "ORTHOGONAL";
    }

    void TILE_EDITOR::setDirectionMapRender(const bool left_to_right,const bool top_to_down)
    {
        tileMap.render_left_to_right = left_to_right;
        tileMap.render_top_to_down   = top_to_down;
    }

    void TILE_EDITOR::getDirectionMapRender(bool & left_to_right, bool & top_to_down)const
    {
        left_to_right = tileMap.render_left_to_right;
        top_to_down   = tileMap.render_top_to_down;
    }


    //this method works with absolute index when using filter (stored at vector). Use id (map) when no filter . (different place to search)
    std::shared_ptr<BRICK>     TILE_EDITOR::getBrick(const uint16_t index,const char * filterTileSet) const
    { 
        if(filterTileSet == nullptr)
        {
            auto it = tileMap.bricks.find(index);
            if(it != tileMap.bricks.cend())
                return it->second;
        }
        else
        {
            std::shared_ptr<TILED_SET> tileset;
            for(auto & ts : this->tileMap.tile_sets)
            {
                if(ts->name.compare(filterTileSet) == 0)
                {
                    tileset = ts;
                    break;
                }
            }
            if(tileset && index < tileset->bricks.size())
            {
                return tileset->bricks[index];
            }
        }
        return std::shared_ptr<BRICK>();
    }

    bool TILE_EDITOR::newTileSet(const char * tile_set_name,
                                    std::vector<std::string> & textures,
                                    const uint32_t width,
                                    const uint32_t height,
                                    const uint32_t space_x,
                                    const uint32_t space_y,
                                    const uint32_t margin_x,
                                    const uint32_t margin_y,
                                    const VEC2 * min_bound,
                                    const VEC2 * max_bound)
    {
        mbm::TEXTURE::enableFilter(false);
        auto texManager         = mbm::TEXTURE_MANAGER::getInstance();
        
        std::vector<TEXTURE*>   allTextures;
        if(textures.size() == 0)
        {
            mbm::TEXTURE::enableFilter(true);
            ERROR_LOG("%s","There is no Texture\nCould not create tileSet!");
            return false;
        }
        for (auto texture_file_name : textures)
        {
            auto texture        = texManager->load(texture_file_name.c_str(),true);
            if(texture == nullptr)
            {
                mbm::TEXTURE::enableFilter(true);
                ERROR_LOG("Failed to load texture [%s]\nCould not create tileSet!",texture_file_name.c_str());
                return false;
            }
            allTextures.push_back(texture);
        }

        mbm::TEXTURE::enableFilter(true);
        if(width == 0)
        {
            ERROR_LOG("%s","Failed to create tileSet. Width can not be ZERO!");
            return false;
        }
        if(height == 0)
        {
            ERROR_LOG("%s","Failed to create tileSet. Height can not be ZERO!");
            return false;
        }

        auto tile_set           = std::make_shared<mbm::TILED_SET>(tile_set_name,width,height);
        this->tileMap.tile_sets.push_back(tile_set);
        const bool has_bounds = min_bound != nullptr && max_bound != nullptr;

        for (auto texture : allTextures)
        {
            const uint32_t lines    = std::clamp(static_cast<uint32_t>(std::floor(texture->getWidth() / width)),  static_cast<uint32_t>(1), static_cast<uint32_t>(std::numeric_limits<uint32_t>::max()));
            const uint32_t column   = std::clamp(static_cast<uint32_t>(std::floor(texture->getHeight() / height)),static_cast<uint32_t>(1), static_cast<uint32_t>(std::numeric_limits<uint32_t>::max()));
            for (uint32_t j = 0; j < column; j++)
            {
                for (uint32_t i = 0; i < lines; i++)
                {
                    float pixel_position_u    = std::clamp(static_cast<float>((i * width)  + margin_x)     /  static_cast<float>(texture->getWidth()), 0.0f, 1.0f);
                    float pixel_position_v    = std::clamp(static_cast<float>((j * height) + margin_y)     /  static_cast<float>(texture->getHeight()), 0.0f, 1.0f);
                    float pixel_next_pos_u    = std::clamp(static_cast<float>(((i +1) * width ) + margin_x - (space_x)) / static_cast<float>(texture->getWidth()), 0.0f, 1.0f);
                    float pixel_next_pos_v    = std::clamp(static_cast<float>(((j +1) * height) + margin_y - (space_y)) / static_cast<float>(texture->getHeight()), 0.0f, 1.0f);

                    if(has_bounds)
                    {
                        const float current_min_bound_x = texture->getWidth()  * pixel_position_u;
                        const float current_min_bound_y = texture->getHeight() * pixel_position_v;

                        const float current_max_bound_x = texture->getWidth()  * pixel_next_pos_u;
                        const float current_max_bound_y = texture->getHeight() * pixel_next_pos_v;

                        if(current_max_bound_x < min_bound->x )
                            continue;
                        if(current_max_bound_y < min_bound->y )
                            continue;
                        if(current_min_bound_x > max_bound->x )
                            continue;
                        if(current_min_bound_y > max_bound->y )
                            continue;
                    }
                    
                    auto brick                = std::make_shared<mbm::BRICK>(ROTATION_NONE,false,std::numeric_limits<uint16_t>::max());
                    VEC2 uv[4];
                    uv[0].x                   = pixel_position_u;
                    uv[0].y                   = pixel_next_pos_v;
                    uv[1].x                   = pixel_position_u;
                    uv[1].y                   = pixel_position_v;
                    uv[2].x                   = pixel_next_pos_u;
                    uv[2].y                   = pixel_next_pos_v;
                    uv[3].x                   = pixel_next_pos_u;
                    uv[3].y                   = pixel_position_v;
                    brick->texture            = texture;
                    brick->width              = width;
                    brick->height             = height;
                    brick->build(nullptr,uv);
                    tileMap.bricks[brick->id] = brick;
                    tile_set->bricks.push_back(brick);
                }
            }
        }
        if(has_bounds == false)
            textureTileSetPreview = nullptr;
        return true;
    }

    void  TILE_EDITOR::setTileSetPreview(const char * fileNameTexture,
                                    const int32_t width,
                                    const int32_t height,
                                    const int32_t space_x,
                                    const int32_t space_y,
                                    const int32_t margin_x,
                                    const int32_t margin_y)
    {
        if(fileNameTexture)
        {
            auto tex = mbm::TEXTURE_MANAGER::getInstance();
            mbm::TEXTURE::enableFilter(false);
            textureTileSetPreview = tex->load(fileNameTexture,true);
            auto texture = textureTileSetPreview;
            mbm::TEXTURE::enableFilter(false);
            
            if(textureTileSetPreview && loadBufferGl(tileSetPreview))
            {
                if(line_tileSetPreview == nullptr)
                    line_tileSetPreview = new mbm::LINE_MESH(this->device->scene,false,false);

                line_tileSetPreview->release();
                std::vector<VEC3> arrayLines;
                const float wTex = static_cast<const float>(texture->getWidth());
                const float hTex = static_cast<const float>(texture->getHeight());
                const float cx = wTex * 0.5f;
                const float cy = hTex * 0.5f;

                arrayLines.push_back(VEC3(-cx, -cy, 0));
                arrayLines.push_back(VEC3(-cx,  cy, 0));
                arrayLines.push_back(VEC3( cx,  cy, 0));
                arrayLines.push_back(VEC3( cx, -cy, 0));
                arrayLines.push_back(VEC3(-cx,- cy, 0));

                line_tileSetPreview->add(std::move(arrayLines));
                line_tileSetPreview->color = COLOR(1, 0, 1, 1.0f);
                const float d[4] = {1,0,1,1};
                auto * anim = line_tileSetPreview->getAnimation(0);
                if(anim)
                {
                    anim->fx.setVarPShader("color", d);
                    anim->fx.setMaxVarPShader("color", d);
                    anim->fx.setMinVarPShader("color", d);
                }
                tileSetPreview.idTexture0[0] = textureTileSetPreview->idTexture;
                tileSetPreview.idTexture1    = 0;
                tileSetPreview.useAlpha[0]   = textureTileSetPreview->useAlphaChannel ? 1 : 0;

                const uint32_t lines    = std::clamp(static_cast<uint32_t>(static_cast<uint32_t>(std::floor(texture->getWidth() / width  ))), static_cast<uint32_t>(1), static_cast<uint32_t>(std::numeric_limits<uint32_t>::max()));
                const uint32_t column   = std::clamp(static_cast<uint32_t>(static_cast<uint32_t>(std::floor(texture->getHeight() / height))), static_cast<uint32_t>(1), static_cast<uint32_t>(std::numeric_limits<uint32_t>::max()));
                for (uint32_t j = 0; j < column; j++)
                {
                    for (uint32_t i = 0; i < lines; i++)
                    {
                        std::vector<VEC3> array_lines;
                        float pixel_position_u    = std::clamp(static_cast<float>((i * width)  + margin_x)     /  static_cast<float>(texture->getWidth()), 0.0f, 1.0f);
                        float pixel_position_v    = std::clamp(static_cast<float>((j * height) + margin_y)     /  static_cast<float>(texture->getHeight()), 0.0f, 1.0f);
                        float pixel_next_pos_u    = std::clamp(static_cast<float>(((i +1) * width ) + margin_x - (space_x)) / static_cast<float>(texture->getWidth()), 0.0f, 1.0f);
                        float pixel_next_pos_v    = std::clamp(static_cast<float>(((j +1) * height) + margin_y - (space_y)) / static_cast<float>(texture->getHeight()), 0.0f, 1.0f);
                        
                        array_lines.push_back(VEC3((pixel_position_u * wTex) - cx,hTex - (hTex * pixel_position_v) - cy,0));
                        array_lines.push_back(VEC3((pixel_position_u * wTex) - cx,hTex - (hTex * pixel_next_pos_v) - cy,0));
                        array_lines.push_back(VEC3((pixel_next_pos_u * wTex) - cx,hTex - (hTex * pixel_next_pos_v) - cy,0));
                        array_lines.push_back(VEC3((pixel_next_pos_u * wTex) - cx,hTex - (hTex * pixel_position_v) - cy,0));
                        array_lines.push_back(VEC3((pixel_position_u * wTex) - cx,hTex - (hTex * pixel_position_v) - cy,0));

                        line_tileSetPreview->add(std::move(array_lines));
                    }
                }
            }
        }
    }

    bool TILE_EDITOR::isOnFrustum()
    {
        if(render_what != mbm::RENDER_LAYER)
            iLastIndexBrickOver        = std::numeric_limits<uint16_t>::max()-1;
        switch (render_what)
        {
            case mbm::RENDER_MAP:      scale.x = scale_map.x;   scale.y = scale_map.y;   break;
            case mbm::RENDER_TILE_SET: scale.x = scale_tile.x;  scale.y = scale_tile.y;  break;
            case mbm::RENDER_LAYER:    scale.x = scale_layer.x; scale.y = scale_layer.y; break;
            case mbm::RENDER_BRICK:    scale.x = scale_brick.x; scale.y = scale_brick.y; break;
        }
        return true;
    }

    bool TILE_EDITOR::render()
    {
        ANIMATION *anim = this->getAnimation(0);
        if(anim == nullptr)
        {
            if(createAnim() == false)
            {
                ERROR_AT(__LINE__,__FILE__, "%s", "Failed to create animation");
                return false;
            }
            anim = this->getAnimation();
        }
        if (this->alwaysRenderize)
        {
            if (this->isOnFrustum() == false)
                return false;
        }

        this->blend.set(anim->blendState);
        anim->updateAnimation(this->device->delta, this, this->onEndAnimation, this->onEndFx);
        anim->fx.setBlendOp();
        anim->fx.shader.update();
        //only 2dw
        MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
        MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
        switch (render_what)
        {
            case RENDER_MAP:
            {
                if(line_tileSetPreview)
                    line_tileSetPreview->enableRender = false;
                return renderMap(&anim->fx.shader);
            }
            break;
            case RENDER_TILE_SET:
            {
                return renderTileSet();
            }
            break;
            case RENDER_LAYER:
            {
                positionSelectedBrick.clear();
                if(line_tileSetPreview)
                    line_tileSetPreview->enableRender = false;
                const VEC2 scale_offset(scale.x,scale.y);

                for (uint32_t i = 0; i < tileMap.layers.size(); i++)
                {
                    const bool enable_highlights = i == index_render_what;
                    const float transparency = i > index_render_what ? 0.7f : 0.0f;
                    if(renderLayer(i, enable_highlights ,scale_offset,transparency) == false)
                        return false;
                }
            }
            break;
            case RENDER_BRICK:
            {
                if(line_tileSetPreview)
                    line_tileSetPreview->enableRender = false;
                return renderBrick();
            }
            break;
        }
        return false;
    }

    bool TILE_EDITOR::onRestoreDevice()
    {
        return false;
    }

    void TILE_EDITOR::onStop()
    {
        
    }

    bool TILE_EDITOR::renderMap(SHADER *shader)
    {
        if(tileMap.background.a > 0 || tileMap.background_texture)
        {
            if(backGroundMap.isLoadedBuffer() == false)
            {
                if(loadBufferGl(backGroundMap) == false)
                    return false;
            }
            auto texManager = mbm::TEXTURE_MANAGER::getInstance();
            char whatColor[20] = "";
            COLOR::getStringHexColorFromColor(tileMap.background,whatColor,sizeof(whatColor));
            mbm::TEXTURE::enableFilter(false);
            auto texture                      = tileMap.background_texture ? tileMap.background_texture : texManager->load(whatColor,true);
            mbm::TEXTURE::enableFilter(true);
            if(texture == nullptr)
            {
                ERROR_LOG("Could not create texture from color %s",whatColor);
                return false;
            }
            this->backGroundMap.idTexture0[0] = texture->idTexture;
            this->backGroundMap.idTexture1    = 0;
            this->backGroundMap.useAlpha[0]   = texture && texture->useAlphaChannel ? 1 : 0;

            float multiply = 1.0f;
            if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
            {
                multiply = 0.5f;
            }

            const float width_tile    = static_cast<float>(tileMap.size_width_tile  * scale.x);
            const float height_tile   = static_cast<float>(tileMap.size_height_tile * scale.y) * multiply;
            const float width_map     = static_cast<float>(width_tile  * tileMap.count_width_tile);
            const float height_map    = static_cast<float>(height_tile * tileMap.count_height_tile);

            VEC3 backGroundPosition(0,0,0);
            VEC3 backGround_scale(width_map,height_map,1.0f);

            if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
            {
                backGround_scale.x   += width_tile  * 0.5f;
                backGround_scale.y   += height_tile;
                backGroundPosition.x += width_tile * 0.25f;
            }

            MatrixTranslationRotationScale(&SHADER::modelView, &backGroundPosition, &this->angle, &backGround_scale);
            MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
            if(shader->render(&this->backGroundMap) == false)
                return false;
        }
        const bool enable_highlights = false;
        const float transparency = 0.0f;
        for (size_t i = 0; i < tileMap.layers.size(); i++)
        {
            index_render_what = i;
            const bool oldStateVisible = tileMap.layers[i]->visible;
            tileMap.layers[i]->visible = true;
            if(renderLayer(i,enable_highlights,scale_map,transparency) == false)
            {
                tileMap.layers[i]->visible = oldStateVisible;
                return false;
            }
            tileMap.layers[i]->visible = oldStateVisible;
        }
        position.z = 0;
        
        return true;
    }
    bool TILE_EDITOR::renderTileSet()
    {
        if(textureTileSetPreview)
        {
            if(line_tileSetPreview)
                line_tileSetPreview->enableRender = true;
            position.x = 0;
            position.y = 0;
            ANIMATION *anim = this->getAnimation(0);
            if(anim == nullptr)
                return false;
            auto shader = &anim->fx.shader;
            const float width  = static_cast<float>(textureTileSetPreview->getWidth());
            const float height = static_cast<float>(textureTileSetPreview->getHeight());
            const VEC3 tex_scale(width * this->scale_tile.x,height * this->scale_tile.y,1);
            line_tileSetPreview->position.z = position.z - 2;
            line_tileSetPreview->scale.x = this->scale_tile.x;
            line_tileSetPreview->scale.y = this->scale_tile.y;
            MatrixTranslationRotationScale(&SHADER::modelView, &position, &this->angle, &tex_scale);
            MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
            if(shader->render(&this->tileSetPreview) == false)
                return false;
            return true;
        }
        else if(index_render_what < tileMap.tile_sets.size())
        {
            if(line_tileSetPreview)
                line_tileSetPreview->enableRender = false;
            auto & tileSet = tileMap.tile_sets[index_render_what];
            const size_t m_max = static_cast<size_t>(std::ceil(std::sqrt(tileSet->bricks.size()))) + 1;
            position.x = position_aux_tileset.x;
            position.y = position_aux_tileset.y;
            
            ANIMATION *anim = this->getAnimation(0);
            VEC2 m_pmax(std::numeric_limits<float>::min() ,std::numeric_limits<float>::min());
            VEC2 m_pmin(std::numeric_limits<float>::max() ,std::numeric_limits<float>::max());
            size_t index = 0;
            const float space_between = 5.0f;

            //find the width
            for (size_t i = 0; i < m_max; i++)
            {
                for (size_t j = 0; j < m_max; j++)
                {
                    if(index < tileSet->bricks.size())
                    {
                        const auto & brick = tileSet->bricks[index];
                        m_pmax.x = std::max(position.x,m_pmax.x);
                        m_pmax.y = std::max(position.y,m_pmax.y);
                        m_pmin.x = std::min(position.x,m_pmin.x);
                        m_pmin.y = std::min(position.y,m_pmin.y);
                        position.x += (brick->width * this->scale.x) + space_between;
                    }
                    else
                    {
                        break;
                    }
                    index++;
                }
                position.x = position_aux_tileset.x;
                if(index < tileSet->bricks.size())
                {
                    auto & brick = tileSet->bricks[index];
                    position.y += (brick->height * this->scale.y) + space_between;
                }
            }

            //draw
            index = 0;
            position_aux_tileset.x = (std::abs(m_pmax.x) + std::abs(m_pmin.x)) * -0.5f;
            position_aux_tileset.y = (std::abs(m_pmax.y) + std::abs(m_pmin.y)) * -0.5f;
            position.x             = position_aux_tileset.x;
            position.y             = position_aux_tileset.y;
            for (size_t i = 0; i < m_max; i++)
            {
                for (size_t j = 0; j < m_max; j++)
                {
                    if(index < tileSet->bricks.size())
                    {
                        auto & brick = tileSet->bricks[index];

                        SHADER::modelView._41 = position.x;
                        SHADER::modelView._42 = position.y;
                        MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView,&this->device->camera.matrixPerspective2d);

                        brick->render(&anim->fx.shader,0);
                        position.x += (brick->width * this->scale.x) + 5;
                    }
                    else
                    {
                        break;
                    }
                    index++;
                }
                position.x = position_aux_tileset.x;
                if(index < tileSet->bricks.size())
                {
                    auto & brick = tileSet->bricks[index];
                    position.y += (brick->height * this->scale.y) + 5;
                }
            }
            position.x = 0;
            position.y = 0;
            return true;
        }
        return false;
    }

    bool TILE_EDITOR::renderLayer(const uint32_t index_layer, const bool enable_highlights,const VEC2 & scale_offset,const float transparency)
    {
        if(index_layer < tileMap.layers.size())
        {
            auto & layer              = tileMap.layers[index_layer];
            if(layer->visible == false)
                return true;
            ANIMATION *anim_normal    = lsAnimation[0];
            ANIMATION *transparent    = lsAnimation[3];
            const uint32_t total      = tileMap.count_width_tile * tileMap.count_height_tile;
            position.x                = layer->offset.x * scale_offset.x;
            position.y                = layer->offset.y * scale_offset.y;
            position.z                = layer->offset.z;
            float  multiply           = 1.0f;
            if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
                multiply              = 0.5f;
            const float width_tile    = static_cast<float>(tileMap.size_width_tile  * scale.x);
            const float height_tile   = static_cast<float>(tileMap.size_height_tile * scale.y) * multiply;
            const float width_map     = static_cast<float>(width_tile  * tileMap.count_width_tile);
            const float height_map    = static_cast<float>(height_tile * tileMap.count_height_tile);
            const float initial_x     = width_map  * -0.5f + (width_tile  * 0.5f) + position.x;
            const float initial_y     = height_map * -0.5f + (height_tile * 0.5f) + position.y;
            if(layer->bricks.size() !=  total)
            {
                layer->bricks.resize(total);
            }
            if(transparency > 0.0f)
            {
                this->indexCurrentAnimation = 3;
                this->blend.set(transparent->blendState);
                layer->fx.setBlendOp();
                transparent->fx.fxPS->updateEffect(device->delta);
                transparent->fx.shader.update();
            }
            else
            {
                this->indexCurrentAnimation = 1;
                this->blend.set(anim_normal->blendState);
                layer->fx.setBlendOp();
                layer->fx.fxPS->updateEffect(device->delta);
                layer->fx.shader.update();
            }

            bool updatedSelected = false;
            uint32_t iTotalTilesToRender = 0;
            

            if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
            {
                if(tileMap.render_left_to_right)
                {
                    if(tileMap.render_top_to_down)
                    {
                        for (uint32_t j = tileMap.count_height_tile; j > 0; j--)
                        {
                            for (uint32_t i = 0; i < tileMap.count_width_tile; i++)
                            {
                                if(renderBrickMap(layer,i,j-1,width_tile,height_tile,initial_x,initial_y,updatedSelected,enable_highlights,transparency) == false)
                                    return false;
                                iTotalTilesToRender++;
                                if(iTotalTilesToRender == iTilesToRender)
                                    return true;
                            }
                        }
                    }
                    else
                    {
                        for (uint32_t j = 0; j < tileMap.count_height_tile; j++)
                        {
                            for (uint32_t i = 0; i < tileMap.count_width_tile; i++)
                            {
                                if(renderBrickMap(layer,i,j,width_tile,height_tile,initial_x,initial_y,updatedSelected,enable_highlights,transparency) == false)
                                    return false;
                                iTotalTilesToRender++;
                                if(iTotalTilesToRender == iTilesToRender)
                                    return true;
                            }
                        }
                    }
                }
                else
                {
                    if(tileMap.render_top_to_down)
                    {
                        for (uint32_t j = tileMap.count_height_tile; j > 0; j--)
                        {
                            for (uint32_t i = tileMap.count_width_tile; i > 0; i--)
                            {
                                if(renderBrickMap(layer,i-1,j-1,width_tile,height_tile,initial_x,initial_y,updatedSelected,enable_highlights,transparency) == false)
                                    return false;
                                iTotalTilesToRender++;
                                if(iTotalTilesToRender == iTilesToRender)
                                    return true;
                            }
                        }
                    }
                    else
                    {
                        for (uint32_t j = 0; j < tileMap.count_height_tile; j++)
                        {
                            for (uint32_t i = tileMap.count_width_tile; i > 0; i--)
                            {
                                if(renderBrickMap(layer,i-1,j,width_tile,height_tile,initial_x,initial_y,updatedSelected,enable_highlights,transparency) == false)
                                    return false;
                                iTotalTilesToRender++;
                                if(iTotalTilesToRender == iTilesToRender)
                                    return true;
                            }
                        }
                    }
                }
            }
            else
            {
                if(tileMap.render_left_to_right)
                {
                    if(tileMap.render_top_to_down)
                    {
                        for (uint32_t j = tileMap.count_height_tile; j > 0; j--)
                        {
                            for (uint32_t i = 0; i < tileMap.count_width_tile; i++)
                            {
                                if(renderBrickMap(layer,i,j-1,width_tile,height_tile,initial_x,initial_y,updatedSelected,enable_highlights,transparency) == false)
                                    return false;
                                iTotalTilesToRender++;
                                if(iTotalTilesToRender == iTilesToRender)
                                    return true;
                            }
                        }
                    }
                    else
                    {
                        for (uint32_t j = 0; j < tileMap.count_height_tile; j++)
                        {
                            for (uint32_t i = 0; i < tileMap.count_width_tile; i++)
                            {
                                if(renderBrickMap(layer,i,j,width_tile,height_tile,initial_x,initial_y,updatedSelected,enable_highlights,transparency) == false)
                                    return false;
                                iTotalTilesToRender++;
                                if(iTotalTilesToRender == iTilesToRender)
                                    return true;
                            }
                        }
                    }
                }
                else
                {
                    if(tileMap.render_top_to_down)
                    {
                        for (uint32_t j = tileMap.count_height_tile; j > 0; j--)
                        {
                            for (uint32_t i = tileMap.count_width_tile; i > 0; i--)
                            {
                                if(renderBrickMap(layer,i-1,j-1,width_tile,height_tile,initial_x,initial_y,updatedSelected,enable_highlights,transparency) == false)
                                    return false;
                                iTotalTilesToRender++;
                                if(iTotalTilesToRender == iTilesToRender)
                                    return true;
                            }
                        }
                    }
                    else
                    {
                        for (uint32_t j = 0; j < tileMap.count_height_tile; j++)
                        {
                            for (uint32_t i = tileMap.count_width_tile; i > 0; i--)
                            {
                                if(renderBrickMap(layer,i-1,j,width_tile,height_tile,initial_x,initial_y,updatedSelected,enable_highlights,transparency) == false)
                                    return false;
                                iTotalTilesToRender++;
                                if(iTotalTilesToRender == iTilesToRender)
                                    return true;
                            }
                        }
                    }
                }
            }
            position.x  = 0;
            position.y  = 0;
            this->indexCurrentAnimation = 0;
            return true;
        }
        return false;
    }

    bool TILE_EDITOR::renderBrickMap(const std::shared_ptr<LAYER> & layer,
                                     const uint32_t i,
                                     const uint32_t j,
                                     const float width_tile,
                                     const float height_tile,
                                     const float initial_x,
                                     const float initial_y, 
                                     bool & updatedSelected, 
                                     const bool enable_highlights,
                                     const bool transparency)
    {
        ANIMATION *anim_normal    = lsAnimation[0];
        ANIMATION *anim_selected  = lsAnimation[1];
        ANIMATION *anim_over      = lsAnimation[2];
        ANIMATION *transparent    = lsAnimation[3];
        if(transparency)
        {
            anim_normal = transparent;
            this->indexCurrentAnimation = 3;
        }
        else
        {
            this->indexCurrentAnimation = 1;
        }
        float displacement = 0.0f;
        if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
        {
            if(j % 2 == 0)
                displacement = width_tile * 0.5f;
        }
        const float x        = (i * width_tile  ) + initial_x + displacement;
        const float y        = (j * height_tile ) + initial_y;
        const VEC3 brick_position(x,y,position.z);
        const uint32_t index = j + (i  * tileMap.count_height_tile);
        auto & brick         = layer->bricks[index];
        const uint32_t idTexStage2 = layer->fx.textureOverrideStage2 ? layer->fx.textureOverrideStage2->idTexture : 0;
        
        if(brick == nullptr)
        {
            if(enable_highlights)
            {
                const VEC3 empty_scale(width_tile,height_tile,1.0f);
                MatrixTranslationRotationScale(&SHADER::modelView, &brick_position, &this->angle, &empty_scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
                if(renderEmptyBrick(&anim_normal->fx.shader,iLastIndexBrickOver == index,selectedBrick[index]) == false)
                    return false;
            }
        }
        else
        {
            MatrixTranslationRotationScale(&SHADER::modelView, &brick_position, &this->angle, &scale);
            MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
            const bool bSelected  = enable_highlights && selectedBrick[index];
            const bool bOverBrick = enable_highlights && iLastIndexBrickOver == index;
            if(bOverBrick)//only one
            {
                this->blend.set(anim_over->blendState);
                anim_over->updateAnimation(this->device->delta, this, this->onEndAnimation, this->onEndFx);
                anim_over->fx.setBlendOp();
                anim_over->fx.shader.update();
                if(brick->render(&anim_over->fx.shader,idTexStage2) == false)
                    return false;
            }
            else if(bSelected)
            {
                this->blend.set(anim_selected->blendState);
                if(updatedSelected == false)
                {
                    updatedSelected = true;
                    anim_selected->updateAnimation(this->device->delta, this, this->onEndAnimation, this->onEndFx);
                    anim_selected->fx.setBlendOp();
                    anim_selected->fx.shader.update();
                }
                if(brick->render(&anim_selected->fx.shader,idTexStage2) == false)
                    return false;
            }
            else if(transparency == true)
            {
                if(brick->render(&transparent->fx.shader,idTexStage2) == false)
                    return false;
            }
            else
            {
                if(brick->render(&layer->fx.shader,idTexStage2) == false)
                    return false;
            }
            if(render_what == RENDER_LAYER)
            {
                positionSelectedBrick[index] = VEC2(brick_position.x,brick_position.y);
            }
        }
        return true;
    }

    bool TILE_EDITOR::isBrickSelected(const uint16_t index) const 
    {
        const auto it = selectedBrick.find(index);
        if(it != selectedBrick.cend())
            return it->second;
        return false;
    }

    uint16_t TILE_EDITOR::getFirstSelectedBrick() const
    {
        if(index_render_what < tileMap.layers.size())
        {
            const auto & layer      = tileMap.layers[index_render_what];
            for (const auto & brick : selectedBrick)
            {
                if(brick.second == true && brick.first < layer->bricks.size())
                {
                    auto & the_brick = layer->bricks[brick.first];
                    if(the_brick)
                        return the_brick->id;
                }
            }
        }
        return 0;
    }

    uint16_t TILE_EDITOR::getIndexTileIdOver(const float x, const float y) const
    {
        if((render_what == RENDER_LAYER || render_what == RENDER_MAP) && index_render_what < tileMap.layers.size())
        {
            const int total         = tileMap.count_width_tile * tileMap.count_height_tile;
            const auto & layer      = tileMap.layers[index_render_what];
            VEC2 pos;
            device->transformeScreen2dToWorld2d_scaled(x - (layer->offset.x * scale.x),y + (layer->offset.y * scale.y),pos);
            float multiply          = 1.0f;
            if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
                multiply            = 0.5f;
            const float width_tile  = static_cast<float>(tileMap.size_width_tile  * scale.x);
            const float height_tile = static_cast<float>(tileMap.size_height_tile * scale.y) * multiply;
            const float width_map   = static_cast<float>(width_tile  * tileMap.count_width_tile);
            const float height_map  = static_cast<float>(height_tile * tileMap.count_height_tile);
            const float initial_x   = width_map  * -0.5f + (width_tile  );
            const float initial_y   = height_map * -0.5f + (height_tile );
            const float xMin        = initial_x - width_tile;
            const float yMin        = initial_y - height_tile;
            const float xMax        = initial_x + width_map - width_tile;
            const float yMax        = initial_y + height_map - height_tile;

            if(pos.x >= xMin && pos.y >= yMin && pos.x <= xMax && pos.y <= yMax)
            {
                pos.y = std::ceil(pos.y / height_tile) * height_tile;
                const int j = static_cast<int>((pos.y - initial_y + (height_tile * 0.5f)) / height_tile);
                if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
                {
                    float displacement = 0.0f;
                    if(j % 2 == 0)
                        displacement = width_tile * 0.5f;
                    pos.x = (std::ceil((pos.x - displacement) / width_tile)  * width_tile);
                }
                else
                    pos.x = std::ceil(pos.x / width_tile)  * width_tile;
                const int i = static_cast<int>((pos.x - initial_x + (width_tile  * 0.5f)) / width_tile);
                const int index = j + (i * tileMap.count_height_tile);
                if(index >= 0 && index < total)
                {
                    return static_cast<uint16_t>(index);
                }
            }
        }
        return std::numeric_limits<uint16_t>::max()-1;
    }

    bool TILE_EDITOR::setBrick2Layer(uint16_t idBrick,uint32_t index_layer)
    {
        if(render_what == RENDER_LAYER && index_render_what < tileMap.layers.size())
        {
            auto & layer         = tileMap.layers[index_render_what];
            if(index_layer < layer->bricks.size())
            {
                auto brick           = tileMap.bricks[idBrick];
                layer->bricks[index_layer] = brick;
                return true;
            }
        }
        return false;
    }

    void TILE_EDITOR::selectAllBricks()
    {
        if(render_what == RENDER_LAYER && index_render_what < tileMap.layers.size())
        {
            const auto & layer         = tileMap.layers[index_render_what];
            for (size_t i = 0; i < layer->bricks.size(); i++)
            {
                const auto & brick = layer->bricks[i];
                if(brick)
                    selectedBrick[static_cast<uint16_t>(i)] = true;
            }
        }
    }

    void TILE_EDITOR::invertSelectedBricks()
    {
        if(render_what == RENDER_LAYER && index_render_what < tileMap.layers.size())
        {
            const auto & layer         = tileMap.layers[index_render_what];
            for (size_t i = 0; i < layer->bricks.size(); i++)
            {
                const auto & brick = layer->bricks[i];
                if(brick)
                    selectedBrick[static_cast<uint16_t>(i)] = !selectedBrick[static_cast<uint16_t>(i)];
            }
        }
    }

    void TILE_EDITOR::unselectAllBricks()
    {
        if(render_what == RENDER_LAYER && index_render_what < tileMap.layers.size())
        {
            const auto & layer         = tileMap.layers[index_render_what];
            for (size_t i = 0; i < layer->bricks.size(); i++)
            {
                const auto & brick = layer->bricks[i];
                if(brick)
                    selectedBrick[static_cast<uint16_t>(i)] = false;
            }
        }
    }

    void TILE_EDITOR::deleteSelectedBricks()
    {
        if(render_what == RENDER_LAYER && index_render_what < tileMap.layers.size())
        {
            auto & layer         = tileMap.layers[index_render_what];
            for (size_t i = 0; i < layer->bricks.size(); i++)
            {
                auto & brick = layer->bricks[i];
                if(brick && selectedBrick[static_cast<uint16_t>(i)])
                {
                    selectedBrick[static_cast<uint16_t>(i)] = false;
                    layer->bricks[i] = std::shared_ptr<BRICK>();
                }
            }
        }
    }

    std::shared_ptr<BRICK> TILE_EDITOR::getBrickByOriginalId(const uint32_t original_id,const ROTATION_BRICK iRotation,const bool flipped) const
    {
        for(auto & brick : tileMap.bricks)
        {
            const auto that_brick  = brick.second;
            if(that_brick->original_id == original_id && that_brick->iRotation == iRotation && that_brick->bFlipped == flipped)
            {
                return that_brick;
            }
        }
        return std::shared_ptr<BRICK>();
    }

    uint16_t TILE_EDITOR::rotateSelectedBrick(const char * orientation)
    {
        uint16_t id_latest_brick_rotated = std::numeric_limits<uint16_t>::max() -1;
        if(render_what == RENDER_LAYER && index_render_what < tileMap.layers.size())
        {
            const bool rotate_right = orientation == nullptr || strcmp(orientation,"right") == 0;
            auto & layer         = tileMap.layers[index_render_what];
            for (size_t i = 0; i < layer->bricks.size(); i++)
            {
                auto & brick = layer->bricks[i];
                if(brick && selectedBrick[static_cast<uint16_t>(i)])
                {
                    ROTATION_BRICK iRotation;
                    if(rotate_right)
                    {
                        switch (brick->iRotation)
                        {
                            case ROTATION_NONE: iRotation = ROTATE_1;      break;
                            case ROTATE_1:      iRotation = ROTATE_2;      break;
                            case ROTATE_2:      iRotation = ROTATE_3;      break;
                            case ROTATE_3:      iRotation = ROTATION_NONE; break;
                        }
                    }
                    else
                    {
                        switch (brick->iRotation)
                        {
                            case ROTATION_NONE: iRotation = ROTATE_3;      break;
                            case ROTATE_1:      iRotation = ROTATION_NONE; break;
                            case ROTATE_2:      iRotation = ROTATE_1;      break;
                            case ROTATE_3:      iRotation = ROTATE_2;      break;
                        }
                    }
                    auto existentBrick = getBrickByOriginalId(brick->original_id,iRotation,brick->bFlipped);
                    if(existentBrick != nullptr)
                    {
                        //INFO_LOG("Found brick id %d",existentBrick->id);
                        layer->bricks[i] = existentBrick;
                        id_latest_brick_rotated = existentBrick->id;
                    }
                    else
                    {
                        auto originalBrick  = getBrickByOriginalId(brick->original_id,ROTATION_NONE,false);
                        if(originalBrick)
                        {
                            auto newBrick                    = std::make_shared<mbm::BRICK>(iRotation,brick->bFlipped,originalBrick->original_id);
                            tileMap.bricks[newBrick->id]     = newBrick;
                            constexpr int size_uv            = sizeof(brick->uv) / sizeof(VEC2);
                            VEC2 uv[size_uv];

                            if(rotate_right)
                            {
                        /*
                        1    -------   3
                             |     |
                             |     |
                        0    -------   2

                        0    -------   1
                             |     |
                             |     |
                        2    -------   3
                            */
                                uv[1]     = brick->uv[0];
                                uv[3]     = brick->uv[1];
                                uv[0]     = brick->uv[2];
                                uv[2]     = brick->uv[3];
                            }
                            else
                            {
                                /*
                        1    -------   3
                             |     |
                             |     |
                        0    -------   2

                        3    -------   2
                             |     |
                             |     |
                        1    -------   0
                            */
                                uv[2]     = brick->uv[0];
                                uv[0]     = brick->uv[1];
                                uv[3]     = brick->uv[2];
                                uv[1]     = brick->uv[3];
                            }
                            newBrick->texture            = brick->texture;
                            newBrick->width              = brick->width;
                            newBrick->height             = brick->height;
                            newBrick->build(brick->vertex,uv);
                            layer->bricks[i]             = newBrick;
                            id_latest_brick_rotated      = newBrick->id;
                            //INFO_LOG("created a new brick rotated id %d",newBrick->id);
                        }
                        else
                        {
                            ERROR_LOG("Something went wrong!, could not find the original brick [%d]",brick->original_id);
                        }
                    }
                }
            }
        }
        return id_latest_brick_rotated;
    }

    void TILE_EDITOR::moveLayerUp(const uint32_t index_layer)
    {
        if(index_layer < tileMap.layers.size() && index_layer > 0)
        {
            std::swap(tileMap.layers[index_layer],tileMap.layers[index_layer-1]);
            for(uint32_t i=0; i < tileMap.layers.size(); ++i)
            {
                auto & layer    = tileMap.layers[i];
                layer->offset.z = (i + 1) * -0.1f;
            }
        }
    }

    void TILE_EDITOR::moveLayerDown(const uint32_t index_layer)
    {
        if((index_layer + 1) < tileMap.layers.size())
        {
            std::swap(tileMap.layers[index_layer],tileMap.layers[index_layer+1]);
            for(uint32_t i=0; i < tileMap.layers.size(); ++i)
            {
                auto & layer    = tileMap.layers[i];
                layer->offset.z = (i + 1) * -0.1f;
            }
        }
    }

    bool TILE_EDITOR::unDo()
    {
        if(index_history > 0 && index_history < history_files.size())
        {
            const uint32_t index = index_history -1;
            release();
            const bool ret = loadBinary(history_files[index].c_str());
            index_history  = index;
            return ret;
        }
        return false;
    }

    bool TILE_EDITOR::redDo()
    {
        if((index_history + 1) < history_files.size())
        {
            const uint32_t index = index_history +1;
            release();
            const bool ret = loadBinary(history_files[index].c_str());
            index_history  = index;
            return ret;
        }
        return false;
    }

    void TILE_EDITOR::clearHistory()
    {
        for (size_t i = 0; i < history_files.size(); i++)
        {
            const std::string file_to_erase = history_files[i];
            remove(file_to_erase.c_str());
        }
        history_files.clear();
        index_history = 0;
    }

    void TILE_EDITOR::addHistoric()
    {
        if(tileMap.bricks.size() > 0 && tileMap.layers.size() > 0)
        {
            std::string file_name = std::tmpnam(nullptr);
            if(this->saveBinary(file_name.c_str()))
            {
                if(index_history == (history_files.size()))
                {
                    history_files.push_back(file_name);
                    index_history = history_files.size() -1;
                }
                else if(index_history < history_files.size())
                {
                    index_history++;
                    if(index_history < history_files.size())
                    {
                        history_files[index_history] = file_name;
                    }
                    else
                    {
                        history_files.push_back(file_name);
                    }
                    for (size_t i = index_history + 1; i < history_files.size(); i++)
                    {
                        const std::string file_to_erase = history_files[i];
                        remove(file_to_erase.c_str());
                    }
                    history_files.resize(index_history+1);
                }
                else
                {
                    ERROR_AT(__LINE__,__FILE__,"Error internal\n index file [%u] > size of history [%u]",index_history,history_files.size());
                }
            }
        }
    }
    
    uint16_t TILE_EDITOR::flipSelectedBrick()
    {
        uint16_t id_latest_brick_flipped = std::numeric_limits<uint16_t>::max() -1;
        if(render_what == RENDER_LAYER && index_render_what < tileMap.layers.size())
        {
            auto & layer       = tileMap.layers[index_render_what];
            for (size_t i = 0; i < layer->bricks.size(); i++)
            {
                auto & brick = layer->bricks[i];
                if(brick && selectedBrick[static_cast<uint16_t>(i)])
                {
                    auto existentBrick = getBrickByOriginalId(brick->original_id,brick->iRotation,!brick->bFlipped);
                    if(existentBrick != nullptr)
                    {
                        //INFO_LOG("Found brick flipped id %d",existentBrick->id);
                        layer->bricks[i] = existentBrick;
                        id_latest_brick_flipped = existentBrick->id;
                    }
                    else
                    {
                        auto originalBrick  = getBrickByOriginalId(brick->original_id,ROTATION_NONE,false);
                        if(originalBrick)
                        {
                            auto newBrick                    = std::make_shared<mbm::BRICK>(brick->iRotation,!brick->bFlipped,originalBrick->original_id);
                            tileMap.bricks[newBrick->id]     = newBrick;
                            constexpr int size_uv            = sizeof(brick->uv) / sizeof(VEC2);
                            VEC2 uv[size_uv];

                            
                        /*
                        1    -------   3
                             |     |
                             |     |
                        0    -------   2

                        3    -------   1
                             |     |
                             |     |
                        2    -------   0
                            */
                            uv[2]     = brick->uv[0];
                            uv[3]     = brick->uv[1];
                            uv[0]     = brick->uv[2];
                            uv[1]     = brick->uv[3];
                            
                            newBrick->texture            = brick->texture;
                            newBrick->width              = brick->width;
                            newBrick->height             = brick->height;
                            newBrick->build(brick->vertex,uv);
                            layer->bricks[i]             = newBrick;
                            id_latest_brick_flipped      = newBrick->id;
                            //INFO_LOG("created a new brick flipped id %d",newBrick->id);
                        }
                        else
                        {
                            ERROR_LOG("Something went wrong!, could not find the original brick [%d]",brick->original_id);
                        }
                    }
                }
            }
        }
        return id_latest_brick_flipped;
    }

    void TILE_EDITOR::setOverBrick(const float x, const float y)
    {
        if((render_what == RENDER_LAYER || render_what == RENDER_MAP) && index_render_what < tileMap.layers.size())
        {
            const uint32_t total  = tileMap.count_width_tile * tileMap.count_height_tile;
            const uint16_t index  = getIndexTileIdOver(x,y);
            if(index < total)
            {
                iLastIndexBrickOver  = index;
            }
            else
            {
                iLastIndexBrickOver        = std::numeric_limits<uint16_t>::max()-1;
            }
        }
    }

    uint32_t TILE_EDITOR::selectBrick(const int index,const bool unique)
    {
        if((render_what == RENDER_LAYER || render_what == RENDER_MAP) && index_render_what < tileMap.layers.size())
        {
            auto & layer         = tileMap.layers[index_render_what];
            if(index < (int)layer->bricks.size())
            {
                auto & brick = layer->bricks[index];
                if(brick)
                {
                    if(unique)
                        selectedBrick.clear();    
                    selectedBrick[index] = true;
                }
                return selectedBrick.count(true);
            }
            else
            {
                ERROR_LOG("Index brick [%d] out of range [%d]",index,layer->bricks.size());
            }
        }
        return 0;
    }

    bool TILE_EDITOR::renderBrick()
    {
        auto it = tileMap.bricks.find(index_render_what);
        if(it != tileMap.bricks.end())
        {
            ANIMATION *anim = this->getAnimation(0);
            return it->second->render(&anim->fx.shader,0);
        }
        return false;
    }

    bool TILE_EDITOR::createAnim()
    {
        this->releaseAnimation();
        for (int i=0; i < 3; ++i)
        {
            auto anim = new mbm::ANIMATION();
            this->lsAnimation.push_back(anim);
            auto pShaderCfg = device->cfg.getShader("color it.ps");
            if(anim->fx.loadNewShader(pShaderCfg, nullptr, TYPE_ANIMATION_GROWING, 0.1f, TYPE_ANIMATION_GROWING, 0) == true)
            {
                if(i == 0) // normal
                {
                    constexpr float enable[4] = {0,0,0,0};
                    anim->fx.setVarPShader("enable",    enable);
                    anim->fx.setMaxVarPShader("enable", enable);
                    anim->fx.setMinVarPShader("enable", enable);

                    const float * color_back     = enable;
                    constexpr float color_red[4] = {0,0,0,0};
                    anim->fx.setVarPShader("color",    color_back);
                    anim->fx.setMaxVarPShader("color", color_red);
                    anim->fx.setMinVarPShader("color", color_back);
                }
                else if(i == 1) // selected
                {
                    constexpr float enable[4] = {1,0,0,0};
                    anim->fx.setTypePS(TYPE_ANIMATION_RECURSIVE_LOOP);
                    anim->fx.setVarPShader("enable",    enable);
                    anim->fx.setMaxVarPShader("enable", enable);
                    anim->fx.setMinVarPShader("enable", enable);

                    constexpr float color_from[4] = {1,1,1,0};
                    constexpr float color_to[4]  = {0,1,0,0};
                    anim->fx.setVarPShader("color",    color_from);
                    anim->fx.setMinVarPShader("color", color_from);
                    anim->fx.setMaxVarPShader("color", color_to);

                }
                else // over
                {
                    constexpr float enable[4] = {1,0,0,0};
                    anim->fx.setVarPShader("enable",    enable);
                    anim->fx.setMaxVarPShader("enable", enable);
                    anim->fx.setMinVarPShader("enable", enable);

                    constexpr float color_back[4] = {0,0,0,0};
                    constexpr float color_green[4]  = {0,1,0,0};
                    anim->fx.setVarPShader("color",    color_green);
                    anim->fx.setMaxVarPShader("color", color_green);
                    anim->fx.setMinVarPShader("color", color_back);
                }
            }
            else
            {
                ERROR_LOG("%s","Failed to compile shader color it.ps");
                return false;
            }
        }
        auto anim = new mbm::ANIMATION();
        this->lsAnimation.push_back(anim);
        auto pShaderCfg = device->cfg.getShader("transparent.ps");
        if(anim->fx.loadNewShader(pShaderCfg, nullptr, TYPE_ANIMATION_GROWING, 0.1f, TYPE_ANIMATION_GROWING, 0) == true)
        {
            constexpr float alpha[4] = {0.7f,0,0,0};
            anim->fx.setVarPShader("alpha",    alpha);
            anim->fx.setMaxVarPShader("alpha", alpha);
            anim->fx.setMinVarPShader("alpha", alpha);
        }
        this->indexCurrentAnimation = 0;
        return true;
    }

    bool TILE_EDITOR::loadBufferGl(BUFFER_GL & whatBuffer)
    {
        if(whatBuffer.isLoadedBuffer() == false)
        {
            VEC3 vertex[4];
            VEC3 normal[4];
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
            for (int i = 0; i < 4; ++i)
            {
                normal[i].x = 0;
                normal[i].y = 0;
                normal[i].z = 1;
            }
            uv[0].x = 0;
            uv[0].y = 1;
            uv[1].x = 0;
            uv[1].y = 0;
            uv[2].x = 1;
            uv[2].y = 1;
            uv[3].x = 1;
            uv[3].y = 0;
            
            unsigned short int index[6]      = {0, 1, 2, 2, 1, 3};
            return whatBuffer.loadBuffer(vertex, normal, uv, 4, index, 1, &indexStart, &indexCount,nullptr);
        }
        return true;
    }

    bool TILE_EDITOR::renderEmptyBrick(SHADER *shader,bool highlight,bool selected)
    {
        if(emptyBrick.isLoadedBuffer() == true)
        {
            if(highlight)
                emptyBrick.idTexture0[0] = id_texture_highlight_brick;
            else if(selected)
                emptyBrick.idTexture0[0] = id_texture_selected_brick;
            else
                emptyBrick.idTexture0[0] = id_texture_normal_brick;
            return shader->render(&emptyBrick);
        }
        else
        {
            const bool ret  = loadBufferGl(emptyBrick);
            auto texManager = mbm::TEXTURE_MANAGER::getInstance();
            mbm::TEXTURE::enableFilter(false);
            auto texture    = texManager->load("#aaffffaa",true);
            mbm::TEXTURE::enableFilter(true);
            id_texture_normal_brick    = texture ? texture->idTexture : 0;
            if (ret)
            {
                this->emptyBrick.idTexture0[0] = id_texture_normal_brick;
                this->emptyBrick.idTexture1    = 0;
                this->emptyBrick.useAlpha[0]   = texture && texture->useAlphaChannel ? 1 : 0;
            }
            texture                    = texManager->load("#aaff0000",true);
            id_texture_highlight_brick = texture ? texture->idTexture : 0;

            texture                    = texManager->load("#aa00ff00",true);
            id_texture_selected_brick  = texture ? texture->idTexture : 0;
            return ret;
        }
    }

    bool TILE_EDITOR::loadBinary(const char * fileName)
    {
        if(fileName)
        {
            tileMap.release();
            mbm::MESH_MBM_DEBUG meshDebug;
            if(meshDebug.loadDebug(fileName))
            {
                if(meshDebug.typeMe != util::TYPE_MESH_TILE_MAP)
                {
                    const char * that_type = "UNKNOWN";
                    switch (meshDebug.typeMe)
                    {
                        case util::TYPE_MESH_3D         : that_type = "3D";       break;
                        case util::TYPE_MESH_USER       : that_type = "USER";     break;
                        case util::TYPE_MESH_SPRITE     : that_type = "SPRITE";   break;
                        case util::TYPE_MESH_FONT       : that_type = "FONT";     break;
                        case util::TYPE_MESH_TEXTURE    : that_type = "TEXTURE";  break;
                        case util::TYPE_MESH_UNKNOWN    : that_type = "UNKNOWN";  break;
                        case util::TYPE_MESH_SHAPE      : that_type = "SHAPE";    break;
                        case util::TYPE_MESH_PARTICLE   : that_type = "PARTICLE"; break;
                        case util::TYPE_MESH_TILE_MAP   : that_type = "TILE_MAP"; break;
                    }
                    ERROR_LOG("Expected type TILE_MAP got[%s]",that_type);
                    return false;
                }
                auto tileInfo     = static_cast<const util::BTILE_INFO*>(meshDebug.extraInfo);
                if(tileInfo == nullptr)
                {
                    ERROR_LOG("Mesh has no BTILE_INFO");
                    tileMap.release();
                    return false;
                }
                auto tile_set         = std::make_shared<mbm::TILED_SET>("tileset-1",tileInfo->map.count_width_tile,tileInfo->map.count_height_tile);
                tile_set->tile_width  = tileInfo->map.size_width_tile;
                tile_set->tile_height = tileInfo->map.size_height_tile;
                this->tileMap.tile_sets.push_back(tile_set);
                auto tex = mbm::TEXTURE_MANAGER::getInstance();
                for(uint32_t i=0; i < meshDebug.buffer.size(); ++ i)
                {
                    auto * buffer  = meshDebug.buffer[i];
                    if(buffer->headerFrame.sizeVertexBuffer != 4)
                    {
                        ERROR_AT(__LINE__,__FILE__,"Expected vertex buffer size [4] got [%d]",buffer->headerFrame.sizeVertexBuffer);
                        tileMap.release();
                        return false;
                    }
                    if(buffer->headerFrame.totalSubset != 1)
                    {
                        ERROR_AT(__LINE__,__FILE__,"Expected one subset got [%d]",buffer->headerFrame.totalSubset);
                        tileMap.release();
                        return false;
                    }

                    auto & infoBrickEditor = tileInfo->infoBrickEditor[i];

                    auto brick     = std::make_shared<mbm::BRICK>(static_cast<ROTATION_BRICK>(infoBrickEditor.rotation),infoBrickEditor.flipped,infoBrickEditor.original_index + 1);
                    VEC3 * vertex  = reinterpret_cast<VEC3 *>(buffer->position);
                    VEC2 * uv      = reinterpret_cast<VEC2 *>(buffer->uv);
                    brick->texture = tex->load(buffer->subset[0]->texture.c_str(),true);
                    if(brick->texture == nullptr)
                    {
                        tileMap.release();
                        return false;
                    }
                    brick->width  = static_cast<uint32_t>(vertex[3].x - vertex[1].x);
                    brick->height = static_cast<uint32_t>(vertex[1].y - vertex[0].y);
                    brick->build(vertex,uv);

                    tileMap.bricks[brick->id] = brick;
                    tile_set->bricks.push_back(brick);
                }

                tileMap.count_width_tile         = tileInfo->map.count_width_tile;
                tileMap.count_height_tile        = tileInfo->map.count_height_tile;
                tileMap.size_width_tile          = tileInfo->map.size_width_tile;
                tileMap.size_height_tile         = tileInfo->map.size_height_tile;
                tileMap.background               = tileInfo->map.background;
                tileMap.typeMap                  = tileInfo->map.typeMap;
                tileMap.render_left_to_right     = tileInfo->map.renderDirection[0] == 1 ? true : false;
                tileMap.render_top_to_down       = tileInfo->map.renderDirection[1] == 1 ? true : false;

                if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
                    tileMap.size_height_tile = tileMap.size_height_tile * 2;

                if(tileInfo->map.background_texture[0])
                {
                    tileMap.background_texture = tex->load(tileInfo->map.background_texture,true);
                }

                const uint32_t tileCount         =   tileInfo->map.count_width_tile * tileInfo->map.count_height_tile;

                for (size_t i = 0; i < tileInfo->map.layerCount; i++)
                {
                    const auto tInfolayer = &tileInfo->layers[i];
                    auto layer            = std::make_shared<mbm::LAYER>();
                    const uint32_t total  = tileMap.count_width_tile * tileMap.count_height_tile;
                    layer->offset.x       = tInfolayer->offset[0];
                    layer->offset.y       = tInfolayer->offset[1];
                    //layer->       = tInfolayer->offset[1];
                    layer->bricks.reserve(total);

                    for (size_t j = 0; j < tileCount; j++)
                    {
                        auto & tIndexBrick = tInfolayer->lsIndexTiles[j];
                        if(tIndexBrick.index < tileCount)
                        {
                            auto brick       = tileMap.bricks[tIndexBrick.index + 1];
                            layer->bricks.push_back(brick);
                        }
                        else
                        {
                            layer->bricks.push_back(std::shared_ptr<mbm::BRICK>());
                        }
                    }
                    const auto anim = meshDebug.getAnim(i);
                    if(anim->effetcShader)
                    {
                        const char * sPsShaderName = "null";
                        const char * sVsShaderName = "null";
                        TYPE_ANIMATION psTypeAnim  = TYPE_ANIMATION_PAUSED;
                        TYPE_ANIMATION vsTypeAnim  = TYPE_ANIMATION_PAUSED;
                        float fTimePs              = 1.0f;
                        float fTimeVs              = 1.0f;
                        SHADER_CFG * psShaderCfg   = nullptr;
                        SHADER_CFG * vsShaderCfg   = nullptr;
                        if(anim->effetcShader->dataPS && anim->effetcShader->dataPS->fileNameShader)
                        {
                            sPsShaderName = anim->effetcShader->dataPS->fileNameShader;
                            psShaderCfg   = mbm::DEVICE::getInstance()->cfg.getShader(anim->effetcShader->dataPS->fileNameShader);
                            psTypeAnim    = (TYPE_ANIMATION)anim->effetcShader->dataPS->typeAnimation;
                            fTimePs       = anim->effetcShader->dataPS->timeAnimation;
                            if(anim->effetcShader->dataPS->fileNameTextureStage2)
                            {
                                layer->fx.textureOverrideStage2 = tex->load(anim->effetcShader->dataPS->fileNameTextureStage2,true);
                            }
                        }
                        if(anim->effetcShader->dataVS && anim->effetcShader->dataVS->fileNameShader)
                        {
                            sVsShaderName = anim->effetcShader->dataVS->fileNameShader;
                            vsShaderCfg   = mbm::DEVICE::getInstance()->cfg.getShader(anim->effetcShader->dataVS->fileNameShader);
                            vsTypeAnim    = (TYPE_ANIMATION)anim->effetcShader->dataVS->typeAnimation;
                            fTimeVs       = anim->effetcShader->dataVS->timeAnimation;
                            if(anim->effetcShader->dataVS->fileNameTextureStage2)
                            {
                                layer->fx.textureOverrideStage2 = tex->load(anim->effetcShader->dataVS->fileNameTextureStage2,true);
                            }
                        }
                        if(layer->fx.loadNewShader(psShaderCfg, vsShaderCfg, psTypeAnim, fTimePs, vsTypeAnim, fTimeVs) == true)
                        {
                            if(psShaderCfg)
                            {
                                if(anim->effetcShader->dataPS->lenVars == static_cast<int>(psShaderCfg->lsVar.size()))
                                {
                                    if(strcmp(anim->effetcShader->dataPS->fileNameShader,"tint.ps") == 0 )
                                    {
                                        layer->typeTint  = psTypeAnim;
                                        layer->fTimeTint = fTimePs;
                                    }
                                    for(int j=0 ; j < anim->effetcShader->dataPS->lenVars; j++)
                                    {
                                        const auto * var  = psShaderCfg->lsVar[j];
                                        const int index   = j * 4;
                                        float fmin[4]     = {0,0,0,0};
                                        float fmax[4]     = {0,0,0,0};
                                        memcpy(fmin, &anim->effetcShader->dataPS->min[index],sizeof(fmin));
                                        memcpy(fmax, &anim->effetcShader->dataPS->max[index],sizeof(fmax));
                                        layer->fx.setMinVarPShader(var->name.c_str(),fmin);
                                        layer->fx.setMaxVarPShader(var->name.c_str(),fmax);
                                        if(strcmp(anim->effetcShader->dataPS->fileNameShader,"tint.ps") == 0 )
                                        {
                                            layer->tint_min.r = fmin[0];
                                            layer->tint_min.g = fmin[1];
                                            layer->tint_min.b = fmin[2];
                                            layer->tint_min.a = fmin[3];

                                            layer->tint_max.r = fmax[0];
                                            layer->tint_max.g = fmax[1];
                                            layer->tint_max.b = fmax[2];
                                            layer->tint_max.a = fmax[3];
                                        }
                                    }
                                }
                                else
                                {
                                    INFO_LOG("Unexpected different size of variable [%d/%d]\nDo not know what to do!\nDid the shader [%s] change?",anim->effetcShader->dataPS->lenVars,psShaderCfg->lsVar.size(),psShaderCfg->fileName.c_str());
                                }
                            }
                            if(vsShaderCfg)
                            {
                                if(anim->effetcShader->dataVS->lenVars == static_cast<int>(vsShaderCfg->lsVar.size()))
                                {
                                    for(int j=0 ; j < anim->effetcShader->dataVS->lenVars; j++)
                                    {
                                        const auto * var  = vsShaderCfg->lsVar[j];
                                        const int index   = j * 4;
                                        float fmin[4]     = {0,0,0,0};
                                        float fmax[4]     = {0,0,0,0};
                                        memcpy(fmin, &anim->effetcShader->dataVS->min[index],sizeof(fmin));
                                        memcpy(fmax, &anim->effetcShader->dataVS->max[index],sizeof(fmax));
                                        layer->fx.setMinVarVShader(var->name.c_str(),fmin);
                                        layer->fx.setMaxVarVShader(var->name.c_str(),fmax);
                                    }
                                }
                                else
                                {
                                    INFO_LOG("Unexpected different size of variable [%d/%d]\nDo not know what to do!\nDid the shader [%s] change?",anim->effetcShader->dataVS->lenVars,vsShaderCfg->lsVar.size(),vsShaderCfg->fileName.c_str());
                                }
                            }
                        }
                        else
                        {
                            INFO_LOG("Failed to load shaders [%s][%s] not found\nloading default [%s][null]",sPsShaderName,sVsShaderName,"tint.ps");
                            if(layer->createFx() == false)
                                return false;
                        }
                    }
                    else
                    {
                        if(layer->createFx() == false)
                            return false;
                    }
                    tileMap.layers.emplace_back(layer);
                }

                auto addToRightPlace = [] (DYNAMIC_VAR* var, TILED_MAP & tileMap, const std::string & name,const std::string & owner) -> void
                {
                    std::shared_ptr<DYNAMIC_VAR> pVar;
                    pVar.reset(var);
                    if(owner.compare("map") == 0)
                    {
                        tileMap.properties[name] = std::move(pVar);
                    }
                    else
                    {
                        std::vector<std::string> splitted;
                        util::split(splitted,owner.c_str(), '-');
                        if(splitted.size() == 2)
                        {
                            if(splitted[0].compare("layer") == 0)
                            {
                                const uint32_t index = std::atoi(splitted[1].c_str());
                                if(index < tileMap.layers.size())
                                {
                                    tileMap.layers[index]->properties[name] = std::move(pVar);
                                }
                            }
                            else if(splitted[0].compare("brick") == 0)
                            {
                                const uint32_t index = std::atoi(splitted[1].c_str());
                                auto brick = tileMap.bricks[index];
                                if(brick)
                                {
                                    brick->properties[name] = std::move(pVar);
                                }
                            }
                        }
                    }
                };

                for(uint32_t i = 0; i < tileInfo->lsProperty.size(); ++i)
                {
                    auto * pproperty = tileInfo->lsProperty[i];
                    switch(pproperty->type)
                    {
                        case util::BTILE_PROPERTY_TYPE_BOOL:
                        {
                            bool value = pproperty->value.compare("1") == 0 ? true : false;
                            auto   var = new DYNAMIC_VAR(DYNAMIC_BOOL,&value);
                            addToRightPlace(var,tileMap,pproperty->name,pproperty->owner);
                            
                        }break;
                        case util::BTILE_PROPERTY_TYPE_INT:
                        case util::BTILE_PROPERTY_TYPE_COLOR:
                        {
                            int value  = std::atoi(pproperty->value.c_str());
                            auto   var = new DYNAMIC_VAR(DYNAMIC_INT,&value);
                            addToRightPlace(var,tileMap,pproperty->name,pproperty->owner);
                        }break;
                        case util::BTILE_PROPERTY_TYPE_FLOAT:
                        {
                            float value = static_cast<float>(std::atof(pproperty->value.c_str()));
                            auto   var  = new DYNAMIC_VAR(DYNAMIC_FLOAT,&value);
                            addToRightPlace(var,tileMap,pproperty->name,pproperty->owner);
                        }break;
                        case util::BTILE_PROPERTY_TYPE_FILE:
                        case util::BTILE_PROPERTY_TYPE_STRING:
                        {
                            auto   var = new DYNAMIC_VAR(DYNAMIC_CSTRING,pproperty->value.c_str());
                            addToRightPlace(var,tileMap,pproperty->name,pproperty->owner);
                        }break;
                        default:{}
                    }
                }

                for(uint32_t i=0; i < tileInfo->lsObj.size(); ++i)
                {
                    std::shared_ptr<OBJ_TILE_MAP> ptrObj = std::make_shared<mbm::OBJ_TILE_MAP>();
                    auto * obj = tileInfo->lsObj[i];
                    switch (obj->type)
                    {
                        case util::BTILE_OBJ_TYPE_RECT:
                        {
                            std::shared_ptr<mbm::CUBE> cube = std::make_shared<mbm::CUBE>();
                            cube->absCenter.x = obj->lsPoints[0]->x;
                            cube->absCenter.y = obj->lsPoints[0]->y;
                            cube->halfDim.x   = obj->lsPoints[1]->x * 0.5f;
                            cube->halfDim.y   = obj->lsPoints[1]->y * 0.5f;
                            ptrObj->cube      = std::move(cube);
                        }
                        break;
                        case util::BTILE_OBJ_TYPE_CIRCLE:
                        {
                            std::shared_ptr<mbm::SPHERE> sphere = std::make_shared<mbm::SPHERE>();
                            sphere->absCenter[0] = obj->lsPoints[0]->x;
                            sphere->absCenter[1] = obj->lsPoints[0]->y;
                            sphere->ray          = obj->lsPoints[1]->x;
                            ptrObj->sphere       = std::move(sphere);
                        }
                        break;
                        case util::BTILE_OBJ_TYPE_TRIANGLE:
                        {
                            std::shared_ptr<mbm::TRIANGLE> triangle = std::make_shared<mbm::TRIANGLE>();
                            triangle->point[0].x = obj->lsPoints[0]->x;
                            triangle->point[0].y = obj->lsPoints[0]->y;
                            triangle->point[1].x = obj->lsPoints[1]->x;
                            triangle->point[1].y = obj->lsPoints[1]->y;
                            triangle->point[2].x = obj->lsPoints[2]->x;
                            triangle->point[2].y = obj->lsPoints[2]->y;
                            ptrObj->triangle     = std::move(triangle);
                        }
                        break;
                        case util::BTILE_OBJ_TYPE_POINT:
                        {
                            std::shared_ptr<VEC2> vec2 = std::make_shared<VEC2>();
                            vec2->x        = obj->lsPoints[0]->x;
                            vec2->y        = obj->lsPoints[0]->y;
                            ptrObj->point  = std::move(vec2);
                        }
                        break;
                        case util::BTILE_OBJ_TYPE_POLYLINE:
                        {
                            ptrObj->size_line = obj->lsPoints.size();
                            auto ptr_vec2 = std::shared_ptr<mbm::VEC2>(new mbm::VEC2[ptrObj->size_line],array_deleter<mbm::VEC2>());
                            auto vec2     = ptr_vec2.get();
                            for(uint32_t j=0; j < obj->lsPoints.size(); j++)
                            {
                                vec2[j].x     = obj->lsPoints[j]->x;
                                vec2[j].y     = obj->lsPoints[j]->y;
                            }
                            ptrObj->line      = std::move(ptr_vec2);
                        }
                        break;
                        default:{}
                    }
                    if(ptrObj)
                    {
                        ptrObj->name = obj->name;
                        const auto p = obj->name.find("physic-");
                        if(p != std::string::npos)
                        {
                            const int ID = std::atoi(obj->name.substr(p+7).c_str());
                            if(ID != 0)
                            {
                                auto & brick = tileMap.bricks[static_cast<uint16_t>(ID)];
                                if(brick)
                                {
                                    switch (obj->type)
                                    {
                                        case util::BTILE_OBJ_TYPE_RECT:
                                        {
                                            std::shared_ptr<mbm::CUBE> cube = std::make_shared<mbm::CUBE>();
                                            cube->absCenter.x = obj->lsPoints[0]->x;
                                            cube->absCenter.y = obj->lsPoints[0]->y;
                                            cube->halfDim.x   = obj->lsPoints[1]->x * 0.5f;
                                            cube->halfDim.y   = obj->lsPoints[1]->y * 0.5f;
                                            brick->brick_physics.lsCube.emplace_back(cube);
                                        }
                                        break;
                                        case util::BTILE_OBJ_TYPE_CIRCLE:
                                        {
                                            std::shared_ptr<mbm::SPHERE> sphere = std::make_shared<mbm::SPHERE>();
                                            sphere->absCenter[0] = obj->lsPoints[0]->x;
                                            sphere->absCenter[1] = obj->lsPoints[0]->y;
                                            sphere->ray          = obj->lsPoints[1]->x;
                                            brick->brick_physics.lsSphere.emplace_back(sphere);
                                        }
                                        break;
                                        case util::BTILE_OBJ_TYPE_TRIANGLE:
                                        {
                                            std::shared_ptr<mbm::TRIANGLE> triangle = std::make_shared<mbm::TRIANGLE>();
                                            triangle->point[0].x = obj->lsPoints[0]->x;
                                            triangle->point[0].y = obj->lsPoints[0]->y;
                                            triangle->point[1].x = obj->lsPoints[1]->x;
                                            triangle->point[1].y = obj->lsPoints[1]->y;
                                            triangle->point[2].x = obj->lsPoints[2]->x;
                                            triangle->point[2].y = obj->lsPoints[2]->y;
                                            brick->brick_physics.lsTriangle.emplace_back(triangle);
                                        }
                                        break;
                                        default:{}
                                    }
                                }
                            }
                        }
                        else
                        {
                            tileMap.objects.emplace_back(ptrObj);
                        }
                    }
                }
                return true;
            }
        }
        return false;
    }

    bool TILE_EDITOR::saveBinary(const char * fileName)
    {
        if(tileMap.bricks.size() == 0)
        {
            ERROR_LOG("At least one brick should exist");
            return false;
        }
        if(tileMap.layers.size() == 0)
        {
            ERROR_LOG("At least one layer should exist");
            return false;
        }
        if(fileName)
        {
            mbm::MESH_MBM_DEBUG meshDebug;
            const unsigned short int indexBuffer []      = {0, 1, 2,   2, 1, 3};
            const int indexBufferCount = sizeof(indexBuffer) / sizeof(unsigned short int);
            util::BTILE_INFO* tileInfo	     = new util::BTILE_INFO();
            meshDebug.extraInfo              = tileInfo;
            tileInfo->infoBrickEditor        = new util::BTILE_BRICK_INFO[tileMap.bricks.size()];
            
            for(uint32_t i=0; i < tileMap.bricks.size(); ++i)
            {
                auto & brick = tileMap.bricks[i+1]; // one based
                if(brick == nullptr)
                {
                    return log_util::fail(__LINE__, __FILE__, "Unexpected null brick at index [%u]",i);
                }
                const unsigned int nFrame = meshDebug.addBuffer(2);
                if(nFrame > 0 )
                {
                    const unsigned int indexFrame = nFrame - 1;
                    const unsigned int nSubset = meshDebug.addSubset(indexFrame);
                    if(nSubset > 0 )
                    {
                        const unsigned int indexSubset = nSubset - 1;
                        if(meshDebug.addVertex(indexFrame,indexSubset,4))
                        {
                            util::BUFFER_MESH_DEBUG *bufferCurrent      = meshDebug.buffer[indexFrame];
                            bufferCurrent->subset[indexSubset]->texture = brick->texture ? brick->texture->getFileNameTexture() : "";
                            for(unsigned int k=0, n = 0, uv = 0; k< 4; ++k, n+=3, uv+=2)
                            {
                                bufferCurrent->position[n]   = brick->vertex[k].x;
                                bufferCurrent->position[n+1] = brick->vertex[k].y;
                                bufferCurrent->position[n+2] = brick->vertex[k].z;

                                bufferCurrent->normal[n]     = 0;
                                bufferCurrent->normal[n+1]   = 0;
                                bufferCurrent->normal[n+2]   = 1;

                                bufferCurrent->uv[uv]   = brick->uv[k].x;
                                bufferCurrent->uv[uv+1] = brick->uv[k].y;

                                auto & infoBrick          = tileInfo->infoBrickEditor[i];
                                infoBrick.index           = brick->id - 1;
                                infoBrick.original_index  = brick->original_id - 1;
                                infoBrick.flipped         = brick->bFlipped ? 1 : 0;
                                infoBrick.rotation        = (uint16_t)brick->iRotation;
                            }
                        }

                        char errorText[255] = "";
                        if(meshDebug.addIndex(indexFrame,indexSubset,indexBuffer,indexBufferCount,errorText) == false)
                        {
                            return log_util::fail(__LINE__, __FILE__, "Failed add index buffer\n[%s]",errorText);
                        }
                    }
                }
            }
            meshDebug.typeMe                 = util::TYPE_MESH_TILE_MAP;
            tileInfo->map.count_width_tile	 = tileMap.count_width_tile;
            tileInfo->map.count_height_tile	 = tileMap.count_height_tile;
            tileInfo->map.layerCount	     = static_cast<const uint32_t>(tileMap.layers.size());
            tileInfo->map.countRawTiles      = static_cast<const uint32_t>(meshDebug.buffer.size());
            tileInfo->map.size_width_tile	 = tileMap.size_width_tile;
            tileInfo->map.size_height_tile	 = tileMap.size_height_tile;
            tileInfo->map.background         = tileMap.background;
            tileInfo->map.typeMap			 = tileMap.typeMap;
            tileInfo->map.renderDirection[0] = tileMap.render_left_to_right ? 1 : 0;
            tileInfo->map.renderDirection[1] = tileMap.render_top_to_down ? 1 : 0;

            if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
                tileInfo->map.size_height_tile = static_cast<uint32_t>(tileMap.size_height_tile * 0.5f);
            
            if(tileMap.background_texture)
            {
                const char* base_file_name = util::getBaseName(tileMap.background_texture->getFileNameTexture());
                strncpy(tileInfo->map.background_texture,base_file_name,sizeof(tileInfo->map.background_texture)-1);
            }

            const uint32_t tileCount         =   tileInfo->map.count_width_tile * tileInfo->map.count_height_tile;

            scale.x = 1;
            scale.y = 1;
            float multiply = 1.0f;
            if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
                multiply = 0.5f;

            const float width_tile    = static_cast<float>(tileMap.size_width_tile  * scale.x);
            const float height_tile   = static_cast<float>(tileMap.size_height_tile * scale.y) * multiply;
            const float width_map     = static_cast<float>(width_tile  * tileMap.count_width_tile);
            const float height_map    = static_cast<float>(height_tile * tileMap.count_height_tile);
            const float initial_x     = width_map  * -0.5f + (width_tile  * 0.5f) + position.x;
            const float initial_y     = height_map * -0.5f + (height_tile * 0.5f) + position.y;

            tileInfo->layers				= new util::BTILE_LAYER[tileInfo->map.layerCount];
            for(size_t k=0; k < tileMap.layers.size(); k++)
            {
                util::BTILE_INDEX_TILE * lsIndexTiles = new util::BTILE_INDEX_TILE[tileCount];
                tileInfo->layers[k].lsIndexTiles       = lsIndexTiles;
                tileInfo->layers[k].offset[0]          = tileMap.layers[k]->offset.x;
                tileInfo->layers[k].offset[1]          = tileMap.layers[k]->offset.y;
                tileInfo->layers[k].offset[2]          = tileMap.layers[k]->offset.z;
                auto & layer                           = tileMap.layers[k];

                for (uint32_t i = 0; i < tileMap.count_width_tile; i++)
                {
                    for (uint32_t j = 0; j < tileMap.count_height_tile; j++)
                    {
                        
                        const uint32_t index = j + (i  * tileMap.count_height_tile);
                        auto & brick         = layer->bricks[index];
                        auto & btile_brick   = lsIndexTiles[index];
                    
                        if(brick == nullptr)
                        {
                            btile_brick.index = std::numeric_limits<uint16_t>::max()-1;
                        }
                        else
                        {
                            btile_brick.index          = brick->id - 1;
                        }
                        if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ISOMETRIC)
                        {
                            float displacement = 0.0f;
                            if(j % 2 == 0)
                                displacement = width_tile * 0.5f;
                            btile_brick.x              = (i * width_tile  ) + initial_x + displacement;
                            btile_brick.y              = (j * height_tile ) + initial_y;
                        }
                        else if(tileMap.typeMap == util::BTILE_TYPE_ORIENTATION_ORTHOGONAL)
                        {
                            btile_brick.x              = (i * width_tile  ) + initial_x;
                            btile_brick.y              = (j * height_tile ) + initial_y;
                        }
                    }
                }
            }

            // each layer should have "tint.ps" shader color on layer (might be overwritten by shader editor)
            for(size_t k=0; k < tileMap.layers.size(); k++)
            {
                const auto & layer                          = tileMap.layers[k];
                auto * infoAnim	                            = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
                infoAnim->headerAnim						= new util::HEADER_ANIMATION();
                infoAnim->effetcShader						= new util::INFO_FX();
                meshDebug.infoAnimation.lsHeaderAnim.push_back(infoAnim);

                snprintf(infoAnim->headerAnim->nameAnimation,sizeof(infoAnim->headerAnim->nameAnimation), "layer-%zu",k+1);
                if(layer->fx.fxPS->ptrCurrentShader != nullptr)
                {
                    const unsigned int totalVar            = layer->fx.fxPS->ptrCurrentShader->getTotalVar();
                    const unsigned int sizeArrayVarInBytes = totalVar * 4;
                    const unsigned int sizeFileName        = layer->fx.fxPS->ptrCurrentShader->fileName.size();
                    infoAnim->effetcShader->dataPS         = new util::INFO_SHADER_DATA(sizeArrayVarInBytes,(short)(sizeFileName ? sizeFileName + 1 : 0),0);
                    if(sizeFileName)
                        strcpy(infoAnim->effetcShader->dataPS->fileNameShader,layer->fx.fxPS->ptrCurrentShader->fileName.c_str());

                    if(layer->fx.textureOverrideStage2)
                    {
                        const char * textureOverrideStage2 = layer->fx.textureOverrideStage2->getFileNameTexture();
                        const int len                      = strlen(textureOverrideStage2);
                        infoAnim->effetcShader->dataPS->fileNameTextureStage2 = new char[len + 1];
                        strncpy(infoAnim->effetcShader->dataPS->fileNameTextureStage2,textureOverrideStage2,len+1);
                        infoAnim->effetcShader->dataPS->fileNameTextureStage2[len] = 0;
                    }
                    infoAnim->effetcShader->dataPS->typeAnimation = layer->fx.fxPS->typeAnim;
                    infoAnim->effetcShader->dataPS->timeAnimation = layer->fx.fxPS->timeAnimation;

                    for(unsigned int j=0; j < totalVar; ++j)
                    {
                        const int index       = j * 4;
                        VAR_SHADER* var       = layer->fx.fxPS->ptrCurrentShader->getVar(j);
                        memcpy(&infoAnim->effetcShader->dataPS->min[index],var->min,sizeof(var->min));
                        memcpy(&infoAnim->effetcShader->dataPS->max[index],var->max,sizeof(var->max));
                        infoAnim->effetcShader->dataPS->typeVars[j]      = var->typeVar;
                    }
                }

                if(layer->fx.fxVS->ptrCurrentShader != nullptr)
                {
                    const unsigned int totalVar            = layer->fx.fxVS->ptrCurrentShader->getTotalVar();
                    const unsigned int sizeArrayVarInBytes = totalVar * 4;
                    const unsigned int sizeFileName        = layer->fx.fxVS->ptrCurrentShader->fileName.size();
                    infoAnim->effetcShader->dataVS         = new util::INFO_SHADER_DATA(sizeArrayVarInBytes,(short)(sizeFileName ? sizeFileName + 1 : 0),0);
                    if(sizeFileName)
                        strcpy(infoAnim->effetcShader->dataVS->fileNameShader,layer->fx.fxVS->ptrCurrentShader->fileName.c_str());
                    infoAnim->effetcShader->dataVS->typeAnimation = layer->fx.fxVS->typeAnim;
                    infoAnim->effetcShader->dataVS->timeAnimation = layer->fx.fxVS->timeAnimation;

                    for(unsigned int j=0; j < totalVar; ++j)
                    {
                        const int index       = j * 4;
                        VAR_SHADER* var       = layer->fx.fxVS->ptrCurrentShader->getVar(j);
                        memcpy(&infoAnim->effetcShader->dataVS->min[index],var->min,sizeof(var->min));
                        memcpy(&infoAnim->effetcShader->dataVS->max[index],var->max,sizeof(var->max));
                        infoAnim->effetcShader->dataVS->typeVars[j]    = var->typeVar;
                    }
                }
            }

            auto fGetPropertyType = [] (const DYNAMIC_TYPE & dType) -> util::BTILE_PROPERTY_TYPE
            {
                switch(dType)
                {
                    case DYNAMIC_BOOL     : return util::BTILE_PROPERTY_TYPE_BOOL;
                    case DYNAMIC_CHAR     : return util::BTILE_PROPERTY_TYPE_STRING;
                    case DYNAMIC_INT      : return util::BTILE_PROPERTY_TYPE_INT;
                    case DYNAMIC_FLOAT    : return util::BTILE_PROPERTY_TYPE_FLOAT;
                    case DYNAMIC_CSTRING  : return util::BTILE_PROPERTY_TYPE_STRING;
                    default               : return util::BTILE_PROPERTY_TYPE_INT;
                };
            };

            auto fGetValueAsString = [] (const std::shared_ptr<DYNAMIC_VAR> & dType) -> std::string
            {
                switch(dType->type)
                {
                    case DYNAMIC_BOOL     : return (dType->getBool() ? "1" : "0");
                    case DYNAMIC_CHAR     : return std::string(1,dType->getChar());
                    case DYNAMIC_INT      : return std::to_string(dType->getInt());
                    case DYNAMIC_FLOAT    : return std::to_string(dType->getFloat());
                    case DYNAMIC_CSTRING  : return std::string(dType->getString());
                    default               : return std::to_string(dType->getInt());;
                };
            };

            for(const auto & property : this->tileMap.properties)
            {
                util::BTILE_PROPERTY * bProperty = new util::BTILE_PROPERTY(fGetPropertyType(property.second->type));
                bProperty->name  = property.first;
                bProperty->value = fGetValueAsString(property.second);
                bProperty->owner = "map";
                tileInfo->lsProperty.push_back(bProperty);
            }

            int index_layer = 1;
            for(const auto & layer : this->tileMap.layers)
            {
                for(const auto & property : layer->properties)
                {
                    util::BTILE_PROPERTY * bProperty = new util::BTILE_PROPERTY(fGetPropertyType(property.second->type));
                    bProperty->name  = property.first;
                    bProperty->value = fGetValueAsString(property.second);
                    bProperty->owner = "layer-";
                    bProperty->owner += std::to_string(index_layer++);
                    tileInfo->lsProperty.push_back(bProperty);
                }
            }

            for(const auto & brick : this->tileMap.bricks)
            {
                for(const auto & property : brick.second->properties)
                {
                    util::BTILE_PROPERTY * bProperty = new util::BTILE_PROPERTY(fGetPropertyType(property.second->type));
                    bProperty->name  = property.first;
                    bProperty->value = fGetValueAsString(property.second);
                    bProperty->owner = "brick-";
                    bProperty->owner += std::to_string(brick.second->id);
                    tileInfo->lsProperty.push_back(bProperty);
                }
                for(const auto & cube : brick.second->brick_physics.lsCube)
                {
                    util::BTILE_OBJ* btile_obj = new util::BTILE_OBJ(util::BTILE_OBJ_TYPE_RECT);
                    btile_obj->name = "physic-";
                    btile_obj->name += std::to_string(brick.second->id);
                    btile_obj->lsPoints.push_back(new VEC2(cube->absCenter.x,  cube->absCenter.y));
                    btile_obj->lsPoints.push_back(new VEC2(cube->halfDim.x * 2,cube->halfDim.y * 2));
                    tileInfo->lsObj.push_back(btile_obj);
                }
                for(const auto & sphere : brick.second->brick_physics.lsSphere)
                {
                    util::BTILE_OBJ* btile_obj = new util::BTILE_OBJ(util::BTILE_OBJ_TYPE_CIRCLE);
                    btile_obj->name = "physic-";
                    btile_obj->name += std::to_string(brick.second->id);
                    btile_obj->lsPoints.push_back(new VEC2(sphere->absCenter[0],sphere->absCenter[1]));
                    btile_obj->lsPoints.push_back(new VEC2(sphere->ray,sphere->ray));
                    tileInfo->lsObj.push_back(btile_obj);
                }
                for(const auto & triangle : brick.second->brick_physics.lsTriangle)
                {
                    util::BTILE_OBJ* btile_obj = new util::BTILE_OBJ(util::BTILE_OBJ_TYPE_TRIANGLE);
                    btile_obj->name = "physic-";
                    btile_obj->name += std::to_string(brick.second->id);
                    btile_obj->lsPoints.push_back(new VEC2(triangle->point[0].x,triangle->point[0].y));
                    btile_obj->lsPoints.push_back(new VEC2(triangle->point[1].x,triangle->point[1].y));
                    btile_obj->lsPoints.push_back(new VEC2(triangle->point[2].x,triangle->point[2].y));
                    tileInfo->lsObj.push_back(btile_obj);
                }
            }

            for(const auto & obj : tileMap.objects)
            {
                util::BTILE_OBJ* btile_obj = nullptr;
                if(obj->cube)
                {
                    btile_obj = new util::BTILE_OBJ(util::BTILE_OBJ_TYPE_RECT);
                    btile_obj->lsPoints.push_back(new VEC2(obj->cube->absCenter.x,obj->cube->absCenter.y));
                    btile_obj->lsPoints.push_back(new VEC2(obj->cube->halfDim.x * 2,obj->cube->halfDim.y * 2));
                }
                else if(obj->triangle)
                {
                    btile_obj = new util::BTILE_OBJ(util::BTILE_OBJ_TYPE_TRIANGLE);
                    btile_obj->lsPoints.push_back(new VEC2(obj->triangle->point[0].x,obj->triangle->point[0].y));
                    btile_obj->lsPoints.push_back(new VEC2(obj->triangle->point[1].x,obj->triangle->point[1].y));
                    btile_obj->lsPoints.push_back(new VEC2(obj->triangle->point[2].x,obj->triangle->point[2].y));
                }
                else if(obj->sphere)
                {
                    btile_obj = new util::BTILE_OBJ(util::BTILE_OBJ_TYPE_CIRCLE);
                    btile_obj->lsPoints.push_back(new VEC2(obj->sphere->absCenter[0],obj->sphere->absCenter[1]));
                    btile_obj->lsPoints.push_back(new VEC2(obj->sphere->ray,obj->sphere->ray));
                }
                else if(obj->line)
                {
                    btile_obj = new util::BTILE_OBJ(util::BTILE_OBJ_TYPE_POLYLINE);
                    VEC2 * lines = obj->line.get();
                    for (size_t i = 0; i < obj->size_line; i++)
                    {
                        btile_obj->lsPoints.push_back(new VEC2(lines[i].x,lines[i].y));
                    }
                }
                else if(obj->point)
                {
                    btile_obj = new util::BTILE_OBJ(util::BTILE_OBJ_TYPE_POINT);
                    btile_obj->lsPoints.push_back(new VEC2(obj->point->x,obj->point->y));
                }
                if(btile_obj)
                {
                    btile_obj->name = obj->name;
                    tileInfo->lsObj.push_back(btile_obj);
                }
            }

            char errorText[255] = "";
            const bool ret = meshDebug.saveDebug(fileName,false,false,errorText,sizeof(errorText));
            if(ret == false)
                return log_util::fail(__LINE__, __FILE__, "Failed to save mesh\n[%s]",errorText);
            return true;
        }

        return false;
    }

    uint32_t TILE_EDITOR::getHowManyTile2Render() const
    {
        return iTilesToRender;
    }

    void TILE_EDITOR::setHowManyTile2Render(const uint32_t value)
    {
        iTilesToRender = value;
    }

    void TILE_EDITOR::selectTiles(const VEC2 & start,const VEC2 & end)
    {
        if (render_what == mbm::RENDER_LAYER)
        {
            if(index_render_what < tileMap.layers.size())
            {
                const auto & layer          = tileMap.layers[index_render_what];
                const float width_tile      = static_cast<float>(tileMap.size_width_tile  * scale.x);
                const float height_tile     = static_cast<float>(tileMap.size_height_tile * scale.y);
                
                for(float x = start.x; x <= end.x; x += width_tile)
                {
                    for(float y = start.y; y <= end.y; y += height_tile)
                    {
                        const uint16_t index_tile =  this->getIndexTileIdOver(x,y);
                        if(index_tile < layer->bricks.size())
                        {
                            const auto brick = layer->bricks[index_tile];
                            if(brick)
                            {
                                selectedBrick[index_tile] = true;
                            }
                        }
                    }
                }
            }
        }
    }

    uint16_t  TILE_EDITOR::getBrickIDByTileID(const uint16_t index_tile) const
    {
        if (render_what == mbm::RENDER_LAYER)
        {
            if(index_render_what < tileMap.layers.size())
            {
                const auto & layer = tileMap.layers[index_render_what];
                if(index_tile < layer->bricks.size())
                {
                    const auto   brick = layer->bricks[index_tile];
                    if(brick)
                        return brick->id;
                }
            }
        }
        return std::numeric_limits<uint16_t>::max()-1;
    }

    void TILE_EDITOR::duplicateSelectedTiles(const float x, const float y)
    {
        if (render_what == mbm::RENDER_LAYER && index_render_what < tileMap.layers.size() && selectedBrick.size() > 0)
        {
            const uint16_t iIndexBrickOver  = getIndexTileIdOver(x,y);
            auto & layer = tileMap.layers[index_render_what];
            if(iIndexBrickOver < layer->bricks.size())
            {
                float m_min_x             =  std::numeric_limits<float>::max();
                float m_max_y             = -std::numeric_limits<float>::max();
                uint16_t index_tile_found =  std::numeric_limits<uint16_t>::max();
                for(auto it = selectedBrick.cbegin(); it != selectedBrick.cend(); ++it)
                {
                    const uint16_t index_tile = it->first;
                    const auto brick          = layer->bricks[index_tile];
                    const auto itPos          = positionSelectedBrick.find(index_tile);
                    if(it->second && brick && itPos != positionSelectedBrick.cend())
                    {
                        if(itPos->second.x <= m_min_x && itPos->second.y >= m_max_y )
                        {
                            m_min_x          = itPos->second.x;
                            m_max_y          = itPos->second.y;
                            index_tile_found = index_tile;
                        }
                    }
                }
                if(index_tile_found != std::numeric_limits<uint16_t>::max())
                {
                    layer->bricks[iIndexBrickOver] = layer->bricks[index_tile_found];
                    for(auto it = selectedBrick.cbegin(); it != selectedBrick.cend(); ++it)
                    {
                        const uint16_t index_tile = it->first;
                        const auto brick          = layer->bricks[index_tile];
                        if(it->second && brick && index_tile_found != index_tile)
                        {
                            int diffOriginal = static_cast<int>(index_tile) - static_cast<int>(index_tile_found);
                            int iIndexNext   = static_cast<int>(iIndexBrickOver) + static_cast<int>(diffOriginal);
                            if(iIndexNext < static_cast<int>(layer->bricks.size()) && iIndexNext >= 0)
                            {
                                layer->bricks[iIndexNext] = layer->bricks[index_tile];
                            }
                        }
                    }
                }
            }
        }
    }
}
