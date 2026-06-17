// Objective-C++ implementation of the plain-C++ ImGui Metal bridge.
// This file is compiled with -fobjc-arc (see plugins/imGui/CMakeLists.txt).
//
// It is the only file in the ImGui plugin that is allowed to include Objective-C
// Metal headers. Everything else in the plugin sees only the plain-C++ bridge
// declared in imgui_metal_bridge.h.

#import "imgui.h"
#import <Metal/Metal.h>
#import "backends/imgui_impl_metal.h"
#import "imgui_metal_bridge.h"

// Access Metal frame objects through narrow engine bridge functions.
#import <core_mbm/specific-metal.h>

void ImGui_Metal_Init(void* mtlDevice)
{
    id<MTLDevice> device = (__bridge id<MTLDevice>)mtlDevice;
    ImGui_ImplMetal_Init(device);
}

void ImGui_Metal_NewFrame()
{
    MTLRenderPassDescriptor* passDescriptor =
        (__bridge MTLRenderPassDescriptor*)mbm_metal_get_current_pass_descriptor();
    if (!passDescriptor)
        return;

    ImGui_ImplMetal_NewFrame(passDescriptor);

    // On Retina/HiDPI displays the Metal drawable is larger than the logical window size.
    // Tell ImGui about the pixel-to-point ratio so it renders at the correct physical scale.
    // DisplaySize is kept in logical points (what mouse / touch coordinates use).
    // DisplayFramebufferScale is the multiplier from points → physical pixels.
    id<MTLTexture> colorTex = passDescriptor.colorAttachments[0].texture;
    if (colorTex)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f)
        {
            float scaleX = static_cast<float>(colorTex.width)  / io.DisplaySize.x;
            float scaleY = static_cast<float>(colorTex.height) / io.DisplaySize.y;
            io.DisplayFramebufferScale = ImVec2(scaleX, scaleY);
        }
    }
}

void ImGui_Metal_RenderDrawData(ImDrawData* drawData)
{
    id<MTLCommandBuffer> commandBuffer =
        (__bridge id<MTLCommandBuffer>)mbm_metal_get_current_command_buffer();
    id<MTLRenderCommandEncoder> encoder =
        (__bridge id<MTLRenderCommandEncoder>)mbm_metal_get_current_encoder();
    if (!commandBuffer || !encoder)
        return;

    ImGui_ImplMetal_RenderDrawData(drawData,
                                   commandBuffer,
                                   encoder);
}

void ImGui_Metal_Shutdown()
{
    ImGui_ImplMetal_Shutdown();
}
