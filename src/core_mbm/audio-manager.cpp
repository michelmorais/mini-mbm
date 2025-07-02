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

namespace mbm
{
#if defined(AUDIO_ENGINE_AUDIERE)
    using namespace audiere;
#endif
	AUDIO_MANAGER* AUDIO_MANAGER::getInstance()
	{
		if(AUDIO_MANAGER::instance == nullptr)
			AUDIO_MANAGER::instance = new AUDIO_MANAGER();
		return AUDIO_MANAGER::instance;
	}

	AUDIO_MANAGER::AUDIO_MANAGER():pauseAudioOnPauseGame(true)
	{
		mbm::DEVICE *device = mbm::DEVICE::getInstance();
		#if defined(AUDIO_ENGINE_AUDIERE)
		if (!AUDIO_MANAGER::audioDevice)
			AUDIO_MANAGER::audioDevice = OpenDevice();
		AUDIO_MANAGER::audioDevice->registerCallback(new AUDIO_MANAGER::STOP_AUDIERE());//leak*** :( for some reason if we destroy callback from audiere before detach it (DLL), it crashes. so, we make this leak.
		#elif defined(AUDIO_ENGINE_DIRECT_SOUND_8)
		m_directSound = nullptr;
		if (FAILED(DirectSoundCreate8(nullptr, &m_directSound, nullptr )))
		{
			m_directSound = nullptr;
			ERROR_LOG("Failed calling DirectSoundCreate8");
		}
		else
		{
			HWND hwnd = mbm::DEVICE::getInstance()->window.getHwnd();
			if FAILED(m_directSound->SetCooperativeLevel(hwnd, DSSCL_PRIORITY))
			{
				ERROR_LOG("Failed calling m_directSound->SetCooperativeLevel");
				if(m_directSound!=nullptr)
					m_directSound->Release();
				m_directSound = nullptr;
			}
		}
		#endif
		device->setAudioManagerInterface(this);
	}

	AUDIO_MANAGER::~AUDIO_MANAGER()
	{
		const size_t s = audios.size();
		for (size_t i = 0; i < s; ++i)
		{
			AUDIO* my_audio = audios[i];
			my_audio->stop();
			delete my_audio;
		}
		audios.clear();
		#if defined(AUDIO_ENGINE_DIRECT_SOUND_8)
		if(m_directSound != nullptr)
			m_directSound->Release();
		m_directSound = nullptr;
		#endif
	}

	AUDIO* AUDIO_MANAGER::load(const char *fileNameSound, const bool loop, const bool inMemory)
	{
		if(fileNameSound == nullptr)
			return nullptr;
		#if defined(ANDROID) // if Is ANDROID, we do not copy the sound file to /data/data/com.mini.mbm.<my_game>/files
		const std::string fileName(fileNameSound);
		#else
		bool bFileExist = false;
		const std::string fileName = util::getBaseName(fileNameSound);
        fileNameSound = util::getFullPath(fileNameSound,&bFileExist);
		if(bFileExist == false)
		{
			PRINT_IF_DEBUG("File [%s] not found", fileNameSound);
			return nullptr;
		}
		#endif
		
		mbm::DEVICE *device = mbm::DEVICE::getInstance();
		const int idScene = device->scene ? device->scene->getIdScene() : -1;
		const size_t s1 = audios.size();
		for (size_t i = 0; i < s1; ++i)
		{
			AUDIO* my_audio = audios[i];
			if (my_audio->idScene != idScene && 
				my_audio->fileName.compare(fileName) == 0)
			{
				my_audio->idScene = idScene;//make this sound belongs to this scene
				return my_audio;
			}
		}

		const size_t s2 = audiosToDelete.size();
		for (size_t i = 0; i < s2; ++i)
		{
			AUDIO* my_audio = audiosToDelete[i];
			if (my_audio->idScene != idScene &&
				my_audio->fileName.compare(fileName) == 0)
			{
				my_audio->idScene = idScene;//make this sound belongs to this scene
				PRINT_IF_DEBUG("Resuscitated audio: %s [%p]\n", my_audio->fileName.c_str(), my_audio);
				audiosToDelete.erase(audiosToDelete.begin() + std::vector<AUDIO*>::difference_type(i));
				audios.push_back(my_audio);
				return my_audio;
			}
		}
		auto* my_audio = new AUDIO(idScene);
		if (my_audio->load(fileNameSound, loop, inMemory))
		{
			audios.push_back(my_audio);
			my_audio->fileName = fileName;
			return my_audio;
		}
		else
		{
			PRINT_IF_DEBUG("delete audio from c++ %s\n",my_audio->fileName.c_str());
			delete my_audio;
		}
		return nullptr;
	}

	void AUDIO_MANAGER::pauseAll(const int)
	{
		if(pauseAudioOnPauseGame == true)
		{
			const size_t s = audios.size();
			for (size_t i = 0; i < s; ++i)
			{
				AUDIO* my_audio = audios[i];
				my_audio->pause();
			}
		}
	}
	void AUDIO_MANAGER::resumeAll(const int idScene)
	{
		if(pauseAudioOnPauseGame == true)
		{
			const size_t s = audios.size();
			for (size_t i = 0; i < s; ++i)
			{
				AUDIO* my_audio = audios[i];
				if(my_audio->idScene == idScene)
					my_audio->resume();
			}
		}
	}

	void AUDIO_MANAGER::update(CORE_MANAGER* coreManager,const int idScene)
	{
		if(audiosToDelete.size())
		{
			AUDIO* my_audio = audiosToDelete[0];
			if(my_audio->idScene != idScene && coreManager->existScene(my_audio->idScene) == false )//destroy only if scene do not exist anymore
			{
				if(my_audio->bPersistent)
				{
					audiosToDelete.erase(audiosToDelete.begin());
					audios.push_back(my_audio);
				}
				else
				{
					#if defined DEBUG_AUDIO
					PRINT_IF_DEBUG("Deleting audio [%p] C++:%s\n", my_audio, my_audio->fileName.c_str());
					#endif
					if (my_audio->userData)
					{
						PRINT_IF_DEBUG("Possible error on destroy audio. userData has value [%p] [%s] ", my_audio, my_audio->fileName.c_str());
					}
					else
					{
						audiosToDelete.erase(audiosToDelete.begin());
						delete my_audio;
					}
				}
			}
		}
        #if defined(AUDIO_ENGINE_AUDIERE)
        if(AUDIO_MANAGER::audioDevice)
            AUDIO_MANAGER::audioDevice->update();
        #endif
		#if defined(AUDIO_ENGINE_DIRECT_SOUND_8)
		for (AUDIO* my_audio : AUDIO_MANAGER::instance->audios)
		{
			my_audio->update();
		}
		#endif
	}

	void AUDIO_MANAGER::setPersist(AUDIO* audio, bool bValue)
	{
		if(audio)
			audio->bPersistent = bValue;
		if(bValue)
		{
			const size_t s2 = audiosToDelete.size();
			for (size_t i = 0; i < s2; ++i)
			{
				AUDIO* my_audio = audiosToDelete[i];
				if (my_audio == audio)
				{
#if defined DEBUG_AUDIO
					PRINT_IF_DEBUG("setPersist, Resuscitated audio: %s [%p]\n", my_audio->fileName.c_str(), my_audio);
#endif
					audiosToDelete.erase(audiosToDelete.begin() + std::vector<AUDIO*>::difference_type(i));
					audios.push_back(my_audio);
					break;
				}
			}
		}
		else if(audio)
		{
			auto scene = mbm::DEVICE::getInstance()->scene;
			if(audio->idScene != scene->getIdScene())
				this->destroy(audio);
		}
	}

	void AUDIO_MANAGER::destroy(AUDIO* that)
	{
        if (AUDIO_MANAGER::instance && that)
		{
			if(that->bPersistent == true)//just stop
			{
				that->stop();
			}
			else
			{
				const size_t s = AUDIO_MANAGER::instance->audios.size();
				for (size_t i = 0; i < s; ++i)
				{
					AUDIO* my_audio = AUDIO_MANAGER::instance->audios[i];
					if (my_audio == that)
					{
						AUDIO_MANAGER::instance->audios.erase(AUDIO_MANAGER::instance->audios.begin() + std::vector<AUDIO *>::difference_type(i));
						AUDIO_MANAGER::instance->audiosToDelete.push_back(that);
						that->stop();
						break;
					}
				}
			}
		}
	}
	
	void AUDIO_MANAGER::release()
	{
        #if defined(AUDIO_ENGINE_AUDIERE)
        AUDIO_MANAGER::audioDevice->update();
        #endif

		for (auto my_audio : audios)
		{
			my_audio->stop();
			delete my_audio;
		}
		audios.clear();
		
		for (auto my_audio : audiosToDelete)
		{
			my_audio->stop();
			if(my_audio->userData)
			{
				PRINT_IF_DEBUG("Possible error on destroy audio [%p] [%s] ", my_audio, my_audio->fileName.c_str());
				return;
			}
			else
			{
				delete my_audio;
			}
		}
		audiosToDelete.clear();
		
        #if defined(AUDIO_ENGINE_AUDIERE)
        AUDIO_MANAGER::audioDevice->clearCallbacks();
        AUDIO_MANAGER::audioDevice->update();
        AUDIO_MANAGER::audioDevice = nullptr;
        #endif
	}

    void AUDIO_MANAGER::stopAll()
	{
        #if defined(AUDIO_ENGINE_AUDIERE)
        AUDIO_MANAGER::audioDevice->update();
        #endif

		for (auto my_audio : audios)
		{
			my_audio->stop();
		}
		
		for (auto my_audio : audiosToDelete)
		{
			my_audio->stop();
		}
		
        #if defined(AUDIO_ENGINE_AUDIERE)
        AUDIO_MANAGER::audioDevice->update();
        #endif
	}

	#if defined(AUDIO_ENGINE_AUDIERE)
	void AUDIO_MANAGER::STOP_AUDIERE::streamStopped(StopEvent *eventStoped)
	{
		auto reason = eventStoped->getReason();
		if (StopEvent::STREAM_ENDED == reason && AUDIO_MANAGER::instance)
		{
			auto sound = eventStoped->getOutputStream();
			bool bAudioFound = false;
			for (size_t i = 0; i < AUDIO_MANAGER::instance->audios.size(); ++i)
			{
				AUDIO* my_audio = AUDIO_MANAGER::instance->audios[i];
				if (my_audio->sound == sound)
				{
					if (reason == audiere::StopEvent::STREAM_ENDED)
					{
						my_audio->state = mbm::STATE_AUDIO::AUDIO_STOPPED;
					}
					if (my_audio->onEndStreamCallBack)
					{
						my_audio->onEndStreamCallBack(my_audio);
					}
					bAudioFound = true;
					break;
				}
			}
			if(bAudioFound == false)
			{
				for (size_t i = 0; i < AUDIO_MANAGER::instance->audiosToDelete.size(); ++i)
				{
					AUDIO* my_audio = AUDIO_MANAGER::instance->audiosToDelete[i];
					if (my_audio->sound == sound)
					{
						if (reason == audiere::StopEvent::STREAM_ENDED)
						{
							my_audio->state = mbm::STATE_AUDIO::AUDIO_STOPPED;
						}
						if (my_audio->onEndStreamCallBack)
						{
							my_audio->onEndStreamCallBack(my_audio);
						}
						break;
					}
				}
			}
		}
	}
    #endif

#if defined(AUDIO_ENGINE_ANDROID_JNI)
    void AUDIO_MANAGER::streamStopped(const int indexJNI)
    {
        for (size_t i = 0; i < audios.size(); ++i)
        {
            AUDIO* my_audio = audios[i];
            if (my_audio->indexJNI == indexJNI)
            {
                if(my_audio->onEndStreamCallBack)
                    my_audio->onEndStreamCallBack(my_audio);
            }
        }
    }
#endif
	void AUDIO_MANAGER::releaseStaticInstance()
	{
		if (AUDIO_MANAGER::instance)
		{
			AUDIO_MANAGER::instance->release();
			delete AUDIO_MANAGER::instance;
			AUDIO_MANAGER::instance = nullptr;
		}
	}

	void releaseAudioManager()
	{
		AUDIO_MANAGER::releaseStaticInstance();
	}

	AUDIO_MANAGER* AUDIO_MANAGER::instance = nullptr;
}