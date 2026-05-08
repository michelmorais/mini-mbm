#ifndef _PA_INTERFACE_H_
#define _PA_INTERFACE_H_

#include <vector>
#include <atomic>
#include <stdio.h>
#include <WAVE.h>
#include <stdint.h>

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
 * PA_DATA — base class holding per-source state for the global software mixer.
 * No per-source PaStream is opened; all sources share one output stream.
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
    bool                m_loop;
    bool                m_paused;
    double              m_volume;           // 0.0 - 1.0 linear
    double              m_pan;              // -1.0 (left) .. 0.0 (centre) .. +1.0 (right)
    std::atomic<bool>   m_finished;         // set by mixer when a non-loop stream ends

    PA_DATA(uint16_t numChannels, uint16_t sampleFormat, uint16_t bitsPerChannel,
            uint32_t sampleRate, uint16_t bytesPerSample,
            uint32_t dataLength, uint32_t bytesPerSecond);
    virtual ~PA_DATA() = default;

    virtual void   setPosition(double pos) = 0;
    virtual double getPosition()     const = 0;

    /* Read exactly frameCount frames of raw PCM (m_bytesPerSample * frameCount
     * bytes) into buf.  Handles looping and silence-padding at end of stream.
     * Returns true when the stream ended (non-looping, data exhausted). */
    virtual bool readFrames(void* buf, uint32_t frameCount) = 0;

    /* Legacy helpers retained for source compatibility; no longer called by
     * the mixer (volume/pan are applied inline during float32 conversion). */
    void adjustVolume(void* data, uint32_t sizeInBytes, double volume);
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
    bool   readFrames(void* buf, uint32_t frameCount) override;
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
    bool   readFrames(void* buf, uint32_t frameCount) override;
};

/*
 * PA_INTERFACE — interface to the global software mixer.
 *
 * All PA_INTERFACE objects share a single PaStream (opened at first sound
 * load).  play()/stop() just activate/deactivate a slot in the mixer with
 * no OS audio calls — eliminating the per-sound ALSA stream overhead that
 * causes frame drops when many short sounds play simultaneously.
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
    /* Register a memory-backed source with the global mixer. */
    bool openStream(uint16_t numChannels, uint16_t sampleFormat, uint16_t bitsPerChannel,
                    uint32_t sampleRate, uint16_t bytesPerSample,
                    uint32_t dataLength, uint32_t bytesPerSecond,
                    std::vector<char>&& sample);

    /* Register a file-streaming source with the global mixer. */
    bool openStream(WaveFile* wave);

    PA_DATA*  m_data;
    int       m_slotIndex;   // index into PA_MIXER slot array; -1 = not registered
};

#endif /* _PA_INTERFACE_H_ */