
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
#include <thread>
#include <chrono>
#if defined(__linux__) || defined(__APPLE__)
    #include <fcntl.h>
    #include <unistd.h>
#endif

/* ---------------------------------------------------------------------------
 * Software mixer — one global PaStream shared by all active sound sources.
 *
 * Each PA_INTERFACE registers a slot here.  play()/stop() just flip an
 * atomic flag — no Pa_OpenStream / Pa_StartStream / Pa_StopStream per sound.
 * The single ALSA PCM stream is opened once at first sound load, eliminating
 * the per-sound ALSA overhead that causes frame drops in games with many
 * concurrent short effects (arrows, explosions, etc.).
 * -------------------------------------------------------------------------*/
namespace {

static const int MIXER_MAX_SOURCES    = 64;   // max simultaneously loaded sounds
static const int MIXER_FRAMES_PER_BUF = 256;  // frames per callback invocation
// Maximum source-to-mixer sample-rate ratio supported for resampling.
// 4× covers: 48000→11025, 96000→24000, etc.  Common game ratios are ≤2.2×.
static const int MIXER_MAX_RATE_RATIO = 4;

/* Convert raw PCM (any PaSampleFormat, 1 or 2 channels) to interleaved
 * float32 stereo in-place.  Mono sources are duplicated L→R. */
static void convertToFloat32Stereo(const void* in, uint32_t paFmt, uint16_t inCh,
                                   uint32_t frames, float* out)
{
    const uint32_t outSamples = frames * 2u;
    if (paFmt == paFloat32)
    {
        const float* src = static_cast<const float*>(in);
        if (inCh == 1)
            { for (uint32_t f = 0; f < frames; ++f) out[f*2] = out[f*2+1] = src[f]; }
        else
            memcpy(out, src, outSamples * sizeof(float));
    }
    else if (paFmt == paInt16)
    {
        const int16_t* src = static_cast<const int16_t*>(in);
        const float inv = 1.0f / 32768.0f;
        if (inCh == 1)
            { for (uint32_t f = 0; f < frames; ++f) out[f*2] = out[f*2+1] = src[f] * inv; }
        else
            { for (uint32_t s = 0; s < outSamples; ++s) out[s] = src[s] * inv; }
    }
    else if (paFmt == paInt32)
    {
        const int32_t* src = static_cast<const int32_t*>(in);
        const float inv = 1.0f / 2147483648.0f;
        if (inCh == 1)
            { for (uint32_t f = 0; f < frames; ++f) out[f*2] = out[f*2+1] = src[f] * inv; }
        else
            { for (uint32_t s = 0; s < outSamples; ++s) out[s] = src[s] * inv; }
    }
    else if (paFmt == paUInt8)
    {
        const uint8_t* src = static_cast<const uint8_t*>(in);
        const float inv = 1.0f / 128.0f;
        if (inCh == 1)
            { for (uint32_t f = 0; f < frames; ++f) out[f*2] = out[f*2+1] = (src[f] - 128) * inv; }
        else
            { for (uint32_t s = 0; s < outSamples; ++s) out[s] = (src[s] - 128) * inv; }
    }
    else
        memset(out, 0, outSamples * sizeof(float));
}

class PA_MIXER
{
public:
    static PA_MIXER& instance()
    {
        static PA_MIXER s;
        return s;
    }

    /* Called on first openStream() — initialises PortAudio and opens the one
     * shared output stream.  Subsequent calls with different sample rates are
     * silently accepted (the mixer rate is fixed at first-call time). */
    bool init(uint32_t sampleRate)
    {
        if (m_initialized) return true;

#if defined(__linux__) || defined(__APPLE__)
        // Pa_Initialize() probes ALSA/JACK devices and prints diagnostic
        // messages to stderr.  Suppress them — they are not useful to users.
        const int savedErr = dup(STDERR_FILENO);
        const int devNull  = open("/dev/null", O_WRONLY);
        if (devNull >= 0) dup2(devNull, STDERR_FILENO);
        if (devNull >= 0) close(devNull);
#endif
        PaError err = Pa_Initialize();
#if defined(__linux__) || defined(__APPLE__)
        if (savedErr >= 0) { dup2(savedErr, STDERR_FILENO); close(savedErr); }
#endif
        if (err != paNoError) return false;

        PaDeviceIndex devIdx = Pa_GetDefaultOutputDevice();
        if (devIdx == paNoDevice) { Pa_Terminate(); return false; }

        PaStreamParameters p{};
        p.device                    = devIdx;
        p.channelCount              = 2;           // always stereo out; mono sources are upmixed
        p.sampleFormat              = paFloat32;   // float mixing is simplest and accurate
        p.suggestedLatency          = Pa_GetDeviceInfo(devIdx)->defaultLowOutputLatency;
        p.hostApiSpecificStreamInfo = nullptr;

        m_sampleRate = sampleRate;
        err = Pa_OpenStream(&m_stream, nullptr, &p,
                            static_cast<double>(sampleRate),
                            MIXER_FRAMES_PER_BUF, 0,
                            &PA_MIXER::staticCallback, this);
        if (err != paNoError) { Pa_Terminate(); return false; }

        err = Pa_StartStream(m_stream);
        if (err != paNoError)
        {
            Pa_CloseStream(m_stream);
            m_stream = nullptr;
            Pa_Terminate();
            return false;
        }

        m_initialized = true;
        return true;
    }

    /* Allocate a mixer slot for 'data'.  Returns slot index, -1 if full. */
    int registerSource(PA_DATA* data)
    {
        for (int i = 0; i < MIXER_MAX_SOURCES; ++i)
        {
            if (!m_slots[i].registered)
            {
                m_slots[i].data       = data;
                m_slots[i].active.store(false, std::memory_order_release);
                m_slots[i].registered = true;
                return i;
            }
        }
        return -1;
    }

    /* Deactivate slot, wait up to 20 ms for any in-flight callback access to
     * complete, then free PA_DATA.  Safe to call from destructor. */
    void unregisterSource(int idx, PA_DATA* data)
    {
        if (idx >= 0 && idx < MIXER_MAX_SOURCES)
        {
            m_slots[idx].active.store(false, std::memory_order_seq_cst);
            m_slots[idx].registered = false;
            m_slots[idx].data       = nullptr;

            // Spin until the callback counter advances once, proving no
            // callback frame is still holding the old 'data' pointer.
            if (m_initialized)
            {
                const int64_t seqBefore = m_callbackSeq.load(std::memory_order_acquire);
                for (int i = 0; i < 20; ++i)
                {
                    if (m_callbackSeq.load(std::memory_order_acquire) != seqBefore) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        }
        delete data;
    }

    void activate(int idx)
    {
        if (idx >= 0 && idx < MIXER_MAX_SOURCES)
            m_slots[idx].active.store(true, std::memory_order_release);
    }

    void deactivate(int idx)
    {
        if (idx >= 0 && idx < MIXER_MAX_SOURCES)
            m_slots[idx].active.store(false, std::memory_order_release);
    }

    bool isActive(int idx) const
    {
        if (idx < 0 || idx >= MIXER_MAX_SOURCES) return false;
        return m_slots[idx].active.load(std::memory_order_acquire);
    }

    bool     isInitialized() const { return m_initialized; }
    uint32_t sampleRate()     const { return m_sampleRate;  }

private:
    PA_MIXER() = default;
    ~PA_MIXER()
    {
        m_initialized = false;   // stop PA_INTERFACE methods from accessing us
        if (m_stream)
        {
            Pa_AbortStream(m_stream);
            Pa_CloseStream(m_stream);
        }
        Pa_Terminate();
    }
    PA_MIXER(const PA_MIXER&)            = delete;
    PA_MIXER& operator=(const PA_MIXER&) = delete;

    static int staticCallback(const void*, void* out, unsigned long frames,
                              const PaStreamCallbackTimeInfo*, unsigned long, void* ud)
    {
        return static_cast<PA_MIXER*>(ud)->mixCallback(out, frames);
    }

    int mixCallback(void* out, unsigned long frameCount)
    {
        const uint32_t fc   = static_cast<uint32_t>(frameCount);
        float*         fOut = static_cast<float*>(out);
        memset(fOut, 0, fc * 2u * sizeof(float));

        // Stack buffers.  rawBuf/floatBuf are sized for up to MIXER_MAX_RATE_RATIO×
        // the output frame count so that sources at higher sample rates fit without
        // heap allocation.  E.g. 96 kHz source into a 44.1 kHz mixer reads ≈2.18×
        // as many source frames as output frames.
        // rawBuf worst case: ratio×fc frames, stereo, 32-bit = ratio×256×2×4 bytes
        // floatBuf: same frame count, float32 stereo
        static const uint32_t MAX_SRC_FRAMES = MIXER_FRAMES_PER_BUF * MIXER_MAX_RATE_RATIO;
        char  rawBuf  [MAX_SRC_FRAMES * 2 * 4];
        float floatBuf[MAX_SRC_FRAMES * 2];

        for (int i = 0; i < MIXER_MAX_SOURCES; ++i)
        {
            if (!m_slots[i].active.load(std::memory_order_acquire)) continue;
            PA_DATA* d = m_slots[i].data;
            if (!d) continue;

            // Calculate how many source frames to read.
            // If source rate == mixer rate: read exactly fc frames (no resampling).
            // If they differ: read proportionally more/fewer source frames, then
            //                 nearest-neighbour resample to fc output frames.
            uint32_t srcFrames = fc;
            bool     needResample = false;
            if (d->m_sampleRate != m_sampleRate && m_sampleRate > 0)
            {
                // Ceiling division to avoid dropping end-of-buffer samples
                srcFrames = (fc * d->m_sampleRate + m_sampleRate - 1) / m_sampleRate;
                if (srcFrames > MAX_SRC_FRAMES)
                    srcFrames = MAX_SRC_FRAMES;   // cap; quality degrades but no crash
                needResample = true;
            }

            const bool ended = d->readFrames(rawBuf, srcFrames);

            convertToFloat32Stereo(rawBuf, d->m_sampleFormat, d->m_numChannels, srcFrames, floatBuf);

            // Per-source linear volume + pan applied as L/R gains
            const float vol       = static_cast<float>(d->m_volume);
            const float leftGain  = (d->m_pan <= 0.0)
                                        ? vol
                                        : static_cast<float>(vol * (1.0 - d->m_pan));
            const float rightGain = (d->m_pan >= 0.0)
                                        ? vol
                                        : static_cast<float>(vol * (1.0 + d->m_pan));

            if (needResample)
            {
                // Nearest-neighbour resampling: map each output frame f → source frame
                const float step = static_cast<float>(srcFrames) / static_cast<float>(fc);
                for (uint32_t f = 0; f < fc; ++f)
                {
                    uint32_t sf = static_cast<uint32_t>(static_cast<float>(f) * step);
                    if (sf >= srcFrames) sf = srcFrames - 1u;
                    fOut[f * 2]     += floatBuf[sf * 2]     * leftGain;
                    fOut[f * 2 + 1] += floatBuf[sf * 2 + 1] * rightGain;
                }
            }
            else
            {
                for (uint32_t f = 0; f < fc; ++f)
                {
                    fOut[f * 2]     += floatBuf[f * 2]     * leftGain;
                    fOut[f * 2 + 1] += floatBuf[f * 2 + 1] * rightGain;
                }
            }

            if (ended)
            {
                m_slots[i].active.store(false, std::memory_order_release);
                d->m_finished.store(true, std::memory_order_release);
            }
        }

        // Clamp mixed output to [-1, 1] to prevent clipping artefacts
        for (uint32_t s = 0; s < fc * 2u; ++s)
        {
            if      (fOut[s] >  1.0f) fOut[s] =  1.0f;
            else if (fOut[s] < -1.0f) fOut[s] = -1.0f;
        }

        m_callbackSeq.fetch_add(1, std::memory_order_release);
        return paContinue;
    }

    struct MixerSlot
    {
        PA_DATA*          data{nullptr};
        std::atomic<bool> active{false};
        bool              registered{false};
    };

    PaStream*            m_stream{nullptr};
    MixerSlot            m_slots[MIXER_MAX_SOURCES]{};
    std::atomic<int64_t> m_callbackSeq{0};
    bool                 m_initialized{false};
    uint32_t             m_sampleRate{44100};
};

} // anonymous namespace

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
    , m_loop(false)
    , m_paused(false)
    , m_volume(1.0)
    , m_pan(0.0)
    , m_finished(false)
{}

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

bool PA_DATA_MEMORY::readFrames(void* buf, uint32_t frameCount)
{
    const uint32_t bytesNeeded = m_bytesPerSample * frameCount;
    const uint32_t totalBytes  = static_cast<uint32_t>(m_sample.size());
    const uint32_t remaining   = (m_index < totalBytes) ? (totalBytes - m_index) : 0u;
    char*          out         = static_cast<char*>(buf);

    if (bytesNeeded <= remaining)
    {
        memcpy(out, &m_sample[m_index], bytesNeeded);
        m_index += bytesNeeded;
        return false;
    }
    if (remaining > 0)
        memcpy(out, &m_sample[m_index], remaining);
    if (m_loop)
    {
        m_index = 0;
        uint32_t left = bytesNeeded - remaining;
        if (left > totalBytes) left = totalBytes;
        if (left > 0)
            memcpy(out + remaining, &m_sample[0], left);
        m_index = left;
        return false;
    }
    else
    {
        const uint32_t pad = bytesNeeded - remaining;
        if (m_sampleFormat == paUInt8)
            memset(out + remaining, 128, pad);  // unsigned 8-bit silence
        else
            memset(out + remaining, 0, pad);
        m_index = totalBytes;
        return true; // stream ended
    }
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

bool PA_DATA_FILE::readFrames(void* buf, uint32_t frameCount)
{
    const uint32_t bytesNeeded = m_bytesPerSample * frameCount;
    char*          out         = static_cast<char*>(buf);
    size_t         bytesRead   = 0;

    wave->ReadRaw(out, bytesNeeded, bytesRead);
    m_bytesRead += static_cast<uint32_t>(bytesRead);

    if (static_cast<uint32_t>(bytesRead) < bytesNeeded)
    {
        if (m_loop)
        {
            wave->ResetToStart();
            m_bytesRead = 0;
            const uint32_t deficit = bytesNeeded - static_cast<uint32_t>(bytesRead);
            size_t bytesRead2 = 0;
            wave->ReadRaw(out + bytesRead, deficit, bytesRead2);
            m_bytesRead += static_cast<uint32_t>(bytesRead2);
            if (bytesRead2 < deficit)
                memset(out + bytesRead + bytesRead2, 0, deficit - bytesRead2);
            return false;
        }
        else
        {
            memset(out + bytesRead, 0, bytesNeeded - static_cast<uint32_t>(bytesRead));
            return true; // stream ended
        }
    }
    return false;
}

/* ---------------------------------------------------------------------------
 * PA_INTERFACE — construction / destruction
 * -------------------------------------------------------------------------*/

PA_INTERFACE::PA_INTERFACE() : m_data(nullptr), m_slotIndex(-1) {}

PA_INTERFACE::~PA_INTERFACE()
{
    if (m_data)
    {
        PA_MIXER::instance().unregisterSource(m_slotIndex, m_data);
        m_slotIndex = -1;
        m_data      = nullptr;
    }
}

/* ---------------------------------------------------------------------------
 * PA_INTERFACE::openStream  (memory-backed)
 * -------------------------------------------------------------------------*/

bool PA_INTERFACE::openStream(const uint16_t numChannels, const uint16_t sampleFormat,
                              const uint16_t bitsPerChannel, const uint32_t sampleRate,
                              const uint16_t bytesPerSample, const uint32_t dataLength,
                              const uint32_t bytesPerSecond, std::vector<char>&& sample)
{
    PA_MIXER& mixer = PA_MIXER::instance();
    if (!mixer.init(sampleRate)) return false;

    auto* data = new PA_DATA_MEMORY(numChannels, sampleFormat, bitsPerChannel,
                                    sampleRate, bytesPerSample,
                                    dataLength, bytesPerSecond,
                                    std::move(sample));
    m_slotIndex = mixer.registerSource(data);
    if (m_slotIndex < 0)
    {
        delete data;
        return false;
    }
    m_data = data;
    return true;
}

/* ---------------------------------------------------------------------------
 * PA_INTERFACE::openStream  (file-streaming)
 * -------------------------------------------------------------------------*/

bool PA_INTERFACE::openStream(WaveFile* wave)
{
    PA_MIXER& mixer = PA_MIXER::instance();
    if (!mixer.init(wave->GetSampleRate())) return false;

    auto* data = new PA_DATA_FILE(wave);
    m_slotIndex = mixer.registerSource(data);
    if (m_slotIndex < 0)
    {
        delete data;
        return false;
    }
    m_data = data;
    return true;
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
    if (!m_data || m_slotIndex < 0 || !PA_MIXER::instance().isInitialized()) return false;
    m_data->m_paused = false;
    m_data->m_finished.store(false, std::memory_order_release);
    PA_MIXER::instance().activate(m_slotIndex);
    return true;
}

bool PA_INTERFACE::stop()
{
    if (!m_data) return true;
    m_data->m_paused = false;
    m_data->m_finished.store(false, std::memory_order_release);
    if (m_slotIndex >= 0)
        PA_MIXER::instance().deactivate(m_slotIndex);
    m_data->setPosition(0.0);   // rewind so next play() starts from beginning
    return true;
}

bool PA_INTERFACE::pause()
{
    if (!m_data || m_data->m_paused || m_slotIndex < 0) return false;
    if (!PA_MIXER::instance().isActive(m_slotIndex)) return false;
    PA_MIXER::instance().deactivate(m_slotIndex);
    m_data->m_paused = true;
    return true;
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
    if (!m_data || m_data->m_paused || m_slotIndex < 0) return false;
    return PA_MIXER::instance().isActive(m_slotIndex);
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