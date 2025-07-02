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

#include "defaultThemePlusWindows.h"

COLORREF THEME_WINPLUS_COLORS::color_release_from = RGB(41, 57, 85);
COLORREF THEME_WINPLUS_COLORS::color_release_to   = RGB(41, 57, 85);
COLORREF THEME_WINPLUS_COLORS::color_hover_from   = RGB(61, 85, 126);
COLORREF THEME_WINPLUS_COLORS::color_hover_to     = RGB(61, 85, 126);
COLORREF THEME_WINPLUS_COLORS::color_press_from   = RGB(32, 45, 66);
COLORREF THEME_WINPLUS_COLORS::color_press_to     = RGB(32, 45, 66);
COLORREF THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
COLORREF THEME_WINPLUS_COLORS::color_text_hover   = RGB(0, 0, 0);
    void THEME_WINPLUS_PEN_COLORS::init()
    {
        THEME_WINPLUS_PEN_COLORS::hPen_color_release_from =
            CreatePen(PS_SOLID, 1, THEME_WINPLUS_COLORS::color_release_from);
        THEME_WINPLUS_PEN_COLORS::hPen_color_release_to = CreatePen(PS_SOLID, 1, THEME_WINPLUS_COLORS::color_release_to);
        THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from = CreatePen(PS_SOLID, 1, THEME_WINPLUS_COLORS::color_hover_from);
        THEME_WINPLUS_PEN_COLORS::hPen_color_hover_to   = CreatePen(PS_SOLID, 1, THEME_WINPLUS_COLORS::color_hover_to);
        THEME_WINPLUS_PEN_COLORS::hPen_color_press_from = CreatePen(PS_SOLID, 1, THEME_WINPLUS_COLORS::color_press_from);
        THEME_WINPLUS_PEN_COLORS::hPen_color_press_to   = CreatePen(PS_SOLID, 1, THEME_WINPLUS_COLORS::color_press_to);
        THEME_WINPLUS_PEN_COLORS::hPen_color_text_release =
            CreatePen(PS_SOLID, 1, THEME_WINPLUS_COLORS::color_text_release);
        THEME_WINPLUS_PEN_COLORS::hPen_color_text_hover = CreatePen(PS_SOLID, 1, THEME_WINPLUS_COLORS::color_text_hover);
        THEME_WINPLUS_PEN_COLORS::hPen_noPen            = CreatePen(PS_NULL, 0, 0);
    }
    void THEME_WINPLUS_PEN_COLORS::release()
    {
        if (THEME_WINPLUS_PEN_COLORS::hPen_color_release_from)
            DeleteObject(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
        if (THEME_WINPLUS_PEN_COLORS::hPen_color_release_to)
            DeleteObject(THEME_WINPLUS_PEN_COLORS::hPen_color_release_to);
        if (THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from)
            DeleteObject(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
        if (THEME_WINPLUS_PEN_COLORS::hPen_color_hover_to)
            DeleteObject(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_to);
        if (THEME_WINPLUS_PEN_COLORS::hPen_color_press_from)
            DeleteObject(THEME_WINPLUS_PEN_COLORS::hPen_color_press_from);
        if (THEME_WINPLUS_PEN_COLORS::hPen_color_press_to)
            DeleteObject(THEME_WINPLUS_PEN_COLORS::hPen_color_press_to);
        if (THEME_WINPLUS_PEN_COLORS::hPen_color_text_release)
            DeleteObject(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
        if (THEME_WINPLUS_PEN_COLORS::hPen_color_text_hover)
            DeleteObject(THEME_WINPLUS_PEN_COLORS::hPen_color_text_hover);
        THEME_WINPLUS_PEN_COLORS::hPen_color_release_from = nullptr;
        THEME_WINPLUS_PEN_COLORS::hPen_color_release_to   = nullptr;
        THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from   = nullptr;
        THEME_WINPLUS_PEN_COLORS::hPen_color_hover_to     = nullptr;
        THEME_WINPLUS_PEN_COLORS::hPen_color_press_from   = nullptr;
        THEME_WINPLUS_PEN_COLORS::hPen_color_press_to     = nullptr;
        THEME_WINPLUS_PEN_COLORS::hPen_color_text_release = nullptr;
        THEME_WINPLUS_PEN_COLORS::hPen_color_text_hover   = nullptr;
    }

HPEN THEME_WINPLUS_PEN_COLORS::hPen_color_release_from = nullptr;
HPEN THEME_WINPLUS_PEN_COLORS::hPen_color_release_to   = nullptr;
HPEN THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from   = nullptr;
HPEN THEME_WINPLUS_PEN_COLORS::hPen_color_hover_to     = nullptr;
HPEN THEME_WINPLUS_PEN_COLORS::hPen_color_press_from   = nullptr;
HPEN THEME_WINPLUS_PEN_COLORS::hPen_color_press_to     = nullptr;
HPEN THEME_WINPLUS_PEN_COLORS::hPen_color_text_release = nullptr;
HPEN THEME_WINPLUS_PEN_COLORS::hPen_color_text_hover   = nullptr;
HPEN THEME_WINPLUS_PEN_COLORS::hPen_noPen              = nullptr;
bool THEME_WINPLUS_PEN_COLORS::enablePenBorder         = true;

    THEME_WINPLUS_BRUSH_RELEASE::THEME_WINPLUS_BRUSH_RELEASE()
    {
        this->release_brush = nullptr;
    }
    void THEME_WINPLUS_BRUSH_RELEASE::initRelease(mbm::DRAW *draw, const bool horizontal , const bool power2 )
    {
        if (release_brush)
            DeleteObject(release_brush);
        this->release_brush            = nullptr;
        mbm::COMPONENT_INFO *component = draw->getCurrentComponent();
        this->release_brush =
            draw->createGradientBrush(THEME_WINPLUS_COLORS::color_release_from, THEME_WINPLUS_COLORS::color_release_to,
                                      component->rect, horizontal, power2);
    }
    THEME_WINPLUS_BRUSH_RELEASE::~THEME_WINPLUS_BRUSH_RELEASE()
    {
        if (release_brush)
            DeleteObject(release_brush);
        release_brush = nullptr;
    }

    THEME_WINPLUS_BRUSH_HOVER::THEME_WINPLUS_BRUSH_HOVER()
    {
        this->hover_brush = nullptr;
    }
    void THEME_WINPLUS_BRUSH_HOVER::initHover(mbm::DRAW *draw, const bool horizontal , const bool power2 )
    {
        if (hover_brush)
            DeleteObject(hover_brush);
        hover_brush                    = nullptr;
        mbm::COMPONENT_INFO *component = draw->getCurrentComponent();
        this->hover_brush =
            draw->createGradientBrush(THEME_WINPLUS_COLORS::color_hover_from, THEME_WINPLUS_COLORS::color_hover_to,
                                      component->rect, horizontal, power2);
    }
    THEME_WINPLUS_BRUSH_HOVER::~THEME_WINPLUS_BRUSH_HOVER()
    {
        if (hover_brush)
            DeleteObject(hover_brush);
        hover_brush = nullptr;
    }

    THEME_WINPLUS_BRUSH_PRESS::THEME_WINPLUS_BRUSH_PRESS()
    {
        this->press_brush = nullptr;
    }
    void THEME_WINPLUS_BRUSH_PRESS::initPress(mbm::DRAW *draw, const bool horizontal , const bool power2)
    {
        if (press_brush)
            DeleteObject(press_brush);
        press_brush                    = nullptr;
        mbm::COMPONENT_INFO *component = draw->getCurrentComponent();
        this->press_brush =
            draw->createGradientBrush(THEME_WINPLUS_COLORS::color_press_from, THEME_WINPLUS_COLORS::color_press_to,
                                      component->rect, horizontal, power2);
    }
    THEME_WINPLUS_BRUSH_PRESS::~THEME_WINPLUS_BRUSH_PRESS()
    {
        if (press_brush)
            DeleteObject(press_brush);
        press_brush = nullptr;
    }

    THEME_WINPLUS_DRAW_BASIC_OPERATIONS::THEME_WINPLUS_DRAW_BASIC_OPERATIONS(mbm::COMPONENT_INFO &component) : mbm::DRAW(&component)
    {
    }
    THEME_WINPLUS_DRAW_BASIC_OPERATIONS::~THEME_WINPLUS_DRAW_BASIC_OPERATIONS()
    {
    }
    void THEME_WINPLUS_DRAW_BASIC_OPERATIONS::drawBrush(mbm::COMPONENT_INFO &component, HBRUSH hbrush)
    {
        HGDIOBJ original = this->setBrush(hbrush);
        this->drawRectangle(component.rect);
        this->setBrush(original);
    }

    void THEME_WINPLUS_DRAW_BASIC_OPERATIONS::DrawMyText(mbm::COMPONENT_INFO &component, int x, int y)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        const int       len      = component.ptrWindow->getTextLength(component.id);
        if (len)
        {
            std::vector<char> text;
            text.resize(len);
            if (component.ptrWindow->getText(component.id, &text[0], (WORD)len))
            {
                if (userDrawer)
                {
                    if (component.isHover && userDrawer->enableHover)
                        this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_hover, &text[0]);
                    else
                        this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_release, &text[0]);
                }
                else
                {
                    if (component.isHover)
                        this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_hover, &text[0]);
                    else
                        this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_release, &text[0]);
                }
            }
        }
    }

    THEME_WINPLUS_DRAW_WINDOW::THEME_WINPLUS_DRAW_WINDOW(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        THEME_WINPLUS_BRUSH_RELEASE::initRelease(this);
        PostMessageA(component.hwnd, WM_SIZE, SIZE_RESTORED,
                     MAKELPARAM(component.rect.right - component.rect.left, component.rect.bottom - component.rect.top));
    }
    void THEME_WINPLUS_DRAW_WINDOW::setCtlColor(HDC hdcStatic)
    {
        SetBkColor(hdcStatic, THEME_WINPLUS_COLORS::color_release_from);
        SetTextColor(hdcStatic, THEME_WINPLUS_COLORS::color_text_release);
    }
    bool THEME_WINPLUS_DRAW_WINDOW::render(mbm::COMPONENT_INFO &component)
    {
        HPEN    oldPen   = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
        HGDIOBJ original = this->setBrush(this->release_brush);
        this->drawRectangle(component.rect);
        this->setBrush(original);
        this->setPen(oldPen);
        return true;
    }

    THEME_WINPLUS_DRAW_WINDOWNC::THEME_WINPLUS_DRAW_WINDOWNC(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        mbm::__NC_BORDERS *nc = static_cast<mbm::__NC_BORDERS *>(component.userDrawer->that);
        component.rect        = nc->rectLeft;
        borderLeft.initRelease(this);

        component.rect = nc->rectRight;
        borderRight.initRelease(this);

        component.rect = nc->rectTop;
        borderTop.initRelease(this);

        component.rect             = nc->rectBottom;
        borderBottom.release_brush = CreateSolidBrush(THEME_WINPLUS_COLORS::color_release_from);

        TITLEBARINFOEX p;
        memset(&p, 0, sizeof(p));
        p.cbSize = sizeof(p);
        SendMessageA(component.hwnd, WM_GETTITLEBARINFOEX, 0, (LPARAM)&p);
        if (nc->buttons->hasCloseButton)
        {
            component.rect = p.rgrect[5];
            component.rect.right -= nc->sides;
            this->moveRect2Origin(nc, &component.rect);
            if(nc->buttons->rectClose.top       == 0 && //we do not recalc is already is there
                nc->buttons->rectClose.left     == 0 && 
                nc->buttons->rectClose.right    == 0 && 
                nc->buttons->rectClose.bottom   == 0 )
            {
                nc->buttons->rectClose      = component.rect;
            }
            else
            {
                component.rect = nc->buttons->rectClose;
            }
            nc->buttons->distCloseRight = (nc->rcWind.right - nc->rcWind.left) - nc->buttons->rectClose.right;
            closeButtonHover.initHover(this, true, true);
            closeButtonRelease.initRelease(this, true, true);
        }
        if (nc->buttons->hasMaximizeButton)
        {
            component.rect = p.rgrect[3];
            this->moveRect2Origin(nc, &component.rect);
            if(nc->buttons->rectMaximize.top       == 0 && //we do not recalc is already is there
                nc->buttons->rectMaximize.left     == 0 && 
                nc->buttons->rectMaximize.right    == 0 && 
                nc->buttons->rectMaximize.bottom   == 0 )
            {
                nc->buttons->rectMaximize = component.rect;
            }
            else
            {
                component.rect = nc->buttons->rectMaximize;
            }
            
            nc->buttons->distMaxRight = (nc->rcWind.right - nc->rcWind.left) - nc->buttons->rectMaximize.right;
            maximizeButtonHover.initHover(this, true, true);
            maximizeButtonRelease.initRelease(this, true, true);
        }
        if (nc->buttons->hasMinimizeButton)
        {
            if(nc->buttons->rectMinimize.top       == 0 && //we do not recalc is already is there
                nc->buttons->rectMinimize.left     == 0 && 
                nc->buttons->rectMinimize.right    == 0 && 
                nc->buttons->rectMinimize.bottom   == 0 )
            {
                if (nc->buttons->hasMaximizeButton)
                    component.rect = p.rgrect[2];
                else
                    component.rect = p.rgrect[3];
                this->moveRect2Origin(nc, &component.rect);
                nc->buttons->rectMinimize = component.rect;
                nc->buttons->distMinRight = (nc->rcWind.right - nc->rcWind.left) - nc->buttons->rectMinimize.right;
            }
            else
            {
                component.rect = nc->buttons->rectMinimize;
            }
            minimizeButtonHover.initHover(this, true, true);
            minimizeButtonRelease.initRelease(this, true, true);
        }

        DWORD dwStyle                  = GetWindowLong(component.hwnd, GWL_STYLE);
        if (nc->buttons->hasMaximizeButton && (dwStyle & WS_MAXIMIZEBOX))
            dwStyle = (dwStyle & ~WS_MAXIMIZEBOX);
        if (nc->buttons->hasMinimizeButton && (dwStyle & WS_MINIMIZEBOX))
            dwStyle = (dwStyle & ~WS_MINIMIZEBOX);
        if (nc->buttons->hasCloseButton && (dwStyle & WS_SYSMENU))
            dwStyle = (dwStyle & ~WS_SYSMENU);
        // remove all buttons cause we draw it!
        SetWindowLong(component.hwnd, GWL_STYLE, dwStyle);
    }
    THEME_WINPLUS_DRAW_WINDOWNC::~THEME_WINPLUS_DRAW_WINDOWNC()
    {
    }
    bool THEME_WINPLUS_DRAW_WINDOWNC::render(mbm::COMPONENT_INFO &component)
    {
        if (component.userDrawer && component.userDrawer->render(component))
        {
            return true;
        }
        mbm::__NC_BORDERS *nc = static_cast<mbm::__NC_BORDERS *>(component.userDrawer->that);
		if(nc == nullptr)
			return false;
        moveRect2Origin(nc, &nc->rectLeft);
        moveRect2Origin(nc, &nc->rectRight);
        moveRect2Origin(nc, &nc->rectTop);
        moveRect2Origin(nc, &nc->rectBottom);

        renderBorder(nc->rectLeft, &this->borderLeft);
        renderBorder(nc->rectRight, &this->borderRight);

        HPEN    oldPen   = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
        HGDIOBJ original = this->setBrush(borderTop.release_brush);
        component.rect   = nc->rectTop;
        this->drawRectangle(nc->rectTop);
        this->DrawMyText(component, 10, 4);
        this->setBrush(original);
        this->setPen(oldPen);

        renderBorder(nc->rectBottom, &this->borderBottom);

        if (nc->buttons->hasCloseButton)
        {
            this->restorePositionButtons(nc, &nc->buttons->rectClose, nc->buttons->distCloseRight);
            if (nc->buttons->isHoverClose)
                renderBorder(nc->buttons->rectClose, &closeButtonHover);
            else
                renderBorder(nc->buttons->rectClose, &closeButtonRelease);
            this->drawTextButtons(&nc->buttons->rectClose, nc->buttons->isHoverClose, "X");
        }
        if (nc->buttons->hasMaximizeButton)
        {
            this->restorePositionButtons(nc, &nc->buttons->rectMaximize, nc->buttons->distMaxRight);
            if (nc->buttons->isHoverMax)
                renderBorder(nc->buttons->rectMaximize, &maximizeButtonHover);
            else
                renderBorder(nc->buttons->rectMaximize, &maximizeButtonRelease);
            this->drawMinMaxButtons(&nc->buttons->rectMaximize, nc->buttons->isHoverMax);
        }
        if (nc->buttons->hasMinimizeButton)
        {
            this->restorePositionButtons(nc, &nc->buttons->rectMinimize, nc->buttons->distMinRight);
            if (nc->buttons->isHoverMin)
                renderBorder(nc->buttons->rectMinimize, &minimizeButtonHover);
            else
                renderBorder(nc->buttons->rectMinimize, &minimizeButtonRelease);
            this->drawTextButtons(&nc->buttons->rectMinimize, nc->buttons->isHoverMin, "-");
        }
        return true;
    }

    void THEME_WINPLUS_DRAW_WINDOWNC::renderBorder(RECT &rect, THEME_WINPLUS_BRUSH_RELEASE *border)
    {
        HPEN    oldPen   = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
        HGDIOBJ original = this->setBrush(border->release_brush);
        this->drawRectangle(rect);
        this->setBrush(original);
        this->setPen(oldPen);
    }

    void THEME_WINPLUS_DRAW_WINDOWNC::renderBorder(RECT &rect, THEME_WINPLUS_BRUSH_HOVER *border)
    {
        HPEN    oldPen   = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
        HGDIOBJ original = this->setBrush(border->hover_brush);
        this->drawRectangle(rect);
        this->setBrush(original);
        this->setPen(oldPen);
    }
    void THEME_WINPLUS_DRAW_WINDOWNC::moveRect2Origin(mbm::__NC_BORDERS *nc, RECT *rc)
    {
        rc->left -= nc->rcWind.left;
        rc->right -= nc->rcWind.left;
        rc->top -= nc->rcWind.top;
        rc->bottom -= nc->rcWind.top;
    }
    void THEME_WINPLUS_DRAW_WINDOWNC::restorePositionButtons(mbm::__NC_BORDERS *nc, RECT *rc, int dist)
    {
        const int width  = rc->right - rc->left;
        const int mWidth = nc->rcWind.right - nc->rcWind.left;
        rc->left         = mWidth - width - dist;
        rc->right        = mWidth - dist;
    }
    void THEME_WINPLUS_DRAW_WINDOWNC::drawTextButtons(RECT *rc, bool hover, const char *text)
    {
        const SIZE sz     = this->getSizeText(text, this->getCurrentComponent()->hwnd);
        const int  width  = rc->right - rc->left;
        const int  height = rc->bottom - rc->top;
        const int  x      = ((width - sz.cx) / 2) + rc->left;
        const int  y      = ((height - sz.cy) / 2) + rc->top;
        if (hover)
            this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_hover, text);
        else
            this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_release, text);
    }
    void THEME_WINPLUS_DRAW_WINDOWNC::drawMinMaxButtons(RECT *rc, bool hover)
    {
        const int width  = rc->right - rc->left;
        const int margin = width / 10 * 3;
        POINT     p[4];
        p[0].x = rc->left + margin;
        p[0].y = rc->top + margin;
        p[1].x = rc->right - margin;
        p[1].y = rc->top + margin;
        p[2].x = rc->right - margin;
        p[2].y = rc->bottom - margin;
        p[3].x = rc->left + margin;
        p[3].y = rc->bottom - margin;

        if (hover)
            this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_hover);
        else
            this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
        this->drawLine(p[0], p[1]);
        this->drawLine(p[1], p[2]);
        this->drawLine(p[2], p[3]);
        this->drawLine(p[3], p[0]);
    }

    THEME_WINPLUS_DRAW_TRACK::THEME_WINPLUS_DRAW_TRACK(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        THEME_WINPLUS_BRUSH_RELEASE::initRelease(this);
        this->rectMainBar   = getMainBarRect(component);
        const int  width    = (this->rectMainBar.right - this->rectMainBar.left);
        const int  height   = (this->rectMainBar.bottom - this->rectMainBar.top);
        const bool vertical = width < height;
        hBrushMainBar = this->createGradientBrush(RGB(0, 255, 0), RGB(255, 0, 0), this->rectMainBar, vertical, true);
        RECT rectScrollBar;
        if (vertical)
        {
            rectScrollBar.left   = 0;
            rectScrollBar.right  = width;
            rectScrollBar.top    = 0;
            rectScrollBar.bottom = width;
            center               = ((component.rect.right - component.rect.left) / 2);
            realInterval         = height;
        }
        else
        {
            rectScrollBar.left   = 0;
            rectScrollBar.right  = height;
            rectScrollBar.top    = 0;
            rectScrollBar.bottom = height;
            center               = ((component.rect.bottom - component.rect.top) / 2);
            realInterval         = width;
        }
        ray = rectScrollBar.right / 2;
    }
    THEME_WINPLUS_DRAW_TRACK::~THEME_WINPLUS_DRAW_TRACK()
    {
        if (hBrushMainBar)
            DeleteObject(hBrushMainBar);
        hBrushMainBar = nullptr;
    }

    bool THEME_WINPLUS_DRAW_TRACK::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HPEN oldPen = nullptr;
            if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
            else
                oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
            drawBrush(component.rect, release_brush);
            TRACK_BAR_INFO *infoTrack = component.ptrWindow->getInfoTrack(component.id);
            if (infoTrack)
            {
                oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_noPen);
                drawBrush(rectMainBar, hBrushMainBar);
                this->setPen(oldPen);
                oldPen                 = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                HGDIOBJ oldHGDi        = this->setBrush(THEME_WINPLUS_BRUSH_RELEASE::release_brush);
                float   interval       = infoTrack->maxPosition - infoTrack->minPosition;
                float   tick           = (float)interval / (float)realInterval;
                float   handlePosition = ((float)infoTrack->position / tick);
                if (infoTrack->isVertical)
                {
                    if (component.isPressed)
                    {
                        POINT p = {0, 0};
                        GetCursorPos(&p);
                        MapWindowPoints(HWND_DESKTOP, component.hwnd, &p, 1);
                        handlePosition = (float)p.y;
                    }
                    if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                        this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                    this->drawCircle(center, (int)handlePosition, ray);
                }
                else
                {
                    if (component.isPressed)
                    {
                        POINT p = {0, 0};
                        GetCursorPos(&p);
                        MapWindowPoints(HWND_DESKTOP, component.hwnd, &p, 1);
                        handlePosition = (float)p.x;
                    }
                    if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                        this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                    this->drawCircle((int)handlePosition, center, ray);
                }
                handlePosition      = ((float)handlePosition * tick);
                infoTrack->position = handlePosition;
                if (infoTrack->position > infoTrack->maxPosition)
                    infoTrack->position = infoTrack->maxPosition;
                else if (infoTrack->position < infoTrack->minPosition)
                    infoTrack->position = infoTrack->minPosition;
                this->setBrush(oldHGDi);
                this->setPen(oldPen);
                char strPosition[255]       = "";
                infoTrack->positionInverted = interval - infoTrack->position;
                if (infoTrack->invertMinMaxText)
                    sprintf(strPosition, " %02.0f", infoTrack->positionInverted);
                else
                    sprintf(strPosition, " %02.0f", infoTrack->position);
                DrawMyText(component, strPosition, infoTrack->isVertical);
            }
        }
        return true;
    }
    RECT THEME_WINPLUS_DRAW_TRACK::getMainBarRect(mbm::COMPONENT_INFO &component)
    {
        TRACK_BAR_INFO *infoTrack = component.ptrWindow->getInfoTrack(component.id);
        if (infoTrack && infoTrack->isVertical)
        {
            const int width = (component.rect.right - component.rect.left);
            const int halfW = width / 2;
            RECT      rectBar;
            rectBar.left = component.rect.left + halfW - 5;
            rectBar.top  = component.rect.top + 5;

            rectBar.right  = component.rect.right - halfW + 5;
            rectBar.bottom = component.rect.bottom - 5;
            return rectBar;
        }
        else
        {
            const int height = (component.rect.bottom - component.rect.top);
            const int halfH  = height / 2;
            RECT      rectBar;
            rectBar.left = component.rect.left + 5;
            rectBar.top  = component.rect.top + halfH - 5;

            rectBar.right  = component.rect.right - 5;
            rectBar.bottom = component.rect.bottom - halfH + 5;
            return rectBar;
        }
    }
    void THEME_WINPLUS_DRAW_TRACK::drawBrush(const RECT & rect, HBRUSH hbrush)
    {
        HGDIOBJ original = this->setBrush(hbrush);
        this->drawRectangle(rect);
        this->setBrush(original);
    }
    void THEME_WINPLUS_DRAW_TRACK::DrawMyText(mbm::COMPONENT_INFO &component, const char *text, const bool isVertical)
    {
        DWORD           color_red = RGB(255, 20, 20);
        mbm::USER_DRAWER *userDrawer  = component.userDrawer;
        const int       x         = isVertical ? 0 : 5;
        const int       y         = component.rect.bottom - 20;
        if (component.isPressed && (userDrawer == nullptr || userDrawer->enablePressed))
            this->drawText(x, y, color_red, text);
        else
            this->drawText(x, y, color_text_release, text);
    }

    THEME_WINPLUS_DRAW_BUTTON::THEME_WINPLUS_DRAW_BUTTON(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        THEME_WINPLUS_BRUSH_HOVER::initHover(this);
        THEME_WINPLUS_BRUSH_PRESS::initPress(this);
        THEME_WINPLUS_BRUSH_RELEASE::initRelease(this);
    }
    bool THEME_WINPLUS_DRAW_BUTTON::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HPEN oldPen = nullptr;
            if (component.isPressed)
            {
                if (userDrawer == nullptr || userDrawer->enablePressed)
                {
                    if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                    else
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_press_from);
                    drawBrush(component, press_brush);
                }
                else
                {
                    if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                    else
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                    drawBrush(component, release_brush);
                }
            }
            else if (component.isHover)
            {
                if (userDrawer == nullptr || userDrawer->enableHover)
                {
                    if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                    else
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
                    drawBrush(component, hover_brush);
                }
                else
                {
                    if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                    else
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                    drawBrush(component, release_brush);
                }
            }
            else
            {
                if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                else
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                drawBrush(component, release_brush);
            }
            DrawMyText(component, 5, 0);
            this->setPen(oldPen);
        }
        return true;
    }

    THEME_WINPLUS_DRAW_LABEL::THEME_WINPLUS_DRAW_LABEL(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        THEME_WINPLUS_BRUSH_HOVER::initHover(this);
        THEME_WINPLUS_BRUSH_RELEASE::initRelease(this);
    }
    bool THEME_WINPLUS_DRAW_LABEL::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HPEN oldPen = nullptr;
            if (userDrawer && userDrawer->enableHover)
            {
                if (component.isHover)
                {
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
                    drawBrush(component, hover_brush);
                }
                else
                {
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                    drawBrush(component, release_brush);
                }
            }
            else
            {
                oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                drawBrush(component, release_brush);
            }
            DrawMyText(component, 4, 0);
            this->setPen(oldPen);
        }
        return true;
    }
    void THEME_WINPLUS_DRAW_LABEL::DrawMyText(mbm::COMPONENT_INFO &component, int x, int y)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        const WORD      len      = (WORD)component.ptrWindow->getTextLength(component.id);
        if (len)
        {
            std::vector<char> text;
            text.resize(len);
            if (component.ptrWindow->getText(component.id, &text[0], len))
            {
                if (userDrawer)
                {
                    if (userDrawer->enableHover)
                        this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_hover, &text[0]);
                    else
                        this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_release, &text[0]);
                }
                else
                {
                    this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_release, &text[0]);
                }
            }
        }
    }

    THEME_WINPLUS_DRAW_SCROLL::THEME_WINPLUS_DRAW_SCROLL(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        THEME_WINPLUS_BRUSH_HOVER::initHover(this);
        THEME_WINPLUS_BRUSH_RELEASE::initRelease(this);
    }

    bool THEME_WINPLUS_DRAW_SCROLL::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HPEN oldPen = nullptr;
            if (userDrawer && userDrawer->enableHover)
            {
                oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
                drawBrush(component, hover_brush);
            }
            else
            {
                oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                drawBrush(component, release_brush);
            }
            this->setPen(oldPen);
        }
        return false;
    }

    THEME_WINPLUS_DRAW_IMAGE::THEME_WINPLUS_DRAW_IMAGE(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        THEME_WINPLUS_BRUSH_RELEASE::initRelease(this);
        this->hover_brush    = static_cast<HBRUSH>(component.extraParams);
        this->useTranparency = true;
        this->colorKeying    = RGB(0, 0, 0);
    }

    bool THEME_WINPLUS_DRAW_IMAGE::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HPEN oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_noPen);
            drawBrush(component, release_brush); // background
            drawBrush(component, hover_brush);   // icon
            this->setPen(oldPen);
        }
        return true;
    }
    THEME_WINPLUS_DRAW_IMAGE::~THEME_WINPLUS_DRAW_IMAGE()
    {
    }

    THEME_WINPLUS_DRAW_TOOL_TIP::THEME_WINPLUS_DRAW_TOOL_TIP(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        THEME_WINPLUS_BRUSH_HOVER::initHover(this, true, true);
    }
    bool THEME_WINPLUS_DRAW_TOOL_TIP::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HPEN oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
            drawBrush(component, hover_brush);
            DrawMyText(component, 5, 0);
            this->setPen(oldPen);
        }
        return true;
    }
    void THEME_WINPLUS_DRAW_TOOL_TIP::DrawMyText(mbm::COMPONENT_INFO &component, int x, int y)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        const WORD      len      = (WORD)component.ptrWindow->getTextLength(component.id);
        if (len)
        {
            std::vector<char> text;
            text.resize(len);
            if (component.ptrWindow->getText(component.id, &text[0], len))
            {
                if (userDrawer)
                {
                    if (userDrawer->enableHover)
                        this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_hover, &text[0]);
                    else
                        this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_release, &text[0]);
                }
                else
                {
                    this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_hover, &text[0]);
                }
            }
        }
    }


    THEME_WINPLUS_DRAW_MENU::THEME_WINPLUS_DRAW_MENU(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        THEME_WINPLUS_BRUSH_HOVER::initHover(this);
        THEME_WINPLUS_BRUSH_RELEASE::initRelease(this);
    }
    bool THEME_WINPLUS_DRAW_MENU::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HPEN oldPen = nullptr;
            if (component.isHover || (userDrawer != nullptr && userDrawer->enableHover))
            {
                if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                else
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
                drawBrush(component, hover_brush);
            }
            else
            {
                if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                else
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                drawBrush(component, release_brush);
            }
            DrawMyText(component, 4, 0);
            this->setPen(oldPen);
        }
        return true;
    }

    THEME_WINPLUS_DRAW_SUB_MENU::THEME_WINPLUS_DRAW_SUB_MENU(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        THEME_WINPLUS_BRUSH_RELEASE::initRelease(this);
        const mbm::WINDOW::__MENU_DRAW *menu = component.ptrWindow->getMenuInfo(component.id);
        component.rect.bottom                = menu->minSize[1];
        THEME_WINPLUS_BRUSH_HOVER::initHover(this);
    }
    THEME_WINPLUS_DRAW_SUB_MENU::~THEME_WINPLUS_DRAW_SUB_MENU()
    {
    }
    bool THEME_WINPLUS_DRAW_SUB_MENU::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HPEN oldPen = nullptr;
            if (component.isHover && userDrawer != nullptr && userDrawer->enableHover)
            {
                if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                else
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
                drawBrush(component, hover_brush);
            }
            else
            {
                if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                else
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                drawBrush(component, release_brush);
            }
            DrawMyTextSubMenu(component);
            this->setPen(oldPen);
        }
        return true;
    }
    void THEME_WINPLUS_DRAW_SUB_MENU::DrawMyTextSubMenu(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *                userDrawer = component.userDrawer;
        const mbm::WINDOW::__MENU_DRAW *menu     = component.ptrWindow->getMenuInfo(component.id);
        const unsigned int              s        = menu->lsSubMenusTitles.size();
        const int                       height   = menu->minSize[1];
        POINT                           mouse    = {0, 0};
        GetCursorPos(&mouse);
        MapWindowPoints(HWND_DESKTOP, menu->hwndSubMenu, &mouse, 1);
        if (s)
        {
            for (unsigned int i = 0; i < s; ++i)
            {
                const int   x       = 4;
                const int   y       = height * (i);
                const char *text    = menu->lsSubMenusTitles[i].c_str();
                bool        isHover = mouse.y > y && mouse.y < menu->lsSubMenusHeight[i];
                if (text[0] == 0)
                {
                    const int m      = height / 2 + y;
                    POINT     p1     = {0, m};
                    POINT     p2     = {component.rect.right, m};
                    HPEN      oldpen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                    this->drawLine(p1, p2);
                    this->setPen(oldpen);
                }
                else if (userDrawer)
                {
                    if (isHover)
                    {
                        HPEN oldPen           = nullptr;
                        component.rect.top    = y;
                        component.rect.bottom = y + menu->minSize[1];
                        if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                            oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                        else
                            oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
                        drawBrush(component, hover_brush);
                        this->setPen(oldPen);
                    }
                    if (component.isHover && userDrawer->enableHover && isHover)
                        this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_hover, text);
                    else
                        this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_release, text);
                }
                else
                {
                    if (isHover)
                    {
                        HPEN oldPen           = nullptr;
                        component.rect.top    = y;
                        component.rect.bottom = y + menu->minSize[1];
                        if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                            oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                        else
                            oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
                        drawBrush(component, hover_brush);
                        this->setPen(oldPen);
                    }
                    if (component.isHover && isHover)
                        this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_hover, text);
                    else
                        this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_release, text);
                }
            }
        }
    }


    THEME_WINPLUS_DRAW_SPIN::THEME_WINPLUS_DRAW_SPIN(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        component.rect.right += 3;
        THEME_WINPLUS_BRUSH_HOVER::initHover(this);
        THEME_WINPLUS_BRUSH_PRESS::initPress(this);
        THEME_WINPLUS_BRUSH_RELEASE::initRelease(this);
        this->hBrushSolidRelease = CreateSolidBrush(color_text_hover);
    }
    THEME_WINPLUS_DRAW_SPIN::~THEME_WINPLUS_DRAW_SPIN()
    {
        DeleteObject(this->hBrushSolidRelease);
        this->hBrushSolidRelease = nullptr;
    }
    bool THEME_WINPLUS_DRAW_SPIN::hasHover()
    {
        return true;
    }
    bool THEME_WINPLUS_DRAW_SPIN::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HPEN oldPen = nullptr;
            if (component.isPressed)
            {
                if (userDrawer == nullptr || userDrawer->enablePressed)
                {
                    if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                    else
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_press_from);
                    drawBrush(component, press_brush);
                }
                else
                {
                    if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                    else
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                    drawBrush(component, release_brush);
                }
            }
            else if (component.isHover)
            {
                if (userDrawer == nullptr || userDrawer->enableHover)
                {
                    if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                    else
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
                    drawBrush(component, hover_brush);
                }
                else
                {
                    if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                    else
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                    drawBrush(component, release_brush);
                }
            }
            else
            {
                if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                else
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                drawBrush(component, release_brush);
            }
            const int sx       = 10;
            const int margin   = 4;
            const int height   = 3;
            const int m_height = component.rect.bottom / 2;
            POINT     p[6];
            p[0].x = component.rect.right - sx - margin;
            p[0].y = m_height - 1;
            p[1].x = component.rect.right - (sx / 2) - margin;
            p[1].y = component.rect.top + height;
            p[2].x = component.rect.right - margin;
            p[2].y = m_height - 1;

            p[3].x = component.rect.right - sx - margin;
            p[3].y = m_height + 2;
            p[4].x = component.rect.right - (sx / 2) - margin;
            p[4].y = component.rect.bottom - height;
            p[5].x = component.rect.right - margin;
            p[5].y = m_height + 2;

            if (component.isHover)
                this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_hover);
            else
                this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
            this->drawLine(p[0], p[1]);
            this->drawLine(p[1], p[2]);
            this->drawLine(p[0], p[2]);

            this->drawLine(p[3], p[4]);
            this->drawLine(p[4], p[5]);
            this->drawLine(p[5], p[3]);

            if (component.isHover)
            {
                this->setBrush(this->hBrushSolidRelease);
                if (component.mouse.y <= m_height)
                    this->drawPoygon(p, 3);
                else
                    this->drawPoygon(&p[3], 3);
            }
        }
        return true;
    }

    THEME_WINPLUS_DRAW_TEXT::THEME_WINPLUS_DRAW_TEXT(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        THEME_WINPLUS_BRUSH_HOVER::initHover(this);
        THEME_WINPLUS_BRUSH_RELEASE::initRelease(this);
    }
    THEME_WINPLUS_DRAW_TEXT::~THEME_WINPLUS_DRAW_TEXT()
    {
    }
    bool THEME_WINPLUS_DRAW_TEXT::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HPEN oldPen = nullptr;
            if (component.isHover && (userDrawer == nullptr || userDrawer->enableHover))
            {
                if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                else
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
                drawBrush(component, hover_brush);
            }
            else
            {
                if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                else
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                drawBrush(component, release_brush);
            }
            int       y      = 0;
            const int margin = LOWORD(SendMessageA(component.hwnd, EM_GETMARGINS, 0, 0));
            const int count  = SendMessageA(component.hwnd, EM_GETLINECOUNT, 0, 0);
            const int line   = SendMessageA(component.hwnd, EM_GETFIRSTVISIBLELINE, 0, 0);
            DWORD     iStart = 0;
            DWORD     iEnd   = 0;
            SendMessageA(component.hwnd, EM_GETSEL, (WPARAM)&iStart, (LPARAM)&iEnd);
            for (int i = line; i < count; ++i)
            {
                const int  nextLine = i;
                const int  index    = SendMessageA(component.hwnd, EM_LINEINDEX, nextLine, 0);
                const WORD length   = (WORD)SendMessageA(component.hwnd, EM_LINELENGTH, index, 0);
                if (length > 0)
                {
                    auto myText = new char[length + 4];
                    memset(myText, 0, length + 4);
                    WORD *des         = (WORD *)myText;
                    *des              = (length < 3 ? 3 : length);
                    const WORD copied = (WORD)SendMessageA(component.hwnd, EM_GETLINE, (WPARAM)nextLine, (LPARAM)myText);
                    if (copied == length)
                    {
                        myText[length] = 0;
                        if (component.isHover && (userDrawer == nullptr || userDrawer->enableHover))
                        {
                            if (iStart != iEnd)
                                this->drawSelecteLine(component, iStart, iEnd, index, index + length,
                                                      this->release_brush);
                            this->drawText(margin, y, color_text_hover, myText);
                        }
                        else
                        {
                            if (iStart != iEnd)
                                this->drawSelecteLine(component, iStart, iEnd, index, index + length, this->hover_brush);
                            this->drawText(margin, y, color_text_release, myText);
                        }
                        const SIZE sz = this->getSizeText(myText, component.hwnd);
                        y += sz.cy + margin;
                    }
                    delete[] myText;
                }
                else
                {
                    char       otherText[2] = {'A', 0};
                    const SIZE sz           = this->getSizeText(otherText, component.hwnd);
                    y += sz.cy + margin;
                }
            }
            this->setPen(oldPen);
        }
        return true;
    }

    void THEME_WINPLUS_DRAW_TEXT::drawSelecteLine(mbm::COMPONENT_INFO &component, const DWORD iStart, const DWORD iEnd, const DWORD indexStartLine,
                         const DWORD indexEndLine, HBRUSH brushRect)
    {
        if (indexEndLine >= iStart && indexStartLine <= iEnd)
        {
            char        otherText[2] = {'A', 0};
            const SIZE  sz           = this->getSizeText(otherText, component.hwnd);
            const DWORD ret1         = SendMessageA(component.hwnd, EM_POSFROMCHAR,
                                            (WPARAM)indexStartLine > iStart ? indexStartLine : iStart, (LPARAM)0);
            const DWORD ret2 = SendMessageA(component.hwnd, EM_POSFROMCHAR,
                                            (WPARAM)indexEndLine > iEnd ? iEnd : indexEndLine, (LPARAM)0);
            const POINTL p1     = {LOWORD(ret1), HIWORD(ret1)};
            POINTL       p2     = {LOWORD(ret2), HIWORD(ret2)};
            component.rect.left = p1.x;
            component.rect.top  = p1.y;
            if (p2.x == 65535 || p2.y == 65535)
            {
                const DWORD ret3 = SendMessageA(component.hwnd, EM_POSFROMCHAR, (WPARAM)iEnd - 1, (LPARAM)0);
                p2.x             = LOWORD(ret3) + sz.cx;
                p2.y             = HIWORD(ret3) + sz.cy;
            }
            component.rect.right  = p2.x;
            component.rect.bottom = p2.y + sz.cy;
            this->drawFrameRect(component.rect, brushRect);
        }
    }

    THEME_WINPLUS_DRAW_RADIO::THEME_WINPLUS_DRAW_RADIO(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BUTTON(component)
    {
        this->part            = 4;
        int wh                = component.rect.bottom - component.rect.top;
        int _1_part           = wh / this->part;
        int width             = (int)((float)wh / 2.0f) - _1_part;
        component.rect.left   = 0;
        component.rect.right  = width;
        component.rect.top    = 0;
        component.rect.bottom = width;
        this->checked_brush   = CreateSolidBrush(THEME_WINPLUS_COLORS::color_text_release);
    }
    THEME_WINPLUS_DRAW_RADIO::~THEME_WINPLUS_DRAW_RADIO()
    {
        DeleteObject(this->checked_brush);
    }
    bool THEME_WINPLUS_DRAW_RADIO::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HGDIOBJ original = nullptr;
            HPEN    oldPen   = nullptr;
            if (component.isHover && (userDrawer == nullptr || userDrawer->enableHover))
            {
                if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                else
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
                drawBrush(component, hover_brush);
                original = this->setBrush(release_brush);
                this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_hover);
            }
            else
            {
                if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                else
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                drawBrush(component, release_brush);
                original = this->setBrush(release_brush);
                this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
            }
            int wh      = component.rect.bottom - component.rect.top;
            int _1_part = wh / this->part;
            int width   = (int)((float)wh / 2.0f) - _1_part;
            int pos     = wh / 2;
            this->drawCircle(pos, pos, width);
            this->setBrush(original);
            if (component.isPressed)
            {
                original = this->setBrush(checked_brush);
                this->drawCircle(pos, pos, width);
                this->setBrush(original);
            }
            DrawMyText(component, wh + _1_part, 0);
            this->setPen(oldPen);
        }
        return true;
    }

    THEME_WINPLUS_DRAW_CHECK::THEME_WINPLUS_DRAW_CHECK(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BUTTON(component)
    {
        int wh                = component.rect.bottom - component.rect.top;
        int width             = (int)((float)wh / 2.0f);
        int _1_10             = wh / 10;
        component.rect.left   = 0;
        component.rect.right  = width - _1_10;
        component.rect.top    = 0;
        component.rect.bottom = width - _1_10;
        this->checked_brush   = CreateSolidBrush(THEME_WINPLUS_COLORS::color_text_release);
    }
    THEME_WINPLUS_DRAW_CHECK::~THEME_WINPLUS_DRAW_CHECK()
    {
        DeleteObject(this->checked_brush);
    }
    bool THEME_WINPLUS_DRAW_CHECK::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HGDIOBJ original = nullptr;
            HPEN    oldPen   = nullptr;
            if (component.isHover && (userDrawer == nullptr || userDrawer->enableHover))
            {
                if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                else
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
                drawBrush(component, hover_brush);
                original = this->setBrush(release_brush);
                this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_hover);
            }
            else
            {
                if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                else
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                drawBrush(component, release_brush);
                original = this->setBrush(release_brush);
                this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
            }
            int wh    = component.rect.bottom - component.rect.top;
            int _1_10 = wh / 10;
            int width = (int)((float)wh / 2.0f);
            int pos   = (wh - width) / 2;
            this->drawRectangle(pos, pos, width, width);
            this->setBrush(original);
            if (component.isPressed)
            {
                original       = this->setBrush(checked_brush);
                const int diff = (_1_10 / 2);
                this->drawRectangle(pos + diff, pos + diff, width - _1_10, width - _1_10);
                this->setBrush(original);
            }
            DrawMyText(component, wh + _1_10, 0);
            this->setPen(oldPen);
        }
        return true;
    }

    THEME_WINPLUS_DRAW_LIST::THEME_WINPLUS_DRAW_LIST(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        THEME_WINPLUS_BRUSH_HOVER::initHover(this);
        THEME_WINPLUS_BRUSH_RELEASE::initRelease(this);
    }
    HBRUSH THEME_WINPLUS_DRAW_LIST::getBackgroundBrush()
    {
        return release_brush;
    }
    bool THEME_WINPLUS_DRAW_LIST::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HPEN             oldPen = nullptr;
            COLORREF         clrForeground;
            LPDRAWITEMSTRUCT lpdis = component.lpdis;
            if (lpdis == nullptr || lpdis->itemID == -1)
            {
                oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                drawBrush(component, release_brush);
                this->setPen(oldPen);
                return true;
            }
            if (lpdis->itemState & ODS_SELECTED)
            {
                if (component.isHover && (userDrawer == nullptr || userDrawer->enableHover))
                {
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
                    drawBrush(component, hover_brush);
                }
                else
                {
                    oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                    drawBrush(component, release_brush);
                }
                clrForeground = SetTextColor(lpdis->hDC, this->color_text_hover);
            }
            else
            {
                oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                drawBrush(component, release_brush);
                clrForeground = SetTextColor(lpdis->hDC, this->color_text_release);
            }
            TEXTMETRIC tm;
            GetTextMetrics(lpdis->hDC, &tm);
            int         y    = (lpdis->rcItem.bottom + lpdis->rcItem.top - tm.tmHeight) / 2;
            int         x    = LOWORD(GetDialogBaseUnits()) / 4;
            const char *text = (const char *)lpdis->itemData;
            if (lpdis->itemState & ODS_SELECTED)
                this->drawText(x, y, color_text_hover, text);
            else
                this->drawText(x, y, color_text_release, text);
            SetTextColor(lpdis->hDC, clrForeground);
            this->setPen(oldPen);
        }
        return true;
    }

    THEME_WINPLUS_DRAW_PROGRESS_BAR::THEME_WINPLUS_DRAW_PROGRESS_BAR(mbm::COMPONENT_INFO &component) : mbm::DRAW(&component)
    {
        THEME_WINPLUS_BRUSH_RELEASE::initRelease(this);
        THEME_WINPLUS_BRUSH_HOVER::initHover(this);
    }
    THEME_WINPLUS_DRAW_PROGRESS_BAR::~THEME_WINPLUS_DRAW_PROGRESS_BAR()
    {
    }
    bool THEME_WINPLUS_DRAW_PROGRESS_BAR::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HPEN        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
            const RECT  _real(component.rect);
            RECT        _passed(component.rect);
            RECT        _final(component.rect);
            const float w   = (float)(_real.right - _real.left);
            const int   pos = SendMessageA(component.hwnd, PBM_GETPOS, 0, 0);
            const float l   = ((float)(pos / 100.0f));
            _passed.right   = (LONG)(l * w) + _real.left;
            component.rect  = _passed;
            drawBrush(component, hover_brush);
            _final.left    = _passed.right;
            component.rect = _final;
            drawBrush(component, release_brush);
            this->setPen(oldPen);
        }
        return true;
    }
    void THEME_WINPLUS_DRAW_PROGRESS_BAR::drawBrush(mbm::COMPONENT_INFO &component, HBRUSH hbrush)
    {
        HGDIOBJ original = this->setBrush(hbrush);
        this->drawRectangle(component.rect);
        this->setBrush(original);
    }


        THEME_WINPLUS_COMBO::DRAW_COMBO::DRAW_COMBO(mbm::DRAW *draw)
        {
            THEME_WINPLUS_BRUSH_HOVER::initHover(draw);
            this->initPress(draw);
            THEME_WINPLUS_BRUSH_RELEASE::initRelease(draw);
        }
    THEME_WINPLUS_COMBO::THEME_WINPLUS_COMBO(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        this->hBrushSolidRelease = CreateSolidBrush(color_text_hover);

        component.rect.bottom += 1;
        component.rect.top -= 3;
        component.rect.left -= 3;
        component.rect.right -= 3;

        COMBOBOXINFO pcbi;
        memset(&pcbi, 0, sizeof(pcbi));
        pcbi.cbSize = sizeof(pcbi);
        GetComboBoxInfo(component.hwnd, &pcbi);
        RECT rc1 = {0, 0, 0, 0};
        RECT rc3 = {0, 0, 0, 0};
        GetClientRect(pcbi.hwndCombo, &rc1);
        GetClientRect(pcbi.hwndList, &rc3);

        this->infoActualComponent = &component;
        {
            component.rect = rc1;
            this->inner    = new DRAW_COMBO(this);
        }
        {
            component.rect = rc3;
            this->out      = new DRAW_COMBO(this);
        }
    }
    THEME_WINPLUS_COMBO::~THEME_WINPLUS_COMBO()
    {
        delete this->inner;
        delete this->out;
        this->inner = nullptr;
        this->out   = nullptr;
        DeleteObject(this->hBrushSolidRelease);
        this->hBrushSolidRelease = nullptr;
    }
    bool THEME_WINPLUS_COMBO::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            HPEN hpenOld = nullptr;
            if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                hpenOld = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
            else if (component.isHover)
                hpenOld = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_hover);
            else
                hpenOld = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
            COLORREF         clrForeground;
            LPDRAWITEMSTRUCT lpdis = component.lpdis;

            if (lpdis == nullptr || lpdis->itemID == -1)
            {

                if (component.isHover)
                    this->drawBrush(component, this->inner->hover_brush);
                else
                    this->drawBrush(component, this->inner->release_brush);
                const WORD len = (WORD)component.ptrWindow->getTextLength(component.id);
                if (len)
                {
                    std::vector<char> text;
                    text.resize(len);
                    if (component.ptrWindow->getText(component.id, &text[0], len))
                        drawText(component, &text[0]);
                }
                const int sx     = 8;
                const int margin = 4;
                const int height = 5;
                POINT     p[6];
                p[0].x = component.rect.right - sx - margin;
                p[0].y = height;
                p[1].x = component.rect.right - margin;
                p[1].y = height;
                p[2].x = component.rect.right - (sx / 2) - margin;
                p[2].y = component.rect.bottom - height;
                p[3].x = p[0].x;
                p[3].y = p[0].y;
                p[4].x = p[0].x - (margin / 2);
                p[4].y = margin;
                p[5].x = p[0].x - (margin / 2);
                p[5].y = component.rect.bottom - margin;
                if (component.isHover)
                    this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_hover);
                else
                    this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                this->drawLine(p[0], p[1]);
                this->drawLine(p[1], p[2]);
                this->drawLine(p[2], p[3]);
                this->drawLine(p[4], p[5]);

                if (component.isHover)
                {
                    this->setBrush(this->hBrushSolidRelease);
                    this->drawPoygon(p, 3);
                }
                return true;
            }
            this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_noPen);
            component.rect.top -= 3;
            component.rect.bottom += 3;
            if (lpdis->itemState & ODS_SELECTED)
            {
                if (component.isHover && (userDrawer == nullptr || userDrawer->enableHover))
                    drawBrush(component, this->out->hover_brush);
                else
                    drawBrush(component, this->out->release_brush);
                clrForeground = SetTextColor(lpdis->hDC, this->color_text_hover);
            }
            else
            {
                drawBrush(component, this->out->release_brush);
                clrForeground = SetTextColor(lpdis->hDC, this->color_text_release);
            }
            TEXTMETRIC tm;
            GetTextMetrics(lpdis->hDC, &tm);
            int         y    = (lpdis->rcItem.bottom + lpdis->rcItem.top - tm.tmHeight) / 2;
            int         x    = LOWORD(GetDialogBaseUnits()) / 4 + 4;
            const char *text = (const char *)lpdis->itemData;
            if (lpdis->itemState & ODS_SELECTED)
                DRAW::drawText(x, y, color_text_release, text);
            else
                DRAW::drawText(x, y, color_text_hover, text);
            SetTextColor(lpdis->hDC, clrForeground);
            this->setPen(hpenOld);
            if ((int)lpdis->CtlID == component.id)
                InvalidateRect(component.hwnd, 0, 0);
        }
        return true;
    }
    void THEME_WINPLUS_COMBO::drawText(mbm::COMPONENT_INFO &component, const char *text)
    {
        if (text)
        {
            mbm::USER_DRAWER *userDrawer = component.userDrawer;
            int             x        = 4;
            int             y        = component.lpdis ? component.lpdis->rcItem.top : component.rect.top;
            if (userDrawer)
            {
                if (component.isHover && userDrawer->enableHover)
                    DRAW::drawText(x, y, color_text_hover, text);
                else
                    DRAW::drawText(x, y, color_text_release, text);
            }
            else
            {
                if (component.isHover)
                    DRAW::drawText(x, y, color_text_hover, text);
                else
                    DRAW::drawText(x, y, color_text_release, text);
            }
        }
    }
    
    THEME_WINPLUS_DRAW_STATUS_BAR::THEME_WINPLUS_DRAW_STATUS_BAR(mbm::COMPONENT_INFO &component) : THEME_WINPLUS_DRAW_BASIC_OPERATIONS(component)
    {
        THEME_WINPLUS_BRUSH_HOVER::initHover(this);
        THEME_WINPLUS_BRUSH_RELEASE::initRelease(this);
    }
    bool THEME_WINPLUS_DRAW_STATUS_BAR::render(mbm::COMPONENT_INFO &component)
    {
        mbm::USER_DRAWER *userDrawer = component.userDrawer;
        if (userDrawer && userDrawer->render(component))
        {
            return true;
        }
        else
        {
            std::vector<std::string> *lsStatusBar = component.ptrWindow->getStatusBar(component.id);
            if (lsStatusBar)
            {
                HPEN oldPen = nullptr;
                if (component.isHover && (userDrawer != nullptr && userDrawer->enableHover))
                {
                    if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                    else
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_hover_from);
                    drawBrush(component, hover_brush);
                }
                else
                {
                    if (THEME_WINPLUS_PEN_COLORS::enablePenBorder)
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_text_release);
                    else
                        oldPen = this->setPen(THEME_WINPLUS_PEN_COLORS::hPen_color_release_from);
                    drawBrush(component, release_brush);
                }
                SIZE      totalSize   = {0, 4};
                const int space       = 20;
                SIZE      currentSize = {0, 0};
                for (unsigned int i = 0; i < lsStatusBar->size(); ++i)
                {
                    const int   x    = totalSize.cx + space;
                    const int   y    = 4;
                    const char *text = lsStatusBar->at(i).c_str();
                    if (text && strlen(text))
                    {
                        currentSize = this->getSizeText(text, component.hwnd);
                        if (userDrawer)
                        {
                            if (component.isHover && userDrawer->enableHover)
                                this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_hover, text);
                            else
                                this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_release, text);
                        }
                        else
                        {
                            this->drawText(x, y, THEME_WINPLUS_COLORS::color_text_release, text);
                        }
                        totalSize.cx += currentSize.cx + space;
                        totalSize.cy += currentSize.cy;
                    }
                    if ((i + 1) < lsStatusBar->size())
                    {
                        POINT p1 = {x + currentSize.cx + (space / 2), component.rect.top};
                        POINT p2 = {p1.x, component.rect.bottom};
                        this->drawLine(p1, p2);
                    }
                }
                this->setPen(oldPen);
            }
        }
        return true;
    }

    THEME_WINPLUS_CUSTOM_RENDER::THEME_WINPLUS_CUSTOM_RENDER()
    {
        this->font             = nullptr;
        static bool firstTheme = true;
        if (firstTheme)
        {
            THEME_WINPLUS_CUSTOM_RENDER::setTheme(11, true);
            firstTheme = false;
        }
    }
    THEME_WINPLUS_CUSTOM_RENDER::~THEME_WINPLUS_CUSTOM_RENDER()
    {
        this->release();
    }
    void THEME_WINPLUS_CUSTOM_RENDER::release()
    {
        for (std::map<int, DRAW *>::const_iterator it = this->lsDrawer.cbegin(); it != this->lsDrawer.cend(); ++it)
        {
            DRAW *cDrawer = it->second;
            delete cDrawer;
        }
        this->lsDrawer.clear();
        if (this->font)
            DeleteFont(this->font);
        this->font = nullptr;

        THEME_WINPLUS_PEN_COLORS::release();
    }
    int THEME_WINPLUS_CUSTOM_RENDER::measureItem(mbm::COM_BETWEEN_WINP *ptr, MEASUREITEMSTRUCT *lpi)
    {
        if (ptr->getType() == mbm::WINPLUS_TYPE_COMBO_BOX)
        {
            int i = lpi->itemID;
            if (i == -1)
                lpi->itemHeight -= 6;
        }
        else if (ptr->getType() == mbm::WINPLUS_TYPE_MENU || ptr->getType() == mbm::WINPLUS_TYPE_SUB_MENU)
        {
            const char *text = (const char *)lpi->itemData;
            if (text)
            {
                SIZE size = getSizeText(text, ptr->getHwnd());
                if (size.cx != 0 && size.cy != 0)
                {
                    lpi->itemHeight = size.cy + 4;
                    lpi->itemWidth  = size.cx + 4;
                }
            }
            else
            {
                lpi->itemHeight = 30;
                lpi->itemWidth  = 200;
            }
        }
        return 1;
    }
    void THEME_WINPLUS_CUSTOM_RENDER::init()
    {
        if (this->font == nullptr)
        {
            this->font = this->createFont("Gill Sans MT", 20, 5, 0, 0, 400, 0, 0, 0);
            THEME_WINPLUS_PEN_COLORS::init();
        }
    }
    bool THEME_WINPLUS_CUSTOM_RENDER::render(mbm::COMPONENT_INFO &component)
    {
        this->init();
        DRAW *     drawer    = nullptr;
        mbm::DRAW *otherDraw = this->lsDrawer[component.id];
        //if (otherDraw)
        //    return false;
        if (otherDraw && otherDraw->render(component))
        {
            return true;
        }

        switch (component.typeWindowWinPlus)
        {
            case mbm::WINPLUS_TYPE_CHILD_WINDOW:
            case mbm::WINPLUS_TYPE_WINDOW:
            case mbm::WINPLUS_TYPE_WINDOW_MESSAGE_BOX: { drawer = new THEME_WINPLUS_DRAW_WINDOW(component);
            }
            break;
            case mbm::WINPLUS_TYPE_IMAGE: { drawer = new THEME_WINPLUS_DRAW_IMAGE(component);
            }
            break;
            case mbm::WINPLUS_TYPE_WINDOWNC: { drawer = new THEME_WINPLUS_DRAW_WINDOWNC(component);
            }
            break;
            case mbm::WINPLUS_TYPE_TOOL_TIP: { drawer = new THEME_WINPLUS_DRAW_TOOL_TIP(component);
            }
            break;
            case mbm::WINPLUS_TYPE_LABEL: { drawer = new THEME_WINPLUS_DRAW_LABEL(component);
            }
            break;
            case mbm::WINPLUS_TYPE_BUTTON:
            case mbm::WINPLUS_TYPE_BUTTON_TAB: { drawer = new THEME_WINPLUS_DRAW_BUTTON(component);
            }
            break;
            case mbm::WINPLUS_TYPE_CHECK_BOX: { drawer = new THEME_WINPLUS_DRAW_CHECK(component);
            }
            break;
            case mbm::WINPLUS_TYPE_RADIO_BOX: { drawer = new THEME_WINPLUS_DRAW_RADIO(component);
            }
            break;
            case mbm::WINPLUS_TYPE_COMBO_BOX: { drawer = new THEME_WINPLUS_COMBO(component);
            }
            break;
            case mbm::WINPLUS_TYPE_LIST_BOX: { drawer = new THEME_WINPLUS_DRAW_LIST(component);
            }
            break;
            case mbm::WINPLUS_TYPE_TEXT_BOX: { drawer = new THEME_WINPLUS_DRAW_TEXT(component);
            }
            break;
            case mbm::WINPLUS_TYPE_SCROLL: { drawer = new THEME_WINPLUS_DRAW_SCROLL(component);
            }
            break;
            case mbm::WINPLUS_TYPE_SPIN_INT:
            case mbm::WINPLUS_TYPE_SPIN_FLOAT: { drawer = new THEME_WINPLUS_DRAW_SPIN(component);
            }
            break;
            case mbm::WINPLUS_TYPE_RICH_TEXT: { drawer = new THEME_WINPLUS_DRAW_TEXT(component);
            }
            break;
            case mbm::WINPLUS_TYPE_GROUP_BOX:
            case mbm::WINPLUS_TYPE_GROUP_BOX_TAB: { drawer = new THEME_WINPLUS_DRAW_LABEL(component);
            }
            break;
            case mbm::WINPLUS_TYPE_PROGRESS_BAR: { drawer = new THEME_WINPLUS_DRAW_PROGRESS_BAR(component);
            }
            break;
            case mbm::WINPLUS_TYPE_TIMER: {
            }
            break;
            case mbm::WINPLUS_TYPE_TRACK_BAR: { drawer = new THEME_WINPLUS_DRAW_TRACK(component);
            }
            break;
            case mbm::WINPLUS_TYPE_STATUS_BAR: { drawer = new THEME_WINPLUS_DRAW_STATUS_BAR(component);
            }
            break;
            case mbm::WINPLUS_TYPE_MENU: { drawer = new THEME_WINPLUS_DRAW_MENU(component);
            }
            break;
            case mbm::WINPLUS_TYPE_SUB_MENU: { drawer = new THEME_WINPLUS_DRAW_SUB_MENU(component);
            }
            break;

            case mbm::WINPLUS_TYPE_TRY_ICON_MENU: {
            }
            break;
            case mbm::WINPLUS_TYPE_TRY_ICON_SUB_MENU: {
            }
            break;
        }

        if (drawer)
        {
            component.ptrWindow->setDrawer(drawer, component.id);
            lsDrawer[component.id] = (drawer);
            SendMessage(component.hwnd, WM_SETFONT, (WPARAM) this->font, MAKELPARAM(FALSE, 0));
            InvalidateRect(component.hwnd, nullptr, 1);
            drawer->setFont(this->font);
        }
        else
        {
            const WORD len = (WORD)component.ptrWindow->getTextLength(component.id);
            if (len)
            {
                std::vector<char> text;
                text.resize(len);
                this->drawRectangle(component.rect);
                if (component.ptrWindow->getText(component.id, &text[0], len))
                    this->drawText(0, 0, 0, 0, 0, &text[0]);
            }
        }
        return true;
    }
    void THEME_WINPLUS_CUSTOM_RENDER::setTheme(int which, const bool enableBorder )
    {
        THEME_WINPLUS_PEN_COLORS::enablePenBorder = enableBorder;
        switch (which)
        {
            case 0: // white
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(200, 200, 200);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(200, 200, 200);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(50, 50, 50);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(100, 100, 100);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 100, 0);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(50, 20, 20);
            }
            break;
            case 1: // blue dark
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(16, 16, 255);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(16, 16, 16);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(200, 200, 200);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(40, 40, 40);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(100, 100, 100);
                THEME_WINPLUS_COLORS::color_text_release = RGB(252, 255, 0);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(40, 16, 16);
            }
            break;
            case 2: // black
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(0, 0, 0);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(37, 37, 37);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(37, 37, 37);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(0, 0, 0);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(0, 0, 0);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(77, 77, 77);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 100, 0);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(255, 255, 255);
            }
            break;
            case 3: // orange
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(140, 70, 0);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(255, 128, 64);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(255, 128, 0);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(128, 64, 0);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(255, 128, 0);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(79, 39, 0);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(204, 204, 204);
            }
            break;
            case 4: // burro quando foge
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(0, 128, 128);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(0, 0, 0);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(0, 0, 0);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(0, 128, 128);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(0, 128, 128);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(168, 255, 168);
            }
            break;
            case 5: // azul opaco
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(0, 128, 192);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(0, 128, 255);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(0, 128, 192);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(0, 67, 100);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(0, 81, 119);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(0, 128, 255);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(192, 192, 192);
            }
            break;
            case 6:
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(128, 64, 0);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(0, 64, 64);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(0, 64, 64);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(128, 64, 0);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(0, 64, 0);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(144, 144, 144);
                THEME_WINPLUS_COLORS::color_text_release = RGB(0, 255, 0);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(192, 192, 192);
            }
            break;
            case 7: // red blood
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(128, 0, 0);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(94, 0, 0);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(94, 0, 0);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(128, 0, 0);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(128, 0, 0);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(111, 0, 0);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(170, 0, 0);
            }
            break;
            case 8: // Blue visual studio theme
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(156, 170, 193);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(130, 150, 170);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(130, 150, 170);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(160, 170, 190);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(130, 150, 170);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(192, 192, 192);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(192, 192, 192);
            }
            break;
            case 9: // yellow
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(255, 251, 239);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(255, 232, 166);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(255, 232, 166);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(200, 200, 166);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(255, 255, 128);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(255, 255, 128);
                THEME_WINPLUS_COLORS::color_text_release = RGB(0, 0, 0);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(0, 0, 0);
            }
            break;
            case 10: // another blue
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(188, 221, 253);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(95, 172, 250);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(0, 128, 192);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(74, 197, 255);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(0, 128, 192);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(164, 225, 255);
                THEME_WINPLUS_COLORS::color_text_release = RGB(0, 0, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(0, 0, 0);
            }
            break;
            case 11: // no gradiente blue dark
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(41, 57, 85);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(41, 57, 85);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(61, 85, 126);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(61, 85, 126);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(32, 45, 66);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(32, 45, 66);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(0, 0, 0);
            }
            break;
            case 12: // no gradiente green ugly
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(128, 128, 0);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(128, 128, 0);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(128, 128, 64);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(128, 128, 64);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(111, 111, 55);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(111, 111, 55);
                THEME_WINPLUS_COLORS::color_text_release = RGB(128, 255, 128);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(0, 255, 0);
            }
            break;
            case 13: // no gradient grayed
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(200, 200, 200);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(200, 200, 200);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(150, 150, 150);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(150, 150, 150);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(128, 128, 128);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(128, 128, 128);
                THEME_WINPLUS_COLORS::color_text_release = RGB(0, 0, 0);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(64, 0, 0);
            }
            break;
            case 14: // no gradiente white black
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(0, 0, 0);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(0, 0, 0);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(0, 0, 0);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(0, 0, 0);
                THEME_WINPLUS_COLORS::color_text_release = RGB(0, 0, 0);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(255, 255, 255);
            }
            break;
            case 15: // no gradiente Orange dark
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(140, 70, 0);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(140, 70, 0);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(128, 64, 0);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(128, 64, 0);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(255, 128, 0);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(255, 128, 0);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(204, 204, 204);
            }
            break;
            case 16: // no gradient Red Dark
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(128, 0, 0);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(128, 0, 0);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(94, 0, 0);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(94, 0, 0);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(111, 0, 0);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(111, 0, 0);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(255, 0, 0);
            }
            break;
            case 17: // no gradient Visual studio 2013 theme
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(156, 170, 193);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(156, 170, 193);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(130, 150, 170);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(130, 150, 170);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(100, 100, 100);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(100, 100, 100);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(192, 192, 192);
            }
            break;
            case 18: // no gradient Green dark
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(0, 128, 128);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(0, 128, 128);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(0, 155, 155);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(0, 155, 155);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(0, 128, 128);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(0, 128, 128);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(192, 192, 192);
            }
            break;
            case 19: // no gradient Green dark / combined purple
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(128, 128, 192);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(128, 128, 192);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(0, 155, 155);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(0, 155, 155);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(128, 128, 192);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(128, 128, 192);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(192, 192, 192);
            }
            break;
            case 20: // no gradient blue  / combined orange
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(0, 128, 192);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(0, 128, 192);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(255, 128, 0);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(255, 128, 0);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(255, 128, 0);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(255, 128, 0);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(192, 192, 192);
            }
            break;
            case 21: // no gradient brown / combined blue
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(128, 64, 64);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(128, 64, 64);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(0, 128, 192);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(0, 128, 192);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(0, 128, 192);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(0, 128, 192);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(255, 255, 255);
            }
            break;
            case 22: // black 
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(37, 37, 37);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(70, 70, 70);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(70, 70, 70);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(37, 37, 37);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(37, 37, 37);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(87, 87, 87);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 100, 0);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(255, 255, 255);
            }
            break;
            case 23: // no gradient green  / combined blue
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(19, 145, 20);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(19, 145, 20);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(19, 23, 145);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(19, 23, 145);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(255, 128, 0);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(255, 128, 0);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(192, 192, 192);
            }
            break;
            case 24:
            {
                THEME_WINPLUS_COLORS::color_release_from = RGB(37, 37, 38);
                THEME_WINPLUS_COLORS::color_release_to   = RGB(37, 37, 38);
                THEME_WINPLUS_COLORS::color_hover_from   = RGB(90, 90, 95);
                THEME_WINPLUS_COLORS::color_hover_to     = RGB(90, 90, 95);
                THEME_WINPLUS_COLORS::color_press_from   = RGB(255, 128, 0);
                THEME_WINPLUS_COLORS::color_press_to     = RGB(255, 128, 0);
                THEME_WINPLUS_COLORS::color_text_release = RGB(255, 255, 255);
                THEME_WINPLUS_COLORS::color_text_hover   = RGB(192, 192, 192);
            }
            break;
        }
    }

    int THEME_WINPLUS_CUSTOM_RENDER::getTotalThemes()
    {
        return 25;
    }


namespace mbm
{
    THEME_WINPLUS_CUSTOM_RENDER _winplusDefaultThemeDrawRender;
    DRAW *                      _winplusDefaultThemeDraw = (DRAW *)&_winplusDefaultThemeDrawRender;
};
