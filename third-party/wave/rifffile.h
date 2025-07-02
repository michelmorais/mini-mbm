/* rifffile.h - Copyright (c) 1996, 1998 by Timothy J. Weber */

#ifndef __RIFFFILE_H
#define __RIFFFILE_H

/* Headers required to use this module */
#include <stack>
#include <string>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/***************************************************************************
	macros, constants, and enums
***************************************************************************/

/***************************************************************************
	typedefs, structs, classes
***************************************************************************/

class RiffFile;
class RiffChunk {
public:
	char name[5];
	uint32_t size;  // the length, read from the second chunk header entry
	char subType[5];  // valid for RIFF and LIST chunks
	int32_t start;  // the file offset in bytes of the chunk contents
	int32_t after;  // the start of what comes after this chunk

	// initialize at the file's current read position, and mark the file as bad
	// if there's an error.
	RiffChunk()
		{
			memset(name,0,sizeof(name));
			memset(subType,0,sizeof(subType));
			size = 0;
			start = 0;
			after = 0;
		};
	RiffChunk(RiffFile& file);

	bool operator < (const RiffChunk& other) const
		{ return start < other.start; };
	bool operator == (const RiffChunk& other) const
	{ return strcmp(name, other.name) == 0
		&& size == other.size
		&& strcmp(subType, other.subType) == 0
		&& start == other.start; };
};

class RiffFile {
	FILE* fp;

	uint32_t formSize;

	std::stack<RiffChunk, std::vector<RiffChunk> > chunks;

public:
	RiffFile(const char *name);
	~RiffFile();

	bool rewind();
	bool push(const char* chunkType = 0);
	bool pop();
	uint32_t chunkSize() const;
	const char* chunkName() const;
	const char* subType() const;
	bool getNextExtraItem(std::string& type, std::string& value);
	FILE* filep()
		{ return fp; };

protected:
	bool readExtraItem(std::string& type, std::string& value);
};

/***************************************************************************
	public variables
***************************************************************************/

#ifndef IN_RIFFFILE
#endif

/***************************************************************************
	function prototypes
***************************************************************************/

#endif
/* __RIFFFILE_H */
