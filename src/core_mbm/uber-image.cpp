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

#include <uber-image.h>
#include <header-mesh.h>
#include <util-interface.h>

inline float fRound(const float value) noexcept
{
    const auto nValue = static_cast<const float>(static_cast<const uint32_t>(value));
    const float rest   = value - nValue;
    if (rest >= 0.5f)
        return nValue + 1.0f;
    return nValue;
}

namespace mbm
{
    UBER_IMG::UBER_IMG() noexcept
        : width(0), height(0), depth(8), channel(3), hasAlphaColor(false), dataRGBfrom4Depth(nullptr),
          dataRGBfrom3Depth(nullptr)
    {
    }
    
    UBER_IMG::~UBER_IMG()
    {
        this->release();
    }
    
    void UBER_IMG::release()
    {
        width         = 0;
        height        = 0;
        hasAlphaColor = false;
        if (dataRGBfrom4Depth)
            delete[] dataRGBfrom4Depth;
        dataRGBfrom4Depth = nullptr;

        if (dataRGBfrom3Depth)
            delete[] dataRGBfrom3Depth;
        dataRGBfrom3Depth = nullptr;

        miniz.release();
    }
    
    bool UBER_IMG::load(const char *fileName)
    {
        util::HEADER header;
        util::HEADER_IMG headerImage;
        FILE *           fp = nullptr;
        if (fileName == nullptr)
            return onFail(fp, "file name is null!");
        this->release();
        fp = util::openFile(fileName, "rb");
        if (!fp)
            return onFail(fp, "failed to open in file!");
        if (!fread(&header, sizeof(util::HEADER), 1, fp))
            return onFail(fp, "failed to read in file!");
        if (strncmp(header.name, "mbm", 3) || strncmp(header.typeApp, "img uberimg", 11) || header.magic != 0x010203ff ||
            header.reserved != 0 || header.version != 1)
        {
            return onFail(fp, "is not uberimg!");
        }
        if (!fread(&headerImage, sizeof(util::HEADER_IMG), 1, fp))
            return onFail(fp, "failed to read HEADER_IMG!");
        if (headerImage.width == 0 || headerImage.height == 0)
            return onFail(fp, "size of image unexpected ZERO!");
        if (headerImage.lenght == 0)
            return onFail(fp, "size of compressed image unexpected ZERO!");
        auto data = new uint8_t[headerImage.lenght];
        if (!fread(data, static_cast<size_t>(headerImage.lenght), 1, fp))
        {
            return onFail(fp, "failed to read image data", data);
        }
        this->hasAlphaColor      = headerImage.hasAlpha != 0;
        this->channel            = headerImage.channel == 4 ? 4 : 3;
        uint32_t sizeOfImage = headerImage.width * headerImage.height * this->channel;
        this->depth              = headerImage.depth;
        switch (depth)
        {
            case 3:
            {
                sizeOfImage = 3 * this->channel * headerImage.width * headerImage.height;
                while (sizeOfImage % 8)
                {
                    sizeOfImage++;
                }
                sizeOfImage = sizeOfImage / 8;
            }
            break;
            case 4:
            {

                sizeOfImage = 4 * this->channel * headerImage.width * headerImage.height;
                while (sizeOfImage % 8)
                {
                    sizeOfImage++;
                }
                sizeOfImage = sizeOfImage / 8;
            }
            break;
            case 8: {
            }
            break;
            default: { return onFail(fp, "failed to read depth image data", data);}
            
        }
        if (miniz.decompressStream(data, headerImage.lenght, sizeOfImage))
        {
            width               = headerImage.width;
            height              = headerImage.height;
            this->hasAlphaColor = headerImage.hasAlpha != 0;
            switch (depth)
            {
                case 3:
                {
                    if (headerImage.channel == 4)
                    {
                        if (!this->getDataPixelARGBfrom3Depth(miniz.getDataStreamOut(), width, height))
                            return onFail(fp, "failed to get ARGB from image of 3 bits RGB!", data);
                    }
                    else
                    {
                        if (!this->getDataPixelRGBfrom3Depth(miniz.getDataStreamOut(), width, height))
                            return onFail(fp, "failed to get data RGB of image 3 bits RGB!", data);
                    }
                }
                break;
                case 4:
                {
                    if (headerImage.channel == 4)
                    {
                        if (!this->getDataPixelARGBfrom4Depth(miniz.getDataStreamOut(), width, height))
                            return onFail(fp, "failed to get data RGB of image 4 bits RGB!", data);
                    }
                    else
                    {
                        if (!this->getDataPixelRGBfrom4Depth(miniz.getDataStreamOut(), width, height))
                            return onFail(fp, "failed to get data RGB of image 4 bits RGB!", data);
                    }
                }
                break;
                case 8:
                {
                    // Do nothing
                }
                break;
                default: return onFail(fp, "impossible , however depth has changed", data);
            }
        }
        else
        {
            return onFail(fp, "failed to decompress image", data);
        }
        delete[] data;
        fclose(fp);
        return true;
    }
    
    bool UBER_IMG::loadFromFileOpened(FILE *fp)
    {
        util::HEADER_IMG headerImage;
        this->release();
        if (!fread(&headerImage, sizeof(util::HEADER_IMG), 1, fp))
            return onFail(fp, "failed to read HEADER_IMG!");
        if (headerImage.width == 0 || headerImage.height == 0)
            return onFail(fp, "size of image unexpected ZERO!");
        if (headerImage.lenght == 0)
            return onFail(fp, "size of compressed image unexpected ZERO!");
        auto data = new uint8_t[headerImage.lenght];
        if (!fread(data, static_cast<size_t>(headerImage.lenght), 1, fp))
            return onFail(fp, "failed to read image data", data);
        this->hasAlphaColor      = headerImage.hasAlpha != 0;
        this->channel            = headerImage.channel == 4 ? 4 : 3;
        uint32_t sizeOfImage = headerImage.width * headerImage.height * this->channel;
        this->depth              = headerImage.depth;
        switch (depth)
        {
            case 3:
            {
                sizeOfImage = 3 * 3 * headerImage.width * headerImage.height;
                while (sizeOfImage % 8)
                {
                    sizeOfImage++;
                }
                sizeOfImage = sizeOfImage / 8;
            }
            break;
            case 4:
            {

                sizeOfImage = 4 * 3 * headerImage.width * headerImage.height;
                while (sizeOfImage % 8)
                {
                    sizeOfImage++;
                }
                sizeOfImage = sizeOfImage / 8;
            }
            break;
            case 8: {
            }
            break;
            default: { return onFail(fp, "failed to read depth image data", data);}
        }
        if (miniz.decompressStream(data, headerImage.lenght, sizeOfImage))
        {
            width               = headerImage.width;
            height              = headerImage.height;
            this->hasAlphaColor = headerImage.hasAlpha == '1';
            switch (depth)
            {
                case 3:
                {
                    if (headerImage.channel == 4)
                    {
                        if (!this->getDataPixelARGBfrom3Depth(miniz.getDataStreamOut(), width, height))
                            return onFail(fp, "failed to get ARGB from image of 3 bits RGB!", data);
                    }
                    else
                    {
                        if (!this->getDataPixelRGBfrom3Depth(miniz.getDataStreamOut(), width, height))
                            return onFail(fp, "failed to get data RGB of image 3 bits RGB!", data);
                    }
                }
                break;
                case 4:
                {
                    if (headerImage.channel == 4)
                    {
                        if (!this->getDataPixelARGBfrom4Depth(miniz.getDataStreamOut(), width, height))
                            return onFail(fp, "failed to get data RGB of image 4 bits RGB!", data);
                    }
                    else
                    {
                        if (!this->getDataPixelRGBfrom4Depth(miniz.getDataStreamOut(), width, height))
                            return onFail(fp, "failed to get data RGB of image 4 bits RGB!", data);
                    }
                }
                break;
                case 8:
                {
                    // Do nothing
                }
                break;
                default: return onFail(fp, "impossible , however depth has changed", data);
            }
        }
        else
        {
            return onFail(fp, "failed to decompress image", data);
        }
        delete[] data;
        return true;
    }
    
    bool UBER_IMG::save(const char *fileName, uint8_t *datRGB8bits, uint32_t sizeOfData, uint32_t widthImage,
                     uint32_t heightImage, const uint16_t depthBitsDest,
                     const uint16_t channelDest, const uint16_t fromChannel)
    {
        uint8_t *newData = nullptr;
        if (widthImage == 0 || heightImage == 0 || sizeOfData == 0 || datRGB8bits == nullptr || fileName == nullptr)
            return false;
        if ((widthImage * heightImage * fromChannel) != sizeOfData)
            return false;
        if (channelDest != fromChannel)
        {
            PRINT_IF_DEBUG("Not implemented channelDest != fromChannel");
            return false;
        }
        util::HEADER header("img uberimg", 1);
        util::HEADER_IMG headerImage;
        headerImage.r        = 0;
        headerImage.g        = 0;
        headerImage.b        = 0;
        headerImage.hasAlpha = channelDest == 4;
        headerImage.depth    = depthBitsDest;
        headerImage.channel  = channelDest;
        headerImage.width    = widthImage;
        headerImage.height   = heightImage;
        this->channel        = channelDest;
        this->release();
        switch (depthBitsDest)
        {
            case 3: // 3 bits por cor
            {
                uint32_t newSize = 3 * this->channel * widthImage * heightImage;
                while (newSize % 8)
                {
                    newSize++;
                }
                newSize             = newSize / 8;
                newData             = new uint8_t[newSize];
                uint32_t index  = 0;
                const float  _1_255 = 1.0f / 255.0f;
                if (this->channel == 4) // assumimos que dest channel e source channel são iguais
                {
                    uint8_t _0_5 = 0;
                    for (uint32_t i = 0; i < sizeOfData; i += 4)
                    {
                        const auto a = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i])) * 7);
                        const auto r = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i + 1])) * 7);
                        const auto g = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i + 2])) * 7);
                        const auto b = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i + 3])) * 7);
                        switch (_0_5++)
                        {
                            case 0: // ARGB ok --------------
                            {
                                // 224  1110-000
                                newData[index] = static_cast<uint8_t>(a << 5);
                                // 28   0001-1100
                                newData[index] = static_cast<uint8_t>((r << 2) | newData[index]);
                                // 3        0000-0011
                                newData[index] = static_cast<uint8_t>(((g >> 1) & 3) | newData[index]);
                                index++;
                                // 128  1000-000
                                newData[index] = static_cast<uint8_t>((g & 1) << 7);
                                // 112  0111-0000
                                newData[index] = static_cast<uint8_t>((b << 4) | newData[index]);
                            }
                            break;
                            case 1: // ARGB ok --------------
                            {
                                // 14   0000-1110
                                newData[index] = static_cast<uint8_t>((a << 1) | newData[index]);
                                // 1        0000-0001
                                newData[index] = static_cast<uint8_t>(((r >> 2) & 1) | newData[index]);
                                index++;
                                // 192  1100-0000
                                newData[index] = static_cast<uint8_t>(((r << 6) & 192));
                                // 56   0011-1000
                                newData[index] = static_cast<uint8_t>((g << 3) | newData[index]);
                                // 7    0000-0111
                                newData[index] = static_cast<uint8_t>(b | newData[index]);
                                index++;
                            }
                            break;
                            case 2: // ARGB ok --------------
                            {
                                // 224  1110-0000
                                newData[index] = static_cast<uint8_t>(a << 5);
                                // 28   0001-1100
                                newData[index] = static_cast<uint8_t>((r << 2) | newData[index]);
                                // 3        0000-0011
                                newData[index] = static_cast<uint8_t>(((g >> 1) & 3) | newData[index]);
                                index++;
                                // 128  1000-000
                                newData[index] = static_cast<uint8_t>((g & 1) << 7);
                                // 112  0111-0000
                                newData[index] = static_cast<uint8_t>((b << 4) | newData[index]);
                            }
                            break;
                            case 3: // ARGB ok --------------
                            {
                                // 14   0000-1110
                                newData[index] = static_cast<uint8_t>((a << 1) | newData[index]);
                                // 1        0000-0001
                                newData[index] = static_cast<uint8_t>(((r >> 2) & 1) | newData[index]);
                                index++;
                                // 192  1100-0000
                                newData[index] = static_cast<uint8_t>(((r << 6) & 192));
                                // 56   0011-1000
                                newData[index] = static_cast<uint8_t>((g << 3) | newData[index]);
                                // 7    0000-0111
                                newData[index] = static_cast<uint8_t>(b | newData[index]);
                                index++;
                            }
                            break;
                            case 4: // ARGB
                            {
                                // 224  1110-000
                                newData[index] = static_cast<uint8_t>(a << 5);
                                // 28   0001-1100
                                newData[index] = static_cast<uint8_t>((r << 2) | newData[index]);
                                // 3        0000-0011
                                newData[index] = static_cast<uint8_t>(((g >> 1) & 3) | newData[index]);
                                index++;
                                // 128  1000-000
                                newData[index] = static_cast<uint8_t>((g & 1) << 7);
                                // 112  0111-0000
                                newData[index] = static_cast<uint8_t>((b << 4) | newData[index]);
                            }
                            break;
                            case 5: // ARGB
                            {
                                // 14   0000-1110
                                newData[index] = static_cast<uint8_t>((a << 1) | newData[index]);
                                // 1        0000-0001
                                newData[index] = static_cast<uint8_t>(((r >> 2) & 1) | newData[index]);
                                index++;
                                // 192  1100-0000
                                newData[index] = static_cast<uint8_t>(((r << 6) & 192));
                                // 56   0011-1000
                                newData[index] = static_cast<uint8_t>((g << 3) | newData[index]);
                                // 7    0000-0111
                                newData[index] = static_cast<uint8_t>(b | newData[index]);
                                index++;
                                _0_5 = 0;
                            }
                            break;
                            default:
                            {
                                PRINT_IF_DEBUG("\n\nerror logic...");
                                delete [] newData;
                                return false;
                            }
                        }
                    }
                }
                else
                {
                    uint8_t _0_7 = 0;
                    for (uint32_t i = 0; i < sizeOfData; i += 3)
                    {
                        const auto r = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i])) * 7);
                        const auto g = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i + 1])) * 7);
                        const auto b = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i + 2])) * 7);
                        switch (_0_7++)
                        {
                            case 0: // RGB ok --------------
                            {
                                // 224  1110-000
                                newData[index] = static_cast<uint8_t>(r << 5);
                                // 28   0001-1100
                                newData[index] = static_cast<uint8_t>((g << 2) | newData[index]);
                                // 3        0000-0011
                                newData[index] = static_cast<uint8_t>(((b >> 1) & 3) | newData[index]);
                                index++;
                                // 128  1000-000
                                newData[index] = static_cast<uint8_t>((b & 1) << 7);
                            }
                            break;
                            case 1: // RGB ok --------------
                            {
                                // 112  0111-0000
                                newData[index] = static_cast<uint8_t>((r << 4) | newData[index]);
                                // 14   0000-1110
                                newData[index] = static_cast<uint8_t>((g << 1) | newData[index]);
                                // 1        0000-0001
                                newData[index] = static_cast<uint8_t>(((b >> 2) & 1) | newData[index]);
                                index++;
                                // 192  1100-0000
                                newData[index] = static_cast<uint8_t>(((b << 6) & 192));
                            }
                            break;
                            case 2: // RGB ok --------------
                            {
                                // 56   0011-1000
                                newData[index] = static_cast<uint8_t>((r << 3) | newData[index]);
                                // 7    0000-0111
                                newData[index] = static_cast<uint8_t>(g | newData[index]);
                                index++;
                                // 224  1110-0000
                                newData[index] = static_cast<uint8_t>(b << 5);
                            }
                            break;
                            case 3: // RGB ok --------------
                            {
                                // 28   0001-1100
                                newData[index] = static_cast<uint8_t>((r << 2) | newData[index]);
                                // 3        0000-0011
                                newData[index] = static_cast<uint8_t>(((g >> 1) & 3) | newData[index]);
                                index++;
                                // 128  1000-000
                                newData[index] = static_cast<uint8_t>((g & 1) << 7);
                                // 112  0111-0000
                                newData[index] = static_cast<uint8_t>((b << 4) | newData[index]);
                            }
                            break;
                            case 4: // RGB ok --------------
                            {
                                // 14   0000-1110
                                newData[index] = static_cast<uint8_t>((r << 1) | newData[index]);
                                // 1        0000-0001
                                newData[index] = static_cast<uint8_t>(((g >> 2) & 1) | newData[index]);
                                index++;
                                // 192  1100-0000
                                newData[index] = static_cast<uint8_t>(((g << 6) & 192));
                                // 56   0011-1000
                                newData[index] = static_cast<uint8_t>((b << 3) | newData[index]);
                            }
                            break;
                            case 5: // RGB
                            {
                                // 7    0000-0111
                                newData[index] = static_cast<uint8_t>(r | newData[index]);
                                index++;
                                // 224  1110-000
                                newData[index] = static_cast<uint8_t>(g << 5);
                                // 28   0001-1100
                                newData[index] = static_cast<uint8_t>((b << 2) | newData[index]);
                            }
                            break;
                            case 6: // RGB
                            {
                                // 3        0000-0011
                                newData[index] = static_cast<uint8_t>(((r >> 1) & 3) | newData[index]);
                                index++;
                                // 128  1000-000
                                newData[index] = static_cast<uint8_t>((r & 1) << 7);
                                // 112  0111-0000
                                newData[index] = static_cast<uint8_t>((g << 4) | newData[index]);
                                // 14   0000-1110
                                newData[index] = static_cast<uint8_t>((b << 1) | newData[index]);
                            }
                            break;
                            case 7: // RGB
                            {
                                // 1        0000-0001
                                newData[index] = static_cast<uint8_t>(((r >> 2) & 1) | newData[index]);
                                index++;
                                // 192  1100-0000
                                newData[index] = static_cast<uint8_t>(((r << 6) & 192));
                                // 56   0011-1000
                                newData[index] = static_cast<uint8_t>((g << 3) | newData[index]);
                                // 7    0000-0111
                                newData[index] = static_cast<uint8_t>(b | newData[index]);
                                index++;
                                _0_7 = 0;
                            }
                            break;
                            default:
                            {
                                PRINT_IF_DEBUG("\n\nerror logic");
                                delete [] newData;
                                return false;
                            }
                        }
                    }
                }
                datRGB8bits = newData;
                sizeOfData  = newSize;
            }
            break;
            case 4: // 4 bits por cor
            {
                uint32_t newSize = 4 * this->channel * widthImage * heightImage;
                while (newSize % 8)
                {
                    newSize++;
                }
                newSize             = newSize / 8;
                newData             = new uint8_t[newSize];
                uint32_t index  = 0;
                const float  _1_255 = 1.0f / 255.0f;
                if (this->channel == 4) // assumimos que dest channel e source channel são iguais
                {
                    for (uint32_t i = 0; i < sizeOfData; i += 4, index += 2)
                    {
                        /*
                        Organizacao argb de 4 bits
                        [byte 1]    [byte 2]
                         a -  r  -   g -  b
                        1111-0010   1010-0101
                        */
                        const auto a = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i])) * 0xf);
                        const auto r = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i + 1])) * 0xf);
                        const auto g = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i + 2])) * 0xf);
                        const auto b = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i + 3])) * 0xf);
                        newData[index]        = static_cast<uint8_t>(((a << 4) | r));
                        newData[index + 1]    = static_cast<uint8_t>(((g << 4) | b));
                    }
                }
                else
                {
                    for (uint32_t i = 0; i < sizeOfData; i += 3)
                    {
                        /*
                        Organizacao rgb de 4 bits
                          r -  g -  b  - r  - g - b
                        1111-0010 1010-0101 1100-0011
                        */
                        const auto r = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i])) * 0xf);
                        const auto g = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i + 1])) * 0xf);
                        const auto b = static_cast<uint8_t>((_1_255 * static_cast<float>(datRGB8bits[i + 2])) * 0xf);
                        if (i % 2) // impar r:0xf(15) g:0xf0(240) b:0xf
                        {
                            newData[index] = static_cast<uint8_t>((r | newData[index]));
                            index++;
                            newData[index] = static_cast<uint8_t>(((g << 4) | b));
                            index++;
                        }
                        else // par r:0xf0 g:0xf b:0xf0
                        {
                            newData[index] = static_cast<uint8_t>(((r << 4) | g));
                            index++;
                            newData[index] = static_cast<uint8_t>((b << 4));
                        }
                    }
                }
                datRGB8bits = newData;
                sizeOfData  = newSize;
            }
            break;
            case 8: // 8 bits por cor (imagem original)
            {
                // do nothing
            }
            break;
            default: { return onFail(nullptr, "Depth not compatible!", newData);
            }
        }
        if (miniz.compressStream(datRGB8bits, sizeOfData, MZ_UBER_COMPRESSION))
        {
            FILE *fp = util::openFile(fileName, "wb");
            if (!fp)
                return onFail(fp, "failed to open file de saida!", newData);
            if (!fwrite(&header, sizeof(util::HEADER), 1, fp))
                return onFail(fp, "failed to write to file!", newData);
            headerImage.lenght = miniz.getSizeDataStreamOut();
            if (!headerImage.lenght)
                return onFail(fp, "size of compressed image is ZERO!", newData);
            if (!fwrite(&headerImage, sizeof(util::HEADER_IMG), 1, fp))
                return onFail(fp, "failed to write to file!", newData);
            if (!fwrite(miniz.getDataStreamOut(), miniz.getSizeDataStreamOut(), 1, fp))
                return onFail(fp, "Falha ao escrever a imagem no arquivo de saida!", newData);
            if (EOF == fclose(fp))
                return onFail(nullptr, "failed to write to file! Disco Cheio!!!", newData);
        }
        else
        {
            return onFail(nullptr, "failed to copress image!", newData);
        }
        if (newData)
            delete[] newData;
        return true;
    }
    
    uint32_t UBER_IMG::getDepth() const noexcept
    {
        return depth;
    }
    
    uint32_t UBER_IMG::getWidth() const noexcept
    {
        return width;
    }
    
    uint32_t UBER_IMG::getHeight() const noexcept
    {
        return height;
    }
    
    const uint8_t * UBER_IMG::getImage() const
    {
        if (dataRGBfrom4Depth)
            return dataRGBfrom4Depth;
        else if (dataRGBfrom3Depth)
            return dataRGBfrom3Depth;
        return miniz.getDataStreamOut();
    }
    
    const uint8_t * UBER_IMG::getImage8bitsPerPixel(const uint8_t *dataRGBAnotherUberimage,
                                               uint32_t widthAnotherUberimage, uint32_t heightAnotherUberimage,
                                               const uint32_t       depthAnotherUberImage,
                                               const uint16_t channelAnotherUberImage)
    {
        switch (depthAnotherUberImage)
        {
            case 3:
            {
                if (channelAnotherUberImage == 4)
                    return this->getDataPixelARGBfrom3Depth(dataRGBAnotherUberimage, widthAnotherUberimage,
                                                            heightAnotherUberimage);
                return this->getDataPixelRGBfrom3Depth(dataRGBAnotherUberimage, widthAnotherUberimage,
                                                       heightAnotherUberimage);
            }
            case 4:
            {
                if (channelAnotherUberImage == 4)
                    return this->getDataPixelARGBfrom4Depth(dataRGBAnotherUberimage, widthAnotherUberimage,
                                                            heightAnotherUberimage);
                return this->getDataPixelRGBfrom4Depth(dataRGBAnotherUberimage, widthAnotherUberimage,
                                                       heightAnotherUberimage);
            }
            case 8: { return dataRGBAnotherUberimage;}
            default: return nullptr;
        }
    }
    
    bool UBER_IMG::hasAlpha() const noexcept
    {
        return hasAlphaColor;
    }
    
    uint16_t UBER_IMG::getChannel() const noexcept
    {
        return this->channel;
    }
    
    uint8_t * UBER_IMG::getDataPixelRGBfrom4Depth(const uint8_t *dataRGB4bits, uint32_t widthImg,
                                             uint32_t heightImg) // Recupera um array RGB true color(8 bits por cor)
                                                                     // de um array 4 bits uberimg. (a data é deletada
                                                                     // pela classe ao ser destruida).
    {
        if (dataRGB4bits == nullptr || widthImg == 0 || heightImg == 0)
        {
#if defined _DEBUG
            PRINT_IF_DEBUG("dataRGB4bits == nullptr || widthImg == 0 || heightImg == 0");
#endif
            return nullptr;
        }
        uint32_t sizeOfDataOut = 3 * widthImg * heightImg;
        if (dataRGBfrom4Depth)
            delete[] dataRGBfrom4Depth;
        dataRGBfrom4Depth  = new uint8_t[sizeOfDataOut];
        uint32_t index = 0;
        // 17 * 15 = 255;
        for (uint32_t i = 0; i < sizeOfDataOut; i += 3)
        {
            /*
            Organizacao rgb de 4 bits
              r -  g -  b  - r  - g - b
            1111-0010 1010-0101 1100-0011
            */
            if (i % 2) // impar r:0xf(15) g:0xf0(240) b:0xf
            {
                dataRGBfrom4Depth[i] = ((dataRGB4bits[index] & 15)) * 17; // r
                index++;
                dataRGBfrom4Depth[i + 1] = ((dataRGB4bits[index] & 240) >> 4) * 17; // g
                dataRGBfrom4Depth[i + 2] = ((dataRGB4bits[index] & 15)) * 17;       // b
                index++;
            }
            else // par r:0xf0 g:0xf b:0xf0
            {
                dataRGBfrom4Depth[i]     = ((dataRGB4bits[index] & 240) >> 4) * 17; // r
                dataRGBfrom4Depth[i + 1] = ((dataRGB4bits[index] & 15)) * 17;       // g
                index++;
                dataRGBfrom4Depth[i + 2] = ((dataRGB4bits[index] & 240) >> 4) * 17; // b
            }
        }
        return dataRGBfrom4Depth;
    }
    
    uint8_t * UBER_IMG::getDataPixelARGBfrom4Depth(const uint8_t *dataARGB4bits, uint32_t widthImg,
                                              uint32_t heightImg) // Recupera um array ARGB true color(8 bits por cor)
                                                                      // de um array 4 bits uberimg. (a data é deletada
                                                                      // pela classe ao ser destruida).
    {
        if (dataARGB4bits == nullptr || widthImg == 0 || heightImg == 0)
        {
            PRINT_IF_DEBUG("dataARGB4bits == nullptr || widthImg == 0 || heightImg == 0 ");
            return nullptr;
        }
        uint32_t sizeOfDataOut = 4 * widthImg * heightImg;
        if (dataRGBfrom4Depth)
            delete[] dataRGBfrom4Depth;
        dataRGBfrom4Depth  = new uint8_t[sizeOfDataOut];
        uint32_t index = 0;
        // 17 * 15 = 255;
        for (uint32_t i = 0; i < sizeOfDataOut; i += 4, index += 2)
        {
            /*
            Organizacao rgb de 4 bits
            [byte 1]  [byte 2]
            a -  r  -  g -  b
            1111-0010 1010-0101 1100-0011
            */
            dataRGBfrom4Depth[i]     = ((dataARGB4bits[index] & 240) >> 4) * 17;     // a 00001111
            dataRGBfrom4Depth[i + 1] = ((dataARGB4bits[index] & 15)) * 17;           // r 11110000
            dataRGBfrom4Depth[i + 2] = ((dataARGB4bits[index + 1] & 240) >> 4) * 17; // g 00001111
            dataRGBfrom4Depth[i + 3] = ((dataARGB4bits[index + 1] & 15)) * 17;       // b 11110000
        }
        return dataRGBfrom4Depth;
    }
    
    uint8_t * UBER_IMG::getDataPixelARGBfrom3Depth(const uint8_t *dataRGB3bits, uint32_t widthImg,
                                              uint32_t heightImg) // Recupera um array RGB true color(8 bits por cor)
                                                                      // de um array 3 bits por cor uberimg. (a data é
                                                                      // deletada pela classe ao ser destruida).
    {
        if (dataRGB3bits == nullptr || widthImg == 0 || heightImg == 0)
        {
            PRINT_IF_DEBUG("dataRGB3bits == nullptr || widthImg == 0 || heightImg == 0 ");
            return nullptr;
        }
        uint32_t sizeOfDataOut = 4 * widthImg * heightImg;
        if (dataRGBfrom3Depth)
            delete[] dataRGBfrom3Depth;
        dataRGBfrom3Depth   = new uint8_t[sizeOfDataOut];
        uint32_t  index = 0;
        const float   perc  = 255.0f / 7.0f;
        uint8_t _0_5  = 0;
        for (uint32_t i = 0; i < sizeOfDataOut; i += 4)
        {
            switch (_0_5++)
            {
                case 0: // ARGB
                {
                    // 224  1110-0000
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 5) & 7)    ) * perc));
                    // 28   0001-1100
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 2) & 7) ) * perc));
                    // 3        0000-0011
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound(
                        (static_cast<float>(((dataRGB3bits[index] << 1) | (dataRGB3bits[index + 1] >> 7)) & 7)) * perc));
                    index++;
                    // 112  0111-0000
                    dataRGBfrom3Depth[i + 3] = static_cast<uint8_t>(fRound(static_cast<float>((dataRGB3bits[index] >> 4) & 7) * perc));
                }
                break;
                case 1: // ARGB
                {
                    // 14   0000-1110
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 1) & 7)) * perc));
                    // 1        0000-0001
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound(
                        (static_cast<float>(((dataRGB3bits[index] & 1) << 2) | ((dataRGB3bits[index + 1] >> 6) & 3))) * perc));
                    index++;
                    // 192  1100-0000
                    // 56   0011-1000
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 3) & 7)) * perc));
                    // 7        0000-0111
                    dataRGBfrom3Depth[i + 3] = static_cast<uint8_t>(fRound((static_cast<float>(dataRGB3bits[index] & 7)) * perc));
                    index++;
                }
                break;
                case 2: // ARGB ok --------------
                {
                    // 224  1110-0000
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 5) & 7)) * perc));
                    // 28   0001-1100
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 2) & 7)) * perc));
                    // 3        0000-0011
                    // 128  1000-000
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound(
                        (static_cast<float>(((dataRGB3bits[index] << 1) | (dataRGB3bits[index + 1] >> 7)) & 7)) * perc));
                    index++;
                    // 112  0111-0000
                    dataRGBfrom3Depth[i + 3] = static_cast<uint8_t>(fRound(static_cast<float>((dataRGB3bits[index] >> 4) & 7) * perc));
                }
                break;
                case 3: // ARGB ok --------------
                {
                    // 14   0000-1110
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 1) & 7)) * perc));
                    // 1        0000-0001
                    // 192  1100-0000 chek
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound(
                        (static_cast<float>((dataRGB3bits[index] << 2 | dataRGB3bits[index + 1] >> 6) & 7)) * perc));
                    index++;
                    // 56   0011-1000
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 3) & 7)) * perc));
                    // 7        0000-0111
                    dataRGBfrom3Depth[i + 3] = static_cast<uint8_t>(fRound((static_cast<float>(dataRGB3bits[index] & 7)) * perc));
                    index++;
                }
                break;
                case 4: // ARGB
                {
                    // 224  1110-0000
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 5) & 7)) * perc));
                    // 28   0001-1100
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 2) & 7)) * perc));
                    // 3        0000-0011
                    // 128  1000-000
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound(
                        (static_cast<float>(((dataRGB3bits[index] << 1) | (dataRGB3bits[index + 1] >> 7)) & 7)) * perc));
                    index++;
                    // 112  0111-0000
                    dataRGBfrom3Depth[i + 3] = static_cast<uint8_t>(fRound(static_cast<float>((dataRGB3bits[index] >> 4) & 7) * perc));
                }
                break;
                case 5: // ARGB
                {
                    // 14   0000-1110
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 1) & 7)) * perc));
                    // 1        0000-0001
                    // 192  1100-0000
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound(
                        (static_cast<float>(((dataRGB3bits[index] & 1) << 2) | ((dataRGB3bits[index + 1] >> 6) & 3))) * perc));
                    index++;
                    // 56   0011-1000
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 3) & 7)) * perc));
                    // 7    0000-0111
                    dataRGBfrom3Depth[i + 3] = static_cast<uint8_t>(fRound((static_cast<float>(dataRGB3bits[index] & 7)) * perc));
                    index++;
                    _0_5 = 0;
                }
                break;
                default:
                {
                    PRINT_IF_DEBUG("\n\nerror logic!");
                    return nullptr;
                }
            }
        }
        return dataRGBfrom3Depth;
    }
    
    uint8_t * UBER_IMG::getDataPixelRGBfrom3Depth(const uint8_t *dataRGB3bits, uint32_t widthImg,
                                             uint32_t heightImg) // Recupera um array RGB true color(8 bits por cor)
                                                                     // de um array 3 bits por cor uberimg. (a data é
                                                                     // deletada pela classe ao ser destruida).
    {
        if (dataRGB3bits == nullptr || widthImg == 0 || heightImg == 0)
        {
#if defined _DEBUG
            PRINT_IF_DEBUG("dataRGB3bits == nullptr || widthImg == 0 || heightImg == 0 ");
#endif
            return nullptr;
        }
        uint32_t sizeOfDataOut = 3 * widthImg * heightImg;
        if (dataRGBfrom3Depth)
            delete[] dataRGBfrom3Depth;
        dataRGBfrom3Depth   = new uint8_t[sizeOfDataOut];
        uint32_t  index = 0;
        const float   perc  = 255.0f / 7.0f;
        uint8_t _0_7  = 0;
        for (uint32_t i = 0; i < sizeOfDataOut; i += 3)
        {
            switch (_0_7++)
            {
                case 0: // RGB
                {
                    // 224  1110-0000
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 5) & 7)) * perc));
                    // 28   0001-1100
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 2) & 7)) * perc));
                    // 3        0000-0011
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound(
                        (static_cast<float>(((dataRGB3bits[index] << 1) | (dataRGB3bits[index + 1] >> 7)) & 7)) * perc));
                    index++;
                }
                break;
                case 1: // RGB
                {
                    // 112  0111-0000
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound(static_cast<float>((dataRGB3bits[index] >> 4) & 7) * perc));
                    // 14   0000-1110
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 1) & 7)) * perc));
                    // 1        0000-0001
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound(
                        (static_cast<float>(((dataRGB3bits[index] & 1) << 2) | ((dataRGB3bits[index + 1] >> 6) & 3))) * perc));
                    index++;
                    // 192  1100-0000
                }
                break;
                case 2: // RGB
                {
                    // 56   0011-1000
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 3) & 7)) * perc));
                    // 7        0000-0111
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound((static_cast<float>(dataRGB3bits[index] & 7)) * perc));
                    index++;
                    // 224  1110-0000
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 5) & 7)) * perc));
                }
                break;
                case 3: // RGB ok --------------
                {
                    // 28   0001-1100
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 2) & 7)) * perc));
                    // 3        0000-0011
                    // 128  1000-000
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound(
                        (static_cast<float>(((dataRGB3bits[index] << 1) | (dataRGB3bits[index + 1] >> 7)) & 7)) * perc));
                    index++;
                    // 112  0111-0000
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound(static_cast<float>((dataRGB3bits[index] >> 4) & 7) * perc));
                }
                break;
                case 4: // RGB ok --------------
                {
                    // 14   0000-1110
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 1) & 7)) * perc));
                    // 1        0000-0001
                    // 192  1100-0000 chek
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound(
                        (static_cast<float>((dataRGB3bits[index] << 2 | dataRGB3bits[index + 1] >> 6) & 7)) * perc));
                    index++;
                    // 56   0011-1000
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 3) & 7)) * perc));
                }
                break;
                case 5: // RGB
                {
                    // 7        0000-0111
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound((static_cast<float>(dataRGB3bits[index] & 7)) * perc));
                    index++;
                    // 224  1110-0000
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 5) & 7)) * perc));
                    // 28   0001-1100
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 2) & 7)) * perc));
                }
                break;
                case 6: // RGB
                {
                    // 3        0000-0011
                    // 128  1000-000
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound(
                        (static_cast<float>(((dataRGB3bits[index] << 1) | (dataRGB3bits[index + 1] >> 7)) & 7)) * perc));
                    index++;
                    // 112  0111-0000
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound(static_cast<float>((dataRGB3bits[index] >> 4) & 7) * perc));
                    // 14   0000-1110
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 1) & 7)) * perc));
                }
                break;
                case 7: // RGB
                {
                    // 1        0000-0001
                    // 192  1100-0000
                    dataRGBfrom3Depth[i] = static_cast<uint8_t>(fRound(
                        (static_cast<float>(((dataRGB3bits[index] & 1) << 2) | ((dataRGB3bits[index + 1] >> 6) & 3))) * perc));
                    index++;
                    // 56   0011-1000
                    dataRGBfrom3Depth[i + 1] = static_cast<uint8_t>(fRound((static_cast<float>((dataRGB3bits[index] >> 3) & 7)) * perc));
                    // 7    0000-0111
                    dataRGBfrom3Depth[i + 2] = static_cast<uint8_t>(fRound((static_cast<float>(dataRGB3bits[index] & 7)) * perc));
                    index++;
                    _0_7 = 0;
                }
                break;
                default:
                {
                    PRINT_IF_DEBUG("\n\nerror logic!");
                    return nullptr;
                }
            }
        }
        return dataRGBfrom3Depth;
    }
    
    bool UBER_IMG::onFail(FILE *fp, const char *message, uint8_t *newData)
    {
        if (fp)
            fclose(fp);
        if (newData)
            delete[] newData;
        printf("\n%s", message);
        return false;
    }
    
    bool UBER_IMG::onFail(FILE *fp, const char *message)
    {
        if (fp)
            fclose(fp);
        printf("\n%s", message);
        return false;
    }
}
