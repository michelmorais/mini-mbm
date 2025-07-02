#ifndef _PA_WAVE_H_
#define _PA_WAVE_H_

#include <WAVE.h>
#include <pa-audio-interface.h>

uint32_t TranslateFormatType(const uint16_t sampleFormat);

class PA_WAVE : public WaveFile, public PA_INTERFACE
{
public:
	PA_WAVE();
	virtual ~PA_WAVE();
	bool load(const std::string & fileName,const bool in_memory = false );
	bool play(bool bLoop);
};

const char* PA_version();

#endif