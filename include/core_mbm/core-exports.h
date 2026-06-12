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

#ifndef CORE_EXPORTS_H
#define CORE_EXPORTS_H

#if defined _WIN32 || defined __CYGWIN__
  #ifdef CORE_EXPORTS
    #ifdef __GNUC__
      #define API_IMPL __attribute__ ((dllexport))
    #else
      #define API_IMPL __declspec(dllexport)
    #endif
  #else
    #ifdef __GNUC__
      #define API_IMPL __attribute__ ((dllimport))
    #else
      #define API_IMPL __declspec(dllimport)
    #endif
  #endif
#else
  #if __GNUC__ >= 4
    #define API_IMPL __attribute__ ((visibility ("default")))
  #else
    #define API_IMPL
  #endif
#endif

/* API_IMPL_OVERRIDE: use for virtual methods that override a secondary base
 * class in a multiply-inherited hierarchy (e.g. JOYSTICK_BASE methods inside
 * CORE_MANAGER : public EVENTS, public JOYSTICK_BASE).
 * GCC/MinGW consumers must not mark these dllimport because the compiler
 * cannot generate non-virtual thunks for dllimport'd functions, which causes
 * "undefined reference to non-virtual thunk" link errors when a user-side
 * class (GAME) inherits from CORE_MANAGER.  All calls go through the vtable
 * anyway, so dllimport is not needed on the consumer side. */
#if defined _WIN32 || defined __CYGWIN__
  #ifdef CORE_EXPORTS
    #define API_IMPL_OVERRIDE API_IMPL   /* dllexport when building DLL */
  #elif defined __GNUC__
    #define API_IMPL_OVERRIDE            /* no dllimport for GCC consumers */
  #else
    #define API_IMPL_OVERRIDE __declspec(dllimport)  /* MSVC is fine */
  #endif
#else
  #define API_IMPL_OVERRIDE API_IMPL
#endif

#if defined(_MSC_VER)
  #define MBM_MSVC_DISABLE_DLL_INTERFACE_WARNING_BEGIN __pragma(warning(push)) __pragma(warning(disable : 4251))
  #define MBM_MSVC_DISABLE_DLL_INTERFACE_WARNING_END __pragma(warning(pop))
#else
  #define MBM_MSVC_DISABLE_DLL_INTERFACE_WARNING_BEGIN
  #define MBM_MSVC_DISABLE_DLL_INTERFACE_WARNING_END
#endif

#endif
