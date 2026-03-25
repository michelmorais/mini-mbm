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

#if defined(AUDIO_ENGINE_ANDROID_OPENSL)
#if defined(ANDROID)
#if defined(USE_OPENGL_ES)

#include <audio.h>
#include <device.h>
#include <core-manager.h>
#include <util-interface.h>
#include <specific-opengl_es.h>

#include <android/asset_manager.h>
#include <android/log.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <mutex>
#include <cmath>

#define OPENSL_TAG "mini-mbm/OpenSL"
#define OPENSL_ERR(...) __android_log_print(ANDROID_LOG_ERROR, OPENSL_TAG, __VA_ARGS__)

namespace mbm
{

// ─────────────────────────────────────────────────────────────────────────────
// Global OpenSL ES engine (shared; ref-counted across all AUDIO instances)
// ─────────────────────────────────────────────────────────────────────────────
static SLObjectItf  g_engineObj    = nullptr;
static SLEngineItf  g_engineIf     = nullptr;
static SLObjectItf  g_outputMixObj = nullptr;
static int          g_refCount     = 0;
static std::mutex   g_engineMutex;

static bool opensl_init_engine()
{
    if (g_engineObj != nullptr) {
        ++g_refCount;
        return true;
    }
    SLresult r = slCreateEngine(&g_engineObj, 0, nullptr, 0, nullptr, nullptr);
    if (r != SL_RESULT_SUCCESS) {
        OPENSL_ERR("slCreateEngine failed: %d", (int)r);
        g_engineObj = nullptr;
        return false;
    }
    r = (*g_engineObj)->Realize(g_engineObj, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) {
        (*g_engineObj)->Destroy(g_engineObj); g_engineObj = nullptr;
        return false;
    }
    r = (*g_engineObj)->GetInterface(g_engineObj, SL_IID_ENGINE, &g_engineIf);
    if (r != SL_RESULT_SUCCESS) {
        (*g_engineObj)->Destroy(g_engineObj); g_engineObj = nullptr;
        return false;
    }
    r = (*g_engineIf)->CreateOutputMix(g_engineIf, &g_outputMixObj, 0, nullptr, nullptr);
    if (r != SL_RESULT_SUCCESS) {
        (*g_engineObj)->Destroy(g_engineObj); g_engineObj = nullptr;
        return false;
    }
    r = (*g_outputMixObj)->Realize(g_outputMixObj, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) {
        (*g_outputMixObj)->Destroy(g_outputMixObj); g_outputMixObj = nullptr;
        (*g_engineObj)->Destroy(g_engineObj); g_engineObj = nullptr;
        return false;
    }
    ++g_refCount;
    return true;
}

static void opensl_release_engine()
{
    if (--g_refCount <= 0) {
        if (g_outputMixObj) { (*g_outputMixObj)->Destroy(g_outputMixObj); g_outputMixObj = nullptr; }
        if (g_engineObj)    { (*g_engineObj)->Destroy(g_engineObj); g_engineObj = nullptr; g_engineIf = nullptr; }
        g_refCount = 0;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-instance state (stored in AUDIO::userData)
// ─────────────────────────────────────────────────────────────────────────────
struct OSLPlayer {
    SLObjectItf playerObj  = nullptr;
    SLPlayItf   playIf     = nullptr;
    SLVolumeItf volumeIf   = nullptr;
    SLSeekItf   seekIf     = nullptr;
    AAsset*     asset      = nullptr; // kept open to maintain a valid FD while playing
    bool        paused     = false;
    float       volumeF    = 1.0f;
    float       panF       = 0.0f;
};

// volume [0,1] → SL millibel
static SLmillibel float_to_mb(float v)
{
    if (v <= 0.0f) return SL_MILLIBEL_MIN;
    SLmillibel mb = static_cast<SLmillibel>(2000.0f * log10f(v));
    if (mb < SL_MILLIBEL_MIN) mb = SL_MILLIBEL_MIN;
    if (mb > 0)               mb = 0;
    return mb;
}

// ─────────────────────────────────────────────────────────────────────────────
// AUDIO implementation
// ─────────────────────────────────────────────────────────────────────────────
AUDIO::AUDIO(const int newIdScene)
    : AUDIO_INTERFACE(newIdScene)
    , onEndStreamCallBack(nullptr)
{}

AUDIO::~AUDIO()
{
    std::lock_guard<std::mutex> lk(g_engineMutex);
    auto* d = static_cast<OSLPlayer*>(this->userData);
    if (d) {
        if (d->playerObj) {
            (*d->playerObj)->Destroy(d->playerObj);
            d->playerObj = nullptr;
        }
        if (d->asset) {
            AAsset_close(d->asset);
            d->asset = nullptr;
        }
        delete d;
        this->userData = nullptr;
    }
    opensl_release_engine();
    fileName.clear();
}

bool AUDIO::load(const char* filenameSound, const bool loop, const bool inMemory)
{
    (void)inMemory; // OpenSL ES always streams from the asset FD
    if (!filenameSound) return false;
    if (isLoaded()) return true;

    std::lock_guard<std::mutex> lk(g_engineMutex);

    if (!opensl_init_engine()) return false;

    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    AAssetManager* mgr  = device->specificContextDevice->assetManager;
    if (!mgr) { OPENSL_ERR("AAssetManager is null"); return false; }

    // Strip leading slashes if present; AAssetManager_open expects a relative path.
    const char* relPath = filenameSound;
    while (*relPath == '/') ++relPath;

    AAsset* asset = AAssetManager_open(mgr, relPath, AASSET_MODE_UNKNOWN);
    if (!asset) {
        // The Lua side may pass just "01.wav" while the file lives in "sounds/01.wav".
        // Search through the engine's registered paths (mbm.addPath) to find it.
        std::vector<std::string> paths;
        util::getAllPaths(paths);
        for (const auto& dir : paths) {
            std::string tryPath = dir + "/" + relPath;
            // Strip leading slashes — AAssetManager expects relative paths.
            const char* p = tryPath.c_str();
            while (*p == '/') ++p;
            asset = AAssetManager_open(mgr, p, AASSET_MODE_UNKNOWN);
            if (asset) break;
        }
    }
    if (!asset) {
        OPENSL_ERR("Cannot open asset: %s", filenameSound);
        return false;
    }

    off_t start = 0, length = 0;
    int fd = AAsset_openFileDescriptor(asset, &start, &length);
    if (fd < 0) {
        AAsset_close(asset);
        OPENSL_ERR("AAsset_openFileDescriptor failed for: %s", filenameSound);
        return false;
    }

    // Data source: Android FD locator
    SLDataLocator_AndroidFD fdLoc = { SL_DATALOCATOR_ANDROIDFD, fd, start, length };
    SLDataFormat_MIME fmtMime     = { SL_DATAFORMAT_MIME, nullptr, SL_CONTAINERTYPE_UNSPECIFIED };
    SLDataSource src              = { &fdLoc, &fmtMime };

    // Data sink: output mix
    SLDataLocator_OutputMix outLoc = { SL_DATALOCATOR_OUTPUTMIX, g_outputMixObj };
    SLDataSink sink                = { &outLoc, nullptr };

    // Request volume and (optionally) seek interfaces
    const SLInterfaceID ids[]   = { SL_IID_VOLUME, SL_IID_SEEK };
    const SLboolean     reqs[]  = { SL_BOOLEAN_TRUE, SL_BOOLEAN_FALSE };

    auto* d = new OSLPlayer();
    d->asset = asset;

    SLresult r = (*g_engineIf)->CreateAudioPlayer(g_engineIf, &d->playerObj, &src, &sink,
                                                   2, ids, reqs);
    if (r != SL_RESULT_SUCCESS) {
        OPENSL_ERR("CreateAudioPlayer failed: %d", (int)r);
        AAsset_close(asset);
        delete d;
        return false;
    }

    r = (*d->playerObj)->Realize(d->playerObj, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) {
        OPENSL_ERR("Realize player failed: %d", (int)r);
        (*d->playerObj)->Destroy(d->playerObj);
        AAsset_close(asset);
        delete d;
        return false;
    }

    (*d->playerObj)->GetInterface(d->playerObj, SL_IID_PLAY,   &d->playIf);
    (*d->playerObj)->GetInterface(d->playerObj, SL_IID_VOLUME, &d->volumeIf);
    (*d->playerObj)->GetInterface(d->playerObj, SL_IID_SEEK,   &d->seekIf);

    if (loop && d->seekIf)
        (*d->seekIf)->SetLoop(d->seekIf, SL_BOOLEAN_TRUE, 0, SL_TIME_UNKNOWN);

    this->userData = d;
    fileName = util::getBaseName(filenameSound);
    return true;
}

bool AUDIO::play(const bool loop)
{
    auto* d = static_cast<OSLPlayer*>(this->userData);
    if (!d || !d->playIf) return false;
    {
        std::lock_guard<std::mutex> lk(g_engineMutex);
        if (d->seekIf)
            (*d->seekIf)->SetLoop(d->seekIf,
                                  loop ? SL_BOOLEAN_TRUE : SL_BOOLEAN_FALSE,
                                  0, SL_TIME_UNKNOWN);
        (*d->playIf)->SetPlayState(d->playIf, SL_PLAYSTATE_PLAYING);
        d->paused = false;
    }
    state = AUDIO_PLAYING;
    return true;
}

bool AUDIO::pause()
{
    if (!isPlaying()) return false;
    auto* d = static_cast<OSLPlayer*>(this->userData);
    if (!d || !d->playIf) return false;
    {
        std::lock_guard<std::mutex> lk(g_engineMutex);
        (*d->playIf)->SetPlayState(d->playIf, SL_PLAYSTATE_PAUSED);
        d->paused = true;
    }
    state = AUDIO_PLAYING; // keep AUDIO_PLAYING so resume() works (mirrors JNI backend behaviour)
    return true;
}

bool AUDIO::resume()
{
    if (state != AUDIO_PLAYING) return false;
    auto* d = static_cast<OSLPlayer*>(this->userData);
    if (!d || !d->playIf || !d->paused) return false;
    {
        std::lock_guard<std::mutex> lk(g_engineMutex);
        (*d->playIf)->SetPlayState(d->playIf, SL_PLAYSTATE_PLAYING);
        d->paused = false;
    }
    return true;
}

bool AUDIO::stop()
{
    auto* d = static_cast<OSLPlayer*>(this->userData);
    if (!d || !d->playIf) return false;
    {
        std::lock_guard<std::mutex> lk(g_engineMutex);
        (*d->playIf)->SetPlayState(d->playIf, SL_PLAYSTATE_STOPPED);
        d->paused = false;
    }
    state = AUDIO_STOPPED;
    return true;
}

bool AUDIO::reset()
{
    auto* d = static_cast<OSLPlayer*>(this->userData);
    if (!d || !d->playIf) return false;
    {
        std::lock_guard<std::mutex> lk(g_engineMutex);
        (*d->playIf)->SetPlayState(d->playIf, SL_PLAYSTATE_STOPPED);
        if (d->seekIf)
            (*d->seekIf)->SetPosition(d->seekIf, 0, SL_SEEKMODE_FAST);
        d->paused = false;
    }
    state = AUDIO_STOPPED;
    return true;
}

bool AUDIO::isPlaying()
{
    if (state == AUDIO_STOPPED) return false;
    auto* d = static_cast<OSLPlayer*>(this->userData);
    if (!d || !d->playIf) return false;
    SLuint32 ps = SL_PLAYSTATE_STOPPED;
    {
        std::lock_guard<std::mutex> lk(g_engineMutex);
        (*d->playIf)->GetPlayState(d->playIf, &ps);
    }
    if (ps == SL_PLAYSTATE_PLAYING) {
        state = AUDIO_PLAYING;
        return true;
    }
    state = AUDIO_STOPPED;
    return false;
}

bool AUDIO::isPaused()
{
    const auto* d = static_cast<const OSLPlayer*>(this->userData);
    return d && d->paused;
}

bool AUDIO::setVolume(const float volume)
{
    auto* d = static_cast<OSLPlayer*>(this->userData);
    if (!d || !d->volumeIf) return false;
    std::lock_guard<std::mutex> lk(g_engineMutex);
    d->volumeF = volume;
    (*d->volumeIf)->SetVolumeLevel(d->volumeIf, float_to_mb(volume));
    return true;
}

float AUDIO::getVolume()
{
    const auto* d = static_cast<const OSLPlayer*>(this->userData);
    return d ? d->volumeF : 1.0f;
}

bool AUDIO::setPan(const float pan)
{
    auto* d = static_cast<OSLPlayer*>(this->userData);
    if (!d) return false;
    d->panF = pan;
    // Basic OpenSL ES without SL_IID_STEREOPOSITION is not universally available on all
    // Android versions.  Store the value so getPan() is consistent; actual panning is
    // a best-effort no-op here to avoid init-time failures on older devices.
    return true;
}

float AUDIO::getPan()
{
    const auto* d = static_cast<const OSLPlayer*>(this->userData);
    return d ? d->panF : 0.0f;
}

bool AUDIO::setPitch(const float /*pitch*/)
{
    // OpenSL ES SL_IID_PLAYBACKRATE is optional and not widely supported.
    // Return true to avoid breaking callers; pitch is effectively 1.0.
    return true;
}

float AUDIO::getPitch() { return 1.0f; }

int AUDIO::getLength()
{
    auto* d = static_cast<OSLPlayer*>(this->userData);
    if (!d || !d->playIf) return 0;
    SLmillisecond dur = 0;
    std::lock_guard<std::mutex> lk(g_engineMutex);
    (*d->playIf)->GetDuration(d->playIf, &dur);
    return static_cast<int>(dur);
}

bool AUDIO::setPosition(const int pos)
{
    auto* d = static_cast<OSLPlayer*>(this->userData);
    if (!d || !d->seekIf) return false;
    std::lock_guard<std::mutex> lk(g_engineMutex);
    (*d->seekIf)->SetPosition(d->seekIf, static_cast<SLmillisecond>(pos), SL_SEEKMODE_FAST);
    return true;
}

bool AUDIO::isLoaded() { return !fileName.empty(); }

void AUDIO::setOnEndstream(AUDIO::OnEndStreamCallBack cb) { onEndStreamCallBack = cb; }

const char* AUDIO::getFileName() const noexcept { return this->fileName.c_str(); }

const char* AUDIO_ENGINE_version() { return "Android OpenSL ES"; }

} // namespace mbm

#endif // USE_OPENGL_ES
#endif // ANDROID
#endif // AUDIO_ENGINE_ANDROID_OPENSL
