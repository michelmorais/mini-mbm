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
#include <memory>

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
    struct SPECIFIC_AUX_CONTEXT_DEVICE;

    
    class DEVICE : public TIME_CONTROL, public FRUSTUM
    {
        friend class RENDERIZABLE;
        friend class CORE_MANAGER;
      public:
        bool   verbose;
        bool   run;
        float  backBufferWidth;
        float  backBufferHeight;
        //Using GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA (or GL_SRC_ALPHA, GL_ONE for additive) makes particle visibility depend on the particle alpha value, not on whatever was previously written into the destination alpha.
        COLOR  colorClearBackGround; //	Clear the back buffer alpha to 1.0 , this make the ALPHA operation work
        CAMERA camera;
    
        SHADER_CFG_LOADER cfg;                       // CFG files
        std::map<std::string, DYNAMIC_VAR *> lsDynamicVarGlobal;
        CORE_MANAGER *      ptrManager;
        SCENE *             scene;
        
        mbm::ORDER_RENDER   orderRender;
        API_IMPL static DEVICE *     getInstance();
        API_IMPL void initializeSpecificContext();
        API_IMPL void destroySpecificContext();
        API_IMPL SPECIFIC_AUX_CONTEXT_DEVICE * getSpecificContextDevice() const noexcept;
        API_IMPL void setAppReturnCode(const int returnCode) noexcept;
        API_IMPL int getAppReturnCode() const noexcept;
        API_IMPL static void quit();
        API_IMPL float getBackBufferWidth() const noexcept;
        API_IMPL float getBackBufferHeight() const noexcept;
        API_IMPL float getScaleBackBufferWidth() const noexcept;
        API_IMPL float getScaleBackBufferHeight() const noexcept;
        API_IMPL uint32_t getTotalObjectsOnFrustum3D() const noexcept;
        API_IMPL uint32_t getTotalObjectsOnFrustum2D() const noexcept;
        API_IMPL uint32_t getTotalObjectsIsRendering3D() const noexcept;
        API_IMPL uint32_t getTotalObjectsIsRendering2D() const noexcept;
        API_IMPL uint32_t getTotalObjects3D() const noexcept;
        API_IMPL uint32_t getTotalObjects2D() const noexcept;
        API_IMPL void setClearBackGround(bool clear) noexcept;
        API_IMPL bool isClearBackGroundEnabled() const noexcept;
        API_IMPL void setStopScriptOnError(bool stop) noexcept;
        API_IMPL bool isStopScriptOnErrorEnabled() const noexcept;
        API_IMPL void resetSwapBackBufferStep() noexcept;
        API_IMPL void incrementSwapBackBufferStep() noexcept;
        API_IMPL int getSwapBackBufferStep() const noexcept;
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
        API_IMPL void setDepthTest(const bool enable);
        API_IMPL void refreshDevice();
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
        API_IMPL void clearDepth();
        API_IMPL const char* getBackendEngineName()const noexcept;
        API_IMPL const char* getBackendEngineVersion()const noexcept;
        API_IMPL void clearDepthColored();
        API_IMPL void setAudioManagerInterface(AUDIO_MANAGER_INTERFACE* _audioInterface);
        API_IMPL AUDIO_MANAGER_INTERFACE* getAudioManagerInterface() const noexcept;
        API_IMPL void * get_lua_state();//if we are using lua we should be able to retrieve the current state
        API_IMPL const char* copyFileFromAsset(const char* assetName, const char* mode);// Meant to be used in Android / Iphone (others specific implementations can just return assetName).
        API_IMPL void disableFilteringForPixelPerfect() noexcept;//backend specific way to disable texture filtering for pixel perfect rendering
        API_IMPL void enableFilteringAfterPixelPerfect() noexcept;//backend specific way to restore texture filtering
        API_IMPL bool isPixelPerfectRendering() const noexcept;// true while tile (etc.) is drawing; used to skip UV inset on D3D9
        API_IMPL bool isGamePaused() const noexcept;

        int                                   windowPositionX;
        int                                   windowPositionY;

    private:
        static DEVICE *                       instanceDevice;
        struct Impl;
        struct ImplDeleter
        {
            void operator()(Impl *ptr) const;
        };
        std::unique_ptr<Impl, ImplDeleter> impl;
        DEVICE();
        virtual ~DEVICE();
        void setProjectionMode(const bool is3D, const float width, const float height);
        void setCamera2dScaleCache(const float percX, const float percY) noexcept;
        void setPixelPerfectRenderingActive(const bool active) noexcept;
        void setSpecificContextDevice(SPECIFIC_AUX_CONTEXT_DEVICE *context) noexcept;
        void setTotalObjectsOnFrustum3D(uint32_t total) noexcept;
        void setTotalObjectsOnFrustum2D(uint32_t total) noexcept;
        void setTotalObjectsIsRendering3D(uint32_t total) noexcept;
        void setTotalObjectsIsRendering2D(uint32_t total) noexcept;
        void incrementTotalObjectsIsRendering3D() noexcept;
        void incrementTotalObjectsIsRendering2D() noexcept;
        void setTotalObjects3D(uint32_t total) noexcept;
        void setTotalObjects2D(uint32_t total) noexcept;
        void setNearFrustumDimension(const VEC3 &dimension) noexcept;
        void setFarFrustumDimension(const VEC3 &dimension) noexcept;
        void setNearFrustumDimensionX(float value) noexcept;
        void setNearFrustumDimensionY(float value) noexcept;
        void setFarFrustumDimensionX(float value) noexcept;
        void setFarFrustumDimensionY(float value) noexcept;
        uint32_t getTotalPhysics() const noexcept;
        PHYSICS * getPhysics(const uint32_t index) const noexcept;
        uint32_t getTotalRenderTargets() const noexcept;
        RENDERIZABLE_TO_TARGET * getRenderTarget(const uint32_t index) const noexcept;
        std::vector<RENDERIZABLE *> & getRender3DList() noexcept;
        std::vector<RENDERIZABLE *> & getRender2DWList() noexcept;
        std::vector<RENDERIZABLE *> & getRender2DSList() noexcept;
    };
}
#endif
