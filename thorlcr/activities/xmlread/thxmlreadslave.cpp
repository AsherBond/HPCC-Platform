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

#include "platform.h"
#include <limits.h>
#include "jio.hpp"
#include "jlzw.hpp"
#include "jsort.hpp"
#include "eclhelper.hpp"
#include "slave.ipp"

#include "thexception.hpp"
#include "thactivityutil.ipp"
#include "thormisc.hpp"
#include "thmfilemanager.hpp"
#include "thbufdef.hpp"
#include "thsortu.hpp"
#include "thorxmlread.hpp"
#include "thdiskbaseslave.ipp"

class CXmlReadSlaveActivity : public CDiskReadSlaveActivityBase
{
    typedef CDiskReadSlaveActivityBase PARENT;

    IHThorXmlReadArg *helper;
    Owned<CSeqPartHandler> partSequencer;
    rowcount_t limit;
    rowcount_t stopAfter;

    class CXmlPartHandler : public CDiskPartHandlerBase, implements IXMLSelect
    {
        CXmlReadSlaveActivity &activity;
        IXmlToRowTransformer *xmlTransformer;
        Linked<IColumnProvider> lastMatch;
        Owned<IXMLParse> xmlParser;
        CRC32 inputCRC;
        OwnedIFileIO iFileIO;
        Owned<IIOStream> inputIOstream;
        offset_t localOffset;  // not sure what this is for 
        unsigned __int64 progress = 0;
        Linked<IEngineRowAllocator> allocator;

    public:
        IMPLEMENT_IINTERFACE_USING(CSimpleInterface);

        CXmlPartHandler(CXmlReadSlaveActivity &_activity, IEngineRowAllocator *_allocator) 
            : CDiskPartHandlerBase(_activity), activity(_activity), allocator(_allocator)
        {
            xmlTransformer = activity.helper->queryTransformer();
            localOffset = 0;
        }

        virtual void open() 
        {
            CDiskPartHandlerBase::open();

            OwnedIFileIO partFileIO;
            if (compressed)
            {
                partFileIO.setown(createCompressedFileReader(iFile, activity.eexp, useDefaultIoBufferSize, false, IFEnone));
                if (!partFileIO)
                    throw MakeActivityException(&activity, 0, "Failed to open block compressed file '%s'", filename.get());
            }
            else
                partFileIO.setown(iFile->open(IFOread));

            {
                CriticalBlock block(inputCs);
                iFileIO.setown(partFileIO.getClear());
            }

            Owned<IIOStream> stream = createIOStream(iFileIO);
            inputIOstream.setown(createBufferedIOStream(stream));
            OwnedRoxieString xmlIterator(activity.helper->getXmlIteratorPath());
            if (activity.queryContainer().getKind()==TAKjsonread)
                xmlParser.setown(createJSONParse(*inputIOstream.get(), xmlIterator, *this, (0 != (TDRxmlnoroot & activity.helper->getFlags()))?ptr_noRoot:ptr_none, 0 != (TDRusexmlcontents & activity.helper->getFlags())));
            else
                xmlParser.setown(createXMLParse(*inputIOstream.get(), xmlIterator, *this, (0 != (TDRxmlnoroot & activity.helper->getFlags()))?ptr_noRoot:ptr_none, 0 != (TDRusexmlcontents & activity.helper->getFlags())));
            progress = 0;
        }
        virtual void close(CRC32 &fileCRC)
        {
            xmlParser.clear();
            inputIOstream.clear();
            Owned<IFileIO> partFileIO;
            {
                CriticalBlock block(inputCs);
                partFileIO.setown(iFileIO.getClear());
            }
            if (partFileIO)
            {
                mergeStats(closedPartFileStats, partFileIO);
                closedPartFileStats.mergeStatistic(StNumDiskRowsRead, progress);
                progress = 0;
            }
        }

        const void *nextRow()
        {
            if (eoi || activity.abortSoon)
                return NULL;

            try
            {
                while (xmlParser->next())
                {
                    if (lastMatch)
                    {
                        RtlDynamicRowBuilder row(allocator);
                        size32_t sz = xmlTransformer->transform(row, lastMatch, this);
                        lastMatch.clear();
                        if (sz)
                        {
                            localOffset = 0;
                            ++progress;
                            return row.finalizeRowClear(sz);
                        }
                    }
                }
            }
            catch (IPTreeException *e)
            {
                StringBuffer context;
                e->errorMessage(context).newline();
                context.append("Logical filename = ").append(activity.logicalFilename).newline();
                context.append("Physical file part = ").append(iFile->queryFilename()).newline();
                context.append("offset = ").append(localOffset);
                throw MakeStringException(e->errorCode(), "%s", context.str());
            }
            catch (IPTreeReadException *e)
            {
                if (PTreeRead_syntax != e->errorCode())
                    throw;
                Owned<IException> _e = e;
                offset_t localFPos = makeLocalFposOffset(activity.queryContainer().queryJobChannel().queryMyRank()-1, e->queryOffset());
                StringBuffer context;
                context.append("Logical filename = ").append(activity.logicalFilename).newline();
                context.append("Local fileposition = ");
                _WINREV8(localFPos);
                context.append("0x");
                appendDataAsHex(context, sizeof(localFPos), &localFPos);
                context.newline();
                context.append(e->queryContext());
                throw createPTreeReadException(e->errorCode(), e->queryDescription(), context.str(), e->queryLine(), e->queryOffset());
            }
            catch (IOutOfMemException *e)
            {
                StringBuffer s("XMLRead actId(");
                s.append(activity.queryContainer().queryId()).append(") out of memory.").newline();
                s.append("INTERNAL ERROR ").append(e->errorCode());
                Owned<IException> e2 = MakeActivityException(&activity, e, "%s", s.str());
                e->Release();
                throw e2.getClear();
            }
            catch (IException *e)
            {
                StringBuffer s("XMLRead actId(");
                s.append(activity.queryContainer().queryId());
                s.append(") INTERNAL ERROR ").append(e->errorCode());
                Owned<IException> e2 = MakeActivityException(&activity, e, "%s", s.str());
                e->Release();
                throw e2.getClear();
            }
            eoi = true;
            return NULL;
        }

// IXMLSelect
        virtual void match(IColumnProvider &entry, offset_t startOffset, offset_t endOffset)
        {
            localOffset = startOffset;
            lastMatch.set(&entry);
        }

        offset_t getLocalOffset()
        {
            return localOffset; // NH->JCS is this what is wanted? (or should it be stream position relative?
        }
    
        virtual void gatherStats(CRuntimeStatisticCollection & merged)
        {
            CriticalBlock block(inputCs);
            CDiskPartHandlerBase::gatherStats(merged);
            mergeStats(merged, iFileIO);
            merged.mergeStatistic(StNumDiskRowsRead, progress);
        }
    };
public:
    CXmlReadSlaveActivity(CGraphElementBase *_container) : CDiskReadSlaveActivityBase(_container, nullptr)
    {
        helper = (IHThorXmlReadArg *)queryHelper();
        stopAfter = (rowcount_t)helper->getChooseNLimit();
        if (helper->getFlags() & TDRlimitskips)
            limit = RCMAX;
        else
            limit = (rowcount_t)helper->getRowLimit();  
        appendOutputLinked(this);
    }
    ~CXmlReadSlaveActivity()
    {
    }
    virtual void init(MemoryBuffer &data, MemoryBuffer &slaveData) override
    {
        CDiskReadSlaveActivityBase::init(data, slaveData);
        partHandler.setown(new CXmlPartHandler(*this,queryRowAllocator()));
    }
    virtual void kill()
    {
        partSequencer.clear();
        CDiskReadSlaveActivityBase::kill();
    }
    
// IThorDataLink
    virtual void start() override
    {
        ActivityTimer s(slaveTimerStats, timeActivities);
        CDiskReadSlaveActivityBase::start();
        partSequencer.setown(createSequentialPartHandler(partHandler, partDescs, false));
    }
    virtual void stop() override
    {
        if (partSequencer)
        {
            partSequencer->stop();
            partSequencer.clear();
        }
        PARENT::stop();
    }

    CATCH_NEXTROW()
    {
        ActivityTimer t(slaveTimerStats, timeActivities);
        OwnedConstThorRow row = partSequencer->nextRow();
        if (unlikely(!row))
            return NULL;
        rowcount_t c = getDataLinkCount();
        if (stopAfter && (c >= stopAfter)) // NB: only slave limiter, global performed in chained choosen activity 
            return NULL;
        if (c >= limit) // NB: only slave limiter, global performed in chained limit activity
        {
            helper->onLimitExceeded();
            return NULL;
        }
        dataLinkIncrement();
        return row.getClear();
    }
    
    virtual bool isGrouped() const override { return false; }
    virtual void getMetaInfo(ThorDataLinkMetaInfo &info) const override
    {
        if (!gotMeta)
        {
            gotMeta = true;
            initMetaInfo(cachedMetaInfo);
            cachedMetaInfo.isSource = true;
            getPartsMetaInfo(cachedMetaInfo, partDescs.ordinality(), ((IArrayOf<IPartDescriptor> &) partDescs).getArray(), partHandler);
            cachedMetaInfo.unknownRowsOutput = true; // at least I don't think we know
            cachedMetaInfo.fastThrough = true;
        }
        info = cachedMetaInfo;
    }
friend class CXmlPartHandler;
};

CActivityBase *createXmlReadSlave(CGraphElementBase *container)
{
    return new CXmlReadSlaveActivity(container);
}

