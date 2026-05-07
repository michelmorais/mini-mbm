
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

#include <string.h>
#include <math.h>
#include <portaudio.h>
#include <pa-audio-interface.h>
#if defined(__linux__) || defined(__APPLE__)
    #include <fcntl.h>
    #include <unistd.h>
#endif

/* ---------------------------------------------------------------------------
 * Format translation
 * -------------------------------------------------------------------------*/

uint32_t TranslateFormatType(const uint16_t sampleFormat, const uint16_t bitsPerChannel)
{
    if (sampleFormat == 3)
        return paFloat32;       // IEEE 754 single-precision float
    // sampleFormat == 1 (PCM) or similar; select by bit depth
    switch (bitsPerChannel)
    {
        case  8: return paUInt8;   // unsigned 8-bit, silence at 128
        case 16: return paInt16;
        case 24: return paInt24;   // packed 3-byte little-endian
        case 32: return paInt32;
        default: return paFloat32;
    }
}

/* ---------------------------------------------------------------------------
 * PA_DATA
 * -------------------------------------------------------------------------*/

PA_DATA::PA_DATA(const uint16_t numChannels, const uint16_t sampleFormat,
                 const uint16_t bitsPerChannel, const uint32_t sampleRate,
                 const uint16_t bytesPerSample, const uint32_t dataLength,
                 const uint32_t bytesPerSecond)
    : m_numChannels(numChannels)
    , m_sampleFormat(TranslateFormatType(sampleFormat, bitsPerChannel))
    , m_bitsPerChannel(bitsPerChannel)
    , m_sampleRate(sampleRate)
    , m_bytesPerSample(bytesPerSample)
    , m_durationMs(bytesPerSecond > 0u
          ? (dataLength / bytesPerSecond) * 1000u
            + ((dataLength % bytesPerSecond) * 1000u / bytesPerSecond)
          : 0u)
    , m_stream(nullptr)
    , m_loop(false)
    , m_paused(false)
    , m_volume(1.0)
    , m_pan(0.0)
    , m_finished(false)
    , m_outputParameters(new PaStreamParameters())
{}

PA_DATA::~PA_DATA()
{
    if (m_stream)
        Pa_CloseStream(static_cast<PaStream*>(m_stream));
    delete m_outputParameters;
}

void PA_DATA::adjustVolume(void* data, const uint32_t sizeInBytes, const double volume)
{
    if (volume >= 1.0) return;
    if (volume <= 0.0)
    {
        if (m_sampleFormat == paUInt8)
            memset(data, 128, sizeInBytes);   // unsigned 8-bit silence = 128
        else
            memset(data, 0, sizeInBytes);
        return;
    }
    if (m_sampleFormat == paInt16)
    {
        int16_t* buf   = static_cast<int16_t*>(data);
        uint32_t count = sizeInBytes / sizeof(int16_t);
        for (uint32_t i = 0; i < count; ++i)
            buf[i] = static_cast<int16_t>(buf[i] * volume);
    }
    else if (m_sampleFormat == paInt32)
    {
        int32_t* buf   = static_cast<int32_t*>(data);
        uint32_t count = sizeInBytes / sizeof(int32_t);
        for (uint32_t i = 0; i < count; ++i)
            buf[i] = static_cast<int32_t>(static_cast<double>(buf[i]) * volume);
    }
    else if (m_sampleFormat == paFloat32)
    {
        float*   buf   = static_cast<float*>(data);
        uint32_t count = sizeInBytes / sizeof(float);
        const float fvol = static_cast<float>(volume);
        for (uint32_t i = 0; i < count; ++i)
            buf[i] *= fvol;
    }
    else if (m_sampleFormat == paUInt8)
    {
        uint8_t* buf = static_cast<uint8_t*>(data);
        for (uint32_t i = 0; i < sizeInBytes; ++i)
            buf[i] = static_cast<uint8_t>(
                128 + static_cast<int>((static_cast<int>(buf[i]) - 128) * volume));
    }
    // paInt24: rare; skipped — passthrough at current volume
}

void PA_DATA::applyPan(void* data, const uint32_t frames)
{
    if (m_numChannels != 2 || m_pan == 0.0) return;

    // Linear pan: at centre (0) both gains are 1.0;
    // at hard right (+1) left gain = 0, right gain = 1;
    // at hard left (-1) left gain = 1, right gain = 0.
    const double leftGain  = (m_pan < 0.0) ? 1.0 : (1.0 - m_pan);
    const double rightGain = (m_pan > 0.0) ? 1.0 : (1.0 + m_pan);

    if (m_sampleFormat == paInt16)
    {
        int16_t* buf = static_cast<int16_t*>(data);
        for (uint32_t i = 0; i < frames; ++i)
        {
            buf[i * 2]     = static_cast<int16_t>(buf[i * 2]     * leftGain);
            buf[i * 2 + 1] = static_cast<int16_t>(buf[i * 2 + 1] * rightGain);
        }
    }
    else if (m_sampleFormat == paFloat32)
    {
        float* buf = static_cast<float*>(data);
        for (uint32_t i = 0; i < frames; ++i)
        {
            buf[i * 2]     *= static_cast<float>(leftGain);
            buf[i * 2 + 1] *= static_cast<float>(rightGain);
        }
    }
    else if (m_sampleFormat == paInt32)
    {
        int32_t* buf = static_cast<int32_t*>(data);
        for (uint32_t i = 0; i < frames; ++i)
        {
            buf[i * 2]     = static_cast<int32_t>(static_cast<double>(buf[i * 2])     * leftGain);
            buf[i * 2 + 1] = static_cast<int32_t>(static_cast<double>(buf[i * 2 + 1]) * rightGain);
        }
    }
}

/* ---------------------------------------------------------------------------
 * PA_DATA_MEMORY
 * -------------------------------------------------------------------------*/

PA_DATA_MEMORY::PA_DATA_MEMORY(const uint16_t numChannels, const uint16_t sampleFormat,
                               const uint16_t bitsPerChannel, const uint32_t sampleRate,
                               const uint16_t bytesPerSample, const uint32_t dataLength,
                               const uint32_t bytesPerSecond, std::vector<char>&& sample)
    : PA_DATA(numChannels, sampleFormat, bitsPerChannel, sampleRate, bytesPerSample,
              dataLength, bytesPerSecond)
    , m_sample(std::move(sample))
    , m_index(0)
{}

void PA_DATA_MEMORY::setPosition(const double pos)
{
    if (pos <= 0.0 || m_sample.empty())
    {
        m_index = 0;
        return;
    }
    m_index = static_cast<uint32_t>(pos * static_cast<double>(m_sample.size()));
    // align to frame boundary
    if (m_bytesPerSample > 0)
        m_index = (m_index / m_bytesPerSample) * m_bytesPerSample;
    if (m_index >= static_cast<uint32_t>(m_sample.size()))
        m_index = 0;
}

double PA_DATA_MEMORY::getPosition() const
{
    if (m_sample.empty() || m_index == 0)
        return 0.0;
    return static_cast<double>(m_index) / static_cast<double>(m_sample.size());
}

/* ---------------------------------------------------------------------------
 * PA_DATA_FILE
 * -------------------------------------------------------------------------*/

PA_DATA_FILE::PA_DATA_FILE(WaveFile* wave)
    : PA_DATA(wave->GetNumChannels(), wave->GetFormatType(), wave->GetBitsPerChannel(),
              wave->GetSampleRate(), wave->GetBytesPerSample(),
              wave->GetDataLength(), wave->GetBytesPerSecond())
    , wave(wave)
    , m_dataStart(wave->GetFile() ? ftell(wave->GetFile()) : 0L)
    , m_bytesRead(0)
{}

void PA_DATA_FILE::setPosition(const double pos)
{
    if (pos <= 0.0)
    {
        wave->ResetToStart();
        m_bytesRead = 0;
        return;
    }
    const uint32_t dataLength = wave->GetDataLength();
    if (dataLength == 0) return;
    uint32_t targetByte = static_cast<uint32_t>(pos * static_cast<double>(dataLength));
    // align to frame boundary
    if (m_bytesPerSample > 0)
        targetByte = (targetByte / m_bytesPerSample) * m_bytesPerSample;
    FILE* fp = wave->GetFile();
    if (fp && fseek(fp, m_dataStart + static_cast<long>(targetByte), SEEK_SET) == 0)
        m_bytesRead = targetByte;
}

double PA_DATA_FILE::getPosition() const
{
    const uint32_t dataLength = wave->GetDataLength();
    if (dataLength == 0 || m_bytesRead == 0)
        return 0.0;
    return static_cast<double>(m_bytesRead) / static_cast<double>(dataLength);
}

/* ---------------------------------------------------------------------------
 * PA_INTERFACE — static member
 * -------------------------------------------------------------------------*/
bool PA_INTERFACE::initialized = false;

/* ---------------------------------------------------------------------------
 * PA_INTERFACE — construction / destruction
 * -------------------------------------------------------------------------*/

PA_INTERFACE::PA_INTERFACE() : m_data(nullptr)
{
    if (!PA_INTERFACE::initialized)
    {
        // Pa_Initialize() causes ALSA and JACK to probe devices and print
        // diagnostic messages to stderr that are not meaningful to the user.
        // Suppress stderr during initialisation, then restore it.
#if defined(__linux__) || defined(__APPLE__)
        const int savedErr = dup(STDERR_FILENO);
        const int devNull  = open("/dev/null", O_WRONLY);
        if (devNull >= 0)
            dup2(devNull, STDERR_FILENO);
        if (devNull >= 0)
            close(devNull);
#endif
        const bool ok = (Pa_Initialize() == paNoError);
#if defined(__linux__) || defined(__APPLE__)
        if (savedErr >= 0)
        {
            dup2(savedErr, STDERR_FILENO);
            close(savedErr);
        }
#endif
        if (ok)
            PA_INTERFACE::initialized = true;
    }
}

PA_INTERFACE::~PA_INTERFACE()
{
    delete m_data;
    m_data = nullptr;
}

/* ---------------------------------------------------------------------------
 * PA_INTERFACE::openStream  (memory-backed)
 * -------------------------------------------------------------------------*/

bool PA_INTERFACE::openStream(const uint16_t numChannels, const uint16_t sampleFormat,
                              const uint16_t bitsPerChannel, const uint32_t sampleRate,
                              const uint16_t bytesPerSample, const uint32_t dataLength,
                              const uint32_t bytesPerSecond, std::vector<char>&& sample)
{
    if (!initialized) return false;

    PA_DATA_MEMORY* data = new PA_DATA_MEMORY(numChannels, sampleFormat, bitsPerChannel,
                                              sampleRate, bytesPerSample,
                                              dataLength, bytesPerSecond,
                                              std::move(sample));
    m_data = data;

    data->m_outputParameters->device = Pa_GetDefaultOutputDevice();
    if (data->m_outputParameters->device == paNoDevice)
    {
        delete data;
        m_data = nullptr;
        return false;
    }
    data->m_outputParameters->channelCount          = data->m_numChannels;
    data->m_outputParameters->sampleFormat          = data->m_sampleFormat;
    data->m_outputParameters->hostApiSpecificStreamInfo = nullptr;
    data->m_outputParameters->suggestedLatency =
        Pa_GetDeviceInfo(data->m_outputParameters->device)->defaultLowOutputLatency;

    PaError ret = Pa_OpenStream(
        static_cast<PaStream**>(&data->m_stream),
        nullptr,
        data->m_outputParameters,
        static_cast<double>(data->m_sampleRate),
        256,
        0,
        &paStreamCallback,
        data);
    if (ret != paNoError)
    {
        delete data;
        m_data = nullptr;
        return false;
    }
    return true;
}

/* ---------------------------------------------------------------------------
 * PA_INTERFACE::openStream  (file-streaming)
 * -------------------------------------------------------------------------*/

bool PA_INTERFACE::openStream(WaveFile* wave)
{
    if (!initialized) return false;

    PA_DATA_FILE* data = new PA_DATA_FILE(wave);
    m_data = data;

    data->m_outputParameters->device = Pa_GetDefaultOutputDevice();
    if (data->m_outputParameters->device == paNoDevice)
    {
        delete data;
        m_data = nullptr;
        return false;
    }
    data->m_outputParameters->channelCount          = data->m_numChannels;
    data->m_outputParameters->sampleFormat          = data->m_sampleFormat;
    data->m_outputParameters->hostApiSpecificStreamInfo = nullptr;
    data->m_outputParameters->suggestedLatency =
        Pa_GetDeviceInfo(data->m_outputParameters->device)->defaultLowOutputLatency;

    PaError ret = Pa_OpenStream(
        static_cast<PaStream**>(&data->m_stream),
        nullptr,
        data->m_outputParameters,
        static_cast<double>(data->m_sampleRate),
        256,
        0,
        &paStreamCallbackFromFile,
        data);
    if (ret != paNoError)
    {
        delete data;
        m_data = nullptr;
        return false;
    }
    return true;
}

/* ---------------------------------------------------------------------------
 * PA_INTERFACE — stream callbacks
 * -------------------------------------------------------------------------*/

int PA_INTERFACE::paStreamCallback(
    const void* /*input*/, void* output, const unsigned long frameCount,
    const PaStreamCallbackTimeInfo* /*timeInfo*/, unsigned long /*statusFlags*/,
    void* userData)
{
    PA_DATA_MEMORY* data        = static_cast<PA_DATA_MEMORY*>(userData);
    const uint32_t  bytesNeeded = data->m_bytesPerSample * static_cast<uint32_t>(frameCount);
    const uint32_t  totalBytes  = static_cast<uint32_t>(data->m_sample.size());
    const uint32_t  remaining   = (data->m_index < totalBytes) ? (totalBytes - data->m_index) : 0u;
    char*           out         = static_cast<char*>(output);

    if (bytesNeeded <= remaining)
    {
        // Normal case: enough buffered data available
        memcpy(out, &data->m_sample[data->m_index], bytesNeeded);
        data->m_index += bytesNeeded;
    }
    else if (data->m_loop)
    {
        // Copy tail of buffer, wrap to front
        if (remaining > 0)
            memcpy(out, &data->m_sample[data->m_index], remaining);
        data->m_index = 0;
        uint32_t left = bytesNeeded - remaining;
        if (left > totalBytes) left = totalBytes;
        if (left > 0)
            memcpy(out + remaining, &data->m_sample[0], left);
        data->m_index = left;
    }
    else
    {
        // End of non-looping stream: copy what remains, pad with silence
        if (remaining > 0)
            memcpy(out, &data->m_sample[data->m_index], remaining);
        const uint32_t pad = bytesNeeded - remaining;
        if (pad > 0)
        {
            if (data->m_sampleFormat == paUInt8)
                memset(out + remaining, 128, pad);
            else
                memset(out + remaining, 0, pad);
        }
        data->m_index = totalBytes;
        if (data->m_volume < 1.0) data->adjustVolume(output, bytesNeeded, data->m_volume);
        if (data->m_pan  != 0.0) data->applyPan(output, frameCount);
        data->m_finished.store(true, std::memory_order_release);
        return paComplete;
    }

    if (data->m_volume < 1.0) data->adjustVolume(output, bytesNeeded, data->m_volume);
    if (data->m_pan  != 0.0) data->applyPan(output, frameCount);
    return paContinue;
}

int PA_INTERFACE::paStreamCallbackFromFile(
    const void* /*input*/, void* output, const unsigned long frameCount,
    const PaStreamCallbackTimeInfo* /*timeInfo*/, unsigned long /*statusFlags*/,
    void* userData)
{
    PA_DATA_FILE*  data        = static_cast<PA_DATA_FILE*>(userData);
    const uint32_t bytesNeeded = data->m_bytesPerSample * static_cast<uint32_t>(frameCount);
    char*          out         = static_cast<char*>(output);

    size_t bytesRead = 0;
    data->wave->ReadRaw(out, bytesNeeded, bytesRead);
    data->m_bytesRead += static_cast<uint32_t>(bytesRead);

    if (static_cast<uint32_t>(bytesRead) < bytesNeeded)
    {
        if (data->m_loop)
        {
            data->wave->ResetToStart();
            data->m_bytesRead = 0;
            const uint32_t deficit = bytesNeeded - static_cast<uint32_t>(bytesRead);
            size_t bytesRead2 = 0;
            data->wave->ReadRaw(out + bytesRead, deficit, bytesRead2);
            data->m_bytesRead += static_cast<uint32_t>(bytesRead2);
            if (bytesRead2 < deficit)
                memset(out + bytesRead + bytesRead2, 0, deficit - bytesRead2);
        }
        else
        {
            // End of stream: pad with silence
            const uint32_t pad = bytesNeeded - static_cast<uint32_t>(bytesRead);
            memset(out + bytesRead, 0, pad);
            if (data->m_volume < 1.0) data->adjustVolume(output, bytesNeeded, data->m_volume);
            if (data->m_pan  != 0.0) data->applyPan(output, static_cast<uint32_t>(frameCount));
            data->m_finished.store(true, std::memory_order_release);
            return paComplete;
        }
    }

    if (data->m_volume < 1.0) data->adjustVolume(output, bytesNeeded, data->m_volume);
    if (data->m_pan  != 0.0) data->applyPan(output, static_cast<uint32_t>(frameCount));
    return paContinue;
}

/* ---------------------------------------------------------------------------
 * PA_INTERFACE — default play() (subclasses may override for rewind+start)
 * -------------------------------------------------------------------------*/

bool PA_INTERFACE::play(bool bLoop)
{
    stop();
    setLoop(bLoop);
    return start();
}

/* ---------------------------------------------------------------------------
 * PA_INTERFACE — transport controls
 * -------------------------------------------------------------------------*/

bool PA_INTERFACE::start()
{
    if (!m_data || !m_data->m_stream) return false;
    m_data->m_paused = false;
    m_data->m_finished.store(false, std::memory_order_release);
    return Pa_StartStream(static_cast<PaStream*>(m_data->m_stream)) == paNoError;
}

bool PA_INTERFACE::stop()
{
    if (!m_data) return true;
    m_data->m_paused = false;
    m_data->m_finished.store(false, std::memory_order_release);
    if (m_data->m_stream &&
        Pa_IsStreamStopped(static_cast<PaStream*>(m_data->m_stream)) == 0)
    {
        // Pa_IsStreamStopped() == 0 means the stream is either active, or
        // inactive-after-paComplete.  Both states require Pa_StopStream()
        // before Pa_StartStream() can be called again.
        Pa_StopStream(static_cast<PaStream*>(m_data->m_stream));
    }
    m_data->setPosition(0.0);   // always rewind so next play() starts from beginning
    return true;
}

bool PA_INTERFACE::pause()
{
    if (!m_data || m_data->m_paused) return false;
    if (m_data->m_stream &&
        Pa_IsStreamActive(static_cast<PaStream*>(m_data->m_stream)) > 0)
    {
        if (Pa_StopStream(static_cast<PaStream*>(m_data->m_stream)) != paNoError)
            return false;
        m_data->m_paused = true;
        return true;
    }
    return false;
}

void PA_INTERFACE::setPosition(const double pos)
{
    if (m_data) m_data->setPosition(pos);
}

double PA_INTERFACE::getPosition() const
{
    return m_data ? m_data->getPosition() : 0.0;
}

void PA_INTERFACE::setLoop(const bool bLoop)
{
    if (m_data) m_data->m_loop = bLoop;
}

bool PA_INTERFACE::isPlaying() const
{
    if (!m_data || m_data->m_paused || !m_data->m_stream) return false;
    return Pa_IsStreamActive(static_cast<PaStream*>(m_data->m_stream)) > 0;
}

bool PA_INTERFACE::isPaused() const
{
    return m_data && m_data->m_paused;
}

bool PA_INTERFACE::isFinished() const
{
    return m_data && m_data->m_finished.load(std::memory_order_acquire);
}

void PA_INTERFACE::clearFinished()
{
    if (m_data) m_data->m_finished.store(false, std::memory_order_release);
}

void PA_INTERFACE::setVolume(const double volume)
{
    if (m_data)
        m_data->m_volume = (volume > 1.0) ? 1.0 : (volume < 0.0 ? 0.0 : volume);
}

double PA_INTERFACE::getVolume()
{
    return m_data ? m_data->m_volume : 0.0;
}

void PA_INTERFACE::setPan(const double pan)
{
    if (m_data)
        m_data->m_pan = (pan > 1.0) ? 1.0 : (pan < -1.0 ? -1.0 : pan);
}

double PA_INTERFACE::getPan() const
{
    return m_data ? m_data->m_pan : 0.0;
}

int PA_INTERFACE::getLength() const
{
    return m_data ? static_cast<int>(m_data->m_durationMs) : 0;
}

#endif