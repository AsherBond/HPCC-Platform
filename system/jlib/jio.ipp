/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */


#ifndef JIO_IPP
#define JIO_IPP

#include "jiface.hpp"
#include "jexcept.hpp"

#include "jio.hpp"

#ifdef __64BIT__
#define DEFAULT_BUFFER_SIZE     0x100000  // Always buffer 1MB on 64bit systems
#else
// Restrict buffer sizes on 32-bit systems
#define DEFAULT_BUFFER_SIZE     0x10000
#endif

class CBufferedIOStreamBase: public CInterface
{
public:
    CBufferedIOStreamBase(unsigned _bufferSize);


protected:
    inline size32_t  bytesRemaining() { return numInBuffer + curBufferOffset; }
    inline void writeToBuffer(size32_t len, const void * data)
    {
        memcpy(buffer + curBufferOffset, data, len);
        curBufferOffset += len;
        if (curBufferOffset > numInBuffer)
            numInBuffer = curBufferOffset;
    }
    inline size32_t readFromBuffer(size32_t len, void * data) 
    {
        size32_t sizeGot = numInBuffer - curBufferOffset;
        if (sizeGot > len)
            sizeGot = len;
        memcpy(data, buffer+curBufferOffset, sizeGot);
        curBufferOffset += sizeGot;
        return sizeGot;
    }

    virtual bool fillBuffer()=0;
    virtual size32_t directRead(size32_t len, void * data)=0;
    virtual size32_t directWrite(size32_t len, const void * data)=0;
    virtual void doflush()=0;
    size32_t doread(size32_t len, void * data);
    size32_t dowrite(size32_t len, const void * data);


protected:
    byte *                  buffer;
    size32_t                bufferSize;
    size32_t                minDirectSize;
    size32_t                numInBuffer;
    size32_t                curBufferOffset;
    bool                    reading;
};

extern size32_t chunked_fwrite(void *buffer, size32_t count, FILE *stream);



#endif
