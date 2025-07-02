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

#ifndef AUDIO_INTERFACE__H
#define AUDIO_INTERFACE__H

#include "core-exports.h"

#ifdef _WIN32
    #include <Windows.h>
#endif

namespace mbm 
{
	class CORE_MANAGER;
	class API_IMPL AUDIO_MANAGER_INTERFACE
	{
		public:
		AUDIO_MANAGER_INTERFACE() = default;
		virtual ~AUDIO_MANAGER_INTERFACE() = default;
		virtual void pauseAll(const int idScene) = 0;
		virtual void resumeAll(const int idScene) = 0;
		virtual void stopAll() = 0;
		virtual void update(CORE_MANAGER* coreManager,const int idScene) = 0;
		virtual void release() = 0;
		#ifdef ANDROID
		virtual void streamStopped(const int indexJNI) = 0;
		#endif
	};

    enum STATE_AUDIO : char
    {
        AUDIO_STOPPED,
        AUDIO_PLAYING,
		AUDIO_PAUSED
    };


    class API_IMPL AUDIO_INTERFACE
    {
      public:
		int idScene;
	#ifdef ANDROID
        int indexJNI;
	#endif
        STATE_AUDIO state;
		virtual bool setVolume(const float volume) = 0;
        virtual bool pause() = 0;
        virtual bool resume() = 0;
		virtual bool isLoaded() = 0;
        AUDIO_INTERFACE(const int _idScene) noexcept;
        virtual ~AUDIO_INTERFACE();
        void *userData;
		bool isPersist()const noexcept;
		virtual const char* getFileName() const noexcept = 0;
	  protected:
		bool bPersistent;//keep audio between scenes (default is false)
    };

	void releaseAudioManager();//defined in audio.cpp

	API_IMPL const char* AUDIO_ENGINE_version();
}
#endif
