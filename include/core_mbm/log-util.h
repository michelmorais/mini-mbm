/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2016 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef LOG_UTIL_H
#define LOG_UTIL_H

#include "core-exports.h"
#include <string>
#include <vector>


namespace log_util
{
    API_IMPL const char *getDescriptionError(const unsigned int error);
    API_IMPL void replaceString(std::string &source, const std::string &from, const std::string &to);
    API_IMPL void replaceString(std::string &source, const char *from, const char *to);
    API_IMPL void repalceDefaultSeparator(const char *fileNameIn, std::string &fileNameOut);
    API_IMPL const char *basename(const char *fileName);
    API_IMPL void checkGlError(const char *fileName, const int numLine, const char *message);
    API_IMPL void checkGlError(const char *fileName, const int numLine);
    API_IMPL char *formatNewMessage(const size_t length, const char *message, va_list params);
    API_IMPL bool fail(const int lineNum, const char *fileName, const char *format, ...);
    API_IMPL bool onFailed(FILE *fp,const char* fileName, const int numLine, const char *format, ...);
}

#endif
