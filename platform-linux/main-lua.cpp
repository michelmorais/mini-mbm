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

#ifndef __linux__
    #error "Target this main is linux"
#endif

#include <lua-wrap/manager-lua.h>
#include <core_mbm/device.h>
#include <core_mbm/usage-help.h>
#include <util-interface.h>
#include <core_mbm/parse-launcher-args.hpp>

#include <mini-mbm-lib.h>
#include <core_mbm/strings-pt-br.h>

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
                strncpy(result, temporary_folder_path.c_str(), temporary_folder_path.size() - 1);
            }
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
    bool disable_select_monitor = false;

    mbm::APP_RUN default_applications[] = {
            {"Asset packager"        , STR_PT_BR_ASSET_PACKAGER,    "asset_packager.lua"},
            {"Font Maker"            , STR_PT_BR_FONT_MAKER,        "font_maker.lua"},
            {"Mesh Debug Editor"     , STR_PT_BR_MESH_DEBUG_EDITOR,  "mesh_debug.lua"},
            {"Particle Editor"       , STR_PT_BR_PARTICLE_EDITOR,   "particle_editor.lua"},
            {"Physics Editor"        , STR_PT_BR_PHYSICS_EDITOR,     "physic_editor.lua"},
            {"Scene 2D Editor"       , STR_PT_BR_SCENE_2D_EDITOR,    "scene_editor2d.lua"},
            {"Scene 3D Editor"       , STR_PT_BR_SCENE_3D_EDITOR,    "scene_editor3d.lua"},
            {"Shader Editor"         , STR_PT_BR_SHADER_EDITOR,      "shader_editor.lua"},
            {"Sprite Maker"          , STR_PT_BR_SPRITE_MAKER,       "sprite_maker.lua"},
            {"Texture Packer"        , STR_PT_BR_TEXTURE_PACKER,      "texture_packer.lua"},
            {"Tile-Map Editor"       , STR_PT_BR_TILEMAP_EDITOR,     "tilemap_editor.lua"},
            {"User specified"        , STR_PT_BR_USER_SPECIFIED,      "user_specified.lua"},
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
        PARSE_launcher_ARGS parser(argv, argc);

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
        if(parser.getExpectedWidthHeight(expected_width, expected_height))
                    {
            mbm::set_expected_window_size(
                expected_width,
                expected_height);
		}
        else
        {
            // Default the "expected" (design) resolution to the actual requested window size so
            // camera.scaleScreen2d (device-common.cpp) comes out to 1.0 unless the caller
            // explicitly opts into design-resolution scaling via -ew/-eh. This used to hardcode
            // 1920x1080 here regardless of -w/-h, which silently left camera.scaleScreen2d
            // permanently wrong (e.g. 0.556 at a 800x600 launch) for any --disable_select_monitor
            // launch at a resolution other than exactly 1920x1080 -- breaking the screen-to-world
            // math in mbm.to3d/mbm.getPickRay/obj:collide. See docs/future_investigation.md.
            mbm::set_expected_window_size(
                requested_width  > 0 ? requested_width  : 1920,
                requested_height > 0 ? requested_height : 1080);
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
        mbm::set_window_resizable(parser.enableResizeWindow);

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
        disable_select_monitor = parser.disable_select_monitor;
    }
    int ret = 0;
    
    
	mbm::set_verbose(true);
	mbm::disable_splash();
    mbm::push_arg("--showconsole","true");
    #if defined _DEBUG
        #if defined(__linux__) || !defined(__APPLE__) 
            mbm::add_path("/home/michel/mini-mbm/editor/");
            mbm::add_path("/home/michel/mini-mbm/editor/shaders");
        #elif defined(__APPLE__) 
            mbm::add_path("/Users/michel/mini-mbm/editor/");
            mbm::add_path("/Users/michel/mini-mbm/editor/shaders");
        #endif
    #endif
    size_app = sizeof(default_applications) / sizeof(mbm::APP_RUN); // add the user specified script
    if (disable_select_monitor)
    {
        // Automated/headless testing path: skip the monitor/fullscreen/script picker dialog
        // entirely and run whatever scene was set via --scene above.
        ret = mbm::onLoop();
    }
    else if (mbm::select_app_and_resolution(default_applications, size_app, &index_app_selected, nullptr, 0, allowFullScreen, full_screen_checked, requested_width, requested_height))
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

