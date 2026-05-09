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

#include <pa-ogg.h>
#include <string.h>
#include <stdlib.h>

/*
 * stb_vorbis is compiled as part of the engine's STB_SOURCES glob
 * (third-party/stb/stb_vorbis.c).  Declare only the function we use;
 * the linker resolves it from the compiled object.
 */
extern "C" {
    int stb_vorbis_decode_filename(const char* filename,
                                   int*        channels,
                                   int*        sample_rate,
                                   short**     output);
}

bool PA_OGG::load(const std::string& fileName)
{
    int    channels   = 0;
    int    sampleRate = 0;
    short* decoded    = nullptr;

    const int totalSamples = stb_vorbis_decode_filename(
        fileName.c_str(), &channels, &sampleRate, &decoded);

    if (totalSamples <= 0 || !decoded || channels <= 0 || sampleRate <= 0)
    {
        if (decoded) free(decoded);
        return false;
    }

    // Pack decoded shorts into a char vector
    const uint32_t byteCount = static_cast<uint32_t>(totalSamples) * sizeof(short);
    std::vector<char> sample(byteCount);
    memcpy(sample.data(), decoded, byteCount);
    free(decoded);

    const uint16_t numChannels    = static_cast<uint16_t>(channels);
    const uint16_t bitsPerChannel = 16;
    const uint16_t bytesPerFrame  = numChannels * (bitsPerChannel / 8u);  // blockAlign
    const uint32_t bytesPerSecond = static_cast<uint32_t>(sampleRate) * bytesPerFrame;
    const uint16_t wavFmtPcm      = 1;  // PCM format type

    return openStream(numChannels, wavFmtPcm, bitsPerChannel,
                      static_cast<uint32_t>(sampleRate), bytesPerFrame,
                      byteCount, bytesPerSecond, std::move(sample));
}

bool PA_OGG::play(const bool bLoop)
{
    stop();        // rewind + stop any active stream before starting
    setLoop(bLoop);
    return start();
}
#endif