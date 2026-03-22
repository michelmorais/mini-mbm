/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015-2026  by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
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

/*
 * Audio backend: AVFoundation (macOS / Apple)
 *
 * Natively supported formats (via AVAudioFile):
 *   WAV, AIFF, CAF, AU, MP3, AAC/M4A, FLAC (macOS 10.13+)
 *
 * OGG/Vorbis support (via stb_vorbis, compiled as a separate C object):
 *   stb_vorbis decodes the OGG file to interleaved PCM shorts, which are
 *   then wrapped in an AVAudioPCMBuffer and played through AVAudioEngine.
 *
 * CMake flag: -DAUDIO=avfoundation   (default on Apple when AUDIO is not set)
 * Framework links: AVFoundation, AudioToolbox
 */

#if defined(AUDIO_ENGINE_AVFOUNDATION)

#import <AVFoundation/AVFoundation.h>
#include <CoreAudio/CoreAudioTypes.h>

#include <audio.h>
#include <core-manager.h>
#include <util-interface.h>

#include <string>
#include <memory>
#include <atomic>
#include <cstdlib>   // free()
#include <cstring>   // memcpy

// ---------------------------------------------------------------------------
// stb_vorbis (compiled separately as a C object via the third-party/stb glob)
// Declare only the function signatures we use; the linker resolves them.
// ---------------------------------------------------------------------------
extern "C" {
    int stb_vorbis_decode_filename(const char *filename,
                                   int *channels,
                                   int *sample_rate,
                                   short **output);
}

// ---------------------------------------------------------------------------
// Global AVAudioEngine — shared across all AUDIO instances
// ---------------------------------------------------------------------------
static AVAudioEngine *g_avf_engine = nil;

extern "C" void avfoundation_audio_init(void)
{
    if (g_avf_engine == nil) {
        g_avf_engine = [[AVAudioEngine alloc] init];
        // Pre-warm mainMixerNode here, on the main thread, before any call to
        // AUDIO::load(). The first access to mainMixerNode triggers lazy creation
        // of an AVAudioMixerNode and its underlying AUAudioUnitV2Bridge, which
        // internally builds an AudioUnit parameter tree via AudioUnitGetProperty.
        // If that first access happens inside AUDIO::load() (which may be called
        // from a Lua coroutine mid-frame), the AudioUnit bridge can throw an
        // ObjC exception that propagates uncaught through the C++ Lua stack and
        // terminates the app. Accessing it once here prevents that cold-init path
        // from being hit inside load().
        @try {
            (void)g_avf_engine.mainMixerNode;
        } @catch (NSException *ex) {
            NSLog(@"[mini-mbm] AVAudioEngine mainMixerNode pre-warm failed: %@", ex.reason);
            g_avf_engine = nil;
        }
    }
}

extern "C" void avfoundation_audio_release(void)
{
    if (g_avf_engine) {
        [g_avf_engine stop];
        g_avf_engine = nil;
    }
}

// ---------------------------------------------------------------------------
// AVFAudioData — per-instance data (ObjC++ PIMPL; defined here, forward-
// declared in audio.h so the C++ header stays ObjC-free)
// ---------------------------------------------------------------------------
struct mbm::AUDIO::AVFAudioData
{
    AVAudioPlayerNode  *node     = nil;
    AVAudioPCMBuffer   *buffer   = nil;
    bool                loaded   = false;
    float               volume   = 1.0f;
    // Set to true by the audio-render thread when a non-looping buffer finishes.
    // AVAudioPlayerNode.isPlaying stays true after the buffer ends (it reflects
    // the node's play-state, not active rendering), so we track this ourselves.
    std::atomic<bool>   finished { false };
};

// ---------------------------------------------------------------------------
// mbm::AUDIO methods
// ---------------------------------------------------------------------------
namespace mbm
{

AUDIO::AUDIO(const int newIdScene)
    : AUDIO_INTERFACE(newIdScene), onEndStreamCallBack(nullptr)
{}

AUDIO::~AUDIO()
{
    if (avf_data && avf_data->node && g_avf_engine) {
        [avf_data->node stop];
        [g_avf_engine detachNode:avf_data->node];
    }
}

// ----------------------------------------------------------------------- load

bool AUDIO::load(const char *filenameSound, const bool /*loop*/, const bool /*inMemory*/)
{
    if (!filenameSound) {
        ERROR_LOG("file name is [null]");
        return false;
    }

    if (!g_avf_engine)
        avfoundation_audio_init();

    bool found = false;
    const char *fullPath = util::getFullPath(filenameSound, &found);
    if (!found || !fullPath) {
        ERROR_LOG("file not found: %s", filenameSound);
        return false;
    }

    avf_data = std::make_unique<AVFAudioData>();

    NSString *nsPath = [NSString stringWithUTF8String:fullPath];

    // AVFoundation does not support the OGG container. stb_vorbis handles
    // OGG Vorbis, but files converted for Android are often OGG Opus which
    // stb_vorbis cannot decode. Detect OGG Opus by reading the stream header
    // and, if found, fall back to the .wav counterpart in the same directory.
    const bool isOgg =
        [[nsPath.pathExtension lowercaseString] isEqualToString:@"ogg"];

    if (isOgg) {
        // Read the first 64 bytes to check for "OpusHead" magic.
        bool isOpus = false;
        if (FILE *f = fopen(fullPath, "rb")) {
            uint8_t hdr[64] = {};
            fread(hdr, 1, sizeof(hdr), f);
            fclose(f);
            // OGG Opus first-page header contains the string "OpusHead"
            for (int i = 0; i <= (int)sizeof(hdr) - 8; ++i) {
                if (memcmp(hdr + i, "OpusHead", 8) == 0) { isOpus = true; break; }
            }
        }
        if (isOpus) {
            // Swap the .ogg extension for .wav and try again.
            NSString *wavPath = [[nsPath stringByDeletingPathExtension]
                                  stringByAppendingPathExtension:@"wav"];
            bool wavFound = false;
            const char *wavFullPath = util::getFullPath([wavPath UTF8String], &wavFound);
            if (wavFound && wavFullPath) {
                NSLog(@"[mini-mbm] OGG Opus not supported natively; falling back to %s", wavFullPath);
                avf_data.reset();
                return load(wavFullPath, false, false);
            } else {
                ERROR_LOG("OGG Opus not supported and no .wav fallback found for: %s", filenameSound);
                avf_data.reset();
                return false;
            }
        }
    }

    NSURL *url = [NSURL fileURLWithPath:nsPath];

    AVAudioFormat    *format    = nil;
    AVAudioPCMBuffer *pcmBuffer = nil;

    if (isOgg) {
        // ------------------------------------------------------------------
        // OGG path — decode via stb_vorbis → AVAudioPCMBuffer (int16)
        // ------------------------------------------------------------------
        int    channels   = 0;
        int    sampleRate = 0;
        short *samples    = nullptr;

        const int totalSamples =
            stb_vorbis_decode_filename(fullPath, &channels, &sampleRate, &samples);

        if (totalSamples <= 0 || !samples) {
            ERROR_LOG("stb_vorbis failed to decode: %s", filenameSound);
            avf_data.reset();
            return false;
        }

        const AVAudioChannelCount ch = (AVAudioChannelCount)channels;
        AudioChannelLayoutTag tag =
            (ch == 1) ? kAudioChannelLayoutTag_Mono : kAudioChannelLayoutTag_Stereo;
        AVAudioChannelLayout *layout =
            [[AVAudioChannelLayout alloc] initWithLayoutTag:tag];

        format = [[AVAudioFormat alloc]
            initWithCommonFormat:AVAudioPCMFormatInt16
                      sampleRate:(double)sampleRate
                   interleaved:YES
                   channelLayout:layout];

        const AVAudioFrameCount frameCount = (AVAudioFrameCount)(totalSamples / channels);
        pcmBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:format
                                                  frameCapacity:frameCount];
        pcmBuffer.frameLength = frameCount;

        // int16ChannelData[0] holds all interleaved samples for interleaved formats
        int16_t *dst = (int16_t *)pcmBuffer.int16ChannelData[0];
        memcpy(dst, samples, (size_t)totalSamples * sizeof(short));
        free(samples);

    } else {
        // ------------------------------------------------------------------
        // Native path — let AVAudioFile handle the format (WAV, MP3, AAC, …)
        // ------------------------------------------------------------------
        NSError    *error  = nil;
        AVAudioFile *avFile = [[AVAudioFile alloc] initForReading:url error:&error];
        if (!avFile) {
            ERROR_LOG("AVAudioFile could not open: %s  (%s)",
                      filenameSound,
                      error ? [error.localizedDescription UTF8String] : "unknown error");
            avf_data.reset();
            return false;
        }

        format                 = avFile.processingFormat;
        const AVAudioFrameCount frameCount = (AVAudioFrameCount)avFile.length;
        pcmBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:format
                                                   frameCapacity:frameCount];

        if (![avFile readIntoBuffer:pcmBuffer error:&error]) {
            ERROR_LOG("AVAudioFile read failed: %s  (%s)",
                      filenameSound,
                      error ? [error.localizedDescription UTF8String] : "unknown error");
            avf_data.reset();
            return false;
        }
    }

    // Attach a new player node to the shared engine.
    // Wrapped in @try/@catch: AVAudioEngine graph mutations can throw ObjC
    // exceptions on audio route changes or hardware permission errors; catching
    // them here prevents propagation through the C++ Lua call stack.
    avf_data->buffer = pcmBuffer;
    avf_data->node   = [[AVAudioPlayerNode alloc] init];

    @try {
        [g_avf_engine attachNode:avf_data->node];
        [g_avf_engine connect:avf_data->node
                           to:g_avf_engine.mainMixerNode
                       format:format];

        // Start (or restart) the engine if it is not already running.
        // This covers the first load() call and also audio-route changes
        // (e.g. headphones plugged/unplugged) that stop the engine.
        if (![g_avf_engine isRunning]) {
            NSError *startError = nil;
            if (![g_avf_engine startAndReturnError:&startError]) {
                ERROR_LOG("AVAudioEngine failed to start: %s",
                          startError ? [startError.localizedDescription UTF8String] : "unknown");
                [g_avf_engine detachNode:avf_data->node];
                avf_data.reset();
                return false;
            }
        }
    } @catch (NSException *ex) {
        ERROR_LOG("AVAudioEngine graph setup threw exception: %s",
                  [ex.reason UTF8String] ? [ex.reason UTF8String] : "unknown");
        avf_data.reset();
        return false;
    }

    avf_data->loaded = true;
    const char *baseName = util::getBaseName(fullPath);
    this->fileName = baseName ? baseName : "";
    return true;
}

// ----------------------------------------------------------------------- play

bool AUDIO::play(const bool loop)
{
    if (!avf_data || !avf_data->buffer || !avf_data->node)
        return false;

    [avf_data->node stop];
    avf_data->finished = false;   // reset for this playback

    if (loop) {
        // Looping buffers run until stop() is called — no completion needed.
        [avf_data->node scheduleBuffer:avf_data->buffer
                                atTime:nil
                               options:AVAudioPlayerNodeBufferLoops
                     completionHandler:nil];
    } else {
        // For non-looping buffers we ALWAYS attach a completion handler so that
        // `finished` is set when the render thread drains the last sample.
        // Without this, AVAudioPlayerNode.isPlaying stays true after the buffer
        // ends (it tracks the node's play-state, not active rendering), causing
        // isPlaying() to return true forever and starving the sound pool.
        AVFAudioData *rawData = avf_data.get();   // raw ptr safe: [node stop] in dtor cancels the handler
        AUDIO        *self    = this;
        [avf_data->node scheduleBuffer:avf_data->buffer
                                atTime:nil
                               options:0
                     completionHandler:^{
            rawData->finished = true;
            if (self->onEndStreamCallBack) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (self->onEndStreamCallBack)
                        self->onEndStreamCallBack(self);
                });
            }
        }];
    }

    [avf_data->node play];
    state = AUDIO_PLAYING;
    return true;
}

// -------------------------------------------------------------------- control

bool AUDIO::pause()
{
    if (avf_data && avf_data->node && avf_data->node.isPlaying) {
        state = AUDIO_PLAYING;   // remember we were playing for resume()
        [avf_data->node pause];
        return true;
    }
    return false;
}

bool AUDIO::resume()
{
    if (avf_data && avf_data->node && state == AUDIO_PLAYING) {
        [avf_data->node play];
        return true;
    }
    return false;
}

bool AUDIO::stop()
{
    if (avf_data && avf_data->node) {
        [avf_data->node stop];
        state = AUDIO_STOPPED;
        return true;
    }
    return false;
}

bool AUDIO::setVolume(const float volume)
{
    if (avf_data && avf_data->node) {
        avf_data->node.volume = volume;
        avf_data->volume      = volume;
        return true;
    }
    return false;
}

bool AUDIO::setPan(const float pan)
{
    if (avf_data && avf_data->node) {
        avf_data->node.pan = pan;
        return true;
    }
    return false;
}

bool AUDIO::setPitch(const float)
{
    ERROR_AT(__LINE__, __FILE__, "AUDIO::setPitch not implemented for AVFoundation");
    return false;
}

bool AUDIO::setPosition(const int)
{
    ERROR_AT(__LINE__, __FILE__,
             "AUDIO::setPosition not implemented for AVFoundation — "
             "use stop() + play() to restart from the beginning");
    return false;
}

// -------------------------------------------------------------------- queries

bool AUDIO::isPlaying()
{
    if (!avf_data || !avf_data->loaded || state != AUDIO_PLAYING)
        return false;
    // Once the render thread signals the buffer is done, transition state so
    // repeated calls don't need to re-read the atomic.
    if (avf_data->finished) {
        state = AUDIO_STOPPED;
        return false;
    }
    return true;
}

bool AUDIO::isPaused()
{
    // Paused = in AUDIO_PLAYING state but node is not actively rendering.
    return avf_data && avf_data->node &&
           state == AUDIO_PLAYING &&
           !avf_data->finished &&
           !avf_data->node.isPlaying;
}

bool AUDIO::isLoaded()
{
    return avf_data && avf_data->loaded;
}

float AUDIO::getVolume()
{
    return (avf_data && avf_data->node) ? (float)avf_data->node.volume : 0.0f;
}

float AUDIO::getPan()
{
    return (avf_data && avf_data->node) ? (float)avf_data->node.pan : 0.0f;
}

float AUDIO::getPitch()
{
    return 1.0f;   // not supported; return neutral value
}

int AUDIO::getLength()
{
    return (avf_data && avf_data->buffer)
               ? (int)avf_data->buffer.frameLength
               : 0;
}

bool AUDIO::reset()
{
    if (avf_data && avf_data->buffer) {
        stop();
        return play(false);
    }
    return false;
}

void AUDIO::setOnEndstream(OnEndStreamCallBack cb)
{
    this->onEndStreamCallBack = cb;
}

AUDIO::OnEndStreamCallBack AUDIO::getOnEndstream() const
{
    return this->onEndStreamCallBack;
}

const char *AUDIO::getFileName() const noexcept
{
    return this->fileName.c_str();
}

// -------------------------------------------------------------------- version

const char *AUDIO_ENGINE_version()
{
    return "AVFoundation";
}

}  // namespace mbm

#endif  // AUDIO_ENGINE_AVFOUNDATION
