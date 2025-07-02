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

#ifndef DYNAMIC_VAR_H
#define DYNAMIC_VAR_H

#include <string.h>
#include <string>
#include "core-exports.h"

namespace mbm
{

enum DYNAMIC_TYPE
{
    DYNAMIC_BOOL     = 1,
    DYNAMIC_CHAR     = 2,
    DYNAMIC_INT      = 3,
    DYNAMIC_FLOAT    = 4,
    DYNAMIC_CSTRING  = 5,
    DYNAMIC_SHORT    = 6,
    DYNAMIC_VOID     = 7,
    DYNAMIC_TABLE    = 8, // Table in Lua
    DYNAMIC_FUNCTION = 9,
    DYNAMIC_REF      = 10 // keep ref
};

struct DYNAMIC_VAR
{
    const DYNAMIC_TYPE type;
    void *             var;
    API_IMPL DYNAMIC_VAR(const DYNAMIC_TYPE myType, const void *voidVar);
    API_IMPL virtual ~DYNAMIC_VAR();
    API_IMPL void setVoid(const void *value);
    API_IMPL void setString(const char *value);
    API_IMPL void setFloat(const float value);
    API_IMPL void setInt(const int value);
    API_IMPL void setShort(const int16_t value);
    API_IMPL void setBool(const bool value);
    API_IMPL void setChar(const char value);
    API_IMPL const void *getVoid() const;
    API_IMPL const char *getString() const;
    API_IMPL float       getFloat() const;
    API_IMPL int         getInt() const;
    API_IMPL short       getShort() const;
    API_IMPL bool        getBool() const;
    API_IMPL char        getChar() const;
    };

}

#endif
