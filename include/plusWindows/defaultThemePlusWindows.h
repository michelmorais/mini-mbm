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

#ifndef DEFAULT_THEME_WINPLUS_H
#define DEFAULT_THEME_WINPLUS_H

#ifndef NOMINMAX
  #define NOMINMAX
#endif

#include "plusWindows.h"
#include <map>

class THEME_WINPLUS_COLORS
{
  public:
    static COLORREF color_release_from;
    static COLORREF color_release_to;
    static COLORREF color_hover_from;
    static COLORREF color_hover_to;
    static COLORREF color_press_from;
    static COLORREF color_press_to;
    static COLORREF color_text_release;
    static COLORREF color_text_hover;
};

class THEME_WINPLUS_PEN_COLORS
{
  public:
    static HPEN hPen_color_release_from;
    static HPEN hPen_color_release_to;
    static HPEN hPen_color_hover_from;
    static HPEN hPen_color_hover_to;
    static HPEN hPen_color_press_from;
    static HPEN hPen_color_press_to;
    static HPEN hPen_color_text_release;
    static HPEN hPen_color_text_hover;
    static HPEN hPen_noPen;
    static bool enablePenBorder;
    API_IMPL static void init();
    API_IMPL static void release();
};

class THEME_WINPLUS_BRUSH_RELEASE
{
  public:
    HBRUSH release_brush;
    API_IMPL THEME_WINPLUS_BRUSH_RELEASE();
    API_IMPL void initRelease(mbm::DRAW *draw, const bool horizontal = true, const bool power2 = true);
    API_IMPL virtual ~THEME_WINPLUS_BRUSH_RELEASE();
};

class THEME_WINPLUS_BRUSH_HOVER
{
  public:
    HBRUSH hover_brush;
    API_IMPL THEME_WINPLUS_BRUSH_HOVER();
    API_IMPL void initHover(mbm::DRAW *draw, const bool horizontal = true, const bool power2 = true);
    API_IMPL virtual ~THEME_WINPLUS_BRUSH_HOVER();
};

class THEME_WINPLUS_BRUSH_PRESS
{
  public:
    HBRUSH press_brush;
    API_IMPL THEME_WINPLUS_BRUSH_PRESS();
    API_IMPL void initPress(mbm::DRAW *draw, const bool horizontal = true, const bool power2 = true);
    API_IMPL virtual ~THEME_WINPLUS_BRUSH_PRESS();
};

class THEME_WINPLUS_DRAW_BASIC_OPERATIONS : public mbm::DRAW, public THEME_WINPLUS_COLORS
{
  public:
    API_IMPL THEME_WINPLUS_DRAW_BASIC_OPERATIONS(mbm::COMPONENT_INFO &component);
    API_IMPL virtual ~THEME_WINPLUS_DRAW_BASIC_OPERATIONS();
    API_IMPL virtual void drawBrush(mbm::COMPONENT_INFO &component, HBRUSH hbrush);
    API_IMPL virtual void DrawMyText(mbm::COMPONENT_INFO &component, int x, int y);
    virtual bool eraseBackGround(mbm::COMPONENT_INFO* )
    {
        return false;
    }
};

class THEME_WINPLUS_DRAW_WINDOW : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS, public THEME_WINPLUS_BRUSH_RELEASE
{
  public:
    THEME_WINPLUS_DRAW_WINDOW(mbm::COMPONENT_INFO &component);
    API_IMPL virtual void setCtlColor(HDC hdcStatic);
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
};

class THEME_WINPLUS_DRAW_WINDOWNC : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS
{
  public:
    THEME_WINPLUS_BRUSH_HOVER closeButtonHover;
    THEME_WINPLUS_BRUSH_HOVER maximizeButtonHover;
    THEME_WINPLUS_BRUSH_HOVER minimizeButtonHover;

    THEME_WINPLUS_BRUSH_RELEASE closeButtonRelease;
    THEME_WINPLUS_BRUSH_RELEASE maximizeButtonRelease;
    THEME_WINPLUS_BRUSH_RELEASE minimizeButtonRelease;

    THEME_WINPLUS_BRUSH_RELEASE borderLeft;
    THEME_WINPLUS_BRUSH_RELEASE borderRight;
    THEME_WINPLUS_BRUSH_RELEASE borderTop;
    THEME_WINPLUS_BRUSH_RELEASE borderBottom;
    API_IMPL THEME_WINPLUS_DRAW_WINDOWNC(mbm::COMPONENT_INFO &component);
    API_IMPL virtual ~THEME_WINPLUS_DRAW_WINDOWNC();
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);

    API_IMPL void renderBorder(RECT &rect, THEME_WINPLUS_BRUSH_RELEASE *border);

    API_IMPL void renderBorder(RECT &rect, THEME_WINPLUS_BRUSH_HOVER *border);
    API_IMPL void moveRect2Origin(mbm::__NC_BORDERS *nc, RECT *rc);
    API_IMPL void restorePositionButtons(mbm::__NC_BORDERS *nc, RECT *rc, int dist);
    API_IMPL void drawTextButtons(RECT *rc, bool hover, const char *text);
    API_IMPL void drawMinMaxButtons(RECT *rc, bool hover);    
};

class THEME_WINPLUS_DRAW_TRACK : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS, public THEME_WINPLUS_BRUSH_RELEASE
{
  public:
    HBRUSH hBrushMainBar;
    RECT   rectMainBar;
    int    ray, center, realInterval;
    API_IMPL THEME_WINPLUS_DRAW_TRACK(mbm::COMPONENT_INFO &component);
    API_IMPL virtual ~THEME_WINPLUS_DRAW_TRACK();

    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
    API_IMPL inline RECT getMainBarRect(mbm::COMPONENT_INFO &component);
    API_IMPL virtual void drawBrush(const RECT & rect, HBRUSH hbrush);
    API_IMPL virtual void DrawMyText(mbm::COMPONENT_INFO &component, const char *text, const bool isVertical);
};

class THEME_WINPLUS_DRAW_BUTTON : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS,
                                  public THEME_WINPLUS_BRUSH_RELEASE,
                                  public THEME_WINPLUS_BRUSH_HOVER,
                                  public THEME_WINPLUS_BRUSH_PRESS
{
  public:
      API_IMPL THEME_WINPLUS_DRAW_BUTTON(mbm::COMPONENT_INFO& component);
      API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
};

class THEME_WINPLUS_DRAW_LABEL : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS,
                                 public THEME_WINPLUS_BRUSH_RELEASE,
                                 public THEME_WINPLUS_BRUSH_HOVER
{
  public:
    API_IMPL THEME_WINPLUS_DRAW_LABEL(mbm::COMPONENT_INFO &component);
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
    API_IMPL virtual void DrawMyText(mbm::COMPONENT_INFO &component, int x, int y);
};

class THEME_WINPLUS_DRAW_SCROLL : // doesnt work
                                  public THEME_WINPLUS_DRAW_BASIC_OPERATIONS,
                                  public THEME_WINPLUS_BRUSH_RELEASE,
                                  public THEME_WINPLUS_BRUSH_HOVER
{
  public:
    API_IMPL THEME_WINPLUS_DRAW_SCROLL(mbm::COMPONENT_INFO &component);
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
};

class THEME_WINPLUS_DRAW_IMAGE : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS,
                                 public THEME_WINPLUS_BRUSH_RELEASE,
                                 public THEME_WINPLUS_BRUSH_HOVER
{
  public:
    API_IMPL THEME_WINPLUS_DRAW_IMAGE(mbm::COMPONENT_INFO &component);
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
    API_IMPL virtual ~THEME_WINPLUS_DRAW_IMAGE();
};

class THEME_WINPLUS_DRAW_TOOL_TIP : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS, public THEME_WINPLUS_BRUSH_HOVER
{
  public:
    API_IMPL THEME_WINPLUS_DRAW_TOOL_TIP(mbm::COMPONENT_INFO &component);
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
    API_IMPL virtual void DrawMyText(mbm::COMPONENT_INFO &component, int x, int y);
};

class THEME_WINPLUS_DRAW_MENU : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS,
                                public THEME_WINPLUS_BRUSH_RELEASE,
                                public THEME_WINPLUS_BRUSH_HOVER
{
  public:
    API_IMPL THEME_WINPLUS_DRAW_MENU(mbm::COMPONENT_INFO &component);
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
};

class THEME_WINPLUS_DRAW_SUB_MENU : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS,
                                    public THEME_WINPLUS_BRUSH_RELEASE,
                                    public THEME_WINPLUS_BRUSH_HOVER
{
  public:
    API_IMPL THEME_WINPLUS_DRAW_SUB_MENU(mbm::COMPONENT_INFO &component);
    API_IMPL virtual ~THEME_WINPLUS_DRAW_SUB_MENU();
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
    API_IMPL virtual void DrawMyTextSubMenu(mbm::COMPONENT_INFO &component);
};

class THEME_WINPLUS_DRAW_SPIN : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS,
                                public THEME_WINPLUS_BRUSH_RELEASE,
                                public THEME_WINPLUS_BRUSH_HOVER,
                                public THEME_WINPLUS_BRUSH_PRESS
{
  public:
    API_IMPL THEME_WINPLUS_DRAW_SPIN(mbm::COMPONENT_INFO &component);
    API_IMPL virtual ~THEME_WINPLUS_DRAW_SPIN();
    API_IMPL bool hasHover();
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
    HBRUSH hBrushSolidRelease;
};

class THEME_WINPLUS_DRAW_TEXT : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS,
                                public THEME_WINPLUS_BRUSH_RELEASE,
                                public THEME_WINPLUS_BRUSH_HOVER
{
  public:
    API_IMPL THEME_WINPLUS_DRAW_TEXT(mbm::COMPONENT_INFO &component);
    API_IMPL virtual ~THEME_WINPLUS_DRAW_TEXT();
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);

    API_IMPL void drawSelecteLine(mbm::COMPONENT_INFO &component, const DWORD iStart, const DWORD iEnd, const DWORD indexStartLine,
                         const DWORD indexEndLine, HBRUSH brushRect);
};

class THEME_WINPLUS_DRAW_RADIO : public THEME_WINPLUS_DRAW_BUTTON
{
  public:
    HBRUSH checked_brush;

    API_IMPL THEME_WINPLUS_DRAW_RADIO(mbm::COMPONENT_INFO &component);
    API_IMPL virtual ~THEME_WINPLUS_DRAW_RADIO();
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
    int part;
};

class THEME_WINPLUS_DRAW_CHECK : public THEME_WINPLUS_DRAW_BUTTON
{
  public:
    API_IMPL THEME_WINPLUS_DRAW_CHECK(mbm::COMPONENT_INFO &component);
    API_IMPL virtual ~THEME_WINPLUS_DRAW_CHECK();
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
    HBRUSH checked_brush;
};

class THEME_WINPLUS_DRAW_LIST : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS,
                                public THEME_WINPLUS_BRUSH_RELEASE,
                                public THEME_WINPLUS_BRUSH_HOVER
{
  public:
    API_IMPL THEME_WINPLUS_DRAW_LIST(mbm::COMPONENT_INFO &component);
    API_IMPL virtual HBRUSH getBackgroundBrush();
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
};

class THEME_WINPLUS_DRAW_PROGRESS_BAR : public mbm::DRAW,
                                        public THEME_WINPLUS_BRUSH_RELEASE,
                                        public THEME_WINPLUS_BRUSH_HOVER
{
  public:
    API_IMPL THEME_WINPLUS_DRAW_PROGRESS_BAR(mbm::COMPONENT_INFO &component);
    API_IMPL virtual ~THEME_WINPLUS_DRAW_PROGRESS_BAR();
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
    API_IMPL virtual void drawBrush(mbm::COMPONENT_INFO &component, HBRUSH hbrush);
    virtual bool eraseBackGround(mbm::COMPONENT_INFO* )
    {
        return false;
    }
};

class THEME_WINPLUS_COMBO : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS
{
  public:
    class DRAW_COMBO : public THEME_WINPLUS_BRUSH_RELEASE,
                       public THEME_WINPLUS_BRUSH_HOVER,
                       public THEME_WINPLUS_BRUSH_PRESS
    {
      public:
        DRAW_COMBO(mbm::DRAW *draw);
    };
    API_IMPL THEME_WINPLUS_COMBO(mbm::COMPONENT_INFO &component);
    API_IMPL virtual ~THEME_WINPLUS_COMBO();
    API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
    API_IMPL void drawText(mbm::COMPONENT_INFO &component, const char *text);
    HBRUSH      hBrushSolidRelease;
    DRAW_COMBO *inner;
    DRAW_COMBO *out;
};

class THEME_WINPLUS_DRAW_STATUS_BAR : public THEME_WINPLUS_DRAW_BASIC_OPERATIONS,
                                      public THEME_WINPLUS_BRUSH_RELEASE,
                                      public THEME_WINPLUS_BRUSH_HOVER

{
  public:
      API_IMPL THEME_WINPLUS_DRAW_STATUS_BAR(mbm::COMPONENT_INFO &component);
      API_IMPL virtual bool render(mbm::COMPONENT_INFO &component);
};

class THEME_WINPLUS_CUSTOM_RENDER : public mbm::DRAW
{
  public:
    HFONT font;
    std::map<int, DRAW *> lsDrawer;
    API_IMPL THEME_WINPLUS_CUSTOM_RENDER();
    API_IMPL virtual ~THEME_WINPLUS_CUSTOM_RENDER();
    API_IMPL void release();
    API_IMPL virtual int measureItem(mbm::COM_BETWEEN_WINP *ptr, MEASUREITEMSTRUCT *lpi);
    API_IMPL void init();
    API_IMPL bool render(mbm::COMPONENT_INFO &component);
    virtual bool eraseBackGround(mbm::COMPONENT_INFO* )
    {
        return false;
    }
    API_IMPL int static getTotalThemes();
    API_IMPL void static setTheme(int which, const bool enableBorder = true);
};

#endif