

#ifndef  OBJ_IMPORTER_LUA_H

#if defined (__GNUC__) 
  #define OBJ_IMP_API  __attribute__ ((__visibility__("default")))
#elif defined (WIN32)
  #ifdef OBJ_IMP_BUILD_DLL
    #define OBJ_IMP_API  __declspec(dllexport)
  #else
    #define OBJ_IMP_API   __declspec(dllimport)
  #endif
#endif


extern "C"
{
    #include <lualib.h>
    #include <lauxlib.h>
    #include <lua.h>
}

//name of this function is not flexible
extern "C" OBJ_IMP_API int luaopen_tiny_obj_loader (lua_State * lua);
//sometimes it is followed by "lib" -> "lib"tiny_obj_loader
extern "C" OBJ_IMP_API int luaopen_libtiny_obj_loader (lua_State *lua);


#endif // ! OBJ_IMPORTER_LUA_H
