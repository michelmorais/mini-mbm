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
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef SKELETAL_EXECUTION_POLICY_H
#define SKELETAL_EXECUTION_POLICY_H

#include <core_mbm/shader.h>

namespace mbm::skeletal
{
    struct SKELETAL_EXECUTION_RESOLUTION
    {
        SKELETAL_EXECUTION_PATH resolvedPath = SKELETAL_EXECUTION_PATH::GPU;
        const char *reason = "explicit-gpu";
    };

    inline constexpr SKELETAL_EXECUTION_PATH defaultSkeletalExecutionPath() noexcept
    {
        return SKELETAL_EXECUTION_PATH::AUTO;
    }

    inline SKELETAL_EXECUTION_RESOLUTION resolveSkeletalExecutionPolicy(
        const SKELETAL_EXECUTION_PATH requestedPath,
        const bool meshLoaded,
        const bool gpuSupported,
        const bool cpuAvailable,
        const char *cpuUnavailableReason) noexcept
    {
        if (requestedPath == SKELETAL_EXECUTION_PATH::CPU)
            return {SKELETAL_EXECUTION_PATH::CPU, "explicit-cpu"};
        if (requestedPath == SKELETAL_EXECUTION_PATH::GPU)
            return {SKELETAL_EXECUTION_PATH::GPU, "explicit-gpu"};
        if (!meshLoaded)
            return {SKELETAL_EXECUTION_PATH::GPU, "not-loaded"};
        if (gpuSupported)
            return {SKELETAL_EXECUTION_PATH::GPU, "auto-gpu-supported"};
        if (cpuAvailable)
            return {SKELETAL_EXECUTION_PATH::CPU, "auto-cpu-fallback"};
        return {SKELETAL_EXECUTION_PATH::GPU,
                cpuUnavailableReason ? cpuUnavailableReason : "auto-cpu-unavailable"};
    }
}

#endif
