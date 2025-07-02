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

#include <HMD.h>
#include <util-interface.h>
#include <texture-manager.h>


namespace mbm
{

    HMD::HMD(const SCENE *scene) 
        : RENDER_2_TEXTURE(scene, true, true)

    {
    }
    
    HMD::~HMD()
    {
        this->texture = nullptr;
        this->lsObjects2dRender.clear();
        this->lsObjects3dRender.clear();
        this->fileName.clear();
        this->bufferGLRight.release();
        this->bufferGL.release();
    }
    
    bool HMD::load()
    {
        static int         num         = 0;
        const auto widthFrame  = static_cast<const unsigned int>(this->device->getScaleBackBufferWidth() * 0.5f);
        const auto heightFrame = static_cast<const unsigned int>(this->device->getScaleBackBufferHeight());
        const auto _widthTexture  = static_cast<const unsigned int>(this->device->getBackBufferWidth() * 0.5f);
        const auto _heightTexture = static_cast<const unsigned int>(this->device->getBackBufferHeight());
        char               nickName[255]  = "";
        const bool         hasAlpha       = false;
        sprintf(nickName, "texture_dynamic_%d", ++num);
        #if defined _WIN32
        const char *messageError =
            ""
            "if you create a texture with width > backBuffer or height > backBuffer will ocorrer a problem.\n"
            "The problem was inside present parameters\n"
            "dx_PresParams.Windowed = TRUE;\n"
            "dx_PresParams.SwapEffect = D3DSWAPEFFECT_DISCARD;\n"
            "dx_PresParams.BackBufferFormat = D3DFMT_UNKNOWN;\n"
            "dx_PresParams.BackBufferWidth = 512;//Width\n"
            "dx_PresParams.BackBufferHeight = 512;//Height\n"
            "dx_PresParams.EnableAutoDepthStencil = TRUE;\n"
            "dx_PresParams.AutoDepthStencilFormat = D3DFMT_D24S8;\n"
            "dx_PresParams.MultiSampleType = D3DMULTISAMPLE_8_SAMPLES;\n"
            "\n"
            "But to be exact, the real problem was inside the auto depth stencil that was a surface in resolution of "
            "512x512 \n"
            "and wasn't allow me to render scene in textures with proper size.\n";
        #endif
        if (this->texture == nullptr)
        {
            if (_widthTexture == 0 || _heightTexture == 0)
            {
                PRINT_IF_DEBUG("nickName == nullptr || widthTexture == 0 || heightTexture == 0");
                return false;
            }
            if (_widthTexture > this->device->getBackBufferWidth() ||
                _heightTexture > this->device->getBackBufferHeight())
            {
                #if defined _WIN32
                    PRINT_IF_DEBUG(messageError);
                #endif
                return false;
            }
            this->widthTexture  = _widthTexture;  // 400
            this->heightTexture = _heightTexture; // 600
            this->texture = TEXTURE_MANAGER::getInstance()->createTextureRenderTarget(this, nickName, hasAlpha);
            if (this->texture)
            {
                int                indexStart = 0;
                int                indexCount = 6;
                VEC3            _position[4];
                VEC3            normal[4];
                VEC2            uv[4];
                unsigned short int index[6] = {0, 2, 1, 2, 3, 1};
                this->fillvertexQuad(_position, normal, uv, static_cast<const float>(widthFrame), static_cast<const float>(heightFrame));
                if (this->bufferGL.loadBuffer(_position, normal, uv, 4, index, 1, &indexStart, &indexCount,nullptr))
                {
                    this->bufferGL.idTexture0[0] = this->texture->idTexture;
                    this->bufferGL.useAlpha[0]   = this->texture ? (this->texture->useAlphaChannel ? 1 : 0) : 0;
                }
                else
                {
                    return false;
                }

                if (!this->bufferGLRight.loadBuffer(_position, normal, uv, 4, index, 1, &indexStart, &indexCount,nullptr))
                {
                    this->bufferGLRight.idTexture0[0] = this->texture->idTexture;
                    this->bufferGLRight.useAlpha[0]   = this->texture ? (this->texture->useAlphaChannel ? 1 : 0) : 0;
                }
                else
                {
                    return false;
                }

                this->alwaysRenderize = true;
                if (!createAnimationAndShader2Render2Texture())
                    return false;
                char strTemp[300]="";
                snprintf(strTemp,sizeof(strTemp)-1, "rende2texture|%s|%u|%u|%u|%u|%s", nickName, widthFrame, heightFrame, widthTexture,
                        heightTexture, hasAlpha ? "true" : "false");

                this->fileName = strTemp;
            }
        }
        return (this->texture != nullptr);
    }

    bool HMD::isOnFrustum()
    {
        this->camera2d.position.x = this->device->camera.position2d.x;
        this->camera2d.position.y = this->device->camera.position2d.y;
        this->camera3d.position   = this->device->camera.position;
        this->camera3d.focus      = this->device->camera.focus;
        return RENDER_2_TEXTURE::isOnFrustum();
    }
    
    bool HMD::render()
    {
        if (this->alwaysRenderize)
        {
            this->camera2d.position.x = this->device->camera.position2d.x;
            this->camera2d.position.y = this->device->camera.position2d.y;
            this->camera3d.position   = this->device->camera.position;
            this->camera3d.focus      = this->device->camera.focus;
        }
        this->position.x = this->device->getScaleBackBufferWidth() * 0.25f;
        this->position.y = this->device->getScaleBackBufferHeight() * 0.5f;
        if (!this->renderVR(&this->bufferGL)) // left
            return false;
        this->position.x = this->device->getScaleBackBufferWidth() * 0.75f;
        if (!this->renderVR(&this->bufferGLRight)) // right
            return false;
        return true;
    }
    
    bool HMD::renderVR(BUFFER_GL *bufferSide)
    {
        if (bufferSide->isLoadedBuffer())
        {
            if (this->modeTextureOnly)
                return true;
            if (this->is3D)
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective);
            }
            else if (this->is2dS)
            {
                VEC3 positionScreen(this->position.x * this->device->camera.scaleScreen2d.x,
                                    this->position.y * this->device->camera.scaleScreen2d.y, this->position.z);
                this->device->transformeScreen2dToWorld2d_scaled(this->position.x, this->position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
            }
            else
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
            }
            ANIMATION *anim = this->getAnimation();
            if (anim)
            {
                this->blend.set(anim->blendState);
                anim->updateAnimation(this->device->delta, this, this->onEndAnimation, this->onEndFx);
                anim->fx.shader.update(); // glUseProgram
                anim->fx.setBlendOp();
                if (anim->fx.textureOverrideStage2)
                    bufferSide->idTexture1 = anim->fx.textureOverrideStage2->idTexture;
                if (!anim->fx.shader.render(bufferSide))
                    return false;
                return true;
            }
        }
        return false;
    }
    
    const INFO_PHYSICS * HMD::getInfoPhysics() const
    {
        return nullptr;
    }
    
};
