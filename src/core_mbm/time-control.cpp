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

#include <time-control.h>
#include <ctime>
#include <cstring>
#include "util-interface.h"

namespace mbm
{

    TIME_CONTROL::TIME_CONTROL() noexcept
    {
        fps               = 0;
        delta             = 0;
        fakeFps           = 0;
        fakeFpsCicle      = 0;
        totalRunTimer     = 0.0f;
        initialTime       = 0.0f;
        real_fps          = 120;
        currentRate = 0, countFps = 0;
        speed = 1.0f;
        memset(&realLastTime, 0, sizeof(sys_time_t));
        system_time(&realLastTime);
#if defined ANDROID || (!defined(_WIN32) && !defined(_WIN64))
        realLastTime.tv_sec += 2;
#else
        realLastTime.time += 2;
#endif
    }

    uint32_t TIME_CONTROL::addTimer(const float value) // time in seconds
    {
        aditionalTimes.push_back(value);
        return static_cast<uint32_t>(aditionalTimes.size() - 1);
    }
    
    float TIME_CONTROL::getTimer(const uint32_t index) const // time inseconds
    {
        if (index >= static_cast<uint32_t>(aditionalTimes.size()))
            return 0.0f;
        return aditionalTimes[index];
    }
    
    bool TIME_CONTROL::setTimer(const uint32_t index, const float newTime) // time in seconds
    {
        if (index >= aditionalTimes.size())
            return false;
        aditionalTimes[index] = newTime;
        return true;
    }
    
    void TIME_CONTROL::pauseTimer()
    {
        this->speed = 0.0f;
    }
    
    void TIME_CONTROL::resumeTimer()
    {
        if (this->speed != 1.0f)
        {
            memset(&realLastTime, 0, sizeof(sys_time_t));
            system_time(&realLastTime);
#if defined ANDROID || (!defined(_WIN32) && !defined(_WIN64))
            realLastTime.tv_sec += 2;
#else
            realLastTime.time += 2;
#endif
            this->getTimeTick();
        }
        this->speed = 1.0f;
    }
    
    const char * TIME_CONTROL::getTimeRunTimeAsString()
    {
        static char strTimeHourMinSec[255];
        strTimeHourMinSec[0] = 0;
        int   hour           = 0;
        int   min            = 0;
        int   sec            = 0;
        float second         = this->totalRunTimer;
        if (second > 0.0f)
        {
            if ((second / 3600.0f) >= 1.0f) // one hour
            {
                hour = static_cast<int>(second / 3600.0f);
                second -= (hour * 3600.0f);
            }
            if (second >= 60.0f) // 1 minute
            {
                min = static_cast<int>(second / 60.0f);
                second -= (min * 60.0f);
            }
            sec = static_cast<int>(second);
        }
        sprintf(strTimeHourMinSec, "%02d:%02d:%02d", hour, min, sec);
        return strTimeHourMinSec;
    }
    
    const char * TIME_CONTROL::getTimeHourMinSecond(float second)
    {
        static char strTimeHourMinSec[255];
        strTimeHourMinSec[0] = 0;
        int hour             = 0;
        int min              = 0;
        int sec              = 0;
        if (second > 0.0f)
        {
            if ((second / 360.0f) >= 1.0f) // one hour
            {
                hour = static_cast<int>(second / 3600.0f);
                second -= (hour * 3600.0f);
            }
            if (second >= 60.0f) // 1 minute
            {
                min = static_cast<int>(second / 60.0f);
                second -= (min * 60.0f);
            }
            sec = static_cast<int>(second);
        }
        sprintf(strTimeHourMinSec, "%02d:%02d:%02d", hour, min, sec);
        return strTimeHourMinSec;
    }
    
    float TIME_CONTROL::getTotalRunTimer() const
    {
        return this->totalRunTimer;
    }
    
    void TIME_CONTROL::clearAdditionalTimers()
    {
        this->aditionalTimes.clear();
    }

    void TIME_CONTROL::setFakeFps(uint32_t numCicles,uint32_t _fps)
    {
        if(_fps < 1)
        {
			_fps = 1;
            ERROR_AT(__LINE__,__FILE__,"unable to set FPS less then 1");
        }
        if(_fps > 300)
        {
			_fps = 300;
            ERROR_AT(__LINE__,__FILE__,"unable to set FPS less greater then  300");
        }
        this->fakeFps         = _fps;
        if(numCicles < _fps)
            numCicles = _fps;
        this->fakeFpsCicle    = numCicles;
    }
    
    void TIME_CONTROL::updateFps()
    {
        const float interval = this->getTimeTick();
        totalRunTimer += interval;
        const float intSpeed    = interval * this->speed;
        const bool  hasInterval = interval < 1.0f;
        if (hasInterval)
        {
            for (float & aditionalTime : aditionalTimes)
            {
                aditionalTime += intSpeed;
            }
        }
        else // quando travamos a thread (as vezes antes de inicializar cenas, prompt caixa de texto mensagens etc)
        // este metodo não é chamado causando uma aceleração nos fps... este else esta resolvendo
        {
            currentRate = 0;
            countFps    = 0;
            initialTime = totalRunTimer;
            fps         = 0.0f;
            delta       = 0.0f;
            return;
        }
        countFps++;
        const float difftimeRun = (totalRunTimer - initialTime);
        if (difftimeRun >= 1.0f)
        {
            currentRate = countFps;
            initialTime = totalRunTimer - (difftimeRun - 1.0f);
            countFps    = 0;
            this->real_fps = static_cast<float>(currentRate);
        }
        this->fps = static_cast<float>(currentRate);
        if (fps != 0.0f)
            delta = 1.0f / fps;
        else
            delta = 0.0f;
        fps *= this->speed;
        delta *= this->speed;
        if(fakeFpsCicle > 0)
        {
            fakeFpsCicle--;
            fps =  static_cast<float>(fakeFps);
            delta = 1.0f / fakeFps;
        }
    }
    
    float TIME_CONTROL::getTimeTick() // Recupera o tempo em milisegundos decorrido do ultimo updateFPS
    {
        sys_time_t realCurrentTime;
        system_time(&realCurrentTime);
        const long long realCurrentTimeMs = time_to_msec(realCurrentTime);
        const long long realLastTimeMs    = time_to_msec(realLastTime);
        const float     realInterval      = (0.001f * static_cast<const float>(realCurrentTimeMs - realLastTimeMs));
        this->realLastTime                = realCurrentTime;
        return realInterval;
    }
    
    void TIME_CONTROL::milliSleep(const uint32_t ms)
    {
#if defined(_WIN32)
        Sleep(ms);
#else
        //
        //  usleep() can be interrupted by signal and there isn't much we can do. We
        //  just loose time in such a case. Instead, use nanosleep() in a loop which
        //  can tell us how much remains per iteration. Loop until we've consumed
        //  the requested time.
        //
        struct timespec ts;
        ts.tv_sec  = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000000;
        while (-1 == ::nanosleep(&ts, &ts) && EINTR == errno)
        {
            /* nop */
        }
#endif
    }
    
}

