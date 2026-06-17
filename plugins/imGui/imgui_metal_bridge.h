/*-----------------------------------------------------------------------------------------------------------------------|
| Plain C++ bridge for the ImGui Metal renderer backend.                                                                 |
|                                                                                                                        |
| imgui-lua.cpp is compiled as a plain .cpp file and cannot use Objective-C types such as id<MTLDevice>.                 |
| imgui-metal-bridge.mm implements these functions and is compiled as Objective-C++ — it calls the real                  |
| ImGui_ImplMetal_* API while hiding all Metal / ObjC types from the rest of the codebase.                               |
|                                                                                                                        |
| Usage (from imgui-lua.cpp, guarded by #if defined USE_METAL):                                                          |
|   ImGui_Metal_Init(_renderDevice);          // once: pass the MTLDevice* (as void*)                                    |
|   ImGui_Metal_NewFrame();                   // each frame before ImGui::NewFrame()                                     |
|   ImGui_Metal_RenderDrawData(draw_data);    // after ImGui::Render()                                                   |
|   ImGui_Metal_Shutdown();                   // on destroy                                                              |
|-----------------------------------------------------------------------------------------------------------------------*/

#pragma once

struct ImDrawData;

// _mtlDevice : id<MTLDevice>  cast to void*
void ImGui_Metal_Init(void* mtlDevice);

// Calls ImGui_ImplMetal_NewFrame() with the current MTLRenderPassDescriptor
// fetched through the engine's opaque Metal frame bridge.
void ImGui_Metal_NewFrame();

// Calls ImGui_ImplMetal_RenderDrawData() with the current MTLCommandBuffer
// and MTLRenderCommandEncoder from the engine's opaque Metal frame bridge.
void ImGui_Metal_RenderDrawData(ImDrawData* drawData);

void ImGui_Metal_Shutdown();
