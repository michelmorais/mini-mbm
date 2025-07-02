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

#include <windows.h>
#include <audio.h>
#include <device.h>
#include <core-manager.h>
#include <util-interface.h>
#include <comdef.h>

#if !defined(AUDIO_ENGINE_DIRECT_SOUND_8)
    #error attempt to use Direct sound without define AUDIO_ENGINE_DIRECT_SOUND_8
#endif

#pragma comment(lib, "dsound.lib")
//#pragma comment(lib, "winmm.lib")
//#pragma comment(lib,"dxguid.lib")

namespace mbm
{
	AUDIO::AUDIO(const int newIdScene):AUDIO_INTERFACE(newIdScene)
    {
        onEndStreamCallBack     = nullptr;
        direct_sound_buffer     = nullptr;
        dwDataWaveLength        = 0;
        dwDirectSoundBufferSize = 0;
        dFirstBuffer            = false;//always begin from second buffer because the first was filled already
        bLoop                   = false;
        dwSizeOneBuffer         = 0;
        dwCurrentOffsetBuffer   = 0;
    }

    AUDIO::~AUDIO()
    {
        if(this->direct_sound_buffer)
            this->direct_sound_buffer->Release();
        this->direct_sound_buffer = nullptr;
    }

    bool AUDIO::setVolume(const float volume)
    {
        if(this->direct_sound_buffer)
        {
            LONG Dsvol = 0;
            if(volume <= 0)
                Dsvol = DSBVOLUME_MIN;
            else if(volume > 1.0)
                Dsvol = DSBVOLUME_MAX;
            else
            {
                double decibels = 20.0 * std::log10(static_cast<double>(volume/100.0));
                Dsvol = static_cast<LONG>(decibels * 100.0);
            }
            if(SUCCEEDED(this->direct_sound_buffer->SetVolume(Dsvol)))
                return true;
        }
		return false;
    }
    
    bool AUDIO::setPan(const float pan)
    {
        if(this->direct_sound_buffer)
        {
            if(SUCCEEDED(this->direct_sound_buffer->SetPan(static_cast<LONG>(pan))))
                return true;
        }
		return false;
    }
    
    bool AUDIO::pause()
    {
		if(this->direct_sound_buffer)
        {
            DWORD dwStatus = 0;
            HRESULT hr = direct_sound_buffer->GetStatus(&dwStatus);
            if(FAILED(hr))
            {
                _com_error err(hr);
                ERROR_AT(__LINE__,__FILE__,"GetStatus\n%s",err.ErrorMessage());
                return false;
            }
            if(dwStatus & DSBSTATUS_PLAYING)
                state = AUDIO_PAUSED;
            else
                state = AUDIO_STOPPED;
            if(SUCCEEDED(this->direct_sound_buffer->Stop()))
                return true;
        }
		return false;
    }
    
    bool AUDIO::resume()
    {
        if(this->direct_sound_buffer && state == AUDIO_PAUSED)
        {
            if(SUCCEEDED(this->direct_sound_buffer->Play(0, 0, bLoop || this->wave_reader ? DSBPLAY_LOOPING : 0)))
            {
                state = AUDIO_PLAYING;
                return true;
            }
        }
        return false;
    }
    
    bool AUDIO::stop()
    {
        state = AUDIO_STOPPED;
        if(this->direct_sound_buffer)
        {
            if(FAILED(this->direct_sound_buffer->SetCurrentPosition(0)))
                return false;
            if(SUCCEEDED(this->direct_sound_buffer->Stop()))
            {
                this->rewindAndfillBufferWithSilence();
                return true;
            }
        }
		return false;
    }
    
    bool AUDIO::setPitch(const float pitch) // range from 0.5 to 2.0.  default is 1.0.
    {
		return false;
    }
    
    bool AUDIO::load(const char *filenameSound, const bool loop, const bool inMemory)
    {
        mbm::AUDIO_MANAGER * audio_man = mbm::AUDIO_MANAGER::getInstance();
        if(audio_man->m_directSound == nullptr)
        {
            ERROR_AT(__LINE__,__FILE__,"Direct sound is not initialized");
            return false;
        }
        HRESULT hr =0;
        this->wave_reader = std::make_unique<WaveFile>();
        if(this->wave_reader->OpenRead(filenameSound) == false)
        {
            ERROR_AT(__LINE__,__FILE__,"Error for %s -> %s",util::getBaseName(filenameSound), this->wave_reader->GetError());
            return false;
        }
        this->dwDataWaveLength = this->wave_reader->GetDataLength();
        //more info http://soundfile.sapp.org/doc/WaveFormat/
        WAVEFORMATEX waveformatex;
        memset(&waveformatex,0,sizeof(waveformatex));
        int formatType = this->wave_reader->GetFormatType();
        if(formatType == 1)
            waveformatex.wFormatTag  = WAVE_FORMAT_PCM;
        else
            waveformatex.wFormatTag  = WAVE_FORMAT_IEEE_FLOAT;//IEEE float
        waveformatex.nChannels       = this->wave_reader->GetNumChannels(); // number of channels (i.e. mono, stereo...)
        waveformatex.nSamplesPerSec  = this->wave_reader->GetSampleRate(); // sample rate 22050
        waveformatex.wBitsPerSample  = this->wave_reader->GetBitsPerChannel(); // number of bits per sample of mono data
        waveformatex.nBlockAlign     = waveformatex.wBitsPerSample / 8 * waveformatex.nChannels; // block size of data
        waveformatex.nAvgBytesPerSec = waveformatex.nSamplesPerSec * waveformatex.nBlockAlign; // for buffer estimation
        waveformatex.cbSize          = 0; // the count in bytes of the size of extra information (after cbSize)
        DSBUFFERDESC bufferdesc;
        memset(&bufferdesc,0, sizeof(DSBUFFERDESC));
        bufferdesc.dwSize          = sizeof(DSBUFFERDESC);
        //for DSBCAPS_CTRL3D , DSBCAPS_PRIMARYBUFFER  must be present and must be used with LPDIRECTSOUND instead of LPDIRECTSOUND8
	    bufferdesc.dwFlags         = DSBCAPS_GLOBALFOCUS|DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLPAN|DSBCAPS_CTRLFREQUENCY|DSBCAPS_GETCURRENTPOSITION2;//DSBCAPS_CTRLPOSITIONNOTIFY
        if(inMemory)
        {
	        bufferdesc.dwBufferBytes   = this->dwDataWaveLength;
        }
        else
        {
            dwSizeOneBuffer            = waveformatex.nAvgBytesPerSec;
            bufferdesc.dwBufferBytes   = 2 * waveformatex.nAvgBytesPerSec;
        }
        dwDirectSoundBufferSize    = bufferdesc.dwBufferBytes;
        bufferdesc.guid3DAlgorithm = GUID_NULL;
        bufferdesc.lpwfxFormat     = &waveformatex;
        hr = audio_man->m_directSound->CreateSoundBuffer( &bufferdesc, &this->direct_sound_buffer, nullptr );
        if(FAILED(hr))
        {
            bufferdesc.dwFlags = DSBCAPS_GLOBALFOCUS|DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLPAN|DSBCAPS_GETCURRENTPOSITION2;//DSBCAPS_CTRLPOSITIONNOTIFY
            hr = audio_man->m_directSound->CreateSoundBuffer( &bufferdesc, &this->direct_sound_buffer, nullptr );
            if(FAILED(hr))
            {
                _com_error err(hr);
                ERROR_AT(__LINE__,__FILE__,"Failure creating sound buffer directsound\n%s",err.ErrorMessage());
                this->wave_reader.reset();
                return false;
            }
        }
        void * buffer = nullptr;
        DWORD bufferSize = 0;
        bool bEndOfStream = false;
        hr = this->direct_sound_buffer->Lock( 0, dwDirectSoundBufferSize,&buffer, &bufferSize,nullptr, nullptr, 0L );
        if(FAILED(hr))
        {
            _com_error err(hr);
            ERROR_AT(__LINE__,__FILE__,"Failure locking buffer directsound\n%s",err.ErrorMessage());
            this->wave_reader.reset();
            return false;
        }
        if(bufferSize != dwDirectSoundBufferSize)
        {
            this->direct_sound_buffer->Unlock( buffer, bufferSize,nullptr, 0L );
            ERROR_AT(__LINE__,__FILE__,"The unexpected size of the lock [%u] differs from required[%u]\n%s",bufferSize,dwDirectSoundBufferSize);
            this->wave_reader.reset();
            return false;
        }
        if(inMemory == false)
        {
            bufferSize = dwSizeOneBuffer; // only 1 buffer
        }
        if(this->fillBufferWithSound(buffer,bufferSize ,loop,bEndOfStream) == false)
        {
            ERROR_AT(__LINE__,__FILE__,"Failure fillBufferWithSound wave [%s]\n%s",util::getBaseName(filenameSound),this->wave_reader ? this->wave_reader->GetError() : "?");
            this->wave_reader.reset();
            return false;
        }
        hr = this->direct_sound_buffer->Unlock( buffer, dwDirectSoundBufferSize,nullptr, 0L );
        if(FAILED(hr))
        {
            _com_error err(hr);
            ERROR_AT(__LINE__,__FILE__,"Failure unlocking buffer directsound\n%s",err.ErrorMessage());
            this->wave_reader.reset();
            return false;
        }
        if(inMemory || bEndOfStream)
            this->wave_reader.reset();
        bLoop = loop;
        return true;
    }

    bool AUDIO::restoreBuffer(bool & pbWasRestored)
    {
        if( direct_sound_buffer == nullptr )
            return false;
        pbWasRestored  = false;
        DWORD dwStatus = 0;
        HRESULT hr = direct_sound_buffer->GetStatus(&dwStatus);
        if(FAILED(hr))
        {
            _com_error err(hr);
            ERROR_AT(__LINE__,__FILE__,"GetStatus\n%s",err.ErrorMessage());
            return false;
        }

        if( dwStatus & DSBSTATUS_BUFFERLOST )
        {
            hr = direct_sound_buffer->Restore();
            if( hr == DSERR_BUFFERLOST )
                return false;
            pbWasRestored = true;
            return true;
        }
        else
        {
            return (dwStatus & DSBSTATUS_PLAYING);
        }
    }

    bool AUDIO::rewindAndfillBufferWithSilence()
    {
        if(this->wave_reader)
        {
            this->wave_reader->ResetToStart();
            this->wave_reader->ClearError();
            void* pDSLockedBuffer = nullptr;
            DWORD dwDSLockedBufferSize = 0;
            const DWORD offset_0 = 0;
            HRESULT hr = this->direct_sound_buffer->Lock(offset_0, this->dwDirectSoundBufferSize,&pDSLockedBuffer, &dwDSLockedBufferSize, nullptr, 0, 0L );
            if( FAILED(hr) )
            {
                _com_error err(hr);
                ERROR_AT(__LINE__,__FILE__,"Lock\n%s",err.ErrorMessage());
                return false;
            }
            memset(pDSLockedBuffer,this->wave_reader->GetBitsPerChannel() == 8 ? 128 : 0,dwDSLockedBufferSize);
            hr = this->direct_sound_buffer->Unlock(pDSLockedBuffer, dwDSLockedBufferSize, nullptr, 0);
            if(FAILED(hr))
            {
                _com_error err(hr);
                ERROR_AT(__LINE__,__FILE__,"Failure unlocking buffer directsound\n%s",err.ErrorMessage());
                return false;
            }
            return true;
        }
        return false;
    }


    bool AUDIO::fillBufferWithSound(void * buffer,const size_t iSizeBuffer,const bool bRepeatWavIfBufferLarger,bool & bEndOfStream)
    {
        if(this->wave_reader)
        {
            size_t iTotalRead = 0;
            if(this->wave_reader->ReadRaw(static_cast<char*>(buffer),iSizeBuffer,iTotalRead) == false)
            {
                PRINT_IF_DEBUG(" %s:%d Failure reading from wave [%s]\n%s",__LINE__,__FILE__,util::getBaseName(this->fileName.c_str()),this->wave_reader->GetError());
                return false;
            }
            if(iTotalRead < iSizeBuffer)
            {
                const size_t remainToRead = iSizeBuffer - iTotalRead;
                void * addressToRead = static_cast<unsigned char*>(buffer) + iTotalRead;
                if(bRepeatWavIfBufferLarger)
                {
                    if(this->wave_reader->ResetToStart() == false)
                    {
                        ERROR_AT(__LINE__,__FILE__,"ResetFile wave [%s]\n%s",util::getBaseName(this->fileName.c_str()),this->wave_reader->GetError());
                        return false;
                    }
                    bool dontCare_is_loop = false;
                    return fillBufferWithSound(addressToRead,remainToRead,bRepeatWavIfBufferLarger,dontCare_is_loop);
                }
                else
                {
                    bEndOfStream = true;
                    // Don't repeat the wav file, just fill in silence
                    memset(addressToRead,this->wave_reader->GetBitsPerChannel() == 8 ? 128 : 0,remainToRead);
                }
            }
            return true;
        }
        return false;
    }

    bool AUDIO::update()
    {
        bool pbWasRestored = false;
        if(this->direct_sound_buffer == nullptr)
            return false;
        if(this->wave_reader && restoreBuffer(pbWasRestored) && dwDataWaveLength > 0)
        {   
            if(pbWasRestored)
            {
                return true;
            }
            else
            {
                DWORD dwCurrentPlayPos= 0;
                DWORD lpdwCurrentWriteCursor = 0;
                HRESULT hr = this->direct_sound_buffer->GetCurrentPosition(&dwCurrentPlayPos,&lpdwCurrentWriteCursor);
                if(FAILED(hr))
                {
                    _com_error err(hr);
                    ERROR_AT(__LINE__,__FILE__,"GetCurrentPosition\n%s",err.ErrorMessage());
                    return false;
                }
                const float fPosCurrent = static_cast<float>(dwCurrentPlayPos)       / static_cast<float>(dwDirectSoundBufferSize);
                const float fPosWritter = static_cast<float>(lpdwCurrentWriteCursor) / static_cast<float>(dwDirectSoundBufferSize);
                void* pDSLockedBuffer = nullptr;
                DWORD dwDSLockedBufferSize = 0;
                bool bEndOfStream = false;
                /*
             offset_1                           offset_2
                0              0.5 Second       1Second              1.5 Second        2Second
                |                  |               |                     |                 |
                ---------------------------------------------------------------------------
                |            FIRST_BUFFER          |                SECOND_BUFFER          |
                |--------------------------------------------------------------------------|
                */
                if(this->dFirstBuffer)
                {
                    if(fPosCurrent > 0.5f && fPosWritter > 0.5f )
                    {
                        const DWORD offset_2 = 0;
                        hr = this->direct_sound_buffer->Lock(offset_2, dwSizeOneBuffer,&pDSLockedBuffer, &dwDSLockedBufferSize, nullptr, 0, 0L );
                        if( FAILED(hr) )
                        {
                            _com_error err(hr);
                            ERROR_AT(__LINE__,__FILE__,"Lock\n%s",err.ErrorMessage());
                            return false;
                        }
                        if(fillBufferWithSound(pDSLockedBuffer,dwDSLockedBufferSize,bLoop,bEndOfStream) == false)
                        {
                            bEndOfStream = true;//we assume that the stream ended
                        }
                        hr = this->direct_sound_buffer->Unlock(pDSLockedBuffer, dwDSLockedBufferSize, nullptr, 0);
                        if(FAILED(hr))
                        {
                            _com_error err(hr);
                            ERROR_AT(__LINE__,__FILE__,"Failure unlocking buffer directsound\n%s",err.ErrorMessage());
                            return false;
                        }
                        this->dFirstBuffer = false;
                    }
                }
                else // SECOND_BUFFER
                {
                    if(fPosCurrent < 0.5f && fPosWritter < 0.5f)
                    {
                        const DWORD offset_1 = dwSizeOneBuffer;
                        hr = this->direct_sound_buffer->Lock(offset_1, dwSizeOneBuffer,&pDSLockedBuffer, &dwDSLockedBufferSize, nullptr, 0, 0L );
                        if( FAILED(hr) )
                        {
                            _com_error err(hr);
                            ERROR_AT(__LINE__,__FILE__,"Lock\n%s",err.ErrorMessage());
                            return false;
                        }
                        if(fillBufferWithSound(pDSLockedBuffer,dwDSLockedBufferSize,bLoop,bEndOfStream) == false)
                        {
                            bEndOfStream = true;//we assume that the stream ended
                        }
                        hr = this->direct_sound_buffer->Unlock(pDSLockedBuffer, dwDSLockedBufferSize, nullptr, 0);
                        if(FAILED(hr))
                        {
                            _com_error err(hr);
                            ERROR_AT(__LINE__,__FILE__,"Failure unlocking buffer directsound\n%s",err.ErrorMessage());
                            return false;
                        }
                        this->dFirstBuffer = true;
                    }
                }
                if(bEndOfStream)//it means that is not loop
                {
                    DWORD lpdwStatus = 0;
                    if(FAILED(this->direct_sound_buffer->GetStatus(&lpdwStatus)))
                        return false;
                    if (lpdwStatus & DSBSTATUS_PLAYING)
                    {
                        if(FAILED(this->direct_sound_buffer->Play(0, 0, 0)))//remove the loop flag
                            return false;
                    }
                    this->wave_reader->ResetToStart();
                }
                return true;
            }
        }
        else if(state == AUDIO_PLAYING)
        {
            //fprintf(stdout,"CallBack for %s\n",this->fileName.c_str());
            state = AUDIO_STOPPED;
            this->rewindAndfillBufferWithSilence();
            if(this->onEndStreamCallBack)
                this->onEndStreamCallBack(this);
            return true;
        }
        return false;
    }
    
    bool AUDIO::play(const bool loop)
    {
        if(this->direct_sound_buffer)
        {
            bLoop = loop;
            if(SUCCEEDED(this->direct_sound_buffer->Play(0, 0, bLoop || this->wave_reader ? DSBPLAY_LOOPING : 0)))
            {
                state = AUDIO_PLAYING;
                return true;
            }
        }
        state = AUDIO_STOPPED;
		return false;
    }
    
    bool AUDIO::isPlaying()
    {
		if(this->direct_sound_buffer)
        {
            DWORD lpdwStatus = 0;
            if(FAILED(this->direct_sound_buffer->GetStatus(&lpdwStatus)))
                return false;
            return (lpdwStatus & DSBSTATUS_PLAYING);
        }
		return false;
    }
    
    bool AUDIO::isPaused()
    {
		if(this->direct_sound_buffer)
        {
            DWORD lpdwStatus = 0;
            if(FAILED(this->direct_sound_buffer->GetStatus(&lpdwStatus)))
                return false;
            if (((lpdwStatus & DSBSTATUS_PLAYING) == 0) && state == AUDIO_PLAYING)
                return true;
        }
		return false;
    }
    
    float AUDIO::getVolume()//TODO: getVolume should be set volume calc
    {
		if(this->direct_sound_buffer)
        {
            LONG volume = 0;
            if(FAILED(this->direct_sound_buffer->GetVolume(&volume)))
                return 0.0f;
            return static_cast<float>(volume);
        }
		return 0.0f;
    }
    
    float AUDIO::getPan()
    {
        if(this->direct_sound_buffer)
        {
            LONG pan = 0;
            if(FAILED(this->direct_sound_buffer->GetPan(&pan)))
                return 0.0f;
            return static_cast<float>(pan);
        }
		return 0.0f;
    }
    
    float AUDIO::getPitch()
    {
        return 0.0f;
    }
    
    int AUDIO::getLength()
    {
        if(this->direct_sound_buffer)
        {
            return static_cast<int>(this->dwDataWaveLength);
        }
		return 0;
    }

    bool AUDIO::reset() 
    {
		if(this->direct_sound_buffer)
        {
            bool bRestored = false;
            this->restoreBuffer(bRestored);
            if(SUCCEEDED(this->direct_sound_buffer->Play(0, 0, bLoop || this->wave_reader ? DSBPLAY_LOOPING : 0)) && SUCCEEDED(this->direct_sound_buffer->SetCurrentPosition(0)))
            {
                if(this->wave_reader)
                    rewindAndfillBufferWithSilence();
                state = AUDIO_PLAYING;
                return true;
            }
        }
        state = AUDIO_STOPPED;
		return false;
    }

    bool AUDIO::setPosition(const int pos)
    {
		if(this->direct_sound_buffer)
        {
            if(this->wave_reader)
            {
                ERROR_LOG("AUDIO::setPosition not implemented for stream.");
                return false;
            }
            if(SUCCEEDED(this->direct_sound_buffer->SetCurrentPosition(pos)))
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
        if(this->direct_sound_buffer)
            return true;
		return false;
	}

	const char* AUDIO_ENGINE_version()
	{
        static char version[32] = "";
        snprintf(version,sizeof(version),"Direct sound %x",DIRECTSOUND_VERSION);
        return version;
	}

    const char* AUDIO::getFileName() const noexcept
    {
        return this->fileName.c_str();
    }

}
