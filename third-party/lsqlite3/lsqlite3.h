#ifndef LSQL3_IMPORTER_H
#define LSQL3_IMPORTER_H

#if defined (__GNUC__) 
    #define LSQL3_IMP_API  __attribute__ ((__visibility__("default")))
#elif defined (WIN32)
    #define LSQL3_IMP_API  __declspec(dllexport)
#endif


#include <lualib.h>
#include <lauxlib.h>
#include <lua.h>


LSQL3_IMP_API int luaopen_lsqlite3 (lua_State * lua);

#endif