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
        setSpecificContextDevice(new SPECIFIC_AUX_CONTEXT_DEVICE());
    }

    void DEVICE::destroySpecificContext()
    {
        auto *context = getSpecificContextDevice();
        if (context)
        {
            delete context;
            setSpecificContextDevice(nullptr);
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
            instanceDevice->getSpecificContextDevice()->release(wasDeviceLost);
            delete instanceDevice;
        }
        instanceDevice = nullptr;
    }

    void DEVICE::setDepthTest(const bool enable)
    {
        // Toggle the flag read by SHADER::render() / renderDynamic() to choose between
        // the depth-enabled (less + write) and depth-disabled (always + no-write) states.
        if (auto *context = getSpecificContextDevice())
            context->depthTestEnabled = enable;
    }

    void DEVICE::clearDepth()
    {
        // Metal cannot clear the depth buffer mid-pass via a simple API call.
        // We end the current encoder and start a new one that loads the existing
        // colour attachment (preserving the 3D scene) but clears the depth attachment.
        // This is called between the 3D pass and the 2dw pass so that 3D perspective
        // depth values do not contaminate the 2dw orthographic depth comparisons.
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = getSpecificContextDevice();
        if (!ctx) return;
        if (ctx->currentEncoder && ctx->currentCommandBuffer && ctx->currentPassDescriptor)
        {
            [ctx->currentEncoder endEncoding];
            ctx->currentEncoder = nil;

            MTLRenderPassDescriptor* desc = [MTLRenderPassDescriptor renderPassDescriptor];
            // Preserve the colour buffer (3D scene already rendered into it).
            desc.colorAttachments[0].texture     = ctx->currentPassDescriptor.colorAttachments[0].texture;
            desc.colorAttachments[0].loadAction  = MTLLoadActionLoad;
            desc.colorAttachments[0].storeAction = MTLStoreActionStore;
            // Clear depth to 1.0 (far plane) so 2dw objects get a clean depth slate.
            desc.depthAttachment.texture     = ctx->depthTexture;
            desc.depthAttachment.loadAction  = MTLLoadActionClear;
            desc.depthAttachment.clearDepth  = 1.0;
            desc.depthAttachment.storeAction = MTLStoreActionDontCare;

            ctx->currentEncoder = [ctx->currentCommandBuffer
                renderCommandEncoderWithDescriptor:desc];
            if (ctx->currentEncoder)
                ctx->currentEncoder.label = @"MBM Encoder (2D)";
            ctx->currentPassDescriptor = desc;
        }
        else
        {
            // No encoder active yet — flag it for beginRender().
            ctx->pendingClearDepth = true;
        }
    }

    void DEVICE::clearDepthColored()
    {
        // The clear background color is already updated by the caller before invoking this.
        // beginRender() reads it through DEVICE accessors when building the MTLRenderPassDescriptor.
        if (auto *context = getSpecificContextDevice())
            context->pendingClearDepth = true;
    }

    const char* DEVICE::getBackendEngineName() const noexcept
    {
        return "Metal";
    }

    const char* DEVICE::getBackendEngineVersion() const noexcept
    {
        auto *context = getSpecificContextDevice();
        if (context && context->mtlDevice)
        {
            static std::string ver;
            ver = "\nMetal device: ";
            ver += [context->mtlDevice.name UTF8String];
            return ver.c_str();
        }
        return "\nMetal (device not initialised)";
    }

    void DEVICE::setProjectionMode(const bool is3D, const float width, const float height)
    {
        // In the Metal backend, backBufferWidth/Height and drawableSize are managed
        // by initGraphics() and resetDeviceWithNewDimensions() only.
        // Here we just rebuild the camera matrices for the given dimensions.
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
        setPixelPerfectRenderingActive(true);
        if (auto *context = getSpecificContextDevice())
            context->useNearestSampler = true;
    }

    void DEVICE::enableFilteringAfterPixelPerfect() noexcept
    {
        setPixelPerfectRenderingActive(false);
        if (auto *context = getSpecificContextDevice())
            context->useNearestSampler = false;
    }

} // namespace mbm

#endif // USE_METAL
