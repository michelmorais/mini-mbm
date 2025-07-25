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
#include <platform/usage-help.h>
#include <util-interface.h>

int main(const int argc,const char **argv)
{
    if (argc == 1 || 
		(argc > 1 && 
		(strcmp(argv[1], "--help") == 0 || 
		(strcmp(argv[1], "help")   == 0) || 
		(strcmp(argv[1], "-help")  == 0))))
    {
        help(util::getBaseName(argv[0]));
    }
    mbm::LUA_MANAGER luaCore(argc, argv);
    log_util::print_colored(COLOR_TERMINAL_YELLOW,"For documentation please check at:\n%s\n","https://mbm-documentation.readthedocs.io/en/latest/");
    if (luaCore.initializeSceneLua(luaCore.noBorder == false))
    {
        const int ret = luaCore.run();
        return mbm::DEVICE::returnCodeApp ? mbm::DEVICE::returnCodeApp : ret;
    }
    else
    {
        return 2;
    }
}
