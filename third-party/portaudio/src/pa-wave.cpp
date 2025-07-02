
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
    const unsigned long bufferSize = this->GetDataLength();
    size_t iTotalRead = 0;
    if(in_memory)
    {
        std::vector<char> sample;
        sample.reserve(bufferSize);
        sample.resize(bufferSize);
        if(this->ReadRaw(sample.data(),bufferSize,iTotalRead))
        {
            ret = openStream(this->GetNumChannels(),
                            TranslateFormatType(this->GetFormatType()),
                            this->GetSampleRate(),
                            this->GetBytesPerSample(),
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
    if (start())
    {
    	setLoop(bLoop);
        return true;
    }
    return false;
}

const char* PA_version()
{
    return Pa_GetVersionText();
}