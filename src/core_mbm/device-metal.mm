/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#include <device.h>

#if defined(USE_METAL)

#include <scene.h>
#include <texture-manager.h>
#include <audio-interface.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <specific-metal.h>
#include <dynamic-var.h>

namespace mbm
{
    void DEVICE::initializeSpecificContext()
    {
        this->destroySpecificContext();
        this->specificContextDevice = new SPECIFIC_AUX_CONTEXT_DEVICE();
    }

    void DEVICE::destroySpecificContext()
    {
        if (this->specificContextDevice)
        {
            delete this->specificContextDevice;
            this->specificContextDevice = nullptr;
        }
    }

    void DEVICE::quit()
    {
        TEXTURE_MANAGER::release();
        MESH_MANAGER::release();
        releaseAudioManager();
        if (instanceDevice)
        {
            constexpr bool wasDeviceLost = false;
            instanceDevice->specificContextDevice->release(wasDeviceLost);
            delete instanceDevice;
        }
        instanceDevice = nullptr;
    }

    void DEVICE::setDephtTest(const bool /*enable*/)
    {
        // Metal: depth testing is configured in MTLDepthStencilState baked into
        // the pipeline state. A full implementation would rebuild/swap pipeline
        // states here. For Milestone 1 (clear-screen), this is a no-op.
    }

    void DEVICE::clearDepth()
    {
        // Metal: the actual clear happens via the render-pass descriptor in
        // CORE_MANAGER::beginRender(). Just set the pending flag.
        if (this->specificContextDevice)
            this->specificContextDevice->pendingClearDepth = true;
    }

    void DEVICE::clearDepthColored()
    {
        // colorClearBackGround is already updated by the caller before invoking this.
        // beginRender() reads it when building the MTLRenderPassDescriptor.
        if (this->specificContextDevice)
            this->specificContextDevice->pendingClearDepth = true;
    }

    const char* DEVICE::getBackendEngineName() const noexcept
    {
        return "Metal";
    }

    const char* DEVICE::getBackendEngineVersion() const noexcept
    {
        if (this->specificContextDevice && this->specificContextDevice->mtlDevice)
        {
            static std::string ver;
            ver = "\nMetal device: ";
            ver += [this->specificContextDevice->mtlDevice.name UTF8String];
            return ver.c_str();
        }
        return "\nMetal (device not initialised)";
    }

    void DEVICE::setProjectionMode(const bool is3D, const float width, const float height)
    {
        // Update Metal layer drawable size to match the requested resolution.
        if (this->specificContextDevice && this->specificContextDevice->metalLayer && width > 0 && height > 0)
        {
            this->specificContextDevice->metalLayer.drawableSize =
                CGSizeMake(static_cast<CGFloat>(width), static_cast<CGFloat>(height));
        }
        if (width > 0)
            backBufferWidth = width;
        if (height > 0)
            backBufferHeight = height;
        if (width > 0 && height > 0)
            this->camera.updateCam(is3D, static_cast<float>(width), static_cast<float>(height));
    }

    const char* DEVICE::copyFileFromAsset(const char* assetName, const char* /*mode*/)
    {
        // macOS: assets are regular filesystem files — return path unchanged.
        return assetName;
    }

    void DEVICE::disableFilteringForPixelPerfect() noexcept
    {
        _pixelPerfectRenderingActive = true;
        // Metal: texture sampling is configured in MTLSamplerState baked at
        // pipeline setup time. Full pixel-perfect support requires switching
        // sampler states. Stored as a flag for future pipeline-state management.
    }

    void DEVICE::enableFilteringAfterPixelPerfect() noexcept
    {
        _pixelPerfectRenderingActive = false;
    }

} // namespace mbm

#endif // USE_METAL
