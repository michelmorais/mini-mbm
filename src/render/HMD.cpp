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
#include <core_mbm/scene.h>


namespace mbm
{
    struct HMD::Impl
    {
        BUFFER_GL rightEyeBuffer;
    };

    HMD::HMD(const SCENE *scene) 
        : RENDER_2_TEXTURE(scene, true, true)
        , impl(new Impl())
    {
    }
    
    HMD::~HMD()
    {
        this->setRenderTargetTexture(nullptr);
        this->clearRenderObjectLists();
        this->clearInternalFileName();
        BUFFER_GL &rightEyeBuffer = this->getRightEyeBuffer();
        rightEyeBuffer.release();
        this->getRenderTargetBuffer().release();
    }
    
    bool HMD::load()
    {
        static int         num     = 0;
        mbm::DEVICE* device        = mbm::DEVICE::getInstance();
        const auto widthFrame      = static_cast<const unsigned int>(device->getScaleBackBufferWidth() * 0.5f);
        const auto heightFrame     = static_cast<const unsigned int>(device->getScaleBackBufferHeight());
        const auto _widthTexture   = static_cast<const unsigned int>(device->getBackBufferWidth() * 0.5f);
        const auto _heightTexture  = static_cast<const unsigned int>(device->getBackBufferHeight());
        char               nickName[255]  = "";
        const bool         hasAlpha       = false;
        snprintf(nickName, sizeof(nickName), "texture_dynamic_%d", ++num);
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
        TEXTURE *renderTargetTexture = this->getRenderTargetTexture();
        if (renderTargetTexture == nullptr)
        {
            if (_widthTexture == 0 || _heightTexture == 0)
            {
                PRINT_IF_DEBUG("nickName == nullptr || widthTexture == 0 || heightTexture == 0");
                return false;
            }
            if (_widthTexture > device->getBackBufferWidth() ||
                _heightTexture > device->getBackBufferHeight())
            {
                #if defined _WIN32
                    PRINT_IF_DEBUG(messageError);
                #endif
                return false;
            }
            this->setRenderTargetSize(_widthTexture, _heightTexture);  // 400x600 default
            renderTargetTexture = TEXTURE_MANAGER::getInstance()->createTextureRenderTarget(this, nickName, hasAlpha);
            this->setRenderTargetTexture(renderTargetTexture);
            if (renderTargetTexture)
            {
                int                indexStart = 0;
                int                indexCount = 6;
                VEC3            _position[4];
                VEC3*            normal = nullptr;
                VEC2            uv[4];
                unsigned short int index[6] = {0, 2, 1, 2, 3, 1};
                this->fillvertexQuad(_position, normal, uv, static_cast<const float>(widthFrame), static_cast<const float>(heightFrame));
                BUFFER_GL &renderTargetBuffer = this->getRenderTargetBuffer();
                if (renderTargetBuffer.loadBuffer(_position, normal, uv, 4, index, 1, &indexStart, &indexCount,nullptr))
                {
                    renderTargetBuffer.setTextureByStage(renderTargetTexture, 0, 0);
                }
                else
                {
                    return false;
                }

                BUFFER_GL &rightEyeBuffer = this->getRightEyeBuffer();
                if (rightEyeBuffer.loadBuffer(_position, normal, uv, 4, index, 1, &indexStart, &indexCount,nullptr))
                {
                    rightEyeBuffer.setTextureByStage(renderTargetTexture, 0, 0);
                }
                else
                {
                    return false;
                }

                this->setAlwaysRenderize(true);
                if (!createAnimationAndShader2Render2Texture())
                    return false;
                char strTemp[300]="";
                snprintf(strTemp,sizeof(strTemp)-1, "rende2texture|%s|%u|%u|%u|%u|%s", nickName, widthFrame, heightFrame,
                        this->getRenderTargetWidth(), this->getRenderTargetHeight(), hasAlpha ? "true" : "false");

                this->setInternalFileName(strTemp);
            }
        }
        return (this->getRenderTargetTexture() != nullptr);
    }

    bool HMD::isOnFrustum()
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        const CAMERA &camera = device->getCamera();
        CAMERA_TARGET &camera2dTarget = this->getCamera2d();
        CAMERA_TARGET &camera3dTarget = this->getCamera3d();
        VEC3 &camera2dPosition = camera2dTarget.getPosition();
        camera2dPosition.x = camera.position2d.x;
        camera2dPosition.y = camera.position2d.y;
        camera3dTarget.getPosition() = camera.position;
        camera3dTarget.getFocus() = camera.focus;
        return RENDER_2_TEXTURE::isOnFrustum();
    }
    
    bool HMD::render()
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        if (this->isAlwaysRenderizeEnabled())
        {
            const CAMERA &camera = device->getCamera();
            CAMERA_TARGET &camera2dTarget = this->getCamera2d();
            CAMERA_TARGET &camera3dTarget = this->getCamera3d();
            VEC3 &camera2dPosition = camera2dTarget.getPosition();
            camera2dPosition.x = camera.position2d.x;
            camera2dPosition.y = camera.position2d.y;
            camera3dTarget.getPosition() = camera.position;
            camera3dTarget.getFocus() = camera.focus;
        }
        VEC3 &position = this->getPosition();
        position.x = device->getScaleBackBufferWidth() * 0.25f;
        position.y = device->getScaleBackBufferHeight() * 0.5f;
        if (!this->renderVR(&this->getRenderTargetBuffer())) // left
            return false;
        position.x = device->getScaleBackBufferWidth() * 0.75f;
        BUFFER_GL &rightEyeBuffer = this->getRightEyeBuffer();
        if (!this->renderVR(&rightEyeBuffer)) // right
            return false;
        return true;
    }
    
    bool HMD::renderVR(BUFFER_GL *bufferSide)
    {
        if (bufferSide->isLoadedBuffer())
        {
            if (this->isTextureOnlyModeEnabled())
                return true;
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
            const CAMERA &camera = device->getCamera();
            const VEC3 &position = this->getPosition();
            const VEC3 &angle = this->getAngle();
            const VEC3 &scale = this->getScale();
            if (this->is3DObject())
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &position, &angle, &scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &camera.matrixPerspective);
            }
            else if (this->is2dScreenObject())
            {
                VEC3 positionScreen(position.x * camera.scaleScreen2d.x,
                                    position.y * camera.scaleScreen2d.y, position.z);
                device->transformeScreen2dToWorld2d_scaled(position.x, position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &angle, &scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &camera.matrixPerspective2d);
            }
            else
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &position, &angle, &scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &camera.matrixPerspective2d);
            }
            ANIMATION *anim = this->getAnimation();
            if (anim)
            {
                FX &fx = anim->getFx();
                this->setBlendState(anim->getBlendState());
                anim->updateAnimation(device->delta, this, this->getOnEndAnimation(), this->getOnEndFx());
                fx.shader.update(); // glUseProgram
                fx.setBlendOp();
                if (fx.textureAnimationEffect)
                    bufferSide->setTextureByStage(fx.textureAnimationEffect, 1, 0);
                if (!fx.shader.render(bufferSide))
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

    BUFFER_GL & HMD::getRightEyeBuffer() noexcept
    {
        return this->impl->rightEyeBuffer;
    }

    const BUFFER_GL & HMD::getRightEyeBuffer() const noexcept
    {
        return this->impl->rightEyeBuffer;
    }
    
};
