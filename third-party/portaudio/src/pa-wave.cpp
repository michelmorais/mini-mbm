
#if defined(AUDIO_ENGINE_PORT_AUDIO)

#include <pa-wave.h>
#include <portaudio.h>

PA_WAVE::PA_WAVE()
{
}

PA_WAVE::~PA_WAVE()
{

}

bool PA_WAVE::load(const std::string & fileName,const bool in_memory)
{
    if(this->OpenRead(fileName.c_str()) == false)
    {
        return false;
    }
    bool ret = false;
    const unsigned long dataLength = this->GetDataLength();
    size_t iTotalRead = 0;
    if(in_memory)
    {
        std::vector<char> sample;
        sample.reserve(dataLength);
        sample.resize(dataLength);
        if(this->ReadRaw(sample.data(),dataLength,iTotalRead))
        {
            ret = openStream(this->GetNumChannels(),
                             this->GetFormatType(),
                             this->GetBitsPerChannel(),
                             this->GetSampleRate(),
                             this->GetBytesPerSample(),
                             this->GetDataLength(),
                             this->GetBytesPerSecond(),
                             std::move(sample));
        }
    }
    else
    {
    	ret = openStream(this);
    }
    return ret;
}
bool PA_WAVE::play(bool bLoop)
{
    stop();        // rewind + stop any active stream before starting
    setLoop(bLoop);
    return start();
}

const char* PA_version()
{
    return Pa_GetVersionText();
}

#endif