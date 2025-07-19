/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <device.h>
#include <scene.h>
#include <texture-manager.h>
#include <audio-interface.h>
#include <shapes.h>
#include <physics.h>
#include <renderizable.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <gles-debug.h>
#include <dynamic-var.h>

#if defined ANDROID
    #include <platform/common-jni.h>
#elif defined _WIN32
    #include <plusWindows/defaultThemePlusWindows.h>
#elif defined(__linux__)
    #include <X11/Xutil.h>
#endif

namespace mbm
{

    DEVICE * DEVICE::getInstance()
    {
        if (instanceDevice == nullptr)
        {
            instanceDevice = new DEVICE();
        }
        return instanceDevice;
    }

#ifdef ANDROID
    void DEVICE::callQuitInJava()
    {
        util::COMMON_JNI *cJni  = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv   = cJni->jenv;
        jfieldID         fidRun = jenv->GetStaticFieldID(cJni->jclassInstanceActivityEngine, "run", "Z");
        if (nullptr == fidRun)
        {
            PRINT_IF_DEBUG( "wasn't found variable \"run\" class: %s", cJni->jclassInstanceActivityEngine);
            return;
        }
        jenv->SetStaticBooleanField(cJni->jclassInstanceActivityEngine, fidRun, false);
    }
#endif

    void DEVICE::quit()
    {
        TEXTURE_MANAGER::release();
        MESH_MANAGER::release();
#ifdef ANDROID
        util::COMMON_JNI::release();
#endif
		releaseAudioManager();
		if (instanceDevice)
        {
            delete instanceDevice;
        }
        instanceDevice = nullptr;
    }

#ifdef ANDROID
    
    void DEVICE::streamStopped(const int indexJNI)
    {
		if(this->audioInterface)
			this->audioInterface->streamStopped(indexJNI);
    }
#endif
    
    float DEVICE::getBackBufferWidth() const noexcept
    {
        return backBufferWidth;
    }
    
    float DEVICE::getBackBufferHeight() const noexcept
    {
        return backBufferHeight;
    }
    
    float DEVICE::getScaleBackBufferWidth() const noexcept
    {
        return static_cast<float>(backBufferWidth / this->camera.scale2d.x);
    }
    
    float DEVICE::getScaleBackBufferHeight() const noexcept
    {
        return static_cast<float>(backBufferHeight / this->camera.scale2d.y);
    }
    
    void DEVICE::scaleToScreen(const float widthScreen, const float heightScreen,
                              const char *stretch) noexcept // stretch: x, y xy nullptr
    {
        if (widthScreen != 0.0f && heightScreen != 0.0f)
        {
            const float percx = this->backBufferWidth / widthScreen;
            const float percy = this->backBufferHeight / heightScreen;
            if (percx != 0.0f && percy != 0.0f)
            {
                this->camera.expectedScreen.x = widthScreen;
                this->camera.expectedScreen.y = heightScreen;
                if (stretch)
                {
                    if (strcmp(stretch, "x") == 0)
                    {
                        this->camera.scale2d.x = percx;
                        this->camera.scale2d.y = percx;
                        strncpy(this->camera.stretch, "x",sizeof(this->camera.stretch)-1);
                    }
                    else if (strcmp(stretch, "y") == 0)
                    {
                        this->camera.scale2d.x = percy;
                        this->camera.scale2d.y = percy;
                        strncpy(this->camera.stretch, "y",sizeof(this->camera.stretch)-1);
                    }
                    else if (strcmp(stretch, "xy") == 0)
                    {
                        this->camera.scale2d.x = percx;
                        this->camera.scale2d.y = percy;
                        strncpy(this->camera.stretch, "xy",sizeof(this->camera.stretch));
                    }
                    else if (percx < percy)
                    {
                        this->camera.scale2d.x = percx;
                        this->camera.scale2d.y = percx;
                        strncpy(this->camera.stretch, "x",sizeof(this->camera.stretch)-1);
                    }
                    else
                    {
                        this->camera.scale2d.x = percy;
                        this->camera.scale2d.y = percy;
                        strncpy(this->camera.stretch, "y",sizeof(this->camera.stretch)-1);
                    }
                }
                else if (percx < percy)
                {
                    this->camera.scale2d.x = percx;
                    this->camera.scale2d.y = percx;
                    strncpy(this->camera.stretch, "x",sizeof(this->camera.stretch)-1);
                }
                else
                {
                    this->camera.scale2d.x = percy;
                    this->camera.scale2d.y = percy;
                    strncpy(this->camera.stretch, "y",sizeof(this->camera.stretch)-1);
                }
            }
        }
    }
    
    void DEVICE::pauseGame()
    {
        this->pauseTimer();
        if(this->audioInterface)
			this->audioInterface->pauseAll(this->scene ? this->scene->getIdScene() : 0);
    }
    
    void DEVICE::resumeGame()
    {
        this->resumeTimer();
		if (this->audioInterface)
			this->audioInterface->resumeAll(this->scene ? this->scene->getIdScene() : 0);
    }
    
    void DEVICE::addPhysics(PHYSICS *physics)
    {
        if (physics)
            lsPhysics.push_back(physics);
    }
    
    void DEVICE::removePhysics(PHYSICS *physics)
    {
        for (std::vector<PHYSICS *>::size_type i = 0; i < this->lsPhysics.size(); ++i)
        {
            PHYSICS *ptrPhysics = this->lsPhysics[i];
            if (ptrPhysics == physics)
            {
                this->lsPhysics.erase(this->lsPhysics.begin() + std::vector<PHYSICS *>::difference_type(i));
                break;
            }
        }
    }
    
    void DEVICE::addRenderizable(RENDERIZABLE *renderizable)
    {
        if (renderizable != nullptr)
        {
            if (renderizable->is3D)
            {
                if (renderizable->position.z == 0.0f)
                    renderizable->position.z = this->orderRender.getNextZOrderControl3d();
                this->lsObjectRender3D.push_back(renderizable);
            }
            else if (renderizable->is2dS)
            {
                if (renderizable->position.z == 0.0f)
                    renderizable->position.z = this->orderRender.getNextZOrderControl2d(
                        renderizable->is2dS, renderizable->typeClass == TYPE_CLASS_TEXT);
                this->lsObjectRender2DS.push_back(renderizable);
            }
            else
            {
                if (renderizable->position.z == 0.0f)
                    renderizable->position.z = this->orderRender.getNextZOrderControl2d(
                        renderizable->is2dS, renderizable->typeClass == TYPE_CLASS_TEXT);
                this->lsObjectRender2DW.push_back(renderizable);
            }
        }
#if defined _DEBUG
        else
        {
            PRINT_IF_DEBUG( "error on add renderizable ");
        }
#endif
    }
    
    void DEVICE::addObjectRender2Texture(RENDERIZABLE_TO_TARGET *ObjectRenderTarget)
    {
        if (ObjectRenderTarget != nullptr)
        {
            lsObjectRenderToTarget.push_back(ObjectRenderTarget);
        }
        else
        {
            PRINT_IF_DEBUG( "error on add renderizable ");
        }
    }
    
    void DEVICE::removeObjectRender2Texture(RENDERIZABLE_TO_TARGET *object)
    {
        for (std::vector<RENDERIZABLE_TO_TARGET *>::size_type i = 0; i < lsObjectRenderToTarget.size(); ++i)
        {
            RENDERIZABLE_TO_TARGET *ptr = lsObjectRenderToTarget[i];
            if (ptr == object)
            {
                for (auto ph : this->lsPhysics)
                {
                    ph->removeObject(ptr);
                }
                lsObjectRenderToTarget.erase(lsObjectRenderToTarget.begin() + std::vector<RENDERIZABLE_TO_TARGET *>::difference_type(i));
                break;
            }
        }
    }
    
    void DEVICE::disableAllButThis(mbm::RENDERIZABLE *draw)
    {
        for (auto ptr : lsObjectRender3D)
        {
            ptr->enableRender = false;
        }
        for (auto ptr : lsObjectRender2DS)
        {
            ptr->enableRender = false;
        }
        for (auto ptr : lsObjectRender2DW)
        {
            ptr->enableRender = false;
        }
        draw->enableRender = true;
    }
    
    void DEVICE::removeObjectByIdSceneScene(const int idScene)
    {
        for (auto ph : this->lsPhysics)
        {
            ph->removeObjectByIdSceneScene(idScene);
        }
        for (std::vector<RENDERIZABLE *>::size_type i = 0; i < lsObjectRender3D.size(); ++i)
        {
            RENDERIZABLE *ptr = lsObjectRender3D[i];
            if (ptr->idScene == idScene)
            {
                lsObjectRender3D.erase(lsObjectRender3D.begin() + std::vector<RENDERIZABLE *>::difference_type(i));
                i--;
            }
        }
        for (std::vector<RENDERIZABLE *>::size_type i = 0; i < lsObjectRender2DW.size(); ++i)
        {
            RENDERIZABLE *ptr = lsObjectRender2DW[i];
            if (ptr->idScene == idScene)
            {
                lsObjectRender2DW.erase(lsObjectRender2DW.begin() + std::vector<RENDERIZABLE *>::difference_type(i));
                i--;
            }
        }
        for (std::vector<RENDERIZABLE *>::size_type i = 0; i < lsObjectRender2DS.size(); ++i)
        {
            RENDERIZABLE *ptr = lsObjectRender2DS[i];
            if (ptr->idScene == idScene)
            {
                lsObjectRender2DS.erase(lsObjectRender2DS.begin() + std::vector<RENDERIZABLE *>::difference_type(i));
                i--;
            }
        }
    }
    
    void DEVICE::stopRender2Texture2(RENDERIZABLE *ptr)
    {
        for (auto r : this->lsObjectRenderToTarget)
        {
            r->removeFromRender2Texture(ptr);
        }
    }
    
    void DEVICE::removeRenderizable(RENDERIZABLE *object)
    {
        if (object == nullptr)
            return;
		if(object->typeClass == TYPE_CLASS_TILE_OBJ)
		{
			for (auto ph : this->lsPhysics)
			{
				ph->removeObject(object);
			}
		}
        else if (object->is3D)
        {
            for (std::vector<RENDERIZABLE *>::size_type i = 0; i < lsObjectRender3D.size(); ++i)
            {
                RENDERIZABLE *ptr = lsObjectRender3D[i];
                if (ptr == object)
                {
                    if (ptr->isRender2Texture)
                        this->stopRender2Texture2(ptr);
                    for (auto ph : this->lsPhysics)
                    {
                        ph->removeObject(ptr);
                    }
                    lsObjectRender3D.erase(lsObjectRender3D.begin() + std::vector<RENDERIZABLE *>::difference_type(i));
                    return;
                }
            }
        }
        else if (object->is2dS == false)
        {
            for (std::vector<RENDERIZABLE *>::size_type i = 0; i < lsObjectRender2DW.size(); ++i)
            {
                RENDERIZABLE *ptr = lsObjectRender2DW[i];
                if (ptr == object)
                {
                    if (ptr->isRender2Texture)
                        this->stopRender2Texture2(ptr);
                    for (auto ph : this->lsPhysics)
                    {
                        ph->removeObject(ptr);
                    }
                    lsObjectRender2DW.erase(lsObjectRender2DW.begin() + std::vector<RENDERIZABLE *>::difference_type(i));
                    break;
                }
            }
        }
        else
        {
            for (std::vector<RENDERIZABLE *>::size_type i = 0; i < lsObjectRender2DS.size(); ++i)
            {
                RENDERIZABLE *ptr = lsObjectRender2DS[i];
                if (ptr == object)
                {
                    if (ptr->isRender2Texture)
                        this->stopRender2Texture2(ptr);
                    for (auto ph : this->lsPhysics)
                    {
                        ph->removeObject(ptr);
                    }
                    lsObjectRender2DS.erase(lsObjectRender2DS.begin() + std::vector<RENDERIZABLE *>::difference_type(i));
                    break;
                }
            }
        }
    }
    
    void DEVICE::setDephtTest(const bool enable)
    {
        if (enable)
        {
            GLEnable(GL_DEPTH_TEST);
        }
        else
        {
            GLDisable(GL_DEPTH_TEST);
        }
    }
    
    bool DEVICE::rayCast(const float sx, const float sy, VEC3 *rayOriginOut, VEC3 *rayDir) const
    {
        // two ways to do it ...
        const float vx = (sx /  backBufferWidth - 0.5f) * 2.0f / camera.matrixProj._11;
        const float vy = -(sy / backBufferHeight - 0.5f) * 2.0f / camera.matrixProj._22;
        const float vz = 1.0f;
        MATRIX      m;
        if (MatrixInverse(&m, nullptr, &camera.matrixView) == nullptr)
            return false;
        // Transform the screen space pick ray into 3D space
        rayDir->x       = vx * m._11 + vy * m._21 + vz * m._31;
        rayDir->y       = vx * m._12 + vy * m._22 + vz * m._32;
        rayDir->z       = vx * m._13 + vy * m._23 + vz * m._33;
        rayOriginOut->x = m._41;
        rayOriginOut->y = m._42;
        rayOriginOut->z = m._43;
        return true;
        /*
        const float vx =  (sx/backBufferWidth  - 0.5f) * 2.0f; // [0,1024] -> [-1,1]
        const float vy = -(sy/backBufferHeight - 0.5f) * 2.0f; // [0, 768] -> [-1,1]
        VEC3 origin(vx,vy,0.0f);
        VEC3 Far(vx,vy,1.0f);
        MATRIX inverseviewproj;
        MATRIX m = camera.matrixView * camera.matrixProj;
        if(FAILED(MatrixInverse( &inverseviewproj, nullptr, &m)))
            return false;
        VEC3 rayorigin;
        VEC3 rayend;
        Vec3TransformCoord(&rayorigin,&origin,&inverseviewproj);
        Vec3TransformCoord(&rayend,&Far,&inverseviewproj);
        VEC3 pv(rayend-rayorigin), raydirection;
        Vec3Normalize(&raydirection,&pv);
        *rayDir = raydirection;
        *rayOriginOut = rayorigin;
        return true;
        */
    }
    
    bool DEVICE::transformeScreen2dToWorld3d_scaled(const float x, const float y, VEC3 *out,
                                                         const float howFarZFromCamera) const
    {
        VEC3        rayOriginOut, rayDirOut;
        const float newX = x * this->camera.scaleScreen2d.x;
        const float newY = y * this->camera.scaleScreen2d.y;
        if (this->rayCast(newX, newY, &rayOriginOut, &rayDirOut))
        {
            out->x = rayDirOut.x * howFarZFromCamera + rayOriginOut.x;
            out->y = rayDirOut.y * howFarZFromCamera + rayOriginOut.y;
            out->z = rayDirOut.z * howFarZFromCamera + rayOriginOut.z;
            return true;
        }
        return false;
    }
    
    void DEVICE::transformeScreen2dToWorld2d_scaled(const float x, const float y, VEC2 &out) const noexcept
    {
        const VEC2 middle(this->backBufferWidth * 0.5f, this->backBufferHeight * 0.5f);
        out.x = (x * this->camera.scaleScreen2d.x) - middle.x + this->camera.position2d.x;
        out.y = -((y * this->camera.scaleScreen2d.y) - middle.y) + this->camera.position2d.y;
        out.x *= this->__percXcam2dScale;
        out.y *= this->__percYcam2dScale;
    }
    
    void DEVICE::transformeScreen2dToWorld2d_scaled(const float x, const float y, VEC3 &out) const noexcept
    {
        VEC2 out2d(out.x, out.y);
        this->transformeScreen2dToWorld2d_scaled(x, y, out2d);
        out.x = out2d.x;
        out.y = out2d.y;
    }
    
    void DEVICE::transformeWorld2dToScreen2d_scaled(const float x, const float y, VEC2 &out) const noexcept
    {
        const VEC2 newIn(x / this->__percXcam2dScale, y / this->__percYcam2dScale);
        const VEC2 middle(this->backBufferWidth * 0.5f, this->backBufferHeight * 0.5f);
        out.x = newIn.x + middle.x - this->camera.position2d.x;
        out.y = this->backBufferHeight - ((newIn.y + middle.y) - this->camera.position2d.y);
        out.x /= this->camera.scaleScreen2d.x;
        out.y /= this->camera.scaleScreen2d.y; // TODO
    }
    
    bool DEVICE::isPointWorld2dOnScreen2D(const float x, const float y) const noexcept
    {
        VEC2 onScreen(x, y);
        this->transformeWorld2dToScreen2d_scaled(x, y, onScreen);
        if (onScreen.x < 0)
            return false;
        else if (onScreen.x > this->backBufferWidth * this->__percXcam2dScale)
            return false;
        else if (onScreen.y < 0)
            return false;
        else if (onScreen.y > this->backBufferHeight * this->__percYcam2dScale)
            return false;
        return true;
    }
    
    bool DEVICE::isCircleScreen2dOnScreen2D_scaled(const float x, const float y, const float ray) const noexcept
    {
        const float newX = this->camera.scaleScreen2d.x * x;
        const float newY = this->camera.scaleScreen2d.y * y;
        if ((newX + ray) < 0)
            return false;
        else if ((newX - ray) > this->backBufferWidth)
            return false;
        else if ((newY + ray) < 0)
            return false;
        else if ((newY - ray) > this->backBufferHeight)
            return false;
        return true;
    }
    
    bool DEVICE::isCircleScreen2dOnScreen2D(const float x, const float y, const float ray) const noexcept
    {
        if ((x + ray) < 0)
            return false;
        else if ((x - ray) > this->backBufferWidth)
            return false;
        else if ((y + ray) < 0)
            return false;
        else if ((y - ray) > this->backBufferHeight)
            return false;
        return true;
    }
    
    bool DEVICE::isCircleWorld2dOnScreen2D_scaled(const float x, const float y, const float ray) const noexcept
    {
        VEC2 onScreen(x, y);
        this->transformeWorld2dToScreen2d_scaled(x, y, onScreen);
        if ((onScreen.x + ray) < 0)
            return false;
        else if ((onScreen.x - ray) > this->backBufferWidth * this->__percXcam2dScale)
            return false;
        else if ((onScreen.y + ray) < 0)
            return false;
        else if ((onScreen.y - ray) > this->backBufferHeight * this->__percYcam2dScale)
            return false;
        return true;
    }
    
    bool DEVICE::isRectangleScreen2dOnScreen2D_scaled(const float x, const float y, const float widthRectangle,
                                                           const float heightRectangle) const noexcept
    {
        VEC3 pos1(x,y,0);
        VEC3 pos2(this->getScaleBackBufferWidth() * 0.5f,this->getScaleBackBufferHeight() * 0.5f ,0);
        return this->checkBoundCollision(pos1,
            widthRectangle,
            heightRectangle,
            pos2,
            this->getScaleBackBufferWidth(),
            this->getScaleBackBufferHeight());
    }
    
    bool DEVICE::isPointScreen2dOnScreen2D_scaled(const float x, const float y) const noexcept
    {
        const VEC2  onScreen(x, y);
        const float w = this->getScaleBackBufferWidth();
        const float h = this->getScaleBackBufferHeight();
        if (onScreen.x < 0)
            return false;
        else if (onScreen.x > w)
            return false;
        else if (onScreen.y < 0)
            return false;
        else if (onScreen.y > h)
            return false;
        return true;
    }
    
    bool DEVICE::isPointScreen2dOnScreen2D(const float x, const float y) const noexcept
    {
        const VEC2 onScreen(x, y);
        if (onScreen.x < 0)
            return false;
        else if (onScreen.x > this->backBufferWidth)
            return false;
        else if (onScreen.y < 0)
            return false;
        else if (onScreen.y > this->backBufferHeight)
            return false;
        return true;
    }
    
    bool DEVICE::isRectangleWorld2dOnScreen2D_scaled(const float x, const float y, const float widthRectangle,
                                                          const float heightRectangle) const noexcept
    {
        VEC3 pos1(x,y,0);
        VEC3 pos2;
        this->transformeScreen2dToWorld2d_scaled(this->getScaleBackBufferWidth() * 0.5f,this->getScaleBackBufferHeight() * 0.5f,pos2);
        return this->checkBoundCollision(pos1,
            widthRectangle,
            heightRectangle,
            pos2,
            this->getScaleBackBufferWidth(),
            this->getScaleBackBufferHeight());
    }
    
    bool DEVICE::isPointScreen2DOnRectangleScreen2d(const VEC2 &pointInScreen2d, const VEC2 &halfDimRectangle,
                                                         const VEC3 &positionRectangleScreen2d) const noexcept
    {
        if (pointInScreen2d.x < (positionRectangleScreen2d.x - halfDimRectangle.x))
            return false;
        if (pointInScreen2d.x > (positionRectangleScreen2d.x + halfDimRectangle.x))
            return false;
        if (pointInScreen2d.y < (positionRectangleScreen2d.y - halfDimRectangle.y))
            return false;
        if (pointInScreen2d.y > (positionRectangleScreen2d.y + halfDimRectangle.y))
            return false;
        return true;
    }
    
    bool DEVICE::isPointScreen2DOnRectangleWorld2d(const VEC2 &pointInScreen2d, const VEC2 &halfDimRectangle,
                                                        const VEC3 &positionRectangleWorld2d) const noexcept
    {
        VEC2 point2dWorld;
        this->transformeScreen2dToWorld2d_scaled(pointInScreen2d.x, pointInScreen2d.y, point2dWorld);
        if (point2dWorld.x < (positionRectangleWorld2d.x - halfDimRectangle.x))
            return false;
        if (point2dWorld.x > (positionRectangleWorld2d.x + halfDimRectangle.x))
            return false;
        if (point2dWorld.y < (positionRectangleWorld2d.y - halfDimRectangle.y))
            return false;
        if (point2dWorld.y > (positionRectangleWorld2d.y + halfDimRectangle.y))
            return false;
        return true;
    }
    
    bool DEVICE::isPointScreen2DOnRectangleWorld2d(const VEC2 &pointInScreen2d, const VEC2 &halfDimRectangle,
                                                        const VEC2 &positionRectangleWorld2d) const noexcept
    {
        VEC2 point2dWorld;
        this->transformeScreen2dToWorld2d_scaled(pointInScreen2d.x, pointInScreen2d.y, point2dWorld);
        if (point2dWorld.x < (positionRectangleWorld2d.x - halfDimRectangle.x))
            return false;
        if (point2dWorld.x > (positionRectangleWorld2d.x + halfDimRectangle.x))
            return false;
        if (point2dWorld.y < (positionRectangleWorld2d.y - halfDimRectangle.y))
            return false;
        if (point2dWorld.y > (positionRectangleWorld2d.y + halfDimRectangle.y))
            return false;
        return true;
    }

    bool DEVICE::checkBoundCollision(const VEC3 & p1,const float w1,const float h1,const VEC3 & p2,const float w2,const float h2)const noexcept
    {
        const float w1Half = w1 * 0.5f;
        const float h1Half = h1 * 0.5f;
        const float w2Half = w2 * 0.5f;
        const float h2Half = h2 * 0.5f;

        if(p1.x - w1Half > p2.x + w2Half)
            return false;
        if(p1.x + w1Half < p2.x - w2Half)
            return false;

        if(p1.y - h1Half > p2.y + h2Half)
            return false;
        if(p1.y + h1Half < p2.y - h2Half)
            return false;
        return true;
    }

    bool DEVICE::checkBoundCollision(const VEC3 & p1,const float w1,const float h1,const float d1,const VEC3 & p2,const float w2,const float h2,const float d2)const noexcept
    {
        const float w1Half = w1 * 0.5f;
        const float h1Half = h1 * 0.5f;
        const float d1Half = d1 * 0.5f;
        const float w2Half = w2 * 0.5f;
        const float h2Half = h2 * 0.5f;
        const float d2Half = d2 * 0.5f;

        if(p1.x - w1Half > p2.x + w2Half)
            return false;
        if(p1.x + w1Half < p2.x - w2Half)
            return false;

        if(p1.y - h1Half > p2.y + h2Half)
            return false;
        if(p1.y + h1Half < p2.y - h2Half)
            return false;

        if(p1.z - d1Half > p2.z + d2Half)
            return false;
        if(p1.z + d1Half < p2.z - d2Half)
            return false;
        return true;
    }

    void DEVICE::getDimFromFrustum(VEC3 *dimNear, VEC3 *dimFar) const noexcept
    {
        *dimNear = this->dimNearFrustum3d;
        *dimFar  = this->dimFarFrustum3d;
    }
    
    void DEVICE::setBillboard(MATRIX *out, VEC3 *position , VEC3 *scale)
    {
        if (out)
        {
            MATRIX matrixAux;
            *out = this->camera.matrixBillboard;
            if (scale)
            {
                MatrixScaling(&matrixAux, scale->x, scale->y, scale->z);
                MatrixMultiply(out, &matrixAux, out);
            }
            if (position)
            {
                out->_41 = position->x;
                out->_42 = position->y;
                out->_43 = position->z;
            }
        }
    }

    bool DEVICE::renderToRestore(RENDERIZABLE * renderizable)
    {
        return renderizable && renderizable->render();
    }

    #if defined _WIN32
    void DEVICE::setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        this->window.setMinSizeAllowed(min_x,min_y);
        this->window.setMaxSizeAllowed(max_x,max_y);
    }
    #elif defined(__linux__) && !defined(ANDROID)
    void DEVICE::setMinMaxSizeWindow(Window win,Display * display,int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        XSizeHints xsize;
        long min_flag = PMinSize;
        long max_flag = PMaxSize;
        if(min_x == 0 && min_y == 0)
            min_flag = 0;
        if(max_x == 0 && max_y == 0)
            max_flag = 0;

        xsize.flags         = max_flag|min_flag|USPosition;
        xsize.max_width     = static_cast<int>(max_x);
        xsize.max_height    = static_cast<int>(max_y);
        xsize.min_width     = static_cast<int>(min_x);
        xsize.min_height    = static_cast<int>(min_y);
        if(static_cast<int32_t>(this->backBufferWidth) <= max_x && static_cast<int32_t>(this->backBufferWidth) >= min_x)
        {
            xsize.base_width    = static_cast<int>(this->backBufferWidth);
            xsize.width         = static_cast<int>(this->backBufferWidth);
        }
        else
        {
            xsize.base_width    = min_x;
            xsize.width         = static_cast<int>(min_x);
        }

        if(static_cast<int32_t>(this->backBufferHeight) <= max_y && static_cast<int32_t>(this->backBufferHeight) >= min_y)
        {
            xsize.base_height   = static_cast<int>(this->backBufferHeight);
            xsize.height        = static_cast<int>(this->backBufferHeight);
        }
        else
        {
            xsize.base_height   = min_y;
            xsize.height        = static_cast<int>(min_y);
        }
        xsize.width_inc     = 0;
        xsize.height_inc    = 0;
        xsize.x             = 0;
        xsize.y             = 0;
        XSetWMNormalHints(display,win,&xsize);
    }
    #elif defined(__linux__) && defined(ANDROID)
    void DEVICE::setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        INFO_LOG("setMinMaxSizeWindow (%d,%d,%d,%d) has not effect on this ANDROID platform.",min_x,min_y,max_x,max_y);
    }
    #endif

	void DEVICE::setAudioManagerInterface(AUDIO_MANAGER_INTERFACE* _audioInterface)
	{
		this->audioInterface = _audioInterface;
	}

    void * DEVICE::get_lua_state()//if we are using lua we should be able to retrieve the current state
    {
        if(this->scene)
            return this->scene->get_lua_state();
        return nullptr;
    }
    
    DEVICE::DEVICE()
    {
    #ifdef ANDROID
        jni = util::COMMON_JNI::getInstance();
    #endif
        bOnErrorStopScript         = false;
        clearBackGround            = true;
        ptrManager                 = nullptr;
        scene                      = nullptr;
        run                        = true;
		verbose					   = true;
        backBufferWidth            = 0;
        backBufferHeight           = 0;
        colorClearBackGround       = COLOR(0.0f, 0.0f, 0.0f, 0.0f);
        totalObjectsOnFrustum3D    = 0;
        totalObjectsOnFrustum2D    = 0;
        totalObjectsIsRendering3D  = 0;
        totalObjectsIsRendering2D  = 0;
        totalObjects3D             = 0;
        totalObjects2D             = 0;
        dimFarFrustum3d            = VEC3(0, 0, 0);
        dimNearFrustum3d           = VEC3(0, 0, 0);
        __percXcam2dScale          = 1.0f;
        __percYcam2dScale          = 1.0f;
        __swapBackBufferStep	   = 3;
		audioInterface			   = nullptr;
    }

	int DEVICE::returnCodeApp = 0;
    
    DEVICE::~DEVICE()
    {
        for (const auto & i : this->lsDynamicVarGlobal)
        {
            DYNAMIC_VAR *dVar = i.second;
            delete dVar;
        }
        this->lsDynamicVarGlobal.clear();
    }
    
    void DEVICE::setProjectionMode(const bool is3D, const float width, const float height)
    {
        if (width > 0 && height > 0)
        {
            GLViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        }
        if (width > 0)
            backBufferWidth = width;
        if (height > 0)
            backBufferHeight = height;
        if (width > 0 && height > 0)
            this->camera.updateCam(is3D, static_cast<float>(width), static_cast<float>(height));
    }

#if defined _WIN32
    void setTheme(int value, bool enableBorder)
    {
        THEME_WINPLUS_CUSTOM_RENDER::setTheme(value, enableBorder);
    }
    void hideConsoleWindow()
    {
        HWND hConsole = GetConsoleWindow();
        if (hConsole)
            ShowWindow(hConsole, SW_HIDE);
    }
    void showConsoleWindow()
    {
        HWND hConsole = GetConsoleWindow();
        if (hConsole)
            ShowWindow(hConsole, SW_SHOW | SW_NORMAL);
    }
    const char* selectFolderDialog(char * folderPathOut)
    {
        HWND hwnd = mbm::DEVICE::getInstance()->window.getHwnd();
        const char *      path         = mbm::selectetDirectory(hwnd,folderPathOut);
        return path;
    }

#endif
}

mbm::DEVICE *          mbm::DEVICE::instanceDevice                   = nullptr;


