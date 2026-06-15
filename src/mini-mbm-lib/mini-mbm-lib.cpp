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

#include <vector>
#include "mini-mbm-lib.h"
#include <util-interface.h>
#include <lua-wrap/manager-lua.h>
#include <device.h>
#include <core_mbm/platform-win32.h>
#include <version/version.h>
#include <core_mbm/usage-help.h>
#include <core_mbm/strings-pt-br.h>
#if defined (WIN32)
#include <defaultThemePlusWindows.h>
#endif


class ARGS
{
public:
    ARGS(const std::vector<std::string> &argv):_argc(static_cast<int>(argv.size())),_argv(nullptr)
    {
        if (_argc > 0)
        {
            _argv = new char *[_argc];
        }
        for (int n = 0; n < _argc; n++)
        {
            const int string_size = static_cast<int>(strlen(argv[n].c_str()));
            _argv[n] = new char[string_size+1];
            _argv[n][string_size] = 0;
            snprintf(_argv[n],string_size +1,"%s",argv[n].c_str());
        }
    }
    ~ARGS()
    {
        if (_argc > 0)
        {
            for (int n = 0; n < _argc; n++)
            {
                delete [] _argv[n];
            }
            delete [] _argv;
        }
    }
    const int get_argc()const  { return _argc;}
    char ** get_argv()const    { return static_cast<char**>(_argv);}
private:
    const int _argc;
    char ** _argv;
};

#if defined (WIN32)
HWND external_hwnd = 0;
DWORD  external_ID_ICON = 0;
bool _my_theme_selected = false;
#else
constexpr int  external_ID_ICON = 0;
#endif
std::string my_app_name("Mini-Mbm");
std::vector<std::string> my_args;

OnDoNativeCommand externalDoNativeCommand = nullptr;

namespace mbm
{

    void push_arg(const char * name,const char * value)
    {
        std::string var_name(name ? name : "");
        std::string var_value(value ? value : "");
        my_args.emplace_back(var_name);
        my_args.emplace_back(var_value);
    }

    void set_string_to_execute(const char * string_lua)
    {
        if(string_lua)
        {
            my_args.emplace_back("-execute");
            my_args.emplace_back(string_lua);
        }
        else
        {
            ERROR_LOG("parameter string_lua can not be empty");
        }
    }

    void add_path(const char * path)
    {
        if(path)
        {
            util::addPath(path);
        }
        else
        {
            ERROR_LOG("parameter path can not be empty");
        }
    }

    void set_window_size(const int width,const int height)
    {
        if(width > 0)
        {
            my_args.emplace_back("-w");
            my_args.emplace_back(std::to_string(width));
        }
        else
        {
            ERROR_LOG("parameter width can not be < 0");
        }
        if(height > 0)
        {
            my_args.emplace_back("-h");
            my_args.emplace_back(std::to_string(height));
        }
        else
        {
            ERROR_LOG("parameter height can not be < 0");
        }
    }

    void set_window_resizable(const bool value)
    {
        my_args.emplace_back("-enableResizeWindow");
        if(value)
            my_args.emplace_back("1");
        else
            my_args.emplace_back("0");
    }

    void set_expected_window_size(const int expected_width,const int expected_height)
    {
        if(expected_width > 0)
        {
            my_args.emplace_back("-ew");
            my_args.emplace_back(std::to_string(expected_width));
        }
        else
        {
            ERROR_LOG("parameter expected_width can not be < 0");
        }
        if(expected_height > 0)
        {
            my_args.emplace_back("-eh");
            my_args.emplace_back(std::to_string(expected_height));
        }
        else
        {
            ERROR_LOG("parameter expected_height can not be < 0");
        }
    }

    void set_window_position(const int x,const int y)
    {
        my_args.emplace_back("-x");
        my_args.emplace_back(std::to_string(x));
        my_args.emplace_back("-y");
        my_args.emplace_back(std::to_string(y));
    }

    void set_window_maximized(const bool value)
    {
        my_args.emplace_back("-maximizedwindow");
        if(value)
            my_args.emplace_back("1");
        else
            my_args.emplace_back("0");
    }

    void set_scene(const char * scene_name_lua)
    {
        my_args.emplace_back("-scene");
        my_args.emplace_back(scene_name_lua);
    }


    void set_app_name(const char * app_name)
    {
        my_args.emplace_back("-name");
        my_args.emplace_back(app_name);
        my_app_name = app_name? app_name : "Mini-mbm";
    }

    void set_verbose(const bool value)
    {
        if(value)
            my_args.emplace_back("-verbose");
        else
            my_args.emplace_back("-notverbose");
    }

    void disable_splash()
    {
        my_args.emplace_back("-nosplash");
    }

    void disable_window_border()
    {
        my_args.emplace_back("-noborder");
    }

    void set_callback_do_commands(OnDoNativeCommand onDoNativeCommand)
    {
        externalDoNativeCommand = onDoNativeCommand;
    }

    #if defined (WIN32)
    void set_hwnd(HWND hwnd)
    {
        log_util::print_colored(COLOR_TERMINAL_RED,"set_hwnd not implemented yet :/");
        external_hwnd = hwnd;
    }

    void set_window_theme(const int id,const bool enable_border)
    {
        if(id < THEME_WINPLUS_CUSTOM_RENDER::getTotalThemes() && id >= 0)
        {
            mbm::setTheme(id, enable_border);
            if(enable_border == false)
                my_args.emplace_back("-noborder");
            _my_theme_selected = true;
        }
        else
        {
            log_util::print_colored(COLOR_TERMINAL_RED,"Invalid theme selected: %d/%d",id,THEME_WINPLUS_CUSTOM_RENDER::getTotalThemes());
        }
    }

    void set_icon(DWORD ID_ICON)
    {
        external_ID_ICON = ID_ICON;
    }
    #endif

    inline int  start_main_loop(const std::vector<std::string> & args, const int ID_ICON)
    {
        if(args.size() <= 1 ||  (args.size() > 1 && args[1].find("help") != std::string::npos))
        {
            usage::help(util::getBaseName(args[0].c_str()));
        }
        #if defined (WIN32)
        if(_my_theme_selected == false)
            mbm::setTheme(22, true);
        #endif
        mbm::LUA_MANAGER luaCore(args);
        DEVICE *device = luaCore.getDevice();
        if(device && device->isVerbose())
            log_util::print_colored(COLOR_TERMINAL_YELLOW,"For documentation please check at:\n%s\n","https://mbm-documentation.readthedocs.io/en/latest/");
    
        luaCore.onDoNativeCommand = externalDoNativeCommand;
#if (defined (__MINGW32__) || defined (__CYGWIN__) || defined(_WIN32))
        setWin32IconToBeUsed(ID_ICON);
        DisableProcessWindowsGhosting();
#endif
        int expectedWidth = luaCore.widthWindow;
        int expectedHeight = luaCore.heightWindow;
        std::string stretch("y");
        luaCore.getExpectedSizeOfWindow(expectedWidth, expectedHeight, stretch);
        if (luaCore.initializeSceneLua(luaCore.widthWindow, luaCore.heightWindow, expectedWidth, expectedHeight,luaCore.windowBorder))
        {
            // TODO: review if these options are still necessary
            //device->getSpecificContextDevice()->window.askOnExit = false;
            //device->window.exitOnEsc = false;
    
    #if !defined(_DEBUG) && (defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32))
            bool hideConsole = true;
            for (const auto & arg : args)
            {
                if (arg.find("--showconsole") != std::string::npos)
                {
                    hideConsole = false;
                    break;
                }
            }
            if(hideConsole)
                mbm::hideConsoleWindow();
#endif
            const int ret = luaCore.run();
            const int code_quit = device->getAppReturnCode();
            return code_quit ? code_quit : ret;
        }
        else
        {
            ERROR_LOG("Failed to load Mini Mbm %s engine backend [%s]", MBM_VERSION, device ? device->getBackendEngineName() : "unknown");
            //fprintf(stderr, "\nMini-Mbm-OpenGLES is necessary to have the following DLLs:");
            //fprintf(stderr, "\nlibEGL.dll, libGLESv2.dll and d3dcompiler_47.dll");
            //fprintf(stderr, "\nfound in mini-mbm/third-party/gles/bin");
            std::getchar();
            return -1;
        }
    }


    int onLoop()
    {
     #if defined (WIN32)
        char    myExe[MAX_PATH] = "";
        HMODULE HMod = GetModuleHandle(nullptr);
        if(GetModuleFileNameA(HMod, myExe, sizeof(myExe)))
        {
            my_args.insert(my_args.begin(),myExe);
			auto p = std::string(myExe).find_last_of("\\/");
			if (p != std::string::npos)
			{
				std::string path = std::string(myExe).substr(0, p);
				my_args.push_back("--addPath");
                my_args.push_back(path);
			}
        }
        else
        {
            my_args.insert(my_args.begin(),"mini_mbm.exe");
        }
        
#if _DEBUG
        my_args.push_back("--addPath");
        my_args.push_back("C:\\Users\\miche\\Documents\\mini-mbm\\editor");
#endif
    #else
        my_args.insert(my_args.begin(),"mini_mbm");
        push_arg("--name",my_app_name.c_str());
    #endif
        return start_main_loop(my_args,external_ID_ICON);
    }


    int  forward_args_and_do_loop(const int argc,const char **argv,const int ID_ICON)
    {
        for(int i=0; i < argc; ++i)
        {
            my_args.emplace_back(argv[i]);
        }
        return start_main_loop(my_args,ID_ICON);
    }

    bool create_temp_folder(const char * folder_name,char* folder_name_output,const int size_folder_name_output)
    {
        return util::create_tmp_directoy(folder_name,folder_name_output,size_folder_name_output);
    }

    void remove_folder(const char * folder_name)
    {
        util::remove_directory(folder_name);
    }

    bool folder_exists(const char * tmpFolder)
    {
        return util::directoy_exists(tmpFolder);
    }

    void replace_string(char *source_in_out,const int size_of_source, const char *from, const char *to)
    {
        const std::string _from(from);
        const std::string _to(to);
        std::string source(source_in_out ? source_in_out : "");
        log_util::replaceString(source, _from, _to);
        snprintf(source_in_out,size_of_source,"%s",source.c_str());
        const int len_dest  =  source_in_out ? static_cast<int>(strlen(source_in_out)) : 0;
        const int len_result = static_cast<int>(source.length());
        if(source_in_out && len_dest != len_result)
        {
            ERROR_LOG("'replace_string' received a too short string to store the result");
            source_in_out[0]=0;
        }
    }

#if defined _DEBUG
    void restoreDeviceTest()
    {
        log_util::print_colored(COLOR_TERMINAL_YELLOW, "restoreDeviceTest called");
        mbm::DEVICE::getInstance()->refreshDevice();
    }
#endif

}
