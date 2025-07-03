/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT);                                                                                                     |
| Copyright (C); 2022      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software");, to deal in the Software without restriction, including without limitation       |
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

// This is an extention of imgui used by imgui-lua.cpp

#include "imgui.h"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

namespace ImGui
{
    void IsScrollVisible(bool* x, bool * y)
    {
        ImGuiWindow* window = GImGui->CurrentWindow;
        *x = window->ScrollbarX;
        *y = window->ScrollbarY;
    }

    void ImageQuad(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0 , const ImVec2& uv1, const ImVec2& uv2,const ImVec2& uv3 , const ImVec4& tint_col, const ImVec4& border_col)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return;

        ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
        if (border_col.w > 0.0f)
            bb.Max += ImVec2(2, 2);
        ItemSize(bb);
        if (!ItemAdd(bb, 0))
            return;
        const ImVec2 a(bb.Min + ImVec2(1, 1));
        const ImVec2 c(bb.Max - ImVec2(1, 1));
        const ImVec2 b(c.x, a.y);
        const ImVec2 d(a.x, c.y);

        if (border_col.w > 0.0f)
        {
            window->DrawList->AddRect(bb.Min, bb.Max, GetColorU32(border_col), 0.0f);
            window->DrawList->AddImageQuad(user_texture_id, a, b, c, d, uv0, uv1, uv2, uv3, GetColorU32(tint_col));
        }
        else
        {
            window->DrawList->AddImageQuad(user_texture_id, a, b, c, d, uv0, uv1, uv2, uv3, GetColorU32(tint_col));
        }
    }

    float GetMainMenuBarHeight()
    {
        float height = 0;
        if (ImGuiWindow* window = FindWindowByName("##MainMenuBar"))
            height = window->Size.y;
        return height;
    }
}