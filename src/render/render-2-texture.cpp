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
#include <gles-debug.h>

namespace mbm
{

    CAMERA_TARGET::CAMERA_TARGET() noexcept : position(0, 0, 0), scale(1, 1, 1), angle(0, 0, 0), focus(0, 0, 0), up(0, 1, 0), zNear(0.1f), zFar(1000.0f)
    {
        MatrixIdentity(&this->matrixView);
        MatrixIdentity(&this->matrixOrtho);
    }
    
    void CAMERA_TARGET::enableMode2D(mbm::DEVICE *device, const float width, const float height)
    {
        const VEC3 posCam(-this->position.x, -this->position.y, 100);
        MatrixIdentity(&this->matrixView);
        MatrixTranslationRotationScale(&SHADER::modelView, &posCam, &this->angle, &this->scale);
        MatrixOrthoLH(&this->matrixOrtho, width, height, zNear, zFar);
        MatrixMultiply(&device->camera.matrixPerspective2d, &this->matrixView, &this->matrixOrtho);
    }
    
    void CAMERA_TARGET::enableMode3D(mbm::DEVICE *device, const float width, const float height)
    {
        const float aspect = width / height;
        const auto Scale  = static_cast<const float>(1.0f / tan(device->camera.angleOfView * 0.5f * static_cast<const float>(M_PI) / 180.0f));
        MatrixPerspectiveFovLH(&this->matrixProj, Scale, aspect, zNear, zFar);
        MatrixLookAtLH(&this->matrixView, &this->position, &this->focus, &this->up);
        MatrixMultiply(&device->camera.matrixPerspective, &this->matrixView, &this->matrixProj);
    }
    
    RENDER_2_TEXTURE::RENDER_2_TEXTURE(const SCENE *scene, const bool _is3d, const bool _is2dScreen) :
        RENDERIZABLE_TO_TARGET(scene, TYPE_CLASS_RENDER_2_TEX, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->modeTextureOnly = false;
        this->device->addRenderizable(this);
        this->device->addObjectRender2Texture(this);
        this->isRender2Texture = false;
        this->texture          = nullptr;
    }
    
    RENDER_2_TEXTURE::~RENDER_2_TEXTURE()
    {
        this->device->removeObjectRender2Texture(this);
        this->device->removeRenderizable(this);
    }
    
    void RENDER_2_TEXTURE::removeFromRender2Texture(RENDERIZABLE *ptr)
    {
        if (ptr && ptr->isRender2Texture)
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
        this->texture = nullptr;
        this->clear();
        this->fileName.clear();
        this->bufferGL.release();
    }
    
    bool RENDER_2_TEXTURE::load(const unsigned int widthFrame, const unsigned int heightFrame, const unsigned int _widthTexture,const unsigned int _heightTexture, const char *nickName, const bool hasAlpha, int * texture_id_out)
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
                return false;
            }
            this->widthTexture  = _widthTexture;
            this->heightTexture = _heightTexture;
            this->texture = mbm::TEXTURE_MANAGER::getInstance()->createTextureRenderTarget(this, nickName, hasAlpha);
            if (this->texture)
            {
                int             indexStart = 0;
                int             indexCount = 6;
                VEC3            _position[4];
                VEC3            normal[4];
                VEC2            uv[4];
                unsigned short int index[6] = {0, 1, 2, 2, 1, 3};
                this->fillvertexQuad(_position, normal, uv, static_cast<const float>(widthFrame), static_cast<const float>(heightFrame));
                if (this->bufferGL.loadBuffer(_position, normal, uv, 4, index, 1, &indexStart, &indexCount,nullptr))
                {
                    this->bufferGL.idTexture0[0] = this->texture->idTexture;
                    if(texture_id_out)
                        *texture_id_out          = this->texture->idTexture;
                    this->bufferGL.useAlpha[0]   = this->texture ? (this->texture->useAlphaChannel ? 1 : 0) : 0;
                }
                else
                {
                    return false;
                }
                if (!createAnimationAndShader2Render2Texture())
                    return false;
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
                this->fileName = strTemp;
                this->updateAABB();
            }
        }
        return (this->texture != nullptr);
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
    
    bool RENDER_2_TEXTURE::saveAsPNG(const char* newFileOutNamePNG,
        const int x,const int y,
        const int _width,const int _height)
    {
        if(newFileOutNamePNG == nullptr)
            return log_util::fail(__LINE__,__FILE__,"file name to save png is null");
        if(!this->isLoaded())
            return log_util::fail(__LINE__,__FILE__,"render to texture is not loaded!");
        if(this->idTextureDynamic == 0)
            return log_util::fail(__LINE__,__FILE__,"texture is not created!");
        if(this->texture == nullptr)
            return log_util::fail(__LINE__,__FILE__,"texture is not created!");
        if(strcasecmp(newFileOutNamePNG,this->fileName.c_str()) == 0)
            return log_util::fail(__LINE__,__FILE__,"file name texture in is the same as render2texture [%s]!",fileName.c_str());
        if(x < 0 || _width <= 0 || (_width + x) > static_cast<int>(this->widthTexture))
            return log_util::fail(__LINE__,__FILE__,"size expected [0-0 %dx%d] got [%d-%d %dx%d]",this->widthTexture,this->heightTexture,x,y,_width,_height);
        if(y < 0 || _height <= 0 || (_height + y) > static_cast<int>(this->heightTexture))
            return log_util::fail(__LINE__,__FILE__,"size expected [0-0 %dx%d] got [%d-%d %dx%d]",this->widthTexture,this->heightTexture,x,y,_width,_height);
        const int channel = this->texture->useAlphaChannel ? 4 : 3;
        const int sizeImage = _width * _height * channel;
        auto  image = new unsigned char[sizeImage];

        GLBindFramebuffer(GL_FRAMEBUFFER, this->idFrameBuffer);
        
        glReadPixels(x,y,_width,_height,channel == 4 ? GL_RGBA : GL_RGB,GL_UNSIGNED_BYTE,image);
        const GLenum error = glGetError();
        if(error)
        {
            delete [] image;
            const char *errorAsString = log_util::getDescriptionError(error);
            return log_util::fail(__LINE__,__FILE__,"Failed to read pixel [%s]",errorAsString);
        }
        
        //if(this->texture->useAlphaChannel == false)
        //{
        //    const int s = w * h;
        //    const int stride = 3;
        //    auto  image3x3 = new unsigned char[s * 3];
        //    for(int i=0,j=0; i< sizeImage; i+=4,j+=3)
        //    {
        //        memcpy(&image3x3[j],&image[i],stride);
        //    }
        //    delete [] image;
        //    image = image3x3;
        //}

        GLBindFramebuffer(GL_FRAMEBUFFER, 0);
        this->flip_vertically(image,_width,_height,channel);
        std::vector<unsigned char> png;
        unsigned int errorPNG = lodepng::encode(png,image, static_cast<unsigned int>(_width), static_cast<unsigned int>(_height),channel == 4 ? LCT_RGBA : LCT_RGB);
        delete [] image;
        if (errorPNG)
            return log_util::fail(__LINE__,__FILE__, "PNG encoding error  [%s]", lodepng_error_text(errorPNG));
        errorPNG = lodepng::save_file(png, newFileOutNamePNG);
        if (errorPNG)
            return log_util::fail(__LINE__,__FILE__, "PNG encoding error  [%s]", lodepng_error_text(errorPNG));
        return true;
    }

    void RENDER_2_TEXTURE::clear()
    {
        for (unsigned int i = 0; i < this->lsObjects3dRender.size(); ++i)
        {
            RENDERIZABLE *ptr = lsObjects3dRender[i];
            ptr->isRender2Texture = false;
        }
        this->lsObjects3dRender.clear();
        for (unsigned int i = 0; i < this->lsObjects2dRender.size(); ++i)
        {
            RENDERIZABLE *ptr = lsObjects2dRender[i];
            ptr->isRender2Texture = false;
        }
        this->lsObjects2dRender.clear();
    }

    bool RENDER_2_TEXTURE::removeObject2Render(RENDERIZABLE *ptr)
    {
        if (ptr == nullptr)
            return false;
        if (ptr->isRender2Texture)
            return true;
        if (ptr->is3D)
        {
            for (unsigned int i = 0; i < this->lsObjects3dRender.size(); ++i)
            {
                RENDERIZABLE *other = lsObjects3dRender[i];
                if (ptr == other)
                {
                    ptr->isRender2Texture = false;
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
                    ptr->isRender2Texture = false;
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
        if (ptr->isRender2Texture)
            return true;
        if (ptr->is3D)
        {
            for (unsigned int i = 0; i < this->lsObjects3dRender.size(); ++i)
            {
                RENDERIZABLE *other = lsObjects3dRender[i];
                if (ptr == other)
                    return true;
            }
            lsObjects3dRender.push_back(ptr);
            ptr->isRender2Texture = true;
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
            ptr->isRender2Texture = true;
        }
        return true;
    }
    
    void RENDER_2_TEXTURE::onStop()
    {
        this->release();
    }
    
    bool RENDER_2_TEXTURE::render() // Renderiza a textura
    {
        if (this->bufferGL.isLoadedBuffer())
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
            mbm::ANIMATION *anim = this->getAnimation();
            if (anim)
            {
                this->blend.set(anim->blendState);
                anim->updateAnimation(this->device->delta, this, this->onEndAnimation,this->onEndFx);
                anim->fx.shader.update(); // glUseProgram
                anim->fx.setBlendOp();
                if (anim->fx.textureOverrideStage2)
                    this->bufferGL.idTexture1 = anim->fx.textureOverrideStage2->idTexture;
                if (!anim->fx.shader.render(&this->bufferGL))
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
            const CUBE *cube        = this->infoPhysics.lsCube[0];
            const float widthFrame  = cube->halfDim.x * 2.0f;
            const float heightFrame = cube->halfDim.y * 2.0f;
            this->camera3d.enableMode3D(this->device, widthFrame, heightFrame);
            for (unsigned int i = 0; i < this->lsObjects3dRender.size(); ++i)
            {
                RENDERIZABLE *ptr = lsObjects3dRender[i];
                const VEC3 distFromCam(ptr->position - this->camera3d.position);
                ptr->__distFromView = distFromCam.length();
            }
            std::sort(lsObjects3dRender.begin(), lsObjects3dRender.end(),
                      [](const RENDERIZABLE *a, const RENDERIZABLE *b) { return b->__distFromView < a->__distFromView; });
            for (unsigned int i = 0; i < this->lsObjects3dRender.size(); ++i)
            {
                RENDERIZABLE *ptr = lsObjects3dRender[i];
                if (ptr->enableRender)
                {
                    const bool oldAlwaysRender = ptr->alwaysRenderize;
                    ptr->alwaysRenderize       = false; // for not animate twice
                    const bool ret             = ptr->render();
                    ptr->alwaysRenderize       = oldAlwaysRender;
                    if (!ret)
                        return false;
                }
            }
        }
        if (this->lsObjects2dRender.size())
        {
            this->camera2d.enableMode2D(this->device, static_cast<float>(this->texture->getWidth()), static_cast<float>(this->texture->getHeight()));
            for (unsigned int i = 0; i < this->lsObjects2dRender.size(); ++i)
            {
                RENDERIZABLE *ptr   = lsObjects2dRender[i];
                ptr->__distFromView = ptr->position.z;
            }
            std::sort(lsObjects2dRender.begin(), lsObjects2dRender.end(),
                      [](const RENDERIZABLE *a, const RENDERIZABLE *b) { return b->__distFromView < a->__distFromView; });
            for (unsigned int i = 0; i < this->lsObjects2dRender.size(); ++i)
            {
                RENDERIZABLE *ptr = lsObjects2dRender[i];
                if (ptr->enableRender)
                {
                    const bool oldAlwaysRender = ptr->alwaysRenderize;
                    ptr->alwaysRenderize       = false; // to not animate twice
                    const bool ret             = ptr->render();
                    ptr->alwaysRenderize       = oldAlwaysRender;
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
            if (this->isRender2Texture)
                return false;
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
    
    bool RENDER_2_TEXTURE::onRestoreDevice()
    {
        std::vector<std::string> result;
        util::split(result, this->fileName.c_str(), '|');
        if (result.size() <= 1)
            return false;
        if (result[0].compare("rende2texture") == 0)
        {
            if (result.size() != 7)
                return false;
            const char *fileNameTexture = result[1].c_str();
            if (result[1].size() == 0)
                return false;
            const auto width    = static_cast<const unsigned int>(std::atoi(result[4].c_str()));
            const auto height   = static_cast<const unsigned int>(std::atoi(result[5].c_str()));
            bool               hasAlpha = result[6].compare("true") == 0 ? true : false;
            this->texture               = nullptr;
            CUBE *      cube            = this->infoPhysics.lsCube[0];
            const float widthFrame      = cube->halfDim.x * 2.0f;
            const float heightFrame     = cube->halfDim.y * 2.0f;
            if (!this->load(static_cast<const unsigned int>(widthFrame), static_cast<const unsigned int>(heightFrame), width, height, fileNameTexture, hasAlpha, nullptr))
                return false;
#if defined DEBUG_RESTORE
            PRINT_IF_DEBUG("rende2texture [%s] successfully restored", log_util::basename(fileNameTexture));
#endif
            return true;
        }
        #if defined DEBUG_RESTORE
        PRINT_IF_DEBUG("Failed to restore rende2texture [%s]",log_util::basename(this->fileName.c_str()));
        #endif
        return false;
    }
    
    void RENDER_2_TEXTURE::fillvertexQuad(VEC3 *_position, VEC3 *normal, VEC2 *uv, const float width, const float height)
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
        uv[0].x = 0;
        uv[0].y = 0;
        uv[1].x = 0;
        uv[1].y = 1;
        uv[2].x = 1;
        uv[2].y = 0;
        uv[3].x = 1;
        uv[3].y = 1;
    }
    
    bool RENDER_2_TEXTURE::createAnimationAndShader2Render2Texture()
    {
        this->releaseAnimation();
        auto anim = new mbm::ANIMATION();
        this->lsAnimation.push_back(anim);
        if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader))
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
			return &anim->fx;
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
