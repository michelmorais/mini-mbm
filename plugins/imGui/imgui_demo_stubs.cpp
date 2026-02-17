// Stub implementations for ImGui demo functions when imgui_demo.cpp is excluded (release builds).
// These satisfy the linker when the full demo is not compiled in.

#ifndef DEBUG
#ifndef  _DEBUG
#include "imgui.h"

void ImGui::ShowDemoWindow(bool*)
{
}

void ImGui::ShowUserGuide()
{
}

bool ImGui::ShowStyleSelector(const char*)
{
    return false;
}
#endif
#endif