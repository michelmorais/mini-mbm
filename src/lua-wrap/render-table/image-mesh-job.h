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

#ifndef IMAGE_MESH_JOB_LUA_H
#define IMAGE_MESH_JOB_LUA_H

#include <core_mbm/image-mesh.h>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace mbm
{
    class MESH_DEBUG_LUA;

    // Private binding state. Methods are defined where MESH_DEBUG_LUA is complete.
    struct IMAGE_MESH_JOB_LUA
    {
        enum class STATE { RUNNING, COMPLETED, FAILED };
        std::atomic<STATE> state{STATE::RUNNING};
        std::atomic<bool> cancelled{false};
        std::atomic<float> progress{0};
        std::atomic<const char *> stage{"decode"};
        std::thread worker;
        std::unique_ptr<MESH_DEBUG_LUA> result;
        IMAGE_MESH_OPTIONS options;
        IMAGE_MESH_REPORT report;
        std::string path,side,back,heightImage,error,output;
        std::vector<IMAGE_MESH_POINT> contour;
        std::vector<IMAGE_MESH_DAB> dabs;
        std::vector<IMAGE_MESH_HOLE> holes;
        std::vector<IMAGE_MESH_HEIGHT_AREA> areas;
        std::vector<std::vector<IMAGE_MESH_POINT>> holePoints,areaPoints;
        bool taken=false, mapJob=false, overlay=false;

        IMAGE_MESH_JOB_LUA();
        ~IMAGE_MESH_JOB_LUA();
        void snapshot(const char *source, const IMAGE_MESH_OPTIONS &options);
        void run();
    };
}
#endif
