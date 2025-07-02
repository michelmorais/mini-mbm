/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT);                                                                                                     |
| Copyright (C); 2021      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
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

#ifndef LIB_MINIMBM_LUA_HEADER_H

#define LIB_MINIMBM_LUA_HEADER_H

#if defined (__GNUC__) 
  #define LIB_IMP_API  __attribute__ ((__visibility__("default")))
#elif defined (WIN32)
#include <Windows.h>
  #ifdef LIB_MBM_EXPORTS
    #define LIB_IMP_API  __declspec(dllexport)
  #else
    #define LIB_IMP_API   __declspec(dllimport)
  #endif
#endif


#ifndef _DO_NATIVE_COMMANDS_FROM_LUA
#define _DO_NATIVE_COMMANDS_FROM_LUA
typedef void (*OnDoNativeCommand)(const char *command, const char *param,char * result,const int max_size_result);
#endif

extern "C"
{
    namespace mbm
    {

        struct SCREEN_RESOLUTION
        {
            int width;
            int height;
            const char* description; // "HD", "Full HD", "Recomended for this game", etc
        };
        /*
         default:
         static SCREEN_RESOLUTION default_resolutions [] = {
            {640,     360,   "Low resolution"},
            {800,     600,   "XVGA"},
            {960,     540,   "qHD"},
            {1024,    768,   "qHD"},
            {1280,    720,   "Standard High Density (HD)"},
            {1280,    736,   "HD"},
            {1280,    768,   "WXGA"},
            {1280,    800,   "WXGA"},
            {1600,    900,   "HD"},
            {1920,    1080,  "Standard Full HD Display"},
            {2560,    1440,  "Standard Quad HD Display"},
            {3200,    1800,  "QHD"},
            {3840,    2160,  "Standard Ultra HD Display"},
            {5120,    2880,  "5K"},
            {7680,    4320,  "8K UHD"},
            {15360,   8640,  "16K"}};
        */

        // This is the main entry point for the engine. You probably will not use it since you could add your own args
        LIB_IMP_API int  forward_args_and_do_loop(const int argc,const char **argv,const int ID_ICON = 0);


        //The following methods will help you to set up your game. When it is done call loop.
        LIB_IMP_API void push_arg(const char * name,const char * value);// e.g ("-scene" , "main.lua")  , ("width", "1024"), (variable="blaBla"), etc...
        LIB_IMP_API void set_window_size(const int width,const int height);
        LIB_IMP_API void set_expected_window_size(const int expected_width,const int expected_height);
        LIB_IMP_API void set_window_resizable(const bool value);
        LIB_IMP_API void set_window_position(const int x,const int y);
        LIB_IMP_API void add_path(const char * path);// as many paths as needed
        LIB_IMP_API void set_scene(const char * scene_name_lua);//usually do not need! the first scene should be "main.lua"
        LIB_IMP_API void set_string_to_execute(const char * string_lua);//string instead of file for "main.lua"
        LIB_IMP_API void set_app_name(const char * app_name);//The aplication name (on top of window)
        LIB_IMP_API void set_verbose(const bool value);//console verbose in case some errors.
        LIB_IMP_API void disable_splash();//Disable the splash "MBM" logo
        LIB_IMP_API void disable_window_border();// window without border, usually full screen
        LIB_IMP_API void set_callback_do_commands(OnDoNativeCommand onDoNativeCommand);//e.g. from lua script: mbm.doCommands('command','parameter') and this callback will be called.

        //some facility
        LIB_IMP_API bool folder_exists(const char* folder_name);//check if the folder exists
        LIB_IMP_API bool create_temp_folder(const char * folder_name,char* folder_name_output,const int size_folder_name_output);//folder_name can be null (generated), '.' (for local) or a base name (no separator) to be created in the /tmp folder
        LIB_IMP_API void remove_folder(const char * folder_name);//after you create a tmp folder, dump files there and stop the application yous should remove them
        LIB_IMP_API void replace_string(char *source_in_out,const int size_of_source, const char *from, const char *to);// simple implementation of replace string in place

        //The following methods is meant for Windows only, for now.
        #if defined (WIN32)
        LIB_IMP_API void set_hwnd(HWND hwnd);//not implemented yet
        LIB_IMP_API HWND get_hwnd();// retrieve the HWND created for this application
        LIB_IMP_API void set_window_theme(const int id,const bool enable_border);//id range from 0 to 25. try some! is the border color arrangement.
        LIB_IMP_API void set_icon(DWORD ID_ICON);
        LIB_IMP_API bool select_resolution(SCREEN_RESOLUTION* screen_resolution_list = nullptr, int size_screen_resolution_list = 0,bool allow_full_screen = true,const bool full_screen_checked = true);// with no args provide the most common resolution, you can set your own resolution list
        #endif


        //when all args are set, call loop
        LIB_IMP_API int  loop();
    }
}

#endif // ! LIB_MINIMBM_LUA_HEADER_H

