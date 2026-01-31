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

#include <string>
#include <vector>

#include <platform/mismatch-platform.h>

namespace util
{

    API_IMPL const char *getPathFromFullPathName(const char *fileNamePath);
    #if (defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32))
    API_IMPL WCHAR *toWchar(const char *str, WCHAR *outText);
    API_IMPL char *toChar(const WCHAR *wstr, char *outText);
    #endif

    API_IMPL float degreeToRadian(const float degree);
    API_IMPL float radianToDegree(const float radian);
    API_IMPL void setRandomSeed();
    API_IMPL float getHeightMaxWithInitialSpeed(const float gravity, const float speedInitial) noexcept;
    API_IMPL float getHeightWithTime(const float gravity, const float time) noexcept;
    API_IMPL float getTimeWithMaxHeight(const float gravity, const float heigth) noexcept;
    API_IMPL float getSpeedWithTimeFall(const float gravity, const float time) noexcept;
    API_IMPL float getSpeedWithHeight(const float gravity, const float heigth) noexcept;

    API_IMPL int getRandomInt(const int min, const int max) noexcept;
    API_IMPL char getRandomChar(const char min, const char max) noexcept;
    API_IMPL float getRandomFloat(const float min, const float max) noexcept;
    API_IMPL uint32_t FloatToDWORD(float &Float) noexcept;
    API_IMPL const char* getBaseName(const char *fileName);
    API_IMPL float getByteProp(); // 1 / 255
	API_IMPL void base_64_decode(const std::string & str_encoded, std::string & result);
    API_IMPL void getAABB(const float halfDimInOut[2], const float angleRadian, float *widthOut, float *heightOut) noexcept;
    API_IMPL void split(std::vector<std::string> &result, const char *in, const char delim);
}

#endif
