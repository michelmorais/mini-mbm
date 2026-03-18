// Objective-C++ implementation of the plain-C++ ImGui Metal bridge.
// This file is compiled with -fobjc-arc (see plugins/imGui/CMakeLists.txt).
//
// It is the only file in the ImGui plugin that is allowed to include Objective-C
// Metal headers. Everything else in the plugin sees only the plain-C++ bridge
// declared in imgui_metal_bridge.h.

#import "imgui.h"
#import "backends/imgui_impl_metal.h"
#import "imgui_metal_bridge.h"

// Access SPECIFIC_AUX_CONTEXT_DEVICE via the engine device singleton.
// The header must be imported AFTER imgui.h to satisfy the Metal framework order.
#import <core_mbm/device.h>
#import <core_mbm/specific-metal.h>

void ImGui_Metal_Init(void* mtlDevice)
{
    id<MTLDevice> device = (__bridge id<MTLDevice>)mtlDevice;
    ImGui_ImplMetal_Init(device);
}

void ImGui_Metal_NewFrame()
{
    mbm::SPECIFIC_AUX_CONTEXT_DEVICE* ctx =
        mbm::DEVICE::getInstance()->specificContextDevice;

    // currentPassDescriptor is valid between CORE_MANAGER::beginRender and swapBuffers.
    ImGui_ImplMetal_NewFrame(ctx->currentPassDescriptor);
}

void ImGui_Metal_RenderDrawData(ImDrawData* drawData)
{
    mbm::SPECIFIC_AUX_CONTEXT_DEVICE* ctx =
        mbm::DEVICE::getInstance()->specificContextDevice;

    ImGui_ImplMetal_RenderDrawData(drawData,
                                   ctx->currentCommandBuffer,
                                   ctx->currentEncoder);
}

void ImGui_Metal_Shutdown()
{
    ImGui_ImplMetal_Shutdown();
}
