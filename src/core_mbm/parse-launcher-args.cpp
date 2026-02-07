
#include <parse-launcher-args.hpp>
#include <platform/mismatch-platform.h>
#include <core_mbm/util-interface.h>

#include <string>

enum ARGS_WINDWON
{
    NONE,
    WIDTH_SCREEN,
    HEIGHT_SCREEN,
    EXPECTED_WIDTH_SCREEN,
    EXPECTED_HEIGHT_SCREEN,
    POSITION_X_SCREEN,
    POSITION_Y_SCREEN,
    MAXIMIZED_WINDOW,
    INITIAL_SCENE_LUA,
    NAME_APP,
    ADD_PATH,
    NO_SPLASH,
	NO_BORDER,
    DISABLE_FULL_SCREEN,
    DISABLE_BORDER,
    FULL_SCREEN_CHECKED,
    ENABLE_RESIZE_WINDOW,
    WINDOW_THEME,
    DISABLE_SELECT_MONITOR,
};

PARSE_laucher_ARGS::PARSE_laucher_ARGS(const char** argv, const int pNumArgs)
{
    noSplash = false;
    noBorder = false;
    enableResizeWindow = false;
	maximizedWindow = false;
	enableBorder = true;
    allowFullScreen = true;
    full_screen_checked = true;
    disable_select_monitor = false;
	window_theme = 24;
	positionXWindow = 0;
	positionYWindow = 0;

    
    #if defined _WIN32
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(),&pNumArgs);
    #elif defined __linux__  || defined(__APPLE__)
    /*FILE *f = fopen("/proc/self/cmdline", "r");
    char **argv = NULL;
    if (f) {
        char buffer[4096];
        size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
        fclose(f);
        buffer[len] = '\0';

        
        int argc = 0;
        char *p = buffer;
        while (*p) {
            argv = realloc(argv, (argc + 1) * sizeof(char *));
            argv[argc++] = p;
            p += strlen(p) + 1;
        }
        argv = realloc(argv, (argc + 1) * sizeof(char *));
        argv[argc] = NULL;

        // Now argv contains the parsed arguments
    }*/


    #endif
    if(argv != nullptr)
    {
        parserArgs(argv, pNumArgs);
    }

}

void PARSE_laucher_ARGS::parserArgs(const char** argv, const int pNumArgs)
{
    #if defined _DEBUG || defined DEBUG || defined _DEBUG_
    // Debug: print all arguments
    for (int dbg_i = 0; dbg_i < pNumArgs; ++dbg_i)
    {
        printf("DEBUG argv[%d] = %s\n", dbg_i, argv[dbg_i]);
    }
    fflush(stdout);
    #endif
    
    ARGS_WINDWON nextArg = NONE;
    ARGS_WINDWON lastArg = NONE;
            
    for (unsigned int i = 0; i < static_cast<unsigned int>(pNumArgs); ++i)
    {
        #if defined _WIN32
        char buffer[1024] = "";
        const char* arg = util::toChar(argv[i],buffer);
        #elif defined __linux__  || defined(__APPLE__)
        const char* arg = argv[i];
        #endif
        if (strcasecmp(arg, "-w") == 0 || strcasecmp(arg, "-width") == 0 || strcasecmp(arg, "--width") == 0)
            nextArg = WIDTH_SCREEN;
        else if (strcasecmp(arg, "-ew") == 0 || strcasecmp(arg, "-expectedwidth") == 0 || strcasecmp(arg, "--expectedwidth") == 0)
            nextArg = EXPECTED_WIDTH_SCREEN;
        else if (strcasecmp(arg, "-windowtheme") == 0 || strcasecmp(arg, "--windowtheme") == 0)
            nextArg = WINDOW_THEME;
        else if (strcasecmp(arg, "-disable_select_monitor") == 0 || strcasecmp(arg, "--disable_select_monitor") == 0)
        {
            this->disable_select_monitor = true;
            nextArg = DISABLE_SELECT_MONITOR;
        }
        else if (strcasecmp(arg, "-disable_border") == 0 || strcasecmp(arg, "--disable_border") == 0)
        {
            this->enableBorder= false;
            nextArg = DISABLE_BORDER;
        }
        else if (strcasecmp(arg, "--nosplash") == 0 || strcasecmp(arg, "-nosplash") == 0)
        {
            nextArg        = NONE;
            this->noSplash = true;
        }
        else if (strcasecmp(arg, "--disablefullscreen") == 0 || strcasecmp(arg, "-disablefullscreen") == 0)
        {
            nextArg        = DISABLE_FULL_SCREEN;
            this->allowFullScreen = false;
        }
        else if (strcasecmp(arg, "--disablefullscreenchecked") == 0 || strcasecmp(arg, "-disablefullscreenchecked") == 0)
        {
            nextArg        = FULL_SCREEN_CHECKED;
            this->full_screen_checked = false;
        }
        else if (strcasecmp(arg, "-h") == 0 || strcasecmp(arg, "-height") == 0 || strcasecmp(arg, "--height") == 0)
            nextArg = HEIGHT_SCREEN;
        else if (strcasecmp(arg, "-eh") == 0 || strcasecmp(arg, "-expectedheight") == 0 || strcasecmp(arg, "--expectedheight") == 0)
            nextArg = EXPECTED_HEIGHT_SCREEN;
        else if (strcasecmp(arg, "--posx") == 0 || strcasecmp(arg, "-x") == 0 || strcasecmp(arg, "-posx") == 0)
            nextArg = POSITION_X_SCREEN;
        else if (strcasecmp(arg, "--posy") == 0 || strcasecmp(arg, "-y") == 0 || strcasecmp(arg, "-posy") == 0)
            nextArg = POSITION_Y_SCREEN;
        else if (strcasecmp(arg, "--maximizedwindow") == 0 || strcasecmp(arg, "-maximizedwindow") == 0)
            nextArg = MAXIMIZED_WINDOW;
        else if (strcasecmp(arg, "--scene") == 0 || strcasecmp(arg, "-s") == 0 || strcasecmp(arg, "-scene") == 0)
            nextArg = INITIAL_SCENE_LUA;
        else if (strcasecmp(arg, "--name") == 0 || strcasecmp(arg, "--n") == 0 || strcasecmp(arg, "-name") == 0)
            nextArg = NAME_APP;
        else if (strcasecmp(arg, "-a") == 0 || strcasecmp(arg, "--addpath") == 0 || strcasecmp(arg, "-addpath") == 0)
            nextArg = ADD_PATH;
        else if (strcasecmp(arg, "-enableResizeWindow") == 0 || strcasecmp(arg, "--enableResizeWindow") == 0)
            nextArg = ENABLE_RESIZE_WINDOW;
		else if (strcasecmp(arg, "--noborder") == 0 || strcasecmp(arg, "-noborder") == 0)
		{
			nextArg = NONE;  // No value expected, don't consume next arg
			noBorder = true;
		}
        else if(i == 0)//first arg must be the executable name (just ignore)
        {
            nextArg        = NONE;
        }
        else
        {
            std::string the_arg(arg);
            if (the_arg.find('=') != std::string::npos)
                nextArg = NONE;
            
            // Check if this is a .lua file FIRST, before processing other cases
            // This allows .lua files to be passed anywhere in the argument list
            bool is_lua_file = false;
            {
                std::vector<std::string> result;
                util::split(result, arg, '.');
                if (result.size() >= 2 && result[result.size() - 1].compare("lua") == 0)
                {
                    if (this->fileNameInitialLua.compare("main.lua") == 0 || this->fileNameInitialLua.size() == 0)
                    {
                        this->fileNameInitialLua = the_arg;
                    }
                    is_lua_file = true;
                    nextArg = NONE;
                    lastArg = NONE;
                }
            }
            
            if (!is_lua_file)
            {
            switch (nextArg)
            {
                case NONE:
                {
                    switch (lastArg)
                    {
                        case NONE:
                        {
                            nextArg = NONE;
                            nextArg = NONE;
                        }
                        break;
                        case NO_SPLASH:
                        {
                            this->noSplash = ((unsigned int)std::atoi(arg)) ? true : false;
                            nextArg        = NO_SPLASH;
                        }
                        break;
						case NO_BORDER:
						{
							noBorder = true;
							nextArg = NONE;
						}
						break;
                        case ENABLE_RESIZE_WINDOW:
                        {
                            enableResizeWindow = ((unsigned int)std::atoi(arg)) ? true : false;
                            nextArg = NONE;
                        }
                        break;
                        case WIDTH_SCREEN:
                        {
                            auto newW = (unsigned int)std::atoi(arg);
                            if (newW > 0)
                                this->width_list.push_back(newW);
                            nextArg               = WIDTH_SCREEN;
                        }
                        break;
                        case HEIGHT_SCREEN:
                        {
                            auto newH = (unsigned int)std::atoi(arg);
                            if (newH > 0)
                                this->height_list.push_back(newH);
                            nextArg                = HEIGHT_SCREEN;
                        }
                        break;
                        case EXPECTED_WIDTH_SCREEN:
                        {
                            int newW = std::atoi(arg);
                            if (newW > 0)
                            {
                                expected_width_list.push_back(newW);
                            }
                            nextArg               = EXPECTED_WIDTH_SCREEN;
                        }
                        break;
                        case EXPECTED_HEIGHT_SCREEN:
                        {
                            int newH = std::atoi(arg);
                            if (newH > 0)
                            {
                                expected_height_list.push_back(newH);
                            }
                            nextArg                = EXPECTED_HEIGHT_SCREEN;
                        }
                        break;
                        case POSITION_X_SCREEN:
                        {
                            this->positionXWindow = (unsigned int)std::atoi(arg);
                            nextArg               = POSITION_X_SCREEN;
                        }
                        break;
                        case POSITION_Y_SCREEN:
                        {
                            this->positionYWindow = (unsigned int)std::atoi(arg);
                            nextArg               = POSITION_Y_SCREEN;
                        }
                        break;
                        case MAXIMIZED_WINDOW:
                        {
                            this->maximizedWindow = std::atoi(arg) ? true : false;
                            nextArg               = MAXIMIZED_WINDOW;
                        }
                        break;
                        case INITIAL_SCENE_LUA:
                        {
                            this->fileNameInitialLua = the_arg;
                            nextArg                  = INITIAL_SCENE_LUA;
                        }
                        break;
                        case NAME_APP: 
                        {   
                            this->nameAplication = the_arg;
                        }
                        break;
                        case ADD_PATH: { util::addPath(arg);}
                        break;
                        case WINDOW_THEME:
                        {
                            this->window_theme = std::atoi(arg);
                        }
                        break;
                        default:
                        {
                            nextArg = NONE;
                            WARN_LOG("\nArgument: %s %s", arg, " ignored\r\n");
                        }
                        break;
                    }
                }
                break;
                case NO_SPLASH: { this->noSplash = ((unsigned int)std::atoi(arg)) ? true : false;}
                break;
				case NO_BORDER:
				{
					noBorder = true;
					nextArg = NONE;
				}
				break;
                case ENABLE_RESIZE_WINDOW:
                {
                    enableResizeWindow = ((unsigned int)std::atoi(arg)) ? true : false;
                    nextArg = NONE;
                }
                break;
                case WIDTH_SCREEN:
                {
                    auto newW = (unsigned int)std::atoi(arg);
                    if (newW > 0)
                        width_list.push_back(newW);
                }
                break;
                case HEIGHT_SCREEN:
                {
                    auto newH = (unsigned int)std::atoi(arg);
                    if (newH > 0)
                        height_list.push_back(newH);
                }
                break;
                case EXPECTED_WIDTH_SCREEN:
                {
                    auto newW = (unsigned int)std::atoi(arg);
                    if (newW > 0)
                    {
                        expected_width_list.push_back(newW);
                    }
                }
                break;
                case EXPECTED_HEIGHT_SCREEN:
                {
                    auto newH = (unsigned int)std::atoi(arg);
                    if (newH > 0)
                    {
                        expected_height_list.push_back(newH);
                    }
                }
                break;
                case POSITION_X_SCREEN: { this->positionXWindow = (unsigned int)std::atoi(arg);}
                break;
                case POSITION_Y_SCREEN: { this->positionYWindow = (unsigned int)std::atoi(arg);}
                break;
                case MAXIMIZED_WINDOW: { this->maximizedWindow = std::atoi(arg) ? true : false;}
                break;
                case INITIAL_SCENE_LUA: { this->fileNameInitialLua = the_arg;}
                break;
                case NAME_APP: 
                { 
                    this->nameAplication = the_arg;
                }
                break;
                case ADD_PATH: { util::addPath(arg);}
                break;
                case WINDOW_THEME:
                {
                    this->window_theme = std::atoi(arg);
                }
                break;
                default:
                {
                    nextArg = NONE;
                    WARN_LOG("\nArgument: %s %s", arg, " ignored\r\n");
                }
                break;
            }
            } // end if (!is_lua_file)
            if (nextArg != NONE)
                lastArg = nextArg;
            nextArg     = NONE;
        }
    }
}