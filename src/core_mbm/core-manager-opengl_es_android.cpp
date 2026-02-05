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


#if defined(USE_OPENGL_ES)
#if defined(ANDROID)

#include <core-manager.h>
#include <mesh-manager.h>
#include <texture-manager.h>
#include <device.h>
#include <specific-opengl_es.h>


namespace mbm
{
    void CORE_MANAGER::handleEventFromWindow()
    {
        // do nothing, events are handled from JNI
    }

    void CORE_MANAGER::ReleaseGraphics(const bool wasDeviceLost)
    {
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        this->device->specificContextDevice->release(wasDeviceLost);
    }

    void CORE_MANAGER::moveWindow(int , int )
    {
        // On Android, window movement is not applicable
    }

    bool CORE_MANAGER::initGraphics(const char *nameAplication, int width, int height, const int px, const int py, const bool border,const bool enable_resize)
    {
        //For ANDROID it is initialized from JAVA side
        int x = width;
        int y = height;
        this->nameAplication = nameAplication ? nameAplication : "Mini-mbm";
        GLViewport(0, 0, x <= 0 ? 800 : x, y <= 0 ? 600 : y);
        GLClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        GLDepthRangef(0.0f, 1.0f);
        GLEnable(GL_CULL_FACE);
        GLCullFace(GL_BACK);//initial value, any mesh can decide it
        GLFrontFace(GL_CW); //initial value, any mesh can decide it
        GLEnable(GL_DEPTH_TEST);
        // GLDepthFunc(GL_GREATER);
        // GLDepthFunc(GL_LESS);
        GLDepthFunc(GL_LEQUAL);
        GLClearDepthf(1.0f);
        GLEnable(GL_BLEND);
        if (x > 0)
            device->backBufferWidth = static_cast<float>(x);
        if (y > 0)
            device->backBufferHeight = static_cast<float>(y);

        TEXTURE_MANAGER* texture_manager = TEXTURE_MANAGER::getInstance();
        GLint maxTextureSize = 0;
        GLGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        //const GLint MaxTextureWidth = static_cast<GLint>(std::sqrt(static_cast<float>(maxTextureSize)));
        const GLint MaxTextureWidth = maxTextureSize;
        const GLint MaxTextureHeight = MaxTextureWidth;
        texture_manager->setTextureCapabilities(static_cast<const int32_t>(maxTextureSize), static_cast<const int32_t>(MaxTextureWidth), static_cast<const int32_t>(MaxTextureHeight));

        constexpr GLint index[2] = { GL_TEXTURE1, GL_TEXTURE0 };
        for (int i = 0; i < 2; i++)
        {
            GLActiveTexture(index[i]);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &device->specificContextDevice->filter_GL_TEXTURE_WRAP_S);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &device->specificContextDevice->filter_GL_TEXTURE_WRAP_T);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &device->specificContextDevice->filter_GL_TEXTURE_MIN_FILTER);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &device->specificContextDevice->filter_GL_TEXTURE_MAG_FILTER);

        return true;
    }
}

#endif // USE_OPENGL_ES
#endif //ANDROID
