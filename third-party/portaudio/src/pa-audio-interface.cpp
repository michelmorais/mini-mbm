
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <math.h>
#include <portaudio.h>
#include <pa-audio-interface.h>

/*
@see Pa_OpenStream, Pa_OpenDefaultStream, PaDeviceInfo
 @see paFloat32, paInt16, paInt32, paInt24, paInt8
 @see paUInt8, paCustomFormat, paNonInterleaved
*/

//typedef unsigned long PaSampleFormat;


//#define paFloat32        ((PaSampleFormat) 0x00000001) /**< @see PaSampleFormat */
//#define paInt32          ((PaSampleFormat) 0x00000002) /**< @see PaSampleFormat */
//#define paInt24          ((PaSampleFormat) 0x00000004) /**< Packed 24 bit format. @see PaSampleFormat */
//#define paInt16          ((PaSampleFormat) 0x00000008) /**< @see PaSampleFormat */
//#define paInt8           ((PaSampleFormat) 0x00000010) /**< @see PaSampleFormat */
//#define paUInt8          ((PaSampleFormat) 0x00000020) /**< @see PaSampleFormat */
//#define paCustomFormat   ((PaSampleFormat) 0x00010000) /**< @see PaSampleFormat */
//
//#define paNonInterleaved ((PaSampleFormat) 0x80000000) /**< @see PaSampleFormat */
//
uint32_t TranslateFormatType(const uint16_t sampleFormat)
{
	fprintf(stdout,"TranslateFormatType for %u\n",sampleFormat);
	switch (sampleFormat)
	{
		case 1: return paInt32;
		case 2: return paInt32;
		default: return paFloat32;
	}
}

PA_DATA::PA_DATA(const uint16_t numChannels,const uint16_t sampleFormat,const uint32_t sampleRate,const uint16_t bytesPerSample):
    m_numChannels(numChannels),
    m_sampleFormat(TranslateFormatType(sampleFormat)),
    m_sampleRate(sampleRate),
    m_bytesPerSample(bytesPerSample),
    m_stream(nullptr),
    m_loop(false),
    m_volume(1.0),
    m_outputParameters(new PaStreamParameters())
{

}

PA_DATA::~PA_DATA()
{
    if (m_stream)
        Pa_CloseStream(m_stream);   
    delete m_outputParameters;
}

void PA_DATA::adjustVolume(void* data,const uint32_t size_data,double volume)
{
	/*
	volume in dB 0db = unity gain, no attenuation, full amplitude signal
	           -20db = 10x attenuation, significantly more quiet*/
	float volumeLevelDb = -6.f; //cut amplitude in half; same as 0.5 above
	const float volumeMultiplier = (volume * pow(10, (volumeLevelDb / 20.f)));
	char* buffer = static_cast<char*>(data);
	for(uint32_t i = 0; i < size_data; ++i)
	{
	   buffer[i] *= volumeMultiplier;
	}
}

PA_DATA_MEMORY::PA_DATA_MEMORY(const uint16_t numChannels,const uint16_t sampleFormat,const uint32_t sampleRate,const uint16_t bytesPerSample,std::vector<char> && sample):
    PA_DATA(numChannels,sampleFormat,sampleRate,bytesPerSample),
    m_sample(std::move(sample)),
    m_index(0)
{
}

void PA_DATA_MEMORY::setPosition(double pos)
{
	const double s = static_cast<double>(m_sample.size());
	if(pos <= 0.0 || s == 0.0)
	{
		m_index = 0;
	}
	else
	{
		double p = s / pos;
		m_index = static_cast<uint32_t>(p);
	}
}

double PA_DATA_MEMORY::getPosition()const
{
	if(m_sample.size() > 0)
	{
		if(m_index == 0)
			return 0.0;
		double p =  static_cast<double>(m_index) / static_cast<double>(m_sample.size());
		return p;
	}
	return 0.0;
}


PA_DATA_FILE::PA_DATA_FILE(WaveFile * wave):
    PA_DATA(wave->GetNumChannels(),wave->GetFormatType(),wave->GetSampleRate(),wave->GetBytesPerSample()),wave(wave)
{
}

void PA_DATA_FILE::setPosition(double pos)
{
	if(pos == 0.0)
		this->wave->ResetToStart();
	//if(m_fp)
	//{
	//	const double s = static_cast<double>(m_size_file);
	//	if(pos <= 0.0 || s == 0.0)
	//	{
	//		if(fseek(m_fp, m_begin, SEEK_SET) == 0)
	//			m_position = 0;
	//		else
	//			std::cout << "Error on set position\n";
	//	}
	//	else
	//	{
	//		const double p = s / pos;
	//		long pFp = static_cast<long>(p);
	//		if(fseek(m_fp, m_begin + pFp, SEEK_SET) == 0)
	//			m_position = m_begin + pFp;
	//		else
	//			std::cout << "Error on set position\n";
	//	}
	//}
}

double PA_DATA_FILE::getPosition()const
{
	//if(m_fp && m_size_file > 0)
	//{
	//	if(m_position == 0)
	//		return 0.0;
	//	const double p =  static_cast<double>(m_position) / static_cast<double>(m_size_file);
	//	return p;
	//}
	return 0.0;
}

bool PA_INTERFACE::initialized = false;

PA_INTERFACE::PA_INTERFACE():m_data(nullptr)
{
    if(PA_INTERFACE::initialized == false)
    {
        if(Pa_Initialize() == paNoError)
            PA_INTERFACE::initialized = true;
        else
            std::cout << "Error on initialize PA" << std::endl;
    }
}

PA_INTERFACE::~PA_INTERFACE()
{
	if(m_data)
		delete m_data;
	m_data = nullptr;
}


bool PA_INTERFACE::openStream(const int numChannels,const int sampleFormat,const int sampleRate,const int bytesPerSample,std::vector<char> && sample)
{
    if(initialized)
    {
        std::cout << "stream from memory \n";
        PA_DATA_MEMORY * data = new PA_DATA_MEMORY(numChannels,sampleFormat,sampleRate,bytesPerSample,std::move(sample));
        m_data = data;

        data->m_outputParameters->device = Pa_GetDefaultOutputDevice();
        //m_outputParameters.device != paNoDevice;//check
        
        data->m_outputParameters->channelCount = data->m_numChannels;
        data->m_outputParameters->sampleFormat = data->m_sampleFormat;
		data->m_outputParameters->hostApiSpecificStreamInfo = nullptr;
        data->m_outputParameters->suggestedLatency = Pa_GetDeviceInfo( data->m_outputParameters->device )->defaultHighOutputLatency;
        
        PaError ret = Pa_OpenStream(
            &data->m_stream,
            NULL, // no input
            data->m_outputParameters,
            data->m_sampleRate,
            paFramesPerBufferUnspecified, // framesPerBuffer
            0, // flags
            &paStreamCallback,
            data //void *userData
            );
        return ret == 0;
    }
    return false;
}

bool PA_INTERFACE::openStream(WaveFile * wave)
{
    if(initialized)
    {
        std::cout << "stream from file \n";
        PA_DATA_FILE * data = new PA_DATA_FILE(wave);
        m_data = data;
        data->m_outputParameters->device = Pa_GetDefaultOutputDevice();
        //m_outputParameters.device != paNoDevice;//check
        
        data->m_outputParameters->channelCount = data->m_numChannels;
        data->m_outputParameters->sampleFormat = data->m_sampleFormat;
		data->m_outputParameters->hostApiSpecificStreamInfo = nullptr;
		const PaDeviceInfo* paDeviceInfo = Pa_GetDeviceInfo( data->m_outputParameters->device );
		data->m_outputParameters->suggestedLatency = paDeviceInfo->defaultHighOutputLatency;
        
        PaError ret = Pa_OpenStream(
            &data->m_stream,
            NULL, // no input
            data->m_outputParameters,
            data->m_sampleRate,
            paFramesPerBufferUnspecified, // framesPerBuffer
            0, // flags
            &paStreamCallbackFromFile,
            data //void *userData
            );
        return ret == 0;
    }
    return false;
}


int PA_INTERFACE::paStreamCallback(const void *input, void *output,unsigned long frameCount,const PaStreamCallbackTimeInfo* timeInfo,unsigned long statusFlags,void *userData)
{
    PA_DATA_MEMORY* paInterface = static_cast<PA_DATA_MEMORY*>(userData);
    uint32_t t = paInterface->m_bytesPerSample * paInterface->m_numChannels * frameCount;
    if(t + paInterface->m_index >= static_cast<uint32_t>(paInterface->m_sample.size()))
    {
    	if(paInterface->m_loop)
    		paInterface->m_index = 0;
    	else
        	return paComplete;
    }
    char* s = static_cast<char*>(output);
    memcpy(s,&paInterface->m_sample[paInterface->m_index],t * sizeof(char));
    paInterface->m_index += t;
    if(paInterface->m_volume < 1.0)
	{
		paInterface->adjustVolume(output,t,paInterface->m_volume);
	}
    
    return paContinue;
}

int PA_INTERFACE::paStreamCallbackFromFile(const void *input, void *output,unsigned long frameULongCount,const PaStreamCallbackTimeInfo* timeInfo,unsigned long statusFlags,void *userData)
{
    PA_DATA_FILE* paInterface = static_cast<PA_DATA_FILE*>(userData);
    uint32_t bytes_vs_channel = paInterface->m_bytesPerSample * paInterface->m_numChannels;
	size_t numRead = 0;
	paInterface->wave->ReadRaw(static_cast<char*>(output),bytes_vs_channel * frameULongCount,numRead);
	int64_t frameCount = frameULongCount;
	uint32_t numFramesRead = numRead ? numRead / bytes_vs_channel : 0;
	frameCount -= numFramesRead;
    
    if(frameCount > 0 || numFramesRead == 0) 
    {
        if(paInterface->m_loop)
        {
			paInterface->wave->ResetToStart();
			paInterface->wave->ReadRaw(static_cast<char*>(output),bytes_vs_channel * frameCount,numRead);
			numFramesRead = numRead ? numRead / bytes_vs_channel : 0;
        	//numRead = fread(output, bytes_vs_channel, frameCount, paInterface->m_fp);
        	//if(fseek(paInterface->m_fp, paInterface->m_begin, SEEK_SET) == 0)
			if(frameCount > 0 || numFramesRead == 0) 
			{
				memset(output, 0, frameCount * paInterface->m_numChannels * paInterface->m_bytesPerSample);
			}
			//paInterface->m_position = numRead * bytes_vs_channel;
			if(paInterface->m_volume < 1.0)
			{
				paInterface->adjustVolume(output,numFramesRead * bytes_vs_channel,paInterface->m_volume);
			}
			return paContinue;
        }
        else
        {
        	memset(output, 0, frameCount * paInterface->m_numChannels * paInterface->m_bytesPerSample);
        }
        //paInterface->m_position = paInterface->m_size_file;
        return paComplete;
    }
    //paInterface->m_position += numRead * bytes_vs_channel;
    if(paInterface->m_volume < 1.0)
	{
		paInterface->adjustVolume(output,numFramesRead * bytes_vs_channel,paInterface->m_volume);
	}
    return paContinue;
}

void PA_INTERFACE::setLoop(bool bLoop)
{
	if(m_data)
		m_data->m_loop = bLoop;
}

bool PA_INTERFACE::start()
{
	if(m_data)
	{
	    auto err = Pa_StartStream(m_data->m_stream);
	    if(err == paNoError)
	    	return true;
	    else
	        std::cout << "error on play \n";
	}
	return false;
}

bool PA_INTERFACE::stop()
{
	if(m_data && Pa_IsStreamActive(m_data->m_stream) > 0)
	{
		auto err = Pa_StopStream( m_data->m_stream );
		if(err != paNoError)
			return false;
		m_data->setPosition(0.0);
	}
	return true;
}

bool PA_INTERFACE::pause()
{
	if(m_data && Pa_IsStreamActive(m_data->m_stream) > 0)
	{
		auto err = Pa_StopStream( m_data->m_stream );
		if(err != paNoError)
			return false;
	}
	return true;
}

void PA_INTERFACE::setPosition(double pos)
{
	if(m_data)
		m_data->setPosition(pos);
}

bool PA_INTERFACE::isPlaying()const
{
	if(m_data && Pa_IsStreamActive(m_data->m_stream) > 0)
        return true;
    return false;
}

double PA_INTERFACE::getPosition()const
{
	if(m_data)
		return m_data->getPosition();
	return 0.0;
}

void PA_INTERFACE::setVolume(double volume)
{
	if(m_data)
	{
		m_data->m_volume = volume;
		if(m_data->m_volume >= 1.0)
			m_data->m_volume = 1.0;
		if(m_data->m_volume < 0.0)
			m_data->m_volume = 0.0;
	}
}

double PA_INTERFACE::getVolume()
{
	if(m_data)
		return m_data->m_volume;
	return 0.0;
}