/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <font.h>
#include <util.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <algorithm>
#include <cfloat>
#include <set>
#include <platform/mismatch-platform.h>
#include <cstdarg>


namespace mbm
{

    TEXT_DRAW::~TEXT_DRAW()
    {
        this->device->removeRenderizable(this);
        this->release();
    }
    
    void TEXT_DRAW::release()
    {
        this->releaseAnimation();
        this->mesh                  = nullptr;
        this->indexCurrentAnimation = 0;
        this->wildCardChangeAnim      = 0;
    }
    
    TEXT_DRAW::TEXT_DRAW(const int idScene, const bool _is3d, const bool _is2dScreen, OnRestoreFont ptrOnRestoreFont,FONT_DRAW *_parentFONT_DRAW)
        : RENDERIZABLE(idScene, TYPE_CLASS_TEXT, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->widthFirstLetter      = 0;
        this->indexCurrentAnimation = 0;
        this->mesh                  = nullptr;
        this->onRestoreFont         = ptrOnRestoreFont;
        this->parentFONT_DRAW       = _parentFONT_DRAW;
        this->beginText             = VEC2(0, 0);
        this->endText               = VEC2(0, 0);
        this->spaceXCharacter       = 0.0f;
        this->spaceYCharacter       = 0.0f;
        this->wildCardChangeAnim      = 0;
        this->aligned               = ALIGN_LEFT;
        this->device->addRenderizable(this);
        this->text = "Hello Font!";
    }

    TEXT_DRAW::TEXT_DRAW(const int idScene, const bool _is3d, const bool _is2dScreen, const char *newText,
                         OnRestoreFont ptrOnRestoreFont, FONT_DRAW *_parentFONT_DRAW)
        : RENDERIZABLE(idScene, TYPE_CLASS_TEXT, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->widthFirstLetter      = 0;
        this->indexCurrentAnimation = 0;
        this->mesh                  = nullptr;
        this->onRestoreFont         = ptrOnRestoreFont;
        this->parentFONT_DRAW       = _parentFONT_DRAW;
        this->beginText             = VEC2(0, 0);
        this->endText               = VEC2(0, 0);
        this->spaceXCharacter       = 0.0f;
        this->spaceYCharacter       = 0.0f;
        this->wildCardChangeAnim      = 0;
        this->aligned               = ALIGN_LEFT;
        this->device->addRenderizable(this);
        if (newText)
            this->text = newText;
        else
            this->text = "Hello Font!";
    }
    
    TEXT_DRAW::TEXT_DRAW(const int idScene, const bool _is3d, const bool _is2dScreen, const char *newText, VEC3 &position,
              OnRestoreFont ptrOnRestoreFont, FONT_DRAW *_parentFONT_DRAW)
        : RENDERIZABLE(idScene, TYPE_CLASS_TEXT, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->widthFirstLetter      = 0;
        this->position              = position;
        this->indexCurrentAnimation = 0;
        this->mesh                  = nullptr;
        this->onRestoreFont         = ptrOnRestoreFont;
        this->parentFONT_DRAW       = _parentFONT_DRAW;
        this->beginText             = VEC2(0, 0);
        this->endText               = VEC2(0, 0);
        this->spaceXCharacter       = 0.0f;
        this->spaceYCharacter       = 0.0f;
        this->wildCardChangeAnim      = 0;
        this->aligned               = ALIGN_LEFT;
        this->device->addRenderizable(this);
        if (newText)
            this->text = newText;
        else
            this->text = "Hello Font!";
    }
    
    TEXT_DRAW::TEXT_DRAW(const int idScene, const bool _is3d, const bool _is2dScreen, const char *newText, VEC2 &position,
              OnRestoreFont ptrOnRestoreFont, FONT_DRAW *_parentFONT_DRAW)
        : RENDERIZABLE(idScene, TYPE_CLASS_TEXT, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->widthFirstLetter      = 0;
        this->position.x            = position.x;
        this->position.y            = position.y;
        this->indexCurrentAnimation = 0;
        this->mesh                  = nullptr;
        this->beginText             = VEC2(0, 0);
        this->endText               = VEC2(0, 0);
        this->spaceXCharacter       = 0.0f;
        this->spaceYCharacter       = 0.0f;
        this->wildCardChangeAnim      = 0;
        this->aligned               = ALIGN_LEFT;
        if (newText)
            this->text = newText;
        else
            this->text        = "Hello Font!";
        this->onRestoreFont   = ptrOnRestoreFont;
        this->parentFONT_DRAW = _parentFONT_DRAW;
        this->device->addRenderizable(this);
    }

	unsigned char TEXT_DRAW::withoutBOM2Map(unsigned char index, const unsigned char mapBoom) noexcept
    {
        switch (mapBoom)
        {
            case 194:
            {
                switch (index)
                {
                    case 162: { index = 162;}
                    break; //¢
                    case 163: { index = 163;}
                    break; //£
                    case 185: { index = 185;}
                    break; //¹
                    case 186: { index = 186;}
                    break; //º
                    case 187: { index = 187;}
                    break; //»
                    case 170: { index = 170;}
                    break; //ª
                    case 171: { index = 171;}
                    break; //«
                    case 172: { index = 172;}
                    break; //¬
                    case 176: { index = 176;}
                    break; //°
                    case 178: { index = 178;}
                    break; //²
                    case 179: { index = 179;}
                    break; //³
                    default: { return index;}
                }
            }
            break;
            case 195:
            {
                switch (index)
                {
                    case 161: { index = 225;}
                    break; //á
                    case 160: { index = 224;}
                    break; //à
                    case 169: { index = 233;}
                    break; //é
                    case 168: { index = 232;}
                    break; //è
                    case 173: { index = 237;}
                    break; //í
                    case 172: { index = 236;}
                    break; //ì
                    case 179: { index = 243;}
                    break; //ó
                    case 178: { index = 242;}
                    break; //ò
                    case 186: { index = 250;}
                    break; //ú
                    case 185: { index = 249;}
                    break; //ù
                    case 129: { index = 193;}
                    break; //Á
                    case 128: { index = 192;}
                    break; //À
                    case 137: { index = 201;}
                    break; //É
                    case 136: { index = 200;}
                    break; //È
                    case 141: { index = 205;}
                    break; //Í
                    case 140: { index = 204;}
                    break; //Ì
                    case 147: { index = 211;}
                    break; //Ó
                    case 146: { index = 210;}
                    break; //Ò
                    case 154: { index = 218;}
                    break; //Ú
                    case 153: { index = 217;}
                    break; //Ù
                    case 163: { index = 227;}
                    break; //ã
                    case 181: { index = 245;}
                    break; //õ
                    case 131: { index = 195;}
                    break; //Ã
                    case 149: { index = 213;}
                    break; //Õ
                    case 167: { index = 231;}
                    break; //ç
                    case 135: { index = 199;}
                    break; //Ç
                    case 162: { index = 226;}
                    break; //â
                    case 170: { index = 234;}
                    break; //ê
                    case 174: { index = 238;}
                    break; //î
                    case 180: { index = 244;}
                    break; //ô
                    case 187: { index = 251;}
                    break; //û
                    case 130: { index = 194;}
                    break; //Â
                    case 138: { index = 202;}
                    break; //Ê
                    case 142: { index = 206;}
                    break; //Î
                    case 148: { index = 212;}
                    break; //Ô
                    case 155: { index = 219;}
                    break; //Û
                    default: { return index;}
                }
            }
            break;
        }
        return index;
    }
    
    void TEXT_DRAW::setText(const char *format, ...)
    {
        va_list va_args;
        va_start(va_args, format);
        auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, va_args));
        va_end(va_args);
        va_start(va_args, format);
        char *_buffer = log_util::formatNewMessage(length, format, va_args);
        va_end(va_args);
        this->text = _buffer;
        delete[] _buffer;
    }
    
    bool TEXT_DRAW::getWidthHeight(float *_width, float *_height, const bool ) const
    {
        if (this->mesh && mesh->isLoaded())
        {
            *_width  = this->endText.x - this->beginText.x;
            *_height = this->beginText.y - this->endText.y;
            return true;
        }
        return false;
    }
    
    void TEXT_DRAW::forceCalcSize()
    {
        this->renderText(false);
        float w, h;
        if (this->getWidthHeight(&w, &h))
        {
            mbm::CUBE *cube = nullptr;
            if (this->mesh->infoPhysics.lsCube.size() == 0)
            {
                cube = new mbm::CUBE();
                this->mesh->infoPhysics.lsCube.push_back(cube);
            }
            else
            {
                cube = this->mesh->infoPhysics.lsCube[0];
            }
            cube->halfDim.x       = w * 0.5f;
            cube->halfDim.y       = h * 0.5f;
            cube->halfDim.z       = 1.0f;
            this->bounding_AABB.x = w;
            this->bounding_AABB.y = h;
        }
    }
    
    bool TEXT_DRAW::getWidthHeightString(float *_width, float *_height, const char *str)
    {
        bool              ret = false;
        const std::string oldText(this->text);
        const VEC2        oldEndText(this->endText);
        const VEC2        oldBeginText(this->beginText);
        this->text = str;
        const float oldWidthFirstLetter(this->widthFirstLetter);
        if (this->renderText(false))
        {
            *_width  = this->endText.x - this->beginText.x;
            *_height = this->beginText.y - this->endText.y;
            ret      = true;
        }
        this->endText   = oldEndText;
        this->beginText = oldBeginText;
        this->text      = oldText;
        this->widthFirstLetter = oldWidthFirstLetter;
        return ret;
    }
    
    bool TEXT_DRAW::isOver3d(DEVICE *, const float x, const float y) const
    {
        float w, h, d;
        this->getAABB(&w, &h, &d);
        VEC3 p1, p2;
        device->transformeScreen2dToWorld3d_scaled(x, y, &p1, 100);
        device->transformeScreen2dToWorld3d_scaled(x, y, &p2, 1000);
        const VEC3 dir(p2 - p1);
        w *= 0.5f;
        h *= 0.5f;
        d *= 0.5f;
        const VEC3 pos(this->position.x + (w * 0.5f), this->position.y - (h * 0.5f), this->position.z);
        // dir is unit direction vector of ray
        const VEC3 dirfrac(dir.x != 0.0f ? 1.0f / dir.x : 0.0f, dir.y != 0.0f ? 1.0f / dir.y : 0.0f,
                           dir.z != 0.0f ? 1.0f / dir.z : 0.0f);
        float t1 = ((pos.x + w) - p1.x) * dirfrac.x;
        float t2 = ((pos.x - w) - p1.x) * dirfrac.x;
        float t3 = ((pos.y + h) - p1.y) * dirfrac.y;
        float t4 = ((pos.y - h) - p1.y) * dirfrac.y;
        float t5 = ((pos.z + d) - p1.z) * dirfrac.z;
        float t6 = ((pos.z - d) - p1.z) * dirfrac.z;

        float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
        float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));
        // if tmax < 0, ray (line) is intersecting AABB, but whole AABB is behing us
        if (tmax < 0)
            return false;
        // if tmin > tmax, ray doesn't intersect AABB
        if (tmin > tmax)
            return false;
        return true;
    }
    
    bool TEXT_DRAW::isOver2dw(DEVICE *, const float x, const float y) const
    {
        float w, h;
        this->getAABB(&w, &h);
        const VEC2 point(x, y);
        VEC2       halfDim(w * 0.5f, h * 0.5f);
        const VEC3 pos(this->position.x + halfDim.x, this->position.y - halfDim.y, this->position.z);
        if (device->isPointScreen2DOnRectangleWorld2d(point, halfDim, pos))
            return true;
        return false;
    }
    
    bool TEXT_DRAW::isOver2ds(DEVICE *, const float x, const float y) const
    {
        float w, h;
        this->getAABB(&w, &h);
        const VEC2 point(x, y);
        VEC2       halfDim(w * 0.5f, h * 0.5f);
        const VEC3 pos(this->position.x + halfDim.x , this->position.y + halfDim.y, this->position.z);
        if (device->isPointScreen2DOnRectangleScreen2d(point, halfDim, pos))
            return true;
        return false;
    }
    
    bool TEXT_DRAW::isOnFrustum()
    {
        if (this->mesh && mesh->isLoaded() && this->indexCurrentAnimation < this->lsAnimation.size())
        {
            float w = 0.0f;
            float h = 0.0f;
            if (this->getWidthHeight(&w, &h))
            {
                mbm::CUBE *cube = nullptr;
                if (this->mesh->infoPhysics.lsCube.size() == 0)
                {
                    cube = new mbm::CUBE();
                    this->mesh->infoPhysics.lsCube.push_back(cube);
                }
                else
                {
                    cube = this->mesh->infoPhysics.lsCube[0];
                }
                cube->halfDim.x       = w * 0.5f;
                cube->halfDim.y       = h * 0.5f;
                cube->halfDim.z       = 1.0f;
                this->bounding_AABB.x = w;
                this->bounding_AABB.y = h;
            }
            const float ws = w * 0.5f;
            const float hs = h * 0.5f;
            this->position.x += ws;
            this->position.y -= hs;
            IS_ON_FRUSTUM verify(this);
            const bool ret = verify.isOnFrustum(this->is3D, this->is2dS);
            this->position.x -= ws;
            this->position.y += hs;
            if(ret == false)
            {
                ANIMATION *anim = this->getAnimation();
                anim->updateAnimation(this->device->delta, this, this->onEndAnimation, this->onEndFx);
            }
            return ret;
            return ret;
        }
        return false;
    }
    
    bool TEXT_DRAW::renderText(const bool doRender)
    {
        if (this->mesh && this->isLoaded() && this->indexCurrentAnimation < this->lsAnimation.size())
        {
            ANIMATION *anim = this->lsAnimation[this->indexCurrentAnimation];
            if (doRender)
                anim->updateAnimation(this->device->delta,this,this->onEndAnimation,this->onEndFx);
			const INFO_BOUND_FONT * infoFont = this->mesh->getInfoFont();
			if(infoFont == nullptr)
				return false;
            const std::string  textDraw(this->text);
            const auto s = static_cast<unsigned int>(textDraw.size());
            static VEC3     posTemp2d(0, 0, 0);
            if (this->is3D)
                posTemp2d = this->position;
            else if (this->is2dS)
            {
                this->device->transformeScreen2dToWorld2d_scaled(this->position.x, this->position.y, posTemp2d);
                posTemp2d.x -= this->widthFirstLetter * this->scale.x;
                posTemp2d.z = this->position.z;
            }
            else
                posTemp2d = this->position;
            MatrixTranslationRotationScale(&SHADER::modelView, &posTemp2d, &this->angle, &this->scale);
            endText.x = -FLT_MAX;
            if (doRender)
                this->blend.set(anim->blendState);
            SHADER::modelView._42 -= infoFont->heightLetter * 0.5f  * this->scale.y;
            beginText.x          = SHADER::modelView._41;
            beginText.y          = SHADER::modelView._42;
            if(s == 0)
            {
                this->endText.x =   beginText.x;
                this->endText.y =   beginText.y;
                return true;
            }
            float beginText_x        = beginText.x;
            float beginText_y        = beginText.y;
            float endText_x          = endText.x;
            float endText_y          = endText.y;

            float wTotalAlign = 0.0f;
            float hTotalAlign = 0.0f;
            std::size_t  pN = std::string::npos;
            if (doRender && aligned != ALIGN_LEFT)
            {
                const MATRIX bck(SHADER::modelView);
                textWithoutSpeciaLetters = getTextWithoutSpecialLetters();
                pN = textWithoutSpeciaLetters.find('\n');
                if(pN != std::string::npos && 
                    this->getWidthHeightString(&wTotalAlign,&hTotalAlign,textWithoutSpeciaLetters.c_str()))
                {
                    float wPartAlign = 0.0f;
                    float hPartAlign = 0.0f;
                    const std::string newTextPart = this->textWithoutSpeciaLetters.substr(0,pN+1);
                    if(this->getWidthHeightString(&wPartAlign,&hPartAlign,newTextPart.c_str())
                        && wPartAlign < wTotalAlign)
                    {
                        SHADER::modelView = bck;
                        const float diff = (wTotalAlign - wPartAlign);
                        if(aligned == ALIGN_CENTER)
                            SHADER::modelView._41 += diff * 0.5f;
                        else
                            SHADER::modelView._41 += diff;
                    }
                    else
                    {
                        SHADER::modelView = bck;
                    }
                }
                else
                {
                    SHADER::modelView = bck;
                }
                beginText.x = beginText_x;
                beginText.y = beginText_y;
                endText.x   = endText_x;
                endText.y   = endText_y;
            }
            std::set<unsigned int> lsUpdateAnimation = {0xffffffff};
            float curWidthLetter = 0;
            for (unsigned int i = 0; i < s; ++i)
            {
                auto index = static_cast<unsigned char>(textDraw[i]);
                switch (index)
                {
                    case 194: // UTF8 - Without BOM
                    {
                        if ((i + 1) < s)
                        {
                            i++;
                            index = static_cast<unsigned char>(textDraw[i]);
                            index = withoutBOM2Map(index, 194);
                        }
                    }
                    break;
                    case 195: // UTF8 - Without BOM
                    {
                        if ((i + 1) < s)
                        {
                            i++;
                            index = static_cast<unsigned char>(textDraw[i]);
                            index = withoutBOM2Map(index, 195);
                        }
                    }
                    break;
                    default: {
                    }
                    break;
                }
                switch (index)
                {
                    case '\n':
                    {
                        SHADER::modelView._41 = beginText.x;
                        SHADER::modelView._42 -= static_cast<float>(infoFont->heightLetter + this->spaceYCharacter)  * this->scale.y;
                        if (doRender && aligned != ALIGN_LEFT)
                        {
                            beginText_x        = beginText.x;
                            beginText_y        = beginText.y;
                            endText_x          = endText.x;
                            endText_y          = endText.y;
                            float wPartAlign = 0.0f;
                            float hPartAlign = 0.0f;
                            const MATRIX bck(SHADER::modelView);
                            const std::size_t newP = textWithoutSpeciaLetters.find('\n',pN+1);
                            if(newP != std::string::npos)
                            {
                                const std::string newText = textWithoutSpeciaLetters.substr(pN+1,newP-pN);
                                if(this->getWidthHeightString(&wPartAlign,&hPartAlign,newText.c_str()))
                                {
                                    SHADER::modelView = bck;
                                    if(wPartAlign < wTotalAlign)
                                    {
                                        const float diff = wTotalAlign - wPartAlign;
                                        if(aligned == ALIGN_CENTER)
                                            SHADER::modelView._41 += (diff * 0.5f);
                                        else//right
                                            SHADER::modelView._41 += diff;
                                    }
                                }
                                else
                                {
                                    SHADER::modelView = bck;
                                }
                            }
                            else
                            {
                                const std::string newText = textWithoutSpeciaLetters.substr(pN+1);
                                if(this->getWidthHeightString(&wPartAlign,&hPartAlign,newText.c_str()))
                                {
                                    SHADER::modelView = bck;
                                    if(wPartAlign < wTotalAlign)
                                    {
                                        const float diff = wTotalAlign - wPartAlign;
                                        if(aligned == ALIGN_CENTER)
                                            SHADER::modelView._41 += (diff * 0.5f);
                                        else//right
                                            SHADER::modelView._41 += diff;
                                    }
                                }
                                else
                                {
                                    SHADER::modelView = bck;
                                }
                            }
                            beginText.x = beginText_x;
                            beginText.y = beginText_y;
                            endText.x   = endText_x;
                            endText.y   = endText_y;
                            pN = newP;
                        }
                    }
                    break;
                    case '\t': { SHADER::modelView._41 += ((curWidthLetter * 4) + (this->spaceXCharacter > 0.0f ? this->spaceXCharacter * 4.0f : 0.0f)) * this->scale.x;}
                    break;
                    default:
                    {
                        if (index == this->wildCardChangeAnim)
                        {
                            unsigned int indexNewAnim = 0xffffffff;
                            ANIMATION *newAnim = getNextIndexSpecialAnim(textDraw, s, &i,indexNewAnim);
                            if (newAnim)
                            {
                                anim = newAnim;
                                if (doRender && lsUpdateAnimation.find(indexNewAnim) == lsUpdateAnimation.end())
                                {
                                    lsUpdateAnimation.insert(indexNewAnim);
                                    anim->updateAnimation(this->device->delta, this, this->onEndAnimation,this->onEndFx);
                                }
                            }
                        }
                        else
                        {
                            util::DETAIL_LETTER *detail = infoFont->letter[index].detail;
                            if (detail) // existe esta letra nesta fonte?
                            {
                                BUFFER_MESH *frame = this->mesh->getBuffer(detail->indexFrame);
                                if (frame)
                                {
                                    curWidthLetter = static_cast<float>(detail->widthLetter) * 0.5f  * this->scale.x;
                                    if (i == 0)
                                        this->widthFirstLetter = curWidthLetter * 0.5f;
                                    SHADER::modelView._41 += curWidthLetter;
                                    #ifdef USE_EDITOR_FEATURES
                                    SHADER::modelView._41 += infoFont->letterDiffX[index];
                                    SHADER::modelView._42 += infoFont->letterDiffY[index];
                                    #endif
                                    if (this->is3D)
                                        MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView,
                                                       &this->device->camera.matrixPerspective);
                                    else
                                        MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView,
                                                       &this->device->camera.matrixPerspective2d);
                                    #ifdef USE_EDITOR_FEATURES
                                    SHADER::modelView._41 -= infoFont->letterDiffX[index];
                                    SHADER::modelView._42 -= infoFont->letterDiffY[index];
                                    #endif
                                    if (doRender)
                                    {
                                        anim->fx.shader.update(); // glUseProgram
                                        anim->fx.setBlendOp();
                                        if (anim->fx.textureOverrideStage2)
                                        {
                                            if (!this->mesh->render(detail->indexFrame, &anim->fx.shader,anim->fx.textureOverrideStage2->idTexture))
                                                return false;
                                        }
                                        else
                                        {
                                            if (!this->mesh->render(detail->indexFrame, &anim->fx.shader,0))
                                                return false;
                                        }
                                    }
                                }
                            }
                            SHADER::modelView._41 += curWidthLetter + this->spaceXCharacter;
                            if (SHADER::modelView._41 > endText.x)
                            {
                                endText.x = SHADER::modelView._41;
                                endText.x += curWidthLetter * 0.5f;
                            }
                        }
                    }
                    break;
                }
            }
            endText.y = SHADER::modelView._42;
            endText.y -= (static_cast<float>(infoFont->heightLetter)  * this->scale.y);
            return true;
        }
        return false;
    }
    
    bool TEXT_DRAW::render()
    {
        return this->renderText(true);
    }
    
    ANIMATION * TEXT_DRAW::getNextIndexSpecialAnim(const std::string &textDraw, const unsigned int s, unsigned int *curIndex,unsigned int & indexNewAnim)
    {
        std::string  nextAnim;
        unsigned int indexForChangelocal = (*(curIndex)) + 1;
        for (unsigned int i = indexForChangelocal, j = 0; i < s; i++, ++j)
        {
            auto index = static_cast<unsigned char>(textDraw[i]);
            switch (index)
            {
                case 194: // UTF8 - Without BOM
                {
                    if ((i + 1) < s)
                    {
                        i++;
                        index = static_cast<unsigned char>(textDraw[i]);
                        index = withoutBOM2Map(index, 194);
                    }
                }
                break;
                case 195: // UTF8 - Without BOM
                {
                    if ((i + 1) < s)
                    {
                        i++;
                        index = static_cast<unsigned char>(textDraw[i]);
                        index = withoutBOM2Map(index, 195);
                    }
                }
                break;
                default: {
                }
                break;
            }
            if (index == this->wildCardChangeAnim)
            {
                indexForChangelocal = i;
                break;
            }
            else
            {
                nextAnim.push_back(static_cast<char>(index));
            }
        }
        if (nextAnim.size())
        {
            const char * nextAnimstrNextAnim  = nextAnim.c_str();
            const auto indexAnim = static_cast<unsigned int>(std::atoi(nextAnimstrNextAnim)-1);
            *curIndex                    = indexForChangelocal;
            if (indexAnim < static_cast<unsigned int>(this->lsAnimation.size()))
            {
                mbm::ANIMATION* newAnim = this->lsAnimation[indexAnim];
                if(newAnim)
                    indexNewAnim = indexAnim;
                return newAnim;
            }
        }
        return nullptr;
    }

    const std::string TEXT_DRAW::getTextWithoutSpecialLetters()const
    {
        std::string ret;
        bool insideSpecialLetter = false;
        const unsigned int s = text.size();
        for (unsigned int i = 0; i < s; i++)
        {
            auto index = static_cast<unsigned char>(text[i]);
            switch (index)
            {
                case 194: // UTF8 - Without BOM
                {
                    if ((i + 1) < s)
                    {
                        i++;
                        index = static_cast<unsigned char>(text[i]);
                        index = withoutBOM2Map(index, 194);
                    }
                }
                break;
                case 195: // UTF8 - Without BOM
                {
                    if ((i + 1) < s)
                    {
                        i++;
                        index = static_cast<unsigned char>(text[i]);
                        index = withoutBOM2Map(index, 195);
                    }
                }
                break;
                default: {
                }
                break;
            }
            if (index == this->wildCardChangeAnim)
            {
                if(insideSpecialLetter)
                    insideSpecialLetter = false;
                else
                    insideSpecialLetter = true;
            }
            else if (!insideSpecialLetter)
            {
                ret.push_back(static_cast<char>(index));
            }
        }
        return ret;
    }
    
    bool TEXT_DRAW::onRestoreDevice()
    {
        const unsigned int oldIndexCurrentAnimation = this->indexCurrentAnimation;
        const bool         ret                      = this->onRestoreFont(this->parentFONT_DRAW, this);
        if(ret)
        {
            this->indexCurrentAnimation                 = oldIndexCurrentAnimation;
            this->lsAnimation[this->indexCurrentAnimation]->restartAnimation();
        }
        return ret;
    }
    
    void TEXT_DRAW::onStop()
    {
        this->releaseAnimation();
        this->mesh = nullptr;
        this->parentFONT_DRAW->onStop();
    }
    
    const mbm::INFO_PHYSICS * TEXT_DRAW::getInfoPhysics() const
    {
        if (this->mesh)
            return &this->mesh->infoPhysics;
        return nullptr;
    }
    
    const MESH_MBM * TEXT_DRAW::getMesh() const
    {
        return this->mesh;
    }

	FX*  TEXT_DRAW::getFx()const
	{
		auto * anim = getAnimation();
		if (anim)
			return &anim->fx;
		return nullptr;
	}

    bool TEXT_DRAW::setTexture(
        const MESH_MBM *mesh, // fixa textura para o estagio 0 e 1, mesh == nullptr e stage = 1 para textura de estagio 2
        const char *fileNametexture, const unsigned int stage, const bool hasAlpha)
    {
        mbm::ANIMATION *anim = this->getAnimation();
        if (anim)
        {
            TEXTURE *newTex = TEXTURE_MANAGER::getInstance()->load(fileNametexture, hasAlpha);
            if (newTex)
            {
                if (mesh)
                {
                    if (stage == 0)
                    {
                        //change to all letters
                        const unsigned int s = mesh->getTotalFrame();
                        for (unsigned int i = 0; i < s; ++i)
                        {
                            mbm::BUFFER_MESH *buff = mesh->getBuffer(static_cast<unsigned int>(i));
                            if (buff)
                            {
                                for (unsigned int j = 0; j < buff->totalSubset; ++j)
                                {
                                    util::SUBSET *subset           = &buff->subset[j];
                                    subset->texture                = newTex;
                                    buff->pBufferGL->idTexture0[j] = newTex->idTexture;
                                }
                            }
                        }
                    }
                    else
                    {
                        anim->fx.textureOverrideStage2 = newTex;
                        return true;
                    }
                }
                else if (stage)
                {
                    anim->fx.textureOverrideStage2 = newTex;
                    return true;
                }
            }
        }
        return false;
    }

	ANIMATION_MANAGER*  TEXT_DRAW::getAnimationManager()
	{
		return this;
	}
    
    bool TEXT_DRAW::isLoaded() const
    {
        return this->mesh != nullptr;
    }
    
    FONT_DRAW::FONT_DRAW(const SCENE *scene) : idScene(scene->getIdScene())
    {
        this->mesh = nullptr;
    }
    
    FONT_DRAW::~FONT_DRAW()
    {
        this->release();
    }
    
    void FONT_DRAW::release()
    {
        this->mesh = nullptr;
        for (auto text : this->lsText)
        {
            delete text;
        }
        this->lsText.clear();
    }
    
    TEXT_DRAW * FONT_DRAW::addText(const char *newText, VEC2 &position, const bool _is2dFont ,
                              const bool isScreen2d )
    {
        VEC3    pos(position.x, position.y, 1);
        auto text = new TEXT_DRAW(this->idScene, _is2dFont == false, isScreen2d, newText, pos, FONT_DRAW::OnRestoreFont, this);
        this->lsText.push_back(text);
        this->fillAnimation(text);
        text->restartAnimation();
        text->renderText(false);
        text->bounding_AABB.x = text->endText.x - text->beginText.x;
        text->bounding_AABB.y = text->beginText.y - text->endText.y;
        text->isOnFrustum();
        text->updateAABB();
        return text;
    }
    
    TEXT_DRAW * FONT_DRAW::addText(const char *newText, const bool _is2dFont , const bool isScreen2d )
    {
        auto text =
            new TEXT_DRAW(this->idScene, _is2dFont == false, isScreen2d, newText, FONT_DRAW::OnRestoreFont, this);
        this->lsText.push_back(text);
        this->fillAnimation(text);
        text->restartAnimation();
        text->renderText(false);
        text->bounding_AABB.x = text->endText.x - text->beginText.x;
        text->bounding_AABB.y = text->beginText.y - text->endText.y;
        text->isOnFrustum();
        text->updateAABB();
        return text;
    }
    
    TEXT_DRAW * FONT_DRAW::addText(const char *newText, VEC3 &position, const bool _is2dFont ,
                              const bool isScreen2d )
    {
        auto text = new TEXT_DRAW(this->idScene, _is2dFont == false, isScreen2d, newText, position,
                                        FONT_DRAW::OnRestoreFont, this);
        this->lsText.push_back(text);
        this->fillAnimation(text);
        text->restartAnimation();
        text->renderText(false);
        text->bounding_AABB.x = text->endText.x - text->beginText.x;
        text->bounding_AABB.y = text->beginText.y - text->endText.y;
        text->isOnFrustum();
        text->updateAABB();
        return text;
    }
    
    unsigned int FONT_DRAW::getTotalText() const
    {
        return static_cast<unsigned int>(this->lsText.size());
    }
    
    TEXT_DRAW * FONT_DRAW::getText(const unsigned char index)
    {
        if (index < this->lsText.size())
            return this->lsText[index];
        return nullptr;
    }
    
    bool FONT_DRAW::loadFont(const char *fileNameMbmOrTtf, const float heightLetter, const short spaceWidth,
                         const short spaceHeight,const bool saveTextureAsPng)
    {
        if (this->isLoaded())
            return true;
        std::vector<std::string> result;
        util::split(result, fileNameMbmOrTtf, '.');
        if (result.size() == 0)
        {
#if defined _DEBUG
            PRINT_IF_DEBUG("unknown [%s]", log_util::basename(fileNameMbmOrTtf));
#endif
            return false;
        }
        TEXTURE * texture_loaded = nullptr;
        const char* ext =  result[result.size() - 1].c_str();
        if (strcasecmp("mbm",ext) == 0 || strcasecmp("fnt",ext) == 0)
            this->mesh = MESH_MANAGER::getInstance()->load(fileNameMbmOrTtf);
        else if (strcasecmp("ttf", result[result.size() - 1].c_str()) == 0)
            this->mesh =
                MESH_MANAGER::getInstance()->loadTrueTypeFont(fileNameMbmOrTtf, heightLetter, spaceWidth, spaceHeight,saveTextureAsPng,&texture_loaded);
        if (this->mesh)
        {
            const util::TYPE_MESH type = this->mesh->getTypeMesh();
			const INFO_BOUND_FONT * infoFont = mesh->getInfoFont();
            if (type != util::TYPE_MESH_FONT || infoFont == nullptr)
            {
                this->mesh->release();
                this->mesh = nullptr;
                ERROR_LOG( "type of file is not font!\ntype: %s",MESH_MANAGER::typeClassName(type));
                return false;
            }
            this->fontName = infoFont->fontName;
            for (auto & i : this->lsText)
            {
                i->releaseAnimation();
                this->fillAnimation(i);
            }
            char strTemp[255]             = "";
            this->fileName = fileNameMbmOrTtf;
            sprintf(strTemp, "|%f|%d|%d", heightLetter, spaceWidth, spaceHeight);
            this->fileName += strTemp;
            #ifdef USE_EDITOR_FEATURES
            if(texture_loaded)
                texture_file_name_created = texture_loaded->getFileNameTexture();
            #endif
        }
        return this->mesh != nullptr;
    }
    
    const char * FONT_DRAW::getFileName() const
    {
        return this->fileName.c_str();
    }
    
    MESH_MBM * FONT_DRAW::getMesh()
    {
        return mesh;
    }
    void FONT_DRAW::onStop()
    {
        this->mesh = nullptr;
    }
    
    void FONT_DRAW::fillAnimation(TEXT_DRAW *text)
    {
        if (text == nullptr || this->mesh == nullptr)
            return;
        text->mesh            = this->mesh;
		const INFO_BOUND_FONT * infoFont = mesh->getInfoFont();
        text->spaceXCharacter = infoFont->spaceXCharacter;
        text->spaceYCharacter = infoFont->spaceYCharacter;
        // adicionamos as animações
        for (unsigned int i = 0; i < this->mesh->infoAnimation.lsHeaderAnim.size(); ++i)
        {
            util::INFO_ANIMATION::INFO_HEADER_ANIM *header = this->mesh->infoAnimation.lsHeaderAnim[i];
            if (!text->populateAnimationFromHeader(this->mesh, header->headerAnim, i))
            {
                this->release();
                ERROR_AT(__LINE__,__FILE__, "error on add animation!!");
            }
        }
        // carregamos a textura do estagio 2
        text->populateTextureStage2FromMesh(this->mesh);
    }
    
    bool FONT_DRAW::OnRestoreFont(FONT_DRAW *that, TEXT_DRAW *TEXT_DRAW_ptr)
    {
        return that->OnRestore(TEXT_DRAW_ptr);
    }
    
    bool FONT_DRAW::OnRestore(TEXT_DRAW *whatText)
    {
        if (this->lsText.size() == 0)
        {
            #if defined DEBUG_RESTORE
            PRINT_IF_DEBUG("there is no font to be loaded\n[%s]", log_util::basename(this->fileName.c_str()));
            #endif
        }
        else if(this->mesh == nullptr)
        {
            std::vector<std::string> lsRet;
            util::split(lsRet, this->fileName.c_str(), '|');
            if (lsRet.size() == 4)
            {
                const char *fileNameMbmOrTtf = lsRet[0].c_str();
                const auto heightLetter     = static_cast<const float>(atof(lsRet[1].c_str()));
                const auto spaceWidth       = static_cast<short>(std::atoi(lsRet[2].c_str()));
                const auto spaceHeight      = static_cast<short>(std::atoi(lsRet[3].c_str()));
                const bool ret = this->loadFont(fileNameMbmOrTtf, heightLetter, spaceWidth, spaceHeight,false);
                #if defined DEBUG_RESTORE
                if(ret)
				{
                    PRINT_IF_DEBUG("Font [%s] successfully restored", log_util::basename(fileNameMbmOrTtf));
				}
                else
				{
                    PRINT_IF_DEBUG("Failed to restore [%s]", log_util::basename(fileNameMbmOrTtf));
				}
                #endif
                return ret;
            }
            #if defined DEBUG_RESTORE
            PRINT_IF_DEBUG("Failed to restore font [%s]", log_util::basename(this->fileName.c_str()));
            #endif
        }
        else
        {
            whatText->mesh = this->mesh;
            return true;
        }
        return false;
    }
    
    bool FONT_DRAW::isLoaded() const
    {
        return this->mesh != nullptr;
    }

    #ifdef USE_EDITOR_FEATURES

    const char *FONT_DRAW::getFileNameTextureLoaded() const
    {
        return texture_file_name_created.c_str();
    }

    unsigned char FONT_DRAW::getIndexFromLetter(const char* letter)const
    {
        unsigned char index = 0;
        switch (static_cast<unsigned char>(letter[0]))
        {
            case 194: // UTF8 - Without BOM
            {
                if (letter[1])
                {
                    index = static_cast<unsigned char>(letter[1]);
                    index = TEXT_DRAW::withoutBOM2Map(index, 194);
                }
            }
            break;
            case 195: // UTF8 - Without BOM
            {
                if (letter[1])
                {
                    index = static_cast<unsigned char>(letter[1]);
                    index = TEXT_DRAW::withoutBOM2Map(index, 195);
                }
            }
            break;
            default: 
            {
                index = static_cast<unsigned char>(letter[0]);
            }
            break;
        }
        return index;
    }

    void FONT_DRAW::setLetterYDiff(const char* letter,const float diffY)
    {
		INFO_BOUND_FONT * infoFont = (mesh ? const_cast<INFO_BOUND_FONT *>(mesh->getInfoFont()) : nullptr);
        if(infoFont)
        {
            const unsigned char index = getIndexFromLetter(letter);
			infoFont->letterDiffY[index] = diffY;
        }
    }

    float FONT_DRAW::getLetterYDiff(const char * letter)const
    {
		const INFO_BOUND_FONT * infoFont = (mesh ? mesh->getInfoFont() : nullptr);
        if(infoFont)
        {
            const unsigned char index = getIndexFromLetter(letter);
            return infoFont->letterDiffY[index];
        }
        return 0.0f;
    }

    void FONT_DRAW::setLetterXDiff(const char* letter,const float diffX)
    {
		INFO_BOUND_FONT * infoFont = (mesh ? const_cast<INFO_BOUND_FONT *>(mesh->getInfoFont()) : nullptr);
		if(infoFont)
        {
            const unsigned char index = getIndexFromLetter(letter);
            infoFont->letterDiffX[index] = diffX;
        }
    }

    float FONT_DRAW::getLetterXDiff(const char * letter)const
    {
		const INFO_BOUND_FONT * infoFont = (mesh ? mesh->getInfoFont() : nullptr);
		if(infoFont)
        {
            const unsigned char index = getIndexFromLetter(letter);
            return infoFont->letterDiffX[index];
        }
        return 0.0f;
    }

    void  FONT_DRAW::setLetterSize(const char* letter,const unsigned int size_x,const unsigned int size_y)
    {
		INFO_BOUND_FONT * infoFont = (mesh ? const_cast<INFO_BOUND_FONT *>(mesh->getInfoFont()) : nullptr);
		if(infoFont)
        {
            const unsigned char index = getIndexFromLetter(letter);
            LETTER* l = & infoFont->letter[index];
            if(l->detail)
            {
                l->detail->widthLetter  = static_cast<unsigned short>(size_x);
                l->detail->heightLetter = static_cast<unsigned short>(size_y);
            }
        }
    }
    bool FONT_DRAW::getLetterSize(const char* letter,unsigned int & out_size_x,unsigned int & out_size_y)const
    {
		const INFO_BOUND_FONT * infoFont = (mesh ? mesh->getInfoFont() : nullptr);
		if(infoFont)
        {
            const unsigned char index = getIndexFromLetter(letter);
            const LETTER* l = & infoFont->letter[index];
            if(l->detail)
            {
                out_size_x = l->detail->widthLetter;
                out_size_y = l->detail->heightLetter;
                return true;
            }
        }
        return false;
    }

    #endif
    
}
