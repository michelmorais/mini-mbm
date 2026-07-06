/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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
#include <audio-interface.h>
#include <renderizable.h>
#include <physics.h>
#include <util-interface.h>
#include <dynamic-var.h>
#include <core-manager.h>
#include <header-mesh.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

//#if (defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32))
//    #include <plusWindows/defaultThemePlusWindows.h>
//#endif

namespace mbm
{
    struct DEVICE::Impl
    {
        int                      returnCodeApp = 0;
        AUDIO_MANAGER_INTERFACE* audioInterface = nullptr;
        bool                     isGamePaused = false;
        float                    percXcam2dScale = 1.0f;
        float                    percYcam2dScale = 1.0f;
        bool                     pixelPerfectRenderingActive = false;
        std::vector<PHYSICS *>   physics;
        std::vector<RENDERIZABLE_TO_TARGET *> renderTargets;
        std::vector<RENDERIZABLE *> render3D;
        std::vector<RENDERIZABLE *> render2DW;
        std::vector<RENDERIZABLE *> render2DS;
        bool verbose = true;
        bool run = true;
        float backBufferWidth = 0.0f;
        float backBufferHeight = 0.0f;
        CORE_MANAGER *ptrManager = nullptr;
        SHADER_CFG_LOADER cfg;
        mbm::ORDER_RENDER orderRender;
        std::map<std::string, DYNAMIC_VAR *> lsDynamicVarGlobal;
        SPECIFIC_AUX_CONTEXT_DEVICE *specificContextDevice = nullptr;
        SCENE *scene = nullptr;
        CAMERA camera;
        uint32_t totalObjectsOnFrustum3D = 0;
        uint32_t totalObjectsOnFrustum2D = 0;
        uint32_t totalObjectsIsRendering3D = 0;
        uint32_t totalObjectsIsRendering2D = 0;
        uint32_t totalObjects3D = 0;
        uint32_t totalObjects2D = 0;
        VEC3 dimFarFrustum3d = VEC3(0, 0, 0);
        VEC3 dimNearFrustum3d = VEC3(0, 0, 0);
        COLOR colorClearBackGround = COLOR(0.0f, 0.0f, 0.0f, 1.0f);
        LIGHT_STATE light3D;
        LIGHT_STATE light2DW;
        LIGHT_MULTI_SETTINGS lightMulti3D;
        LIGHT_MULTI_SETTINGS lightMulti2DW;
        std::vector<LIGHT_POINT> pointLights3D;
        std::vector<LIGHT_POINT> pointLights2DW;
        LIGHT_TARGET currentRenderLightTarget = LIGHT_TARGET_3D;
        bool currentRenderLightTargetEnabled = false;
        const RENDERIZABLE *currentRenderizable = nullptr;
        util::MATERIAL currentRenderMaterial;
        bool currentRenderMaterialEnabled = false;
        bool clearBackGround = true;
        bool stopScriptOnError = false;
        int swapBackBufferStep = 3;
        int windowPositionX = 0;
        int windowPositionY = 0;
    };

    struct DEVICE_LIGHT_ACCESS
    {
        static LIGHT_STATE &getMutableLightState(DEVICE *device, const LIGHT_TARGET target) noexcept
        {
            return target == LIGHT_TARGET_2DW ? device->impl->light2DW : device->impl->light3D;
        }

        static const LIGHT_STATE &getLightState(const DEVICE *device, const LIGHT_TARGET target) noexcept
        {
            return target == LIGHT_TARGET_2DW ? device->impl->light2DW : device->impl->light3D;
        }

        static LIGHT_MULTI_SETTINGS &getMutableLightMultiSettings(DEVICE *device, const LIGHT_TARGET target) noexcept
        {
            return target == LIGHT_TARGET_2DW ? device->impl->lightMulti2DW : device->impl->lightMulti3D;
        }

        static const LIGHT_MULTI_SETTINGS &getLightMultiSettings(const DEVICE *device,
                                                                 const LIGHT_TARGET target) noexcept
        {
            return target == LIGHT_TARGET_2DW ? device->impl->lightMulti2DW : device->impl->lightMulti3D;
        }

        static std::vector<LIGHT_POINT> &getMutablePointLights(DEVICE *device, const LIGHT_TARGET target) noexcept
        {
            return target == LIGHT_TARGET_2DW ? device->impl->pointLights2DW : device->impl->pointLights3D;
        }

        static const std::vector<LIGHT_POINT> &getPointLights(const DEVICE *device, const LIGHT_TARGET target) noexcept
        {
            return target == LIGHT_TARGET_2DW ? device->impl->pointLights2DW : device->impl->pointLights3D;
        }
    };

    void DEVICE::ImplDeleter::operator()(Impl *ptr) const
    {
        delete ptr;
    }

    DEVICE * DEVICE::getInstance()
    {
        if (instanceDevice == nullptr)
        {
            instanceDevice = new DEVICE();
            instanceDevice->initializeSpecificContext();
        }
        return instanceDevice;
    }

    DEVICE::DEVICE()
        : impl(new Impl())
    {
    }

    void DEVICE::setCamera2dScaleCache(const float percX, const float percY) noexcept
    {
        impl->percXcam2dScale = percX;
        impl->percYcam2dScale = percY;
    }

    void DEVICE::setPixelPerfectRenderingActive(const bool active) noexcept
    {
        impl->pixelPerfectRenderingActive = active;
    }

    void DEVICE::setSpecificContextDevice(SPECIFIC_AUX_CONTEXT_DEVICE *context) noexcept
    {
        impl->specificContextDevice = context;
    }

    SPECIFIC_AUX_CONTEXT_DEVICE * DEVICE::getSpecificContextDevice() const noexcept
    {
        return impl->specificContextDevice;
    }

    CAMERA & DEVICE::getCamera() noexcept
    {
        return impl->camera;
    }

    const CAMERA & DEVICE::getCamera() const noexcept
    {
        return impl->camera;
    }

    void DEVICE::setScene(SCENE *scene) noexcept
    {
        impl->scene = scene;
    }

    SCENE * DEVICE::getScene() const noexcept
    {
        return impl->scene;
    }

    void DEVICE::setLightTargetForRender(const LIGHT_TARGET target) noexcept
    {
        impl->currentRenderLightTarget = isValidLightTarget(target) ? target : LIGHT_TARGET_3D;
        impl->currentRenderLightTargetEnabled = isValidLightTarget(target);
    }

    void DEVICE::disableLightForRender() noexcept
    {
        impl->currentRenderLightTargetEnabled = false;
    }

    bool DEVICE::getLightStateForCurrentRender(LIGHT_STATE &outState) const noexcept
    {
        if (impl->currentRenderLightTargetEnabled == false)
            return false;
        outState = DEVICE_LIGHT_ACCESS::getLightState(this, impl->currentRenderLightTarget);
        return true;
    }

    bool DEVICE::getLightTargetForCurrentRender(LIGHT_TARGET &outTarget) const noexcept
    {
        if (impl->currentRenderLightTargetEnabled == false)
            return false;
        outTarget = impl->currentRenderLightTarget;
        return true;
    }

    void DEVICE::setRenderizableForCurrentRender(const RENDERIZABLE *renderizable) noexcept
    {
        impl->currentRenderizable = renderizable;
    }

    void DEVICE::clearRenderizableForCurrentRender() noexcept
    {
        impl->currentRenderizable = nullptr;
    }

    bool DEVICE::getRenderizableForCurrentRender(const RENDERIZABLE *&outRenderizable) const noexcept
    {
        if (impl->currentRenderizable == nullptr)
            return false;
        outRenderizable = impl->currentRenderizable;
        return true;
    }

    uint32_t DEVICE::getSelectedPointLightsForCurrentRender(LIGHT_POINT_SELECTION *outSelections,
                                                            const uint32_t maxOutSelections) const noexcept
    {
        if (impl->currentRenderLightTargetEnabled == false || impl->currentRenderizable == nullptr)
            return 0u;
        return selectPointLightsForObject(impl->currentRenderLightTarget, impl->currentRenderizable->getPosition(),
                                          impl->currentRenderizable->getBoundingAABB(), outSelections,
                                          maxOutSelections);
    }

    void DEVICE::setRenderMaterial(const util::MATERIAL &material) noexcept
    {
        impl->currentRenderMaterial = material;
        impl->currentRenderMaterialEnabled = true;
    }

    void DEVICE::clearRenderMaterial() noexcept
    {
        impl->currentRenderMaterialEnabled = false;
    }

    bool DEVICE::getMaterialForCurrentRender(util::MATERIAL &outMaterial) const noexcept
    {
        if (impl->currentRenderMaterialEnabled == false)
            return false;
        outMaterial = impl->currentRenderMaterial;
        return true;
    }

    namespace
    {
        float clampLightChannel(const float value) noexcept
        {
            return std::max(0.0f, std::min(1.0f, value));
        }

        COLOR clampLightColor(const COLOR &color) noexcept
        {
            return COLOR(clampLightChannel(color.r), clampLightChannel(color.g), clampLightChannel(color.b),
                         clampLightChannel(color.a));
        }

        VEC3 normalizeLightDirection(const VEC3 &direction) noexcept
        {
            const float length = direction.length();
            if (length <= 0.000001f)
                return VEC3(0.0f, -0.70710677f, -0.70710677f);
            return direction / length;
        }

        float clampPointRadius(const float value) noexcept
        {
            return std::max(0.0f, value);
        }

        LIGHT_STATE makeDefaultLightState() noexcept
        {
            return LIGHT_STATE();
        }

        LIGHT_MULTI_SETTINGS makeDefaultLightMultiSettings() noexcept
        {
            return LIGHT_MULTI_SETTINGS();
        }

        uint32_t clampRequestedMaxLights(const uint32_t requestedMaxLights) noexcept
        {
            return std::max<uint32_t>(1u, requestedMaxLights);
        }

        uint32_t getSupportedMaxLightsForActiveBackend() noexcept
        {
#if defined(USE_DIRECTX9)
            return DEFAULT_SUPPORTED_MAX_LIGHTS;
#elif defined(USE_METAL)
            return DEFAULT_SUPPORTED_MAX_LIGHTS;
#elif defined(USE_OPENGL_ES)
            return DEFAULT_SUPPORTED_MAX_LIGHTS;
#else
            return DEFAULT_SUPPORTED_MAX_LIGHTS;
#endif
        }

        float getObjectLightSelectionRadius(const VEC3 &objectBoundingAABB) noexcept
        {
            return objectBoundingAABB.length() * 0.5f;
        }

        uint32_t selectPointLightsForObjectImpl(const LIGHT_TARGET target, const VEC3 &objectCenter,
                                                const VEC3 &objectBoundingAABB,
                                                LIGHT_POINT_SELECTION *outSelections,
                                                const uint32_t maxOutSelections) noexcept;
    }

    bool isValidLightTarget(const LIGHT_TARGET target) noexcept
    {
        return target == LIGHT_TARGET_3D || target == LIGHT_TARGET_2DW;
    }

    const char *getLightTargetName(const LIGHT_TARGET target) noexcept
    {
        switch (target)
        {
            case LIGHT_TARGET_3D: return "3d";
            case LIGHT_TARGET_2DW: return "2dw";
            default: return "unknown";
        }
    }

    bool lightTargetFromString(const char *targetName, LIGHT_TARGET &targetOut) noexcept
    {
        if (targetName == nullptr)
            return false;
        if (strcmp(targetName, "3d") == 0)
        {
            targetOut = LIGHT_TARGET_3D;
            return true;
        }
        if (strcmp(targetName, "2dw") == 0)
        {
            targetOut = LIGHT_TARGET_2DW;
            return true;
        }
        return false;
    }

    bool isValidLightSelectionMode(const LIGHT_SELECTION_MODE selectionMode) noexcept
    {
        return selectionMode == LIGHT_SELECTION_PER_OBJECT_NEAREST;
    }

    const char *getLightSelectionModeName(const LIGHT_SELECTION_MODE selectionMode) noexcept
    {
        switch (selectionMode)
        {
            case LIGHT_SELECTION_PER_OBJECT_NEAREST: return "per_object_nearest";
            default: return "unknown";
        }
    }

    bool lightSelectionModeFromString(const char *selectionModeName,
                                      LIGHT_SELECTION_MODE &selectionModeOut) noexcept
    {
        if (selectionModeName == nullptr)
            return false;
        if (strcmp(selectionModeName, "per_object_nearest") == 0)
        {
            selectionModeOut = LIGHT_SELECTION_PER_OBJECT_NEAREST;
            return true;
        }
        return false;
    }

    bool setLightEnabled(const LIGHT_TARGET target, const bool enabled) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        LIGHT_STATE &state = DEVICE_LIGHT_ACCESS::getMutableLightState(device, target);
        state.enabled = enabled;
        return true;
    }

    bool setAmbientLight(const LIGHT_TARGET target, const COLOR &ambientColor) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        LIGHT_STATE &state = DEVICE_LIGHT_ACCESS::getMutableLightState(device, target);
        state.ambientColor = clampLightColor(ambientColor);
        state.ambientConfigured = true;
        return true;
    }

    bool setDirectionalLight(const LIGHT_TARGET target, const VEC3 &directionalDirection,
                             const COLOR &directionalColor) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        LIGHT_STATE &state = DEVICE_LIGHT_ACCESS::getMutableLightState(device, target);
        state.directionalDirection = normalizeLightDirection(directionalDirection);
        state.directionalColor = clampLightColor(directionalColor);
        state.directionalDirectionConfigured = true;
        state.directionalColorConfigured = true;
        return true;
    }

    bool setDirectionalLightDirection(const LIGHT_TARGET target, const VEC3 &directionalDirection) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        LIGHT_STATE &state = DEVICE_LIGHT_ACCESS::getMutableLightState(device, target);
        state.directionalDirection = normalizeLightDirection(directionalDirection);
        state.directionalDirectionConfigured = true;
        return true;
    }

    bool setDirectionalLightColor(const LIGHT_TARGET target, const COLOR &directionalColor) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        LIGHT_STATE &state = DEVICE_LIGHT_ACCESS::getMutableLightState(device, target);
        state.directionalColor = clampLightColor(directionalColor);
        state.directionalColorConfigured = true;
        return true;
    }

    bool setPointLight(const LIGHT_TARGET target, const VEC3 &pointPosition, const float pointRadius,
                       const COLOR &pointColor) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        LIGHT_STATE &state = DEVICE_LIGHT_ACCESS::getMutableLightState(device, target);
        state.pointPosition = pointPosition;
        state.pointRadius = clampPointRadius(pointRadius);
        state.pointColor = clampLightColor(pointColor);
        state.pointPositionConfigured = true;
        state.pointRadiusConfigured = true;
        state.pointColorConfigured = true;
        return true;
    }

    bool setPointLightPosition(const LIGHT_TARGET target, const VEC3 &pointPosition) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        LIGHT_STATE &state = DEVICE_LIGHT_ACCESS::getMutableLightState(device, target);
        state.pointPosition = pointPosition;
        state.pointPositionConfigured = true;
        return true;
    }

    bool setPointLightRadius(const LIGHT_TARGET target, const float pointRadius) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        LIGHT_STATE &state = DEVICE_LIGHT_ACCESS::getMutableLightState(device, target);
        state.pointRadius = clampPointRadius(pointRadius);
        state.pointRadiusConfigured = true;
        return true;
    }

    bool setPointLightColor(const LIGHT_TARGET target, const COLOR &pointColor) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        LIGHT_STATE &state = DEVICE_LIGHT_ACCESS::getMutableLightState(device, target);
        state.pointColor = clampLightColor(pointColor);
        state.pointColorConfigured = true;
        return true;
    }

    bool setRequestedMaxLights(const LIGHT_TARGET target, const uint32_t requestedMaxLights) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        const uint32_t normalizedRequestedMaxLights = clampRequestedMaxLights(requestedMaxLights);
        uint32_t validatedMaxLights = 0u;
        if (validateRequestedMaxLights(target, normalizedRequestedMaxLights, validatedMaxLights) == false)
        {
            DEVICE *device = DEVICE::getInstance();
            ERROR_LOG("Requested max lights [%u] exceeds supported max lights [%u] for backend [%s] target [%s]",
                      normalizedRequestedMaxLights, getSupportedMaxLights(target), device->getBackendEngineName(),
                      getLightTargetName(target));
            return false;
        }
        DEVICE *device = DEVICE::getInstance();
        LIGHT_MULTI_SETTINGS &settings = DEVICE_LIGHT_ACCESS::getMutableLightMultiSettings(device, target);
        settings.requestedMaxLights = validatedMaxLights;
        return true;
    }

    uint32_t getRequestedMaxLights(const LIGHT_TARGET target) noexcept
    {
        if (isValidLightTarget(target) == false)
            return DEFAULT_REQUESTED_MAX_LIGHTS;
        DEVICE *device = DEVICE::getInstance();
        return DEVICE_LIGHT_ACCESS::getLightMultiSettings(device, target).requestedMaxLights;
    }

    uint32_t getSupportedMaxLights(const LIGHT_TARGET target) noexcept
    {
        if (isValidLightTarget(target) == false)
            return 0u;
        return getSupportedMaxLightsForActiveBackend();
    }

    bool validateRequestedMaxLights(const LIGHT_TARGET target, const uint32_t requestedMaxLights,
                                    uint32_t &validatedMaxLightsOut) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        const uint32_t normalizedRequestedMaxLights = clampRequestedMaxLights(requestedMaxLights);
        const uint32_t supportedMaxLights = getSupportedMaxLights(target);
        if (supportedMaxLights == 0u || normalizedRequestedMaxLights > supportedMaxLights)
            return false;
        validatedMaxLightsOut = normalizedRequestedMaxLights;
        return true;
    }

    bool getValidatedMaxLights(const LIGHT_TARGET target, uint32_t &validatedMaxLightsOut) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        return validateRequestedMaxLights(target, getRequestedMaxLights(target), validatedMaxLightsOut);
    }

    bool setLightSelectionMode(const LIGHT_TARGET target,
                               const LIGHT_SELECTION_MODE selectionMode) noexcept
    {
        if (isValidLightTarget(target) == false || isValidLightSelectionMode(selectionMode) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        LIGHT_MULTI_SETTINGS &settings = DEVICE_LIGHT_ACCESS::getMutableLightMultiSettings(device, target);
        settings.selectionMode = selectionMode;
        return true;
    }

    LIGHT_SELECTION_MODE getLightSelectionMode(const LIGHT_TARGET target) noexcept
    {
        if (isValidLightTarget(target) == false)
            return LIGHT_SELECTION_PER_OBJECT_NEAREST;
        DEVICE *device = DEVICE::getInstance();
        return DEVICE_LIGHT_ACCESS::getLightMultiSettings(device, target).selectionMode;
    }

    bool addPointLight(const LIGHT_TARGET target, const VEC3 &pointPosition, const float pointRadius,
                       const COLOR &pointColor) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        LIGHT_POINT pointLight;
        pointLight.position = pointPosition;
        pointLight.radius = clampPointRadius(pointRadius);
        pointLight.color = clampLightColor(pointColor);
        DEVICE *device = DEVICE::getInstance();
        DEVICE_LIGHT_ACCESS::getMutablePointLights(device, target).push_back(pointLight);
        return true;
    }

    bool clearPointLights(const LIGHT_TARGET target) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        DEVICE_LIGHT_ACCESS::getMutablePointLights(device, target).clear();
        return true;
    }

    uint32_t getTotalPointLights(const LIGHT_TARGET target) noexcept
    {
        if (isValidLightTarget(target) == false)
            return 0u;
        DEVICE *device = DEVICE::getInstance();
        return static_cast<uint32_t>(DEVICE_LIGHT_ACCESS::getPointLights(device, target).size());
    }

    bool getPointLightAt(const LIGHT_TARGET target, const uint32_t index, LIGHT_POINT &outLight) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        const std::vector<LIGHT_POINT> &pointLights = DEVICE_LIGHT_ACCESS::getPointLights(device, target);
        if (index >= pointLights.size())
            return false;
        outLight = pointLights[index];
        return true;
    }

    uint32_t selectPointLightsForObject(const LIGHT_TARGET target, const VEC3 &objectCenter,
                                        const VEC3 &objectBoundingAABB,
                                        LIGHT_POINT_SELECTION *outSelections,
                                        const uint32_t maxOutSelections) noexcept
    {
        return selectPointLightsForObjectImpl(target, objectCenter, objectBoundingAABB, outSelections,
                                              maxOutSelections);
    }

    namespace
    {
        uint32_t selectPointLightsForObjectImpl(const LIGHT_TARGET target, const VEC3 &objectCenter,
                                                const VEC3 &objectBoundingAABB,
                                                LIGHT_POINT_SELECTION *outSelections,
                                                const uint32_t maxOutSelections) noexcept
        {
            if (isValidLightTarget(target) == false || outSelections == nullptr || maxOutSelections == 0u)
                return 0u;

            uint32_t validatedMaxLights = 0u;
            if (getValidatedMaxLights(target, validatedMaxLights) == false || validatedMaxLights == 0u)
                return 0u;

            const LIGHT_SELECTION_MODE selectionMode = getLightSelectionMode(target);
            if (selectionMode != LIGHT_SELECTION_PER_OBJECT_NEAREST)
                return 0u;

            const float objectRadius = getObjectLightSelectionRadius(objectBoundingAABB);
            std::vector<LIGHT_POINT_SELECTION> candidates;
            DEVICE *device = DEVICE::getInstance();
            const std::vector<LIGHT_POINT> &pointLights = DEVICE_LIGHT_ACCESS::getPointLights(device, target);
            if (pointLights.empty())
            {
                // Fallback to the single legacy point-light slot (setPointLight/setPointLightPosition/
                // setPointLightRadius/setPointLightColor) when addPointLight was never called for this target.
                // Without this branch, a game that only ever used setPointLight would silently select zero
                // lights here, since this function was originally list-only.
                const LIGHT_STATE &lightState = DEVICE_LIGHT_ACCESS::getLightState(device, target);
                LIGHT_POINT_SELECTION selection;
                selection.pointLight.position = lightState.pointPosition;
                selection.pointLight.radius = lightState.pointRadius;
                selection.pointLight.color = lightState.pointColor;
                const float distanceToObjectCenter = (selection.pointLight.position - objectCenter).length();
                const float reach = selection.pointLight.radius + objectRadius;
                if (distanceToObjectCenter > reach)
                    return 0u;
                selection.sourceIndex = 0u;
                selection.distanceToObjectCenter = distanceToObjectCenter;
                candidates.push_back(selection);
            }
            else
            {
                candidates.reserve(pointLights.size());

                for (std::vector<LIGHT_POINT>::size_type i = 0; i < pointLights.size(); ++i)
                {
                    const LIGHT_POINT &pointLight = pointLights[i];
                    const float distanceToObjectCenter = (pointLight.position - objectCenter).length();
                    const float reach = pointLight.radius + objectRadius;
                    if (distanceToObjectCenter > reach)
                        continue;

                    LIGHT_POINT_SELECTION selection;
                    selection.pointLight = pointLight;
                    selection.sourceIndex = static_cast<uint32_t>(i);
                    selection.distanceToObjectCenter = distanceToObjectCenter;
                    candidates.push_back(selection);
                }
            }

            if (candidates.empty())
                return 0u;

            std::stable_sort(candidates.begin(), candidates.end(),
                             [](const LIGHT_POINT_SELECTION &a, const LIGHT_POINT_SELECTION &b) noexcept
                             {
                                 if (a.distanceToObjectCenter != b.distanceToObjectCenter)
                                     return a.distanceToObjectCenter < b.distanceToObjectCenter;
                                 return a.sourceIndex < b.sourceIndex;
                             });

            const uint32_t maxSelectedLights = std::min<uint32_t>(validatedMaxLights, maxOutSelections);
            const uint32_t totalSelectedLights = std::min<uint32_t>(maxSelectedLights,
                                                                    static_cast<uint32_t>(candidates.size()));
            for (uint32_t i = 0; i < totalSelectedLights; ++i)
            {
                outSelections[i] = candidates[i];
            }
            return totalSelectedLights;
        }
    }

    bool resetLight(const LIGHT_TARGET target) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        DEVICE_LIGHT_ACCESS::getMutableLightState(device, target) = makeDefaultLightState();
        DEVICE_LIGHT_ACCESS::getMutableLightMultiSettings(device, target) = makeDefaultLightMultiSettings();
        DEVICE_LIGHT_ACCESS::getMutablePointLights(device, target).clear();
        return true;
    }

    void resetAllLights() noexcept
    {
        DEVICE *device = DEVICE::getInstance();
        DEVICE_LIGHT_ACCESS::getMutableLightState(device, LIGHT_TARGET_3D) = makeDefaultLightState();
        DEVICE_LIGHT_ACCESS::getMutableLightState(device, LIGHT_TARGET_2DW) = makeDefaultLightState();
        DEVICE_LIGHT_ACCESS::getMutableLightMultiSettings(device, LIGHT_TARGET_3D) = makeDefaultLightMultiSettings();
        DEVICE_LIGHT_ACCESS::getMutableLightMultiSettings(device, LIGHT_TARGET_2DW) = makeDefaultLightMultiSettings();
        DEVICE_LIGHT_ACCESS::getMutablePointLights(device, LIGHT_TARGET_3D).clear();
        DEVICE_LIGHT_ACCESS::getMutablePointLights(device, LIGHT_TARGET_2DW).clear();
    }

    bool getLightState(const LIGHT_TARGET target, LIGHT_STATE &outState) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        outState = DEVICE_LIGHT_ACCESS::getLightState(device, target);
        return true;
    }

    const LIGHT_STATE &getLightState(const LIGHT_TARGET target) noexcept
    {
        DEVICE *device = DEVICE::getInstance();
        return DEVICE_LIGHT_ACCESS::getLightState(device, target);
    }

    bool getLightMultiSettings(const LIGHT_TARGET target, LIGHT_MULTI_SETTINGS &outSettings) noexcept
    {
        if (isValidLightTarget(target) == false)
            return false;
        DEVICE *device = DEVICE::getInstance();
        outSettings = DEVICE_LIGHT_ACCESS::getLightMultiSettings(device, target);
        return true;
    }

    const LIGHT_MULTI_SETTINGS &getLightMultiSettings(const LIGHT_TARGET target) noexcept
    {
        DEVICE *device = DEVICE::getInstance();
        return DEVICE_LIGHT_ACCESS::getLightMultiSettings(device, target);
    }

    void DEVICE::setTotalObjectsOnFrustum3D(const uint32_t total) noexcept
    {
        impl->totalObjectsOnFrustum3D = total;
    }

    void DEVICE::setTotalObjectsOnFrustum2D(const uint32_t total) noexcept
    {
        impl->totalObjectsOnFrustum2D = total;
    }

    void DEVICE::setTotalObjectsIsRendering3D(const uint32_t total) noexcept
    {
        impl->totalObjectsIsRendering3D = total;
    }

    void DEVICE::setTotalObjectsIsRendering2D(const uint32_t total) noexcept
    {
        impl->totalObjectsIsRendering2D = total;
    }

    void DEVICE::incrementTotalObjectsIsRendering3D() noexcept
    {
        ++impl->totalObjectsIsRendering3D;
    }

    void DEVICE::incrementTotalObjectsIsRendering2D() noexcept
    {
        ++impl->totalObjectsIsRendering2D;
    }

    void DEVICE::setTotalObjects3D(const uint32_t total) noexcept
    {
        impl->totalObjects3D = total;
    }

    void DEVICE::setTotalObjects2D(const uint32_t total) noexcept
    {
        impl->totalObjects2D = total;
    }

    void DEVICE::setNearFrustumDimension(const VEC3 &dimension) noexcept
    {
        impl->dimNearFrustum3d = dimension;
    }

    void DEVICE::setFarFrustumDimension(const VEC3 &dimension) noexcept
    {
        impl->dimFarFrustum3d = dimension;
    }

    void DEVICE::setNearFrustumDimensionX(const float value) noexcept
    {
        impl->dimNearFrustum3d.x = value;
    }

    void DEVICE::setNearFrustumDimensionY(const float value) noexcept
    {
        impl->dimNearFrustum3d.y = value;
    }

    void DEVICE::setFarFrustumDimensionX(const float value) noexcept
    {
        impl->dimFarFrustum3d.x = value;
    }

    void DEVICE::setFarFrustumDimensionY(const float value) noexcept
    {
        impl->dimFarFrustum3d.y = value;
    }

    uint32_t DEVICE::getTotalPhysics() const noexcept
    {
        return static_cast<uint32_t>(impl->physics.size());
    }

    PHYSICS * DEVICE::getPhysics(const uint32_t index) const noexcept
    {
        return index < impl->physics.size() ? impl->physics[index] : nullptr;
    }

    uint32_t DEVICE::getTotalRenderTargets() const noexcept
    {
        return static_cast<uint32_t>(impl->renderTargets.size());
    }

    RENDERIZABLE_TO_TARGET * DEVICE::getRenderTarget(const uint32_t index) const noexcept
    {
        return index < impl->renderTargets.size() ? impl->renderTargets[index] : nullptr;
    }

    std::vector<RENDERIZABLE *> & DEVICE::getRender3DList() noexcept
    {
        return impl->render3D;
    }

    std::vector<RENDERIZABLE *> & DEVICE::getRender2DWList() noexcept
    {
        return impl->render2DW;
    }

    std::vector<RENDERIZABLE *> & DEVICE::getRender2DSList() noexcept
    {
        return impl->render2DS;
    }

    void DEVICE::setAppReturnCode(const int returnCode) noexcept
    {
        impl->returnCodeApp = returnCode;
    }

    int DEVICE::getAppReturnCode() const noexcept
    {
        return impl->returnCodeApp;
    }

    void DEVICE::setRun(const bool run) noexcept
    {
        impl->run = run;
    }

    bool DEVICE::isRunning() const noexcept
    {
        return impl->run;
    }

    void DEVICE::setCoreManager(CORE_MANAGER *manager) noexcept
    {
        impl->ptrManager = manager;
    }

    CORE_MANAGER * DEVICE::getCoreManager() const noexcept
    {
        return impl->ptrManager;
    }

    SHADER_CFG_LOADER & DEVICE::getShaderConfig() noexcept
    {
        return impl->cfg;
    }

    const SHADER_CFG_LOADER & DEVICE::getShaderConfig() const noexcept
    {
        return impl->cfg;
    }

    mbm::ORDER_RENDER & DEVICE::getOrderRender() noexcept
    {
        return impl->orderRender;
    }

    const mbm::ORDER_RENDER & DEVICE::getOrderRender() const noexcept
    {
        return impl->orderRender;
    }

    std::map<std::string, DYNAMIC_VAR *> & DEVICE::getDynamicVars() noexcept
    {
        return impl->lsDynamicVarGlobal;
    }

    const std::map<std::string, DYNAMIC_VAR *> & DEVICE::getDynamicVars() const noexcept
    {
        return impl->lsDynamicVarGlobal;
    }

    void DEVICE::setBackBufferSize(const float width, const float height) noexcept
    {
        impl->backBufferWidth = width;
        impl->backBufferHeight = height;
    }

    void DEVICE::setBackBufferWidth(const float width) noexcept
    {
        impl->backBufferWidth = width;
    }

    void DEVICE::setBackBufferHeight(const float height) noexcept
    {
        impl->backBufferHeight = height;
    }

    float DEVICE::getBackBufferWidth() const noexcept
    {
        return impl->backBufferWidth;
    }
    
    float DEVICE::getBackBufferHeight() const noexcept
    {
        return impl->backBufferHeight;
    }
    
    float DEVICE::getScaleBackBufferWidth() const noexcept
    {
        return static_cast<float>(impl->backBufferWidth / impl->camera.scale2d.x);
    }
    
    float DEVICE::getScaleBackBufferHeight() const noexcept
    {
        return static_cast<float>(impl->backBufferHeight / impl->camera.scale2d.y);
    }

    uint32_t DEVICE::getTotalObjectsOnFrustum3D() const noexcept
    {
        return impl->totalObjectsOnFrustum3D;
    }

    uint32_t DEVICE::getTotalObjectsOnFrustum2D() const noexcept
    {
        return impl->totalObjectsOnFrustum2D;
    }

    uint32_t DEVICE::getTotalObjectsIsRendering3D() const noexcept
    {
        return impl->totalObjectsIsRendering3D;
    }

    uint32_t DEVICE::getTotalObjectsIsRendering2D() const noexcept
    {
        return impl->totalObjectsIsRendering2D;
    }

    uint32_t DEVICE::getTotalObjects3D() const noexcept
    {
        return impl->totalObjects3D;
    }

    uint32_t DEVICE::getTotalObjects2D() const noexcept
    {
        return impl->totalObjects2D;
    }

    void DEVICE::setVerbose(const bool verbose) noexcept
    {
        impl->verbose = verbose;
    }

    bool DEVICE::isVerbose() const noexcept
    {
        return impl->verbose;
    }

    void DEVICE::setColorClearBackGround(const COLOR &color) noexcept
    {
        impl->colorClearBackGround = color;
    }

    const COLOR & DEVICE::getColorClearBackGround() const noexcept
    {
        return impl->colorClearBackGround;
    }

    void DEVICE::setClearBackGround(const bool clear) noexcept
    {
        impl->clearBackGround = clear;
    }

    bool DEVICE::isClearBackGroundEnabled() const noexcept
    {
        return impl->clearBackGround;
    }

    void DEVICE::setStopScriptOnError(const bool stop) noexcept
    {
        impl->stopScriptOnError = stop;
    }

    bool DEVICE::isStopScriptOnErrorEnabled() const noexcept
    {
        return impl->stopScriptOnError;
    }

    void DEVICE::resetSwapBackBufferStep() noexcept
    {
        impl->swapBackBufferStep = 3;
    }

    void DEVICE::incrementSwapBackBufferStep() noexcept
    {
        ++impl->swapBackBufferStep;
    }

    int DEVICE::getSwapBackBufferStep() const noexcept
    {
        return impl->swapBackBufferStep;
    }

    void DEVICE::setWindowPosition(const int x, const int y) noexcept
    {
        impl->windowPositionX = x;
        impl->windowPositionY = y;
    }

    void DEVICE::setWindowPositionX(const int x) noexcept
    {
        impl->windowPositionX = x;
    }

    void DEVICE::setWindowPositionY(const int y) noexcept
    {
        impl->windowPositionY = y;
    }

    int DEVICE::getWindowPositionX() const noexcept
    {
        return impl->windowPositionX;
    }

    int DEVICE::getWindowPositionY() const noexcept
    {
        return impl->windowPositionY;
    }
    
    void DEVICE::scaleToScreen(const float widthScreen, const float heightScreen,
                              const char *stretch) noexcept // stretch: x, y xy nullptr
    {
        if (widthScreen != 0.0f && heightScreen != 0.0f)
        {
            const float percx = impl->backBufferWidth / widthScreen;
            const float percy = impl->backBufferHeight / heightScreen;
            if (percx != 0.0f && percy != 0.0f)
            {
                CAMERA &camera = impl->camera;
                camera.expectedScreen.x = widthScreen;
                camera.expectedScreen.y = heightScreen;
                if (stretch)
                {
                    if (strcmp(stretch, "x") == 0)
                    {
                        camera.scale2d.x = percx;
                        camera.scale2d.y = percx;
                        strncpy(camera.stretch, "x",sizeof(camera.stretch)-1);
                    }
                    else if (strcmp(stretch, "y") == 0)
                    {
                        camera.scale2d.x = percy;
                        camera.scale2d.y = percy;
                        strncpy(camera.stretch, "y",sizeof(camera.stretch)-1);
                    }
                    else if (strcmp(stretch, "xy") == 0)
                    {
                        camera.scale2d.x = percx;
                        camera.scale2d.y = percy;
                        strncpy(camera.stretch, "xy",sizeof(camera.stretch));
                    }
                    else if (percx < percy)
                    {
                        camera.scale2d.x = percx;
                        camera.scale2d.y = percx;
                        strncpy(camera.stretch, "x",sizeof(camera.stretch)-1);
                    }
                    else
                    {
                        camera.scale2d.x = percy;
                        camera.scale2d.y = percy;
                        strncpy(camera.stretch, "y",sizeof(camera.stretch)-1);
                    }
                }
                else if (percx < percy)
                {
                    camera.scale2d.x = percx;
                    camera.scale2d.y = percx;
                    strncpy(camera.stretch, "x",sizeof(camera.stretch)-1);
                }
                else
                {
                    camera.scale2d.x = percy;
                    camera.scale2d.y = percy;
                    strncpy(camera.stretch, "y",sizeof(camera.stretch)-1);
                }
            }
        }
    }

    bool DEVICE::isGamePaused() const noexcept
    {
        return impl->isGamePaused;
    }

    bool DEVICE::isPixelPerfectRendering() const noexcept
    {
        return impl->pixelPerfectRenderingActive;
    }
    
    void DEVICE::pauseGame()
    {
        impl->isGamePaused = true;
        this->pauseTimer();
        if(impl->audioInterface)
            impl->audioInterface->pauseAll(impl->scene ? impl->scene->getIdScene() : 0);
    }
    
    void DEVICE::resumeGame()
    {
        impl->isGamePaused = false;
        this->resumeTimer();
        if (impl->audioInterface)
            impl->audioInterface->resumeAll(impl->scene ? impl->scene->getIdScene() : 0);
    }
    
    void DEVICE::addPhysics(PHYSICS *physics)
    {
        if (physics)
            impl->physics.push_back(physics);
    }
    
    void DEVICE::removePhysics(PHYSICS *physics)
    {
        for (std::vector<PHYSICS *>::size_type i = 0; i < impl->physics.size(); ++i)
        {
            PHYSICS *ptrPhysics = impl->physics[i];
            if (ptrPhysics == physics)
            {
                impl->physics.erase(impl->physics.begin() + std::vector<PHYSICS *>::difference_type(i));
                break;
            }
        }
    }
    
    void DEVICE::addRenderizable(RENDERIZABLE *renderizable)
    {
        if (renderizable != nullptr)
        {
            VEC3 &position = renderizable->getPosition();
            const bool is2dScreen = renderizable->is2dScreenObject();
            const TYPE_CLASS typeClass = renderizable->getTypeClass();
            if (renderizable->is3DObject())
            {
                if (position.z == 0.0f)
                    position.z = impl->orderRender.getNextZOrderControl3d();
                impl->render3D.push_back(renderizable);
            }
            else if (is2dScreen)
            {
                if (position.z == 0.0f)
                    position.z = impl->orderRender.getNextZOrderControl2d(
                        is2dScreen, typeClass == TYPE_CLASS_TEXT);
                impl->render2DS.push_back(renderizable);
            }
            else
            {
                if (position.z == 0.0f)
                    position.z = impl->orderRender.getNextZOrderControl2d(
                        is2dScreen, typeClass == TYPE_CLASS_TEXT);
                impl->render2DW.push_back(renderizable);
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
            impl->renderTargets.push_back(ObjectRenderTarget);
        }
        else
        {
            PRINT_IF_DEBUG( "error on add renderizable ");
        }
    }
    
    void DEVICE::removeObjectRender2Texture(RENDERIZABLE_TO_TARGET *object)
    {
        for (std::vector<RENDERIZABLE_TO_TARGET *>::size_type i = 0; i < impl->renderTargets.size(); ++i)
        {
            RENDERIZABLE_TO_TARGET *ptr = impl->renderTargets[i];
            if (ptr == object)
            {
                for (auto ph : impl->physics)
                {
                    ph->removeObject(ptr);
                }
                impl->renderTargets.erase(impl->renderTargets.begin() + std::vector<RENDERIZABLE_TO_TARGET *>::difference_type(i));
                break;
            }
        }
    }
    
    void DEVICE::disableAllButThis(mbm::RENDERIZABLE *draw)
    {
        for (auto ptr : impl->render3D)
        {
            ptr->setEnableRender(false);
        }
        for (auto ptr : impl->render2DS)
        {
            ptr->setEnableRender(false);
        }
        for (auto ptr : impl->render2DW)
        {
            ptr->setEnableRender(false);
        }
        draw->setEnableRender(true);
    }
    
    void DEVICE::removeObjectByIdSceneScene(const int idScene)
    {
        for (auto ph : impl->physics)
        {
            ph->removeObjectByIdSceneScene(idScene);
        }
        for (std::vector<RENDERIZABLE *>::size_type i = 0; i < impl->render3D.size(); ++i)
        {
            RENDERIZABLE *ptr = impl->render3D[i];
            if (ptr->getIdScene() == idScene)
            {
                impl->render3D.erase(impl->render3D.begin() + std::vector<RENDERIZABLE *>::difference_type(i));
                i--;
            }
        }
        for (std::vector<RENDERIZABLE *>::size_type i = 0; i < impl->render2DW.size(); ++i)
        {
            RENDERIZABLE *ptr = impl->render2DW[i];
            if (ptr->getIdScene() == idScene)
            {
                impl->render2DW.erase(impl->render2DW.begin() + std::vector<RENDERIZABLE *>::difference_type(i));
                i--;
            }
        }
        for (std::vector<RENDERIZABLE *>::size_type i = 0; i < impl->render2DS.size(); ++i)
        {
            RENDERIZABLE *ptr = impl->render2DS[i];
            if (ptr->getIdScene() == idScene)
            {
                impl->render2DS.erase(impl->render2DS.begin() + std::vector<RENDERIZABLE *>::difference_type(i));
                i--;
            }
        }
    }
    
    void DEVICE::stopRender2Texture2(RENDERIZABLE *ptr)
    {
        for (auto r : impl->renderTargets)
        {
            r->removeFromRender2Texture(ptr);
        }
    }
    
    void DEVICE::removeRenderizable(RENDERIZABLE *object)
    {
        if (object == nullptr)
            return;
        for (auto ph : impl->physics)
        {
            ph->removeObject(object);
        }
        // Always evict from any RENDER_2_TEXTURE lists, regardless of the isRender2Texture flag,
        // to prevent stale (dangling) pointers from remaining in those lists after this object is freed.
        this->stopRender2Texture2(object);

        if (object->is3DObject())
        {
            for (std::vector<RENDERIZABLE *>::size_type i = 0; i < impl->render3D.size(); ++i)
            {
                RENDERIZABLE *ptr = impl->render3D[i];
                if (ptr == object)
                {
                    for (auto ph : impl->physics)
                    {
                        ph->removeObject(ptr);
                    }
                    impl->render3D.erase(impl->render3D.begin() + std::vector<RENDERIZABLE *>::difference_type(i));
                    return;
                }
            }
        }
        else if (object->is2dScreenObject() == false)
        {
            for (std::vector<RENDERIZABLE *>::size_type i = 0; i < impl->render2DW.size(); ++i)
            {
                RENDERIZABLE *ptr = impl->render2DW[i];
                if (ptr == object)
                {
                    for (auto ph : impl->physics)
                    {
                        ph->removeObject(ptr);
                    }
                    impl->render2DW.erase(impl->render2DW.begin() + std::vector<RENDERIZABLE *>::difference_type(i));
                    break;
                }
            }
        }
        else
        {
            for (std::vector<RENDERIZABLE *>::size_type i = 0; i < impl->render2DS.size(); ++i)
            {
                RENDERIZABLE *ptr = impl->render2DS[i];
                if (ptr == object)
                {
                    for (auto ph : impl->physics)
                    {
                        ph->removeObject(ptr);
                    }
                    impl->render2DS.erase(impl->render2DS.begin() + std::vector<RENDERIZABLE *>::difference_type(i));
                    break;
                }
            }
        }
    }

    bool DEVICE::rayCast(const float sx, const float sy, VEC3 *rayOriginOut, VEC3 *rayDir) const
    {
        // two ways to do it ...
        const CAMERA &camera = impl->camera;
        const float vx = (sx /  impl->backBufferWidth - 0.5f) * 2.0f / camera.matrixProj._11;
        const float vy = -(sy / impl->backBufferHeight - 0.5f) * 2.0f / camera.matrixProj._22;
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
        const float vx =  (sx/impl->backBufferWidth  - 0.5f) * 2.0f; // [0,1024] -> [-1,1]
        const float vy = -(sy/impl->backBufferHeight - 0.5f) * 2.0f; // [0, 768] -> [-1,1]
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

    // Standard slab-method ray/AABB intersection test. `rayDir` need not be normalized -- the
    // slab method only compares ratios of intersection parameters (tMin/tMax) against each other
    // and against zero, so any non-zero-length direction produces the same true/false result.
    // This replaces the previous 3D collide(x,y) approach, which unprojected the screen point at
    // a *guessed* depth (the object's own world Z, treated as if it approximated distance from
    // the camera) and then did a point-in-box test -- that guess degenerates to the camera's own
    // position for any object near world Z=0 (see onCheckCollisionBoundingBoxRenderizable's
    // history), and is generally inaccurate for any object whose Z doesn't happen to match its
    // real camera distance -- inherent to a free-orbiting camera, not just an edge case. A real
    // ray/box test has no such guess to get wrong.
    bool DEVICE::rayIntersectsAABB(const VEC3 &rayOrigin, const VEC3 &rayDir, const VEC3 &boxCenter,
                                    const float w, const float h, const float d) const noexcept
    {
        const float halfW = w * 0.5f;
        const float halfH = h * 0.5f;
        const float halfD = d * 0.5f;
        const float minB[3] = {boxCenter.x - halfW, boxCenter.y - halfH, boxCenter.z - halfD};
        const float maxB[3] = {boxCenter.x + halfW, boxCenter.y + halfH, boxCenter.z + halfD};
        const float origin[3] = {rayOrigin.x, rayOrigin.y, rayOrigin.z};
        const float dir[3]    = {rayDir.x, rayDir.y, rayDir.z};

        float tMin = -std::numeric_limits<float>::max();
        float tMax = std::numeric_limits<float>::max();

        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::abs(dir[axis]) < 1e-8f)
            {
                // Ray is parallel to this slab -- only a hit if the origin already lies within it.
                if (origin[axis] < minB[axis] || origin[axis] > maxB[axis])
                    return false;
                continue;
            }
            float t1 = (minB[axis] - origin[axis]) / dir[axis];
            float t2 = (maxB[axis] - origin[axis]) / dir[axis];
            if (t1 > t2)
                std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax)
                return false;
        }
        // The box must be in front of (or at) the ray origin, not only behind it.
        return tMax >= 0.0f;
    }

    bool DEVICE::transformeScreen2dToWorld3d_scaled(const float x, const float y, VEC3 *out,
                                                         const float howFarZFromCamera) const
    {
        VEC3        rayOriginOut, rayDirOut;
        const CAMERA &camera = impl->camera;
        const float newX = x * camera.scaleScreen2d.x;
        const float newY = y * camera.scaleScreen2d.y;
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
        //original
        const CAMERA &camera = impl->camera;
        const VEC2 middle(impl->backBufferWidth * 0.5f, impl->backBufferHeight * 0.5f);
        out.x = (x * camera.scaleScreen2d.x) - middle.x + camera.position2d.x;
        out.y = -((y * camera.scaleScreen2d.y) - middle.y) + camera.position2d.y;
        out.x *= impl->percXcam2dScale;
        out.y *= impl->percYcam2dScale;

        // x, y are already in expected screen coordinates (divided by scale2d by caller)
        //const VEC2 middle(camera.expectedScreen.x * 0.5f, camera.expectedScreen.y * 0.5f);
        //out.x = x - middle.x + camera.position2d.x;
        //out.y = -(y - middle.y) + camera.position2d.y;
        //out.x *= impl->percXcam2dScale;
        //out.y *= impl->percYcam2dScale;
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
        //original
        const CAMERA &camera = impl->camera;
        const VEC2 newIn(x / impl->percXcam2dScale, y / impl->percYcam2dScale);
        const VEC2 middle(impl->backBufferWidth * 0.5f, impl->backBufferHeight * 0.5f);
        out.x = newIn.x + middle.x - camera.position2d.x;
        out.y = impl->backBufferHeight - ((newIn.y + middle.y) - camera.position2d.y);
        out.x /= camera.scaleScreen2d.x;
        out.y /= camera.scaleScreen2d.y;

        //const VEC2 newIn(x / impl->percXcam2dScale, y / impl->percYcam2dScale);
        //const VEC2 middle(camera.expectedScreen.x * 0.5f, camera.expectedScreen.y * 0.5f);
        //// Output in expected screen coordinates (caller should multiply by scale2d if actual pixels needed)
        //out.x = newIn.x + middle.x - camera.position2d.x;
        //out.y = camera.expectedScreen.y - ((newIn.y + middle.y) - camera.position2d.y);
    }
    
    bool DEVICE::isPointWorld2dOnScreen2D(const float x, const float y) const noexcept
    {
        VEC2 onScreen(x, y);
        this->transformeWorld2dToScreen2d_scaled(x, y, onScreen);
        if (onScreen.x < 0)
            return false;
        else if (onScreen.x > impl->backBufferWidth * impl->percXcam2dScale)
            return false;
        else if (onScreen.y < 0)
            return false;
        else if (onScreen.y > impl->backBufferHeight * impl->percYcam2dScale)
            return false;
        return true;
    }
    
    bool DEVICE::isCircleScreen2dOnScreen2D_scaled(const float x, const float y, const float ray) const noexcept
    {
        const CAMERA &camera = impl->camera;
        const float newX = camera.scaleScreen2d.x * x;
        const float newY = camera.scaleScreen2d.y * y;
        if ((newX + ray) < 0)
            return false;
        else if ((newX - ray) > impl->backBufferWidth)
            return false;
        else if ((newY + ray) < 0)
            return false;
        else if ((newY - ray) > impl->backBufferHeight)
            return false;
        return true;
    }
    
    bool DEVICE::isCircleScreen2dOnScreen2D(const float x, const float y, const float ray) const noexcept
    {
        if ((x + ray) < 0)
            return false;
        else if ((x - ray) > impl->backBufferWidth)
            return false;
        else if ((y + ray) < 0)
            return false;
        else if ((y - ray) > impl->backBufferHeight)
            return false;
        return true;
    }
    
    bool DEVICE::isCircleWorld2dOnScreen2D_scaled(const float x, const float y, const float ray) const noexcept
    {
        VEC2 onScreen(x, y);
        this->transformeWorld2dToScreen2d_scaled(x, y, onScreen);
        if ((onScreen.x + ray) < 0)
            return false;
        else if ((onScreen.x - ray) > impl->backBufferWidth * impl->percXcam2dScale)
            return false;
        else if ((onScreen.y + ray) < 0)
            return false;
        else if ((onScreen.y - ray) > impl->backBufferHeight * impl->percYcam2dScale)
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
        else if (onScreen.x > impl->backBufferWidth)
            return false;
        else if (onScreen.y < 0)
            return false;
        else if (onScreen.y > impl->backBufferHeight)
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
        *dimNear = impl->dimNearFrustum3d;
        *dimFar  = impl->dimFarFrustum3d;
    }
    
    void DEVICE::setBillboard(MATRIX *out, VEC3 *position , VEC3 *scale)
    {
        if (out)
        {
            MATRIX matrixAux;
            *out = impl->camera.matrixBillboard;
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
    
    void DEVICE::setAudioManagerInterface(AUDIO_MANAGER_INTERFACE* _audioInterface)
    {
        impl->audioInterface = _audioInterface;
    }

    AUDIO_MANAGER_INTERFACE* DEVICE::getAudioManagerInterface() const noexcept
    {
        return impl->audioInterface;
    }

    void * DEVICE::get_lua_state()//if we are using lua we should be able to retrieve the current state
    {
        if(impl->scene)
            return impl->scene->get_lua_state();
        return nullptr;
    }

    DEVICE::~DEVICE()
    {
        for (const auto & i : impl->lsDynamicVarGlobal)
        {
            DYNAMIC_VAR *dVar = i.second;
            delete dVar;
        }
        impl->lsDynamicVarGlobal.clear();
        this->destroySpecificContext();
    }

    void DEVICE::refreshDevice()
    {
        //force refresh window by sending resize event
        const int newWidth     = static_cast<int>(impl->backBufferWidth);
        const int newHeight    = static_cast<int>(impl->backBufferHeight);
        impl->backBufferWidth  = static_cast<float>(newWidth + 1);
        impl->ptrManager->onResizeWindow(newWidth, newHeight);
    }
}

mbm::DEVICE *          mbm::DEVICE::instanceDevice                   = nullptr;
