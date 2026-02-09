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
#include <version/version.h>
#include <core_mbm/usage-help.h>
#if defined (WIN32)
#include <defaultThemePlusWindows.h>
#elif defined (__linux__) || defined(__APPLE__)
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include <cstring>
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
            const int string_size = strlen(argv[n].c_str());
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
        if(luaCore.device && luaCore.device->verbose)
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
            //luaCore.device->specificContextDevice->window.askOnExit = false;
            //luaCore.device->window.exitOnEsc = false;
    
    #ifndef _DEBUG 
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
            const int code_quit = luaCore.device->getAppReturnCode();
            return code_quit ? code_quit : ret;
        }
        else
        {
            ERROR_LOG("Failed to load Mini Mbm %s engine backend [%s]", MBM_VERSION, luaCore.device->getBackendEngineName());
            //fprintf(stderr, "\nMini-Mbm-OpenGLES is necessary to have the following DLLs:");
            //fprintf(stderr, "\nlibEGL.dll, libGLESv2.dll and d3dcompiler_47.dll");
            //fprintf(stderr, "\nfound in mini-mbm/third-party/gles/bin");
            std::getchar();
            return -1;
        }
    }


    int loop()
    {
     #if defined (WIN32)
        char    myExe[MAX_PATH] = "";
        HMODULE HMod = GetModuleHandle(nullptr);
        if(GetModuleFileNameA(HMod, myExe, sizeof(myExe)))
        {
            my_args.insert(my_args.begin(),myExe);
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
        const int len_dest  =  source_in_out ? strlen(source_in_out) : 0;
        const int len_result = source.length();
        if(source_in_out && len_dest != len_result)
        {
            ERROR_LOG("'replace_string' received a too short string to store the result");
            source_in_out[0]=0;
        }
    }
#if defined (WIN32)
    static void onSelectRelosution(mbm::WINDOW *w, mbm::DATA_EVENT &dataEvent)
    {
        SCREEN_RESOLUTION * resolutions  = static_cast<SCREEN_RESOLUTION * >(w->getObjectContext(4));
        int * selected_width       = static_cast<int * >(w->getObjectContext(2));
        int * selected_height      = static_cast<int * >(w->getObjectContext(3));
        int index                  = dataEvent.getAsInt();
        * selected_width           = resolutions[index].width;
        * selected_height          = resolutions[index].height;
    }

    static void onSelectApplication(mbm::WINDOW *w, mbm::DATA_EVENT &dataEvent)
    {
        APP_RUN * app_run = static_cast<APP_RUN *>(w->getObjectContext(6));
        std::string* script_app = static_cast<std::string*>(w->getObjectContext(7));
        int index = dataEvent.getAsInt();
        *script_app = app_run[index].script_path;
    }

    
    static void onSelectUserScript(mbm::WINDOW* w, mbm::DATA_EVENT& dataEvent)
    {
        APP_RUN* app_run = static_cast<APP_RUN*>(w->getObjectContext(6));
        int * idAppSelection = static_cast<int*>(w->getObjectContext(8));
        std::string* custom_script = static_cast<std::string*>(w->getObjectContext(9));
        int* size_app_run = static_cast<int*>(w->getObjectContext(10));
        char file_selected[1024] = {};
        char* the_file = mbm::openFileBox("*.lua", "Script", true, false, w->getHwnd(), custom_script->c_str(), file_selected);
        if (the_file)
        {
            *custom_script = the_file;
            app_run[*size_app_run - 1].script_path = custom_script->c_str();
            w->removeText(*idAppSelection, *size_app_run - 1);
            w->addText(*idAppSelection, the_file);
            w->setSelectedIndex(*idAppSelection, *size_app_run - 1);
        }
    }

    static void onSelectMonitor(mbm::WINDOW *w, mbm::DATA_EVENT &dataEvent)
    {
        __AUX_MONITOR_SELECT *__auxSelectMonitor = static_cast<__AUX_MONITOR_SELECT *>(w->getObjectContext(0));
        mbm::MONITOR_MANAGER *manMonitor         = static_cast<mbm::MONITOR_MANAGER *>(w->getObjectContext(1));
        if (__auxSelectMonitor->monitor && manMonitor)
        {
            mbm::MONITOR monitorOut;
            const int index = dataEvent.getAsInt();
            if (manMonitor->getMonitor(index, &monitorOut))
                *__auxSelectMonitor->monitor = monitorOut;
        }
    }

    
    static void onSelectFullScreen(mbm::WINDOW *w, mbm::DATA_EVENT &dataEvent)
    {
        bool * p_is_full_screen       = static_cast<bool * >(w->getObjectContext(5));
        *p_is_full_screen = dataEvent.getAsBool();
    }
#endif
#if defined _DEBUG
    void restoreDeviceTest()
    {
        log_util::print_colored(COLOR_TERMINAL_YELLOW, "restoreDeviceTest called");
        mbm::DEVICE::getInstance()->refreshDevice();
	}
#endif

#if defined (WIN32)
    bool select_app_and_resolution(APP_RUN* app_run, int size_app_run, int * index_app_selected, SCREEN_RESOLUTION* screen_resolution_list, int size_screen_resolution_list, bool allow_full_screen, const bool full_screen_checked)
    {
        mbm::REGEDIT reg_index_monitor,reg_index_resolution,reg_full_screen, reg_script_app, reg_user_script;
        const char * strKeyName = my_app_name.length() > 0 ? my_app_name.c_str() : "Mini-Mbm";
        std::string key_index_monitor(strKeyName);
        std::string key_resolution(strKeyName);
        std::string key_screen_full_screen(strKeyName);
        std::string key_index_script_app(strKeyName);
        std::string key_user_script(strKeyName);
        key_index_monitor       += "\\index-monoitor";
        key_resolution          += "\\index-resolution";
        key_screen_full_screen  += "\\full-screen";
        key_index_script_app    += "\\script-app";
        key_user_script         += "\\user-script";
        reg_index_monitor.openKey(HKEY_CURRENT_USER,key_index_monitor.c_str());
        reg_index_resolution.openKey(HKEY_CURRENT_USER,key_resolution.c_str());
        reg_full_screen.openKey(HKEY_CURRENT_USER,key_screen_full_screen.c_str());
        reg_script_app.openKey(HKEY_CURRENT_USER, key_index_script_app.c_str());
        reg_user_script.openKey(HKEY_CURRENT_USER, key_user_script.c_str());
        
        mbm::MONITOR my_monitor_selected;
        mbm::MONITOR_MANAGER manMonitor;
        manMonitor.updateMonitors();
        mbm::WINDOW w;
        bool full_screen = allow_full_screen && full_screen_checked;
        int x_las_pos = 0;
        int y_las_pos = 0;
        const int extra_height = size_app_run != 0 ? 60 : 0;
        const int width_screen_option = 400;
        const int height_screen_option = 350 + extra_height;
        const int regindex_monitor    = reg_index_monitor.getVal(key_index_monitor.c_str(),0xff);
        if(regindex_monitor != 0xff && manMonitor.getMonitor(regindex_monitor, &my_monitor_selected))
        {
            x_las_pos = static_cast<int>(((my_monitor_selected.width  * 0.5f) - (width_screen_option * 0.5f)) + my_monitor_selected.position.x);
            y_las_pos = static_cast<int>(((my_monitor_selected.height * 0.5f) - (height_screen_option * 0.5f)) + my_monitor_selected.position.y);
        }
        bool isPTbr = false;
        #if defined   _WIN32
        WCHAR     localeName[LOCALE_NAME_MAX_LENGTH] = {0};
        const int len                                = sizeof(localeName) / sizeof(*(localeName));
        int       ret                                = GetUserDefaultLocaleName(localeName, len);
        if (ret != 0)
        {
            char stextOut[1024] = "";
            const char * idiom = util::toChar(localeName, stextOut);
            std::string the_idom(idiom ? idiom : "");
            if (the_idom.find("pt") != std::string::npos || the_idom.find("PT") != std::string::npos )
                isPTbr  = true;
        }
        #elif defined __linux__ || defined(__APPLE__)
        const char *lang = getenv("LANG");
        if (lang)
        {
            setlocale(LC_ALL, lang);
            const char * idiom = nl_langinfo(_NL_IDENTIFICATION_LANGUAGE);
            std::string the_idom(idiom ? idiom : "");
            if (the_idom.find("pt") != std::string::npos || the_idom.find("PT") != std::string::npos )
                isPTbr  = true;
        }
        #endif
        const char * temp_app_name        = "Screen options";
        const char * temp_resol_name      = "Screen resolution:";
        const char * temp_monitor_lbl     = "Monitor Selection:";
        const char * temp_full_screen_lbl = "Full Screen";
        const char * temp_play_lbl        = "START";
        if (isPTbr)
        {
            temp_app_name        = "Opções de Tela";
            temp_monitor_lbl     = "Selecione um monitor:";
            temp_resol_name      = "Selecione uma Resolução:";
            temp_full_screen_lbl = "Tela cheia";
            temp_play_lbl        = "INICIAR";
        }
        w.init(my_app_name.length() > 0 ? my_app_name.c_str() : temp_app_name, width_screen_option, height_screen_option, x_las_pos, y_las_pos, false, false, false, false, __AUX_MONITOR_SELECT::__0_onProcess, false,external_ID_ICON);
        w.addLabel(temp_monitor_lbl, 10, 10, 380, 25);
        __AUX_MONITOR_SELECT __auxSelectMonitor;
        w.setObjectContext(&__auxSelectMonitor, 0);
        w.setObjectContext(&manMonitor, 1);
        __auxSelectMonitor.monitor                  = &my_monitor_selected;
        __auxSelectMonitor.indexCmbSelectedeMonitor = w.addCombobox(10, 50, 380, 100,onSelectMonitor);
        
        DWORD s = manMonitor.getTotalMonitor();

        int max_width  = 0;
        int max_height = 0;
        int selected_width       = 0;
        int selected_height      = 0;
        w.setObjectContext(&selected_width,2);
        w.setObjectContext(&selected_height,3);
        for (DWORD i = 0; i < s; ++i)
        {
            char         str[255];
            mbm::MONITOR temp;
            if (manMonitor.getMonitor(i, &temp))
            {
                if (isPTbr)
                {
                    sprintf(str, "%d: %ld x %ld, frequência:%lu, posição:%ld x %ld", (int)i + 1, temp.width, temp.height,
                        temp.frequency, temp.position.x, temp.position.y);
                }
                else
                {
                    sprintf(str, "%d: %ld x %ld, frequency:%lu, position:%ld x %ld", (int)i + 1, temp.width, temp.height,
                        temp.frequency, temp.position.x, temp.position.y);
                }
                w.addText(__auxSelectMonitor.indexCmbSelectedeMonitor, str);
                max_width  = max_width  > temp.width ? max_width : temp.width;
                max_height = max_height > temp.height ? max_height : temp.height;
            }
            else
            {
                return false;
            }
        }

        w.setSelectedIndex(__auxSelectMonitor.indexCmbSelectedeMonitor, manMonitor.getIndexMainMonitor());
        manMonitor.getMonitor(manMonitor.getIndexMainMonitor(), &my_monitor_selected);

        w.addLabel(temp_resol_name, 10, 100, 380, 25);
    
        if(screen_resolution_list == nullptr)
        {
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
            size_screen_resolution_list = sizeof(default_resolutions) / sizeof(SCREEN_RESOLUTION);
            screen_resolution_list = default_resolutions;
        }
        
        int ilastIndex = -1;
        w.setObjectContext(static_cast<void*>(screen_resolution_list), 4);
        int idResolution = w.addCombobox(10, 130, 380, 100,onSelectRelosution);
        
        for (int i = 0; i < size_screen_resolution_list; i++)
        {
            char         str[255];
            SCREEN_RESOLUTION * screen_resolution = &screen_resolution_list[i];
            if(screen_resolution->width <= max_width && screen_resolution->height <= max_height)
            {
                sprintf(str, "%d x %d %s",screen_resolution->width, screen_resolution->height, screen_resolution->description ? screen_resolution->description : "");
                w.addText(idResolution, str);
                selected_width  = screen_resolution->width;
                selected_height = screen_resolution->height;
                ilastIndex++;
            }
        }
        w.setSelectedIndex(idResolution,ilastIndex);
        
        w.setObjectContext(static_cast<void*>(&full_screen), 5);
        if(allow_full_screen)
        {
            const int idFull =  w.addCheckBox(temp_full_screen_lbl, 10, extra_height + 300, 200, 20, onSelectFullScreen);
            full_screen = reg_full_screen.getVal(key_screen_full_screen.c_str(),0) ? true : false;
            w.setCheckBox(full_screen, idFull);
        }
        __auxSelectMonitor.idbntOk = w.addButton(temp_play_lbl, 310, extra_height + 300, 70, 20, -1, __AUX_MONITOR_SELECT::__0_onPressOkMonitor);
        w.setCheckBox(false, __auxSelectMonitor.idChkAskAboutMonitor);
        w.askOnExit = false;
        w.hideConsoleWindow();

        if(regindex_monitor != 0xff && regindex_monitor < static_cast<int>(manMonitor.getTotalMonitor()))
        {
            my_monitor_selected.index = regindex_monitor;
            w.setSelectedIndex(__auxSelectMonitor.indexCmbSelectedeMonitor, regindex_monitor);
            manMonitor.getMonitor(regindex_monitor, &my_monitor_selected);
        }
        const int regindex_resolution = reg_index_resolution.getVal(key_resolution.c_str(),0xff);
        if(regindex_resolution != 0xff && regindex_resolution <= ilastIndex && regindex_resolution < size_screen_resolution_list)
        {
            w.setSelectedIndex(idResolution,regindex_resolution);
            selected_width  = screen_resolution_list[regindex_resolution].width;
            selected_height = screen_resolution_list[regindex_resolution].height;
        }

        std::string script_app;
        static std::string custom_script;
        custom_script.clear(); 
        int idAppSelection = -1;
        int idCustomScript = -1;

        if (size_app_run > 0)
        {
            const char* temp_app_label = "Application:";
            const char* temp_app_custom = "Custom Script...";
            int adjusted_custom = 90;
            if (isPTbr)
            {
                temp_app_label = "Aplicativo:";
                temp_app_custom = "Aplicativo Personalizado...";
                adjusted_custom = 150;
            }
            w.addLabel(temp_app_label, 10, 180, 380, 25);
            idAppSelection = w.addCombobox(10, 210, 380, 100, onSelectApplication);
            for (int i = 0; i < size_app_run; i++)
            {
                if(i == size_app_run - 1)
                {
                    custom_script = reg_user_script.getString(key_user_script.c_str(), "User specified script");
                    w.addText(idAppSelection, custom_script.c_str());
                    app_run[i].script_path = custom_script.c_str();
                }
                else if (isPTbr)
                {
                    w.addText(idAppSelection, app_run[i].name_pt_br ? app_run[i].name_pt_br : "Sem nome");
                }
                else
                {
                    w.addText(idAppSelection, app_run[i].name_eng ? app_run[i].name_eng : "No name");
                }
            }
            w.setObjectContext(static_cast<void*>(app_run), 6);
            w.setObjectContext(static_cast<void*>(&script_app), 7);
            w.setObjectContext(static_cast<void*>(&idAppSelection), 8);

            if (index_app_selected != nullptr && (*index_app_selected) == (size_app_run - 1))
            {
                w.setSelectedIndex(idAppSelection, *index_app_selected);
            }
            else
            {
                const int regindex_app = reg_script_app.getVal(key_index_script_app.c_str(), 0xff);
                if (regindex_app != 0xff && regindex_app < size_app_run)
                {
                    w.setSelectedIndex(idAppSelection, regindex_app);
                }
            }
            
            w.setObjectContext(static_cast<void*>(&custom_script), 9);
            w.setObjectContext(static_cast<void*>(&size_app_run), 10);
            idCustomScript = w.addButton(temp_app_custom, 380 + 10 - adjusted_custom, 180, adjusted_custom, 20, -1, onSelectUserScript);
        }

        w.exitOnEsc = false;
        w.enterLoop(nullptr);
        w.run = false;
        w.closeWindow();
        w.doEvents();
        if(full_screen && manMonitor.getMonitor(my_monitor_selected.index, &my_monitor_selected))
        {
            selected_width  = my_monitor_selected.width;
            selected_height = my_monitor_selected.height;
            mbm::disable_window_border();
        }
            
        mbm::set_window_position(my_monitor_selected.position.x,my_monitor_selected.position.y);
        mbm::set_window_size(selected_width,selected_height);
        reg_index_monitor.setVal(key_index_monitor.c_str(),my_monitor_selected.index);
        reg_index_resolution.setVal(key_resolution.c_str(),w.getSelectedIndex(idResolution));
        reg_full_screen.setVal(key_screen_full_screen.c_str(),full_screen ? 1 : 0);
        if (size_app_run > 0)
        {
            int local_index_app_selected = w.getSelectedIndex(idAppSelection);
            reg_script_app.setVal(key_index_script_app.c_str(), local_index_app_selected);
            if(index_app_selected != nullptr)
            {
                *index_app_selected = local_index_app_selected;
            }
            if(custom_script.length() > 0)
            {
                reg_user_script.setString(key_user_script.c_str(), custom_script);
            }
        }
        reg_index_monitor.closeKey();
        reg_index_resolution.closeKey();
        reg_full_screen.closeKey();
        reg_user_script.closeKey();
        return true;
    }

    bool select_resolution(SCREEN_RESOLUTION* screen_resolution_list, int size_screen_resolution_list, bool allow_full_screen, const bool full_screen_checked)
    {
        return select_app_and_resolution(nullptr, 0, nullptr, screen_resolution_list, size_screen_resolution_list, allow_full_screen, full_screen_checked);
    }
    #elif defined (__linux__) || defined(__APPLE__)
    
    // Simple X11 dialog for resolution/app selection
    bool select_app_and_resolution(APP_RUN* app_run, int size_app_run, int * index_app_selected, SCREEN_RESOLUTION* screen_resolution_list, int size_screen_resolution_list, bool allow_full_screen, const bool full_screen_checked)
    {
        Display* display = XOpenDisplay(nullptr);
        if (!display)
        {
            ERROR_LOG("Cannot open X11 display");
            return false;
        }
        
        int screen = DefaultScreen(display);
        Window root = RootWindow(display, screen);
        
        // Get monitor info using Xrandr
        int num_monitors = 0;
        XRRMonitorInfo* monitors = XRRGetMonitors(display, root, True, &num_monitors);
        
        // Get current screen dimensions
        int max_width = DisplayWidth(display, screen);
        int max_height = DisplayHeight(display, screen);
        
        // Default resolutions if none provided
        static SCREEN_RESOLUTION default_resolutions[] = {
            {640,     360,   "Low resolution"},
            {800,     600,   "XVGA"},
            {960,     540,   "qHD"},
            {1024,    768,   "XGA"},
            {1280,    720,   "Standard High Density (HD)"},
            {1280,    768,   "WXGA"},
            {1280,    800,   "WXGA"},
            {1600,    900,   "HD+"},
            {1920,    1080,  "Standard Full HD Display"},
            {2560,    1440,  "Standard Quad HD Display"},
            {3200,    1800,  "QHD+"},
            {3840,    2160,  "Standard Ultra HD Display"},
        };
        
        if (screen_resolution_list == nullptr)
        {
            size_screen_resolution_list = sizeof(default_resolutions) / sizeof(SCREEN_RESOLUTION);
            screen_resolution_list = default_resolutions;
        }
        
        // Filter resolutions that fit the screen
        std::vector<SCREEN_RESOLUTION> valid_resolutions;
        for (int i = 0; i < size_screen_resolution_list; i++)
        {
            if (screen_resolution_list[i].width <= max_width && 
                screen_resolution_list[i].height <= max_height)
            {
                valid_resolutions.push_back(screen_resolution_list[i]);
            }
        }
        
        if (valid_resolutions.empty())
        {
            valid_resolutions.push_back({max_width, max_height, "Native"});
        }
        
        // Selection state
        int selected_monitor = 0;
        int selected_resolution = valid_resolutions.size() - 1; // Default to highest valid
        int selected_app = (index_app_selected && *index_app_selected >= 0) ? *index_app_selected : 0;
        bool full_screen = allow_full_screen && full_screen_checked;
        bool confirmed = false;
        
        // Window dimensions
        const int win_width = 420;
        const int win_height = (size_app_run > 0) ? 400 : 320;
        
        // Create window
        XSetWindowAttributes attrs;
        attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask;
        attrs.background_pixel = WhitePixel(display, screen);
        
        Window win = XCreateWindow(display, root,
            (max_width - win_width) / 2, (max_height - win_height) / 2,
            win_width, win_height, 1,
            CopyFromParent, InputOutput, CopyFromParent,
            CWEventMask | CWBackPixel, &attrs);
        
        // Set window title
        const char* title = my_app_name.length() > 0 ? my_app_name.c_str() : "Screen Options";
        XStoreName(display, win, title);
        
        // Create GC for drawing
        GC gc = XCreateGC(display, win, 0, nullptr);
        XSetForeground(display, gc, BlackPixel(display, screen));
        
        // Load font
        XFontStruct* font = XLoadQueryFont(display, "-*-helvetica-medium-r-*-*-14-*-*-*-*-*-*-*");
        if (!font) font = XLoadQueryFont(display, "fixed");
        if (font) XSetFont(display, gc, font->fid);
        
        // Make window appear
        XMapWindow(display, win);
        
        // UI element positions
        const int margin = 20;
        const int label_height = 20;
        const int combo_height = 25;
        const int spacing = 10;
        int y_pos = margin;
        
        // Button areas (will be calculated during draw)
        struct { int x, y, w, h; } monitor_up, monitor_down;
        struct { int x, y, w, h; } res_up, res_down;
        struct { int x, y, w, h; } app_up, app_down;
        struct { int x, y, w, h; } fullscreen_box;
        struct { int x, y, w, h; } start_btn;
        
        // Event loop
        bool running = true;
        while (running)
        {
            XEvent event;
            XNextEvent(display, &event);
            
            if (event.type == Expose && event.xexpose.count == 0)
            {
                // Clear window
                XClearWindow(display, win);
                y_pos = margin;
                
                // Draw monitor selection
                XDrawString(display, win, gc, margin, y_pos + 14, "Monitor:", 8);
                y_pos += label_height + 5;
                
                char monitor_str[256];
                if (num_monitors > 0 && selected_monitor < num_monitors)
                {
                    snprintf(monitor_str, sizeof(monitor_str), "%d: %dx%d at (%d,%d)",
                        selected_monitor + 1,
                        monitors[selected_monitor].width,
                        monitors[selected_monitor].height,
                        monitors[selected_monitor].x,
                        monitors[selected_monitor].y);
                }
                else
                {
                    snprintf(monitor_str, sizeof(monitor_str), "Primary: %dx%d", max_width, max_height);
                }
                
                // Draw combo-like box with arrows
                XDrawRectangle(display, win, gc, margin, y_pos, win_width - 2*margin - 60, combo_height);
                XDrawString(display, win, gc, margin + 5, y_pos + 17, monitor_str, strlen(monitor_str));
                
                // Up/Down buttons
                monitor_down = {win_width - margin - 55, y_pos, 25, combo_height};
                monitor_up = {win_width - margin - 27, y_pos, 25, combo_height};
                XDrawRectangle(display, win, gc, monitor_down.x, monitor_down.y, monitor_down.w, monitor_down.h);
                XDrawRectangle(display, win, gc, monitor_up.x, monitor_up.y, monitor_up.w, monitor_up.h);
                XDrawString(display, win, gc, monitor_down.x + 8, monitor_down.y + 17, "<", 1);
                XDrawString(display, win, gc, monitor_up.x + 8, monitor_up.y + 17, ">", 1);
                y_pos += combo_height + spacing + 10;
                
                // Draw resolution selection
                XDrawString(display, win, gc, margin, y_pos + 14, "Resolution:", 11);
                y_pos += label_height + 5;
                
                char res_str[256];
                snprintf(res_str, sizeof(res_str), "%d x %d %s",
                    valid_resolutions[selected_resolution].width,
                    valid_resolutions[selected_resolution].height,
                    valid_resolutions[selected_resolution].description ? valid_resolutions[selected_resolution].description : "");
                
                XDrawRectangle(display, win, gc, margin, y_pos, win_width - 2*margin - 60, combo_height);
                XDrawString(display, win, gc, margin + 5, y_pos + 17, res_str, strlen(res_str));
                
                res_down = {win_width - margin - 55, y_pos, 25, combo_height};
                res_up = {win_width - margin - 27, y_pos, 25, combo_height};
                XDrawRectangle(display, win, gc, res_down.x, res_down.y, res_down.w, res_down.h);
                XDrawRectangle(display, win, gc, res_up.x, res_up.y, res_up.w, res_up.h);
                XDrawString(display, win, gc, res_down.x + 8, res_down.y + 17, "<", 1);
                XDrawString(display, win, gc, res_up.x + 8, res_up.y + 17, ">", 1);
                y_pos += combo_height + spacing + 10;
                
                // Draw app selection if provided
                if (size_app_run > 0)
                {
                    XDrawString(display, win, gc, margin, y_pos + 14, "Application:", 12);
                    y_pos += label_height + 5;
                    
                    const char* app_name = "";
                    if (selected_app < size_app_run)
                    {
                        app_name = app_run[selected_app].name_eng ? app_run[selected_app].name_eng : app_run[selected_app].script_path;
                    }
                    
                    XDrawRectangle(display, win, gc, margin, y_pos, win_width - 2*margin - 60, combo_height);
                    if (app_name) XDrawString(display, win, gc, margin + 5, y_pos + 17, app_name, strlen(app_name));
                    
                    app_down = {win_width - margin - 55, y_pos, 25, combo_height};
                    app_up = {win_width - margin - 27, y_pos, 25, combo_height};
                    XDrawRectangle(display, win, gc, app_down.x, app_down.y, app_down.w, app_down.h);
                    XDrawRectangle(display, win, gc, app_up.x, app_up.y, app_up.w, app_up.h);
                    XDrawString(display, win, gc, app_down.x + 8, app_down.y + 17, "<", 1);
                    XDrawString(display, win, gc, app_up.x + 8, app_up.y + 17, ">", 1);
                    y_pos += combo_height + spacing + 10;
                }
                
                // Full screen checkbox
                if (allow_full_screen)
                {
                    fullscreen_box = {margin, y_pos, 20, 20};
                    XDrawRectangle(display, win, gc, fullscreen_box.x, fullscreen_box.y, fullscreen_box.w, fullscreen_box.h);
                    if (full_screen)
                    {
                        XDrawLine(display, win, gc, fullscreen_box.x + 3, fullscreen_box.y + 10, fullscreen_box.x + 8, fullscreen_box.y + 15);
                        XDrawLine(display, win, gc, fullscreen_box.x + 8, fullscreen_box.y + 15, fullscreen_box.x + 17, fullscreen_box.y + 3);
                    }
                    XDrawString(display, win, gc, fullscreen_box.x + 30, fullscreen_box.y + 15, "Full Screen", 11);
                    y_pos += 30 + spacing;
                }
                
                // Start button
                start_btn = {win_width - margin - 100, win_height - margin - 35, 100, 30};
                XDrawRectangle(display, win, gc, start_btn.x, start_btn.y, start_btn.w, start_btn.h);
                XDrawString(display, win, gc, start_btn.x + 30, start_btn.y + 20, "START", 5);
            }
            else if (event.type == ButtonPress)
            {
                int mx = event.xbutton.x;
                int my = event.xbutton.y;
                
                // Check monitor buttons
                if (mx >= monitor_down.x && mx <= monitor_down.x + monitor_down.w &&
                    my >= monitor_down.y && my <= monitor_down.y + monitor_down.h)
                {
                    if (selected_monitor > 0) selected_monitor--;
                    XClearArea(display, win, 0, 0, 0, 0, True);
                }
                else if (mx >= monitor_up.x && mx <= monitor_up.x + monitor_up.w &&
                         my >= monitor_up.y && my <= monitor_up.y + monitor_up.h)
                {
                    if (selected_monitor < num_monitors - 1) selected_monitor++;
                    XClearArea(display, win, 0, 0, 0, 0, True);
                }
                // Check resolution buttons
                else if (mx >= res_down.x && mx <= res_down.x + res_down.w &&
                         my >= res_down.y && my <= res_down.y + res_down.h)
                {
                    if (selected_resolution > 0) selected_resolution--;
                    XClearArea(display, win, 0, 0, 0, 0, True);
                }
                else if (mx >= res_up.x && mx <= res_up.x + res_up.w &&
                         my >= res_up.y && my <= res_up.y + res_up.h)
                {
                    if (selected_resolution < (int)valid_resolutions.size() - 1) selected_resolution++;
                    XClearArea(display, win, 0, 0, 0, 0, True);
                }
                // Check app buttons
                else if (size_app_run > 0)
                {
                    if (mx >= app_down.x && mx <= app_down.x + app_down.w &&
                        my >= app_down.y && my <= app_down.y + app_down.h)
                    {
                        if (selected_app > 0) selected_app--;
                        XClearArea(display, win, 0, 0, 0, 0, True);
                    }
                    else if (mx >= app_up.x && mx <= app_up.x + app_up.w &&
                             my >= app_up.y && my <= app_up.y + app_up.h)
                    {
                        if (selected_app < size_app_run - 1) selected_app++;
                        XClearArea(display, win, 0, 0, 0, 0, True);
                    }
                }
                // Check fullscreen checkbox
                if (allow_full_screen &&
                    mx >= fullscreen_box.x && mx <= fullscreen_box.x + fullscreen_box.w + 100 &&
                    my >= fullscreen_box.y && my <= fullscreen_box.y + fullscreen_box.h)
                {
                    full_screen = !full_screen;
                    XClearArea(display, win, 0, 0, 0, 0, True);
                }
                // Check start button
                if (mx >= start_btn.x && mx <= start_btn.x + start_btn.w &&
                    my >= start_btn.y && my <= start_btn.y + start_btn.h)
                {
                    confirmed = true;
                    running = false;
                }
            }
            else if (event.type == KeyPress)
            {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_Return || key == XK_KP_Enter)
                {
                    confirmed = true;
                    running = false;
                }
                else if (key == XK_Escape)
                {
                    running = false;
                }
            }
            else if (event.type == ClientMessage || event.type == DestroyNotify)
            {
                running = false;
            }
        }
        
        // Apply selections
        if (confirmed)
        {
            int sel_width = valid_resolutions[selected_resolution].width;
            int sel_height = valid_resolutions[selected_resolution].height;
            int pos_x = 0, pos_y = 0;
            
            if (num_monitors > 0 && selected_monitor < num_monitors)
            {
                pos_x = monitors[selected_monitor].x;
                pos_y = monitors[selected_monitor].y;
                
                if (full_screen)
                {
                    sel_width = monitors[selected_monitor].width;
                    sel_height = monitors[selected_monitor].height;
                    mbm::disable_window_border();
                }
            }
            
            mbm::set_window_position(pos_x, pos_y);
            mbm::set_window_size(sel_width, sel_height);
            
            if (index_app_selected)
            {
                *index_app_selected = selected_app;
            }
        }
        
        // Cleanup
        if (font) XFreeFont(display, font);
        XFreeGC(display, gc);
        XDestroyWindow(display, win);
        if (monitors) XRRFreeMonitors(monitors);
        XCloseDisplay(display);
        
        return confirmed;
    }
    
    bool select_resolution(SCREEN_RESOLUTION* screen_resolution_list, int size_screen_resolution_list, bool allow_full_screen, const bool full_screen_checked)
    {
        return select_app_and_resolution(nullptr, 0, nullptr, screen_resolution_list, size_screen_resolution_list, allow_full_screen, full_screen_checked);
    }
#endif

}