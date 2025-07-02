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

#ifndef DEVICE_MANAGER_GLES_H
#define DEVICE_MANAGER_GLES_H

#include "core-exports.h"
#include "shader-cfg.h"
#include "order-render.h"
#include "primitives.h"
#include "frustum.h"
#include "camera.h"
#include "time-control.h"

#if defined ANDROID
    namespace util
    {
        class COMMON_JNI;
    };
#elif defined __linux__
    #include <X11/Xlib.h>
    #include <EGL/egl.h>
#elif defined _WIN32
    #include <plusWindows/plusWindows.h>
#endif

namespace mbm
{
    class CONTROL_SCENE;
    class COMMON_DEVICE;
    class CORE_MANAGER;
    class SCENE;
    class AUDIO_INTERFACE;
	class AUDIO_MANAGER_INTERFACE;
    class PHYSICS;
    class RENDERIZABLE;
    class RENDERIZABLE_TO_TARGET;
    struct DYNAMIC_VAR;

    
    class DEVICE : public TIME_CONTROL, public FRUSTUM
    {
        friend class RENDERIZABLE;
        friend class CORE_MANAGER;
      public:
		bool   verbose;
        bool   run;
        bool   bOnErrorStopScript;
        float  backBufferWidth;
        float  backBufferHeight;
        COLOR  colorClearBackGround;
        CAMERA camera;
    
        uint32_t      totalObjectsOnFrustum3D;
        uint32_t      totalObjectsOnFrustum2D;
        uint32_t      totalObjectsIsRendering3D;
        uint32_t      totalObjectsIsRendering2D;
        uint32_t      totalObjects3D;
        uint32_t      totalObjects2D;
        SHADER_CFG_LOADER cfg;                       // CFG files
    #ifdef _WIN32
        WINDOW window;
    #endif
        std::map<std::string, DYNAMIC_VAR *> lsDynamicVarGlobal;
        VEC3                dimFarFrustum3d, dimNearFrustum3d;
        CORE_MANAGER *      ptrManager;
        SCENE *             scene;
        bool                clearBackGround;
		API_IMPL static int	returnCodeApp;
        mbm::ORDER_RENDER   orderRender;
        int                 __swapBackBufferStep;
        API_IMPL static DEVICE *     getInstance();

    #ifdef ANDROID
        util::COMMON_JNI *jni;
        void callQuitInJava();
        void streamStopped(const int indexJNI);
    #endif
        API_IMPL static void quit();
        API_IMPL float getBackBufferWidth() const noexcept;
        API_IMPL float getBackBufferHeight() const noexcept;
        API_IMPL float getScaleBackBufferWidth() const noexcept;
        API_IMPL float getScaleBackBufferHeight() const noexcept;
        API_IMPL void scaleToScreen(const float widthScreen, const float heightScreen,const char *stretch) noexcept; // stretch: x, y xy nullptr
        API_IMPL void pauseGame();
        API_IMPL void resumeGame();
        API_IMPL void addPhysics(PHYSICS *physics);
        API_IMPL void removePhysics(PHYSICS *physics);
        API_IMPL void addRenderizable(RENDERIZABLE *renderizable);
        API_IMPL void addObjectRender2Texture(RENDERIZABLE_TO_TARGET *ObjectRenderTarget);
        API_IMPL void removeObjectRender2Texture(RENDERIZABLE_TO_TARGET *object);
        API_IMPL void disableAllButThis(mbm::RENDERIZABLE *draw);
        API_IMPL void removeObjectByIdSceneScene(const int idScene);
        API_IMPL void stopRender2Texture2(RENDERIZABLE *ptr);
        API_IMPL void removeRenderizable(RENDERIZABLE *object);
        API_IMPL void setDephtTest(const bool enable);
        API_IMPL bool rayCast(const float sx, const float sy, VEC3 *rayOriginOut, VEC3 *rayDir) const;
        API_IMPL bool transformeScreen2dToWorld3d_scaled(const float x, const float y, VEC3 *out,const float howFarZFromCamera) const;
        API_IMPL void transformeScreen2dToWorld2d_scaled(const float x, const float y, VEC2 &out) const noexcept;
        API_IMPL void transformeScreen2dToWorld2d_scaled(const float x, const float y, VEC3 &out) const noexcept;
        API_IMPL void transformeWorld2dToScreen2d_scaled(const float x, const float y, VEC2 &out) const noexcept;
        API_IMPL bool isPointWorld2dOnScreen2D(const float x, const float y) const noexcept;
        API_IMPL bool isCircleScreen2dOnScreen2D_scaled(const float x, const float y, const float ray) const noexcept;
        API_IMPL bool isCircleScreen2dOnScreen2D(const float x, const float y, const float ray) const noexcept;
        API_IMPL bool isCircleWorld2dOnScreen2D_scaled(const float x, const float y, const float ray) const noexcept;
        API_IMPL bool isRectangleScreen2dOnScreen2D_scaled(const float x, const float y, const float widthRectangle,const float heightRectangle) const noexcept;
        API_IMPL bool isPointScreen2dOnScreen2D_scaled(const float x, const float y) const noexcept;
        API_IMPL bool isPointScreen2dOnScreen2D(const float x, const float y) const noexcept;
        API_IMPL bool isRectangleWorld2dOnScreen2D_scaled(const float x, const float y, const float widthRectangle,const float heightRectangle) const noexcept;
        API_IMPL bool isPointScreen2DOnRectangleScreen2d(const VEC2 &pointInScreen2d, const VEC2 &halfDimRectangle,const VEC3 &positionRectangleScreen2d) const noexcept;
        API_IMPL bool isPointScreen2DOnRectangleWorld2d(const VEC2 &pointInScreen2d, const VEC2 &halfDimRectangle,const VEC3 &positionRectangleWorld2d) const noexcept;
        API_IMPL bool isPointScreen2DOnRectangleWorld2d(const VEC2 &pointInScreen2d, const VEC2 &halfDimRectangle,const VEC2 &positionRectangleWorld2d) const noexcept;
        API_IMPL bool checkBoundCollision(const VEC3 & p1,const float w1,const float h1,const VEC3 & p2,const float w2,const float h2)const noexcept;
        API_IMPL bool checkBoundCollision(const VEC3 & p1,const float w1,const float h1,const float d1,const VEC3 & p2,const float w2,const float h2,const float d2)const noexcept;
        API_IMPL void getDimFromFrustum(VEC3 *dimNear, VEC3 *dimFar) const noexcept;
        API_IMPL void setBillboard(MATRIX *out, VEC3 *position = nullptr, VEC3 *scale = nullptr);
        API_IMPL bool renderToRestore(RENDERIZABLE * renderizable);
        #if defined _WIN32 || defined(ANDROID)
        API_IMPL void setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y);
        #elif defined(__linux__) && !defined(ANDROID)
        API_IMPL void setMinMaxSizeWindow(Window win,Display * display,int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y);
        #endif

	API_IMPL void setAudioManagerInterface(AUDIO_MANAGER_INTERFACE* _audioInterface);
	API_IMPL void * get_lua_state();//if we are using lua we should be able to retrieve the current state
      private:
        static DEVICE *                       instanceDevice;
        std::vector<RENDERIZABLE *>           lsObjectRender3D;
        std::vector<RENDERIZABLE *>           lsObjectRender2DW;
        std::vector<RENDERIZABLE *>           lsObjectRender2DS;
        std::vector<PHYSICS *>                lsPhysics;
        AUDIO_MANAGER_INTERFACE*			  audioInterface;
        std::vector<RENDERIZABLE_TO_TARGET *> lsObjectRenderToTarget;
        float                                 __percXcam2dScale;
        float                                 __percYcam2dScale;
        DEVICE();
        virtual ~DEVICE();
        void setProjectionMode(const bool is3D, const float width, const float height);
    };
    
#if defined _WIN32
    API_IMPL void setTheme(int value, bool enableBorder);
    API_IMPL void hideConsoleWindow();
    API_IMPL void showConsoleWindow();
    API_IMPL const char* selectFolderDialog(char * folderPathOut);
#endif
}
#endif
