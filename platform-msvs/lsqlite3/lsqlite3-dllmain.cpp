// lsqlite3-dllmain.cpp : Defines the entry point for the DLL application.
#include <Windows.h>

#pragma comment(lib, "lua5.4.lib")
#pragma comment(lib, "mini-mbm.lib")


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

