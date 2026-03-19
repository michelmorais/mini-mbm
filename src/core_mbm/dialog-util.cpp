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

// Non-Apple implementation of dialog_util: delegates to tinyfiledialogs.
// Compiled on Windows, Linux, and Android.
// On Apple (macOS), dialog-util-macos.mm provides the implementation instead.

#if !defined(__APPLE__) || defined(ANDROID)

#include <core_mbm/dialog-util.h>

#if !defined(ANDROID)
    #include <tinyfiledialogs/tinyfiledialogs.h>
#endif

namespace dialog_util
{
    const char * openFileDialog(
        const char *             aTitle,
        const char *             aDefaultPathAndFile,
        const char * const *     aFilterPatterns,
        int                      aNumOfFilterPatterns,
        int                      aAllowMultipleSelects)
    {
#if defined(ANDROID)
        return nullptr;
#else
        return tinyfd_openFileDialog(aTitle, aDefaultPathAndFile, aNumOfFilterPatterns,
                                     aFilterPatterns, nullptr, aAllowMultipleSelects);
#endif
    }

    const char * saveFileDialog(
        const char *             aTitle,
        const char *             aDefaultPathAndFile,
        const char * const *     aFilterPatterns,
        int                      aNumOfFilterPatterns)
    {
#if defined(ANDROID)
        return nullptr;
#else
        return tinyfd_saveFileDialog(aTitle, aDefaultPathAndFile, aNumOfFilterPatterns,
                                     aFilterPatterns, nullptr);
#endif
    }
}

#endif // !defined(__APPLE__) || defined(ANDROID)
