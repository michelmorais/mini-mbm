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
#include <platform/usage-help.h>
#include <defaultThemePlusWindows.h>
#include <file-util.h>


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
std::string my_app_name("Mini-Mbm");
std::vector<std::string> my_args;
#endif

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

    HWND get_hwnd()
    {
         mbm::DEVICE* device = mbm::DEVICE::getInstance();
         if(device)
         {
             return device->window.getHwnd();
         }
         return nullptr;
    }

    inline int  start_main_loop(const std::vector<std::string> & args, const int ID_ICON)
    {
        if(args.size() <= 1 ||  (args.size() > 1 && args[1].find("help") != std::string::npos))
        {
		    help(util::getBaseName(args[0].c_str()));
        }
        if(_my_theme_selected == false)
            mbm::setTheme(22, true);
	    mbm::LUA_MANAGER luaCore(args);
        if(luaCore.device && luaCore.device->verbose)
	        log_util::print_colored(COLOR_TERMINAL_YELLOW,"For documentation please check at:\n%s\n","https://mbm-documentation.readthedocs.io/en/latest/");
	
        luaCore.onDoNativeCommand = externalDoNativeCommand;
	    luaCore.idIcon = ID_ICON;
	    DisableProcessWindowsGhosting();
	    if (luaCore.initializeSceneLua(luaCore.noBorder == false))
	    {
	        luaCore.device->window.askOnExit = false;
	        luaCore.device->window.exitOnEsc = false;
	
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
		    return mbm::DEVICE::returnCodeApp ? mbm::DEVICE::returnCodeApp : ret;
	    }
	    else
	    {
	        PRINT_IF_DEBUG("Failed to load Mini Mbm %s Opengles", MBM_VERSION);
	        fprintf(stderr, "\nMini-Mbm-OpenGLES is necessary to have the following DLLs:");
	        fprintf(stderr, "\nlibEGL.dll, libGLESv2.dll and d3dcompiler_47.dll");
	        fprintf(stderr, "\nfound in mini-mbm/third-party/gles/bin");
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
    #else
        my_args.insert(my_args.begin(),"mini_mbm");
        push_arg("--name",progam_name);
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

    static void onSelectRelosution(mbm::WINDOW *w, mbm::DATA_EVENT &dataEvent)
    {
        SCREEN_RESOLUTION * resolutions  = static_cast<SCREEN_RESOLUTION * >(w->getObjectContext(4));
        int * selected_width       = static_cast<int * >(w->getObjectContext(2));
        int * selected_height      = static_cast<int * >(w->getObjectContext(3));
        int index                  = dataEvent.getAsInt();
        * selected_width           = resolutions[index].width;
        * selected_height          = resolutions[index].height;
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

    bool select_resolution(SCREEN_RESOLUTION* screen_resolution_list, int size_screen_resolution_list,bool allow_full_screen,const bool full_screen_checked)
    {
        mbm::REGEDIT reg_index_monitor,reg_index_resolution,reg_full_screen;
        const char * strKeyName = my_app_name.length() > 0 ? my_app_name.c_str() : "Mini-Mbm";
        std::string key_index_monitor(strKeyName);
        std::string key_resolution(strKeyName);
        std::string key_screen_full_screen(strKeyName);
        key_index_monitor       += "\\index-monoitor";
        key_resolution          += "\\index-resolution";
        key_screen_full_screen  += "\\full-screen";
        reg_index_monitor.openKey(HKEY_CURRENT_USER,key_index_monitor.c_str());
        reg_index_resolution.openKey(HKEY_CURRENT_USER,key_resolution.c_str());
        reg_full_screen.openKey(HKEY_CURRENT_USER,key_screen_full_screen.c_str());
        
        mbm::MONITOR my_monitor_selected;
        mbm::MONITOR_MANAGER manMonitor;
        manMonitor.updateMonitors();
        mbm::WINDOW w;
        bool full_screen = allow_full_screen && full_screen_checked;
        int x_las_pos = 0;
        int y_las_pos = 0;
        const int width_screen_option = 400;
        const int height_screen_option = 350;
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
        #elif defined __linux__
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
            const int idFull =  w.addCheckBox(temp_full_screen_lbl, 10, 300, 200, 20, onSelectFullScreen);
            full_screen = reg_full_screen.getVal(key_screen_full_screen.c_str(),0) ? true : false;
            w.setCheckBox(full_screen, idFull);
        }
        __auxSelectMonitor.idbntOk = w.addButton(temp_play_lbl, 310, 300, 70, 20, -1, __AUX_MONITOR_SELECT::__0_onPressOkMonitor);
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
        reg_index_monitor.closeKey();
        reg_index_resolution.closeKey();
        reg_full_screen.closeKey();
        return true;
    }
}