// mini-mbm-launcher.cpp : Defines the entry point for the application.
//
#include <locale.h>
#include "mini-mbm-lib.h"
#include "framework.h"
#include "mini-mbm-launcher.h"
#include <string>
#include <core_mbm/parse-launcher-args.hpp>
#include <core_mbm/util-interface.h>
#include "resource.h"
#include <core_mbm/strings-pt-br.h>

#pragma comment(lib, "core_mbm.lib")
#pragma comment(lib, "plugin-helper.lib")
#pragma comment(lib, "mini-mbm.lib")
#pragma comment(lib, "libEGL.dll.lib")
#pragma comment(lib, "libGLESv2.dll.lib")
#pragma comment(lib, "lua5.4.lib")


std::string title_app = "Mini-MBM-Dev";
std::string temporary_folder_path;
static std::string s_save_dir;

static std::string compute_save_dir(const std::string& app_name)
{
    const char* appdata = getenv("APPDATA");
    std::string dir = std::string(appdata ? appdata : ".") + "\\" + app_name;
    CreateDirectoryA(dir.c_str(), nullptr);  // create if absent (EEXIST is fine)
    return dir;
}

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
        else if (strcmp(command, "get_save_dir") == 0)
        {
            if (!s_save_dir.empty())
                strncpy(result, s_save_dir.c_str(), static_cast<size_t>(max_size_result) - 1);
        }
#if defined _DEBUG // testing proposal, you can call it from lua script, e.g. mbm.doCommands('restoreDeviceTest')
        else if (strcmp(command, "restoreDeviceTest") == 0)
        {
            mbm::restoreDeviceTest();
        }
#endif
    }
}

int main(const int argc,const char **argv)
{
    bool allowFullScreen = false;
    bool full_screen_checked = false;
    // Set console to Windows ANSI code page for Portuguese
    SetConsoleOutputCP(1252);
    SetConsoleCP(1252);
    setlocale(LC_ALL, "Portuguese");

    mbm::APP_RUN default_applications[] = {
            {"Asset packager"        , STR_PT_BR_ASSET_PACKAGER,    "asset_packager.lua"},
            {"Font Maker"            , STR_PT_BR_FONT_MAKER,        "font_maker.lua"},
            {"Mesh Editor"           , STR_PT_BR_MESH_EDITOR,       "mesh_debug.lua"},
            {"Particle Editor"       , STR_PT_BR_PARTICLE_EDITOR,    "particle_editor.lua"},
            {"Physics Editor"        , STR_PT_BR_PHYSICS_EDITOR,    "physic_editor.lua"},
            {"Scene 2D Editor"       , STR_PT_BR_SCENE_2D_EDITOR,   "scene_editor2d.lua"},
            {"Shader Editor"         , STR_PT_BR_SHADER_EDITOR,     "shader_editor.lua"},
            {"Sprite Maker"          , STR_PT_BR_SPRITE_MAKER,       "sprite_maker.lua"},
            {"Texture Packer"        , STR_PT_BR_TEXTURE_PACKER,    "texture_packer.lua"},
            {"Tile-Map Editor"       , STR_PT_BR_TILEMAP_EDITOR,    "tilemap_editor.lua"},
            {"User specified"        , STR_PT_BR_USER_SPECIFIED,   "user_specified.lua"},
    };
    int size_app = sizeof(default_applications) / sizeof(mbm::APP_RUN);
    size_app = size_app - 1; // remove the last one, it is a user specified script
    int index_app_selected = -1;
    std::string user_script_name;
    std::string user_script_display_name;
    unsigned int requested_width = 0, requested_height = 0;

    mbm::set_callback_do_commands(onDoNativeCommand);
    // parse arguments in next block
    {
        PARSE_laucher_ARGS parser(argv, argc);

        unsigned int width = 0, height = 0;
        if (parser.getWidthHeight(width, height))
        {
            requested_width = width;
            requested_height = height;
            mbm::set_window_size(
                width,
                height);
        }
        else
        {
            mbm::set_window_size(1920, 1080);
        }
        
        unsigned int expected_width = 0, expected_height = 0;
        if (parser.getExpectedWidthHeight(expected_width, expected_height))
        {
            mbm::set_expected_window_size(
                expected_width,
                expected_height);
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

        const char* nameApp = parser.getNameApplication();
        if (nameApp && strlen(nameApp) > 0)
        {
            title_app = nameApp;
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

        const char* fileNameInitialLua = parser.getFileNameInitialLua();
        if (fileNameInitialLua && strlen(fileNameInitialLua) > 0)
        {
            mbm::set_scene(fileNameInitialLua);
            size_app = sizeof(default_applications) / sizeof(mbm::APP_RUN); // add the user specified script
            index_app_selected = size_app - 1;
            user_script_name = fileNameInitialLua;
            default_applications[index_app_selected].script_path = user_script_name.c_str();
            user_script_display_name = util::getBaseName(fileNameInitialLua);
            default_applications[index_app_selected].name_eng = user_script_display_name.c_str();
            default_applications[index_app_selected].name_pt_br = user_script_display_name.c_str();
        }

        mbm::set_window_position(parser.positionXWindow, parser.positionYWindow);

        allowFullScreen = parser.allowFullScreen;
        full_screen_checked = parser.full_screen_checked;
    }
    int ret = 0;
    
    
    mbm::set_verbose(true);
    mbm::disable_splash();
    mbm::push_arg("--showconsole","true");
    s_save_dir = compute_save_dir(title_app);
    size_app = sizeof(default_applications) / sizeof(mbm::APP_RUN); // add the user specified script
    if (mbm::select_app_and_resolution(default_applications, size_app, &index_app_selected, nullptr, 0, allowFullScreen, full_screen_checked, requested_width, requested_height))
    {
        if (index_app_selected > -1 && index_app_selected < size_app)
        {
            mbm::set_scene(default_applications[index_app_selected].script_path);
        }
        ret = mbm::onLoop();
    }
    if (temporary_folder_path.size() > 0)
    {
        mbm::remove_folder(temporary_folder_path.c_str());
    }
    return ret;
}
