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

#include "brick.h"
#include <limits>

namespace mbm
{
    BRICK::~BRICK()
    {
        release();
    }

    void BRICK::release()
    {
        properties.clear();
        bufferGL.release();
    }

    void BRICK::build(const VEC3 new_vertex[4], const VEC2 new_uv[4])
    {
        bufferGL.release();
        if(width == 0 && texture)
            width = texture->getWidth();
        if(height == 0 && texture)
            height = texture->getHeight();
        int             indexStart = 0;
        int             indexCount = 6;
        VEC3            normal[4];
        if(new_vertex)
        {
            vertex[0].x = new_vertex[0].x;
            vertex[0].y = new_vertex[0].y;
            vertex[0].z = new_vertex[0].z;

            vertex[1].x = new_vertex[1].x;
            vertex[1].y = new_vertex[1].y;
            vertex[1].z = new_vertex[1].z;

            vertex[2].x = new_vertex[2].x;
            vertex[2].y = new_vertex[2].y;
            vertex[2].z = new_vertex[2].z;

            vertex[3].x = new_vertex[3].x;
            vertex[3].y = new_vertex[3].y;
            vertex[3].z = new_vertex[3].z;
        }
        else
        {
            const float x  = width * 0.5f;
            const float y  = height * 0.5f;
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
        }
        for (int i = 0; i < 4; ++i)
        {
            normal[i].x = 0;
            normal[i].y = 0;
            normal[i].z = 1;
        }
        if(new_uv)
        {
            uv[0].x = new_uv[0].x;
            uv[0].y = new_uv[0].y;
            uv[1].x = new_uv[1].x;
            uv[1].y = new_uv[1].y;
            uv[2].x = new_uv[2].x;
            uv[2].y = new_uv[2].y;
            uv[3].x = new_uv[3].x;
            uv[3].y = new_uv[3].y;
        }
        else
        {
            uv[0].x = 0;
            uv[0].y = 1;
            uv[1].x = 0;
            uv[1].y = 0;
            uv[2].x = 1;
            uv[2].y = 1;
            uv[3].x = 1;
            uv[3].y = 0;
        }
        
        unsigned short int index[6]      = {0, 1, 2, 2, 1, 3};
        const bool ret = this->bufferGL.loadBuffer(vertex, normal, uv, 4, index, 1, &indexStart, &indexCount,nullptr);
        if (ret && texture)
        {
            this->bufferGL.idTexture0[0] = texture->idTexture;
            this->bufferGL.idTexture1    = 0;
            this->bufferGL.useAlpha[0]   = this->texture->useAlphaChannel ? 1 : 0;
        }
        else
        {
            this->bufferGL.idTexture0[0] = 0;
            this->bufferGL.idTexture1    = 0;
            this->bufferGL.useAlpha[0]   = 0;
        }
    }

    bool BRICK::render(SHADER *shader,const uint32_t idTexStage2)
    {
        if(this->bufferGL.isLoadedBuffer())
        {
            this->bufferGL.idTexture1 = idTexStage2;
            return shader->render(&this->bufferGL);
        }
        return false;
    }
    void BRICK::setTexture(TEXTURE* _texture)
    {
        texture = _texture;
        if(this->bufferGL.isLoadedBuffer())
        {
            this->bufferGL.idTexture0[0] = texture ? texture->idTexture : 0;
            this->bufferGL.useAlpha[0]   = texture->useAlphaChannel ? 1 : 0;
        }
    }

    void BRICK::backup()
    {
        if(the_backup == nullptr)
        {
            the_backup          = std::make_unique<BRICK_BACKUP>();
            the_backup->width   = width;
            the_backup->height  = height;
            the_backup->texture = texture;
            the_backup->uv[0]   = uv[0];
            the_backup->uv[1]   = uv[1];
            the_backup->uv[2]   = uv[2];
            the_backup->uv[3]   = uv[3];
        }
    }
    
    void BRICK::restore_backup()
    {
        if(the_backup != nullptr)
        {
            width       = the_backup->width;
            height      = the_backup->height;
            texture     = the_backup->texture;
            uv[0]       = the_backup->uv[0];
            uv[1]       = the_backup->uv[1];
            uv[2]       = the_backup->uv[2];
            uv[3]       = the_backup->uv[3];
        }
    }

    void BRICK::expand(const bool inside,const float value)
    {
        if(inside)
        {
            uv[0].x = uv[0].x - value;
            uv[0].y = uv[0].y + value;
            uv[1].x = uv[1].x - value;
            uv[1].y = uv[1].y - value;
            uv[2].x = uv[2].x + value;
            uv[2].y = uv[2].y + value;
            uv[3].x = uv[3].x + value;
            uv[3].y = uv[3].y - value;
        }
        else
        {
            uv[0].x = uv[0].x + value;
            uv[0].y = uv[0].y - value;
            uv[1].x = uv[1].x + value;
            uv[1].y = uv[1].y + value;
            uv[2].x = uv[2].x - value;
            uv[2].y = uv[2].y - value;
            uv[3].x = uv[3].x - value;
            uv[3].y = uv[3].y + value;
        }
    }

    void BRICK::expandV(const bool inside,const float value)
    {
        if(inside)
        {
            uv[0].y = uv[0].y + value;
            uv[1].y = uv[1].y - value;
            uv[2].y = uv[2].y + value;
            uv[3].y = uv[3].y - value;
        }
        else
        {
            uv[0].y = uv[0].y - value;
            uv[1].y = uv[1].y + value;
            uv[2].y = uv[2].y - value;
            uv[3].y = uv[3].y + value;
        }
    }

    void BRICK::expandH(const bool inside,const float value)
    {
        if(inside)
        {
            uv[0].x = uv[0].x - value;
            uv[1].x = uv[1].x - value;
            uv[2].x = uv[2].x + value;
            uv[3].x = uv[3].x + value;
        }
        else
        {
            uv[0].x = uv[0].x + value;
            uv[1].x = uv[1].x + value;
            uv[2].x = uv[2].x - value;
            uv[3].x = uv[3].x - value;
        }
    }
}