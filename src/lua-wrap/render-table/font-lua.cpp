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

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include <lua-wrap/render-table/font-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <core_mbm/mesh-manager.h>
#include <render/font.h>
#include <platform/mismatch-platform.h>

#if defined (DEBUG_FREE_LUA) || defined(USE_EDITOR_FEATURES)
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{
    extern int setVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what);
    extern int getVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what);
	extern int lua_error_debug(lua_State *lua, const char *format, ...);

    FONT_DRAW *getFontFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<FONT_DRAW **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_FONT));
        return *ud;
    }

    TEXT_DRAW *getTextDrawFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<TEXT_DRAW **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_TEXT));
        return *ud;
    }

    int onDestroyFontLua(lua_State *lua)
    {
        FONT_DRAW *font = getFontFromRawTable(lua, 1, 1);
        for (unsigned int i = 0; i < font->getTotalText(); ++i)
        {
            TEXT_DRAW *           myText   = font->getText((unsigned char)i);
            auto *userData = static_cast<USER_DATA_RENDER_LUA *>(myText->userData);
            if (userData)
            {
                userData->unrefAllTableLua(lua);
                delete userData;
            }
            myText->userData = nullptr;
        }
    #if DEBUG_FREE_LUA
        const char *fileName = font->getFileName();
        static int  num      = 1;
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n","font", fileName ? fileName : "NULL", num++);
    #endif
        delete font;
        return 0;
    }

    int onGetDimTextDraw(lua_State *lua)
    {
        const int  top  = lua_gettop(lua);
        TEXT_DRAW *draw = getTextDrawFromRawTable(lua, 1, 1);
        float      w    = 0;
        float      h    = 0;
        if (top > 1)
        {
            const int t = lua_type(lua,2);
            if(t == LUA_TBOOLEAN)
            {
                const int b = lua_toboolean(lua, 2);
                if(b)
                    draw->forceCalcSize();
                draw->getWidthHeight(&w, &h);
            }
            else if(t == LUA_TSTRING)
            {
                const char* str = lua_tostring(lua,2);
                draw->getWidthHeightString(&w, &h,str);
            }
        }
        else
        {
            draw->getWidthHeight(&w, &h);
        }
        lua_pushnumber(lua, w);
        lua_pushnumber(lua, h);
        return 2;
    }

    
    int onNewIndexTextDraw(lua_State *lua) // escrita
    {
        /*
        **********************************
                Estado da pilha
                -3|    table |1
                -2|   string |2
                -1|   number |3
        **********************************
        */
        TEXT_DRAW * draw = getTextDrawFromRawTable(lua, 1, 1);
        const char *what = luaL_checkstring(lua, 2);
        const int   len  = strlen(what);
        switch (len)
        {
            case 1:
            {
                switch (what[0])
                {
                    case 'x': draw->position.x = luaL_checknumber(lua, 3); break;
                    case 'y': draw->position.y = luaL_checknumber(lua, 3); break;
                    case 'z': draw->position.z = luaL_checknumber(lua, 3); break;
                    default: { return setVariable(lua, draw, what);
                    }
                }
            }
            break;
            case 2:
            {
                switch (what[0])
                {
                    case 's':
                    {
                        switch (what[1])
                        {
                            case 'x': draw->scale.x = luaL_checknumber(lua, 3); break;
                            case 'y': draw->scale.y = luaL_checknumber(lua, 3); break;
                            case 'z': draw->scale.z = luaL_checknumber(lua, 3); break;
                            default: { return setVariable(lua, draw, what);
                            }
                        }
                    }
                    break;
                    case 'a':
                    {
                        switch (what[1])
                        {
                            case 'x': draw->angle.x = luaL_checknumber(lua, 3); break;
                            case 'y': draw->angle.y = luaL_checknumber(lua, 3); break;
                            case 'z': draw->angle.z = luaL_checknumber(lua, 3); break;
                            default: { return setVariable(lua, draw, what);
                            }
                        }
                    }
                    break;
                    default: { return setVariable(lua, draw, what);
                    }
                }
            }
            break;
            case 4:
            {
                if (strcmp("text", what) == 0)
                {
                    draw->text = luaL_checkstring(lua, 3);
                    draw->forceCalcSize();
                }
                else
                    return setVariable(lua, draw, what);
            }
            break;
            case 5:
            {
                if (strcmp("align", what) == 0)
                {
                    const char* align = luaL_checkstring(lua,3);
                    if(strcmp(align,"center") == 0)
                        draw->aligned = ALIGN_CENTER;
                    else if(strcmp(align,"right") == 0)
                        draw->aligned = ALIGN_RIGHT;
                    else if(strcmp(align,"left") == 0)
                        draw->aligned = ALIGN_LEFT;
                }
                else
                    return setVariable(lua, draw, what);
            }
            break;
            case 7:
            {
                if (strcmp("visible", what) == 0)
                    draw->enableRender = lua_toboolean(lua, 3) ? true : false;
                else
                    return setVariable(lua, draw, what);
            }
            break;
            case 12:
            {
                if (strcmp("alwaysRender", what) == 0)
                    draw->alwaysRenderize = lua_toboolean(lua, 3) ? true : false;
                else
                    return setVariable(lua, draw, what);
            }
            break;
            default: { return setVariable(lua, draw, what);
            }
        }
        return 0;
    }

    int onIndexTextDraw(lua_State *lua) // leitura
    {
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1|   string |2
        **********************************
        */
        TEXT_DRAW * draw = getTextDrawFromRawTable(lua, 1, 1);
        const char *what = luaL_checkstring(lua, 2);
        const int   len  = strlen(what);
        switch (len)
        {
            case 1:
            {
                switch (what[0])
                {
                    case 'x': lua_pushnumber(lua, draw->position.x); break;
                    case 'y': lua_pushnumber(lua, draw->position.y); break;
                    case 'z': lua_pushnumber(lua, draw->position.z); break;
                    default: { return getVariable(lua, draw, what);
                    }
                }
            }
            break;
            case 2:
            {
                switch (what[0])
                {
                    case 's':
                    {
                        switch (what[1])
                        {
                            case 'x': lua_pushnumber(lua, draw->scale.x); break;
                            case 'y': lua_pushnumber(lua, draw->scale.y); break;
                            case 'z': lua_pushnumber(lua, draw->scale.z); break;
                            default: { return getVariable(lua, draw, what);
                            }
                        }
                    }
                    break;
                    case 'a':
                    {
                        switch (what[1])
                        {
                            case 'x': lua_pushnumber(lua, draw->angle.x); break;
                            case 'y': lua_pushnumber(lua, draw->angle.y); break;
                            case 'z': lua_pushnumber(lua, draw->angle.z); break;
                            default: { return getVariable(lua, draw, what);
                            }
                        }
                    }
                    break;
                    default: { return getVariable(lua, draw, what);
                    }
                }
            }
            break;
            case 4:
            {
                if (strcmp("text", what) == 0)
                    lua_pushstring(lua, draw->text.c_str());
                else
                    return getVariable(lua, draw, what);
            }
            break;
            case 5:
            {
                if (strcmp("align", what) == 0)
                {
                    switch(draw->aligned)
                    {
                        case ALIGN_LEFT:
                        {
                            lua_pushstring(lua, "left");
                        }
                        break;
                        case ALIGN_CENTER:
                        {
                            lua_pushstring(lua, "center");
                        }
                        break;
                        case ALIGN_RIGHT:
                        {
                            lua_pushstring(lua, "right");
                        }
                        break;
                    }
                }
                else
                    return getVariable(lua, draw, what);
            }
            break;
            case 7:
            {
                if (strcmp("visible", what) == 0)
                    lua_pushboolean(lua, draw->enableRender);
                else
                    return getVariable(lua, draw, what);
            }
            break;
            case 12:
            {
                if (strcmp("alwaysRender", what) == 0)
                    lua_pushboolean(lua, draw->alwaysRenderize);
                else
                    return getVariable(lua, draw, what);
            }
            break;
            default: { return getVariable(lua, draw, what);
            }
        }
        return 1;
    }

    int onSetWildCard(lua_State *lua)
    {
        TEXT_DRAW *         draw        = getTextDrawFromRawTable(lua, 1, 1);
        const char *        varWildCard = luaL_checkstring(lua, 2);
        const unsigned char index       = varWildCard[0];
        switch (index)
        {
            case 194: // UTF8 - Without BOM
            {
                draw->wildCardChangeAnim = TEXT_DRAW::withoutBOM2Map(varWildCard[1], 194);
            }
            break;
            case 195: // UTF8 - Without BOM
            {
                draw->wildCardChangeAnim = TEXT_DRAW::withoutBOM2Map(varWildCard[1], 195);
            }
            break;
            default: { draw->wildCardChangeAnim = index;}
            break;
        }
        return 0;
    }

    int onGetTotalTextFontLua(lua_State *lua)
    {
        FONT_DRAW *font = getFontFromRawTable(lua, 1, 1);
        lua_pushnumber(lua,static_cast<lua_Number>(font->getTotalText()));
        return 1;
    }


    #ifdef USE_EDITOR_FEATURES
    int onGetTextureFromFontLua(lua_State *lua)
    {
        FONT_DRAW *font = getFontFromRawTable(lua, 1, 1);
        lua_pushstring(lua,font->getFileNameTextureLoaded());
        return 1;
    }

    int onSetLetterYDiff(lua_State *lua)
    {
        FONT_DRAW *font = getFontFromRawTable(lua, 1, 1);
        const char* letter = luaL_checkstring(lua,2);
        const float diffY  = luaL_checknumber(lua,3);
        font->setLetterYDiff(letter,diffY);
        return 0;
    }

    int onGetLetterYDiff(lua_State *lua)
    {
        FONT_DRAW *font = getFontFromRawTable(lua, 1, 1);
        const char* letter = luaL_checkstring(lua,2);
        lua_pushnumber(lua,font->getLetterYDiff(letter));
        return 1;
    }

    int onSetLetterXDiff(lua_State *lua)
    {
        FONT_DRAW *font = getFontFromRawTable(lua, 1, 1);
        const char* letter = luaL_checkstring(lua,2);
        const float diffx  = luaL_checknumber(lua,3);
        font->setLetterXDiff(letter,diffx);
        return 0;
    }

    int onGetLetterXDiff(lua_State *lua)
    {
        FONT_DRAW *font = getFontFromRawTable(lua, 1, 1);
        const char* letter = luaL_checkstring(lua,2);
        lua_pushnumber(lua,font->getLetterXDiff(letter));
        return 1;
    }
    
    int onSetSizeLetter(lua_State *lua)
    {
        FONT_DRAW *font = getFontFromRawTable(lua, 1, 1);
        const char* letter = luaL_checkstring(lua,2);
        const unsigned int  sx  = luaL_checkinteger(lua,3);
        const unsigned int  sy  = luaL_checkinteger(lua,4);
        font->setLetterSize(letter,sx,sy);
        return 0;
    }

    int onGetSizeLetter(lua_State *lua)
    {
        FONT_DRAW *font = getFontFromRawTable(lua, 1, 1);
        const char* letter = luaL_checkstring(lua,2);
        unsigned int  sx = 0;
        unsigned int  sy = 0;
        if(font->getLetterSize(letter,sx,sy))
        {
            lua_pushinteger(lua,sx);
            lua_pushinteger(lua,sy);
        }
        else
        {
            lua_pushinteger(lua,0);
            lua_pushinteger(lua,0);
            ERROR_LOG("failed on get size to letter [%s]", letter);
        }
        return 2;
    }

    #endif
                


    int onAddTextFontLua(lua_State *lua)
    {
        FONT_DRAW * font = getFontFromRawTable(lua, 1, 1);
        const int   top  = lua_gettop(lua);
        const char *text = luaL_checkstring(lua, 2);
        TEXT_DRAW * draw = nullptr;
        if (top > 2)
        {
            for (int i = 3; i <= top; ++i)
            {
                switch (i)
                {
                    case 3: // is2d
                    {
                        bool is2dw = false;
                        bool is2ds = false;
                        bool is3d = false;
                        getTypeWordRenderizableLua(lua,i,is2dw,is2ds,is3d);
                        draw              = font->addText(text, is2dw | is2ds, is2ds);
                    }
                    break;
                    case 4: // x
                    {
                        draw->position.x = luaL_checknumber(lua, i);
                    }
                    break;
                    case 5: // y
                    {
                        draw->position.y = luaL_checknumber(lua, i);
                    }
                    break;
                    case 6: // z
                    {
                        draw->position.z = luaL_checknumber(lua, i);
                    }
                    break;
                    default: {
                    }
                    break;
                }
            }
        }
        else
        {
            draw = font->addText(text);
        }
        if (draw)
        {
            lua_settop(lua, 0);
            luaL_Reg regFontMethods[] =    {{"getSizeText",   onGetDimTextDraw}, 
                                            {"setWildCard",   onSetWildCard}, 
                                            {nullptr, nullptr}};

            luaL_Reg regFontReplaceMethods[] = {{"getSize", onGetDimTextDraw}, {nullptr, nullptr}};

            SELF_ADD_COMMON_METHODS selfMethods(regFontMethods);
            const luaL_Reg *             regMethods = selfMethods.get(regFontReplaceMethods);
            // luaL_newlib(lua, regMethods);
            lua_createtable(lua, 0, selfMethods.getSize());
            luaL_setfuncs(lua, regMethods, 0);
            luaL_getmetatable(lua, "_mbmTextDraw");
            lua_setmetatable(lua, -2);

            auto **udata = static_cast<TEXT_DRAW **>(lua_newuserdata(lua, sizeof(FONT_DRAW *)));
            *udata            = draw;
            draw->userData    = new USER_DATA_RENDER_LUA();

            /* trick to ensure that we will receive the expected metatable type expected metatable type. */
            const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_TEXT);
            luaL_getmetatable(lua,__userdata_name);
            lua_setmetatable(lua,-2);
            /* end trick */

            lua_rawseti(lua, -2, 1);
            return 1;
        }
        return 0;
    }

    int onSetSpaceFontLua(lua_State *lua)
    {
        FONT_DRAW * font = getFontFromRawTable(lua, 1, 1);
        MESH_MBM* mesh = font->getMesh();
        if(mesh)
        {
			auto* infoFont = const_cast<INFO_BOUND_FONT*>(mesh->getInfoFont());
			if (infoFont == nullptr)
			{
				lua_pushboolean(lua,0);
				return 1;
			}
            const char* letter      = luaL_checkstring(lua,2);
            const auto v       = (short int)lua_tointeger(lua,3);
            const unsigned int s    = font->getTotalText();
            if(strcasecmp(letter,"x") == 0)
            {
				infoFont->spaceXCharacter = v;
                for (unsigned int i=0; i< s; ++i)
                {
                    TEXT_DRAW* text =  font->getText(static_cast<char>(i));
                    text->spaceXCharacter = v;
                }
                lua_pushboolean(lua,1);
            }
            else if(strcasecmp(letter,"y") == 0)
            {
				infoFont->spaceYCharacter = v;
                for (unsigned int i=0; i< s; ++i)
                {
                    TEXT_DRAW* text =  font->getText(static_cast<char>(i));
                    text->spaceYCharacter = v;
                }
                lua_pushboolean(lua,1);
            }
            else
                lua_pushboolean(lua,0);
        }
        else
            lua_pushboolean(lua,0);
        return 1;
    }

    int onGetSpaceFontLua(lua_State *lua)
    {
        FONT_DRAW * font = getFontFromRawTable(lua, 1, 1);
        MESH_MBM* mesh = font->getMesh();
        if(mesh)
        {
			auto* infoFont = const_cast<INFO_BOUND_FONT*>(mesh->getInfoFont());
			if (infoFont == nullptr)
			{
				lua_pushboolean(lua,0);
				return 1;
			}
            const char* letter  = luaL_checkstring(lua,2);
            if(strcasecmp(letter,"x") == 0)
                lua_pushinteger(lua,infoFont->spaceXCharacter);
            else if(strcasecmp(letter,"y") == 0)
                lua_pushinteger(lua,infoFont->spaceYCharacter);
            else
                lua_pushnil(lua);
        }
        else
            lua_pushnil(lua);
        return 1;
    }


    int onGetHeightFontLua(lua_State *lua)
    {
        FONT_DRAW * font = getFontFromRawTable(lua, 1, 1);
        MESH_MBM* mesh = font->getMesh();
		const INFO_BOUND_FONT* infoFont = (mesh ? mesh->getInfoFont(): nullptr);
		if(infoFont)
            lua_pushinteger(lua,infoFont->heightLetter);
        else
            lua_pushnil(lua);
        return 1;
    }


    int onNewFontLua(lua_State *lua)
    {
        const int    top              = lua_gettop(lua);
        const char * fileName         = luaL_checkstring(lua, 2);
        DEVICE *device                = DEVICE::getInstance();
        auto   font                   = new FONT_DRAW(device->scene);
        const float  heightFont       = top > 2 ? luaL_checknumber(lua, 3) : 50.0f;
        const short  spaceWidth       = top > 3 ? (short)luaL_checkinteger(lua, 4) : (const short)(heightFont*0.1f);
        const short  spaceHeight      = top > 4 ? (short)luaL_checkinteger(lua, 5) : 0;
        const bool   saveTextureAsPng = top > 5 ? (lua_toboolean(lua, 6) ? true : false) : false;
        luaL_Reg     regFontMethods[] = {
            {"add", onAddTextFontLua},
            {"addText", onAddTextFontLua}, 
            {"getTotal", onGetTotalTextFontLua}, 
            {"setSpace", onSetSpaceFontLua}, 
            {"getSpace", onGetSpaceFontLua}, 
            {"getHeight", onGetHeightFontLua},
#ifdef USE_EDITOR_FEATURES
            {"setLetterYDiff",     onSetLetterYDiff}, 
            {"getLetterYDiff",     onGetLetterYDiff}, 
            {"setLetterXDiff",     onSetLetterXDiff}, 
            {"getLetterXDiff",     onGetLetterXDiff}, 
            {"setSizeLetter",      onSetSizeLetter}, 
            {"getSizeLetter",      onGetSizeLetter},
            {"getTexture",         onGetTextureFromFontLua},
#endif
            {nullptr, nullptr}};

        if (!font->loadFont(fileName, heightFont, spaceWidth, spaceHeight,saveTextureAsPng))
        {
            delete font;
            return lua_error_debug(lua, "failed to load %s", fileName);
        }
        lua_settop(lua, 0);

        luaL_newlib(lua, regFontMethods);
        luaL_getmetatable(lua, "_mbmFont");
        lua_setmetatable(lua, -2);

        auto **udata = static_cast<FONT_DRAW **>(lua_newuserdata(lua, sizeof(FONT_DRAW *)));
        *udata            = font;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_FONT);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }

    void registerClassFont(lua_State *lua)
    {
        luaL_Reg regFontMethods[] = {
            {"new", onNewFontLua}, 
            {"__gc", onDestroyFontLua},
            {"__close", onDestroyRenderizable},
            {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmFont");
        luaL_setfuncs(lua, regFontMethods, 0);
        lua_setglobal(lua, "font");

        luaL_Reg regTextDrawMethods[] = {{"__newindex", onNewIndexTextDraw}, {"__index", onIndexTextDraw}, {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmTextDraw");
        luaL_setfuncs(lua, regTextDrawMethods, 0);
        lua_settop(lua,0);
    }
};
