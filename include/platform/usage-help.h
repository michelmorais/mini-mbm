/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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
#ifndef USAGE_HELP_LUA_H
#define USAGE_HELP_LUA_H

#include <iostream>
#include <string.h>
#include <version/version.h>
#include <vector>
#include <string>
#include <util-interface.h>

#if defined _WIN32
    #include <Windows.h>
#else
    #include <unistd.h>
    #include <dirent.h>
#endif

#if defined _WIN32

std::vector<std::string> findLuaScripts(const char* path = nullptr)
{
    std::vector<std::string> ret;
    const char* search_path = path ? path : "..\\editor";
    WIN32_FIND_DATA fd;
    std::string              p(search_path);
    p += "\\*.*";
    const uint32_t l = strlen(search_path);
    HANDLE hFind = FindFirstFile(p.c_str(), &fd); 
    if(hFind != INVALID_HANDLE_VALUE) 
    { 
        do 
        { 
            if(! (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ) 
            {
                std::string file(fd.cFileName);
                if(file.size() > 4 && file.substr(file.size()-4).compare(".lua") == 0)
                {
                    file.insert(0,search_path);
                    file.insert(l,"/");
                    ret.emplace_back(file);
                }
            }
        }
        while(FindNextFile(hFind, &fd)); 
        FindClose(hFind); 
    }
    if (ret.size() == 0 && path == nullptr)
    {
        return findLuaScripts(".");
    }
    if (ret.size() == 0 )
    {
        ret.push_back("not found at ../editor or .");
    }
    return ret;
}

#else

std::vector<std::string> findLuaScripts(const char* path = nullptr)
{
    std::vector<std::string> ret;
    DIR * dir = nullptr;
    const char* search_path = path ? path : "../editor";
    const uint32_t l = strlen(search_path);
    struct dirent * ent = nullptr;
    if ((dir = opendir (search_path)) != nullptr) 
    {
        while ((ent = readdir (dir)) != nullptr) 
        {
            std::string file(ent->d_name);
            if(file.size() > 4 && file.substr(file.size()-4).compare(".lua") == 0)
            {
                file.insert(0,search_path);
                file.insert(l,"/");
                ret.emplace_back(file);
            }
        }
        closedir (dir);
    }
    if (ret.size() == 0 && path == nullptr)
    {
        return findLuaScripts(".");
    }
    if (ret.size() == 0)
    {
        ret.push_back("not found at ../editor or .");
    }
    return ret;
}

#endif

void help(const std::string & executableName)
{
    log_util::print_colored(COLOR_TERMINAL_WHITE,"Version:");
    log_util::print_colored(COLOR_TERMINAL_MAGENTA,"%s\n",MBM_VERSION);
    std::cout <<
    "Usage: " <<  executableName  << " [options]... [file_name.lua] or [--scene file_name.lua]..." << std::endl <<
    "    --help             " << " display this help       " << std::endl <<
    "-w, --width            " << " set window's width " << std::endl <<
    "-h, --height           " << " set window's height" << std::endl <<
    "-ew,--expectedwidth    " << " set expected window's width " << std::endl <<
    "-eh,--expectedheight   " << " set expected window's height" << std::endl <<
    "    --stretch          " << " stretch to axis ('x', 'y', or 'xy') default is 'y'" << std::endl <<
    "    --nosplash         " << " do not display logo     " << std::endl <<
    "-x, --posx             " << " set x window's position" << std::endl <<
    "-y, --posy             " << " set y window's position" << std::endl <<
	#if defined _WIN32
    "    --maximizedwindow  " << " set maximized flag window (Windows platform only until now) " << std::endl <<
	#endif
    "-s, --scene            " << " set scene to load (e.g.: main.lua)" << std::endl <<
    "-n, --name             " << " set window's name" << std::endl <<
    "-a, --addpath          " << " add path to search file at run time" << std::endl <<
	"--noborder             " << " new window without border" << std::endl <<
    
        
    #if defined _WIN32
    "    --showconsole      " << " show windows's console" << std::endl <<
    #endif
    "   " << std::endl <<
    "    note: to add a global variable use '='. example:"<< std::endl <<
    "                            'myVarNumber=5.0' 'stringVar=someString'" << std::endl <<
    "    the variable will be stored as number when is number."  << std::endl <<
    "    otherwise the variable will be stored as string."  << std::endl <<
    "    use getGlobal function to get variable at lua script (see doc for more information)" << std::endl << std::endl <<
    "    use space to separate arguments:" << std::endl <<
    "    usage example:" << std::endl <<
    "            " <<  executableName << " --scene main.lua -w 800 -h 600 " << std::endl <<
    "            " <<  executableName << " -s main.lua -w 800 -h 600 'myVarNumber=5.0' 'stringVar=someString' --nosplash" << std::endl  << std::endl <<
    "    available scripts found:" << std::endl;

    std::vector<std::string> lua_files = findLuaScripts();
    for(uint32_t i=0; i< lua_files.size(); ++i)
    {
        std::cout << "            " <<  executableName << " --scene " << lua_files[i] << std::endl;
    }
    
    log_util::print_colored(COLOR_TERMINAL_YELLOW,"\n    FLAGS enabled:\n");

    #ifdef _DEBUG 
    log_util::print_colored(COLOR_TERMINAL_GREEN,"    DEBUG VERBOSE................: YES\n");
    #else
    log_util::print_colored(COLOR_TERMINAL_RED,  "    DEBUG VERBOSE................: NO\n");
    #endif

    #ifdef USE_OPENGL_ES
    log_util::print_colored(COLOR_TERMINAL_GREEN,"    OPENGL ES....................: YES\n");
    #else
    log_util::print_colored(COLOR_TERMINAL_RED,  "    OPENGL ES....................: NO\n");
    #endif

    #ifdef USE_VULKAN
    log_util::print_colored(COLOR_TERMINAL_GREEN,"    VULKAN.......................: YES\n");
    #else
    log_util::print_colored(COLOR_TERMINAL_RED,  "    VULKAN.......................: NO\n");
    #endif

    #ifdef USE_DEPRECATED_2_MINOR
    log_util::print_colored(COLOR_TERMINAL_GREEN,"    COMPATIBLE DEPRECATED VERSION: YES\n");
    #else
    log_util::print_colored(COLOR_TERMINAL_RED,  "    COMPATIBLE DEPRECATED VERSION: NO\n");
    #endif
    
    #ifdef USE_EDITOR_FEATURES
    log_util::print_colored(COLOR_TERMINAL_GREEN,"    EDITOR FEATURES..............: YES\n");
    #else
    log_util::print_colored(COLOR_TERMINAL_RED,  "    EDITOR FEATURES..............: NO\n");
    #endif
    
    #ifdef USE_VR
    log_util::print_colored(COLOR_TERMINAL_GREEN,"    CLASS VIRTUAL REALITY........: YES\n");
    #else
    log_util::print_colored(COLOR_TERMINAL_RED,  "    CLASS VIRTUAL REALITY........: NO\n");
    #endif

	std::cout << std::endl << std::endl;

	log_util::print_colored(COLOR_TERMINAL_YELLOW,"For documentation please check at:\n%s\n","https://mbm-documentation.readthedocs.io/en/latest/");
}

#endif











