/* wave.h - Copyright (c) 1996-2002 by Timothy J. Weber */

#ifndef __WAVE_H
#define __WAVE_H

/* Headers required to use this module */
#include <stdio.h>
#include <stdint.h>
#include "rifffile.h"

/***************************************************************************
	macros, constants, and enums
***************************************************************************/

/***************************************************************************
	typedefs, structs, classes
***************************************************************************/

class WaveFile
{
public:
	WaveFile();
	~WaveFile();

	bool OpenRead(const char* name);
	bool OpenWrite(const char* name);
	bool ResetToStart();
	bool Close();

	uint16_t GetFormatType() const
		{ return formatType; };
	void SetFormatType(uint16_t type)
		{ formatType = type; changed = true; };
	bool IsCompressed() const
		{ return formatType != 1; };

	uint16_t GetNumChannels() const
		{ return numChannels; };
	void SetNumChannels(uint16_t num)
		{ numChannels = num; changed = true; };

	uint32_t GetSampleRate() const
		{ return sampleRate; };
	void SetSampleRate(uint32_t rate)
		{ sampleRate = rate; changed = true; };

	uint32_t GetBytesPerSecond() const
		{ return bytesPerSecond; };
	void SetBytesPerSecond(uint32_t bytes)
		{ bytesPerSecond = bytes; changed = true; };

	uint16_t GetBytesPerSample() const
		{ return bytesPerSample; };
	void SetBytesPerSample(uint16_t bytes)
		{ bytesPerSample = bytes; changed = true; };

	uint16_t GetBitsPerChannel() const
		{ return bitsPerChannel; };
	void SetBitsPerChannel(uint16_t bits)
		{ bitsPerChannel = bits; changed = true; };

	uint32_t GetNumSamples() const
		{ return (GetBytesPerSample())?
			GetDataLength() / GetBytesPerSample(): 0; };
	void SetNumSamples(uint32_t num)
		{ SetDataLength(num * GetBytesPerSample()); };

	float GetNumSeconds() const
		{ return GetBytesPerSecond()?
			float(GetDataLength()) / GetBytesPerSecond(): 0; };

	uint32_t GetDataLength() const
		{ return dataLength; };
	void SetDataLength(uint32_t numBytes)
		{ dataLength = numBytes; changed = true; };

	bool FormatMatches(const WaveFile& other);

	void CopyFormatFrom(const WaveFile& other);

	void SetupFormat(int sampleRate = 44100, short bitsPerChannel = 16, short channels = 1);

	FILE* GetFile()
		{ return readFile? readFile->filep(): writeFile; };

	RiffFile* GetRiffFile()
		{ return readFile? readFile : 0; };

	bool WriteHeaderToFile(FILE* fp);

	bool ReadSample(unsigned char& sample);
	bool WriteSample(unsigned char sample);
	bool ReadSample(short& sample);
	bool WriteSample(short sample);
	bool ReadSample(float& sample);
	bool WriteSample(float sample);
	bool ReadSample(double& sample);
	bool WriteSample(double sample);

	bool ReadSamples(unsigned char* samples, size_t count = 1);
	bool WriteSamples(unsigned char* samples, size_t count = 1);
	bool ReadSamples(short* samples, size_t count = 1);
	bool WriteSamples(short* samples, size_t count = 1);

	bool ReadRaw(char* buffer, size_t numBytes = 1);
	bool ReadRaw(char* buffer, size_t numBytes,size_t & iTotal_read);
	bool WriteRaw(char* buffer, size_t numBytes = 1);

	bool GetFirstExtraItem(std::string& type, std::string& value);
	bool GetNextExtraItem(std::string& type, std::string& value);

	bool CopyFrom(WaveFile& other);

	const char* GetError() const
		{ return error; };
	void ClearError()
		{ error = 0; };

protected:
	RiffFile* readFile;
	FILE* writeFile;

	uint16_t formatType;
	uint16_t numChannels;
	uint32_t sampleRate;
	uint32_t bytesPerSecond;
	uint16_t bytesPerSample;
	uint16_t bitsPerChannel;
	uint32_t dataLength;

	const char* error;
	bool changed;  // true if any parameters changed since the header was last written
};

/***************************************************************************
	public variables
***************************************************************************/

#ifndef IN_WAVE
#endif

/***************************************************************************
	function prototypes
***************************************************************************/

#endif
/* __WAVE_H */
