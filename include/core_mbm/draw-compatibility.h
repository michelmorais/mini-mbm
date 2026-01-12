/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                            |
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

#ifndef DRAW_COMPATIBILITY_H
#define DRAW_COMPATIBILITY_H

#include <cstdint>
#include "core-exports.h"
//D3DPT_TRIANGLELIST is a primitive type in Direct3D that renders vertices as a sequence of isolated triangles, where each group of three vertices defines a separate triangle
namespace util
{
    enum MODE_DRAW : uint32_t
    {
        MODE_DRAW_POINTS          = 0,
        MODE_DRAW_LINES           = 1,
        MODE_DRAW_LINE_LOOP       = 2,
        MODE_DRAW_LINE_STRIP      = 3,
        MODE_DRAW_TRIANGLES       = 4,
        MODE_DRAW_TRIANGLE_STRIP  = 5,
        MODE_DRAW_TRIANGLE_FAN    = 6,
    };

    API_IMPL const char* get_mode_draw_from_uint(const uint32_t mode_draw, const char* default_mode_draw_ret) noexcept;
    API_IMPL const uint32_t get_mode_draw_from_string(const char* str_mode_draw) noexcept;
    
    enum CULL_MODE : uint32_t
    {
        CULL_FRONT          = 0x0404,
        CULL_BACK           = 0x0405,
        CULL_FRONT_AND_BACK = 0x0408,
    };

    API_IMPL const char* get_mode_cull_face_from_uint(const uint32_t mode_cull_face, const char* default_mode_cull_face_ret) noexcept;
    API_IMPL const uint32_t get_mode_cull_face_from_string(const char* str_mode_cull_face) noexcept;

    enum FACE_DIRECTION : uint32_t
    {
        CW = 0x0900,
        CCW = 0x0901,
    };

    API_IMPL const char* get_mode_front_face_direction_from_uint(const uint32_t mode_front_face_direction, const char* default_mode_front_face_direction_ret) noexcept;
    API_IMPL const uint32_t get_mode_front_face_direction_from_string(const char* str_mode_front_face_direction) noexcept;

    API_IMPL bool isBackendEngineOpenGlEs() noexcept;
    API_IMPL bool isBackendEngineDirectx() noexcept;
    API_IMPL bool isBackendEngineVulkan() noexcept;
    API_IMPL bool isBackendEngineMetal() noexcept;

    API_IMPL const bool is_mode_draw_valid(const uint32_t mode_draw)noexcept;
    API_IMPL const bool is_mode_cull_face_valid(const uint32_t mode_cull_face)noexcept;
    API_IMPL const bool is_mode_front_face_direction_valid(const uint32_t mode_front_face_direction)noexcept;

}
#endif