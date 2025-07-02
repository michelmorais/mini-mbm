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

#ifndef TIME_CONTROL_CLASS_H
#define TIME_CONTROL_CLASS_H

#include <ctime>
#include <errno.h>
#include <list>
#include <stdio.h>
#include <vector>
#include <stdint.h>
#include "core-exports.h"

#if !defined(_WIN32) && !defined(_WIN64) // Linux - Unix

    #include <sys/time.h>

    typedef timeval sys_time_t;
    inline void system_time(sys_time_t *t) noexcept
    {
        gettimeofday(t, nullptr);
    }
    inline long long time_to_msec(const sys_time_t &t) noexcept
    {
        return t.tv_sec * 1000LL + t.tv_usec / 1000;
    }
#else // Windows and MinGW
    #include <windows.h>
    #include <sys/timeb.h>

    typedef _timeb sys_time_t;
    inline void system_time(sys_time_t *t) noexcept
    {
        _ftime64_s(t);
    }
    inline long long time_to_msec(const sys_time_t &t) noexcept
    {
        return t.time * 1000LL + t.millitm;
    }
#endif

namespace mbm
{

    class TIME_CONTROL
    {
      public:
        TIME_CONTROL() noexcept;
        float        fps;
        float        real_fps;
        float        delta;// (1.0f / frames per second)
        float        speed;// Speed game (normal:1.0  fast:0.5 slow: 5.0 paused: 0.0f)
        API_IMPL uint32_t addTimer(const float value = 0); // time in seconds
        API_IMPL float getTimer(const uint32_t index) const; // time inseconds
        API_IMPL bool setTimer(const uint32_t index, const float newTime); // time in seconds
        API_IMPL void pauseTimer();
        API_IMPL void resumeTimer();
        API_IMPL const char *getTimeRunTimeAsString();
        API_IMPL const char *getTimeHourMinSecond(float second);
        API_IMPL float getTotalRunTimer() const;
        API_IMPL void clearAdditionalTimers();
        API_IMPL void setFakeFps(uint32_t numCicles,uint32_t fps);

      protected:
        void updateFps();

      private:
        float          totalRunTimer;
        float          initialTime;
        uint32_t       currentRate;
        uint32_t       countFps;
        uint32_t       fakeFps;
        uint32_t       fakeFpsCicle;
        sys_time_t     realLastTime;
        std::vector<float> aditionalTimes;
        float getTimeTick();
        void milliSleep(const uint32_t ms);
    };

}
#endif
