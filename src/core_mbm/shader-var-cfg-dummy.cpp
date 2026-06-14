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


#if defined (USE_DUMMY_BACK_END_ENGINE)

#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions

#include <shader-var-cfg.h>
#include <cstring>

namespace mbm
{

    VAR_SHADER::VAR_SHADER(const std::string Name, const TYPE_VAR_SHADER TypeVar, const bool isPS) noexcept :
        name(Name),
        typeVar(TypeVar),
        isPS(isPS)
    {
        memset(current, 0, sizeof(current));
        memset(this->min, 0, sizeof(min));
        memset(this->max, 0, sizeof(max));
        memset(this->step, 0, sizeof(step));
        memset(this->control, 1, sizeof(control));
        memset(this->granThen, 0, sizeof(granThen));
        switch (typeVar)
        {
            case VAR_FLOAT: this->sizeVar      = 1; break;
            case VAR_VECTOR: this->sizeVar     = 3; break;
            case VAR_VECTOR2: this->sizeVar    = 2; break;
            case VAR_COLOR_RGB: this->sizeVar  = 3; break;
            case VAR_COLOR_RGBA: this->sizeVar = 4; break;
            default: { this->sizeVar = 0;}
            break;
        }

        ptrHandleVar = nullptr; // to be implemented in the specific back-end engine
        REMINDER_TODO
    }

    VAR_SHADER::~VAR_SHADER()
    {
        //delete static_cast<YOUR_TYPE*>(ptrHandleVar);
        REMINDER_TODO
    }

}

#endif //USE_DUMMY_BACK_END_ENGINE