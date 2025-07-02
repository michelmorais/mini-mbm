/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT);                                                                                                     |
| Copyright (C); 2020      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software");, to deal in the Software without restriction, including without limitation       |
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

#ifndef IMGUI_IMPORTER_H

#define IMGUI_IMPORTER_H

#if defined (__GNUC__) 
  #define IMGUI_IMP_API  __attribute__ ((__visibility__("default")))
#elif defined (WIN32)
    //assuming that we are using the version of LUA 5.4
    #pragma comment(lib, "lua5.4.lib") 
    // To build the DLL on Windows define this IMGUI_IMP_BUILD_DLL, to use the DLL do not define
  #ifdef IMGUI_IMP_BUILD_DLL
    #define IMGUI_IMP_API  __declspec(dllexport)
  #else
    #define IMGUI_IMP_API   __declspec(dllimport)
  #endif
#endif

//You might disable it to do not implement the plugin callback (main purpose of use in the mbm engine)
#ifndef PLUGIN_CALLBACK
  #define PLUGIN_CALLBACK
#endif

extern "C"
{
    #include <lualib.h>
    #include <lauxlib.h>
    #include <lua.h>
}

// Note that the name of this function is not flexible
extern "C" IMGUI_IMP_API int luaopen_ImGui (lua_State * lua);
//sometimes it is followed by "lib" -> "lib"ImGui
extern "C" IMGUI_IMP_API int luaopen_libImGui (lua_State *lua);


#endif // ! IMGUI_IMPORTER_H

