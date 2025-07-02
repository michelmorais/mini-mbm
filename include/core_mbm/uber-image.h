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

#ifndef UBER_IMAGE_H
#define UBER_IMAGE_H

#include <stdio.h>
#include <miniz-wrap/miniz-wrap.h>

namespace mbm
{

    class UBER_IMG
    {
      public:
        UBER_IMG()    noexcept;
        virtual ~UBER_IMG();
        void release();
        bool load(const char *fileName);
        bool loadFromFileOpened(FILE *fp);
        bool save(const char *fileName, uint8_t *datRGB8bits, uint32_t sizeOfData, uint32_t widthImage,uint32_t heightImage, const uint16_t depthBitsDest,const uint16_t channelDest, const uint16_t fromChannel);
        uint32_t getDepth() const noexcept;
        uint32_t         getWidth() const noexcept;
        uint32_t getHeight() const noexcept;
        const uint8_t *getImage() const;
        const uint8_t *getImage8bitsPerPixel(const uint8_t *dataRGBAnotherUberimage,
                                                   uint32_t widthAnotherUberimage, 
                                                   uint32_t heightAnotherUberimage,
                                                   const uint32_t       depthAnotherUberImage,
                                                   const uint16_t channelAnotherUberImage);
    
        bool hasAlpha() const noexcept;
        uint16_t getChannel() const noexcept;
    
      private:
    
        uint32_t       width;
        uint32_t       height;
        uint16_t depth;
        uint16_t channel;
        bool               hasAlphaColor;
        uint8_t *    dataRGBfrom4Depth;
        uint8_t *    dataRGBfrom3Depth;
        mbm::MINIZ         miniz;
    
        uint8_t *getDataPixelRGBfrom4Depth(const uint8_t *dataRGB4bits, uint32_t widthImg,
                                                 uint32_t heightImg);
        uint8_t *getDataPixelARGBfrom4Depth(const uint8_t *dataARGB4bits, uint32_t widthImg,
                                                  uint32_t heightImg);
        uint8_t *getDataPixelARGBfrom3Depth(const uint8_t *dataRGB3bits, uint32_t widthImg,
                                                  uint32_t heightImg);
        uint8_t *getDataPixelRGBfrom3Depth(const uint8_t *dataRGB3bits, uint32_t widthImg,
                                                 uint32_t heightImg);// Recupera um array RGB true color(8 bits por cor)
                                                                         // de um array 3 bits por cor uberimg. (a data é
                                                                         // deletada pela classe ao ser destruida).
        bool onFail(FILE *fp, const char *message, uint8_t *newData);
        bool onFail(FILE *fp, const char *message);
    };
}

#endif
