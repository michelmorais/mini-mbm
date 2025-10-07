// mini-mbm-launcher.cpp : Defines the entry point for the application.
//
#include "mini-mbm-lib.h"
#include "framework.h"
#include "mini-mbm-launcher.h"
#include <string>
#include "parse_laucher_args.hpp"
#include "resource.h"

#pragma comment(lib, "core_mbm.lib")
#pragma comment(lib, "mini-mbm.lib")
#pragma comment(lib, "libEGL.dll.lib")
#pragma comment(lib, "libGLESv2.dll.lib")
#pragma comment(lib, "lua5.4.lib")


std::string title_app = "Mini-MBM-Dev";
std::string temporary_folder_path;

// From LUA Use: doCommands(string command, string parameter)
// e.g.: local tmp_folder = mbm.doCommands('get_tmp_folder')
void onDoNativeCommand(const char* command, const char* param, char* result, const int max_size_result)
{
    if (command)
    {
        if (strcmp(command, "get_tmp_folder") == 0)
        {
            if (temporary_folder_path.size() == 0)
            {
                char temp_folder[1024] = "";
                if (title_app.size() > 0 && mbm::create_temp_folder(title_app.c_str(), temp_folder, sizeof(temp_folder)))
                    temporary_folder_path = temp_folder;
                else if (mbm::create_temp_folder(nullptr, temp_folder, sizeof(temp_folder)))
                    temporary_folder_path = temp_folder;
            }
            if (temporary_folder_path.size() > 0)
            {
                strncpy_s(result, max_size_result, temporary_folder_path.c_str(), temporary_folder_path.size() - 1);
            }
        }
    }
}

int main(const int argc,const char **argv)
{
    bool allowFullScreen = false;
    bool full_screen_checked = true;

    mbm::APP_RUN default_applications[] = {
            {"Asset packager"        ,"Empacotador de ativos",    "asset_packager.lua"},
            {"Font Maker"            ,"Criador de fontes",        "font_maker.lua"},
            {"Particle Editor"       ,"Editor de Partículas",     "particle_editor.lua"},
            {"Physics Editor"        ,"Editor de Física",         "physic_editor.lua"},
            {"Scene 2D Editor"       ,"Editor de Cena 2D",        "scene_editor2d.lua"},
            {"Shader Editor"         ,"Editor de Shader",         "shader_editor.lua"},
            {"Sprite Maker"          ,"Editor de Sprite",         "sprite_maker.lua"},
            {"Texture Packer"        ,"Empacotador de texturas",  "texture_packer.lua"},
            {"Tile-Map Editor"       ,"Editor de mapa de blocos", "tilemap_editor.lua"},
            {"User specified"        ,"Script do usuário",        "user_specified.lua"},
    };
    int size_app = sizeof(default_applications) / sizeof(mbm::APP_RUN);
	size_app = size_app - 1; // remove the last one, it is a user specified script
    int index_app_selected = -1;
    std::string user_script_name;

    mbm::set_callback_do_commands(onDoNativeCommand);
    // parse arguments in next block
    {
        PARSE_laucher_ARGS parser;

        if (parser.width_list.size() > 0 && parser.height_list.size() > 0)
        {
            mbm::set_window_size(
                parser.width_list[parser.width_list.size() - 1],
                parser.height_list[parser.height_list.size() - 1]);
        }
        else
        {
            mbm::set_window_size(1920, 1080);
        }

        if (parser.expected_width_list.size() > 0 && parser.expected_height_list.size() > 0)
        {
            mbm::set_expected_window_size(
                parser.expected_width_list[parser.expected_width_list.size() - 1],
                parser.expected_height_list[parser.expected_height_list.size() - 1]);
        }
        else
        {
            mbm::set_expected_window_size(1920, 1080);
        }
        mbm::set_verbose(false);
        if (parser.noSplash)
        {
            mbm::disable_splash();
        }
        if (parser.nameAplication.size() > 0)
        {
            title_app = parser.nameAplication.c_str();
            mbm::set_app_name(title_app.c_str());
        }
        else
        {
            mbm::set_app_name(title_app.c_str());
        }
        //https://onlineconvertfree.com/convert/png/
        mbm::set_icon(IDI_ICON1);
        mbm::set_window_resizable(parser.enableResizeWindow);

        mbm::set_window_theme(parser.window_theme, parser.enableBorder);//11 15 19 20 21 20, 24 is the default

        if (parser.fileNameInitialLua.size() > 0)
        {
            mbm::set_scene(parser.fileNameInitialLua.c_str());
			size_app = sizeof(default_applications) / sizeof(mbm::APP_RUN); // add the user specified script
            index_app_selected = size_app - 1;
			user_script_name = parser.fileNameInitialLua;
			default_applications[index_app_selected].script_path = user_script_name.c_str();
        }

        mbm::set_window_position(parser.positionXWindow, parser.positionYWindow);

        allowFullScreen = parser.allowFullScreen;
        full_screen_checked = parser.full_screen_checked;
    }
    int ret = 0;
    
    
	mbm::set_verbose(true);
	mbm::disable_splash();
    mbm::push_arg("--showconsole","true");
    size_app = sizeof(default_applications) / sizeof(mbm::APP_RUN); // add the user specified script
    if (mbm::select_app_and_resolution(default_applications, size_app, &index_app_selected, nullptr, 0, allowFullScreen, full_screen_checked))
    {
        if (index_app_selected > -1 && index_app_selected < size_app)
        {
            mbm::set_scene(default_applications[index_app_selected].script_path);
        }
        ret = mbm::loop();
    }
    if (temporary_folder_path.size() > 0)
    {
        mbm::remove_folder(temporary_folder_path.c_str());
    }
    return ret;
}
