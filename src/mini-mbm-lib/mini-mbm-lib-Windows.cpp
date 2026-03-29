/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2021      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#if defined(WIN32) || defined(_WIN32)

#include "mini-mbm-lib.h"
#include <util-interface.h>
#include <core_mbm/strings-pt-br.h>
#include <defaultThemePlusWindows.h>

extern std::string my_app_name;
extern DWORD       external_ID_ICON;
extern bool        _my_theme_selected;

namespace mbm
{

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

    bool select_app_and_resolution(APP_RUN* app_run, int size_app_run, int * index_app_selected, SCREEN_RESOLUTION* screen_resolution_list, int size_screen_resolution_list, bool allow_full_screen, const bool full_screen_checked, int requested_width, int requested_height)
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
        #if defined _WIN32
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
        #endif
        const char * temp_app_name        = "Screen options";
        const char * temp_resol_name      = "Screen resolution:";
        const char * temp_monitor_lbl     = "Monitor Selection:";
        const char * temp_full_screen_lbl = "Full Screen";
        const char * temp_play_lbl        = "START";
        if (isPTbr)
        {
            temp_app_name        = STR_PT_BR_SCREEN_OPTIONS;
            temp_monitor_lbl     = STR_PT_BR_MONITOR_SELECT;
            temp_resol_name      = STR_PT_BR_RESOLUTION_SELECT;
            temp_full_screen_lbl = STR_PT_BR_FULL_SCREEN;
            temp_play_lbl        = STR_PT_BR_START;
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
                    snprintf(str, sizeof(str), STR_PT_BR_MONITOR_FORMAT, (int)i + 1, temp.width, temp.height,
                        temp.frequency, temp.position.x, temp.position.y);
                }
                else
                {
                    snprintf(str, sizeof(str), "%d: %ld x %ld, frequency:%lu, position:%ld x %ld", (int)i + 1, temp.width, temp.height,
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

        int mon_width = static_cast<int>(my_monitor_selected.width);
        int mon_height = static_cast<int>(my_monitor_selected.height);
        if (mon_width <= 0 || mon_height <= 0)
        {
            mon_width = max_width;
            mon_height = max_height;
        }

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
        
        std::vector<SCREEN_RESOLUTION> resolution_list_for_combobox;
        for (int i = 0; i < size_screen_resolution_list; i++)
        {
            SCREEN_RESOLUTION* r = &screen_resolution_list[i];
            if (r->width <= mon_width && r->height <= mon_height)
                resolution_list_for_combobox.push_back(*r);
        }
        if (requested_width > 0 && requested_height > 0 && requested_width <= mon_width && requested_height <= mon_height)
        {
            bool found = false;
            for (size_t i = 0; i < resolution_list_for_combobox.size(); i++)
            {
                if (resolution_list_for_combobox[i].width == requested_width && resolution_list_for_combobox[i].height == requested_height)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                resolution_list_for_combobox.push_back({requested_width, requested_height, "Requested"});
        }
        bool has_native = false;
        for (size_t i = 0; i < resolution_list_for_combobox.size(); i++)
        {
            if (resolution_list_for_combobox[i].width == mon_width && resolution_list_for_combobox[i].height == mon_height)
            {
                has_native = true;
                break;
            }
        }
        if (!has_native)
            resolution_list_for_combobox.push_back({mon_width, mon_height, "Native"});
        if (resolution_list_for_combobox.empty())
            resolution_list_for_combobox.push_back({mon_width, mon_height, "Native"});
        
        SCREEN_RESOLUTION* list_to_use = resolution_list_for_combobox.data();
        int list_size = static_cast<int>(resolution_list_for_combobox.size());
        
        int ilastIndex = -1;
        int index_requested_resolution = -1;
        w.setObjectContext(static_cast<void*>(list_to_use), 4);
        int idResolution = w.addCombobox(10, 130, 380, 100,onSelectRelosution);
        
        for (int i = 0; i < list_size; i++)
        {
            char         str[255];
            SCREEN_RESOLUTION * screen_resolution = &list_to_use[i];
            if(screen_resolution->width <= mon_width && screen_resolution->height <= mon_height)
            {
                snprintf(str, sizeof(str), "%d x %d %s",screen_resolution->width, screen_resolution->height, screen_resolution->description ? screen_resolution->description : "");
                w.addText(idResolution, str);
                selected_width  = screen_resolution->width;
                selected_height = screen_resolution->height;
                if (requested_width > 0 && requested_height > 0 && screen_resolution->width == requested_width && screen_resolution->height == requested_height)
                    index_requested_resolution = ilastIndex + 1;
                ilastIndex++;
            }
        }
        int default_resolution_index = ilastIndex;
        if (index_requested_resolution >= 0)
            default_resolution_index = index_requested_resolution;
        w.setSelectedIndex(idResolution, default_resolution_index);
        
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
        if (index_requested_resolution < 0 && regindex_resolution != 0xff && regindex_resolution <= ilastIndex && regindex_resolution < list_size)
        {
            w.setSelectedIndex(idResolution,regindex_resolution);
            selected_width  = list_to_use[regindex_resolution].width;
            selected_height = list_to_use[regindex_resolution].height;
        }
        else if (index_requested_resolution >= 0)
        {
            selected_width  = requested_width;
            selected_height = requested_height;
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
                temp_app_label = STR_PT_BR_APPLICATION;
                temp_app_custom = STR_PT_BR_CUSTOM_SCRIPT;
                adjusted_custom = 150;
            }
            w.addLabel(temp_app_label, 10, 180, 380, 25);
            idAppSelection = w.addCombobox(10, 210, 380, 100, onSelectApplication);
            for (int i = 0; i < size_app_run; i++)
            {
                if(i == size_app_run - 1)
                {
                    if (index_app_selected != nullptr && (*index_app_selected) == (size_app_run - 1) && app_run[i].script_path &&
                        (strchr(app_run[i].script_path, '/') != nullptr || strchr(app_run[i].script_path, '\\') != nullptr))
                    {
                        custom_script = app_run[i].name_eng ? app_run[i].name_eng : util::getBaseName(app_run[i].script_path);
                        w.addText(idAppSelection, custom_script.c_str());
                    }
                    else
                    {
                        custom_script = reg_user_script.getString(key_user_script.c_str(), "User specified script");
                        w.addText(idAppSelection, custom_script.c_str());
                        app_run[i].script_path = custom_script.c_str();
                    }
                }
                else if (isPTbr)
                {
                    w.addText(idAppSelection, app_run[i].name_pt_br ? app_run[i].name_pt_br : STR_PT_BR_NO_NAME);
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
        if (full_screen)
        {
            mbm::set_expected_window_size(selected_width, selected_height);
        }
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
        return select_app_and_resolution(nullptr, 0, nullptr, screen_resolution_list, size_screen_resolution_list, allow_full_screen, full_screen_checked, 0, 0);
    }

} // namespace mbm

#endif // defined(WIN32) || defined(_WIN32)
