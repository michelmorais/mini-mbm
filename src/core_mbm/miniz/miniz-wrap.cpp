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

#include <platform/mismatch-platform.h>
#include <miniz-wrap/miniz-wrap.h>
#include <util-interface.h>
#include <climits>

#include <cmath>
#include <cmath>
#include <algorithm> // std::min

constexpr uint32_t BUF_SIZE = (1024 * 1024);

namespace mbm
{

    MINIZ::MINIZ() noexcept
    {
        dataStreamOut       =   nullptr;
        sizeDataStreamOut   =   0;
    }
    
    MINIZ::~MINIZ()
    {
        this->release();
    }

    void MINIZ::showVersion()
    {
        static bool version = true;
        if(version)
        {
            INFO_LOG("\nminiz.c version: %s\n", MZ_VERSION);
            version = false;
        }
    }
    
    void MINIZ::release()
    {
        if(dataStreamOut)
            delete [] dataStreamOut;
        dataStreamOut       =   nullptr;
        sizeDataStreamOut   =   0;
    }
    
    bool MINIZ::compressFile(const char *pSrcFileName,const char *pDstFileName,const int level ,char* stringError,const int lenError )
    {
        FILE *          pInfile =   nullptr;
        FILE*           pOutfile=   nullptr;
        uint32_t    infile_size;
        z_stream        stream;
        long            file_loc;
        uint8_t*  s_inbuf =   nullptr;
        uint8_t*  s_outbuf=   nullptr;
        // Open input file.
        if(pSrcFileName == nullptr)
        {
            PRINT_IF_DEBUG("\nFilename is NULL!");
            if(stringError)
                strncpy(stringError,"Filename in is NULL",lenError);
            return false;
        }
        if(pDstFileName == nullptr)
        {
            PRINT_IF_DEBUG("\nfile name out is NULL");
            if(stringError)
                strncpy(stringError,"filename out is NULL!",lenError);
            return false;
        }
        if(strcmp(pSrcFileName,pDstFileName) == 0)
        {
            PRINT_IF_DEBUG("\nfilename in is the same as out!");
            if(stringError)
                strncpy(stringError,"filename in is the same as out",lenError);
            return false;
        }
        if(level < 0 || level > MZ_UBER_COMPRESSION)
        {
            PRINT_IF_DEBUG("allow leve 0 and 10.\n0:no compression10:UBER compression! but not compitible zlib.\n9:best compression and compatible zlib.");
            if(stringError)
                strncpy(stringError,"allow leve 0 and 10.\n0:no compression10:UBER compression! but not compitible zlib.\n9:best compression and compatible zlib.",lenError);
            return false;
        }
        
        pInfile = util::openFile(pSrcFileName, "rb");
        if (!pInfile)
        {
            PRINT_IF_DEBUG("failed to open in file!");
            if(stringError)
                strncpy(stringError,"failed to open in file!",lenError);
            return false;
        }
        // Determine input file's size.
        fseek(pInfile, 0, SEEK_END);
        file_loc        = ftell(pInfile);
        fseek(pInfile, 0, SEEK_SET);
        if ((file_loc < 0) || (file_loc > INT_MAX))
        {
            // This is not a limitation of miniz or tinfl, but this.
            PRINT_IF_DEBUG("\nfile is too large to be processed");
            if(stringError)
                strncpy(stringError,"file is too large to be processed",lenError);
            return false;
        }
        s_inbuf =   new uint8_t [BUF_SIZE];
        s_outbuf=   new uint8_t [BUF_SIZE];
#if defined _WIN32
        RtlSecureZeroMemory(s_inbuf,BUF_SIZE);
        RtlSecureZeroMemory(s_outbuf,BUF_SIZE);
#else
        memset(s_inbuf,0,BUF_SIZE);
        memset(s_outbuf,0,BUF_SIZE);
#endif
        infile_size = static_cast<uint32_t>(file_loc);
        // Open output file.
        pOutfile = util::openFile(pDstFileName, "wb");
        if (!pOutfile)
        {
            PRINT_IF_DEBUG("failed to open out file!\n");
            if(stringError)
                strncpy(stringError,"failed to open out file!",lenError);
            delete [] s_inbuf;
            delete [] s_outbuf;
            return false;
        }
        // Init the z_stream
        memset(&stream, 0, sizeof(stream));
        stream.next_in      = s_inbuf;
        stream.avail_in     = 0;
        stream.next_out     = s_outbuf;
        stream.avail_out    = BUF_SIZE;
        // Compression.
        uint32_t infile_remaining = infile_size;
        if (deflateInit(&stream, level) != Z_OK)
        {
            PRINT_IF_DEBUG("deflateInit() failed!\n");
            if(stringError)
                strncpy(stringError,"deflateInit() failed!",lenError);
            delete [] s_inbuf;
            delete [] s_outbuf;
            return false;
        }
        for ( ; ; )
        {
            int status;
            if (!stream.avail_in)
            {
                // Input buffer is empty, so read more bytes from input file.
                uint32_t n = std::min(BUF_SIZE, infile_remaining);
                if (fread(s_inbuf, 1, n, pInfile) != n)
                {
                    PRINT_IF_DEBUG("failed to read in file!\n");
                    if(stringError)
                        strncpy(stringError,"failed to read in file!",lenError);
                    delete [] s_inbuf;
                    delete [] s_outbuf;
                    return false;
                }
                stream.next_in = s_inbuf;
                stream.avail_in = n;
                infile_remaining -= n;
            }
            status = deflate(&stream, infile_remaining ? Z_NO_FLUSH : Z_FINISH);
            if ((status == Z_STREAM_END) || (!stream.avail_out))
            {
                // Output buffer is full, or compression is done, so write buffer to output file.
                uint32_t n = BUF_SIZE - stream.avail_out;
                if (fwrite(s_outbuf, 1, n, pOutfile) != n)
                {
                    PRINT_IF_DEBUG("failed to write on out file!\n");
                    if(stringError)
                        strncpy(stringError,"failed to write on out file!",lenError);
                    delete [] s_inbuf;
                    delete [] s_outbuf;
                    return false;
                }
                stream.next_out = s_outbuf;
                stream.avail_out = BUF_SIZE;
            }
            if (status == Z_STREAM_END)
                break;
            else if (status != Z_OK)
            {
                PRINT_IF_DEBUG("deflate() failed status %i!\n", status);
                if(stringError)
                    sprintf(stringError,"deflate() failed status %i!\n", status);
                delete [] s_inbuf;
                delete [] s_outbuf;
                return false;
            }
        }
        if (deflateEnd(&stream) != Z_OK)
        {
            PRINT_IF_DEBUG("deflateEnd() failed!\n");
            if(stringError)
                strncpy(stringError,"deflateEnd() failed!",lenError);
            delete [] s_inbuf;
            delete [] s_outbuf;
            return false;
        }
        fclose(pInfile);
        if (EOF == fclose(pOutfile))
        {
            PRINT_IF_DEBUG("failed to write to out file!\n");
            if(stringError)
                strncpy(stringError,"failed to write to out file!",lenError);
            delete [] s_inbuf;
            delete [] s_outbuf;
            return false;
        }
        delete [] s_inbuf;
        delete [] s_outbuf;
        return true;
    }
    
    bool MINIZ::compressStream(uint8_t *dataIn,const uint32_t sizeOfDataIn,const int level ,char* stringError,const int lenError )
    {
        z_stream        stream;
        uint8_t*  s_outbuf    =   nullptr;
        uint32_t    nextBuff    =   0;
        // Open input file.
        if(dataIn == nullptr)
        {
            PRINT_IF_DEBUG("\ndata in is NULL !");
            if(stringError)
                strncpy(stringError,"data in is NULL!",lenError);
            return false;
        }
        if(sizeOfDataIn == 0)
        {
            PRINT_IF_DEBUG("\nsize data in is ZERO!");
            if(stringError)
                strncpy(stringError,"size data in is ZERO!",lenError);
            return false;
        }
        if(level < 0 || level > 10)
        {
            PRINT_IF_DEBUG("\nallow leve 0 and 10.\n0:no compression10:UBER compression! but not compitible zlib.\n9:best compression and compatible zlib.");
            if(stringError)
                strncpy(stringError,"allow leve 0 and 10.\n0:no compression10:UBER compression! but not compitible zlib.\n9:best compression and compatible zlib.",lenError);
            return false;
        }
        if (sizeOfDataIn > INT_MAX)
        {
            // This is not a limitation of miniz or tinfl, but this.
            PRINT_IF_DEBUG("\ndata stream is to long");
            if(stringError)
                strncpy(stringError,"data stream is to long",lenError);
            return false;
        }
        s_outbuf=   new uint8_t [sizeOfDataIn];
        memset(s_outbuf,0,sizeOfDataIn);
        // Init the z_stream
        memset(&stream, 0, sizeof(stream));
        stream.next_in      = dataIn;
        stream.avail_in     = 0;
        stream.next_out     = s_outbuf;
        stream.avail_out    = sizeOfDataIn;
        // Compression.
        uint32_t infile_remaining = sizeOfDataIn;
        if (deflateInit(&stream, level) != Z_OK)
        {
            PRINT_IF_DEBUG("deflateInit() failed!\n");
            if(stringError)
                strncpy(stringError,"deflateInit() failed!\n",lenError);
            delete [] s_outbuf;
            return false;
        }
        for ( ; ; )
        {
            int status;
            if (!stream.avail_in)
            {
                // Input buffer is empty, so read more bytes from input file.
                uint32_t n = std::min(sizeOfDataIn, infile_remaining);
                if (n > sizeOfDataIn)
                {
	                PRINT_IF_DEBUG("\nfailed to read from stream!")
                    ;
                    if(stringError)
                        strncpy(stringError,"failed to read from stream!",lenError);
                    delete [] s_outbuf;
                    return false;
                }
                stream.next_in      = &dataIn[nextBuff];
                stream.avail_in     = n;
                infile_remaining    -= n;
                nextBuff            +=  n;
            }
            status = deflate(&stream, infile_remaining ? Z_NO_FLUSH : Z_FINISH);
            if ((status == Z_STREAM_END) || (!stream.avail_out))
            {
                // Output buffer is full, or decompression is done, so write buffer to output file.
                uint32_t n = sizeOfDataIn - stream.avail_out;
                if(!dataStreamOut)
                {
                    dataStreamOut = new uint8_t[n];
                    memcpy(dataStreamOut,s_outbuf,n);
                    sizeDataStreamOut   =   n;
                }
                else
                {
                    const uint32_t sizeCur = sizeDataStreamOut;
                    sizeDataStreamOut   +=  n;
                    auto newDataStreamOut = new uint8_t[sizeDataStreamOut];
                    memcpy(newDataStreamOut,dataStreamOut,sizeCur);
                    memcpy(&newDataStreamOut[sizeCur],s_outbuf,n);
                    delete [] dataStreamOut;
                    dataStreamOut   =   newDataStreamOut;
                }
                stream.next_out = s_outbuf;
                stream.avail_out = sizeOfDataIn;
            }
            if (status == Z_STREAM_END)
                break;
            else if (status != Z_OK)
            {
                PRINT_IF_DEBUG("deflate() failed status %i!\n", status);
                if(stringError)
                    sprintf(stringError,"deflate() failed status %i!\n", status);
                delete [] s_outbuf;
                return false;
            }
        }
        if (deflateEnd(&stream) != Z_OK)
        {
            PRINT_IF_DEBUG("deflateEnd() failed!\n");
            if(stringError)
                strncpy(stringError,"deflateEnd() failed!\n",lenError);
            delete [] s_outbuf;
            return false;
        }
        delete [] s_outbuf;
        return true;
    }
    
    bool MINIZ::decompressFile(const char *pSrcFileName,const char *pDstFileName,char* stringError,const int lenError )
    {
        FILE *          pInfile     =   nullptr;
        FILE*           pOutfile    =   nullptr;
        uint32_t    infile_size =   0;
        long            file_loc    =   0;
        uint8_t*  s_inbuf     =   nullptr;
        uint8_t*  s_outbuf    =   nullptr;
        z_stream        stream;
        if(pSrcFileName == nullptr)
        {
            PRINT_IF_DEBUG("\nfile name in is NULL");
            if(stringError)
                strncpy(stringError,"file name in is NULL",lenError);
            return false;
        }
        if(pDstFileName == nullptr)
        {
            PRINT_IF_DEBUG("\nfile name out is NULL");
            if(stringError)
                strncpy(stringError,"file name out is NULL",lenError);
            return false;
        }
        if(strcmp(pSrcFileName,pDstFileName) == 0)
        {
            PRINT_IF_DEBUG("\nfile in is the same file out");
            if(stringError)
                strncpy(stringError,"file in is the same file out",lenError);
            return false;
        }
        // Open input file.
        pInfile = util::openFile(pSrcFileName, "rb");
        if (!pInfile)
        {
            PRINT_IF_DEBUG("\nfailed to open file!");
            if(stringError)
                strncpy(stringError,"failed to open in file",lenError);
            return false;
        }
        // Determine input file's size.
        fseek(pInfile, 0, SEEK_END);
        file_loc = ftell(pInfile);
        fseek(pInfile, 0, SEEK_SET);

        if ((file_loc < 0) || (file_loc > INT_MAX))
        {
            // This is not a limitation of miniz or tinfl, but this.
            PRINT_IF_DEBUG("\nfile is to long!");
            if(stringError)
                strncpy(stringError,"file is to long!",lenError);
            fclose(pInfile);
            return false;
        }
        infile_size = static_cast<uint32_t>(file_loc);
        // Open output file.
        pOutfile = util::openFile(pDstFileName, "wb");
        if (!pOutfile)
        {
            PRINT_IF_DEBUG("\nfailed to open file!");
            if(stringError)
                strncpy(stringError,"failed to open out file",lenError);
            fclose(pInfile);
            return false;
        }
        s_inbuf =   new uint8_t [BUF_SIZE];
        s_outbuf=   new uint8_t [BUF_SIZE];
        // Init the z_stream
#if defined _WIN32
        RtlSecureZeroMemory(&stream,sizeof(stream));
#else
        memset(&stream, 0, sizeof(stream));
#endif
        stream.next_in = s_inbuf;
        stream.avail_in = 0;
        stream.next_out = s_outbuf;
        stream.avail_out = BUF_SIZE;
        // Decompression.
        uint32_t infile_remaining = infile_size;
        if (inflateInit(&stream))
        {
            PRINT_IF_DEBUG("\ninflateInit() failed!");
            if(stringError)
                strncpy(stringError,"inflateInit() failed!",lenError);
            fclose(pInfile);
            fclose(pOutfile);
            return false;
        }
        for ( ; ; )
        {
            int status;
            if (!stream.avail_in)
            {
                // Input buffer is empty, so read more bytes from input file.
                uint32_t n = std::min(BUF_SIZE, infile_remaining);
                if (fread(s_inbuf, 1, n, pInfile) != n)
                {
                    PRINT_IF_DEBUG("\nfailed to read from file!");
                    if(stringError)
                        strncpy(stringError,"failed to open out file",lenError);
                    delete [] s_inbuf;
                    delete [] s_outbuf;
                    fclose(pInfile);
                    fclose(pOutfile);
                    return false;
                }
                stream.next_in = s_inbuf;
                stream.avail_in = n;
                infile_remaining -= n;
            }
            status = inflate(&stream, Z_SYNC_FLUSH);
            if ((status == Z_STREAM_END) || (!stream.avail_out))
            {
                // Output buffer is full, or decompression is done, so write buffer to output file.
                uint32_t n = BUF_SIZE - stream.avail_out;
                if (fwrite(s_outbuf, 1, n, pOutfile) != n)
                {
                    PRINT_IF_DEBUG("\nfailed to write file out");
                    if(stringError)
                        strncpy(stringError,"failed to write file out",lenError);
                    delete [] s_inbuf;
                    delete [] s_outbuf;
                    fclose(pInfile);
                    fclose(pOutfile);
                    return false;
                }
                stream.next_out = s_outbuf;
                stream.avail_out = BUF_SIZE;
            }
            if (status == Z_STREAM_END)
                break;
            else if (status != Z_OK)
            {
                PRINT_IF_DEBUG("\ninflate() failed status %i!", status);
                if(stringError)
                    sprintf(stringError,"inflate() failed status %i!", status);
                delete [] s_inbuf;
                delete [] s_outbuf;
                fclose(pInfile);
                fclose(pOutfile);
                return false;
            }
        }
        if (inflateEnd(&stream) != Z_OK)
        {
            PRINT_IF_DEBUG("\ninflateEnd() failed!");
            if(stringError)
                strncpy(stringError,"inflateEnd() failed!",lenError);
            delete [] s_inbuf;
            delete [] s_outbuf;
            fclose(pInfile);
            fclose(pOutfile);
            return false;
        }
        fclose(pInfile);
        if (EOF == fclose(pOutfile))
        {
            PRINT_IF_DEBUG("\nfailed to close out file");
            if(stringError)
                strncpy(stringError,"failed to close out file",lenError);
            delete [] s_inbuf;
            delete [] s_outbuf;
            return false;
        }
        if(stringError)
            sprintf(stringError,"in bytes: %u\nout bytes: %u", static_cast<mz_uint32>(stream.total_in),static_cast<mz_uint32>(stream.total_out));
        delete [] s_inbuf;
        delete [] s_outbuf;
        return true;
    }
    
    bool MINIZ::decompressStream(uint8_t *dataIn,const uint32_t sizeofDataIn,const uint32_t sizeDataOut)
    {
        z_stream        stream;
        this->release();
        if(!dataIn)
        {
            PRINT_IF_DEBUG("\ndata is NULL!");
            return false;
        }
        if(sizeofDataIn == 0)
        {
            PRINT_IF_DEBUG("\ndata has ZERO size!");
            return false;
        }
        if ((sizeofDataIn > INT_MAX) || (sizeDataOut < sizeofDataIn ))
        {
            // This is not a limitation of miniz or tinfl, but this.
            PRINT_IF_DEBUG("\nfile is to long!");
            return false;
        }
        dataStreamOut=  new uint8_t [sizeDataOut];
        sizeDataStreamOut = 0;
        // Init the z_stream
        memset(&stream, 0, sizeof(stream));
        stream.next_in = dataIn;
        stream.avail_in = 0;
        stream.next_out = dataStreamOut;
        stream.avail_out = sizeDataOut;
        // Decompression.
        uint32_t infile_remaining = sizeofDataIn;
        if (inflateInit(&stream))
        {
            PRINT_IF_DEBUG("\ninflateInit() failed!");
            return false;
        }
        for ( ; ; )
        {
            int status;
            
            if (!stream.avail_in)
            {
                // Input buffer is empty, so read more bytes from input file.
                uint32_t n = std::min(sizeDataOut, infile_remaining);
                if (n > sizeofDataIn)
                {
                    PRINT_IF_DEBUG("\nfailed to read from stream!");
                    return false;
                }
                stream.avail_in = n;
                infile_remaining -= n;
            }
            status = inflate(&stream, Z_SYNC_FLUSH);
            if ((status == Z_STREAM_END) || (!stream.avail_out))
            {
                if(status != Z_STREAM_END)
                {
                    PRINT_IF_DEBUG("\ninflate() failed!\nsize of dat out may be wrong status: %d", status);
                    return false;   
                }
                stream.next_out = dataStreamOut;
                stream.avail_out = sizeDataOut;
                sizeDataStreamOut=sizeDataOut;
            }
            if (status == Z_STREAM_END)
                break;
            else if (status != Z_OK)
            {
                PRINT_IF_DEBUG("\ninflate() failed status %i!", status);
                return false;
            }
        }
        if (inflateEnd(&stream) != Z_OK)
        {

            PRINT_IF_DEBUG("\ninflateEnd() failed!");
            return false;
        }
        return true;
    }
    
    uint8_t * MINIZ::getDataStreamOut()const
    {   
        return dataStreamOut;
    }
    
    uint32_t MINIZ::getSizeDataStreamOut()const
    {
        return sizeDataStreamOut;
    }

    int MINIZ::InflateInit(mz_streamp pStream)
    {
        return inflateInit(pStream);
    }

    int MINIZ::Inflate(mz_streamp pStream, int flush)
    {
        return inflate(pStream,flush);
    }

    int MINIZ::InflateEnd(mz_streamp pStream)
    {
        return inflateEnd(pStream);
    }

    
}

