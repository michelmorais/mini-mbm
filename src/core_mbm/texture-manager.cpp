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

#include <texture-manager.h>
#include <renderizable.h>
#include <uber-image.h>
#include <image-resource.h>
#include <util-interface.h>

#include <lodepng/lodepng.h>
#include <stb/stb-interface.h>

#if defined ANDROID
    #include <device.h>
    #if defined     USE_OPENGL_ES
        #if defined (USE_DUMMY_BACK_END_ENGINE)
            // ANDROID_AND_NOT_OPENGL_ES: For different backend engine on Android, implementation here
            #include <dummy-engine.h> // for REMINDER_TODO, you can remove it after implement the functions
        #else
            #include <core_mbm/specific-opengl_es.h>
        #endif
    #endif
#endif

#include <platform/mismatch-platform.h>
#include <cstring>
#include <cstdio>

#if defined USE_EDITOR_FEATURES
    #if (defined __linux__ || defined(__APPLE__) || defined _WIN32) && !defined ANDROID
        #include <tinyfiledialogs/tinyfiledialogs.h>
    #endif
#endif

namespace mbm
{
    bool TEXTURE::no_filter = false;

    TEXTURE::TEXTURE() noexcept
    {
        ptrTexture      = nullptr;
        idTexture       = 0;
        fileName[0]     = 0;
        width           = 0;
        height          = 0;
        useAlphaChannel = false;
    }

    bool TEXTURE::isLoaded()const
    {
        return fileName[0] != 0;
    }
    
    TEXTURE::~TEXTURE()
    {
        release();
    }
    
    const char * TEXTURE::getFileNameTexture() const noexcept
    {
        return this->fileName.c_str();
    }

    struct DeleteArrayChar
    {
        void operator() (uint8_t* f) 
        {
            delete [] f;
        }
    };

    bool TEXTURE::loadTTF(const char *fileNameTTF, std::vector<stbtt_aligned_quad *> *lsStbFontOut,
                       std::vector<VEC2> *lsWidthLetterOut, const float heightLetter,const bool saveAsPng)
    {
        if (!fileNameTTF)
            return false;
        this->useAlphaChannel = true;
        FILE *fp              = util::openFile(fileNameTTF, "rb");
        size_t   sFile        = 0;
        if (fp && util::getSizeFile(fp, &sFile))
        {
            const size_t lenTTFfile  = strlen(fileNameTTF);
            bool                             ret = false;
            stbtt_bakedchar                  cdata[225]; // ASCII 32..126 is 95 glyphs but we want all (255 - 30 = 225)
            std::unique_ptr<uint8_t[],DeleteArrayChar> ttf_buffer(new uint8_t[sFile],DeleteArrayChar());
            if (fread(&ttf_buffer[0], 1, sFile, fp) == sFile)
            {
                this->width  = 512;
                this->height = 256;
                int hRet     = -1;
                do
                {
                    const uint32_t               tTotal = this->width * this->height;
                    #if !defined ANDROID
                    try 
                    #endif
                    {
                        std::unique_ptr<uint8_t[]> temp_bitmap(new uint8_t[tTotal]);
                        uint8_t *                  data = temp_bitmap.get();
                        hRet = stbtt_BakeFontBitmap(&ttf_buffer[0], 0, heightLetter, data, static_cast<int>(this->width), static_cast<int>(this->height), 30, 225,cdata);
                        if (hRet <= 0)
                        {
                            if (this->width > this->height)
                            {
                                this->height *= 2;
                            }
                            else
                            {
                                this->width *= 2;
                            }
                        }
                        else
                        {
                            const uint32_t               s = tTotal * 4;
                            std::unique_ptr<uint8_t[],DeleteArrayChar> pBuffer(new uint8_t[s],DeleteArrayChar());
                            for (uint32_t i = 0, j = 0; i < tTotal; ++i, j += 4)
                            {
                                uint8_t alpha = temp_bitmap[i];
                                pBuffer[j]          = alpha; // red
                                pBuffer[j + 1]      = alpha; // green
                                pBuffer[j + 2]      = alpha; // blue
                                pBuffer[j + 3]      = alpha; // alpha
                            }
                            ret = this->loadFromData(&pBuffer[0], this->width, this->height, 8, 4, true);
                            if(ret && saveAsPng)
                            {
                                char asPngFileName[255] = "";
                                strncpy(asPngFileName,fileNameTTF,sizeof(asPngFileName)-1);
                                if(lenTTFfile > 4)
                                    sprintf(&asPngFileName[lenTTFfile - 4],"-%2.0f.png",heightLetter);
                                else
                                    strncat(asPngFileName,".png",sizeof(asPngFileName) - strlen(asPngFileName) - 1);
                                std::vector<uint8_t> png;
                                uint32_t error = lodepng::encode(png, &pBuffer[0], width, height,LCT_RGBA);
                                if (error)
                                {
                                    ERROR_AT(__LINE__,__FILE__,"PNG encoding error  [%s]", lodepng_error_text(error));
                                }
                                else
                                {
                                    error = lodepng::save_file(png, asPngFileName);
                                    if(error == 0)//no error
                                        this->fileName = util::getBaseName(asPngFileName);
                                }
                            }
                        }
                    }
                    #if !defined ANDROID
                    catch(const std::exception & e)
                    {
                        ERROR_LOG(e.what());
                        return false;
                    }
                    #endif
                } while (hRet <= 0);
            }
            fclose(fp);
            fp = nullptr;
            if (ret)
            {
                if (lsStbFontOut && lsWidthLetterOut)
                {
                    lsStbFontOut->resize(255);
                    lsWidthLetterOut->resize(255);
                    for (int index = 0; index < 225; ++index)
                    {
                        float               x = 0, y = 0;
                        auto q = new stbtt_aligned_quad();
                        stbtt_GetBakedQuad(cdata, static_cast<int>(this->width),static_cast<int>(this->height), index, &x, &y, q,1); // 1=opengl & d3d10+,0=d3d9
                        const VEC2 p(static_cast<float>(cdata[index].x1 - cdata[index].x0),static_cast<float>(cdata[index].y1 - cdata[index].y0));
                        (*lsStbFontOut)[static_cast<std::vector<VEC2>::size_type>(index + 30)]      = q;
                        (*lsWidthLetterOut)[ static_cast<std::vector<VEC2>::size_type>(index + 30)] = p;
                    }
                }
                return true;
            }
        }
        PRINT_IF_DEBUG("error open file [%s]", fileNameTTF);
        return false;
    }

    bool TEXTURE::loadSolidColor(const char* colorAsString, const bool hasColorAlpha)
    {
        if(colorAsString == nullptr)
        {
            PRINT_IF_DEBUG("Color string expected is null");
            return false;
        }
        if(colorAsString[0] != '#')
        {
            PRINT_IF_DEBUG("Color string expected is '#'");
            return false;
        }
        this->fileName = colorAsString;
        COLOR color;
        colorAsString++;
        int len = strlen(colorAsString);
        if (len == 8)
        {
            char alpha[3] = {0,0,0};
            alpha[0] = *colorAsString;
            colorAsString++;
            alpha[1] = *colorAsString;
            colorAsString++;
            const int n = strtol(colorAsString,nullptr, 16);
            color = COLOR(n);
            color.a = strtol(alpha, nullptr, 16) * 1.0f / 255.0f;
        }
        else if (len == 6)
        {
            const int n = strtol(colorAsString, nullptr, 16);
            color = COLOR(n);
            color.a = 1.0f;
        }
        if(hasColorAlpha)
        {
            uint8_t pixel[4 * 4 * 4];
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            uint8_t a = 255;
            color.get(&r,&g,&b,&a);
            for (uint32_t i = 0; i < 4 * 4 * 4; i += 4)
            {
                pixel[i] = r;
                pixel[i+1] = g;
                pixel[i+2] = b;
                pixel[i+3] = a;
            }
            return this->loadFromData(pixel,4,4,8,4,true);
        }
        else
        {
            uint8_t pixel[4 * 4 * 3];
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            color.get(&r,&g,&b);
            for (uint32_t i = 0; i < 4 * 4 * 3; i += 3)
            {
                pixel[i] = r;
                pixel[i+1] = g;
                pixel[i+2] = b;
            }
            return this->loadFromData(pixel,4,4,8,3,false);
        }
    }
    
    bool TEXTURE::load(const char *fileNameTexture, const bool hasColorAlpha)
    {
        if (!fileNameTexture)
            return false;
        this->release();
        this->useAlphaChannel = true;
        if(fileNameTexture[0] == '#' )
            return loadSolidColor(fileNameTexture,hasColorAlpha);
        std::vector<std::string> result;
        util::split(result, fileNameTexture, '.');
        if (result.size() == 0)
        {
            PRINT_IF_DEBUG("not found [%s]", fileNameTexture);
            return false;
        }
        if (strcasecmp("uberimg", result[result.size() - 1].c_str()) == 0)
        {
            mbm::UBER_IMG uberImg;
            if (uberImg.load(fileNameTexture))
                return this->loadFromData(uberImg.getImage(), uberImg.getWidth(), uberImg.getHeight(), 8,
                                          uberImg.getChannel(), hasColorAlpha);
            PRINT_IF_DEBUG("failed to load uberImage %s", fileNameTexture);
            return false;
        }
        else if (strcasecmp("png", result[result.size() - 1].c_str()) == 0)
        {
            std::vector<uint8_t> image; // the raw pixels
            unsigned                   error = 0;
            if (hasColorAlpha)
                error = lodepng::decode(image, this->width, this->height, fileNameTexture, LCT_RGBA, 8);
            else
                error = lodepng::decode(image, this->width, this->height, fileNameTexture, LCT_RGB, 8);
            if (error)
            {
#if defined _WIN32
                PRINT_IF_DEBUG("failed to load [%s].\n%s", fileNameTexture, lodepng_error_text(error));
                return false;
#elif defined ANDROID
                if (!loadFromAndroid(fileNameTexture, hasColorAlpha))
                {
                    PRINT_IF_DEBUG("failed to load %s.\n%s", fileNameTexture,
                                 lodepng_error_text(error));
                    return false;
                }
                return true;
#endif
            }
            if (hasColorAlpha)
                return this->loadFromData(&image[0], this->width, this->height, 8, 4, hasColorAlpha);
            return this->loadFromData(&image[0], this->width, this->height, 8, 3, hasColorAlpha);
        }
        else if (strcasecmp("ttf", result[result.size() - 1].c_str()) == 0)
        {
            if (this->loadTTF(fileNameTexture, nullptr, nullptr, 32.0f,false))
                return true;
            return false;
        }
        else
        {
            int       x    = 0;
            int       y    = 0;
            int       comp = 0;
            bool    result = false;
            const int n    = hasColorAlpha == true ? 4 : 3;
            stbi_uc * data = stbi_load(fileNameTexture, &x, &y, &comp, n);
            if (data && x && y && comp)
            {
                this->width  = static_cast<uint32_t>(x);
                this->height = static_cast<uint32_t>(y);
                if (hasColorAlpha)
                    result = this->loadFromData(data, this->width, this->height, 8, 4, hasColorAlpha);
                else
                    result = this->loadFromData(data, this->width, this->height, 8, 3, hasColorAlpha);
                free(data);
            }
            else
            {
                if (data)
                    free(data);
#if defined ANDROID
                if (loadFromAndroid(fileNameTexture, hasColorAlpha))
                {
                    return true;
                }
                else
                {
                    PRINT_IF_DEBUG("failed to load texture %s.", fileNameTexture);
                    return false;
                }
#endif
                PRINT_IF_DEBUG("failed to load texture %s .", fileNameTexture);
            }
            return result;
        }
    }

    void TEXTURE::enableFilter(bool value) noexcept
    {
        TEXTURE::no_filter = !value;
    }
    
    uint32_t TEXTURE::getWidth()const noexcept
    {
        return this->width;
    }
    
    uint32_t TEXTURE::getHeight()const noexcept
    {
        return this->height;
    }
    
#if defined (USE_DUMMY_BACK_END_ENGINE) && defined ANDROID
    // ANDROID_AND_NOT_OPENGL_ES: For different backend engine on Android, implementation here
    bool TEXTURE::loadFromAndroid(const char *_fileName, const bool hasAlpha)
    {
        return false;
    }
#elif defined ANDROID
    bool TEXTURE::loadFromAndroid(const char *_fileName, const bool hasAlpha) // Android 24/32 bits true color
    {
        mbm::DEVICE *device                    = mbm::DEVICE::getInstance();
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE* cJni = device->specificContextDevice;
        int              wint   = 0;
        int              hint   = 0;
        uint8_t *  pixels = cJni->getImageDataFromDroid(_fileName, &wint, &hint);
        if (pixels)
        {
            if (wint < 0 && hint < 0)
            {
                this->width  = static_cast<uint32_t>(-wint);
                this->height = static_cast<uint32_t>(-hint);
                idTexture = ((static_cast<int>(pixels[0]) << 24) | 
                             (static_cast<int>(pixels[1]) << 16) | 
                             (static_cast<int>(pixels[2]) << 8 ) | 
                             (static_cast<int>(pixels[3])));
                this->useAlphaChannel = (pixels[4] ? true : false) || hasAlpha;
                PRINT_IF_DEBUG("texture generated externally!\n width:%u height:%u id:%d", this->width,this->height, idTexture);
                delete[] pixels;
                return true;
            }
            else
            {
                const bool ret = this->loadFromData(pixels, this->width, this->height, 8, 3, hasAlpha);
                delete[] pixels;
                return ret;
            }
        }
        ERROR_LOG("%s", "Error 'getImageDataFromDroid' JNI ");
        return false;
    }
#endif
    
    TEXTURE_MANAGER * TEXTURE_MANAGER::getInstance()
    {
        if (instanceTextureManager == nullptr)
            instanceTextureManager = new TEXTURE_MANAGER();
        return instanceTextureManager;
    }
    
    void TEXTURE_MANAGER::release()
    {
        if (instanceTextureManager != nullptr)
            delete instanceTextureManager;
        instanceTextureManager = nullptr;
    }
    
    TEXTURE * TEXTURE_MANAGER::load(const IMAGE_RESOURCE *imageResource)
    {
        if (!imageResource)
            return nullptr;
        if (static_cast<int>(imageResource->width) > this->maxTextureSize || static_cast<int>(imageResource->height) > this->maxTextureSize)
        {
            PRINT_IF_DEBUG("max size to generate texture is  %d/%d.",
                         imageResource->width > imageResource->height ? imageResource->width : imageResource->height,
                         this->maxTextureSize);
            return nullptr;
        }
        std::string fileNameBase = util::getBaseName(imageResource->nickName);
        TEXTURE *texture = lsTextures[fileNameBase];
        if (texture)
            return texture;
        texture = new TEXTURE();
        if (texture->loadFromResourceData(imageResource))
        {
            texture->fileName = std::move(fileNameBase);
            lsTextures[texture->fileName] = texture;
        }
        else
        {
            delete texture;
            texture = nullptr;
            PRINT_IF_DEBUG("failed to load texture:%s.",imageResource->nickName);
        }
        return texture;
    }
    
    TEXTURE * TEXTURE_MANAGER::load(const uint32_t width, const uint32_t height, const uint8_t *data,
                         const char *nickName, const uint16_t depth, const uint16_t channel)
    {
        const char *fileName = nickName;
        if (!fileName)
            return nullptr;
        if (static_cast<int>(width) > this->maxTextureSize || static_cast<int>(height) > this->maxTextureSize)
        {
            PRINT_IF_DEBUG("max size to generate texture is  %d/%d.", width > height ? width : height,
                         this->maxTextureSize);
            return nullptr;
        }
        std::string fileNameBase = util::getBaseName(fileName);
        auto texture = lsTextures[fileNameBase];
        if (texture)
            return texture;
        texture  = new TEXTURE();
        if (texture->loadFromData(data, width, height, depth, channel, channel == 4))
        {
            texture->fileName = std::move(fileNameBase);
            lsTextures[texture->fileName] = texture;
        }
        else
        {
            delete texture;
            texture = nullptr;
            PRINT_IF_DEBUG("failed to load texture: %s.", nickName);
        }
        if (channel == 4 && texture)
            texture->useAlphaChannel = true;
        return texture;
    }
    
    TEXTURE * TEXTURE_MANAGER::load(const uint32_t width, const uint32_t height, const uint8_t *data,
                         const char *nickName, const uint16_t depth, const uint16_t channel,
                         const bool hasAlpha)
    {
        if (nickName == nullptr)
            return nullptr;
        if (static_cast<int>(width) > this->maxTextureSize || static_cast<int>(height) > this->maxTextureSize)
        {
            PRINT_IF_DEBUG("max size to generate texture is  %d/%d.", width > height ? width : height,
                         this->maxTextureSize);
            return nullptr;
        }
        std::string fileNameBase = util::getBaseName(nickName);
        auto texture = lsTextures[fileNameBase];
        if (texture)
            return texture;
        texture  = new TEXTURE();
        if (texture->loadFromData(data, width, height, depth, channel, hasAlpha))
        {
            texture->fileName = std::move(fileNameBase);
            texture->useAlphaChannel = hasAlpha;
            lsTextures[texture->fileName] = texture;
        }
        else
        {
            delete texture;
            texture = nullptr;
            PRINT_IF_DEBUG("failed to load texture: %s", nickName);
        }
        return texture;
    }
    
    TEXTURE * TEXTURE_MANAGER::load(const char *fileName, const bool hasAlpha)
    {
        if (fileName == nullptr)
            return nullptr;
        std::string fileNameBase = util::getBaseName(fileName);
        auto texture = lsTextures[fileNameBase];
        if (texture)
            return texture;
        texture  = new TEXTURE();
        if (texture->load(getFilePathTexture(fileNameBase.c_str(),fileName), hasAlpha))
        {
            texture->fileName = std::move(fileNameBase);
            texture->useAlphaChannel = hasAlpha ? true : false;
            lsTextures[texture->fileName] = texture;
        }
        else
        {
            delete texture;
            texture = nullptr;
            texture = this->loadNativeEngine(fileName, hasAlpha);// fallback try to load native load method (e.g using Directx, LoadTxtureFromFile)
            if (texture)
            {
                texture->fileName = std::move(fileNameBase);
                texture->useAlphaChannel = hasAlpha ? true : false;
                lsTextures[texture->fileName] = texture;
            }
            else
            {
                PRINT_IF_DEBUG("failed to load texture: %s.", fileName);
            }
        }
        return texture;
    }

    INFO_GIF::INFO_GIF():totalFrames(0),widthTexture(0),heightTexture(0)
    {}

    bool TEXTURE_MANAGER::loadGIF(const char *fileNameGIF,INFO_GIF & infoGif)
    {
        infoGif.fileNames.clear();
        infoGif.interval.clear();
        infoGif.totalFrames     = 0;
        infoGif.widthTexture    = 0;
        infoGif.heightTexture   = 0;
        if (!fileNameGIF)
            return false;
        std::string fileNameBase = util::getBaseName(fileNameGIF);
        char num[11] = "";
        snprintf(num,sizeof(num),"%d",1);
        std::string firstNameGif(num);
        firstNameGif += "-";
        firstNameGif += fileNameBase;
        TEXTURE * texture = lsTextures[firstNameGif];
        if (texture)
        {
            infoGif.widthTexture = texture->width;
            infoGif.heightTexture = texture->height;
            std::string nextNameGif = std::move(firstNameGif);
            int i = 2;
            do
            {
                infoGif.fileNames.push_back(nextNameGif);
                const uint32_t len  = nextNameGif.size();
                const int16_t mydelay = len == (texture->fileName.size() - 3) ?  * reinterpret_cast<int16_t*>(&texture->fileName[len+1]) : 1;
                float fdelay = static_cast<float>(mydelay) * 0.01f;
                infoGif.interval.push_back(fdelay);
                snprintf(num,sizeof(num),"%d",i);
                nextNameGif = num;
                nextNameGif += "-";
                nextNameGif += fileNameBase;
                texture = lsTextures[nextNameGif];
                ++i;
            }while(texture);
            
            infoGif.totalFrames = infoGif.fileNames.size();
            return infoGif.totalFrames > 0;
        }
        int w = 0;
        int h = 0;
        int frames = 0;
        
        const char *fileNameFullPath = getFilePathTexture(fileNameGIF,nullptr);
        uint8_t * img = stbi_xload(fileNameFullPath,&w,&h,&frames);
        if(img != nullptr && w > 0 && h > 0)
        {
            const uint32_t size = (4 * w * h);
            for(int i=0; i< frames; ++i)
            {
                const uint32_t start = (size + 2) * i;
                texture  = new TEXTURE();
                uint8_t* from = &img[start];
                infoGif.widthTexture  = static_cast<uint32_t>(w);
                infoGif.heightTexture = static_cast<uint32_t>(h);
                if(texture->loadFromData(from,w,h,8,4,true))
                {
                    const int16_t mydelay = * reinterpret_cast<int16_t*>(&img[start+size]);
                    float fdelay = static_cast<float>(mydelay) * 0.01f;
                    infoGif.interval.push_back(fdelay);
                    char num_2[12] = "";
                    snprintf(num_2,sizeof(num_2)-1,"%d",i+1);
                    std::string newNameGif(num_2);
                    newNameGif += "-";
                    newNameGif += fileNameBase;

                    texture->fileName = newNameGif;
                    const int len = texture->fileName.size();
                    texture->fileName += "   ";
                    texture->fileName[len+1] = img[start+size];
                    texture->fileName[len+2] = img[start+size+1];
                    texture->fileName[len]   = 0;
                    texture->useAlphaChannel = true;
                    lsTextures[newNameGif]     = (texture);
                    infoGif.fileNames.emplace_back(newNameGif);
                }
                else
                {
                    delete texture;
                    texture = nullptr;
                    free(img);
                    img = nullptr;
                    PRINT_IF_DEBUG("failed to load texture: %s.", fileNameGIF);
                }
            }
        }
        free(img);
        infoGif.totalFrames = infoGif.fileNames.size();
        return infoGif.totalFrames > 0;
    }
    
    TEXTURE * TEXTURE_MANAGER::loadTTF(const char *fileNameTTF, std::vector<stbtt_aligned_quad *> *lsStbFontOut,
                     std::vector<VEC2> *lsWidthLetterOut, const float heightLetter,const bool saveAsPng)
    {
        std::string fileNameBase = util::getBaseName(fileNameTTF);
        std::string fileNameBaseSuppose(fileNameBase);
        auto len_ = fileNameBase.find_last_of('.');
        if(len_ != std::string::npos)
        {
            char ext[50] = "";
            if(saveAsPng)
                snprintf(ext,sizeof(ext) -1,"-%2.0f.png",heightLetter);
            else
                snprintf(ext,sizeof(ext) -1,"-%2.0f.ttf",heightLetter);
            fileNameBaseSuppose.resize(len_);
            fileNameBaseSuppose += ext;
        }
        auto texture = lsTextures[fileNameBaseSuppose];
        if(texture)
            return texture;
        const char *fileNameFullPath = getFilePathTexture(fileNameTTF,nullptr);
        if (util::existFile(fileNameFullPath))
            fileNameTTF = fileNameFullPath;
        texture         = new TEXTURE();
        if (texture->loadTTF(fileNameTTF, lsStbFontOut, lsWidthLetterOut, heightLetter,saveAsPng))
        {
            texture->fileName = std::move(fileNameBaseSuppose);
            texture->useAlphaChannel = true;
            lsTextures[texture->fileName]  = texture;
        }
        else
        {
            delete texture;
            texture = nullptr;
            PRINT_IF_DEBUG("failed to load texture: %s.", fileNameTTF);
        }
        return texture;
    }
    
    bool TEXTURE_MANAGER::existTexture(const char *fileNametexture)
    {
        if (fileNametexture == nullptr)
            return false;
        TEXTURE *tex = lsTextures[fileNametexture];
        if (tex)
            return true;
        fileNametexture = util::getFullPath(fileNametexture,nullptr);
        tex             = lsTextures[fileNametexture];
        if (tex)
            return true;
        return false;
    }
    
    void TEXTURE_MANAGER::setPath(const char *PathSource)
    {
        strncpy(pathSource, PathSource,sizeof(pathSource)-1);
    }
    
    bool TEXTURE_MANAGER::saveDataAsPNG(const char *fileName, std::vector<uint8_t> &image, const uint32_t channel,
                              const uint32_t width, const uint32_t height, char *strMessageError)
    {
        unsigned                   error = 0;
        std::vector<uint8_t> png;
        error = lodepng::encode(png, image, width, height, channel == 3 ? LCT_RGB : LCT_RGBA);
        if (error)
        {
            if (strMessageError)
                sprintf(strMessageError, "PNG encoding error  [%s]", lodepng_error_text(error));
            return false;
        }
        lodepng::save_file(png, fileName);
        return true;
    }

    #if defined USE_EDITOR_FEATURES && !defined ANDROID
    static bool generateImageFromPng(const char* pngPath,
        std::vector<uint32_t>& outData, uint32_t& outWidth, uint32_t& outHeight,
        char* strMessageError)
    {
        if (!pngPath)
        {
            if (strMessageError)
                sprintf(strMessageError, "PNG path is null");
            return false;
        }
        std::vector<uint8_t> image;
        unsigned w = 0, h = 0;
        unsigned error = lodepng::decode(image, w, h, pngPath, LCT_RGBA, 8);
        if (error)
        {
            if (strMessageError)
                sprintf(strMessageError, "PNG decode error [%s]", lodepng_error_text(error));
            return false;
        }
        const uint32_t width = static_cast<uint32_t>(w);
        const uint32_t height = static_cast<uint32_t>(h);
        const size_t pixelCount = static_cast<size_t>(width) * height;
        outData.resize(pixelCount);
        for (size_t i = 0; i < pixelCount; ++i)
        {
            const size_t j = i * 4;
            const uint8_t r = image[j];
            const uint8_t g = image[j + 1];
            const uint8_t b = image[j + 2];
            const uint8_t a = image[j + 3];
            outData[i] = (static_cast<uint32_t>(r) << 24) | (static_cast<uint32_t>(g) << 16)
                | (static_cast<uint32_t>(b) << 8) | a;
        }
        outWidth = width;
        outHeight = height;
        return true;
    }
    #endif

    bool TEXTURE_MANAGER::generateImageResourceHeaderFromPng(const char* pngPath,
        const char* outputHeaderPath, const char* resourceName,
        uint32_t colorKey, char* strMessageError)
    {
        #if defined USE_EDITOR_FEATURES && !defined ANDROID
        std::vector<uint32_t> data;
        uint32_t width = 0, height = 0;
        if (!generateImageFromPng(pngPath, data, width, height, strMessageError))
            return false;
        if (!outputHeaderPath || !resourceName)
        {
            if (strMessageError)
                sprintf(strMessageError, "output path or resource name is null");
            return false;
        }
        FILE* fp = fopen(outputHeaderPath, "w");
        if (!fp)
        {
            if (strMessageError)
                sprintf(strMessageError, "failed to open output file [%s]", outputHeaderPath);
            return false;
        }
        const uint32_t size = width * height;
        fprintf(fp, "/*-----------------------------------------------------------------------------------------------------------------------|\n");
        fprintf(fp, "| MIT License (MIT)                                                                                                      |\n");
        fprintf(fp, "| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |\n");
        fprintf(fp, "|                                                                                                                        |\n");
        fprintf(fp, "| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |\n");
        fprintf(fp, "| documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation        |\n");
        fprintf(fp, "| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |\n");
        fprintf(fp, "| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |\n");
        fprintf(fp, "|                                                                                                                        |\n");
        fprintf(fp, "| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |\n");
        fprintf(fp, "| the Software.                                                                                                          |\n");
        fprintf(fp, "|                                                                                                                        |\n");
        fprintf(fp, "| THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |\n");
        fprintf(fp, "| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |\n");
        fprintf(fp, "| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |\n");
        fprintf(fp, "| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |\n");
        fprintf(fp, "|                                                                                                                        |\n");
        fprintf(fp, "|-----------------------------------------------------------------------------------------------------------------------*/\n\n");
        fprintf(fp, "#ifndef MY_RESOUCE_%s\n", resourceName);
        fprintf(fp, "#define MY_RESOUCE_%s\n\n", resourceName);
        fprintf(fp, "    #include <core_mbm/image-resource.h>\n\n");
        fprintf(fp, "    static const char * nickNameImageFromResource_%s  = \"__%s\\0\";\n", resourceName, resourceName);
        fprintf(fp, "    static const uint32_t widthImageFromResource_%s  = %u;\n", resourceName, width);
        fprintf(fp, "    static const uint32_t heightImageFromResource_%s = %u;\n", resourceName, height);
        fprintf(fp, "    static const uint32_t sizeImageFromResource_%s = %u;\n", resourceName, size);
        fprintf(fp, "    static const uint32_t alphaImageFromResource_%s = 0x%08x;\n\n", resourceName, colorKey);
        fprintf(fp, "    static const uint32_t imageFromResource_%s [] = {\n", resourceName);
        constexpr int valuesPerLine = 22;
        for (size_t i = 0; i < data.size(); ++i)
        {
            if (i > 0)
            {
                fprintf(fp, ",");
                if (i % valuesPerLine == 0)
                    fprintf(fp, "\n");
            }
            fprintf(fp, "0x%x", data[i]);
        }
        fprintf(fp, "\n};\n");
        fprintf(fp, "namespace mbm\n");
        fprintf(fp, "{\n");
        fprintf(fp, "    const IMAGE_RESOURCE resource_%s(widthImageFromResource_%s,heightImageFromResource_%s,sizeImageFromResource_%s,nickNameImageFromResource_%s,imageFromResource_%s,alphaImageFromResource_%s);\n",
            resourceName, resourceName, resourceName, resourceName, resourceName, resourceName, resourceName);
        fprintf(fp, "}\n\n");
        fprintf(fp, "#endif\n");
        fclose(fp);
        return true;
        #else
        if (strMessageError)
            sprintf(strMessageError, "editor features are not enabled");
        INFO_AT(__LINE__,__FILE__,"editor features are not enabled");
        return false;
        #endif
    }
    
    TEXTURE_MANAGER::~TEXTURE_MANAGER()
    {
        std::unordered_map<std::string, TEXTURE *>::const_iterator it;
        for (it = lsTextures.cbegin(); it != lsTextures.cend(); ++it)
        {
            TEXTURE *ptr = it->second;
            delete ptr;
        }
        lsTextures.clear();
    }
    
    const char * TEXTURE_MANAGER::getFilePathTexture(const char *fileName,const char* fullFileName)
    {
        if(fileName && fileName[0] == '#')
            return fileName;
#if defined (ANDROID)
        bool          existPath = false;
        fileName                = util::getFullPath(fileName, &existPath);
        if (!existPath)
            return fileName;
        else
            return fileName;
    }
#elif defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
        bool          existPath = false;
        fileName                = util::getFullPath(fileName, &existPath);
        if (!existPath)
        {
            if(fullFileName)
            {
                fileName                = util::getFullPath(fullFileName, &existPath);
            }
            if (!existPath)
            {
    #if defined USE_EDITOR_FEATURES
        #if (defined(_WIN32) || ((defined(__linux__) || defined(__APPLE__))) && !defined(ANDROID))
                const char * filters[] = { "*.png","*.jpeg","*.jpg","*.bmp","*.gif","*.psd","*.pic","*.pnm","*.hdr","*.tga","*.tif"};
                constexpr int sizeFilters = sizeof(filters) / sizeof(char*);
                std::string where_str("where:");
                where_str += fileName;
                const char *result = tinyfd_openFileDialog(where_str.c_str(), fileName, sizeFilters, filters, nullptr, 0);
                if (result)
                {
                    util::addPath(result);
                    fileName = result;
                }
        #endif
    #endif
            }
        }
        return fileName;
    }
    
    const char * TEXTURE_MANAGER::findInAllPaths(const char *fileNameTexture)
    {
        std::vector<std::string> result;
        util::split(result, fileNameTexture, util::getCharDirSeparator());
        if (result.size())
        {
            const char *nameTexture = result[result.size() - 1].c_str();
            std::unordered_map<std::string, TEXTURE *>::const_iterator it;
            for (it = lsTextures.cbegin(); it != lsTextures.cend(); ++it)
            {
                mbm::TEXTURE *texture = it->second;
                if (texture)
                {
                    std::string dir(texture->fileName);
                    std::string fullPathTexture;
                    std::string fullPathTexture2;
                    for (size_t j = dir.size() - 1; j; --j)
                    {
                        if (dir[j] == util::getCharDirSeparator())
                        {
                            fullPathTexture  = dir.substr(0, j);
                            fullPathTexture2 = fullPathTexture;
                            fullPathTexture2 += util::getCharDirSeparator();
                            break;
                        }
                    }
                    if (fullPathTexture.size())
                    {
                        fullPathTexture += nameTexture;
                        FILE *fp = util::openFile(fullPathTexture.c_str(), "rb");
                        if (fp)
                        {
                            static char fullPathTextureFileName[1024];
                            memset(fullPathTextureFileName, 0, sizeof(fullPathTextureFileName));
                            fclose(fp);
                            strncpy(fullPathTextureFileName, fullPathTexture.c_str(),sizeof(fullPathTextureFileName)-1);
                            return fullPathTextureFileName;
                        }
                        fullPathTexture2 += nameTexture;
                        fp = util::openFile(fullPathTexture2.c_str(), "rb");
                        if (fp)
                        {
                            static char fullPathTextureFileName[1024];
                            memset(fullPathTextureFileName, 0, sizeof(fullPathTextureFileName));
                            fclose(fp);
                            strncpy(fullPathTextureFileName, fullPathTexture2.c_str(),sizeof(fullPathTextureFileName)-1);
                            return fullPathTextureFileName;
                        }
                    }
                }
            }
        }
        return fileNameTexture;
    }
    
#else
    #error "platform not suported"
#endif

    void TEXTURE_MANAGER::getAllTexturesFullPaths(std::vector<std::string> &result)
    {
        for (auto& texture : lsTextures)
        {
            bool existTexture = false;
            std::string fullPathTexture = util::getFullPath(texture.first.c_str(), &existTexture);
            if (existTexture)
            {
                result.push_back(fullPathTexture);
            }
        }
    }

    TEXTURE_MANAGER::TEXTURE_MANAGER()
    {
        memset(pathSource, 0, sizeof(pathSource));
        this->maxTextureSize = 0;
        this->maxTextureWidth = 0;
        this->maxTextureHeight = 0;
        //Remember to implement setTextureCapabilities by engine backend
    }

    void TEXTURE_MANAGER::setTextureCapabilities(const int32_t maxTextureSizeFound, int32_t maxTextureWidthFound, int32_t maxTextureHeightFound)
    {
        this->maxTextureSize = maxTextureSizeFound;
        this->maxTextureWidth = maxTextureWidthFound;
        this->maxTextureHeight = maxTextureHeightFound;
    }

    mbm::TEXTURE_MANAGER *mbm::TEXTURE_MANAGER::instanceTextureManager = nullptr;    
}

const char* getLodePNGVersion()
{
    return LODEPNG_VERSION_STRING;
}

