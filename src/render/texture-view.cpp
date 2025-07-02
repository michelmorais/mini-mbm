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
#include <gles-debug.h>
#include <util.h>
#include <util-interface.h>

#if (defined _DEBUG || defined DEBUG_RESTORE)
    #include <log-util.h>
#endif

namespace mbm
{

    TEXTURE_VIEW::TEXTURE_VIEW(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_TEXTURE, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->texture      = nullptr;
        this->enableRender = true;
        this->device->addRenderizable(this);
    }

    TEXTURE_VIEW::TEXTURE_VIEW(const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(0, TYPE_CLASS_TEXTURE, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->texture      = nullptr;
        this->enableRender = true;
        //no scene - just restore texture
    }
    
    TEXTURE_VIEW::~TEXTURE_VIEW()
    {
        this->enableRender = false;
        this->device->removeRenderizable(this);
        this->release();
    }
    
    void TEXTURE_VIEW::release()
    {
        this->texture = nullptr;
        this->bufferGL.release();
    }
    
    bool TEXTURE_VIEW::createAnimationAndShader2Texture()
    {
        this->releaseAnimation();
        auto anim = new mbm::ANIMATION();
        this->lsAnimation.push_back(anim);
        if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader))
            return false;
        return true;
    }
    
    bool TEXTURE_VIEW::load(const IMAGE_RESOURCE *image)
    {
        if (this->texture)
            return true;
        if (image == nullptr)
            return false;

        TEXTURE_MANAGER *texMan = TEXTURE_MANAGER::getInstance();
        this->texture            = texMan->load(image);
        if (this->texture == nullptr)
            return false;
        if (!this->setFrame(static_cast<float>(image->width), static_cast<float>(image->height)))
            return false;
        if (!createAnimationAndShader2Texture())
            return false;
        this->bufferGL.idTexture0[0] = this->texture->idTexture;
        this->bufferGL.useAlpha[0]   = 1;
        char strTemp[255];
        snprintf(strTemp,sizeof(strTemp), "texture|%s|%u|%u|%d", image->nickName, image->width, image->height,this->bufferGL.useAlpha[0]);
        this->fileName = strTemp;
        this->updateAABB();
        return true;
    }
    
    bool TEXTURE_VIEW::load(const char *fileNameTexture, const float w , const float h , const bool alpha )
    {
        if (this->texture)
            return true;
        if (fileNameTexture == nullptr)
            return false;
        if (!createAnimationAndShader2Texture())
            return false;
        this->texture = TEXTURE_MANAGER::getInstance()->load(fileNameTexture, alpha);
        if (this->texture == nullptr)
            return false;
        const bool idFrame =
            this->setFrame(w <= 0.0f ? this->texture->getWidth() : w, h <= 0.0f ? this->texture->getHeight() : h);
        if (idFrame == false)
            return false;
        this->bufferGL.idTexture0[0] = this->texture ? this->texture->idTexture : 0;
        this->bufferGL.useAlpha[0]   = this->texture ? (this->texture->useAlphaChannel ? 1 : 0) : 0;
        char strTemp[255];
        const std::string baseFileName = util::getBaseName(fileNameTexture);
        sprintf(strTemp, "texture|%s|%f|%f|%d",baseFileName.c_str() , w, h, alpha ? 1 : 0);
        this->fileName = strTemp;
        this->updateAABB();
        return true;
    }
    
    bool TEXTURE_VIEW::setFrame(const float diameter)
    {
        int                indexStart = 0;
        int                indexCount = 6;
        VEC3            _position[4];
        VEC3            normal[4];
        VEC2            uv[4];
		unsigned short int index[6]      = {0, 1, 2, 2, 1, 3};
        const unsigned int indexTexture0 = this->bufferGL.idTexture0 ? this->bufferGL.idTexture0[0] : 0;
        const unsigned int indexTexture1 = this->bufferGL.idTexture1 ? this->bufferGL.idTexture1 : 0;
        this->bufferGL.release();
        this->fillvertexQuadTexture(_position, normal, uv, diameter <= 0.0f ? 100.0f : diameter,
                                    diameter <= 0.0f ? 100.0f : diameter);
        const bool ret = this->bufferGL.loadBuffer(_position, normal, uv, 4, index, 1, &indexStart, &indexCount,nullptr);
        if (ret)
        {
            this->bufferGL.idTexture0[0] = indexTexture0;
            this->bufferGL.idTexture1    = indexTexture1;
            this->bufferGL.useAlpha[0]   = this->texture ? (this->texture->useAlphaChannel ? 1 : 0) : 0;
        }
        else
            return false;
        mbm::CUBE *cube = nullptr;
        if (this->infoPhysics.lsCube.size())
            cube = this->infoPhysics.lsCube[0];
        else
        {
            cube = new mbm::CUBE();
            this->infoPhysics.lsCube.push_back(cube);
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
        VEC3            normal[4];
        VEC2            uv[4];
        unsigned short int index[6]      = {0, 1, 2, 2, 1, 3};
        const unsigned int indexTexture0 = this->bufferGL.idTexture0 ? this->bufferGL.idTexture0[0] : 0;
        const unsigned int indexTexture1 = this->bufferGL.idTexture1 ? this->bufferGL.idTexture1 : 0;
        this->fillvertexQuadTexture(_position, normal, uv, width <= 0.0f ? 100.0f : width,
                                    height <= 0.0f ? 100.0f : height);
        const bool ret = this->bufferGL.loadBuffer(_position, normal, uv, 4, index, 1, &indexStart, &indexCount,nullptr);
        if (ret)
        {
            this->bufferGL.idTexture0[0] = indexTexture0;
            this->bufferGL.idTexture1    = indexTexture1;
            this->bufferGL.useAlpha[0]   = this->texture ? (this->texture->useAlphaChannel ? 1 : 0) : 0;
        }
        else
            return false;
        mbm::CUBE *cube = nullptr;
        if (this->infoPhysics.lsCube.size())
            cube = this->infoPhysics.lsCube[0];
        else
        {
            cube = new mbm::CUBE();
            this->infoPhysics.lsCube.push_back(cube);
        }
        cube->halfDim.x = width * 0.5f;
        cube->halfDim.y = height * 0.5f;
        this->updateRestoreTexture(width, height);
        return true;
    }
    
    BUFFER_GL * TEXTURE_VIEW::getFrame()
    {
        return &this->bufferGL;
    }
    
    TEXTURE * TEXTURE_VIEW::getTexture() const
    {
        return texture;
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
                this->texture                = newTex;
                this->bufferGL.idTexture0[0] = newTex->idTexture;
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
        this->texture = nullptr;
    }

	std::string TEXTURE_VIEW::getFileNameTexture()const
	{
		std::string ret;
		if(this->texture)
			ret = this->texture->getFileNameTexture();
		return ret;
	}
    
    bool TEXTURE_VIEW::isOnFrustum()
    {
        if (this->bufferGL.isLoadedBuffer())
        {
            IS_ON_FRUSTUM verify(this);
            bool ret = verify.isOnFrustum(this->is3D, this->is2dS);
            if(ret == false)
            {
                ANIMATION *anim = this->getAnimation();
                anim->updateAnimation(this->device->delta, this, this->onEndAnimation, this->onEndFx);
            }
            return ret;
        }
        return false;
    }
    
    bool TEXTURE_VIEW::render()
    {
        if (this->bufferGL.isLoadedBuffer())
        {
            ANIMATION *anim = this->getAnimation();
            if (this->is3D)
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective);
            }
            else if (this->is2dS)
            {
                VEC3 positionScreen(this->position.x, this->position.y, this->position.z);
                this->device->transformeScreen2dToWorld2d_scaled(this->position.x, this->position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
            }
            else
            {
                const VEC3 positionWorld(this->position.x, this->position.y, this->position.z);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionWorld, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
            }
            this->blend.set(anim->blendState);
            anim->updateAnimation(this->device->delta, this, this->onEndAnimation, this->onEndFx);
            anim->fx.setBlendOp();
            anim->fx.shader.update();
            if (anim->fx.textureOverrideStage2)
                this->bufferGL.idTexture1 = anim->fx.textureOverrideStage2->idTexture;
            if (!anim->fx.shader.render(&this->bufferGL))
                return false;
            return true;
        }
        return false;
    }
    
    void TEXTURE_VIEW::onStop()
    {
        this->release();
    }
    
    bool TEXTURE_VIEW::onRestoreDevice()
    {
        this->texture = nullptr;
        std::vector<std::string> result;
        util::split(result, this->fileName.c_str(), '|');
        if (result.size() <= 1)
        {
            this->bufferGL.release();
            return false;
        }
        if (result[0].compare("texture") == 0)
        {
            if (result.size() != 5)
            {
                this->bufferGL.release();
                return false;
            }
            const char *fileNameTexture = result[1].c_str();
            const auto width       = static_cast<float>(atof(result[2].c_str()));
            const auto height      = static_cast<float>(atof(result[3].c_str()));
            const bool  alpha_color = result[4].compare("1") == 0;
            const bool ret = this->load(fileNameTexture,width,height,alpha_color);
#if defined DEBUG_RESTORE
            if(ret)
			{
                PRINT_INFO_IF_DEBUG( "texture [%s] successfully restored", log_util::basename(fileNameTexture));
			}
            else
			{
                PRINT_IF_DEBUG( "Failed to restore texture  [%s]",log_util::basename( this->fileName.c_str()));
			}
#endif
            return ret;
        }
        #if defined DEBUG_RESTORE
        PRINT_IF_DEBUG( "Failed to restore texture  [%s]",log_util::basename(this->fileName.c_str()));
        #endif
        return false;
    }
    
    void TEXTURE_VIEW::fillvertexQuadTexture(VEC3 *_position, VEC3 *normal, VEC2 *uv, const float width,
                                      const float height)
    {
        const float x  = width * 0.5f;
        const float y  = height * 0.5f;
        _position[0].x = -x;
        _position[0].y = -y;
        _position[0].z = 0;

        _position[1].x = -x;
        _position[1].y = y;
        _position[1].z = 0;

        _position[2].x = x;
        _position[2].y = -y;
        _position[2].z = 0;

        _position[3].x = x;
        _position[3].y = y;
        _position[3].z = 0;
        for (int i = 0; i < 4; ++i)
        {
            normal[i].x = 0;
            normal[i].y = 0;
            normal[i].z = 1;
        }

        //----------------------------------------
        uv[0].x = 0;
        uv[0].y = 1;
        uv[1].x = 0;
        uv[1].y = 0;
        uv[2].x = 1;
        uv[2].y = 1;
        uv[3].x = 1;
        uv[3].y = 0;
    }
    
    void TEXTURE_VIEW::updateRestoreTexture(const float w, const float h)
    {
        if (this->fileName.size())
        {
            char                     strTemp[255] = "";
            std::vector<std::string> result;
            util::split(result, this->fileName.c_str(), '|');
            if (result.size() <= 1 || result[0].compare("texture") != 0)
                return;
            sprintf(strTemp, "texture|%s|%f|%f|%s", result[1].c_str(), w, h, result[4].c_str());
            this->fileName = strTemp;
        }
    }
    
    const mbm::INFO_PHYSICS * TEXTURE_VIEW::getInfoPhysics() const
    {
        return &infoPhysics;
    }
    
    const MESH_MBM * TEXTURE_VIEW::getMesh() const
    {
        return nullptr;
    }

	FX*  TEXTURE_VIEW::getFx()const
	{
		auto * anim = getAnimation();
		if (anim)
			return &anim->fx;
		return nullptr;
	}

	ANIMATION_MANAGER*  TEXTURE_VIEW::getAnimationManager()
	{
		return this;
	}
    
    bool TEXTURE_VIEW::isLoaded() const
    {
        return this->bufferGL.isLoadedBuffer() && this->texture && this->lsAnimation.size() > 0;
    }
}


