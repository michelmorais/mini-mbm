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

#ifndef MINIZ_H_WRAP_H
#define MINIZ_H_WRAP_H


#include <miniz/miniz.h>
#include <core_mbm/core-exports.h>

namespace mbm
{

class API_IMPL MINIZ
{
public:
    MINIZ()noexcept;
    
    virtual ~MINIZ();
    
    void release();
    static void showVersion();
    
    bool compressFile(const char *pSrcFileName,const char *pDstFileName,const int level = MZ_UBER_COMPRESSION,char* stringError = nullptr,const int lenError=0);
    bool compressStream(uint8_t *dataIn,const uint32_t sizeOfDataIn,const int level = MZ_UBER_COMPRESSION,char* stringError = nullptr,const int lenError=0);
    bool decompressFile(const char *pSrcFileName,const char *pDstFileName,char* stringError = nullptr,const int lenError=0);
    bool decompressStream(uint8_t *dataIn,const uint32_t sizeofDataIn,const uint32_t sizeDataOut);
    static int InflateInit(mz_streamp pStream);
    static int Inflate(mz_streamp pStream, int flush);
    static int InflateEnd(mz_streamp pStream);
    uint8_t * getDataStreamOut()const;
    uint32_t getSizeDataStreamOut()const;
private:
    uint8_t*  dataStreamOut;
    uint32_t    sizeDataStreamOut;
    
};

}

#endif
