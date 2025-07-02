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

#if defined(AUDIO_ENGINE_PORT_AUDIO)
    #error attempt to NOT use AUDIO and defining AUDIO_ENGINE_PORT_AUDIO
#endif

#if defined(AUDIO_ENGINE_AUDIERE)
    #error attempt to NOT use AUDIO and defining AUDIO_ENGINE_AUDIERE
#endif

#if defined(AUDIO_ENGINE_ANDROID_JNI)
    #error attempt to NOT use AUDIO and defining AUDIO_ENGINE_ANDROID_JNI
#endif


namespace mbm
{
	AUDIO::AUDIO(const int newIdScene):AUDIO_INTERFACE(newIdScene),
    onEndStreamCallBack(nullptr)
    {
        ERROR_AT(__LINE__,__FILE__,"AUDIO::AUDIO is disabled\nDefine: AUDIO=portaudio or AUDIO=audiere or AUDIO=jni to enable it via cmake.");
    }

    AUDIO::~AUDIO()
    = default;

    bool AUDIO::setVolume(const float) 
    {
		return false;
    }
    bool AUDIO::pause() 
	{
		return false;
	}
    bool AUDIO::resume() 
	{
		return false;
	}
    bool AUDIO::play(const bool )
    {
        return false;
    }
    bool AUDIO::load(const char *,const bool , const bool )
    {
        return false;
    }
    bool AUDIO::stop()
    {
		return false;
    }
    bool AUDIO::setPan(const float)
    {
		return false;
    }
    bool AUDIO::setPitch(const float)
    {
		return false;
    }
    bool AUDIO::setPosition(const int)
    {
		return false;
    }
    bool AUDIO::isPlaying()
    {
        return false;
    }
    bool AUDIO::isPaused()
    {
        return false;
    }
    float AUDIO::getVolume()
    {
        return 0.0f;
    }
    float AUDIO::getPan()
    {
        return 0.0f;
    }
    float AUDIO::getPitch()
    {
        return 0.0f;
    }
    bool AUDIO::reset()
    {
        return false;
    }
    int AUDIO::getLength()
    {
        return 0;
    }
    
	bool AUDIO::isLoaded()
	{
		return false;
	}

    const char* AUDIO::getFileName() const noexcept
    {
        return this->fileName.c_str();
    }

    void AUDIO::setOnEndstream(OnEndStreamCallBack ptrOnEndStreamCallBack)
    {
        this->onEndStreamCallBack = ptrOnEndStreamCallBack;
    }

    const char* AUDIO_ENGINE_version()
	{
		return "Audio engine NULL";
	}
}
