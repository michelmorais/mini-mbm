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

#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include "core-exports.h"
#include "primitives.h"
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>

#include <stb/stb-interface.h>

namespace mbm
{
    class RENDERIZABLE_TO_TARGET;
    struct IMAGE_RESOURCE;

    class TEXTURE
    {
        friend class TEXTURE_SHARED;
        friend class TEXTURE_MANAGER;
    public:
    
        API_IMPL TEXTURE() noexcept;
        API_IMPL bool isLoaded()const;
        API_IMPL virtual ~TEXTURE();
        API_IMPL void release();
        API_IMPL const char *getFileNameTexture() const noexcept;
        API_IMPL bool loadTTF(const char *fileNameTTF, std::vector<stbtt_aligned_quad *> *lsStbFontOut,std::vector<VEC2> *lsWidthLetterOut, const float heightLetter,const bool saveAsPng);
        API_IMPL bool load(const char *fileNameTexture, const bool hasColorAlpha);
		API_IMPL bool loadSolidColor(const char* colorAsString, const bool hasColorAlpha);
        API_IMPL uint32_t getWidth()const noexcept;
        API_IMPL uint32_t getHeight()const noexcept;
        API_IMPL static void enableFilter(bool value) noexcept;
        uint32_t idTexture;
        bool     useAlphaChannel;
      private:
        static bool no_filter;
        std::string  fileName;
        uint32_t width;
        uint32_t height;
    #if defined     ANDROID
        bool loadFromAndroid(const char *_fileName, const bool hasAlpha); // Android 24/32 bits true color
    #endif
        bool loadFromData(const uint8_t *data, // Bitmap or uber image
                                 const uint32_t w, const uint32_t h, const uint16_t depth,
                                 const uint16_t channel, const bool hasAlpha);
        bool loadFromResourceData(const IMAGE_RESOURCE *image);
    };

    class TEXTURE_SHARED
    {
    public:
        static TEXTURE_SHARED *getInstance();
        static void release();
        int maxTextureSize;
        char pathSource[255];
        TEXTURE_SHARED();
        std::shared_ptr<TEXTURE> createTextureRenderTarget(RENDERIZABLE_TO_TARGET *renderToTarget, const char *nickName,
                                                  const bool enableAlpha);
        std::shared_ptr<TEXTURE> load(const IMAGE_RESOURCE *imageResource);
        std::shared_ptr<TEXTURE> load(const uint32_t width, const uint32_t height, const uint8_t *data,
                             const char *nickName, const uint16_t depth, const uint16_t channel);
        std::shared_ptr<TEXTURE> load(const uint32_t width, const uint32_t height, const uint8_t *data,
                             const char *nickName, const uint16_t depth, const uint16_t channel,
                             const bool hasAlpha);
        std::shared_ptr<TEXTURE> load(const char *fileName, const bool hasAlpha);
        std::shared_ptr<TEXTURE> loadTTF(const char *fileNameTTF, std::vector<stbtt_aligned_quad *> *lsStbFontOut,
                         std::vector<VEC2> *lsWidthLetterOut, const float heightLetter);
        bool existTexture(const char *fileNametexture);
        void setPath(const char *PathSource);
        bool saveDataAsPNG(const char *fileName, std::vector<uint8_t> &image, const uint32_t channel,
                                  const uint32_t width, const uint32_t height, char *strMessageError);
    private:
        static TEXTURE_SHARED * instanceTextureShared;
        std::shared_ptr<TEXTURE> loadFromCache(const std::string &fileName);
        std::unordered_map<std::string,std::weak_ptr<TEXTURE>> cache;
    };

    struct INFO_GIF
    {
        std::vector<std::string>    fileNames;
        std::vector<float>          interval;
        uint32_t totalFrames;
        uint32_t widthTexture;
        uint32_t heightTexture;
        API_IMPL INFO_GIF();
    };


    class TEXTURE_MANAGER
    {
      public:
        API_IMPL static TEXTURE_MANAGER *getInstance();
        API_IMPL static void release();
        API_IMPL TEXTURE *createTextureRenderTarget(RENDERIZABLE_TO_TARGET *renderToTarget, const char *nickName,
                                                  const bool enableAlpha);
        API_IMPL TEXTURE *load(const IMAGE_RESOURCE *imageResource);
        API_IMPL TEXTURE *load(const uint32_t width, const uint32_t height, const uint8_t *data,
                             const char *nickName, const uint16_t depth, const uint16_t channel);
        API_IMPL TEXTURE *load(const uint32_t width, const uint32_t height, const uint8_t *data,
                             const char *nickName, const uint16_t depth, const uint16_t channel,
                             const bool hasAlpha);
        API_IMPL TEXTURE *load(const char *fileName, const bool hasAlpha);
        API_IMPL TEXTURE *loadTTF(const char *fileNameTTF, std::vector<stbtt_aligned_quad *> *lsStbFontOut,
                         std::vector<VEC2> *lsWidthLetterOut, const float heightLetter,const bool saveAsPng);
        API_IMPL bool loadGIF(const char *fileNameGIF,INFO_GIF & infoGif);
        API_IMPL bool existTexture(const char *fileNametexture);
        API_IMPL void setPath(const char *PathSource);
        API_IMPL bool saveDataAsPNG(const char *fileName, std::vector<uint8_t> &image, const uint32_t channel,
                                  const uint32_t width, const uint32_t height, char *strMessageError);
    
      private:
        static TEXTURE_MANAGER *instanceTextureManager;
        std::unordered_map<std::string,TEXTURE *> lsTextures;
        TEXTURE_MANAGER();
        virtual ~TEXTURE_MANAGER();
        const char *getFilePathTexture(const char *fileName,const char* fullFileName);
        const char *findInAllPaths(const char *fileNameTexture);
        char                     pathSource[255];
        int                      maxTextureSize;
    };
}

#endif
