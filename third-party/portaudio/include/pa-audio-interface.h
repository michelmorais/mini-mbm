#ifndef _PA_INTERFACE_H_
#define _PA_INTERFACE_H_


#include <vector>
#include <stdio.h>
#include <WAVE.h>
#include <stdint.h>

struct PaStreamParameters;
struct PaStreamCallbackTimeInfo;

class PA_DATA
{
public:
	uint16_t 			m_numChannels;
	uint32_t 			m_sampleFormat;
	uint32_t 			m_sampleRate;
	uint16_t 			m_bytesPerSample;
	void* 				m_stream;
	bool				m_loop;
	double				m_volume;
	PaStreamParameters* m_outputParameters;
	PA_DATA(const uint16_t numChannels,const uint16_t sampleFormat,const uint32_t sampleRate,const uint16_t bytesPerSample);
	virtual ~PA_DATA();
	virtual void	setPosition(double pos) = 0;
	virtual double 	getPosition()const = 0;
	void adjustVolume(void* data,const uint32_t size_data, double volume);
};


class PA_DATA_MEMORY : public PA_DATA
{
public:
	std::vector<char>	m_sample;
	uint32_t			m_index;
	PA_DATA_MEMORY(const uint16_t numChannels,const uint16_t sampleFormat,const uint32_t sampleRate,const uint16_t bytesPerSample,std::vector<char>	&& sample);
	virtual ~PA_DATA_MEMORY() = default;
	virtual void	setPosition(double pos) override;
	virtual double 	getPosition()const override;
};

class PA_DATA_FILE : public PA_DATA
{
public:
	WaveFile * wave;
	PA_DATA_FILE(WaveFile * wave);
	virtual ~PA_DATA_FILE() = default;
	virtual void	setPosition(double pos) override;
	virtual double 	getPosition()const override;
};

class PA_INTERFACE
{
public:
	PA_INTERFACE();
	virtual ~PA_INTERFACE();
	bool start();
	bool stop();
	bool pause();
	void setPosition(double pos);
	double getPosition()const;
	void setLoop(bool bLoop);
	bool isPlaying() const;
	void setVolume(double volume);
	double getVolume();
protected:
	bool openStream(const int numChannels,const int sampleFormat,const int sampleRate,const int bytesPerSample,std::vector<char> && sample);
	bool openStream(WaveFile * wave);
	static bool initialized;
	PA_DATA* m_data;
private:
	static int paStreamCallback(const void *input, void *output,unsigned long frameCount,const PaStreamCallbackTimeInfo* timeInfo,unsigned long statusFlags,void *userData);
	static int paStreamCallbackFromFile(const void *input, void *output,unsigned long frameCount,const PaStreamCallbackTimeInfo* timeInfo,unsigned long statusFlags,void *userData);
};

#endif