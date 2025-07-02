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

#if !defined(AUDIO_ENGINE_AUDIERE)
    #error attempt to use AUDIERE without define AUDIO_ENGINE_AUDIERE
#endif

using namespace audiere;
namespace mbm
{
	AUDIO::AUDIO(const int newIdScene):AUDIO_INTERFACE(newIdScene),
    onEndStreamCallBack(nullptr)
    {}

    AUDIO::~AUDIO()
    {}

    bool AUDIO::setVolume(const float volume)
    {
		if(sound)
		{
			sound->setVolume(volume);
			return true;
		}
		return false;
    }
    
    bool AUDIO::setPan(const float pan)
    {
		if (sound)
		{
			sound->setPan(pan);
			return true;
		}
		return false;
    }
    
    bool AUDIO::pause()
    {
		if (sound)
		{
            if(sound->isPlaying())
				state = AUDIO_PLAYING;
			else
				state = AUDIO_STOPPED;
            sound->stop();
			return true;
		}
		return false;
    }
    
    bool AUDIO::resume()
    {
        if (sound && state == AUDIO_PLAYING)
		{
			sound->play();
			return true;
		}
		return false;
    }
    
    bool AUDIO::stop()
    {
        state = AUDIO_STOPPED;
		if (sound)
		{
			sound->stop();
            sound->setPosition(0);
			return true;
		}
		return false;
    }
    
    bool AUDIO::setPitch(const float pitch) // range from 0.5 to 2.0.  default is 1.0.
    {
		if (sound)
		{
			sound->setPitchShift(pitch);
			return true;
		}
		return false;
    }
    
    bool AUDIO::load(const char *filenameSound, const bool loop, const bool inMemory)
    {
		if (this->isLoaded())
			return true;
        if (filenameSound == nullptr)
		{
			ERROR_LOG("file name is [null] ");
            return false;
		}
        
        if (AUDIO_MANAGER::audioDevice)
        {
            const char *    cfileName = util::getFullPath(filenameSound,nullptr);
            sound    = OpenSound(AUDIO_MANAGER::audioDevice, cfileName, !inMemory);
            if (!sound)
			{
				ERROR_LOG("file [%s] not found", filenameSound);
                return false;
			}
            sound->setRepeat(loop);
			this->fileName = util::getBaseName(cfileName);
            return true;
        }
		PRINT_IF_DEBUG( "audioDevice is null");
        return false;
    }
    
    bool AUDIO::play(const bool loop)
    {
		if(sound)
		{
			sound->play();
			sound->setRepeat(loop);
			state = AUDIO_PLAYING;
			return true;
		}
		else
		{
			state = AUDIO_STOPPED;
			PRINT_IF_DEBUG("sound not loaded");
			return false;
		}
    }
    
    bool AUDIO::isPlaying()
    {
		if(sound && sound->isPlaying())
            state = AUDIO_PLAYING;
        else
            state = AUDIO_STOPPED;
        return state == AUDIO_PLAYING;
    }
    
    bool AUDIO::isPaused()
    {
		return sound && (sound->isPlaying() == false);
    }
    
    float AUDIO::getVolume()
    {
		if(sound)
			return sound->getVolume();
        return 0.0f;
    }
    
    float AUDIO::getPan()
    {
		if (sound)
			return sound->getPan();
		return 0.0f;
    }
    
    float AUDIO::getPitch()
    {
		if (sound)
			return sound->getPitchShift();
        return 0.5f;
    }
    
    int AUDIO::getLength()
    {
		if (sound)
			return sound->getLength();
		return 0;
    }

    bool AUDIO::reset() 
    {
		if (sound)
		{
			sound->reset();
			state = AUDIO_PLAYING;
			return true;
		}
		return false;
    }

    bool AUDIO::setPosition(const int pos)
    {
		if (sound)
		{
			sound->setPosition(pos);
			return true;
		}
		return false;
    }
    
    void AUDIO::setOnEndstream(OnEndStreamCallBack ptrOnEndStreamCallBack)
    {
        this->onEndStreamCallBack = ptrOnEndStreamCallBack;
    }
    
	AUDIO::OnEndStreamCallBack AUDIO::getOnEndstream() const
    {
        return this->onEndStreamCallBack;
    }

    bool AUDIO::isLoaded()
	{
		return AUDIO_MANAGER::audioDevice && this->sound;
	}

	const char* AUDIO_ENGINE_version()
	{
		static std::string version;
		version = "Audiere ";
		version += audiere::GetVersion();
		return version.c_str();
	}

    const char* AUDIO::getFileName() const noexcept
    {
        return this->fileName.c_str();
    }

    audiere::AudioDevicePtr AUDIO_MANAGER::audioDevice = nullptr;
}
