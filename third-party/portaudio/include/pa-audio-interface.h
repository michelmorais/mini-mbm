#ifndef _PA_INTERFACE_H_
#define _PA_INTERFACE_H_

#include <vector>
#include <atomic>
#include <stdio.h>
#include <WAVE.h>
#include <stdint.h>

struct PaStreamParameters;
struct PaStreamCallbackTimeInfo;

/*
 * Translate a WAVE format type + bits-per-channel to the matching PortAudio
 * PaSampleFormat constant.
 *
 * WAVE format type 1 = PCM.  PaSampleFormat is determined by bit depth:
 *   8-bit  -> paUInt8  (unsigned, centred at 128)
 *   16-bit -> paInt16
 *   24-bit -> paInt24  (packed 3-byte little-endian)
 *   32-bit -> paInt32
 * WAVE format type 3 = IEEE float -> paFloat32.
 * All other types default to paFloat32.
 */
uint32_t TranslateFormatType(uint16_t sampleFormat, uint16_t bitsPerChannel);

/*
 * PA_DATA — base class holding per-stream state shared by both memory and
 *           file-backed playback variants.
 */
class PA_DATA
{
public:
    uint16_t            m_numChannels;      // channels (1 = mono, 2 = stereo)
    uint32_t            m_sampleFormat;     // PaSampleFormat constant
    uint16_t            m_bitsPerChannel;   // actual bit depth (8/16/24/32)
    uint32_t            m_sampleRate;       // samples per second
    uint16_t            m_bytesPerSample;   // blockAlign = numChannels * bytesPerChannel
    uint32_t            m_durationMs;       // total duration in milliseconds
    void*               m_stream;           // PaStream* (void* to avoid portaudio.h in header)
    bool                m_loop;
    bool                m_paused;
    double              m_volume;           // 0.0 - 1.0 linear
    double              m_pan;              // -1.0 (left) .. 0.0 (centre) .. +1.0 (right)
    std::atomic<bool>   m_finished;         // set by callback thread when non-loop stream ends
    PaStreamParameters* m_outputParameters;

    PA_DATA(uint16_t numChannels, uint16_t sampleFormat, uint16_t bitsPerChannel,
            uint32_t sampleRate, uint16_t bytesPerSample,
            uint32_t dataLength, uint32_t bytesPerSecond);
    virtual ~PA_DATA();

    virtual void   setPosition(double pos) = 0;
    virtual double getPosition()     const = 0;

    /* Apply linear volume scaling to a rendered buffer.
     * Operates on the correct sample type based on m_sampleFormat. */
    void adjustVolume(void* data, uint32_t sizeInBytes, double volume);

    /* Apply stereo panning using a linear left/right gain model.
     * No-op for mono or when m_pan == 0.0. */
    void applyPan(void* data, uint32_t frames);
};

/*
 * PA_DATA_MEMORY — entire audio stored in a RAM buffer.
 */
class PA_DATA_MEMORY : public PA_DATA
{
public:
    std::vector<char> m_sample;   // raw PCM bytes
    uint32_t          m_index;    // current read position in bytes

    PA_DATA_MEMORY(uint16_t numChannels, uint16_t sampleFormat, uint16_t bitsPerChannel,
                   uint32_t sampleRate, uint16_t bytesPerSample,
                   uint32_t dataLength, uint32_t bytesPerSecond,
                   std::vector<char>&& sample);
    virtual ~PA_DATA_MEMORY() = default;

    void   setPosition(double pos) override;
    double getPosition()     const override;
};

/*
 * PA_DATA_FILE — streaming directly from a WaveFile on disk.
 */
class PA_DATA_FILE : public PA_DATA
{
public:
    WaveFile* wave;
    long      m_dataStart;   // file offset at the beginning of the PCM data chunk
    uint32_t  m_bytesRead;   // bytes consumed since last rewind

    explicit PA_DATA_FILE(WaveFile* wave);
    virtual ~PA_DATA_FILE() = default;

    void   setPosition(double pos) override;
    double getPosition()     const override;
};

/*
 * PA_INTERFACE — PortAudio stream management.
 */
class PA_INTERFACE
{
public:
    PA_INTERFACE();
    virtual ~PA_INTERFACE();

    virtual bool play(bool bLoop);  // subclasses override to stop+rewind first
    bool   start();
    bool   stop();
    bool   pause();
    void   setPosition(double pos);
    double getPosition()  const;
    void   setLoop(bool bLoop);
    bool   isPlaying()    const;
    bool   isPaused()     const;
    bool   isFinished()   const;
    void   clearFinished();
    void   setVolume(double volume);
    double getVolume();
    void   setPan(double pan);
    double getPan()       const;
    int    getLength()    const;

protected:
    /* Open a memory-backed stream.  sampleFormat is the raw WAVE format type
     * (1 = PCM, 3 = float); translation to PaSampleFormat happens inside
     * PA_DATA's constructor using bitsPerChannel. */
    bool openStream(uint16_t numChannels, uint16_t sampleFormat, uint16_t bitsPerChannel,
                    uint32_t sampleRate, uint16_t bytesPerSample,
                    uint32_t dataLength, uint32_t bytesPerSecond,
                    std::vector<char>&& sample);

    /* Open a file-streaming stream. */
    bool openStream(WaveFile* wave);

    static bool  initialized;
    PA_DATA*     m_data;

private:
    static int paStreamCallback(
        const void* input, void* output, unsigned long frameCount,
        const PaStreamCallbackTimeInfo* timeInfo, unsigned long statusFlags,
        void* userData);

    static int paStreamCallbackFromFile(
        const void* input, void* output, unsigned long frameCount,
        const PaStreamCallbackTimeInfo* timeInfo, unsigned long statusFlags,
        void* userData);
};

#endif /* _PA_INTERFACE_H_ */