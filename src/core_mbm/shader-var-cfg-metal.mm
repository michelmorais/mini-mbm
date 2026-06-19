/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.                                    |
|-----------------------------------------------------------------------------------------------------------------------*/

// Metal implementation of VAR_SHADER constructor/destructor.
// Mirrors shader-var-cfg-opengl_es.cpp (which is guarded by USE_OPENGL_ES).

#if defined(USE_METAL)

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
            case VAR_FLOAT:      this->sizeVar = 1; break;
            case VAR_INT:        this->sizeVar = 1; break;
            case VAR_VECTOR:     this->sizeVar = 3; break;
            case VAR_VECTOR2:    this->sizeVar = 2; break;
            case VAR_COLOR_RGB:  this->sizeVar = 3; break;
            case VAR_COLOR_RGBA: this->sizeVar = 4; break;
            default:             this->sizeVar = 0; break;
        }
        ptrHandleVar = new int32_t(-1);
    }

    VAR_SHADER::~VAR_SHADER()
    {
        delete static_cast<int32_t*>(ptrHandleVar);
    }

} // namespace mbm

#endif // USE_METAL
