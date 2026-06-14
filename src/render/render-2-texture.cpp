/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <render-2-texture.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <util.h>
#include <lodepng/lodepng.h>
#include <algorithm>
#include <platform/mismatch-platform.h>
#include <core_mbm/scene.h>

namespace mbm
{

    CAMERA_TARGET::CAMERA_TARGET() noexcept : position(0, 0, 0), scale(1, 1, 1), angle(0, 0, 0), focus(0, 0, 0), up(0, 1, 0), zNear(0.1f), zFar(1000.0f)
    {
        MatrixIdentity(&this->matrixView);
        MatrixIdentity(&this->matrixOrtho);
    }
    
    void CAMERA_TARGET::enableMode2D(mbm::DEVICE *device, const float width, const float height)
    {
        //TODO: may need adjust this in the future
        // For 2d, we should not use near 0.1 , if we use the objects bellow that will be hidden
        constexpr float zNear2d = -100;
        constexpr float zFar2d = 100;
        const VEC3 posCam(-this->position.x, -this->position.y, 100);
        CAMERA &camera = device->getCamera();
        MatrixIdentity(&this->matrixView);
        MatrixTranslationRotationScale(&SHADER::modelView, &posCam, &this->angle, &this->scale);
        MatrixOrthoLH(&this->matrixOrtho, width, height, zNear2d, zFar2d);
        MatrixMultiply(&camera.matrixPerspective2d, &this->matrixView, &this->matrixOrtho);
    }
    
    void CAMERA_TARGET::enableMode3D(mbm::DEVICE *device, const float width, const float height)
    {
        CAMERA &camera = device->getCamera();
        const float aspect = width / height;
        const auto Scale  = static_cast<const float>(1.0f / tan(camera.angleOfView * 0.5f * static_cast<const float>(M_PI) / 180.0f));
        MatrixPerspectiveFovLH(&this->matrixProj, Scale, aspect, zNear, zFar);
        MatrixLookAtLH(&this->matrixView, &this->position, &this->focus, &this->up);
        MatrixMultiply(&camera.matrixPerspective, &this->matrixView, &this->matrixProj);
    }
    
    RENDER_2_TEXTURE::RENDER_2_TEXTURE(const SCENE *scene, const bool _is3d, const bool _is2dScreen) :
        RENDERIZABLE_TO_TARGET(scene, TYPE_CLASS_RENDER_2_TEX, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->modeTextureOnly = false;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->addRenderizable(this);
        device->addObjectRender2Texture(this);
        this->setRender2Texture(false);
        this->texture          = nullptr;
    }
    
    RENDER_2_TEXTURE::~RENDER_2_TEXTURE()
    {
        // Evict the render-target texture from the cache and free its GL object before
        // the base-class destructor deletes the FBO/renderbuffer in backend-specific config.
        this->release();
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->removeObjectRender2Texture(this);
        device->removeRenderizable(this);
    }
    
    void RENDER_2_TEXTURE::removeFromRender2Texture(RENDERIZABLE *ptr)
    {
        if (ptr)
        {
            for (unsigned int i = 0; i < this->lsObjects2dRender.size(); ++i)
            {
                RENDERIZABLE *other = this->lsObjects2dRender[i];
                if (ptr == other)
                {
                    this->lsObjects2dRender.erase(this->lsObjects2dRender.begin() + i);
                    break;
                }
            }

            for (unsigned int i = 0; i < this->lsObjects3dRender.size(); ++i)
            {
                RENDERIZABLE *other = this->lsObjects3dRender[i];
                if (ptr == other)
                {
                    this->lsObjects3dRender.erase(this->lsObjects3dRender.begin() + i);
                    break;
                }
            }
        }
    }
    
    void RENDER_2_TEXTURE::release()
    {
        // Evict from TEXTURE_MANAGER cache so the next load() with the same nickname
        // creates a fresh FBO rather than returning this now-dead texture.
        if (this->texture != nullptr)
        {
            TEXTURE_MANAGER::getInstance()->releaseRenderTarget(this->texture->getFileNameTexture());
            this->texture = nullptr;
        }
        this->clear();
        this->clearInternalFileName();
        this->bufferGL.release();
    }
    
    TEXTURE* RENDER_2_TEXTURE::load(const uint32_t widthFrame, const uint32_t heightFrame, const uint32_t _widthTexture,const uint32_t _heightTexture, const char *nickName, const bool hasAlpha)
    {
        #if defined _WIN32
            const char *messageError =
            ""
            "if you create a texture with width > backBuffer or height > backBuffer will occur a problem.\n"
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
            if (nickName == nullptr || _widthTexture == 0 || _heightTexture == 0)
            {
                PRINT_IF_DEBUG("nickName == nullptr || widthTexture == 0 || heightTexture == 0");
                return nullptr;
            }
            this->widthTexture  = _widthTexture;
            this->heightTexture = _heightTexture;
            this->texture = mbm::TEXTURE_MANAGER::getInstance()->createTextureRenderTarget(this, nickName, hasAlpha);
            if (this->texture)
            {
                int             indexStart = 0;
                int             indexCount = 6;
                VEC3            _position[4];
                VEC2            uv[4];
                unsigned short int index[6] = {0, 1, 2, 2, 1, 3};
                this->fillvertexQuad(_position, nullptr, uv, static_cast<const float>(widthFrame), static_cast<const float>(heightFrame));
                if (this->bufferGL.loadBuffer(_position, nullptr, uv, 4, index, 1, &indexStart, &indexCount,nullptr))
                {
                    this->bufferGL.setTextureByStage(this->texture, 0, 0 );
                }
                else
                {
                    this->texture = nullptr;
                    return this->texture;
                }
                if (!createAnimationAndShader2Render2Texture())
                {
                    this->texture = nullptr;
                    return this->texture;
                }
                char strTemp[255];
                snprintf(strTemp,sizeof(strTemp) -1, "rende2texture|%s|%u|%u|%u|%u|%s", 
                    nickName, 
                    widthFrame, 
                    heightFrame, 
                    widthTexture,
                    heightTexture, 
                    hasAlpha ? "true" : "false");
                mbm::CUBE *cube = nullptr;
                if (this->infoPhysics.lsCube.size())
                    cube = this->infoPhysics.lsCube[0];
                else
                {
                    cube = new mbm::CUBE();
                    this->infoPhysics.lsCube.push_back(cube);
                }
                cube->halfDim.x               = widthFrame * 0.5f;
                cube->halfDim.y               = heightFrame * 0.5f;
                cube->halfDim.z               = 1;
                this->setInternalFileName(strTemp);
                this->updateAABB();
            }
        }
        return this->texture;
    }
    
    void RENDER_2_TEXTURE::flip_vertically(unsigned char *pixels, const int width, const int height, const int bytes_per_pixel)
    {
        const int stride = width * bytes_per_pixel;
        auto row = new unsigned char[stride];
        unsigned char *low = pixels;
        unsigned char *high = &pixels[(height - 1) * stride];

        for (; low < high; low += stride, high -= stride) 
        {
            memcpy(row, low,  static_cast<size_t>(stride));
            memcpy(low, high, static_cast<size_t>(stride));
            memcpy(high, row, static_cast<size_t>(stride));
        }
        delete [] row;
    }
    
    void RENDER_2_TEXTURE::clear()
    {
        for (unsigned int i = 0; i < this->lsObjects3dRender.size(); ++i)
        {
            RENDERIZABLE *ptr = lsObjects3dRender[i];
            ptr->setRender2Texture(false);
        }
        this->lsObjects3dRender.clear();
        for (unsigned int i = 0; i < this->lsObjects2dRender.size(); ++i)
        {
            RENDERIZABLE *ptr = lsObjects2dRender[i];
            ptr->setRender2Texture(false);
        }
        this->lsObjects2dRender.clear();
    }

    bool RENDER_2_TEXTURE::removeObject2Render(RENDERIZABLE *ptr)
    {
        if (ptr == nullptr)
            return false;
        // isRender2Texture is shared state across render targets; membership must be checked against this target's list.
        if (ptr->is3DObject())
        {
            for (unsigned int i = 0; i < this->lsObjects3dRender.size(); ++i)
            {
                RENDERIZABLE *other = lsObjects3dRender[i];
                if (ptr == other)
                {
                    ptr->setRender2Texture(false);
                    lsObjects3dRender.erase(lsObjects3dRender.begin() + i);
                    return true;
                }
            }
        }
        else
        {
            for (unsigned int i = 0; i < this->lsObjects2dRender.size(); ++i)
            {
                RENDERIZABLE *other = lsObjects2dRender[i];
                if (ptr == other)
                {
                    ptr->setRender2Texture(false);
                    lsObjects2dRender.erase(lsObjects2dRender.begin() + i);
                    return true;
                }
            }
        }
        return false;
    }
    
    bool RENDER_2_TEXTURE::addObject2Render(RENDERIZABLE *ptr)
    {
        if (ptr == nullptr)
            return false;
        // isRender2Texture is shared state across render targets; membership must be checked against this target's list.
        if (ptr->is3DObject())
        {
            for (unsigned int i = 0; i < this->lsObjects3dRender.size(); ++i)
            {
                RENDERIZABLE *other = lsObjects3dRender[i];
                if (ptr == other)
                    return true;
            }
            lsObjects3dRender.push_back(ptr);
            ptr->setRender2Texture(true);
        }
        else
        {
            for (unsigned int i = 0; i < this->lsObjects2dRender.size(); ++i)
            {
                RENDERIZABLE *other = lsObjects2dRender[i];
                if (ptr == other)
                    return true;
            }
            lsObjects2dRender.push_back(ptr);
            ptr->setRender2Texture(true);
        }
        return true;
    }
    
    bool RENDER_2_TEXTURE::render() // Renderiza a textura
    {
        if (this->bufferGL.isLoadedBuffer())
        {
            if (this->modeTextureOnly)
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
            mbm::ANIMATION *anim = this->getAnimation();
            if (anim)
            {
                FX &fx = anim->getFx();
                this->setBlendState(anim->getBlendState());
                anim->updateAnimation(device->delta, this, this->getOnEndAnimation(),this->getOnEndFx());
                fx.shader.update(); // glUseProgram
                fx.setBlendOp();
                if (fx.textureOverrideStage2)
                    this->bufferGL.setTextureByStage(fx.textureOverrideStage2, 1, 0);
                if (!fx.shader.render(&this->bufferGL))
                    return false;
                return true;
            }
        }
        return false;
    }
    
    bool RENDER_2_TEXTURE::render2Texture()
    {
        if (this->lsObjects3dRender.size())
        {
            mbm::DEVICE* device     = mbm::DEVICE::getInstance();
            const CUBE *cube        = this->infoPhysics.lsCube[0];
            const float widthFrame  = cube->halfDim.x * 2.0f;
            const float heightFrame = cube->halfDim.y * 2.0f;
            this->camera3d.enableMode3D(device, widthFrame, heightFrame);
            for (unsigned int i = 0; i < this->lsObjects3dRender.size(); ++i)
            {
                RENDERIZABLE *ptr = lsObjects3dRender[i];
                const VEC3 distFromCam(ptr->getPosition() - this->camera3d.position);
                ptr->setDistanceFromView(distFromCam.length());
            }
            std::sort(lsObjects3dRender.begin(), lsObjects3dRender.end(),
                      [](const RENDERIZABLE *a, const RENDERIZABLE *b) { return b->getDistanceFromView() < a->getDistanceFromView(); });
            for (unsigned int i = 0; i < this->lsObjects3dRender.size(); ++i)
            {
                RENDERIZABLE *ptr = lsObjects3dRender[i];
                if (ptr->isRenderEnabled())
                {
                    const bool oldAlwaysRender = ptr->isAlwaysRenderizeEnabled();
                    ptr->setAlwaysRenderize(false); // for not animate twice
                    const bool ret             = ptr->render();
                    ptr->setAlwaysRenderize(oldAlwaysRender);
                    if (!ret)
                        return false;
                }
            }
        }
        if (this->lsObjects2dRender.size())
        {
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
            this->camera2d.enableMode2D(device, static_cast<float>(this->texture->getWidth()), static_cast<float>(this->texture->getHeight()));
            for (unsigned int i = 0; i < this->lsObjects2dRender.size(); ++i)
            {
                RENDERIZABLE *ptr   = lsObjects2dRender[i];
                ptr->setDistanceFromView(ptr->getPosition().z);
            }
            std::sort(lsObjects2dRender.begin(), lsObjects2dRender.end(),
                      [](const RENDERIZABLE *a, const RENDERIZABLE *b) { return b->getDistanceFromView() < a->getDistanceFromView(); });
            for (unsigned int i = 0; i < this->lsObjects2dRender.size(); ++i)
            {
                RENDERIZABLE *ptr = lsObjects2dRender[i];
                if (ptr->isRenderEnabled())
                {
                    const bool oldAlwaysRender = ptr->isAlwaysRenderizeEnabled();
                    ptr->setAlwaysRenderize(false); // to not animate twice
                    const bool ret             = ptr->render();
                    ptr->setAlwaysRenderize(oldAlwaysRender);
                    if (!ret)
                        return false;
                }
            }
        }
        return true;
    }
    
    bool RENDER_2_TEXTURE::isOnFrustum()
    {
        if (this->bufferGL.isLoadedBuffer())
        {
            if (this->isRender2TextureEnabled())
                return false;
            IS_ON_FRUSTUM verify(this);
            bool ret = verify.isOnFrustum(this->is3DObject(), this->is2dScreenObject());
            if(ret == false)
            {
                ANIMATION *anim = this->getAnimation();
                mbm::DEVICE* device = mbm::DEVICE::getInstance();
                anim->updateAnimation(device->delta, this, this->getOnEndAnimation(), this->getOnEndFx());
            }
            return ret;
        }
        return false;
    }
    
    bool RENDER_2_TEXTURE::onRestoreDevice()
    {
        std::vector<std::string> result;
        this->texture = nullptr;
        this->bufferGL.release();
        util::split(result, this->getInternalFileName(), '|');
        if (result.size() != 7)
            return false;
        if (result[0].compare("rende2texture") == 0)
        {
            const char *fileNameTexture = result[1].c_str();
            if (result[1].size() == 0)
                return false;
            const auto width  = static_cast<const unsigned int>(std::atoi(result[4].c_str()));
            const auto height = static_cast<const unsigned int>(std::atoi(result[5].c_str()));
            bool     hasAlpha = result[6].compare("true") == 0 ? true : false;
            float widthFrame  = 0;
            float heightFrame = 0;
            this->infoPhysics.getBounds(&widthFrame, &heightFrame);
            if (this->load(static_cast<const unsigned int>(widthFrame), static_cast<const unsigned int>(heightFrame), width, height, fileNameTexture, hasAlpha) == nullptr)
                return false;
#if defined DEBUG
            PRINT_INFO_IF_DEBUG("rende2texture [%s] successfully restored", log_util::basename(fileNameTexture));
#endif
            return true;
        }
        #if defined DEBUG
        PRINT_IF_DEBUG("Failed to restore rende2texture [%s]",log_util::basename(this->getInternalFileName()));
        #endif
        return false;
    }
    
    void RENDER_2_TEXTURE::fillvertexQuad(VEC3 *_position, VEC3 *normal, VEC2 *uv, const float width, const float height)
    {
        // OpenGL ES: FBO row-0 at bottom → uvOriginBottomLeft=false (V=0 samples bottom).
        // DirectX9 / Metal: texture row-0 at top → uvOriginBottomLeft=true (V=0 samples top).
        // Without the flip the captured content appears upside-down on DX9/Metal.
        #if defined (USE_OPENGL_ES)
            mbm::fillVertexQuadTexture(_position, uv, width, height, normal, false);
        #elif defined(USE_DIRECTX9) || defined(USE_METAL)
            mbm::fillVertexQuadTexture(_position, uv, width, height, normal, true);
        #elif defined(USE_DUMMY_BACK_END_ENGINE) // In the dummy backend, we don't have a real texture, so we can choose either way. We choose false to avoid confusion when debugging, but it doesn't matter.
            //just to be able to compile the dummy backend, but this function is not used in this backend, so the flip parameter is not relevant
            mbm::fillVertexQuadTexture(_position, uv, width, height, normal, false);
        #else
            #error "Unknown graphics API (You must define new graphics API or adjust the existing ones in render-2-texture.cpp)"
        #endif
    }
    
    FVF_PROVIDE_BY_ENGINE RENDER_2_TEXTURE::getFvfFromBuffer() const noexcept
    {
        return bufferGL.isLoadedBuffer() ? bufferGL.fvf : FVF_PROVIDE_BY_ENGINE::FVF_NONE;
    }

    bool RENDER_2_TEXTURE::createAnimationAndShader2Render2Texture()
    {
        this->releaseAnimation();
        auto anim = new mbm::ANIMATION();
        this->appendAnimation(anim);
        FX &fx = anim->getFx();
        if (!fx.shader.compileShader(fx.fxPS->getCurrentShader(), fx.fxVS->getCurrentShader(), getFvfFromBuffer()))
            return false;
        return true;
    }
    
    const mbm::INFO_PHYSICS * RENDER_2_TEXTURE::getInfoPhysics() const
    {
        return &this->infoPhysics;
    }
    
    const MESH_MBM * RENDER_2_TEXTURE::getMesh() const
    {
        return nullptr;
    }

    FX*  RENDER_2_TEXTURE::getFx()const
    {
        auto * anim = getAnimation();
        if (anim)
            return &anim->getFx();
        return nullptr;
    }

    ANIMATION_MANAGER*  RENDER_2_TEXTURE::getAnimationManager()
    {
        return this;
    }
    
    bool RENDER_2_TEXTURE::isLoaded() const
    {
        return this->bufferGL.isLoadedBuffer();
    }
    
};
