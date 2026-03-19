/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2021      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifdef __linux__

#include "mini-mbm-lib.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include <cstring>

extern std::string my_app_name;

namespace mbm
{

    // Simple X11 dialog for resolution/app selection
    bool select_app_and_resolution(APP_RUN* app_run, int size_app_run, int * index_app_selected, SCREEN_RESOLUTION* screen_resolution_list, int size_screen_resolution_list, bool allow_full_screen, const bool full_screen_checked, int requested_width, int requested_height)
    {
        Display* display = XOpenDisplay(nullptr);
        if (!display)
        {
            ERROR_LOG("Cannot open X11 display");
            return false;
        }
        
        int screen = DefaultScreen(display);
        Window root = RootWindow(display, screen);
        
        // Get monitor info using Xrandr
        int num_monitors = 0;
        XRRMonitorInfo* monitors = XRRGetMonitors(display, root, True, &num_monitors);
        
        // Get current screen dimensions
        int max_width = DisplayWidth(display, screen);
        int max_height = DisplayHeight(display, screen);
        
        // Default resolutions if none provided
        static SCREEN_RESOLUTION default_resolutions[] = {
            {640,     360,   "Low resolution"},
            {800,     600,   "XVGA"},
            {960,     540,   "qHD"},
            {1024,    768,   "XGA"},
            {1280,    720,   "Standard High Density (HD)"},
            {1280,    768,   "WXGA"},
            {1280,    800,   "WXGA"},
            {1600,    900,   "HD+"},
            {1920,    1080,  "Standard Full HD Display"},
            {2560,    1440,  "Standard Quad HD Display"},
            {3200,    1800,  "QHD+"},
            {3840,    2160,  "Standard Ultra HD Display"},
        };
        
        if (screen_resolution_list == nullptr)
        {
            size_screen_resolution_list = sizeof(default_resolutions) / sizeof(SCREEN_RESOLUTION);
            screen_resolution_list = default_resolutions;
        }
        
        // Selection state - valid_resolutions is rebuilt each redraw based on selected monitor
        int selected_monitor = 0;
        int selected_resolution = 0;
        int selected_width_prev = 0, selected_height_prev = 0; // preserve across monitor change
        int selected_app = (index_app_selected && *index_app_selected >= 0) ? *index_app_selected : 0;
        bool full_screen = allow_full_screen && full_screen_checked;
        bool confirmed = false;
        
        // Window dimensions
        const int win_width = 420;
        const int win_height = (size_app_run > 0) ? 400 : 320;
        
        // Create window
        XSetWindowAttributes attrs;
        attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask;
        attrs.background_pixel = WhitePixel(display, screen);
        
        Window win = XCreateWindow(display, root,
            (max_width - win_width) / 2, (max_height - win_height) / 2,
            win_width, win_height, 1,
            CopyFromParent, InputOutput, CopyFromParent,
            CWEventMask | CWBackPixel, &attrs);
        
        // Set window title
        const char* title = my_app_name.length() > 0 ? my_app_name.c_str() : "Screen Options";
        XStoreName(display, win, title);
        
        // Create GC for drawing
        GC gc = XCreateGC(display, win, 0, nullptr);
        XSetForeground(display, gc, BlackPixel(display, screen));
        
        // Load font
        XFontStruct* font = XLoadQueryFont(display, "-*-helvetica-medium-r-*-*-14-*-*-*-*-*-*-*");
        if (!font) font = XLoadQueryFont(display, "fixed");
        if (font) XSetFont(display, gc, font->fid);
        
        // Make window appear
        XMapWindow(display, win);
        
        // UI element positions
        const int margin = 20;
        const int label_height = 20;
        const int combo_height = 25;
        const int spacing = 10;
        int y_pos = margin;
        
        // Button areas (will be calculated during draw)
        struct { int x, y, w, h; } monitor_up, monitor_down;
        struct { int x, y, w, h; } res_up, res_down;
        struct { int x, y, w, h; } app_up, app_down;
        struct { int x, y, w, h; } fullscreen_box;
        struct { int x, y, w, h; } start_btn;
        
        // Event loop - valid_resolutions persists for ButtonPress to update selection
        std::vector<SCREEN_RESOLUTION> valid_resolutions;
        bool running = true;
        while (running)
        {
            XEvent event;
            XNextEvent(display, &event);
            
            if (event.type == Expose && event.xexpose.count == 0)
            {
                // Get selected monitor dimensions (filter resolutions by this)
                int mon_width = max_width, mon_height = max_height;
                if (num_monitors > 0 && selected_monitor < num_monitors)
                {
                    mon_width = monitors[selected_monitor].width;
                    mon_height = monitors[selected_monitor].height;
                }
                
                // Build valid_resolutions: resolutions <= monitor size, with native as last
                valid_resolutions.clear();
                for (int i = 0; i < size_screen_resolution_list; i++)
                {
                    if (screen_resolution_list[i].width <= mon_width &&
                        screen_resolution_list[i].height <= mon_height)
                    {
                        valid_resolutions.push_back(screen_resolution_list[i]);
                    }
                }
                // When -w/-h passed, add requested resolution before native (if not in list)
                if (requested_width > 0 && requested_height > 0 && requested_width <= mon_width && requested_height <= mon_height)
                {
                    bool found = false;
                    for (size_t i = 0; i < valid_resolutions.size(); i++)
                    {
                        if (valid_resolutions[i].width == requested_width && valid_resolutions[i].height == requested_height)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        valid_resolutions.push_back({requested_width, requested_height, "Requested"});
                }
                // Add native monitor resolution as last if not already present
                bool has_native = false;
                for (size_t i = 0; i < valid_resolutions.size(); i++)
                {
                    if (valid_resolutions[i].width == mon_width && valid_resolutions[i].height == mon_height)
                    {
                        has_native = true;
                        break;
                    }
                }
                if (!has_native)
                    valid_resolutions.push_back({mon_width, mon_height, "Native"});
                if (valid_resolutions.empty())
                    valid_resolutions.push_back({mon_width, mon_height, "Native"});
                
                // When -w/-h passed, select requested resolution as default (only on first run, before user changes it)
                bool set_from_requested = false;
                if (requested_width > 0 && requested_height > 0 && requested_width <= mon_width && requested_height <= mon_height &&
                    selected_width_prev == 0 && selected_height_prev == 0)
                {
                    int found_index = -1;
                    for (size_t i = 0; i < valid_resolutions.size(); i++)
                    {
                        if (valid_resolutions[i].width == requested_width && valid_resolutions[i].height == requested_height)
                        {
                            found_index = static_cast<int>(i);
                            break;
                        }
                    }
                    if (found_index >= 0)
                    {
                        selected_resolution = found_index;
                        selected_width_prev = requested_width;
                        selected_height_prev = requested_height;
                        set_from_requested = true;
                    }
                }
                if (!set_from_requested)
                {
                    // Clamp selected_resolution to valid range; preserve selection when possible
                    if (selected_width_prev > 0 && selected_height_prev > 0)
                    {
                        int found = -1;
                        for (size_t i = 0; i < valid_resolutions.size(); i++)
                        {
                            if (valid_resolutions[i].width == selected_width_prev && valid_resolutions[i].height == selected_height_prev)
                            {
                                found = static_cast<int>(i);
                                break;
                            }
                        }
                        if (found >= 0)
                            selected_resolution = found;
                        else
                            selected_resolution = static_cast<int>(valid_resolutions.size()) - 1;
                    }
                    else
                    {
                        selected_resolution = static_cast<int>(valid_resolutions.size()) - 1;
                    }
                    selected_width_prev = valid_resolutions[selected_resolution].width;
                    selected_height_prev = valid_resolutions[selected_resolution].height;
                }
                
                // Clear window
                XClearWindow(display, win);
                y_pos = margin;
                
                // Draw monitor selection
                XDrawString(display, win, gc, margin, y_pos + 14, "Monitor:", 8);
                y_pos += label_height + 5;
                
                char monitor_str[256];
                if (num_monitors > 0 && selected_monitor < num_monitors)
                {
                    snprintf(monitor_str, sizeof(monitor_str), "%d: %dx%d at (%d,%d)",
                        selected_monitor + 1,
                        monitors[selected_monitor].width,
                        monitors[selected_monitor].height,
                        monitors[selected_monitor].x,
                        monitors[selected_monitor].y);
                }
                else
                {
                    snprintf(monitor_str, sizeof(monitor_str), "Primary: %dx%d", max_width, max_height);
                }
                
                // Draw combo-like box with arrows
                XDrawRectangle(display, win, gc, margin, y_pos, win_width - 2*margin - 60, combo_height);
                XDrawString(display, win, gc, margin + 5, y_pos + 17, monitor_str, strlen(monitor_str));
                
                // Up/Down buttons
                monitor_down = {win_width - margin - 55, y_pos, 25, combo_height};
                monitor_up = {win_width - margin - 27, y_pos, 25, combo_height};
                XDrawRectangle(display, win, gc, monitor_down.x, monitor_down.y, monitor_down.w, monitor_down.h);
                XDrawRectangle(display, win, gc, monitor_up.x, monitor_up.y, monitor_up.w, monitor_up.h);
                XDrawString(display, win, gc, monitor_down.x + 8, monitor_down.y + 17, "<", 1);
                XDrawString(display, win, gc, monitor_up.x + 8, monitor_up.y + 17, ">", 1);
                y_pos += combo_height + spacing + 10;
                
                // Draw resolution selection (when full screen, show monitor resolution since that will be used)
                XDrawString(display, win, gc, margin, y_pos + 14, "Resolution:", 11);
                y_pos += label_height + 5;
                
                char res_str[256];
                if (full_screen)
                {
                    int fs_w = (num_monitors > 0 && selected_monitor < num_monitors) ? monitors[selected_monitor].width : max_width;
                    int fs_h = (num_monitors > 0 && selected_monitor < num_monitors) ? monitors[selected_monitor].height : max_height;
                    snprintf(res_str, sizeof(res_str), "%d x %d (Full screen)", fs_w, fs_h);
                }
                else
                {
                    snprintf(res_str, sizeof(res_str), "%d x %d %s",
                        valid_resolutions[selected_resolution].width,
                        valid_resolutions[selected_resolution].height,
                        valid_resolutions[selected_resolution].description ? valid_resolutions[selected_resolution].description : "");
                }
                
                XDrawRectangle(display, win, gc, margin, y_pos, win_width - 2*margin - 60, combo_height);
                XDrawString(display, win, gc, margin + 5, y_pos + 17, res_str, strlen(res_str));
                
                res_down = {win_width - margin - 55, y_pos, 25, combo_height};
                res_up = {win_width - margin - 27, y_pos, 25, combo_height};
                XDrawRectangle(display, win, gc, res_down.x, res_down.y, res_down.w, res_down.h);
                XDrawRectangle(display, win, gc, res_up.x, res_up.y, res_up.w, res_up.h);
                XDrawString(display, win, gc, res_down.x + 8, res_down.y + 17, "<", 1);
                XDrawString(display, win, gc, res_up.x + 8, res_up.y + 17, ">", 1);
                y_pos += combo_height + spacing + 10;
                
                // Draw app selection if provided
                if (size_app_run > 0)
                {
                    XDrawString(display, win, gc, margin, y_pos + 14, "Application:", 12);
                    y_pos += label_height + 5;
                    
                    const char* app_name = "";
                    if (selected_app < size_app_run)
                    {
                        app_name = app_run[selected_app].name_eng ? app_run[selected_app].name_eng : app_run[selected_app].script_path;
                    }
                    
                    XDrawRectangle(display, win, gc, margin, y_pos, win_width - 2*margin - 60, combo_height);
                    if (app_name) XDrawString(display, win, gc, margin + 5, y_pos + 17, app_name, strlen(app_name));
                    
                    app_down = {win_width - margin - 55, y_pos, 25, combo_height};
                    app_up = {win_width - margin - 27, y_pos, 25, combo_height};
                    XDrawRectangle(display, win, gc, app_down.x, app_down.y, app_down.w, app_down.h);
                    XDrawRectangle(display, win, gc, app_up.x, app_up.y, app_up.w, app_up.h);
                    XDrawString(display, win, gc, app_down.x + 8, app_down.y + 17, "<", 1);
                    XDrawString(display, win, gc, app_up.x + 8, app_up.y + 17, ">", 1);
                    y_pos += combo_height + spacing + 10;
                }
                
                // Full screen checkbox
                if (allow_full_screen)
                {
                    fullscreen_box = {margin, y_pos, 20, 20};
                    XDrawRectangle(display, win, gc, fullscreen_box.x, fullscreen_box.y, fullscreen_box.w, fullscreen_box.h);
                    if (full_screen)
                    {
                        XDrawLine(display, win, gc, fullscreen_box.x + 3, fullscreen_box.y + 10, fullscreen_box.x + 8, fullscreen_box.y + 15);
                        XDrawLine(display, win, gc, fullscreen_box.x + 8, fullscreen_box.y + 15, fullscreen_box.x + 17, fullscreen_box.y + 3);
                    }
                    XDrawString(display, win, gc, fullscreen_box.x + 30, fullscreen_box.y + 15, "Full Screen", 11);
                    y_pos += 30 + spacing;
                }
                
                // Start button
                start_btn = {win_width - margin - 100, win_height - margin - 35, 100, 30};
                XDrawRectangle(display, win, gc, start_btn.x, start_btn.y, start_btn.w, start_btn.h);
                XDrawString(display, win, gc, start_btn.x + 30, start_btn.y + 20, "START", 5);
            }
            else if (event.type == ButtonPress)
            {
                int mx = event.xbutton.x;
                int my = event.xbutton.y;
                
                // Check monitor buttons
                if (mx >= monitor_down.x && mx <= monitor_down.x + monitor_down.w &&
                    my >= monitor_down.y && my <= monitor_down.y + monitor_down.h)
                {
                    if (selected_monitor > 0) selected_monitor--;
                    XClearArea(display, win, 0, 0, 0, 0, True);
                }
                else if (mx >= monitor_up.x && mx <= monitor_up.x + monitor_up.w &&
                         my >= monitor_up.y && my <= monitor_up.y + monitor_up.h)
                {
                    if (selected_monitor < num_monitors - 1) selected_monitor++;
                    XClearArea(display, win, 0, 0, 0, 0, True);
                }
                // Check resolution buttons
                else if (mx >= res_down.x && mx <= res_down.x + res_down.w &&
                         my >= res_down.y && my <= res_down.y + res_down.h)
                {
                    if (selected_resolution > 0)
                    {
                        selected_resolution--;
                        if (!valid_resolutions.empty() && selected_resolution < (int)valid_resolutions.size())
                        {
                            selected_width_prev = valid_resolutions[selected_resolution].width;
                            selected_height_prev = valid_resolutions[selected_resolution].height;
                        }
                    }
                    XClearArea(display, win, 0, 0, 0, 0, True);
                }
                else if (mx >= res_up.x && mx <= res_up.x + res_up.w &&
                         my >= res_up.y && my <= res_up.y + res_up.h)
                {
                    if (selected_resolution < (int)valid_resolutions.size() - 1)
                    {
                        selected_resolution++;
                        if (!valid_resolutions.empty() && selected_resolution < (int)valid_resolutions.size())
                        {
                            selected_width_prev = valid_resolutions[selected_resolution].width;
                            selected_height_prev = valid_resolutions[selected_resolution].height;
                        }
                    }
                    XClearArea(display, win, 0, 0, 0, 0, True);
                }
                // Check app buttons
                else if (size_app_run > 0)
                {
                    if (mx >= app_down.x && mx <= app_down.x + app_down.w &&
                        my >= app_down.y && my <= app_down.y + app_down.h)
                    {
                        if (selected_app > 0) selected_app--;
                        XClearArea(display, win, 0, 0, 0, 0, True);
                    }
                    else if (mx >= app_up.x && mx <= app_up.x + app_up.w &&
                             my >= app_up.y && my <= app_up.y + app_up.h)
                    {
                        if (selected_app < size_app_run - 1) selected_app++;
                        XClearArea(display, win, 0, 0, 0, 0, True);
                    }
                }
                // Check fullscreen checkbox
                if (allow_full_screen &&
                    mx >= fullscreen_box.x && mx <= fullscreen_box.x + fullscreen_box.w + 100 &&
                    my >= fullscreen_box.y && my <= fullscreen_box.y + fullscreen_box.h)
                {
                    full_screen = !full_screen;
                    XClearArea(display, win, 0, 0, 0, 0, True);
                }
                // Check start button
                if (mx >= start_btn.x && mx <= start_btn.x + start_btn.w &&
                    my >= start_btn.y && my <= start_btn.y + start_btn.h)
                {
                    confirmed = true;
                    running = false;
                }
            }
            else if (event.type == KeyPress)
            {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_Return || key == XK_KP_Enter)
                {
                    confirmed = true;
                    running = false;
                }
                else if (key == XK_Escape)
                {
                    running = false;
                }
            }
            else if (event.type == ClientMessage || event.type == DestroyNotify)
            {
                running = false;
            }
        }
        
        // Apply selections
        if (confirmed)
        {
            int sel_width = valid_resolutions[selected_resolution].width;
            int sel_height = valid_resolutions[selected_resolution].height;
            int pos_x = 0, pos_y = 0;
            
            if (num_monitors > 0 && selected_monitor < num_monitors)
            {
                pos_x = monitors[selected_monitor].x;
                pos_y = monitors[selected_monitor].y;
                
                if (full_screen)
                {
                    sel_width = monitors[selected_monitor].width;
                    sel_height = monitors[selected_monitor].height;
                    mbm::disable_window_border();
                }
            }
            
            mbm::set_window_position(pos_x, pos_y);
            mbm::set_window_size(sel_width, sel_height);
            if (full_screen)
            {
                mbm::set_expected_window_size(sel_width, sel_height);
            }
            
            if (index_app_selected)
            {
                *index_app_selected = selected_app;
            }
        }
        
        // Cleanup
        if (font) XFreeFont(display, font);
        XFreeGC(display, gc);
        XDestroyWindow(display, win);
        if (monitors) XRRFreeMonitors(monitors);
        XCloseDisplay(display);
        
        return confirmed;
    }
    
    bool select_resolution(SCREEN_RESOLUTION* screen_resolution_list, int size_screen_resolution_list, bool allow_full_screen, const bool full_screen_checked)
    {
        return select_app_and_resolution(nullptr, 0, nullptr, screen_resolution_list, size_screen_resolution_list, allow_full_screen, full_screen_checked, 0, 0);
    }

} // namespace mbm

#endif // __linux__
