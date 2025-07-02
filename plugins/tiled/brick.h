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

#ifndef BRICK_HEADER_H
#define BRICK_HEADER_H

#include <string>
#include <core_mbm/dynamic-var.h>
#include <core_mbm/primitives.h>
#include <core_mbm/texture-manager.h>
#include <core_mbm/shader.h>
#include <core_mbm/shader-fx.h>
#include <memory>
#include <map>

#include "util_tiled.h"

namespace mbm
{
    struct BRICK_BACKUP
    {
        uint32_t                                            width;
        uint32_t                                            height;
        TEXTURE*                                            texture;
        VEC2                                                uv[4];
        VEC3                                                vertex[4];
    };

    enum ROTATION_BRICK : uint16_t
    {
        ROTATION_NONE,
        ROTATE_1,
        ROTATE_2,
        ROTATE_3,
    };

    struct BRICK
    {
        BRICK(const ROTATION_BRICK  rotation,const bool flipped,const uint16_t the_original_id):
        id(getNextUniqueBrickID()),
        original_id(the_original_id == std::numeric_limits<uint16_t>::max() ? id : the_original_id),
        width(0),
        height(0),
        texture(nullptr),
        iRotation(rotation),
        bFlipped(flipped)
        {
        }
        ~BRICK();

        const uint16_t                                      id;
        const uint16_t                                      original_id;
        uint32_t                                            width;
        uint32_t                                            height;
        TEXTURE*                                            texture;
        VEC2                                                uv[4];
        VEC3                                                vertex[4];
        std::map<std::string,std::shared_ptr<DYNAMIC_VAR>>  properties;
        FREE_PHYSICS                                        brick_physics;

        void build(const VEC3 new_vertex[4], const VEC2 new_uv[4]);
        void release();
        bool render(SHADER *shader,const uint32_t idTexStage2);
        void setTexture(TEXTURE* _texture);
        void backup();
        void restore_backup();
        void expand(const bool inside,const float value);
        void expandV(const bool inside,const float value);
        void expandH(const bool inside,const float value);
        const ROTATION_BRICK  iRotation; // 0, 1, 2, 3
        const bool            bFlipped;
        inline std::map<std::string,std::shared_ptr<DYNAMIC_VAR>> &   getProperties() { return properties;}
        
    private:
        std::shared_ptr<BRICK_BACKUP> the_backup;
        BUFFER_GL bufferGL;
    };
}

#endif