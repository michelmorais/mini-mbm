/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

// iOS stub for mini-mbm-lib: replaces the macOS Cocoa launcher dialog with
// no-op implementations.  The real script / resolution setup happens in
// MetalViewController (platform-ios/) where the bundle path is hard-coded.

#if defined(MBM_PLATFORM_IOS)

#include "mini-mbm-lib.h"

namespace mbm
{
    // On iOS there is no resolution-picker dialog.
    // The caller (MetalViewController) sets the window size directly via
    // set_window_size() / set_expected_window_size() before calling the engine.
    bool select_app_and_resolution(APP_RUN*          /*app_run*/,
                                   int               /*size_app_run*/,
                                   int*              /*index_app_selected*/,
                                   SCREEN_RESOLUTION* /*screen_resolution_list*/,
                                   int               /*size_screen_resolution_list*/,
                                   bool              /*allow_full_screen*/,
                                   const bool        /*full_screen_checked*/,
                                   int               /*requested_width*/,
                                   int               /*requested_height*/)
    {
        // Not supported on iOS — return false so callers skip the dialog path.
        return false;
    }

    bool select_resolution(SCREEN_RESOLUTION* /*screen_resolution_list*/,
                           int               /*size_screen_resolution_list*/,
                           bool              /*allow_full_screen*/,
                           const bool        /*full_screen_checked*/)
    {
        return false;
    }

} // namespace mbm

#endif // MBM_PLATFORM_IOS
