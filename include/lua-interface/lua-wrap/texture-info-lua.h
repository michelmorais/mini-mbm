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

#ifndef TEXTURE_INFO_LUA_H
#define TEXTURE_INFO_LUA_H

#include <string>
#include <core_mbm/texture-manager.h>

struct lua_State;

namespace mbm
{
    class TEXTURE;
    class TEXTURE_MANAGER;
    
    // Wrapper struct to store texture filename for safe lookup
    // Stores filename instead of raw pointer to handle device loss gracefully
    struct TEXTURE_INFO_DATA
    {
        std::string fileName;
        bool        hasAlpha;
        
        TEXTURE_INFO_DATA(const char *name, bool alpha) 
            : fileName(name ? name : ""), hasAlpha(alpha) {}
        
        inline TEXTURE *getTexture() const
        {
            if (fileName.empty())
                return nullptr;
            TEXTURE_MANAGER *manager = TEXTURE_MANAGER::getInstance();
            if (manager && manager->existTexture(fileName.c_str()))
                return manager->load(fileName.c_str(), hasAlpha);
            return nullptr;
        }
    };
    
    int onNewTextureInfoLua(lua_State *lua, TEXTURE *texture);
    void registerClassTextureInfo(lua_State *lua);
}

#endif
