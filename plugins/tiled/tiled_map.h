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

#ifndef TILED_MAP_HEADER_H
#define TILED_MAP_HEADER_H

#include <string>
#include <memory>
#include <map>

#include <core_mbm/primitives.h>
#include <core_mbm/dynamic-var.h>
#include <core_mbm/header-mesh.h>

#include "layer.h"
#include "brick.h"
#include "util_tiled.h"
#include "tile_set.h"

namespace mbm
{
    struct TILED_MAP
    {
        TILED_MAP():count_width_tile(10),
                    count_height_tile(10),
                    size_width_tile(256),
                    size_height_tile(256),
                    render_left_to_right(true),
                    render_top_to_down(false),
                    typeMap(util::BTILE_TYPE_ORIENTATION_ORTHOGONAL),
                    background(0.0f,0.0f,0.0f,0.0f),
                    background_texture(nullptr){}

        virtual ~TILED_MAP();

        uint32_t                                                         count_width_tile;
        uint32_t                                                         count_height_tile;
        uint32_t                                                         size_width_tile;
        uint32_t                                                         size_height_tile;
        bool                                                             render_left_to_right;
        bool                                                             render_top_to_down;
        util::BTILE_TYPE_MAP                                             typeMap;
        COLOR                                                            background;
        TEXTURE*                                                         background_texture;
        std::vector<std::shared_ptr<LAYER>>                              layers;
        std::vector<std::shared_ptr<TILED_SET>>                          tile_sets;
        std::map<std::string,std::shared_ptr<DYNAMIC_VAR>>               properties;
        std::map<uint16_t,std::shared_ptr<BRICK>>                        bricks;
        std::vector<std::shared_ptr<OBJ_TILE_MAP>>                       objects;

        void  release();
       
    };
}

#endif
