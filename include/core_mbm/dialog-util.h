/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2026 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef DIALOG_UTIL_H
#define DIALOG_UTIL_H

#include "core-exports.h"

// Platform-level file-open and file-save dialog API.
//
// On macOS (Apple, non-Android) the implementation uses native Cocoa NSOpenPanel /
// NSSavePanel so that:
//   - Custom / unregistered file extensions (e.g. "*.tile") are correctly shown.
//     tinyfiledialogs' osascript path uses UTI matching, which fails for extensions
//     that have no registered UTI.
//   - The dialog appears in front of fullscreen windows (it runs in-process, in the
//     same macOS Space as the application).
//
// On all other desktop platforms (Windows, Linux) the implementation delegates to
// tinyfiledialogs (tinyfd_openFileDialog / tinyfd_saveFileDialog).
//
// On Android both functions return NULL (no dialog support).

namespace dialog_util
{
    // Opens a native file-open dialog.
    //
    // aFilterPatterns  : array of "*.ext" style strings, e.g. {"*.lua", "*.tile"}.
    //                    Pass {"*.*"} (or NULL / count 0) to allow all files.
    // aAllowMultipleSelects : 0 = single selection, 1 = multiple.
    //                    Multiple-selection result paths are joined with '|'.
    // Returns the selected path string (static storage, valid until the next call),
    // or NULL if the user cancelled.
    API_IMPL const char * openFileDialog(
        const char *             aTitle,
        const char *             aDefaultPathAndFile,
        const char * const *     aFilterPatterns,
        int                      aNumOfFilterPatterns,
        int                      aAllowMultipleSelects);

    // Opens a native file-save dialog.
    //
    // Returns the chosen path (static storage, valid until the next call),
    // or NULL if the user cancelled.
    // Note: on macOS, NSSavePanel automatically enforces the first allowed extension.
    // The extension-append post-processing in the Lua wrappers still works correctly
    // because it checks whether the extension is already present before appending.
    API_IMPL const char * saveFileDialog(
        const char *             aTitle,
        const char *             aDefaultPathAndFile,
        const char * const *     aFilterPatterns,
        int                      aNumOfFilterPatterns);
}

#endif // DIALOG_UTIL_H
