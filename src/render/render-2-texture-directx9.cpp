/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2025 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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


#if defined (USE_DIRECTX9)

#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions

#include <render-2-texture.h>
#include <lodepng/lodepng.h>
#include <texture-manager.h>
#include <util-interface.h>

namespace mbm
{

    bool RENDER_2_TEXTURE::saveAsPNG(const char* newFileOutNamePNG, const int x, const int y, const int _width, const int _height)
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

        #pragma message(REMINDER_TODO "  bind texture render to texture");
        
        #pragma message(REMINDER_TODO "  read pixels from frame buffer");
        unsigned int error = 0; //TODO: set error from read pixels
        if(error)
        {
            delete [] image;
            #pragma message(REMINDER_TODO "  implement getDescriptionError");
            return false;
        }


        #pragma message(REMINDER_TODO "  unbind texture render to texture");
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
    
};
#endif // USE_DIRECTX9