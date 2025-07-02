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

#ifndef MANAGER_AUDIO_H
#define MANAGER_AUDIO_H

#include "core-exports.h"
#include "audio-interface.h"
#include <string>
#include <vector>

#if defined(AUDIO_ENGINE_AUDIERE)
	#include <audiere-1.9.4/src/audiere.h>
	#if defined(_WIN32)
		#pragma comment(lib, "audiere.lib")
	#endif
#elif defined(AUDIO_ENGINE_PORT_AUDIO)
	#include <pa-wave.h>
	#include <memory>
#elif defined(AUDIO_ENGINE_DIRECT_SOUND_8)
	#include <dsound.h>
	#include <mmreg.h>
	#include <WAVE.h>
	#include <memory>
#elif defined(AUDIO_X_AUDIO_2) // XAudio2 for Windows + Xbox
	#error XAudio2 for Windows + Xbox NOT implemented yet

#endif


namespace mbm
{
	class AUDIO : public AUDIO_INTERFACE
	{
		friend class AUDIO_MANAGER;
		typedef void(*OnEndStreamCallBack)(AUDIO*);
    protected:
        AUDIO(const int newIdScene);
		bool load(const char *filenameSound, const bool loop, const bool inMemory);
	public:
        API_IMPL virtual ~AUDIO();
        API_IMPL bool setVolume(const float volume) override;
        API_IMPL bool setPan(const float pan);
        API_IMPL bool pause() override;
        API_IMPL bool resume() override;
        API_IMPL bool stop();
        API_IMPL bool setPitch(const float pitch); // range from 0.5 to 2.0.  default is 1.0. 
        API_IMPL bool play(const bool loop = false);
        API_IMPL bool isPlaying();
        API_IMPL bool isPaused();
        API_IMPL float getVolume();
        API_IMPL float getPan();
        API_IMPL float getPitch();
        API_IMPL int getLength();
        API_IMPL bool reset();
		API_IMPL bool isLoaded()override;
        API_IMPL bool setPosition(const int pos);
		API_IMPL void setOnEndstream(OnEndStreamCallBack ptrOnEndStreamCallBack);
		API_IMPL OnEndStreamCallBack getOnEndstream() const;
		API_IMPL const char* getFileName() const noexcept override;
        OnEndStreamCallBack onEndStreamCallBack;
	#if defined(AUDIO_ENGINE_ANDROID_JNI) //  AUDIO_ENGINE_ANDROID_JNI ------------------------------------------------------
    private:
        bool log(const char *erro, const char *arg1, const int line);
        bool onNew_AudioJniEngine(int *retIndexJni);
    public:
    #elif defined(AUDIO_ENGINE_AUDIERE) //  AUDIO_ENGINE_AUDIERE ------------------------------------------------------------
		audiere::OutputStreamPtr sound;
	#elif defined (AUDIO_ENGINE_PORT_AUDIO) //  AUDIO_ENGINE_PORT_AUDIO -----------------------------------------------------
		std::unique_ptr<PA_WAVE> pa_wave;
	#elif defined(AUDIO_ENGINE_DIRECT_SOUND_8)//  AUDIO_ENGINE_DIRECT_SOUND_8 -----------------------------------------------
		bool update();
	private:
		bool fillBufferWithSound(void * buffer,const size_t iSizeBuffer,const bool bRepeatWavIfBufferLarger,bool & bEndOfStream);
		bool rewindAndfillBufferWithSilence();
		bool dFirstBuffer;
		std::unique_ptr<WaveFile> wave_reader;
		LPDIRECTSOUNDBUFFER direct_sound_buffer;
		bool restoreBuffer(bool & pbWasRestored);
		DWORD dwDataWaveLength;
		DWORD dwDirectSoundBufferSize;
		DWORD dwCurrentOffsetBuffer;
		DWORD dwSizeOneBuffer;
		bool bLoop;
	#endif

	protected:
		std::string fileName;
    };

	class AUDIO_MANAGER: public AUDIO_MANAGER_INTERFACE
	{
	protected:
		API_IMPL AUDIO_MANAGER();
		static AUDIO_MANAGER* instance;
	public:
		API_IMPL ~AUDIO_MANAGER();
		API_IMPL static AUDIO_MANAGER* getInstance();
		API_IMPL AUDIO* load(const char *fileNameSound, const bool loop, const bool inMemory);
		API_IMPL static void destroy(AUDIO* that);
		API_IMPL static void releaseStaticInstance();
		API_IMPL void release()override;
		API_IMPL void stopAll()override;
		API_IMPL void setPersist(AUDIO* audio, bool bValue);
		bool pauseAudioOnPauseGame;
	protected:
		void update(CORE_MANAGER* coreManager, const int idScene) override;
		void pauseAll(const int idScene) override;
		void resumeAll(const int idScene) override;
	private:
		std::vector<AUDIO*> audios;
		std::vector<AUDIO*> audiosToDelete;
	public:
#if defined(AUDIO_ENGINE_AUDIERE)
		class STOP_AUDIERE : public audiere::RefImplementation<audiere::StopCallback>
		{
			void ADR_CALL streamStopped(audiere::StopEvent *eventStoped);
		};

		static audiere::AudioDevicePtr	audioDevice;
#elif defined(AUDIO_ENGINE_ANDROID_JNI)
        void streamStopped(const int indexJNI)override;
#elif defined(AUDIO_ENGINE_DIRECT_SOUND_8)
		LPDIRECTSOUND8		m_directSound; //for DSBCAPS_CTRL3D , DSBCAPS_PRIMARYBUFFER  must be present and must be used with LPDIRECTSOUND instead of LPDIRECTSOUND8
#endif
	};
}

#endif
