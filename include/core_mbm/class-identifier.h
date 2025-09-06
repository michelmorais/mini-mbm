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
        L_USER_TYPE_BEGIN                     = 99,
        L_USER_TYPE_VEC2                      ,
        L_USER_TYPE_VEC3                      ,
        L_USER_TYPE_RENDERIZABLE              ,
        L_USER_TYPE_TIMER                     ,
        L_USER_TYPE_SHADER                    ,
        L_USER_TYPE_AUDIO                     ,
        L_USER_TYPE_VR                        ,
        L_USER_TYPE_TEXTURE                   ,
        L_USER_TYPE_SPRITE                    ,
        L_USER_TYPE_CAMERA_TARGET             ,
        L_USER_TYPE_RENDER_2_TEXTURE          ,
        L_USER_TYPE_PARTICLE                  ,
        L_USER_TYPE_STEERED_PARTICLE          ,
        L_USER_TYPE_MESH                      ,
        L_USER_TYPE_MESH_DEBUG                ,
        L_USER_TYPE_LINE                      ,
        L_USER_TYPE_GIF                       ,
        L_USER_TYPE_SHAPE_MESH                ,
        L_USER_TYPE_FONT                      ,
        L_USER_TYPE_TEXT                      ,
        L_USER_TYPE_BACKGROUND                ,
        L_USER_TYPE_BOX2D_LF                  ,
        L_USER_TYPE_BOX2D_LF_JOINT            ,
        L_USER_TYPE_BOX2D_LF_STEERED_PARTICLE ,
        L_USER_TYPE_BOX2D                     ,
        L_USER_TYPE_BOX2D_JOINT               ,
        L_USER_TYPE_BOX2D_STEERED_PARTICLE    ,
        L_USER_TYPE_BULLET3D                  ,
        L_USER_TYPE_TILE                      ,
        L_USER_TYPE_TILE_OBJ                  ,
        L_USER_TYPE_PLUGIN                    ,
        L_USER_TYPE_NEW_WRAPPER               ,
        L_USER_TYPE_END                       ,
    };
	// If you add type here, remember to update the function getUserTypeAsString

    API_IMPL const char * getUserTypeAsString(const int value);
    API_IMPL bool isRenderizableType(const int value);
    API_IMPL bool isRenderizableType(const L_USER_TYPE value);
}

#endif