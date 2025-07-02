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

#ifndef TILED_SET_HEADER_H
#define TILED_SET_HEADER_H

#include <string>
#include <memory>
#include <map>
#include <vector>

#include <core_mbm/primitives.h>
#include <core_mbm/dynamic-var.h>

#include "brick.h"

#include "util_tiled.h"

namespace mbm
{
    struct TILED_SET
    {
        TILED_SET(std::string _name,const uint32_t w,const uint32_t h):name(_name),tile_width(w),tile_height(h){}
        virtual ~TILED_SET();

        std::string                                         name;
        std::vector<std::shared_ptr<BRICK>>                 bricks;
        std::map<std::string,std::shared_ptr<DYNAMIC_VAR>>  properties;
        uint32_t                                            tile_width;
        uint32_t                                            tile_height;
        void  release();
    
    };
}

#endif