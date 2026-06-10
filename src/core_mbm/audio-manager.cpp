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
#include <scene.h>
#include <core-manager.h>
#include <util-interface.h>
#include <algorithm>

namespace mbm
{
    AUDIO_MANAGER* AUDIO_MANAGER::getInstance()
    {
        if(AUDIO_MANAGER::instance == nullptr)
            AUDIO_MANAGER::instance = new AUDIO_MANAGER();
        return AUDIO_MANAGER::instance;
    }

    AUDIO_MANAGER::AUDIO_MANAGER():pauseAudioOnPauseGame(true)
    {
        mbm::DEVICE *device = mbm::DEVICE::getInstance();
        initializeBackend();
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
        finalizeBackend();
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
            PRINT_INFO_IF_DEBUG("File [%s] not found", fileNameSound);
            return nullptr;
        }
        #endif
        
        mbm::DEVICE *device = mbm::DEVICE::getInstance();
        const int idScene = device->scene ? device->scene->getIdScene() : -1;
        const size_t s1 = audios.size();
        // Only reuse an existing instance if it is NOT currently playing.
        // Reusing a playing instance would interrupt it — causing the "muted"
        // effect when many enemies trigger the same sound simultaneously.
        for (size_t i = 0; i < s1; ++i)
        {
            AUDIO* my_audio = audios[i];
            if (my_audio->idScene != idScene && 
                my_audio->fileName.compare(fileName) == 0 &&
                !my_audio->isPlaying())
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
                my_audio->fileName.compare(fileName) == 0 &&
                !my_audio->isPlaying())
            {
                my_audio->idScene = idScene;//make this sound belongs to this scene
                PRINT_INFO_IF_DEBUG("Resuscitated audio: %s [%p]\n", my_audio->fileName.c_str(), my_audio);
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
            PRINT_INFO_IF_DEBUG("delete audio from c++ %s\n",my_audio->fileName.c_str());
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
        // Process ALL pending deletions in one pass (not just the first element).
        // With PortAudio's shared-slot mixer, leaving many finished-scene audios
        // in audiosToDelete keeps their mixer slots occupied, which can exhaust
        // the pool (MIXER_MAX_SOURCES) when a new scene loads many sounds.
        for (int i = static_cast<int>(audiosToDelete.size()) - 1; i >= 0; --i)
        {
            AUDIO* my_audio = audiosToDelete[i];
            if(my_audio->idScene != idScene && coreManager->existScene(my_audio->idScene) == false )//destroy only if scene do not exist anymore
            {
                if(my_audio->bPersistent)
                {
                    audiosToDelete.erase(audiosToDelete.begin() + i);
                    audios.push_back(my_audio);
                }
                else
                {
                    #if defined DEBUG_AUDIO
                    PRINT_INFO_IF_DEBUG("Deleting audio [%p] C++:%s\n", my_audio, my_audio->fileName.c_str());
                    #endif
                    if (my_audio->userData)
                    {
                        PRINT_INFO_IF_DEBUG("Possible error on destroy audio. userData has value [%p] [%s] ", my_audio, my_audio->fileName.c_str());
                    }
                    else
                    {
                        audiosToDelete.erase(audiosToDelete.begin() + i);
                        delete my_audio;
                    }
                }
            }
        }
        updateBackend();
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
                    PRINT_INFO_IF_DEBUG("setPersist, Resuscitated audio: %s [%p]\n", my_audio->fileName.c_str(), my_audio);
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

    // Unlike destroy(), destroyNow() skips the scene-lifetime deferral and
    // immediately deletes the C++ AUDIO object, freeing its OpenSL player slot.
    // The caller is responsible for nulling any Lua userdata pointer that held
    // this audio object to prevent use-after-free.
    void AUDIO_MANAGER::destroyNow(AUDIO* that)
    {
        if (!AUDIO_MANAGER::instance || !that) return;
        that->stop();
        auto& a = AUDIO_MANAGER::instance->audios;
        auto it = std::find(a.begin(), a.end(), that);
        if (it != a.end()) a.erase(it);
        auto& d = AUDIO_MANAGER::instance->audiosToDelete;
        auto it2 = std::find(d.begin(), d.end(), that);
        if (it2 != d.end()) d.erase(it2);
        delete that; // ~AUDIO() frees the OpenSL player slot immediately
    }
    
    void AUDIO_MANAGER::release()
    {
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
                PRINT_INFO_IF_DEBUG("Possible error on destroy audio [%p] [%s] ", my_audio, my_audio->fileName.c_str());
                return;
            }
            else
            {
                delete my_audio;
            }
        }
        audiosToDelete.clear();
    }

    void AUDIO_MANAGER::stopAll()
    {
        for (auto my_audio : audios)
        {
            my_audio->stop();
        }
        
        for (auto my_audio : audiosToDelete)
        {
            my_audio->stop();
        }
    }

    void AUDIO_MANAGER::releaseStaticInstance()
    {
        if (AUDIO_MANAGER::instance)
        {
            AUDIO_MANAGER* tmp = AUDIO_MANAGER::instance;
            AUDIO_MANAGER::instance = nullptr; // null FIRST so any in-flight streamStopped callbacks bail out immediately
            tmp->release();
            delete tmp;
        }
    }

    void releaseAudioManager()
    {
        AUDIO_MANAGER::releaseStaticInstance();
    }

    AUDIO_MANAGER* AUDIO_MANAGER::instance = nullptr;
}
