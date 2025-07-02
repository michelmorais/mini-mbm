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

#ifndef UTIL_TILED_HEADER_H
#define UTIL_TILED_HEADER_H

#include <stdint.h>
#include <vector>
#include <core_mbm/shapes.h>

#include <memory>

namespace mbm
{
    void resetBrickID();
    uint16_t getNextUniqueBrickID();
    
    struct FREE_PHYSICS
    {
        std::string                             name;
        std::vector<std::shared_ptr<CUBE>>      lsCube;
        std::vector<std::shared_ptr<SPHERE>>    lsSphere;
        std::vector<std::shared_ptr<TRIANGLE>>  lsTriangle;
        
        void release();
    };

    struct OBJ_TILE_MAP
    {
        OBJ_TILE_MAP() noexcept :size_line(0){}
        std::string name;
        uint32_t size_line;
        std::shared_ptr<VEC2>      line;
        std::shared_ptr<VEC2>      point;
        std::shared_ptr<CUBE>      cube;
        std::shared_ptr<SPHERE>    sphere;
        std::shared_ptr<TRIANGLE>  triangle;
        void release();
    };
}

#endif