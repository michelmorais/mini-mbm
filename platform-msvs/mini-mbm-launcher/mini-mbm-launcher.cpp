// mini-mbm-launcher.cpp : Defines the entry point for the application.
//
#include "mini-mbm-lib.h"
#include "framework.h"
#include "mini-mbm-launcher.h"
#include <string>
#include "parse_laucher_args.hpp"

#pragma comment(lib, "mini-mbm.lib")
#pragma comment(lib, "libEGL.dll.lib")
#pragma comment(lib, "libGLESv2.dll.lib")
#pragma comment(lib, "lua5.4.lib")


std::string title_app   = "Launcher";
std::string temporary_folder_path;

// From LUA Use: doCommands(string command, string parameter)
// e.g.: local tmp_folder = mbm.doCommands('get_tmp_folder')
void onDoNativeCommand(const char *command, const char *param, char * result,const int max_size_result)
{
    if(command)
    {
        if(strcmp(command,"get_tmp_folder") == 0)
        {
            if(temporary_folder_path.size() == 0)
            {
                char temp_folder[1024] = "";
                if(title_app.size() > 0 && mbm::create_temp_folder(title_app.c_str(),temp_folder,sizeof(temp_folder)))
                    temporary_folder_path = temp_folder;
                else if(mbm::create_temp_folder(nullptr,temp_folder,sizeof(temp_folder)))
                    temporary_folder_path = temp_folder;
            }
            if(temporary_folder_path.size() > 0)
            {
                strncpy_s(result,max_size_result,temporary_folder_path.c_str(),temporary_folder_path.size()-1);
            }
        }
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    bool allowFullScreen = false;
    bool full_screen_checked = true;
    bool disable_select_monitor = false;

    mbm::set_callback_do_commands(onDoNativeCommand);
    // parse arguments in next block
    {
        PARSE_laucher_ARGS parser;

        if(parser.width_list.size() > 0 && parser.height_list.size() > 0)
        {
            mbm::set_window_size(
                parser.width_list[parser.width_list.size() - 1],
                parser.height_list[parser.height_list.size() - 1]);
        }
        else
        {
            mbm::set_window_size(1920,1080);
        }

        if(parser.expected_width_list.size() > 0 && parser.expected_height_list.size() > 0)
        {
            mbm::set_expected_window_size(
                parser.expected_width_list[parser.expected_width_list.size() - 1],
                parser.expected_height_list[parser.expected_height_list.size() - 1]);
        }
        else
        {
            mbm::set_expected_window_size(1920,1080);
        }
        mbm::set_verbose(false);
        if(parser.noSplash)
        {
            mbm::disable_splash();
        }
        if(parser.nameAplication.size() > 0)
        {
            title_app = parser.nameAplication.c_str();
            mbm::set_app_name(title_app.c_str());
        }
        else
        {
            mbm::set_app_name("Mini MBM");
        }
        //https://onlineconvertfree.com/convert/png/
        mbm::set_icon(IDI_ICON1);
        mbm::set_window_resizable(parser.enableResizeWindow);

        mbm::set_window_theme(parser.window_theme,parser.enableBorder);//11 15 19 20 21 20, 24 is the default

        if(parser.fileNameInitialLua.size() > 0)
        {
            mbm::set_scene(parser.fileNameInitialLua.c_str());
        }

        mbm::set_window_position(parser.positionXWindow, parser.positionYWindow);

        allowFullScreen = parser.allowFullScreen;
        full_screen_checked = parser.full_screen_checked;
        disable_select_monitor = parser.disable_select_monitor;
    }
    int ret = 0;
    if(disable_select_monitor)
    {
        ret = mbm::loop();
    }
    else if(mbm::select_resolution(nullptr,0,allowFullScreen,full_screen_checked))
    {
        ret = mbm::loop();
    }
    if(temporary_folder_path.size() > 0)
    {
        mbm::remove_folder(temporary_folder_path.c_str());
    }
    return ret;
}
