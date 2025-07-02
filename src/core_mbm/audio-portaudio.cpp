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

#include <audio.h>
#include <device.h>
#include <core-manager.h>
#include <util-interface.h>

#if !defined(AUDIO_ENGINE_PORT_AUDIO)
    #error attempt to use PORT AUDIO without define AUDIO_ENGINE_PORT_AUDIO
#endif

namespace mbm
{
    AUDIO::AUDIO(const int newIdScene):AUDIO_INTERFACE(newIdScene),
    onEndStreamCallBack(nullptr)
    {}

    AUDIO::~AUDIO()
    = default;

    bool AUDIO::setVolume(const float volume) 
    {
        if(this->pa_wave)
        {
		    this->pa_wave->setVolume(volume);
            return true;
        }
        return false;
    }
    bool AUDIO::pause() 
	{
        if(this->pa_wave)
            return this->pa_wave->pause();
        return false;
	}
    bool AUDIO::resume() 
	{
        if(this->pa_wave)
		    return this->pa_wave->start();
        return false;
	}
    bool AUDIO::play(const bool bLoop)
    {
        return this->pa_wave && this->pa_wave->play(bLoop);
    }
    bool AUDIO::load(const char * fileName,const bool bLoop, const bool inMemory)
    {
        bool ret = false;
        const char *    cfileName = util::getFullPath(fileName,&ret);
        if(ret)
        {
            this->pa_wave = std::make_unique<PA_WAVE>();
            const std::string file_Name(cfileName ? cfileName : "");
            ret = this->pa_wave->load(file_Name,inMemory);
            if(ret)
                this->pa_wave->setLoop(bLoop);
            else
                this->pa_wave.reset(nullptr);
        }
        else
        {
            ERROR_LOG("file name not found:%s",cfileName ? cfileName : "null");
        }
        return ret;
    }
    bool AUDIO::stop()
    {
		return this->pa_wave && this->pa_wave->stop();
    }
    bool AUDIO::setPan(const float)
    {
		ERROR_AT(__LINE__,__FILE__,"AUDIO::setPan not implemented");
        return false;
    }
    bool AUDIO::setPitch(const float)
    {
		ERROR_AT(__LINE__,__FILE__,"AUDIO::setPitch not implemented");
        return false;
    }
    bool AUDIO::setPosition(const int position)
    {
        if(this->pa_wave)
        {
		    this->pa_wave->setPosition(position);
            return true;
        }
        return false;
    }
    bool AUDIO::isPlaying()
    {
        return this->pa_wave->isPlaying();
    }
    bool AUDIO::isPaused()
    {
        return this->pa_wave && this->pa_wave->isPlaying() == false;
    }
    float AUDIO::getVolume()
    {
        return this->pa_wave->getVolume();
    }
    float AUDIO::getPan()
    {
        ERROR_AT(__LINE__,__FILE__,"AUDIO::getPan not implemented");
        return false;
    }
    float AUDIO::getPitch()
    {
        ERROR_AT(__LINE__,__FILE__,"AUDIO::getPitch not implemented");
        return false;
    }
    bool AUDIO::reset()
    {
        if(this->pa_wave)
        {
            this->pa_wave->setPosition(0);
            return true;
        }
        return false;
    }
    int AUDIO::getLength()
    {
        ERROR_AT(__LINE__,__FILE__,"AUDIO::getLength not implemented");
        return false;
    }
    
	bool AUDIO::isLoaded()
	{
		return this->pa_wave != nullptr;
	}

    void AUDIO::setOnEndstream(OnEndStreamCallBack ptrOnEndStreamCallBack)
    {
        this->onEndStreamCallBack = ptrOnEndStreamCallBack;
        ERROR_AT(__LINE__,__FILE__,"AUDIO::setOnEndstream not implemented");
    }

    const char* AUDIO_ENGINE_version()
	{
		return PA_version();
	}

    const char* AUDIO::getFileName() const noexcept
    {
        return this->fileName.c_str();
    }
}
