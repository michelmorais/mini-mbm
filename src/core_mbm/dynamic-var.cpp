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

#include "dynamic-var.h"

namespace mbm
{
    DYNAMIC_VAR::DYNAMIC_VAR(const DYNAMIC_TYPE myType, const void *voidVar) : type(myType)
    {
        switch (myType)
        {
            case DYNAMIC_BOOL:
            {
                var                 = new bool;
                auto *      v       = static_cast<bool *>(var);
                const auto *value   = static_cast<const bool *>(voidVar);
                *v                  = *value;
            }
            break;
            case DYNAMIC_CHAR:
            {
                var                 = new char;
                auto *v             = static_cast<char *>(var);
                const auto *value   = static_cast<const char *>(voidVar);
                *v                  = *value;
            }
            break;
            case DYNAMIC_INT:
            {
                var                 = new int;
                auto *v             = static_cast<int *>(var);
                const auto *value   = static_cast<const int *>(voidVar);
                *v                  = *value;
            }
            break;
            case DYNAMIC_FLOAT:
            {
                var                = new float;
                auto *v            = static_cast<float *>(var);
                const auto *value  = static_cast<const float *>(voidVar);
                *v                 = *value;
            }
            break;
            case DYNAMIC_CSTRING:
            {
                var                         = new std::string;
                auto *v                     = static_cast<std::string *>(var);
                const auto *stringValue     = static_cast<const char *>(voidVar);
                *v                          = stringValue;
            }
            break;
            case DYNAMIC_SHORT:
            {
                var                = new short;
                auto *v            = static_cast<short *>(var);
                const auto *value  = static_cast<const short *>(voidVar);
                *v                 = *value;
            }
            break;
            case DYNAMIC_FUNCTION:
            case DYNAMIC_VOID:
            {
                var                 = new int;
                auto *v             = static_cast<int *>(var);
                const auto *value   = static_cast<const int *>(voidVar);
                *v                  = *value;
            }
            break;
            case DYNAMIC_TABLE:
            {
                var                 = new int;
                auto *v             = static_cast<int *>(var);
                const auto *value   = static_cast<const int *>(voidVar);
                *v                  = *value;
            }
            break;
            case DYNAMIC_REF:
            {
                var                 = const_cast<void*>(voidVar);
            }
            break;
            default: { var = nullptr;}
            break;
        }
    }
    DYNAMIC_VAR::~DYNAMIC_VAR()
    {
        if (var)
        {
            switch (type)
            {
                case DYNAMIC_BOOL:
                {
                    auto *v = static_cast<bool *>(this->var);
                    delete v;
                }
                break;
                case DYNAMIC_CHAR:
                {
                    auto *v = static_cast<char *>(this->var);
                    delete v;
                }
                break;
                case DYNAMIC_INT:
                {
                    auto *v = static_cast<int *>(this->var);
                    delete v;
                }
                break;
                case DYNAMIC_FLOAT:
                {
                    auto *v = static_cast<float *>(this->var);
                    delete v;
                }
                break;
                case DYNAMIC_CSTRING:
                {
                    auto *v = static_cast<std::string *>(this->var);
                    delete v;
                }
                break;
                case DYNAMIC_SHORT:
                {
                    auto *v = static_cast<short *>(this->var);
                    delete v;
                }
                break;
                case DYNAMIC_VOID:
                case DYNAMIC_FUNCTION:
                {
                    auto *v = static_cast<int *>(this->var);
                    delete v;
                }
                break;
                case DYNAMIC_TABLE: // Lua
                {
                    auto *v = static_cast<int *>(this->var);
                    delete v;
                }
                break;
                default: { var = nullptr;}
                break;
            }
        }
    }
    void DYNAMIC_VAR::setVoid(const void *value)
    {
        if (var)
        {
            auto *v      = static_cast<int *>(var);
            const auto *vvalue = static_cast<const int *>(value);
            *v          = *vvalue;
        }
    }
    void DYNAMIC_VAR::setString(const char *value)
    {
        if (var)
        {
            auto *v = static_cast<std::string *>(var);
            if (value)
                *v = value;
            else
                v->clear();
        }
    }
    void DYNAMIC_VAR::setFloat(const float value)
    {
        if (var)
        {
            auto *v = static_cast<float *>(var);
            *v       = value;
        }
    }
    void DYNAMIC_VAR::setInt(const int value)
    {
        if (var)
        {
            auto *v = static_cast<int *>(var);
            *v     = value;
        }
    }
    void DYNAMIC_VAR::setShort(const int16_t value)
    {
        if (var)
        {
            auto *v = static_cast<int16_t *>(var);
            *v     = value;
        }
    }
    void DYNAMIC_VAR::setBool(const bool value)
    {
        if (var)
        {
            auto *v = static_cast<bool *>(var);
            *v      = value;
        }
    }
    void DYNAMIC_VAR::setChar(const char value)
    {
        if (var)
        {
            auto *v = static_cast<char *>(var);
            *v      = value;
        }
    }
    const void * DYNAMIC_VAR::getVoid() const
    {
        return var;
    }
    const char * DYNAMIC_VAR::getString() const
    {
        const char *ret = nullptr;
        if (var)
        {
            auto *v = static_cast<std::string *>(var);
            ret            = v->c_str();
            return ret;
        }
        return ret;
    }
    float DYNAMIC_VAR::getFloat() const
    {
        if (var)
        {
            auto *v = static_cast<float *>(var);
            return *v;
        }
        return 0.0f;
    }
    int DYNAMIC_VAR::getInt() const
    {
        if (var)
        {
            auto *v = static_cast<int *>(var);
            return *v;
        }
        return 0;
    }
    short DYNAMIC_VAR::getShort() const
    {
        if (var)
        {
            auto *v = static_cast<short *>(var);
            return *v;
        }
        return 0;
    }
    bool DYNAMIC_VAR::getBool() const
    {
        if (var)
        {
            auto *v = static_cast<bool *>(var);
            return *v;
        }
        return false;
    }
    char DYNAMIC_VAR::getChar() const
    {
        if (var)
        {
            auto *v = static_cast<char *>(var);
            return *v;
        }
        return 0;
    }
}
