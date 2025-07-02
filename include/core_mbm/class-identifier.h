/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                            |
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

/*
    Class identifier for the engine
*/

#ifndef CLASS_IDENTIFIER_USER_TYPE_H
#define CLASS_IDENTIFIER_USER_TYPE_H

#include "core-exports.h"

namespace mbm
{
    enum L_USER_TYPE : int
    {
        L_USER_TYPE_BEGIN                  = 99,
        L_USER_TYPE_VEC2                   = 100,
        L_USER_TYPE_VEC3                   = 101,
        L_USER_TYPE_RENDERIZABLE           = 102,
        L_USER_TYPE_TIMER                  = 103,
        L_USER_TYPE_SHADER                 = 104,
        L_USER_TYPE_AUDIO                  = 105,
        L_USER_TYPE_VR                     = 106,
        L_USER_TYPE_TEXTURE                = 107,
        L_USER_TYPE_SPRITE                 = 108,
        L_USER_TYPE_CAMERA_TARGET          = 109,
        L_USER_TYPE_RENDER_2_TEXTURE       = 110,
        L_USER_TYPE_PARTICLE               = 111,
        L_USER_TYPE_STEERED_PARTICLE       = 112,
        L_USER_TYPE_MESH                   = 113,
        L_USER_TYPE_MESH_DEBUG             = 114,
        L_USER_TYPE_LINE                   = 115,
        L_USER_TYPE_GIF                    = 116,
        L_USER_TYPE_SHAPE_MESH             = 117,
        L_USER_TYPE_FONT                   = 118,
        L_USER_TYPE_TEXT                   = 119,
        L_USER_TYPE_BACKGROUND             = 120,
        L_USER_TYPE_BOX2D                  = 121,
        L_USER_TYPE_BOX2D_JOINT            = 122,
        L_USER_TYPE_BOX2D_STEERED_PARTICLE = 123,
        L_USER_TYPE_BULLET3D               = 124,
        L_USER_TYPE_TILE                   = 125,
        L_USER_TYPE_TILE_OBJ               = 126,
        L_USER_TYPE_PLUGIN                 = 127,
        L_USER_TYPE_NEW_WRAPPER            = 128,
        L_USER_TYPE_END                    = 129,
    };

    API_IMPL const char * getUserTypeAsString(const int value);
    API_IMPL bool isRenderizableType(const int value);
    API_IMPL bool isRenderizableType(const L_USER_TYPE value);
}

#endif