/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef MISMATCH_PLATFORM_H
#define MISMATCH_PLATFORM_H

    #if defined _WIN32 
        #include <string.h>
        #include <Windows.h>
        #ifndef M_PI
            #define M_PI 3.14159265358979323846f
        #endif

        #ifndef M_PI_2
            #define M_PI_2 (M_PI/2.0f)
        #endif

        #ifndef strcasecmp
            #define strcasecmp strcmpi
        #endif

        #pragma warning(disable : 4996) // access strcmpi

        #ifndef NOMINMAX
            #define NOMINMAX
        #endif

        #define access_file access

    #elif defined ANDROID
        //do nothing

    #elif defined(__linux__)
        #include <unistd.h>
        #define access_file access
    #else
        #error "unknown platform"
    #endif
#endif
