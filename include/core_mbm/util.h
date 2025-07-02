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

#ifndef UTIL_H_
#define UTIL_H_

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include <platform/mismatch-platform.h>

namespace util
{

    extern const char *getPathFromFullPathName(const char *fileNamePath);
    #if defined   _WIN32
    API_IMPL WCHAR *toWchar(const char *str, WCHAR *outText);
    API_IMPL char *toChar(const WCHAR *wstr, char *outText);
    #endif

    API_IMPL float degreeToRadian(const float degree);
    API_IMPL float radianToDegree(const float radian);
    extern void setRandomSeed();
    extern float getHeightMaxWithInitialSpeed(const float gravity, const float speedInitial) noexcept;
    extern float getHeightWithTime(const float gravity, const float time) noexcept;
    extern float getTimeWithMaxHeight(const float gravity, const float heigth) noexcept;
    extern float getSpeedWithTimeFall(const float gravity, const float time) noexcept;
    extern float getSpeedWithHeight(const float gravity, const float heigth) noexcept;

    API_IMPL int getRandomInt(const int min, const int max) noexcept;
    API_IMPL char getRandomChar(const char min, const char max) noexcept;
    API_IMPL float getRandomFloat(const float min, const float max) noexcept;
    extern uint32_t FloatToDWORD(float &Float) noexcept;
    API_IMPL const char* getBaseName(const char *fileName);
    extern float getByteProp(); // 1 / 255
	API_IMPL void base_64_decode(const std::string & str_encoded, std::string & result);
    API_IMPL void getAABB(const float halfDimInOut[2], const float angleRadian, float *widthOut, float *heightOut) noexcept;
    API_IMPL void split(std::vector<std::string> &result, const char *in, const char delim);
}

#endif
