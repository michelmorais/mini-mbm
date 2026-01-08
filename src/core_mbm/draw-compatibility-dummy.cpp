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

#if defined (USE_DUMMY_BACK_END_ENGINE)

#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions

#include <draw-compatibility.h>
#include <limits>
#include <cstring>


namespace util
{
    uint32_t get_mode_draw_from_enum(const util::MODE_DRAW enum_mode_draw) noexcept 
    {
        #pragma message(REMINDER_TODO "  implement me");
        switch (enum_mode_draw)
        {
            case util::MODE_DRAW_POINTS:         return util::MODE_DRAW_POINTS;
            case util::MODE_DRAW_LINES:          return util::MODE_DRAW_LINES;
            case util::MODE_DRAW_LINE_LOOP:      return util::MODE_DRAW_LINE_LOOP;
            case util::MODE_DRAW_LINE_STRIP:     return util::MODE_DRAW_LINE_STRIP;
            case util::MODE_DRAW_TRIANGLES:      return util::MODE_DRAW_TRIANGLES;
            case util::MODE_DRAW_TRIANGLE_STRIP: return util::MODE_DRAW_TRIANGLE_STRIP;
            case util::MODE_DRAW_TRIANGLE_FAN:   return util::MODE_DRAW_TRIANGLE_FAN;
            default: return std::numeric_limits<uint32_t>::max();
        }
    }

    const char* get_mode_draw_from_uint(const uint32_t mode_draw, const char* default_mode_draw_ret) noexcept 
    {
        switch (mode_draw)
        {
            case util::MODE_DRAW_POINTS:         return "POINTS";
            case util::MODE_DRAW_LINES:          return "LINES";
            case util::MODE_DRAW_LINE_LOOP:      return "LINE_LOOP";
            case util::MODE_DRAW_LINE_STRIP:     return "LINE_STRIP";
            case util::MODE_DRAW_TRIANGLES:      return "TRIANGLES";
            case util::MODE_DRAW_TRIANGLE_STRIP: return "TRIANGLE_STRIP";
            case util::MODE_DRAW_TRIANGLE_FAN:   return "TRIANGLE_FAN";
            default: return default_mode_draw_ret;
        }
    }

    uint32_t get_mode_cull_face_from_enum(const CULL_MODE enum_mode_cull_face) noexcept
    {
        #pragma message(REMINDER_TODO "  implement me");
        switch (enum_mode_cull_face)
        {
            case util::CULL_FRONT:          return util::CULL_FRONT;
            case util::CULL_BACK:           return util::CULL_BACK;
            case util::CULL_FRONT_AND_BACK: return util::CULL_FRONT_AND_BACK;
            default: return std::numeric_limits<uint32_t>::max();
        }
    }

    const char* get_mode_cull_face_from_uint(const uint32_t mode_cull_face, const char* default_mode_cull_face_ret) noexcept
    {
        switch (mode_cull_face)
        {
            case util::CULL_FRONT:          return "FRONT";
            case util::CULL_BACK:           return "BACK";
            case util::CULL_FRONT_AND_BACK: return "FRONT_AND_BACK";
            default: return default_mode_cull_face_ret;
        }
    }

    const uint32_t get_mode_cull_face_from_string(const char* str_mode_cull_face) noexcept
    {
        #pragma message(REMINDER_TODO "  implement me");
        constexpr uint32_t default_mode_cull_face_ret = std::numeric_limits<uint32_t>::max();
        if (str_mode_cull_face == nullptr)
            return default_mode_cull_face_ret;
        if (strcmp(str_mode_cull_face, "FRONT") == 0)
            return util::CULL_FRONT;
        if (strcmp(str_mode_cull_face, "BACK") == 0)
            return util::CULL_BACK;
        if (strcmp(str_mode_cull_face, "FRONT_AND_BACK") == 0)
            return util::CULL_FRONT_AND_BACK;
        return default_mode_cull_face_ret;
    }

    uint32_t get_mode_front_face_direction_enum(const FACE_DIRECTION enum_mode_front_face_direction) noexcept
    {
        #pragma message(REMINDER_TODO "  implement me");
        switch (enum_mode_front_face_direction)
        {
            case util::CW:  return util::CW;
            case util::CCW: return util::CCW;
            default: return std::numeric_limits<uint32_t>::max();
        }
    }

    const char* get_mode_front_face_direction_from_uint(const uint32_t mode_front_face_direction, const char* default_mode_front_face_direction_ret) noexcept
    {
        switch (mode_front_face_direction)
        {
            case util::CW:  return "CW";
            case util::CCW: return "CCW";
            default: return default_mode_front_face_direction_ret;
        }
    }

    const uint32_t get_mode_draw_from_string(const char* str_mode_draw) noexcept
    {
        #pragma message(REMINDER_TODO "  implement me");
        constexpr uint32_t default_mode_draw_ret = std::numeric_limits<uint32_t>::max();
        if (str_mode_draw == nullptr)
            return default_mode_draw_ret;
        if (strcmp(str_mode_draw, "TRIANGLES") == 0)
            return util::MODE_DRAW_TRIANGLES;
        if (strcmp(str_mode_draw, "TRIANGLE_STRIP") == 0)
            return util::MODE_DRAW_TRIANGLE_STRIP;
        if (strcmp(str_mode_draw, "TRIANGLE_FAN") == 0)
            return util::MODE_DRAW_TRIANGLE_FAN;
        if (strcmp(str_mode_draw, "LINES") == 0)
            return util::MODE_DRAW_LINES;
        if (strcmp(str_mode_draw, "LINE_LOOP") == 0)
            return util::MODE_DRAW_LINE_LOOP;
        if (strcmp(str_mode_draw, "LINE_STRIP") == 0)
            return util::MODE_DRAW_LINE_STRIP;
        if (strcmp(str_mode_draw, "POINTS") == 0)
            return util::MODE_DRAW_POINTS;
        return default_mode_draw_ret;
    }

    const uint32_t get_mode_front_face_direction_from_string(const char* str_mode_front_face_direction) noexcept
    {
        #pragma message(REMINDER_TODO "  implement me");
        constexpr uint32_t default_mode_front_face_direction_ret = std::numeric_limits<uint32_t>::max();
        if (str_mode_front_face_direction == nullptr)
            return default_mode_front_face_direction_ret;
        if (strcmp(str_mode_front_face_direction, "CW") == 0)
            return util::CW;
        if (strcmp(str_mode_front_face_direction, "CCW") == 0)
            return util::CCW;
        return default_mode_front_face_direction_ret;
    }

    const bool is_mode_draw_valid(const uint32_t mode_draw)noexcept
    {
        #pragma message(REMINDER_TODO "  implement me");
        switch(mode_draw)
        {
            case util::MODE_DRAW_POINTS		   : return true;
            case util::MODE_DRAW_LINES		   : return true;
            case util::MODE_DRAW_LINE_LOOP	   : return true;
            case util::MODE_DRAW_LINE_STRIP	   : return true;
            case util::MODE_DRAW_TRIANGLES 	   : return true;
            case util::MODE_DRAW_TRIANGLE_STRIP : return true;
            case util::MODE_DRAW_TRIANGLE_FAN   : return true;
            default                : return false;
        }
    }

    const bool is_mode_cull_face_valid(const uint32_t mode_cull_face)noexcept
    {
        #pragma message(REMINDER_TODO "  implement me");
        switch(mode_cull_face)
        {
            case util::CULL_FRONT		   : return true;
            case util::CULL_BACK		   : return true;
            case util::CULL_FRONT_AND_BACK : return true;
            default                : return false;
        }
    }

    const bool is_mode_front_face_direction_valid(const uint32_t mode_front_face_direction)noexcept
    {
        #pragma message(REMINDER_TODO "  implement me");
        switch(mode_front_face_direction)
        {
            case util::CW  		   : return true;
            case util::CCW 		   : return true;
            default                : return false;
        }
    }

}
#endif //USE_OPENGL_ES