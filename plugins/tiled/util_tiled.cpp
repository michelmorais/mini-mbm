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

#include "util_tiled.h"

namespace mbm
{
    static uint16_t unique_id_tiled_brick = 1;
    void resetBrickID()
    {
        unique_id_tiled_brick = 1;
    }

    uint16_t getNextUniqueBrickID()
    {
        return unique_id_tiled_brick++;
    }

    void FREE_PHYSICS::release()
    {
        lsCube.clear();
        lsSphere.clear();
        lsTriangle.clear();
    }
    void OBJ_TILE_MAP::release()
    {
        line      = std::shared_ptr<VEC2>();
        point     = std::shared_ptr<VEC2>();
        cube      = std::shared_ptr<CUBE>();
        sphere    = std::shared_ptr<SPHERE>();
        triangle  = std::shared_ptr<TRIANGLE>();
        size_line = 0;
        name.clear();
    }
}
