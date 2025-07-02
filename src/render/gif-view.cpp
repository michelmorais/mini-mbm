/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2017      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <gif-view.h>
#include <texture-manager.h>
#include <image-resource.h>
#include <gles-debug.h>
#include <util-interface.h>
#include <string>
#include <platform/common-jni.h>

#if (defined _DEBUG || defined DEBUG_RESTORE)
    #include <log-util.h>
#endif


namespace mbm
{

    GIF_VIEW::GIF_VIEW(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_GIF, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->enableRender = true;
        this->device->addRenderizable(this);
    }
    
    GIF_VIEW::~GIF_VIEW()
    {
        this->enableRender = false;
        this->device->removeRenderizable(this);
        this->release();
    }
    
    void GIF_VIEW::release()
    {
        this->textures.clear();
        this->bufferGL.release();
        this->interval.clear();
    }

	FX*  GIF_VIEW::getFx()const
	{
		auto * anim = getAnimation();
		if (anim)
			return &anim->fx;
		return nullptr;
	}

	ANIMATION_MANAGER*  GIF_VIEW::getAnimationManager()
	{
		return this;
	}
    
    bool GIF_VIEW::createAnimationAndShader2Texture()
    {
        this->releaseAnimation();
        auto anim = new mbm::ANIMATION();
        this->lsAnimation.push_back(anim);
        if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader))
            return false;
        return true;
    }
    
    bool GIF_VIEW::load(const char *fileNameTexture, const float w , const float h)
    {
        if (this->textures.size())
            return true;
        if (fileNameTexture == nullptr)
            return false;
        this->bufferGL.release();
        if (!createAnimationAndShader2Texture())
            return false;
        
        TEXTURE_MANAGER* manTex = TEXTURE_MANAGER::getInstance();
        INFO_GIF infoGif;
        if (manTex->loadGIF(fileNameTexture, infoGif) == false)
            return false;
        const bool idFrame = this->setFrame(w <= 0.0f ? infoGif.widthTexture : w, h <= 0.0f ? infoGif.heightTexture : h);
        if (idFrame == false)
            return false;
        this->interval.clear();
        for(unsigned int i=0; i< infoGif.totalFrames; ++i)
        {
            TEXTURE* texture = manTex->load(infoGif.fileNames[i].c_str(),true);
            if(texture)
            {
                this->textures.push_back(texture);
                this->interval.push_back(infoGif.interval[i]);
            }
            else
            {
#if defined _DEBUG
                PRINT_IF_DEBUG("Failed to load Gif [%s]", log_util::basename(infoGif.fileNames[i].c_str()));
#endif
                this->textures.clear();
                return false;
            }
        }
        ANIMATION* anim             = this->getAnimation();
        if(anim)
        {
            anim->indexCurrentFrame     = 0;
            anim->indexInitialFrame     = 0;
            anim->indexFinalFrame       = infoGif.totalFrames -1;
            anim->type                  = TYPE_ANIMATION_GROWING_LOOP;
            anim->intervalChangeFrame   = infoGif.interval[0];
        }
        this->bufferGL.idTexture0[0] = this->textures[0]->idTexture;
        this->bufferGL.useAlpha[0]   = 1;
        
        this->fileName = fileNameTexture;
        this->fileName += '|';
        this->fileName += std::to_string(w);
        this->fileName += '|';
        this->fileName += std::to_string(h);

        this->updateAABB();
        return true;
    }
    
    bool GIF_VIEW::setFrame(const float diameter)
    {
        int             indexStart = 0;
        int             indexCount = 6;
        VEC3            _position[4];
        VEC3            normal[4];
        VEC2            uv[4];
        unsigned short int index[6]      = {0, 2, 1, 2, 3, 1};
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
            this->bufferGL.useAlpha[0]   = 1;
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
    
    bool GIF_VIEW::setFrame(const float width, const float height)
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
        this->fillvertexQuadTexture(_position, normal, uv, width <= 0.0f ? 100.0f : width,
                                    height <= 0.0f ? 100.0f : height);
		const bool ret = this->bufferGL.loadBuffer(_position, normal, uv, 4, index, 1, &indexStart, &indexCount,nullptr);
        if (ret)
        {
            this->bufferGL.idTexture0[0] = indexTexture0;
            this->bufferGL.idTexture1    = indexTexture1;
            this->bufferGL.useAlpha[0]   = 1;
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
    
    BUFFER_GL * GIF_VIEW::getFrame()
    {
        return &this->bufferGL;
    }
    
    TEXTURE * GIF_VIEW::getTexture() const
    {
        if(this->indexCurrentAnimation < this->lsAnimation.size() )
        {
            ANIMATION* anim = this->lsAnimation[this->indexCurrentAnimation];
            if(anim->indexCurrentFrame < static_cast<int>(this->textures.size()))
                return this->textures[anim->indexCurrentFrame];
        }
        return nullptr;
    }
    
    bool GIF_VIEW::setTexture(
        const MESH_MBM *mesh, // fixa textura para o estagio 0 e 1, mesh == nullptr e stage = 1 para textura de estagio 2
        const char *fileNametexture, const unsigned int stage, const bool hasAlpha)
    {
        if (stage == 0)
        {
            mbm::TEXTURE *newTex = mbm::TEXTURE_MANAGER::getInstance()->load(fileNametexture, hasAlpha);
            if (newTex)
            {
                if(this->indexCurrentAnimation < this->lsAnimation.size() )
                {
                    ANIMATION* anim = this->lsAnimation[this->indexCurrentAnimation];
                    if(anim->indexCurrentFrame < static_cast<int>(this->textures.size()))
                        this->textures[anim->indexCurrentFrame] = newTex;
                }
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
    
    void GIF_VIEW::setTextureToNull()
    {
        if(this->indexCurrentAnimation < this->lsAnimation.size() )
        {
            ANIMATION* anim = this->lsAnimation[this->indexCurrentAnimation];
            if(anim->indexCurrentFrame < static_cast<int>(this->textures.size()))
                this->textures[anim->indexCurrentFrame] = nullptr;
        }
    }
    
    void GIF_VIEW::renderToRestore()
    {
        GLClearColor(1.0f, 1.0f, 1.0f, 0.0f);
        GLClearDepthf(1.0f);
        GLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        this->device->camera.updateCam(false, this->device->getBackBufferWidth(), this->device->getBackBufferHeight());
        this->render();
    }
    
    bool GIF_VIEW::isOnFrustum()
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
    
    bool GIF_VIEW::render()
    {
        if (this->bufferGL.isLoadedBuffer())
        {
            ANIMATION *animation = this->getAnimation();
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
            this->blend.set(animation->blendState);
            animation->updateAnimation(this->device->delta, this, this->onEndAnimation, this->onEndFx);
            if(animation->indexCurrentFrame < static_cast<int>(this->textures.size()))
            {
                if(animation->indexCurrentFrame < static_cast<int>(interval.size()))
                    animation->intervalChangeFrame  = interval[animation->indexCurrentFrame];
                else
                    animation->intervalChangeFrame  = interval[0];

                TEXTURE* curTex = this->textures[animation->indexCurrentFrame];
                if(curTex)
                    this->bufferGL.idTexture0[0] = curTex->idTexture;
                else
                    this->bufferGL.idTexture0[0] = 0;
            }
            animation->fx.setBlendOp();
            animation->fx.shader.update();
            if (animation->fx.textureOverrideStage2)
                this->bufferGL.idTexture1 = animation->fx.textureOverrideStage2->idTexture;
                
            if (!animation->fx.shader.render(&this->bufferGL))
                return false;
            return true;
        }
        return false;
    }
    
    void GIF_VIEW::onStop()
    {
        this->release();
    }
    
    bool GIF_VIEW::onRestoreDevice()
    {
        std::vector<std::string> result;
        util::split(result, this->fileName.c_str(), '|');
        if (result.size() != 3)
            return false;
        ANIMATION* anim = this->getAnimation();
        const TYPE_ANIMATION myType = anim ? anim->type : TYPE_ANIMATION_GROWING_LOOP;
        const int indexCurrentFrame = anim ?  anim->indexCurrentFrame : 0;
        const auto width       = static_cast<float>(atof(result[1].c_str()));
        const auto height      = static_cast<float>(atof(result[2].c_str()));
        this->bufferGL.release();
        if (!createAnimationAndShader2Texture())
            return false;
        const char* fileNameTexture = result[0].c_str();
        TEXTURE_MANAGER* manTex = TEXTURE_MANAGER::getInstance();
        INFO_GIF infoGif;
        if (manTex->loadGIF(fileNameTexture, infoGif) == false)
        {
            #if defined _DEBUG
                PRINT_IF_DEBUG("Failed to load [%s]", log_util::basename(fileNameTexture));
            #endif
            return false;
        }
        for(unsigned int i=0; i< infoGif.totalFrames; ++i)
        {
            TEXTURE* texture = manTex->load(infoGif.fileNames[i].c_str(),true);
            if(texture)
            {
                this->textures.push_back(texture);
                this->interval.push_back(infoGif.interval[i]);
            }
            else
            {
#if defined _DEBUG
                PRINT_IF_DEBUG("Failed to load [%s]", log_util::basename(infoGif.fileNames[i].c_str()));
#endif
                this->textures.clear();
                return false;
            }
        }
        const bool idFrame = this->setFrame(width <= 0.0f ? infoGif.widthTexture : width, height <= 0.0f ? infoGif.heightTexture : height);
        if (idFrame == false)
            return false;
        if(anim)
        {
            anim->indexCurrentFrame     = indexCurrentFrame;
            anim->indexInitialFrame     = 0;
            anim->indexFinalFrame       = infoGif.totalFrames -1;
            anim->type                  = myType;
            anim->intervalChangeFrame   = infoGif.interval[0];
        }
        this->bufferGL.idTexture0[0] = this->textures[0]->idTexture;
        this->bufferGL.useAlpha[0]   = 1;
#if defined DEBUG_RESTORE
        PRINT_IF_DEBUG("Gif [%s] successfully restored",log_util::basename(fileNameTexture));
#endif
        return true;
    }
    
    void GIF_VIEW::fillvertexQuadTexture(VEC3 *_position, VEC3 *normal, VEC2 *uv, const float width,
                                      const float height)
    {
        const float x  = width * 0.5f;
        const float y  = height * 0.5f;
        _position[0].x = -x;
        _position[0].y = -y;
        _position[0].z = 0;

        _position[1].x = -x;
        _position[1].y = y; //-V525
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
    
    void GIF_VIEW::updateRestoreTexture(const float w, const float h)
    {
        if (this->fileName.size())
        {
            std::vector<std::string> result;
            util::split(result, this->fileName.c_str(), '|');
            if (result.size() != 3)
                return;
            this->fileName = result[0];
            this->fileName += '|';
            this->fileName += std::to_string(w);
            this->fileName += '|';
            this->fileName += std::to_string(h);
        }
    }
    
    const mbm::INFO_PHYSICS * GIF_VIEW::getInfoPhysics() const
    {
        return &infoPhysics;
    }
    
    const MESH_MBM * GIF_VIEW::getMesh() const
    {
        return nullptr;
    }
    
    bool GIF_VIEW::isLoaded() const
    {
        return this->bufferGL.isLoadedBuffer() && this->textures.size() && this->lsAnimation.size() > 0;
    }
    
}


