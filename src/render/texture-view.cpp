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

#include <texture-view.h>
#include <texture-manager.h>
#include <image-resource.h>
#include <util.h>
#include <util-interface.h>
#include <core_mbm/scene.h>

#if (defined _DEBUG || defined DEBUG)
    #include <log-util.h>
#endif

namespace mbm
{
    struct TEXTURE_VIEW::Impl
    {
        INFO_PHYSICS infoPhysics;
        TEXTURE *texture;
        BUFFER_GL bufferGL;

        Impl() noexcept : texture(nullptr)
        {
        }
    };

    TEXTURE_VIEW::TEXTURE_VIEW(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_TEXTURE, _is3d && _is2dScreen == false, _is2dScreen),
          impl(std::make_unique<Impl>())
    {
        this->setEnableRender(true);
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->addRenderizable(this);
    }

    TEXTURE_VIEW::TEXTURE_VIEW(const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(0, TYPE_CLASS_TEXTURE, _is3d && _is2dScreen == false, _is2dScreen),
          impl(std::make_unique<Impl>())
    {
        this->setEnableRender(true);
        //no scene - just restore texture
    }
    
    TEXTURE_VIEW::~TEXTURE_VIEW()
    {
        this->setEnableRender(false);
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->removeRenderizable(this);
        this->release();
    }
    
    void TEXTURE_VIEW::release()
    {
        this->impl->texture = nullptr;
        this->impl->bufferGL.release();
    }
    
    bool TEXTURE_VIEW::createAnimationAndShader2Texture()
    {
        this->releaseAnimation();
        auto anim = new mbm::ANIMATION();
        this->appendAnimation(anim);
        FX &fx = anim->getFx();
        fx.defaultShaderMode = getDefaultShaderModeForRenderizable(this);
        fx.shader.setUseReservedLightDefault(fx.defaultShaderMode == DEFAULT_SHADER_MODE_LIT);
        if (!fx.shader.compileShader(fx.fxPS->getCurrentShader(), fx.fxVS->getCurrentShader(), getFvfFromBuffer()))
            return false;
        return true;
    }
    
    bool TEXTURE_VIEW::load(const IMAGE_RESOURCE *image)
    {
        if (this->impl->texture)
            return true;
        if (image == nullptr)
            return false;

        TEXTURE_MANAGER *texMan = TEXTURE_MANAGER::getInstance();
        this->impl->texture      = texMan->load(image);
        if (this->impl->texture == nullptr)
            return false;
        if (!this->setFrame(static_cast<float>(image->width), static_cast<float>(image->height)))
            return false;
        if (!createAnimationAndShader2Texture())
            return false;
        this->impl->bufferGL.setTextureByStage(this->impl->texture, 0, 0);
        char strTemp[255];
        snprintf(strTemp,sizeof(strTemp), "texture|%s|%u|%u|%d", image->nickName, image->width, image->height,this->impl->texture->hasAlphaChannel() ? 1 : 0);
        this->setInternalFileName(strTemp);
        this->updateAABB();
        return true;
    }
    
    bool TEXTURE_VIEW::load(const char *fileNameTexture, const float w , const float h , const bool alpha )
    {
        if (this->impl->texture)
            return true;
        if (fileNameTexture == nullptr)
            return false;
        this->impl->texture = TEXTURE_MANAGER::getInstance()->load(fileNameTexture, alpha);
        if (this->impl->texture == nullptr)
            return false;
        const bool idFrame = this->setFrame(w <= 0.0f ? this->impl->texture->getWidth() : w, h <= 0.0f ? this->impl->texture->getHeight() : h);
        if (idFrame == false)
            return false;
        if (!createAnimationAndShader2Texture())
            return false;
        this->impl->bufferGL.setTextureByStage(this->impl->texture, 0, 0);
        const int useAlpha   = this->impl->texture ? (this->impl->texture->hasAlphaChannel() ? 1 : 0) : 0;
        char strTemp[255];
        const std::string baseFileName = util::getBaseName(fileNameTexture);
        snprintf(strTemp, sizeof(strTemp), "texture|%s|%f|%f|%d",baseFileName.c_str() , w, h, useAlpha);
        this->setInternalFileName(strTemp);
        this->updateAABB();
        return true;
    }
    
    bool TEXTURE_VIEW::setFrame(const float diameter)
    {
        int                indexStart = 0;
        int                indexCount = 6;
        VEC3            _position[4];
        VEC2            uv[4];
        unsigned short int index[6]      = {0, 1, 2, 2, 1, 3};
        TEXTURE * idTexture0 = this->impl->bufferGL.getTextureByStage(0, 0);
        TEXTURE * idTexture1 = this->impl->bufferGL.getTextureByStage(1, 0);
        this->impl->bufferGL.release();
        mbm::fillVertexQuadTexture(_position, uv, diameter <= 0.0f ? 100.0f : diameter,
                                   diameter <= 0.0f ? 100.0f : diameter);
        const bool ret = this->impl->bufferGL.loadBuffer(_position, nullptr, uv, 4, index, 1, &indexStart, &indexCount,nullptr);
        if (ret)
        {
            this->impl->bufferGL.setTextureByStage(idTexture0, 0, 0);
            this->impl->bufferGL.setTextureByStage(idTexture1, 1, 0);
        }
        else
            return false;
        mbm::CUBE *cube = nullptr;
        if (this->impl->infoPhysics.lsCube.size())
            cube = this->impl->infoPhysics.lsCube[0];
        else
        {
            cube = new mbm::CUBE();
            this->impl->infoPhysics.lsCube.push_back(cube);
        }
        cube->halfDim.x = diameter * 0.5f;
        cube->halfDim.y = diameter * 0.5f;
        this->updateRestoreTexture(diameter, diameter);
        return true;
    }
    
    bool TEXTURE_VIEW::setFrame(const float width, const float height)
    {
        int                indexStart = 0;
        int                indexCount = 6;
        VEC3            _position[4];
        VEC2            uv[4];
        unsigned short int index[6]      = {0, 1, 2, 2, 1, 3};
        TEXTURE * idTexture0 = this->impl->bufferGL.getTextureByStage(0, 0);
        TEXTURE * idTexture1 = this->impl->bufferGL.getTextureByStage(1, 0);
        mbm::fillVertexQuadTexture(_position, uv, width <= 0.0f ? 100.0f : width,
                                   height <= 0.0f ? 100.0f : height);
        const bool ret = this->impl->bufferGL.loadBuffer(_position, nullptr, uv, 4, index, 1, &indexStart, &indexCount,nullptr);
        if (ret)
        {
            this->impl->bufferGL.setTextureByStage(idTexture0, 0, 0);
            this->impl->bufferGL.setTextureByStage(idTexture1, 1, 0);
        }
        else
            return false;
        mbm::CUBE *cube = nullptr;
        if (this->impl->infoPhysics.lsCube.size())
            cube = this->impl->infoPhysics.lsCube[0];
        else
        {
            cube = new mbm::CUBE();
            this->impl->infoPhysics.lsCube.push_back(cube);
        }
        cube->halfDim.x = width * 0.5f;
        cube->halfDim.y = height * 0.5f;
        this->updateRestoreTexture(width, height);
        return true;
    }
    
    BUFFER_GL * TEXTURE_VIEW::getFrame()
    {
        return &this->impl->bufferGL;
    }
    
    TEXTURE * TEXTURE_VIEW::getTexture() const
    {
        return this->impl->texture;
    }
    
    bool TEXTURE_VIEW::setTexture(
        const MESH_MBM *mesh, // fixa textura para o estagio 0 e 1, mesh == nullptr e stage = 1 para textura de estagio 2
        const char *fileNametexture, const unsigned int stage, const bool hasAlpha)
    {
        if (stage == 0)
        {
            mbm::TEXTURE *newTex = mbm::TEXTURE_MANAGER::getInstance()->load(fileNametexture, hasAlpha);
            if (newTex)
            {
                this->impl->texture          = newTex;
                this->impl->bufferGL.setTextureByStage(newTex, 0, 0);
                return true;
            }
        }
        else
        {
            return ANIMATION_MANAGER::setTexture(mesh, fileNametexture, stage, hasAlpha);//TODO check this
        }
        return false;
    }
    
    void TEXTURE_VIEW::setTextureToNull()
    {
        this->impl->texture = nullptr;
    }

    std::string TEXTURE_VIEW::getFileNameTexture()const
    {
        std::string ret;
        if(this->impl->texture)
            ret = this->impl->texture->getFileNameTexture();
        return ret;
    }
    
    bool TEXTURE_VIEW::isOnFrustum()
    {
        if (this->impl->bufferGL.isLoadedBuffer())
        {
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
    
    bool TEXTURE_VIEW::render()
    {
        if (this->impl->bufferGL.isLoadedBuffer())
        {
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
            const CAMERA &camera = device->getCamera();
            ANIMATION *anim = this->getAnimation();
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
                VEC3 positionScreen(position.x, position.y, position.z);
                device->transformeScreen2dToWorld2d_scaled(position.x, position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &angle, &scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &camera.matrixPerspective2d);
            }
            else
            {
                const VEC3 positionWorld(position.x, position.y, position.z);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionWorld, &angle, &scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &camera.matrixPerspective2d);
            }
            FX &fx = anim->getFx();
            this->setBlendState(anim->getBlendState());
            anim->updateAnimation(device->delta, this, this->getOnEndAnimation(), this->getOnEndFx());
            fx.setBlendOp();
            fx.shader.update();
            if (fx.textureOverrideStage2)
                this->impl->bufferGL.setTextureByStage(fx.textureOverrideStage2, 1, 0);
            if (!fx.shader.render(&this->impl->bufferGL, this))
                return false;
            return true;
        }
        return false;
    }
    
    bool TEXTURE_VIEW::onRestoreDevice()
    {
        this->impl->texture = nullptr; // we can not release texture after device lost
        std::vector<std::string> result;
        util::split(result, this->getInternalFileName(), '|');
        if (result.size() <= 1)
        {
            this->impl->bufferGL.release();
            return false;
        }
        if (result[0].compare("texture") == 0)
        {
            if (result.size() != 5)
            {
                this->impl->bufferGL.release();
                return false;
            }
            const char *fileNameTexture = result[1].c_str();
            const auto width            = static_cast<float>(atof(result[2].c_str()));
            const auto height           = static_cast<float>(atof(result[3].c_str()));
            const bool  alpha_color     = result[4].compare("1") == 0;
            const bool ret = this->load(fileNameTexture,width,height,alpha_color);
#if defined DEBUG
            if(ret)
            {
                PRINT_INFO_IF_DEBUG( "texture [%s] successfully restored", log_util::basename(fileNameTexture));
            }
            else
            {
                PRINT_IF_DEBUG( "Failed to restore texture  [%s]",log_util::basename(this->getInternalFileName()));
            }
#endif
            return ret;
        }
        #if defined DEBUG
        PRINT_IF_DEBUG( "Failed to restore texture  [%s]",log_util::basename(this->getInternalFileName()));
        #endif
        return false;
    }
    
    FVF_PROVIDE_BY_ENGINE TEXTURE_VIEW::getFvfFromBuffer() const noexcept
    {
        return this->impl->bufferGL.isLoadedBuffer() ? this->impl->bufferGL.fvf : FVF_PROVIDE_BY_ENGINE::FVF_NONE;
    }

    void TEXTURE_VIEW::updateRestoreTexture(const float w, const float h)
    {
        const std::string currentFileName = this->getInternalFileNameString();
        if (currentFileName.size())
        {
            char                     strTemp[255] = "";
            std::vector<std::string> result;
            util::split(result, currentFileName.c_str(), '|');
            if (result.size() <= 1 || result[0].compare("texture") != 0)
                return;
            snprintf(strTemp, sizeof(strTemp), "texture|%s|%f|%f|%s", result[1].c_str(), w, h, result[4].c_str());
            this->setInternalFileName(strTemp);
        }
    }
    
    const mbm::INFO_PHYSICS * TEXTURE_VIEW::getInfoPhysics() const
    {
        return &this->impl->infoPhysics;
    }
    
    const MESH_MBM * TEXTURE_VIEW::getMesh() const
    {
        return nullptr;
    }

    FX*  TEXTURE_VIEW::getFx()const
    {
        auto * anim = getAnimation();
        if (anim)
            return &anim->getFx();
        return nullptr;
    }

    ANIMATION_MANAGER*  TEXTURE_VIEW::getAnimationManager()
    {
        return this;
    }
    
    bool TEXTURE_VIEW::isLoaded() const
    {
        return this->impl->bufferGL.isLoadedBuffer() && this->impl->texture && this->getTotalAnimation() > 0;
    }
}
