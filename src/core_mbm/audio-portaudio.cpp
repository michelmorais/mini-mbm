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

#if defined(AUDIO_ENGINE_PORT_AUDIO)

#if defined(_WIN32) && !defined(_WIN64)
    #pragma comment(lib, "portaudio_x86.lib")
#endif



#include <audio.h>
#include <device.h>
#include <core-manager.h>
#include <util-interface.h>
#include <pa-wave.h>
#include <pa-ogg.h>
#include <string>
#include <cstring>


namespace mbm
{
    struct AUDIO::BackendData
    {
        std::unique_ptr<PA_INTERFACE> pa_audio;
    };

    AUDIO::AUDIO(const int newIdScene) : AUDIO_INTERFACE(newIdScene),
        onEndStreamCallBack(nullptr),
        backend(std::make_unique<BackendData>())
    {}

    AUDIO::~AUDIO() = default;

    bool AUDIO::load(const char* fileName, const bool bLoop, const bool inMemory)
    {
        bool found = false;
        const char* fullPath = util::getFullPath(fileName, &found);
        if (!found || !fullPath)
        {
            ERROR_LOG("file not found: %s", fileName ? fileName : "null");
            return false;
        }

        const std::string path(fullPath);

        // Determine format by extension (case-insensitive last 4 chars)
        const size_t dotPos = path.rfind('.');
        bool isOgg = false;
        if (dotPos != std::string::npos)
        {
            std::string ext = path.substr(dotPos + 1);
            for (char& c : ext) c = static_cast<char>(c | 0x20); // to lower
            isOgg = (ext == "ogg" || ext == "oga");
        }

        bool ret = false;
        if (isOgg)
        {
            auto ogg = std::make_unique<PA_OGG>();
            ret = ogg->load(path);
            if (ret)
            {
                ogg->setLoop(bLoop);
                this->backend->pa_audio = std::move(ogg);
            }
        }
        else
        {
            auto wav = std::make_unique<PA_WAVE>();
            ret = wav->load(path, inMemory);
            if (ret)
            {
                wav->setLoop(bLoop);
                this->backend->pa_audio = std::move(wav);
            }
        }

        if (!ret)
            this->backend->pa_audio.reset();
        else
            this->fileName = path;

        return ret;
    }

    bool AUDIO::play(const bool bLoop)
    {
        if (!this->backend->pa_audio) return false;
        if (this->backend->pa_audio->play(bLoop))
        {
            state = AUDIO_PLAYING;
            return true;
        }
        return false;
    }

    bool AUDIO::stop()
    {
        if (!this->backend->pa_audio) return false;
        if (this->backend->pa_audio->stop())
        {
            state = AUDIO_STOPPED;
            return true;
        }
        return false;
    }

    bool AUDIO::pause()
    {
        if (!this->backend->pa_audio) return false;
        if (this->backend->pa_audio->pause())
        {
            state = AUDIO_PAUSED;
            return true;
        }
        return false;
    }

    bool AUDIO::resume()
    {
        if (!this->backend->pa_audio || state != AUDIO_PAUSED) return false;
        if (this->backend->pa_audio->start())
        {
            state = AUDIO_PLAYING;
            return true;
        }
        return false;
    }

    bool AUDIO::setVolume(const float volume)
    {
        if (!this->backend->pa_audio) return false;
        this->backend->pa_audio->setVolume(static_cast<double>(volume));
        return true;
    }

    bool AUDIO::setPan(const float pan)
    {
        if (!this->backend->pa_audio) return false;
        this->backend->pa_audio->setPan(static_cast<double>(pan));
        return true;
    }

    bool AUDIO::setPitch(const float)
    {
        INFO_LOG("AUDIO::setPitch not supported by the PortAudio backend (resampling required)");
        return false;
    }

    bool AUDIO::setPosition(const int positionMs)
    {
        if (!this->backend->pa_audio) return false;
        const int len = this->backend->pa_audio->getLength();
        if (len <= 0) return false;
        const double pos = static_cast<double>(positionMs) / static_cast<double>(len);
        this->backend->pa_audio->setPosition(pos);
        return true;
    }

    bool AUDIO::isPlaying()
    {
        if (!this->backend->pa_audio) return false;
        // m_finished is polled in AUDIO_MANAGER::update(); just query the stream state here
        if (state != AUDIO_PLAYING) return false;
        if (!this->backend->pa_audio->isPlaying() && !this->backend->pa_audio->isPaused())
        {
            state = AUDIO_STOPPED;
            return false;
        }
        return !this->backend->pa_audio->isPaused();
    }

    bool AUDIO::isPaused()
    {
        return this->backend->pa_audio && this->backend->pa_audio->isPaused();
    }

    float AUDIO::getVolume()
    {
        return this->backend->pa_audio ? static_cast<float>(this->backend->pa_audio->getVolume()) : 0.0f;
    }

    float AUDIO::getPan()
    {
        return this->backend->pa_audio ? static_cast<float>(this->backend->pa_audio->getPan()) : 0.0f;
    }

    float AUDIO::getPitch()
    {
        return 1.0f;  // pitch not supported; return neutral value
    }

    int AUDIO::getLength()
    {
        return this->backend->pa_audio ? this->backend->pa_audio->getLength() : 0;
    }

    bool AUDIO::reset()
    {
        if (!this->backend->pa_audio) return false;
        this->backend->pa_audio->setPosition(0.0);
        return true;
    }

    bool AUDIO::isLoaded()
    {
        return this->backend->pa_audio != nullptr;
    }

    void AUDIO::setOnEndstream(OnEndStreamCallBack cb)
    {
        this->onEndStreamCallBack = cb;
        // The callback fires from AUDIO_MANAGER::update() on the main thread
        // when pa_audio->isFinished() becomes true (set by the PortAudio callback thread).
    }

    AUDIO::OnEndStreamCallBack AUDIO::getOnEndstream() const
    {
        return this->onEndStreamCallBack;
    }

    const char* AUDIO::getFileName() const noexcept
    {
        return this->fileName.c_str();
    }

    bool AUDIO::updateBackend()
    {
        if (this->backend->pa_audio && this->backend->pa_audio->isFinished())
        {
            this->backend->pa_audio->clearFinished();
            this->state = mbm::STATE_AUDIO::AUDIO_STOPPED;
            if (this->onEndStreamCallBack)
                this->onEndStreamCallBack(this);
            return true;
        }
        return false;
    }

    void AUDIO_MANAGER::initializeBackend()
    {
    }

    void AUDIO_MANAGER::finalizeBackend()
    {
    }

    void AUDIO_MANAGER::updateBackend()
    {
        updateManagedAudiosBackend();
    }

    const char* AUDIO_ENGINE_version()
    {
        return PA_version();
    }
}

#endif
